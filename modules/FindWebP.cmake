#.rst:
# Find WebP
# -------------
#
# Finds the WebP library. This module defines:
#
#  WebP_FOUND           - True if WebP library is found
#  WebP::WebP           - WebP imported target
#
# Additionally these variables are defined for internal usage:
#
#  WebP_LIBRARY         - WebP library
#  WebP_INCLUDE_DIR     - Include dir
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022, 2023 Vladimír Vondruš <mosra@centrum.cz>
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

# In case we have WebP as a CMake subproject, a webp target should be present.
# If not, try to find it via the installed config file, as that is useful to
# know additional libraries to link to in case of a static build.
if(NOT TARGET webp)
    find_package(WebP CONFIG QUIET)
endif()

# Unfortunately the CMake project itself doesn't provide namespaced ALIAS
# targets so we have to do this insane branching here.
set(_WEBP_TARGET )
set(_WEBP_SUBPROJECT )
if(TARGET webp)
    set(_WEBP_TARGET webp)
    set(_WEBP_SUBPROJECT ON)
elseif(TARGET WebP::webp)
    set(_WEBP_TARGET WebP::webp)
endif()
if(_WEBP_TARGET)
    # The webp target doesn't define any usable INTERFACE_INCLUDE_DIRECTORIES
    # for some reason (apparently the $<BUILD_INTERFACE:> in there doesn't work
    # or whatever), so let's do that ourselves.
    #
    # TODO this could be probably fixable by using target_link_libraries()
    # instead of set_target_properties() because it evaluates generator
    # expressions, but that needs CMake 3.10+, before that
    # target_link_libraries() can't be called on INTERFACE targets.
    if(_WEBP_SUBPROJECT)
        # The target is defined in the root, includes are in src/
        get_target_property(_WEBP_INTERFACE_INCLUDE_DIRECTORIES ${_WEBP_TARGET} SOURCE_DIR)
        set(_WEBP_INTERFACE_INCLUDE_DIRECTORIES ${_WEBP_INTERFACE_INCLUDE_DIRECTORIES}/src)
    else()
        get_target_property(_WEBP_INTERFACE_INCLUDE_DIRECTORIES ${_WEBP_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    endif()

    if(NOT TARGET WebP::WebP)
        # Aliases of (global) targets are only supported in CMake 3.11, so we
        # work around it by this. This is easier than fetching all possible
        # properties (which are impossible to track of) and then attempting to
        # rebuild them into a new target.
        add_library(WebP::WebP INTERFACE IMPORTED)
        set_target_properties(WebP::WebP PROPERTIES
            # TODO the WebP::webp target additionally depends on the sharpyuv
            #   (static) library. The subproject target doesn't but it doesn't
            #   seem to cause any problems, right now at least.
            INTERFACE_LINK_LIBRARIES ${_WEBP_TARGET})
    endif()

    # Just to make FPHSA print some meaningful location, nothing else
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(WebP DEFAULT_MSG
        _WEBP_INTERFACE_INCLUDE_DIRECTORIES)
    return()
endif()

# Library
find_library(WebP_LIBRARY NAMES webp
    # Prebuilt Windows binaries have a `lib` prefix as well, even though they
    # shouldn't, sigh: https://developers.google.com/speed/webp/download
    libwebp)

# Include dir
find_path(WebP_INCLUDE_DIR
    NAMES webp/decode.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WebP DEFAULT_MSG
    WebP_LIBRARY
    WebP_INCLUDE_DIR)

mark_as_advanced(FORCE
    WebP_INCLUDE_DIR
    WebP_LIBRARY)

if(NOT TARGET WebP::WebP)
    add_library(WebP::WebP UNKNOWN IMPORTED)
    set_target_properties(WebP::WebP PROPERTIES
        IMPORTED_LOCATION ${WebP_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${WebP_INCLUDE_DIR})
endif()
