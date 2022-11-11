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

cmake_minimum_required(VERSION 3.4)

# Don't restrict INTERPROCEDURAL_OPTIMIZATION only for icc on Linux
if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
endif()
# If CMAKE_AUTOMOC is set, all uses of corrade_add_resource() would otherwise
# complain on 3.10 that AUTOMOC is not processing GENERATED files
if(POLICY CMP0071)
    cmake_policy(SET CMP0071 NEW)
endif()
# Superprojects can use just set(MAGNUM_WITH_BLAH ON) without FORCE CACHE on
# 3.13+
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()

project(MagnumPlugins CXX)

# Use folders for nice tree in Visual Studio and XCode
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/modules/" ${CMAKE_MODULE_PATH})
find_package(Magnum REQUIRED)

include(CMakeDependentOption)

# Options that used to be unprefixed. 45 values for 45 option() and
# cmake_dependent_option() calls. New options shouldn't be added to this list.
set(_MAGNUMPLUGINS_DEPRECATED_UNPREFIXED_OPTIONS
    WITH_ASSIMPIMPORTER
    WITH_ASTCIMPORTER
    WITH_BASISIMAGECONVERTER
    WITH_BASISIMPORTER
    WITH_DDSIMPORTER
    WITH_DEVILIMAGEIMPORTER
    WITH_DRFLACAUDIOIMPORTER
    WITH_DRMP3AUDIOIMPORTER
    WITH_DRWAVAUDIOIMPORTER
    WITH_FAAD2AUDIOIMPORTER
    WITH_FREETYPEFONT
    WITH_GLSLANGSHADERCONVERTER
    WITH_GLTFIMPORTER
    WITH_HARFBUZZFONT
    WITH_ICOIMPORTER
    WITH_JPEGIMAGECONVERTER
    WITH_JPEGIMPORTER
    WITH_KTXIMAGECONVERTER
    WITH_KTXIMPORTER
    WITH_MESHOPTIMIZERSCENECONVERTER
    WITH_MINIEXRIMAGECONVERTER
    WITH_OPENDDL
    WITH_OPENEXRIMAGECONVERTER
    WITH_OPENEXRIMPORTER
    WITH_OPENGEXIMPORTER
    WITH_PNGIMAGECONVERTER
    WITH_PNGIMPORTER
    WITH_PRIMITIVEIMPORTER
    WITH_SPIRVTOOLSSHADERCONVERTER
    WITH_STANFORDSCENECONVERTER
    WITH_STANFORDIMPORTER
    WITH_STBDXTIMAGECONVERTER
    WITH_STBIMAGECONVERTER
    WITH_STBIMAGEIMPORTER
    WITH_STBTRUETYPEFONT
    WITH_STBVORBISAUDIOIMPORTER
    WITH_STLIMPORTER
    WITH_WEBPIMPORTER
    BUILD_STATIC
    BUILD_STATIC_PIC
    BUILD_PLUGINS_STATIC
    BUILD_TESTS
    BUILD_GL_TESTS)
if(MAGNUM_BUILD_DEPRECATED)
    list(APPEND _MAGNUMPLUGINS_DEPRECATED_UNPREFIXED_OPTIONS
        WITH_CGLTFIMPORTER
        WITH_TINYGLTFIMPORTER)
endif()
# If during the first run (i.e., when the variable isn't in cache yet), check
# if any of the prefixed options are already set, except for BUILD_STATIC which
# may already be coming from Magnum's configure.h. If so, we assume the user is
# already switched to the prefixed options and won't accept the deprecated
# unprefixed options for backwards compatibility. This way it's possible for
# projects to reuse these variables for other purposes without affecting
# Corrade in any way.
if(NOT DEFINED _MAGNUMPLUGINS_ACCEPT_DEPRECATED_UNPREFIXED_OPTIONS)
    set(_MAGNUMPLUGINS_ACCEPT_DEPRECATED_UNPREFIXED_OPTIONS ON CACHE INTERNAL "")
    foreach(option ${_MAGNUMPLUGINS_DEPRECATED_UNPREFIXED_OPTIONS})
        if(NOT "${option}" STREQUAL "BUILD_STATIC" AND DEFINED MAGNUM_${option})
            set(_MAGNUMPLUGINS_ACCEPT_DEPRECATED_UNPREFIXED_OPTIONS OFF CACHE INTERNAL "")
            break()
        endif()
    endforeach()
endif()

