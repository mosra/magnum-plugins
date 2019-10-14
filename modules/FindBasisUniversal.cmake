#.rst:
# Find BasisUniversal
# -------------------
#
# Finds the BasisUniversal library. This module defines:
#
#  BasisUniversal_FOUND         - True if BasisUniversal is found
#  BasisUniversal::Encoder      - Encoder library target
#  BasisUniversal::Transcoder   - Transcoder library target
#
# Additionally these variables are defined for internal usage:
#
#  BasisUniversalTranscoder_INCLUDE_DIR      - Transcoder include dir
#  BasisUniversalEncoder_INCLUDE_DIR         - Encoder include dir
#
# The find module first tries to find ``basisu_transcoder`` and ``basisu_encoder``
# via a CMake config file (which is distributed this way via Vcpkg, for example).
# If that's found, the ``BasisUniversal::Transcoder`` and ``BasisUniversal::Encoder``
# targets are set up as aliases to them.
#
# If ``basisu`` is not found, as a fallback it tries to find the C++ sources.
# You can supply their location via an ``BASIS_UNIVERSAL_DIR`` variable. Once
# found, the ``BasisUniversal::Encoder`` target is set up to compile the
# sources of the encoder and ``BasisUniversal::Transcoder`` the sources of the
# transcoder as static libraries.
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
#             Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2019 Jonathan Hale <squareys@googlemail.com>
#
#   Permission is hereby granted, free of charge, to any person obtaining a
#   copy of this software and associated documentation files (the "Software"),
#   to deal in the Software without restriction, including without limitation
#   the rights to use, copy, modify, merge, publish, distribute, sublicense,
#   and/or sell copies of the Software, and to permit persons to whom the
#   Software is furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#   DEALINGS IN THE SOFTWARE.
#

# Vcpkg distributes imgui as a library with a config file, so try that first --
# but only if BASIS_UNIVERSAL_DIR wasn't explicitly passed, in which case we'll
# look there instead
find_package(basisu CONFIG QUIET)
if(basisu_FOUND AND NOT BASIS_UNIVERSAL_DIR)
    if(NOT TARGET BasisUniversal::Transcoder)
        add_library(BasisUniversal::Transcoder INTERFACE IMPORTED)
        set_property(TARGET BasisUniversal::Transcoder APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES basisu_transcoder)

        # Retrieve include directory for FindPackageHandleStandardArgs later
        get_target_property(BasisUniversalTranscoder_INCLUDE_DIR basisu_transcoder
            INTERFACE_INCLUDE_DIRECTORIES)
    endif()

    if(NOT TARGET BasisUniversal::Encoder)
        add_library(BasisUniversal::Encoder INTERFACE IMPORTED)
        set_property(TARGET BasisUniversal::Encoder APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES basisu_encoder)

        # Retrieve include directory for FindPackageHandleStandardArgs later
        get_target_property(BasisUniversalEncoder_INCLUDE_DIR basisu_encoder
            INTERFACE_INCLUDE_DIRECTORIES)
    endif()

# Otherwise find the source files and compile them as part of the library they
# get linked to
else()
    # Disable the find root path here, it overrides the
    # CMAKE_FIND_ROOT_PATH_MODE_INCLUDE setting potentially set in
    # toolchains.
    find_path(BasisUniversalEncoder_INCLUDE_DIR NAMES basisu_frontend.h HINTS
        HINTS "${BASIS_UNIVERSAL_DIR}" "${BASIS_UNIVERSAL_DIR}/encoder"
        NO_CMAKE_FIND_ROOT_PATH)
    mark_as_advanced(BasisUniversalEncoder_INCLUDE_DIR)

    find_path(BasisUniversalTranscoder_INCLUDE_DIR NAMES basisu_transcoder.h
        HINTS "${BASIS_UNIVERSAL_DIR}/transcoder" "${BASIS_UNIVERSAL_DIR}"
        NO_CMAKE_FIND_ROOT_PATH)
    mark_as_advanced(BasisUniversalTranscoder_INCLUDE_DIR)
endif()

list(FIND BasisUniversal_FIND_COMPONENTS "Encoder" _index)
if(${_index} GREATER -1)
    list(APPEND BasisUniversal_FIND_COMPONENTS "Transcoder")
    list(REMOVE_DUPLICATES BasisUniversal_FIND_COMPONENTS)
endif()

macro(_basis_setup_source_file source)
    # Handle export and import of imgui symbols via IMGUI_API
    # definition in visibility.h of Magnum ImGuiIntegration.
    set_property(SOURCE ${source} APPEND PROPERTY COMPILE_DEFINITIONS
        BASISU_NO_ITERATOR_DEBUG_LEVEL)

    # Hide warnings from basis source files. There's thousands of -Wpedantic
    # ones which are hard to suppress on old GCCs (which have just -pedantic),
    # so simply give up and disable all.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR (CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?Clang"
        AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC") OR CORRADE_TARGET_EMSCRIPTEN)
        set_property(SOURCE ${source} APPEND_STRING PROPERTY COMPILE_FLAGS
            " -w")
    endif()
endmacro()

