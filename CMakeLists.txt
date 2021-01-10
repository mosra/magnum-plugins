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

cmake_minimum_required(VERSION 3.4)

project(MagnumPlugins CXX)

# Use folders for nice tree in Visual Studio and XCode
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/modules/" ${CMAKE_MODULE_PATH})
find_package(Magnum REQUIRED)

include(CMakeDependentOption)

# Plugins to build
option(WITH_ASSIMPIMPORTER "Build AssimpImporter plugin" OFF)
option(WITH_BASISIMAGECONVERTER "Build BasisImageConverter plugin" OFF)
option(WITH_BASISIMPORTER "Build BasisImporter plugin" OFF)
option(WITH_DDSIMPORTER "Build DdsImporter plugin" OFF)
option(WITH_DEVILIMAGEIMPORTER "Build DevIlImageImporter plugin" OFF)
option(WITH_DRFLACAUDIOIMPORTER "Build DrFlacAudioImporter plugin" OFF)
option(WITH_DRMP3AUDIOIMPORTER "Build DrMp3AudioImporter plugin" OFF)
option(WITH_DRWAVAUDIOIMPORTER "Build DrWavAudioImporter plugin" OFF)
option(WITH_FAAD2AUDIOIMPORTER "Build Faad2AudioImporter plugin" OFF)
cmake_dependent_option(WITH_FREETYPEFONT "Build FreeTypeFont plugin" OFF "NOT WITH_HARFBUZZFONT" ON)
option(WITH_GLSLANGSHADERCONVERTER "Build GlslangShaderConverter plugin" OFF)
option(WITH_HARFBUZZFONT "Build HarfBuzzFont plugin" OFF)
option(WITH_ICOIMPORTER "Build IcoImporter plugin" OFF)
option(WITH_JPEGIMAGECONVERTER "Build JpegImageConverter plugin" OFF)
option(WITH_JPEGIMPORTER "Build JpegImporter plugin" OFF)
option(WITH_MESHOPTIMIZERSCENECONVERTER "Build MeshOptimizerSceneConverter plugin" OFF)
option(WITH_MINIEXRIMAGECONVERTER "Build MiniExrImageConverter plugin" OFF)
cmake_dependent_option(WITH_OPENDDL "Build OpenDdl library" OFF "NOT WITH_OPENGEXIMPORTER" ON)
option(WITH_OPENGEXIMPORTER "Build OpenGexImporter plugin" OFF)
option(WITH_PNGIMAGECONVERTER "Build PngImageConverter plugin" OFF)
option(WITH_PNGIMPORTER "Build PngImporter plugin" OFF)
option(WITH_PRIMITIVEIMPORTER "Build PrimitiveImporter plugin" OFF)
option(WITH_SPIRVTOOLSSHADERCONVERTER "Build SpirvToolsShaderConverter plugin" OFF)
option(WITH_STANFORDIMPORTER "Build StanfordImporter plugin" OFF)
option(WITH_STANFORDSCENECONVERTER "Build StanfordSceneConverter plugin" OFF)
option(WITH_STBIMAGECONVERTER "Build StbImageConverter plugin" OFF)
option(WITH_STBIMAGEIMPORTER "Build StbImageImporter plugin" OFF)
option(WITH_STBTRUETYPEFONT "Build StbTrueTypeFont plugin" OFF)
option(WITH_STBVORBISAUDIOIMPORTER "Build StbVorbisAudioImporter plugin" OFF)
option(WITH_STLIMPORTER "Build StlImporter plugin" OFF)
option(WITH_TINYGLTFIMPORTER "Build TinyGltfImporter plugin" OFF)

include(CMakeDependentOption)
option(BUILD_TESTS "Build unit tests" OFF)
cmake_dependent_option(BUILD_GL_TESTS "Build unit tests for OpenGL code" OFF "BUILD_TESTS" OFF)