# Plugins to build
option(MAGNUM_WITH_ASSIMPIMPORTER "Build AssimpImporter plugin" OFF)
option(MAGNUM_WITH_ASTCIMPORTER "Build AstcImporter plugin" OFF)
option(MAGNUM_WITH_BASISIMAGECONVERTER "Build BasisImageConverter plugin" OFF)
option(MAGNUM_WITH_BASISIMPORTER "Build BasisImporter plugin" OFF)
if(MAGNUM_BUILD_DEPRECATED)
    option(MAGNUM_WITH_CGLTFIMPORTER "Build CgltfImporter plugin" OFF)
endif()
option(MAGNUM_WITH_DDSIMPORTER "Build DdsImporter plugin" OFF)
option(MAGNUM_WITH_DEVILIMAGEIMPORTER "Build DevIlImageImporter plugin" OFF)
option(MAGNUM_WITH_DRFLACAUDIOIMPORTER "Build DrFlacAudioImporter plugin" OFF)
option(MAGNUM_WITH_DRMP3AUDIOIMPORTER "Build DrMp3AudioImporter plugin" OFF)
option(MAGNUM_WITH_DRWAVAUDIOIMPORTER "Build DrWavAudioImporter plugin" OFF)
option(MAGNUM_WITH_FAAD2AUDIOIMPORTER "Build Faad2AudioImporter plugin" OFF)
cmake_dependent_option(MAGNUM_WITH_FREETYPEFONT "Build FreeTypeFont plugin" OFF "NOT MAGNUM_WITH_HARFBUZZFONT" ON)
option(MAGNUM_WITH_GLSLANGSHADERCONVERTER "Build GlslangShaderConverter plugin" OFF)
# TODO: make an option() again when CgltfImporter is gone
cmake_dependent_option(MAGNUM_WITH_GLTFIMPORTER "Build GltfImporter plugin" OFF "NOT MAGNUM_WITH_CGLTFIMPORTER" ON)
option(MAGNUM_WITH_GLTFSCENECONVERTER "Build GltfSceneConverter plugin" OFF)
option(MAGNUM_WITH_HARFBUZZFONT "Build HarfBuzzFont plugin" OFF)
option(MAGNUM_WITH_ICOIMPORTER "Build IcoImporter plugin" OFF)
option(MAGNUM_WITH_JPEGIMAGECONVERTER "Build JpegImageConverter plugin" OFF)
option(MAGNUM_WITH_JPEGIMPORTER "Build JpegImporter plugin" OFF)
option(MAGNUM_WITH_KTXIMAGECONVERTER "Build KtxImageConverter plugin" OFF)
option(MAGNUM_WITH_KTXIMPORTER "Build KtxImporter plugin" OFF)
option(MAGNUM_WITH_MESHOPTIMIZERSCENECONVERTER "Build MeshOptimizerSceneConverter plugin" OFF)
option(MAGNUM_WITH_MINIEXRIMAGECONVERTER "Build MiniExrImageConverter plugin" OFF)
cmake_dependent_option(MAGNUM_WITH_OPENDDL "Build OpenDdl library" OFF "NOT MAGNUM_WITH_OPENGEXIMPORTER" ON)
option(MAGNUM_WITH_OPENEXRIMAGECONVERTER "Build OpenExrImageConverter plugin" OFF)
option(MAGNUM_WITH_OPENEXRIMPORTER "Build OpenExrImporter plugin" OFF)
option(MAGNUM_WITH_OPENGEXIMPORTER "Build OpenGexImporter plugin" OFF)
option(MAGNUM_WITH_PNGIMAGECONVERTER "Build PngImageConverter plugin" OFF)
option(MAGNUM_WITH_PNGIMPORTER "Build PngImporter plugin" OFF)
option(MAGNUM_WITH_PRIMITIVEIMPORTER "Build PrimitiveImporter plugin" OFF)
option(MAGNUM_WITH_SPIRVTOOLSSHADERCONVERTER "Build SpirvToolsShaderConverter plugin" OFF)
option(MAGNUM_WITH_STANFORDIMPORTER "Build StanfordImporter plugin" OFF)
option(MAGNUM_WITH_STANFORDSCENECONVERTER "Build StanfordSceneConverter plugin" OFF)
option(MAGNUM_WITH_STBDXTIMAGECONVERTER "Build StbDxtImageConverter plugin" OFF)
option(MAGNUM_WITH_STBIMAGECONVERTER "Build StbImageConverter plugin" OFF)
option(MAGNUM_WITH_STBIMAGEIMPORTER "Build StbImageImporter plugin" OFF)
option(MAGNUM_WITH_STBRESIZEIMAGECONVERTER "Build StbResizeImageConverter plugin" OFF)
option(MAGNUM_WITH_STBTRUETYPEFONT "Build StbTrueTypeFont plugin" OFF)
option(MAGNUM_WITH_STBVORBISAUDIOIMPORTER "Build StbVorbisAudioImporter plugin" OFF)
option(MAGNUM_WITH_STLIMPORTER "Build StlImporter plugin" OFF)
if(MAGNUM_BUILD_DEPRECATED)
    option(MAGNUM_WITH_TINYGLTFIMPORTER "Build TinyGltfImporter plugin" OFF)
