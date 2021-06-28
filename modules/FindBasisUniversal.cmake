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
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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

list(FIND BasisUniversal_FIND_COMPONENTS "Encoder" _index)
if(${_index} GREATER -1)
    list(APPEND BasisUniversal_FIND_COMPONENTS "Transcoder")
    list(REMOVE_DUPLICATES BasisUniversal_FIND_COMPONENTS)
endif()

macro(_basis_setup_source_file source)
    # Basis shouldn't override the MSVC iterator debug level as it would make
    # it inconsistent with the rest of the code
    if(CORRADE_TARGET_WINDOWS)
        set_property(SOURCE ${source} APPEND PROPERTY COMPILE_DEFINITIONS
            BASISU_NO_ITERATOR_DEBUG_LEVEL)
    endif()

    # Hide warnings from basis source files. There's thousands of -Wpedantic
    # ones which are hard to suppress on old GCCs (which have just -pedantic),
    # so simply give up and disable all.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CORRADE_TARGET_EMSCRIPTEN)
        set_property(SOURCE ${source} APPEND_STRING PROPERTY COMPILE_FLAGS
            " -w")
    # Clang supports -w, but it doesn't have any effect on all the
    # -Wall -Wold-style-cast etc flags specified before. -Wno-everything does.
    # Funnily enough this is not an issue on Emscripten.;
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?Clang" AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        set_property(SOURCE ${source} APPEND_STRING PROPERTY COMPILE_FLAGS
            " -Wno-everything")
    endif()
endmacro()

