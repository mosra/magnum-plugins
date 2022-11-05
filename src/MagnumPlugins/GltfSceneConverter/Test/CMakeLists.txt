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
set(CMAKE_FOLDER "MagnumPlugins/GltfSceneConverter/Test")

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(GLTFSCENECONVERTER_TEST_DIR ".")
    set(GLTFSCENECONVERTER_TEST_OUTPUT_DIR "write")
else()
    set(GLTFSCENECONVERTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    set(GLTFSCENECONVERTER_TEST_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

find_package(Magnum REQUIRED DebugTools MeshTools)

if(NOT MAGNUM_GLTFSCENECONVERTER_BUILD_STATIC)
    set(GLTFSCENECONVERTER_PLUGIN_FILENAME $<TARGET_FILE:GltfSceneConverter>)
    if(MAGNUM_WITH_BASISIMAGECONVERTER)
        set(BASISIMAGECONVERTER_PLUGIN_FILENAME $<TARGET_FILE:BasisImageConverter>)
    endif()
    if(MAGNUM_WITH_BASISIMPORTER)
        set(BASISIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:BasisImporter>)
    endif()
    if(MAGNUM_WITH_GLTFIMPORTER)
        set(GLTFIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:GltfImporter>)
    endif()
    if(MAGNUM_WITH_KTXIMAGECONVERTER)
        set(KTXIMAGECONVERTER_PLUGIN_FILENAME $<TARGET_FILE:KtxImageConverter>)
    endif()
    if(MAGNUM_WITH_KTXIMPORTER)
        set(KTXIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:KtxImporter>)
    endif()
    if(MAGNUM_WITH_OPENEXRIMAGECONVERTER)
        set(OPENEXRIMAGECONVERTER_PLUGIN_FILENAME $<TARGET_FILE:OpenExrImageConverter>)
    endif()
    if(MAGNUM_WITH_STBDXTIMAGECONVERTER)
        set(STBDXTIMAGECONVERTER_PLUGIN_FILENAME $<TARGET_FILE:StbDxtImageConverter>)
    endif()
    if(MAGNUM_WITH_STBIMAGECONVERTER)
        set(STBIMAGECONVERTER_PLUGIN_FILENAME $<TARGET_FILE:StbImageConverter>)
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

corrade_add_test(GltfSceneConverterTest GltfSceneConverterTest.cpp
    LIBRARIES
        Magnum::DebugTools
        Magnum::MeshTools
        Magnum::Trade
    FILES
        empty.glb
        empty.gltf
        image-3d-bundled.bin
        image-3d-bundled.gltf
        image-3d-compressed.glb
        image-3d.glb
        image-3d.gltf
        image-3d-no-texture.gltf
        image-3d-not-bundled.glb
        image-3d-not-bundled.0.ktx2
        image-3d.0.ktx2
        image-accessor-names.gltf
        image-basis.glb
        image-bundled.gltf image-bundled.bin
        image.glb
        image.gltf image.0.png
        image-jpeg.glb
        image-ktx.glb
        image-ktx-compressed.glb
        image-ktx-no-extension.glb
        image-multiple.gltf image-multiple.bin image-multiple.1.png
        image-name-accessor-names.gltf
        image-name.gltf
        image-not-bundled.glb image-not-bundled.0.png
        image-tga.glb
        material-2d-array-textures.gltf
        material-alpha-mask-defaults-kept.gltf
        material-alpha-mask-defaults-omitted.gltf
        material-alpha-mask.gltf
        material-common.gltf
        material-defaults-kept.gltf
        material-defaults-omitted.gltf
        material-default-texture-swizzle.gltf
        material-empty.gltf
        material-metallicroughness.gltf
        material-multiple.gltf
        material-name.gltf
        material-unlit.gltf
        mesh-accessor-names.gltf
        mesh-attribute-bitangent.gltf
        mesh-attribute-color3.gltf
        mesh-attribute-color4.gltf
        mesh-attribute-color4us.gltf
        mesh-attribute-matrix2x2b.gltf
        mesh-attribute-matrix3x3s.gltf
        mesh-attribute-matrix4x4b.gltf
        mesh-attribute-normal-quantized.gltf
        mesh-attribute-objectidub.gltf
        mesh-attribute-objectidui.gltf
        mesh-attribute-position-quantized.gltf
        mesh-attribute-tangent.gltf
        mesh-attribute-tangent-quantized.gltf
        mesh-attribute-tangent3.gltf
        mesh-attribute-texture-coordinates.gltf
        mesh-attribute-texture-coordinates-quantized.gltf
        mesh-custom-attribute-no-name.gltf
        mesh-custom-objectid-name.gltf
        mesh-duplicate-attribute.gltf
        mesh.glb
        mesh.gltf mesh.bin
        mesh-multiple.gltf mesh-multiple.bin
        mesh-name-accessor-names.gltf
        mesh-name.gltf
        mesh-no-attributes.gltf
        mesh-no-indices.gltf
        mesh-no-indices-no-attributes.glb
        mesh-no-indices-no-attributes.gltf
        mesh-no-indices-no-vertices.glb
        mesh-no-indices-no-vertices.gltf
        mesh-noninterleaved.gltf
        metadata.gltf
        metadata-explicit-implicit-extensions.gltf
        scene.gltf
        scene-custom-fields.gltf
        scene-empty.gltf
        scene-empty-default.gltf
        scene-invalid.gltf
        scene-meshes-materials.gltf
        scene-name.gltf
        # Not glTFs because there's many combinations
        texcoord-flip-floats.glb
        texcoord-flip-floats-material.glb
        texcoord-flip-normalized-byte-material.glb
        texcoord-flip-normalized-unsigned-byte.glb
        texcoord-flip-normalized-unsigned-byte-material.glb
        texcoord-flip-normalized-unsigned-short.glb
        texcoord-flip-normalized-unsigned-short-material.glb
        texcoord-flip-short-material.glb
        texture.gltf
        texture-basis.gltf
        texture-deduplicated-samplers.gltf
        texture-jpeg.gltf
        texture-ktx.gltf
        texture-ktx-no-extension.gltf
        texture-multiple.gltf
        texture-name.gltf
        texture-tga.gltf)
target_include_directories(GltfSceneConverterTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
if(MAGNUM_GLTFSCENECONVERTER_BUILD_STATIC)
    target_link_libraries(GltfSceneConverterTest PRIVATE GltfSceneConverter)
    if(MAGNUM_WITH_BASISIMAGECONVERTER)
        target_link_libraries(GltfSceneConverterTest PRIVATE BasisImageConverter)
    endif()
    if(MAGNUM_WITH_BASISIMPORTER)
        target_link_libraries(GltfSceneConverterTest PRIVATE BasisImporter)
    endif()
    if(MAGNUM_WITH_GLTFIMPORTER)
        target_link_libraries(GltfSceneConverterTest PRIVATE GltfImporter)
    endif()
    if(MAGNUM_WITH_KTXIMAGECONVERTER)
        target_link_libraries(GltfSceneConverterTest PRIVATE KtxImageConverter)
    endif()
    if(MAGNUM_WITH_KTXIMPORTER)
        target_link_libraries(GltfSceneConverterTest PRIVATE KtxImporter)
    endif()
    if(MAGNUM_WITH_OPENEXRIMAGECONVERTER)
        target_link_libraries(GltfSceneConverterTest PRIVATE OpenExrImageConverter)
    endif()
    if(MAGNUM_WITH_STBDXTIMAGECONVERTER)
        target_link_libraries(GltfSceneConverterTest PRIVATE StbDxtImageConverter)
    endif()
    if(MAGNUM_WITH_STBIMAGECONVERTER)
        target_link_libraries(GltfSceneConverterTest PRIVATE StbImageConverter)
    endif()
    if(MAGNUM_WITH_STBIMAGEIMPORTER)
        target_link_libraries(GltfSceneConverterTest PRIVATE StbImageImporter)
    endif()
else()
    # So the plugins get properly built when building the test
    add_dependencies(GltfSceneConverterTest GltfSceneConverter)
    if(MAGNUM_WITH_BASISIMAGECONVERTER)
        add_dependencies(GltfSceneConverterTest BasisImageConverter)
    endif()
    if(MAGNUM_WITH_BASISIMPORTER)
        add_dependencies(GltfSceneConverterTest BasisImporter)
    endif()
    if(MAGNUM_WITH_GLTFIMPORTER)
        add_dependencies(GltfSceneConverterTest GltfImporter)
    endif()
    if(MAGNUM_WITH_KTXIMAGECONVERTER)
        add_dependencies(GltfSceneConverterTest KtxImageConverter)
    endif()
    if(MAGNUM_WITH_KTXIMPORTER)
        add_dependencies(GltfSceneConverterTest KtxImporter)
    endif()
    if(MAGNUM_WITH_OPENEXRIMAGECONVERTER)
        add_dependencies(GltfSceneConverterTest OpenExrImageConverter)
    endif()
    if(MAGNUM_WITH_STBDXTIMAGECONVERTER)
        add_dependencies(GltfSceneConverterTest StbDxtImageConverter)
    endif()
    if(MAGNUM_WITH_STBIMAGECONVERTER)
        add_dependencies(GltfSceneConverterTest StbImageConverter)
    endif()
    if(MAGNUM_WITH_STBIMAGEIMPORTER)
        add_dependencies(GltfSceneConverterTest StbImageImporter)
    endif()
endif()

# BasisImageConverter needs threads to work, see BasisImageConverter.h for
# details on why it's not linked transitively from the plugin already
if(MAGNUM_WITH_BASISIMAGECONVERTER)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)
    target_link_libraries(GltfSceneConverterTest PRIVATE Threads::Threads)
endif()

if(CORRADE_BUILD_STATIC AND NOT MAGNUM_GLTFSCENECONVERTER_BUILD_STATIC)
    # CMake < 3.4 does this implicitly, but 3.4+ not anymore (see CMP0065).
    # That's generally okay, *except if* the build is static, the executable
    # uses a plugin manager and needs to share globals with the plugins (such
    # as output redirection and so on).
    set_target_properties(GltfSceneConverterTest PROPERTIES ENABLE_EXPORTS ON)
endif()
