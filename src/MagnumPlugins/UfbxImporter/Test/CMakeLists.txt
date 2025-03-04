#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022, 2023, 2024, 2025
#             Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2022, 2023 Samuli Raivio <bqqbarbhg@gmail.com>
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
set(CMAKE_FOLDER "MagnumPlugins/UfbxImporter/Test")

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(UFBXIMPORTER_TEST_DIR ".")
else()
    set(UFBXIMPORTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

find_package(Magnum REQUIRED DebugTools)

if(NOT MAGNUM_UFBXIMPORTER_BUILD_STATIC)
    set(UFBXIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:UfbxImporter>)
    if(MAGNUM_WITH_DDSIMPORTER)
        set(DDSIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:DdsImporter>)
    endif()
    if(MAGNUM_WITH_STBIMAGEIMPORTER)
        set(STBIMAGEIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:StbImageImporter>)
    endif()
endif()

# First replace ${} variables, then $<> generator expressions
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/configure.h
    INPUT ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)

corrade_add_test(UfbxImporterTest UfbxImporterTest.cpp
    LIBRARIES
        Magnum::DebugTools
        Magnum::Trade
    FILES
        blender-default.fbx
        blender-materials.fbx
        instanced-material.fbx
        maya-material-arnold.fbx
        maya-material-phong.fbx
        max-material-physical.fbx
        max-material-spec-gloss.fbx
        layered-pbr-textures.fbx
        geometric-transform.fbx
        texture-transform.fbx
        texture-wrap-modes.fbx
        mesh-point-line.fbx
        triangle.fbx
        cameras.fbx
        lights.fbx
        bad-decay-lights.fbx
        image-absolute-path.fbx
        image-broken-external.fbx
        image-broken-embedded.fbx
        units-cm-z-up.fbx
        maya-camera-light-orientation.fbx
        geometry-cache.fbx
        static-skin.fbx
        warning-cube.fbx
        animation-interpolation.fbx
        animation-rotation-pivot.fbx
        animation-visibility.fbx
        animation-pre-rotation.fbx
        animation-layer.fbx
        animation-layer-nonlinear-weight.fbx
        animation-stacks.fbx
        animation-space-y-up-cm.fbx
        animation-space-y-up-m.fbx
        animation-space-z-up-cm.fbx
        animation-space-z-up-m.fbx
        animation-multi-clip.fbx
        skinning.fbx
        skinning-blender.fbx
        cube.obj
        cube.mtl
        missing-mtl.obj
        image-3d.obj
        image-3d.mtl
        image-3d.dds
        tex-black.png
        tex-blue.png
        tex-cyan.png
        tex-green.png
        tex-pink.png
        tex-red.png
        tex-white.png
        tex-yellow.png
        tex-broken.png)

target_include_directories(UfbxImporterTest PRIVATE ${PROJECT_SOURCE_DIR}/src/external/ufbx)
target_include_directories(UfbxImporterTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
if(MAGNUM_UFBXIMPORTER_BUILD_STATIC)
    target_link_libraries(UfbxImporterTest PRIVATE UfbxImporter)
    if(MAGNUM_WITH_DDSIMPORTER)
        target_link_libraries(UfbxImporterTest PRIVATE DdsImporter)
    endif()
    if(MAGNUM_WITH_STBIMAGEIMPORTER)
        target_link_libraries(UfbxImporterTest PRIVATE StbImageImporter)
    endif()
else()
    # So the plugins get properly built when building the test
    add_dependencies(UfbxImporterTest UfbxImporter)
    if(MAGNUM_WITH_DDSIMPORTER)
        add_dependencies(UfbxImporterTest DdsImporter)
    endif()
    if(MAGNUM_WITH_STBIMAGEIMPORTER)
        add_dependencies(UfbxImporterTest StbImageImporter)
    endif()
endif()
if(CORRADE_BUILD_STATIC AND NOT MAGNUM_UFBXIMPORTER_BUILD_STATIC)
    # CMake < 3.4 does this implicitly, but 3.4+ not anymore (see CMP0065).
    # That's generally okay, *except if* the build is static, the executable
    # uses a plugin manager and needs to share globals with the plugins (such
    # as output redirection and so on).
    set_target_properties(UfbxImporterTest PROPERTIES ENABLE_EXPORTS ON)
endif()