endif()
option(MAGNUM_WITH_WEBPIMPORTER "Build WebPImporter plugin" OFF)

option(MAGNUM_BUILD_TESTS "Build unit tests" OFF)
cmake_dependent_option(MAGNUM_BUILD_GL_TESTS "Build unit tests for OpenGL code" OFF "MAGNUM_BUILD_TESTS" OFF)

option(MAGNUM_BUILD_STATIC "Build static libraries (default are dynamic)" OFF)
option(MAGNUM_BUILD_PLUGINS_STATIC "Build static plugins (default are dynamic)" OFF)
# Disable PIC on Emscripten by default (but still allow it to be enabled
# explicitly if one so desires). Currently causes linker errors related to
# __memory_base etc.: https://github.com/emscripten-core/emscripten/issues/8761
if(CORRADE_TARGET_EMSCRIPTEN)
    set(ON_EXCEPT_EMSCRIPTEN OFF)
else()
    set(ON_EXCEPT_EMSCRIPTEN ON)
endif()
option(MAGNUM_BUILD_STATIC_PIC "Build static plugins with position-independent code" ${ON_EXCEPT_EMSCRIPTEN})

# Backwards compatibility for unprefixed CMake options. If the user isn't
# explicitly using prefixed options in the first run already, accept the
# unprefixed options, and remember this decision for subsequent runs
if(NOT DEFINED _MAGNUMPLUGINS_ACCEPT_DEPRECATED_UNPREFIXED_OPTIONS)
    set(_MAGNUMPLUGINS_ACCEPT_DEPRECATED_UNPREFIXED_OPTIONS ON CACHE INTERNAL "")
endif()
# If the user wasn't explicitly using prefixed options in the first run and the
# MAGNUM_BUILD_DEPRECATED option is not currently disabled (which can get
# changed subsequently), accept the unprefixed options and print a warning if
# they're different from the prefixed ones.
if(_MAGNUMPLUGINS_ACCEPT_DEPRECATED_UNPREFIXED_OPTIONS AND MAGNUM_BUILD_DEPRECATED)
    # The following variables need extra care -- mirroring what's done in
    # cmake_dependent_option(), but only the cases that enable a dependency
    # that is off by default, not cases that hide an option from the GUI when
    # it's needed by something else. In other words, if the original user flag
    # setup didn't make sense before but the option dependency fixed it, it'll
    # be broken now, but if it made sense, it should keep working.
    #
    # Doing this before propagating the unprefixed options to avoid a
    # false-positive warning when e.g. MAGNUM_BUILD_STATIC_PIC is implicitly ON
    # but BUILD_STATIC_PIC not yet.
    if(BUILD_STATIC)
        if(NOT CORRADE_TARGET_EMSCRIPTEN AND NOT DEFINED BUILD_STATIC_PIC)
            set(BUILD_STATIC_PIC ON)
        endif()
    endif()
    if(WITH_HARFBUZZFONT)
        if(NOT DEFINED WITH_FREETYPEFONT)
            set(WITH_FREETYPEFONT ON)
        endif()
    endif()
    if(WITH_CGLTFIMPORTER)
        if(NOT DEFINED WITH_GLTFIMPORTER)
            set(WITH_GLTFIMPORTER ON)
        endif()
    endif()
    if(WITH_OPENGEXIMPORTER)
        if(NOT DEFINED WITH_OPENDDL)
            set(WITH_OPENDDL ON)
        endif()
    endif()

    set(_MAGNUMPLUGINS_WARN_DEPRECATED_UNPREFIXED_OPTION )
    foreach(option ${_MAGNUMPLUGINS_DEPRECATED_UNPREFIXED_OPTIONS})
        if(DEFINED ${option})
            # CMake has no comparison of boolean values (EQUAL returns false if
            # comparing ON and 1 or OFF and FALSE, STREQUAL also), so we have
            # to do it this way. Also warn only on the first encountered
            # variable so people can fix it, reconfigure and go to the next one
            # that warns.
            if((${option} AND NOT MAGNUM_${option}) OR
               (NOT ${option} AND MAGNUM_${option}) AND NOT _MAGNUMPLUGINS_WARN_DEPRECATED_UNPREFIXED_OPTION)
                set(_MAGNUMPLUGINS_WARN_DEPRECATED_UNPREFIXED_OPTION ${option})
            endif()
            set(MAGNUM_${option} ${${option}})
            # If variables specified on the command line don't match any
            # options, they're kept in cache but set as UNINITIALIZED, meaning
            # they don't appear in cmake-gui or ccmake, so there's no way to
            # fix the warning apart from hand-enditing the CMakeCache.txt or
            # recreating the build dir. Update their cached type to be BOOL to
            # make them appear.
            set(${option} ${${option}} CACHE BOOL "Deprecated, use MAGNUM_${option} instead" FORCE)
        endif()
    endforeach()

    if(_MAGNUMPLUGINS_WARN_DEPRECATED_UNPREFIXED_OPTION)
        # CMake 3.5+ has deprecation warnings enabled by default (which makes
        # sense), 3.4 not. Use a warning there instead.
        # TODO: drop when 3.4 is not supported anymore
        if(CMAKE_VERSION VERSION_LESS 3.5)
            set(DEPRECATION_OR_WARNING WARNING)
        else()
            set(DEPRECATION_OR_WARNING DEPRECATION)
        endif()
        message(${DEPRECATION_OR_WARNING} "Unprefixed options such as ${_MAGNUMPLUGINS_WARN_DEPRECATED_UNPREFIXED_OPTION} are deprecated, use MAGNUM_${_MAGNUMPLUGINS_WARN_DEPRECATED_UNPREFIXED_OPTION} instead. Delete the unprefixed variable from CMake cache or set both to the same value to silence this warning.")
    endif()
