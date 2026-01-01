#.rst:
# Find Spng
# --------
#
# Finds the Spng library. This module defines:
#
#  Spng_FOUND           - True if Spng library is found
#  Spng::Spng           - Spng imported target
#
# Additionally these variables are defined for internal usage:
#
#  Spng_LIBRARY         - Spng library
#  Spng_INCLUDE_DIR     - Include dir
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022, 2023, 2024, 2025, 2026
#             Vladimír Vondruš <mosra@centrum.cz>
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

# The logic around finding & using zlib is adapted from CMake's own
# FindPNG.cmake
if(Spng_FIND_QUIETLY)
    set(_FIND_ZLIB_ARG QUIET)
endif()
find_package(ZLIB ${_FIND_ZLIB_ARG})

if(ZLIB_FOUND)
    find_library(Spng_LIBRARY NAMES spng spng_static)
    find_path(Spng_INCLUDE_DIR NAMES spng.h)

    if(Spng_LIBRARY AND Spng_INCLUDE_DIR AND NOT TARGET Spng::Spng)
        add_library(Spng::Spng UNKNOWN IMPORTED)
        set_property(TARGET Spng::Spng PROPERTY
            IMPORTED_LOCATION ${Spng_LIBRARY})
        set_target_properties(Spng::Spng PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${Spng_INCLUDE_DIR}
            INTERFACE_LINK_LIBRARIES ZLIB::ZLIB)
        # libspng doesn't generate any configuration file on its own, requiring
        # us to explicitly disambiguate to avoid marking symbols with dllimport
        # on static builds
        if(WIN32 AND Spng_LIBRARY MATCHES "${CMAKE_STATIC_LIBRARY_SUFFIX}$")
            set_target_properties(Spng::Spng PROPERTIES
                INTERFACE_COMPILE_DEFINITIONS SPNG_STATIC)
        endif()
    endif()

    mark_as_advanced(Spng_LIBRARY Spng_INCLUDE_DIR)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Spng DEFAULT_MSG
    Spng_LIBRARY
    Spng_INCLUDE_DIR)
