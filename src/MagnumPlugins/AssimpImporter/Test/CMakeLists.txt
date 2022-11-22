#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2021 Pablo Escobar <mail@rvrs.in>
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
set(CMAKE_FOLDER "MagnumPlugins/AssimpImporter/Test")

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(ASSIMPIMPORTER_TEST_DIR ".")
else()
    set(ASSIMPIMPORTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if(NOT MAGNUM_ASSIMPIMPORTER_BUILD_STATIC)
    set(ASSIMPIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:AssimpImporter>)
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

corrade_add_test(AssimpImporterTest AssimpImporterTest.cpp
    LIBRARIES Magnum::Trade
    FILES
        animation.gltf # Same as in TinyGltfImporterTest, but added a scene
        animation.bin
        animation-no-scene.gltf
        animation-patching.bin
        animation-patching.gltf
        camera.dae
        camera-light-referenced-by-two-nodes.dae
        camera-orthographic.gltf
        diffuse_texture.png
        embedded-texture.blend
        empty.dae
        empty.gltf
        exported-animation.dae # Converted from exported-animation.blend
        exported-animation.fbx
        exported-animation.gltf
        # Created with Blender 3.1.0 as follows:
        # - Delete default scene
        # - Add plane
        # - Set the diffuse color to image texture with this image: embedded
        # - File > Export > FBX, chose path mode "copy" and press the button
        #   next to the dropdown, which enables embedding images
        image-embedded.fbx
        image-filename-trailing-space.mtl
        image-filename-trailing-space.obj
        image-filename-backslash.mtl
        image-filename-backslash.obj
        image-3d.mtl
        image-3d.obj
        image-3d.dds # Copied from DdsImporter/Test/dxt10-rgba8unorm-3d.dds
        image-mips.mtl
        image-mips.obj
        image-mips.dds # Copied from DdsImporter/Test/bgr8unorm-mips.dds
        light.dae
        light-area.fbx # Converted from light-area.blend
        material-color.dae
        material-texture.dae
        material-texture-coordinates.dae
        material-texture-matrix.gltf
        material-color-texture.mtl
        material-color-texture.obj
        material-layers.fbx
        material-raw.fbx
        material-raw.gltf
        mesh.dae
        mesh-multiple-primitives.dae
        multiple-textures.mtl r.png g.png b.png y.png
        points.obj
        polygon.obj
        scene.dae
        scene+mesh.dae
        scene-name.gltf
        skin.dae # Converted from skin.blend
        skin.fbx
        skin.gltf
        skin-dummy-weights.bin
        skin-dummy-weights.gltf
        skin-multiple-sets.dae
        skin-multiple-sets.bin
        skin-multiple-sets.gltf
        skin-no-mesh.bin
        skin-no-mesh.gltf
        skin-shared.gltf
        skin-shared.bin
        texture-ambient.obj
        texture-ambient.mtl
        quad.stl
        y-up.dae
        z-up.dae)
target_link_libraries(AssimpImporterTest PRIVATE Assimp::Assimp)
target_include_directories(AssimpImporterTest PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>
    # The test needs access to configureInternal.h written by
    # AssimpImporter. The dynamic library doesn't get linked to
    # and hence doesn't get the binary dir in the include dirs.
    ${PROJECT_BINARY_DIR}/src)
if(MAGNUM_ASSIMPIMPORTER_BUILD_STATIC)
    target_link_libraries(AssimpImporterTest PRIVATE AssimpImporter)
    if(MAGNUM_WITH_DDSIMPORTER)
        target_link_libraries(AssimpImporterTest PRIVATE DdsImporter)
    endif()
    if(MAGNUM_WITH_STBIMAGEIMPORTER)
        target_link_libraries(AssimpImporterTest PRIVATE StbImageImporter)
    endif()
else()
    # So the plugins get properly built when building the test
    add_dependencies(AssimpImporterTest AssimpImporter)
    if(MAGNUM_WITH_DDSIMPORTER)
        add_dependencies(AssimpImporterTest DdsImporter)
    endif()
    if(MAGNUM_WITH_STBIMAGEIMPORTER)
        add_dependencies(AssimpImporterTest StbImageImporter)
    endif()
endif()
if(CORRADE_BUILD_STATIC AND NOT MAGNUM_ASSIMPIMPORTER_BUILD_STATIC)
    # CMake < 3.4 does this implicitly, but 3.4+ not anymore (see CMP0065).
    # That's generally okay, *except if* the build is static, the executable
    # uses a plugin manager and needs to share globals with the plugins (such
    # as output redirection and so on).
    set_target_properties(AssimpImporterTest PROPERTIES ENABLE_EXPORTS ON)
endif()
