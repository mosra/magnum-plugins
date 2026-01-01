#.rst:
# Find Resvg
# ----------
#
# Finds the Resvg library. This module defines:
#
#  Resvg_FOUND          - True if Resvg library is found
#  Resvg::Resvg         - Resvg imported target
#
# Additionally these variables are defined for internal usage:
#
#  Resvg_LIBRARY        - Resvg library
#  Resvg_INCLUDE_DIR    - Include dir
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

# Library
find_library(Resvg_LIBRARY NAMES resvg)

# Include dir
find_path(Resvg_INCLUDE_DIR
    NAMES resvg.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Resvg DEFAULT_MSG
    Resvg_LIBRARY
    Resvg_INCLUDE_DIR)

mark_as_advanced(FORCE
    Resvg_INCLUDE_DIR
    Resvg_LIBRARY)

if(NOT TARGET Resvg::Resvg)
    add_library(Resvg::Resvg UNKNOWN IMPORTED)
    set_target_properties(Resvg::Resvg PROPERTIES
        IMPORTED_LOCATION ${Resvg_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${Resvg_INCLUDE_DIR})
endif()