endif()

# Dynamic linking is meaningless on Emscripten and too inconvenient on Android
if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(MAGNUM_BUILD_STATIC ON)
    set(MAGNUM_BUILD_PLUGINS_STATIC ON)
# Dynamic plugins are not working on iOS or Windows RT at the moment
elseif(CORRADE_TARGET_IOS OR CORRADE_TARGET_WINDOWS_RT)
    set(MAGNUM_BUILD_PLUGINS_STATIC ON)
endif()

if(MAGNUM_BUILD_TESTS)
    find_package(Corrade REQUIRED TestSuite)
    if(CORRADE_TARGET_IOS)
        set(CORRADE_TESTSUITE_BUNDLE_IDENTIFIER_PREFIX "cz.mosra.magnum-plugins")
    endif()
    enable_testing()

    # If CORRADE_TESTSUITE_TEST_TARGET is set, tests aren't built by default
    # (in the ALL target) but instead set as dependencies of a target named
    # after the value of CORRADE_TESTSUITE_TEST_TARGET. This is a copy of
    # what's done in corrade_add_test(), because we also build various test
    # libraries and plugins in addition to the test executables.
    if(CORRADE_TESTSUITE_TEST_TARGET)
        if(NOT TARGET ${CORRADE_TESTSUITE_TEST_TARGET})
            add_custom_target(${CORRADE_TESTSUITE_TEST_TARGET})
        endif()
        set(EXCLUDE_FROM_ALL_IF_TEST_TARGET EXCLUDE_FROM_ALL)
    endif()
endif()

if(MAGNUM_BUILD_GL_TESTS)
    # Not needed by anything at the moment, and we want to keep
    # MAGNUM_BUILD_GL_TESTS enabled on the CIs to catch cases of plugins
    # needing GL again.
    #find_package(Magnum REQUIRED OpenGLTester)
endif()

if(NOT MAGNUM_BUILD_STATIC)
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
#   Utility::Path::libraryLocation().
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

# Build snippets as part of testing. Unlike all other Test/ directories, this
# one isn't added with ${EXCLUDE_FROM_ALL_IF_TEST_TARGET} because the targets
# from there aren't referenced by CTest and thus it'd likely happen that they
# accidentally don't get added as a dependency to that target, causing them to
# be never built. Instead, each target there is handled separately, to minimize
# the chance of an accident.
if(MAGNUM_BUILD_TESTS)
    add_subdirectory(doc/snippets)
endif()
