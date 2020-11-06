#.rst:
# Find glsl-optimizer
# -------------------
#
# Finds the glsl-optimizer library. This module defines:
#
#  GlslOptimizer_FOUND  - True if the glsl-optimizer library is found
#  GlslOptimizer::GlslOptimizer - glsl-optimizer imported target.
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

# This currently handles only the case of a CMake subproject. Sorry.
if(TARGET glsl_optimizer)
    # The glsl_optimizer target doesn't define any usable
    # INTERFACE_INCLUDE_DIRECTORIES, so let's extract that from the SOURCE_DIR
    # property instead.
    get_target_property(_GLSL_OPTIMIZER_INTERFACE_INCLUDE_DIRECTORIES glsl_optimizer SOURCE_DIR)
    set(_GLSL_OPTIMIZER_INTERFACE_INCLUDE_DIRECTORIES ${_GLSL_OPTIMIZER_INTERFACE_INCLUDE_DIRECTORIES}/src)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args("GlslOptimizer" DEFAULT_MSG
    _GLSL_OPTIMIZER_INTERFACE_INCLUDE_DIRECTORIES)

if(NOT TARGET GlslOptimizer::GlslOptimizer)
    add_library(GlslOptimizer::GlslOptimizer INTERFACE IMPORTED)
    set_target_properties(GlslOptimizer::GlslOptimizer PROPERTIES
        INTERFACE_LINK_LIBRARIES glsl_optimizer
        INTERFACE_INCLUDE_DIRECTORIES ${_GLSL_OPTIMIZER_INTERFACE_INCLUDE_DIRECTORIES})
endif()
