#.rst:
# Find Zstd
# ---------
#
# Finds the Zstd library. This module defines:
#
#  Zstd_FOUND           - True if Zstd library is found
#  Zstd::Zstd           - Zstd imported target
#
# Additionally these variables are defined for internal usage:
#
#  Zstd_LIBRARY         - Zstd library
#  Zstd_INCLUDE_DIR     - Include dir
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
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

# In case we have Zstd as a CMake subproject, a libzstd_shared / libzstd_static
# target should be present. If not, try to find it via the installed config
# file, as that is useful to know additional libraries to link to in case of a
# static build.
if(NOT TARGET libzstd_shared AND NOT TARGET libzstd_static)
    find_package(zstd CONFIG QUIET)
endif()

# Unfortunately the CMake project itself doesn't provide namespaced ALIAS
# targets so we have to do this insane branching here.
set(_ZSTD_TARGET )
set(_ZSTD_SUBPROJECT )
if(TARGET libzstd_shared)
    set(_ZSTD_TARGET libzstd_shared)
    set(_ZSTD_SUBPROJECT ON)
elseif(TARGET libzstd_static)
    set(_ZSTD_TARGET libzstd_static)
    set(_ZSTD_SUBPROJECT ON)
elseif(TARGET zstd::libzstd_shared)
    set(_ZSTD_TARGET zstd::libzstd_shared)
elseif(TARGET zstd::libzstd_static)
    set(_ZSTD_TARGET zstd::libzstd_static)
endif()
if(_ZSTD_TARGET)
    if(NOT TARGET Zstd::Zstd)
        # Aliases of (global) targets are only supported in CMake 3.11, so we
        # work around it by this. This is easier than fetching all possible
        # properties (which are impossible to track of) and then attempting to
        # rebuild them into a new target.
        add_library(Zstd::Zstd INTERFACE IMPORTED)
        set_target_properties(Zstd::Zstd PROPERTIES
            INTERFACE_LINK_LIBRARIES ${_ZSTD_TARGET})
    endif()

    if(_ZSTD_SUBPROJECT)
        # The target is defined in build/cmake/lib/, includes are in lib/
        get_target_property(_ZSTD_INTERFACE_INCLUDE_DIRECTORIES ${_ZSTD_TARGET} SOURCE_DIR)
        get_filename_component(_ZSTD_INTERFACE_INCLUDE_DIRECTORIES ${_ZSTD_INTERFACE_INCLUDE_DIRECTORIES} DIRECTORY)
        get_filename_component(_ZSTD_INTERFACE_INCLUDE_DIRECTORIES ${_ZSTD_INTERFACE_INCLUDE_DIRECTORIES} DIRECTORY)
        get_filename_component(_ZSTD_INTERFACE_INCLUDE_DIRECTORIES ${_ZSTD_INTERFACE_INCLUDE_DIRECTORIES} DIRECTORY)
        set(_ZSTD_INTERFACE_INCLUDE_DIRECTORIES ${_ZSTD_INTERFACE_INCLUDE_DIRECTORIES}/lib)
    else()
        get_target_property(_ZSTD_INTERFACE_INCLUDE_DIRECTORIES ${_ZSTD_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    endif()

    # Just to make FPHSA print some meaningful location, nothing else
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Zstd DEFAULT_MSG
        _ZSTD_INTERFACE_INCLUDE_DIRECTORIES)
    return()
endif()

# Library
find_library(Zstd_LIBRARY NAMES zstd)

# Include dir
find_path(Zstd_INCLUDE_DIR
    NAMES zstd.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Zstd DEFAULT_MSG
    Zstd_LIBRARY
    Zstd_INCLUDE_DIR)

mark_as_advanced(FORCE
    Zstd_LIBRARY
    Zstd_INCLUDE_DIR)

if(NOT TARGET Zstd::Zstd)
    add_library(Zstd::Zstd UNKNOWN IMPORTED)
    set_target_properties(Zstd::Zstd PROPERTIES
        IMPORTED_LOCATION ${Zstd_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${Zstd_INCLUDE_DIR})
endif()
