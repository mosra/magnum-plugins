#.rst:
# Find Glslang
# ------------
#
# Finds the glslang library. This module defines:
#
#  Glslang_FOUND        - True if the Glslang library is found
#  Glslang::Glslang     - Glslang imported target
#  Glslang::SPIRV       - Glslang libSPIRV imported target
#
# Link to Glslang::Glslang, all its dependencies will get linked transitively.
# Additionally these variables are defined for internal usage:
#
#  GLSLANG_LIBRARY      - Glslang library
#  GLSLANG_SPIRV_LIBRARY - Glslang libSPIRV library
#  GLSLANG_INCLUDE_DIR  - Include dir
#
# Actually, it's 2020 and none of this should be needed, but apparently Magnum
# is the first project ever to use this thing from Khronos as an actual library
# instead of the CLI tool. Their CMake system installs a ton of
# ``glslangTargets.cmake`` files, but no actual ``glslangConfig.cmake``, so I
# have to look for it myself. Scroll below for a continuation of this angry
# rant.
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020 Vladimír Vondruš <mosra@centrum.cz>
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

# Libraries
find_library(GLSLANG_LIBRARY NAMES glslang)
find_library(GLSLANG_SPIRV_LIBRARY NAMES SPIRV)

# Include dir
find_path(GLSLANG_INCLUDE_DIR
    # Actually, WHAT THE FUCK, I get that some people suck at naming, but this
    # is an actual naming skill black hole. Even naming it Windows.h would make
    # more sense than this. Like, what the hell.
    NAMES glslang/Public/ShaderLang.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Glslang DEFAULT_MSG
    GLSLANG_LIBRARY
    GLSLANG_SPIRV_LIBRARY
    GLSLANG_INCLUDE_DIR)

mark_as_advanced(FORCE
    GLSLANG_LIBRARY
    GLSLANG_SPIRV_LIBRARY
    GLSLANG_INCLUDE_DIR)

if(NOT TARGET Glslang::SPIRV)
    add_library(Glslang::SPIRV UNKNOWN IMPORTED)
    set_target_properties(Glslang::SPIRV PROPERTIES
        IMPORTED_LOCATION ${GLSLANG_SPIRV_LIBRARY})
endif()

if(NOT TARGET Glslang::Glslang)
    add_library(Glslang::Glslang UNKNOWN IMPORTED)
    set_target_properties(Glslang::Glslang PROPERTIES
        IMPORTED_LOCATION ${GLSLANG_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${GLSLANG_INCLUDE_DIR}
        INTERFACE_LINK_LIBRARIES Glslang::SPIRV)
endif()
