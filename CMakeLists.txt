#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014
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

cmake_minimum_required(VERSION 2.8.8)
project(MagnumPlugins)

# This is needed for sure
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/modules/")
find_package(Magnum REQUIRED)

include(CMakeDependentOption)

option(WITH_FIND_MODULE "Install FindMagnumPlugins.cmake module into CMake's module dir (might require admin privileges)" OFF)

# Plugins to build
cmake_dependent_option(WITH_ANYIMAGEIMPORTER "Build AnyImageImporter plugin" OFF "NOT WITH_COLLADAIMPORTER" ON)
option(WITH_COLLADAIMPORTER "Build ColladaImporter plugin" OFF)
cmake_dependent_option(WITH_FREETYPEFONT "Build FreeTypeFont plugin" OFF "NOT WITH_HARFBUZZFONT" ON)
option(WITH_HARFBUZZFONT "Build HarfBuzzFont plugin" OFF)
option(WITH_JPEGIMPORTER "Build JpegImporter plugin" OFF)
option(WITH_PNGIMPORTER "Build PngImporter plugin" OFF)
option(WITH_STBIMAGEIMPORTER "Build StbImageImporter plugin" OFF)

include(CMakeDependentOption)
option(BUILD_TESTS "Build unit tests." OFF)
cmake_dependent_option(BUILD_GL_TESTS "Build unit tests for OpenGL code." OFF "BUILD_TESTS" OFF)

option(BUILD_STATIC "Build static plugins (default are dynamic)" OFF)

# Check compiler compatibility
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND "${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS "4.7.0")
    message(FATAL_ERROR "Compatibility branch of Magnum Plugins is needed for use with GCC < 4.7. See the documentation for more information.")
elseif(MSVC)
    message(FATAL_ERROR "Compatibility branch of Magnum Plugins is needed for use with MSVC. See the documentation for more information.")
endif()

# Newlib toolchain supports only static linking
if(CORRADE_TARGET_NACL_NEWLIB OR CORRADE_TARGET_EMSCRIPTEN)
    set(BUILD_STATIC ON)
endif()

if(BUILD_TESTS)
    enable_testing()
endif()

if(BUILD_GL_TESTS)
    if(UNIX AND (NOT MAGNUM_TARGET_GLES OR MAGNUM_TARGET_DESKTOP_GLES))
        find_package(Magnum REQUIRED WindowlessGlxApplication)
    else()
        message(FATAL_ERROR "Cannot run tests for OpenGL code on this platform. Set BUILD_GL_TESTS to OFF to skip building them.")
    endif()

    set(GL_TEST_LIBRARIES ${MAGNUM_WINDOWLESSAPPLICATION_LIBRARIES})
endif()

add_subdirectory(modules)
add_subdirectory(src)
