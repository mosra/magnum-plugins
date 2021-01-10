#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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

# A lot of 3rd party code emits warnings with our stricter flags and some even
# uses -Werror, so those are undesired here.
set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS OFF)

# To help Homebrew and Vcpkg packages, meshoptimizer sources can be cloned to
# src/external and we will use those without any extra effort from the outside.
if(WITH_MESHOPTIMIZERSCENECONVERTER)
    if(NOT TARGET meshoptimizer AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/meshoptimizer)
        # Build (static) meshoptimizer with PIC enabled if we are building
        # dynamic plugins
        if(NOT BUILD_PLUGINS_STATIC)
            set(_PREV_CMAKE_POSITION_INDEPENDENT_CODE ${CMAKE_POSITION_INDEPENDENT_CODE})
            set(CMAKE_POSITION_INDEPENDENT_CODE ON)
        endif()
        add_subdirectory(meshoptimizer EXCLUDE_FROM_ALL)
        if(NOT BUILD_PLUGINS_STATIC)
            set(CMAKE_POSITION_INDEPENDENT_CODE ${_PREV_CMAKE_POSITION_INDEPENDENT_CODE})
        endif()
    endif()
endif()
