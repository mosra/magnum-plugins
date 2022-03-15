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

# IDE folder in VS, Xcode etc. CMake 3.12+, older versions have only the FOLDER
# property that would have to be set on each target separately.
set(CMAKE_FOLDER "MagnumPlugins/OpenGexImporter/Test")

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(OPENGEXIMPORTER_TEST_DIR ".")
else()
    set(OPENGEXIMPORTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# CMake before 3.8 has broken $<TARGET_FILE*> expressions for iOS (see
# https://gitlab.kitware.com/cmake/cmake/merge_requests/404) and since Corrade
# doesn't support dynamic plugins on iOS, this sorta works around that. Should
# be revisited when updating Travis to newer Xcode (xcode7.3 has CMake 3.6).
if(NOT MAGNUM_OPENGEXIMPORTER_BUILD_STATIC)
    set(OPENGEXIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:OpenGexImporter>)
    if(WITH_DDSIMPORTER)
        set(DDSIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:DdsImporter>)
    endif()
    if(WITH_STBIMAGEIMPORTER)
        set(STBIMAGEIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:StbImageImporter>)
    endif()
endif()

# First replace ${} variables, then $<> generator expressions
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/configure.h
    INPUT ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)

corrade_add_test(OpenGexImporterTest OpenGexImporterTest.cpp
    LIBRARIES Magnum::Trade MagnumOpenDdl
    FILES
        camera-invalid.ogex
        camera-metrics.ogex
        camera.ogex
        extension.ogex
        image.tga
        light-invalid.ogex
        light.ogex
        material-invalid.ogex
        material.ogex
        mesh-invalid-int64.ogex
        mesh-invalid.ogex
        mesh-metrics.ogex
        mesh.ogex
        mips.dds
        scene-camera.ogex
        scene-geometry.ogex
        scene-invalid-camera.ogex
        scene-invalid-geometry.ogex
        scene-invalid-light.ogex
        scene-invalid-rotation-array-size.ogex
        scene-invalid-rotation-kind.ogex
        scene-invalid-rotation-object-only.ogex
        scene-invalid-scaling-array-size.ogex
        scene-invalid-scaling-kind.ogex
        scene-invalid-scaling-object-only.ogex
        scene-invalid-transformation-array-size.ogex
        scene-invalid-transformation-object-only.ogex
        scene-invalid-translation-array-size.ogex
        scene-invalid-translation-kind.ogex
        scene-invalid-translation-object-only.ogex
        scene-light.ogex
        scene.ogex
        scene-rotation.ogex
        scene-scaling.ogex
        scene-transformation-concatenation.ogex
        scene-transformation-metrics.ogex
        scene-transformation.ogex
        scene-translation.ogex
        texture-invalid.ogex
        texture.ogex
        texture-mips.ogex
        texture-unique.ogex)
target_include_directories(OpenGexImporterTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
if(MAGNUM_OPENGEXIMPORTER_BUILD_STATIC)
    target_link_libraries(OpenGexImporterTest PRIVATE OpenGexImporter)
    if(WITH_DDSIMPORTER)
        target_link_libraries(OpenGexImporterTest PRIVATE DdsImporter)
    endif()
    if(WITH_STBIMAGEIMPORTER)
        target_link_libraries(OpenGexImporterTest PRIVATE StbImageImporter)
    endif()
else()
    # So the plugins get properly built when building the test
    add_dependencies(OpenGexImporterTest OpenGexImporter)
    if(WITH_DDSIMPORTER)
        add_dependencies(OpenGexImporterTest DdsImporter)
    endif()
    if(WITH_STBIMAGEIMPORTER)
        add_dependencies(OpenGexImporterTest StbImageImporter)
    endif()
endif()
if(CORRADE_BUILD_STATIC AND NOT MAGNUM_OPENGEXIMPORTER_BUILD_STATIC)
    # CMake < 3.4 does this implicitly, but 3.4+ not anymore (see CMP0065).
    # That's generally okay, *except if* the build is static, the executable
    # uses a plugin manager and needs to share globals with the plugins (such
    # as output redirection and so on).
    set_target_properties(OpenGexImporterTest PROPERTIES ENABLE_EXPORTS ON)
endif()