option(BUILD_STATIC "Build static libraries (default are dynamic)" OFF)
option(BUILD_PLUGINS_STATIC "Build static plugins (default are dynamic)" OFF)
# Disable PIC on Emscripten by default (but still allow it to be enabled
# explicitly if one so desires). Currently causes linker errors related to
# __memory_base etc.: https://github.com/emscripten-core/emscripten/issues/8761
if(CORRADE_TARGET_EMSCRIPTEN)
    set(ON_EXCEPT_EMSCRIPTEN OFF)
else()
    set(ON_EXCEPT_EMSCRIPTEN ON)
endif()
option(BUILD_STATIC_PIC "Build static plugins with position-independent code" ${ON_EXCEPT_EMSCRIPTEN})

# Dynamic linking is meaningless on Emscripten and too inconvenient on Android
if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(BUILD_STATIC ON)
    set(BUILD_PLUGINS_STATIC ON)
# Dynamic plugins are not working on iOS or Windows RT at the moment
elseif(CORRADE_TARGET_IOS OR CORRADE_TARGET_WINDOWS_RT)
    set(BUILD_PLUGINS_STATIC ON)
endif()

if(BUILD_TESTS)
    find_package(Corrade REQUIRED TestSuite)
    if(CORRADE_TARGET_IOS)
        set(CORRADE_TESTSUITE_BUNDLE_IDENTIFIER_PREFIX "cz.mosra.magnum-plugins")
    endif()
    enable_testing()
endif()

if(BUILD_GL_TESTS)
    # Not needed by anything at the moment, and we want to keep BUILD_GL_TESTS
    # enabled on the CIs to catch cases of plugins needing GL again.
    #find_package(Magnum REQUIRED OpenGLTester)
endif()

if(NOT BUILD_STATIC)
    set(SHARED_OR_STATIC SHARED)
else()
    set(SHARED_OR_STATIC STATIC)
endif()

set(MAGNUMPLUGINS_CMAKE_MODULE_INSTALL_DIR share/cmake/MagnumPlugins)

# Library version. MAGNUMPLUGINS_VERSION_YEAR/MONTH is used in
# src/Magnum/CMakeLists.txt to generate the versionPlugins.h header.
set(MAGNUMPLUGINS_LIBRARY_VERSION 2.4)
set(MAGNUMPLUGINS_LIBRARY_SOVERSION 2)
set(MAGNUMPLUGINS_VERSION_YEAR 2020)
set(MAGNUMPLUGINS_VERSION_MONTH 6)

# A single output location. After a decade of saying NO THIS IS A NON-SOLUTION
# TO A NON-PROBLEM I reconsidered my views and enabled this, because:
#
# - On Windows (which don't have RPATH), this makes test execution finally
#   possible without having to install all the stuff first (including the
#   test-only libs, which is ugh).
# - With CMake subprojects, this makes it finally possible to use dynamic
#   plugins directly from the build dir (again without installing anything) ---
#   all plugins are put into the same place, so PluginManager has a single
#   place to look into; and thanks to the dynamic libraries being there as
#   well, this location can be automagically detected as relative to
#   Directory::libraryLocation().
# - Thanks to the $<CONFIG> being part of the output path, you are always sure
#   you never accidentally mix up debug/release libraries when switching
#   CMAKE_BUILD_TYPE in an existing build dir.
#
# The runtime location is set to CMAKE_BINARY_DIR and not PROJECT_BINARY_DIR
# because have one runtime location per CMake subproject would not solve much
# either. If the user already provides CMAKE_RUNTIME_OUTPUT_DIRECTORY (even
# empty), it's respected and nothing is being done.
#
# Explicitly using a generator expression to ensure plugins are added to e.g.
# <CONFIG>/lib/magnum/importers/ instead of lib/magnum/importers/<CONFIG>. Also
# adding this to cache, making superprojects pick that up implicitly as well,
# without forcing them to explicitly mirror this setting.
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY AND NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY AND NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/bin CACHE PATH "" FORCE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/lib CACHE PATH "" FORCE)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/lib CACHE PATH "" FORCE)
endif()

add_subdirectory(modules)
add_subdirectory(src)

# Build snippets as part of testing
if(BUILD_TESTS)
    add_subdirectory(doc/snippets)
endif()
