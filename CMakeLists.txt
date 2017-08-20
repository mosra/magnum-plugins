#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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

cmake_minimum_required(VERSION 2.8.12)

# Configuration fails on < 3.4 if only C++ is enabled (CheckFunctionExists
# macro called from FindX11)
if(NOT CMAKE_VERSION VERSION_LESS 3.4.0)
    set(LANG CXX)
endif()
project(MagnumPlugins ${LANG})

# CMake policies: enable MACOSX_RPATH by default
if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()
# Don't treat imported targets with :: as files
if(POLICY CMP0028)
    cmake_policy(SET CMP0028 NEW)
endif()

# Use folders for nice tree in Visual Studio and XCode
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/modules/")
find_package(Magnum REQUIRED)

include(CMakeDependentOption)

# Plugins to build
cmake_dependent_option(WITH_ANYIMAGEIMPORTER "Build AnyImageImporter plugin" OFF "NOT WITH_COLLADAIMPORTER;NOT WITH_OPENGEXIMPORTER;NOT WITH_ASSIMPIMPORTER" ON)
option(WITH_ANYAUDIOIMPORTER "Build AnyAudioImporter plugin" OFF)
option(WITH_ANYIMAGECONVERTER "Build AnyImageConverter plugin" OFF)
option(WITH_ANYSCENEIMPORTER "Build AnySceneImporter plugin" OFF)
option(WITH_ASSIMPIMPORTER "Build AssimpImporter plugin" OFF)
option(WITH_COLLADAIMPORTER "Build ColladaImporter plugin" OFF)
option(WITH_DDSIMPORTER "Build DdsImporter plugin" OFF)
option(WITH_DEVILIMAGEIMPORTER "Build DevILImageImporter plugin" OFF)
option(WITH_DRFLACAUDIOIMPORTER "Build DrFlacAudioImporter plugin" OFF)
option(WITH_DRWAVAUDIOIMPORTER "Build DrWavAudioImporter plugin" OFF)
cmake_dependent_option(WITH_FREETYPEFONT "Build FreeTypeFont plugin" OFF "NOT WITH_HARFBUZZFONT" ON)
option(WITH_HARFBUZZFONT "Build HarfBuzzFont plugin" OFF)
option(WITH_JPEGIMPORTER "Build JpegImporter plugin" OFF)
option(WITH_MINIEXRIMAGECONVERTER "Build MiniExrImageConverter plugin" OFF)
option(WITH_OPENGEXIMPORTER "Build OpenGexImporter plugin" OFF)
option(WITH_PNGIMAGECONVERTER "Build PngImageConverter plugin" OFF)
option(WITH_PNGIMPORTER "Build PngImporter plugin" OFF)
option(WITH_STANFORDIMPORTER "Build StanfordImporter plugin" OFF)
option(WITH_STBIMAGECONVERTER "Build StbImageConverter plugin" OFF)
option(WITH_STBIMAGEIMPORTER "Build StbImageImporter plugin" OFF)
option(WITH_STBTRUETYPEFONT "Build StbTrueTypeFont plugin" OFF)
option(WITH_STBVORBISAUDIOIMPORTER "Build StbVorbisAudioImporter plugin" OFF)

include(CMakeDependentOption)
option(BUILD_TESTS "Build unit tests" OFF)
cmake_dependent_option(BUILD_GL_TESTS "Build unit tests for OpenGL code" OFF "BUILD_TESTS" OFF)

option(BUILD_STATIC "Build static plugins (default are dynamic)" OFF)
cmake_dependent_option(BUILD_STATIC_PIC "Build static plugins with position-independent code" ON "BUILD_STATIC" OFF)

# Dynamic linking is meaningless on Emscripten and too inconvenient on Android
if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(BUILD_STATIC ON)
endif()

if(CORRADE_TARGET_EMSCRIPTEN)
    include(UseEmscripten)
endif()

if(BUILD_TESTS)
    find_package(Corrade REQUIRED TestSuite)
    if(CORRADE_TARGET_IOS)
        set(CORRADE_TESTSUITE_BUNDLE_IDENTIFIER_PREFIX "cz.mosra.magnum-plugins")
    endif()
    enable_testing()
endif()

if(BUILD_GL_TESTS)
    find_package(Magnum REQUIRED OpenGLTester)
endif()

set(MAGNUMPLUGINS_CMAKE_MODULE_INSTALL_DIR share/cmake/MagnumPlugins)

add_subdirectory(modules)
add_subdirectory(src)
