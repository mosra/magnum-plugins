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