# Find components
foreach(_component ${BasisUniversal_FIND_COMPONENTS})
    if(_component STREQUAL "Encoder")
        if(NOT TARGET BasisUniversal::Encoder)
            find_path(BasisUniversalEncoder_DIR NAMES basisu_frontend.cpp
                HINTS "${BASIS_UNIVERSAL_DIR}" "${BASIS_UNIVERSAL_DIR}/encoder"
                NO_CMAKE_FIND_ROOT_PATH)
            if(BasisUniversalEncoder_DIR)
                set(BasisUniversal_Encoder_FOUND TRUE)
            else()
                message(SEND_ERROR
                    "Could not find encoder in basis_universal directory.\n"
                    "Set BASIS_UNIVERSAL_DIR to the root of a directory containing basis_universal source.")
            endif()

            set(BasisUniversalEncoder_SOURCES
                ${BasisUniversalEncoder_DIR}/basisu_astc_decomp.cpp
                ${BasisUniversalEncoder_DIR}/basisu_backend.cpp
                ${BasisUniversalEncoder_DIR}/basisu_basis_file.cpp
                ${BasisUniversalEncoder_DIR}/basisu_comp.cpp
                ${BasisUniversalEncoder_DIR}/basisu_enc.cpp
                ${BasisUniversalEncoder_DIR}/basisu_etc.cpp
                ${BasisUniversalEncoder_DIR}/basisu_frontend.cpp
                ${BasisUniversalEncoder_DIR}/basisu_global_selector_palette_helpers.cpp
                ${BasisUniversalEncoder_DIR}/basisu_gpu_texture.cpp
                ${BasisUniversalEncoder_DIR}/basisu_pvrtc1_4.cpp
                ${BasisUniversalEncoder_DIR}/basisu_resampler.cpp
                ${BasisUniversalEncoder_DIR}/basisu_resample_filters.cpp
                ${BasisUniversalEncoder_DIR}/basisu_ssim.cpp
                ${BasisUniversalEncoder_DIR}/lodepng.cpp)

            foreach(_file ${BasisUniversalEncoder_SOURCES})
                _basis_setup_source_file(${_file})
            endforeach()

            add_library(BasisUniversal::Encoder INTERFACE IMPORTED)
            set_property(TARGET BasisUniversal::Encoder APPEND PROPERTY
                INTERFACE_INCLUDE_DIRECTORIES ${BasisUniversalEncoder_INCLUDE_DIR})
            set_property(TARGET BasisUniversal::Encoder APPEND PROPERTY
                INTERFACE_SOURCES "${BasisUniversalEncoder_SOURCES}")
            # Explicitly *not* linking this to Threads::Threads because when
            # done like that, std::thread creation will die on a null function
            # pointer call (inside __gthread_create, which weakly links to
            # pthread) in the BasisImageConverter UNLESS the final application
            # is linked to pthread as well. As far as I tried (four hours
            # lost), there's no way to check if the weak pthread_create pointer
            # is null -- tried so far:
            #  - dlsym(self, "pthread_create") returns non-null
            #  - dlopen("libpthread.so") returns non-null
            #  - defining a weakref pthread_create the same way as glibc does
            #    https://github.com/gcc-mirror/gcc/blob/3e7b85061947bdc7c7465743ba90734566860821/libgcc/gthr-posix.h#L106
            #    returns non-null
            # The rest is documented in the BasisImageConverter plugin itself.
            set_property(TARGET BasisUniversal::Encoder APPEND PROPERTY
                INTERFACE_LINK_LIBRARIES BasisUniversal::Transcoder)
            set_property(TARGET BasisUniversal::Encoder APPEND PROPERTY
                INTERFACE_COMPILE_DEFINITIONS "BASISU_NO_ITERATOR_DEBUG_LEVEL")
        else()
            set(BasisUniversal_Encoder_FOUND TRUE)
        endif()

    elseif(_component STREQUAL "Transcoder")
        if(NOT TARGET BasisUniversal::Transcoder)
            find_path(BasisUniversalTranscoder_DIR NAMES basisu_transcoder.cpp
                HINTS "${BASIS_UNIVERSAL_DIR}/transcoder" "${BASIS_UNIVERSAL_DIR}"
                NO_CMAKE_FIND_ROOT_PATH)
            if(BasisUniversalTranscoder_DIR)
                set(BasisUniversal_Transcoder_FOUND TRUE)
            else()
                message(SEND_ERROR
                    "Could not find transcoder in basis_universal directory.\n"
                    "Set BASIS_UNIVERSAL_DIR to the root of a directory containing basis_universal source.")
            endif()
            set(BasisUniversalTranscoder_SOURCES
                ${BasisUniversalTranscoder_DIR}/basisu_transcoder.cpp)

            foreach(_file ${BasisUniversalTranscoder_SOURCES})
                _basis_setup_source_file(${_file})
            endforeach()

            add_library(BasisUniversal::Transcoder INTERFACE IMPORTED)
            set_property(TARGET BasisUniversal::Transcoder APPEND PROPERTY
                INTERFACE_INCLUDE_DIRECTORIES ${BasisUniversalTranscoder_INCLUDE_DIR})
            set_property(TARGET BasisUniversal::Transcoder APPEND PROPERTY
                INTERFACE_COMPILE_DEFINITIONS "BASISU_NO_ITERATOR_DEBUG_LEVEL")
            set_property(TARGET BasisUniversal::Transcoder APPEND PROPERTY
                INTERFACE_SOURCES "${BasisUniversalTranscoder_SOURCES}")
        else()
            set(BasisUniversal_Transcoder_FOUND TRUE)
        endif()
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BasisUniversal
    REQUIRED_VARS BasisUniversalTranscoder_INCLUDE_DIR BasisUniversalEncoder_INCLUDE_DIR
    HANDLE_COMPONENTS)