# Find components
foreach(_component ${BasisUniversal_FIND_COMPONENTS})
    if(_component STREQUAL "Encoder")
        if(NOT TARGET BasisUniversal::Encoder)
            # Try to find compiled library when installed through vcpkg -- but
            # only if BASIS_UNIVERSAL_DIR wasn't explicitly passed, in which
            # case we'll look there instead
            if(NOT BASIS_UNIVERSAL_DIR)
                find_library(BasisUniversalEncoder_LIBRARY_RELEASE basisu_encoder)
                find_library(BasisUniversalEncoder_LIBRARY_DEBUG basisu_encoder HINTS "debug")
            endif()
            if(NOT BASIS_UNIVERSAL_DIR AND (BasisUniversalEncoder_LIBRARY_DEBUG OR BasisUniversalEncoder_LIBRARY_RELEASE))
                find_path(BasisUniversalEncoder_INCLUDE_DIR basisu_enc.h PATH_SUFFIXES "basisu/encoder")

                add_library(BasisUniversal::Encoder UNKNOWN IMPORTED)
                if(BasisUniversalEncoder_LIBRARY_DEBUG)
                    set_property(TARGET BasisUniversal::Encoder APPEND PROPERTY
                        IMPORTED_LOCATION_DEBUG ${BasisUniversalEncoder_LIBRARY_DEBUG})
                endif()
                if(BasisUniversalEncoder_LIBRARY_RELEASE)
                    set_property(TARGET BasisUniversal::Encoder APPEND PROPERTY
                        IMPORTED_LOCATION_RELEASE ${BasisUniversalEncoder_LIBRARY_RELEASE})
                endif()
                set_property(TARGET BasisUniversal::Encoder APPEND PROPERTY
                    INTERFACE_INCLUDE_DIRECTORIES ${BasisUniversalEncoder_INCLUDE_DIR})
                set_property(TARGET BasisUniversal::Encoder APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES BasisUniversal::Transcoder)

                if((BasisUniversalEncoder_LIBRARY_DEBUG OR BasisUniversalEncoder_LIBRARY_RELEASE) AND BasisUniversalEncoder_INCLUDE_DIR)
                    set(BasisUniversal_Encoder_FOUND TRUE)
                endif()

            # Fall back to finding sources and compile them with the target they are linked to
            else()
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

                # Disable the find root path here, it overrides the
                # CMAKE_FIND_ROOT_PATH_MODE_INCLUDE setting potentially set in
                # toolchains.
                find_path(BasisUniversalEncoder_INCLUDE_DIR NAMES basisu_frontend.h HINTS
                    HINTS "${BASIS_UNIVERSAL_DIR}" "${BASIS_UNIVERSAL_DIR}/encoder"
                    NO_CMAKE_FIND_ROOT_PATH)
                mark_as_advanced(BasisUniversalEncoder_INCLUDE_DIR)

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
            endif()
        else()
            set(BasisUniversal_Encoder_FOUND TRUE)
        endif()

    elseif(_component STREQUAL "Transcoder")
        if(NOT TARGET BasisUniversal::Transcoder)
            # Try to find compiled library when installed through vcpkg -- but
            # only if BASIS_UNIVERSAL_DIR wasn't explicitly passed, in which
            # case we'll look there instead
            if(NOT BASIS_UNIVERSAL_DIR)
                find_library(BasisUniversalTranscoder_LIBRARY_RELEASE basisu_transcoder)
                find_library(BasisUniversalTranscoder_LIBRARY_DEBUG basisu_transcoder HINTS "debug")
            endif()
            if(NOT BASIS_UNIVERSAL_DIR AND (BasisUniversalTranscoder_LIBRARY_DEBUG OR BasisUniversalTranscoder_LIBRARY_RELEASE))
                # Encoder includes expect the basis includes to be prefixed with transcoder/ as in the
                # original basis_universal repository.
                find_path(BasisUniversalTranscoder_INCLUDE_DIR transcoder/basisu_transcoder.h PATH_SUFFIXES "basisu")

                add_library(BasisUniversal::Transcoder UNKNOWN IMPORTED)
                if(BasisUniversalTranscoder_LIBRARY_DEBUG)
                    set_property(TARGET BasisUniversal::Transcoder APPEND PROPERTY
                        IMPORTED_LOCATION_DEBUG ${BasisUniversalTranscoder_LIBRARY_DEBUG})
                endif()
                if(BasisUniversalTranscoder_LIBRARY_RELEASE)
                    set_property(TARGET BasisUniversal::Transcoder APPEND PROPERTY
                        IMPORTED_LOCATION_RELEASE ${BasisUniversalTranscoder_LIBRARY_RELEASE})
                endif()
                set_property(TARGET BasisUniversal::Transcoder APPEND PROPERTY
                    INTERFACE_INCLUDE_DIRECTORIES
                    ${BasisUniversalTranscoder_INCLUDE_DIR}
                    ${BasisUniversalTranscoder_INCLUDE_DIR}/transcoder)

                if((BasisUniversalTranscoder_LIBRARY_DEBUG OR BasisUniversalTranscoder_LIBRARY_RELEASE) AND BasisUniversalTranscoder_INCLUDE_DIR)
                    set(BasisUniversal_Transcoder_FOUND TRUE)
                endif()

            # Fall back to finding sources and compile them with the target they are linked to
            else()
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

                # Disable the find root path here, it overrides the
                # CMAKE_FIND_ROOT_PATH_MODE_INCLUDE setting potentially set in
                # toolchains.
                find_path(BasisUniversalTranscoder_INCLUDE_DIR NAMES basisu_transcoder.h
                    HINTS "${BASIS_UNIVERSAL_DIR}/transcoder" "${BASIS_UNIVERSAL_DIR}"
                    NO_CMAKE_FIND_ROOT_PATH)
                mark_as_advanced(BasisUniversalTranscoder_INCLUDE_DIR)

                add_library(BasisUniversal::Transcoder INTERFACE IMPORTED)
                set_property(TARGET BasisUniversal::Transcoder APPEND PROPERTY
                    INTERFACE_INCLUDE_DIRECTORIES ${BasisUniversalTranscoder_INCLUDE_DIR})
                set_property(TARGET BasisUniversal::Transcoder APPEND PROPERTY
                    INTERFACE_COMPILE_DEFINITIONS "BASISU_NO_ITERATOR_DEBUG_LEVEL")
                set_property(TARGET BasisUniversal::Transcoder APPEND PROPERTY
                    INTERFACE_SOURCES "${BasisUniversalTranscoder_SOURCES}")
            endif()
        else()
            set(BasisUniversal_Transcoder_FOUND TRUE)
        endif()
    endif()
endforeach()

if(BasisUniversalEncoder_INCLUDE_DIR)
    set(_BASISUNIVERSAL_VARS BasisUniversalEncoder_INCLUDE_DIR)
else()
    set(_BASISUNIVERSAL_VARS BasisUniversalTranscoder_INCLUDE_DIR)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BasisUniversal
    REQUIRED_VARS ${_BASISUNIVERSAL_VARS}
    HANDLE_COMPONENTS)
