#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(TINYGLTFIMPORTER_TEST_DIR ".")
    set(CGLTFIMPORTER_TEST_DIR ".")
else()
    set(TINYGLTFIMPORTER_TEST_DIR ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test)
    set(CGLTFIMPORTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# CMake before 3.8 has broken $<TARGET_FILE*> expressions for iOS (see
# https://gitlab.kitware.com/cmake/cmake/merge_requests/404) and since Corrade
# doesn't support dynamic plugins on iOS, this sorta works around that. Should
# be revisited when updating Travis to newer Xcode (xcode7.3 has CMake 3.6).
if(NOT MAGNUM_CGLTFIMPORTER_BUILD_STATIC)
    set(CGLTFIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:CgltfImporter>)
    if(WITH_BASISIMPORTER)
        set(BASISIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:BasisImporter>)
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

corrade_add_resource(CgltfImporterTest_RESOURCES resources.conf)

corrade_add_test(CgltfImporterTest
    CgltfImporterTest.cpp
    ${CgltfImporterTest_RESOURCES}
    LIBRARIES Magnum::Trade
    FILES
        animation-buffer-notfound.gltf
        animation-invalid-types.gltf
        # modified version of animation-patching.* from TinyGltfImporter
        # without size mismatch between input and output samplers
        animation-patching-fixed.bin
        animation-patching-fixed.gltf
        buffer-short-size.glb
        buffer-short-size.gltf
        buffer-short-size-embedded.glb
        buffer-short-size-embedded.gltf
        # identical to external-data.bin from TinyGltfImporter, but needed
        # locally for buffer-short-size*
        external-data.bin
        external-data-order.glb
        external-data-order.gltf
        image-buffer-notfound.gltf
        image-no-data.gltf
        mesh-indices-buffer-notfound.gltf
        mesh-invalid-types.gltf
        skin-buffer-notfound.gltf
        skin-invalid-types.gltf
        uri-invalid.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/empty.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/empty.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/buffer-notfound.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/buffer-notfound.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/buffer-no-uri.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/buffer-no-uri.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/buffer-wrong-size.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/buffer-wrong-size.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation-embedded.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation-embedded.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation-invalid.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation-invalid-node-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation-invalid-input-accessor-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation-invalid-sampler-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation-invalid-output-accessor-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation-missing-target-node.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation-patching.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation-patching.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/animation-splines-sharing.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/camera.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/camera.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/camera-invalid-type.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/encoded-uris.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/escaped-strings.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image-embedded.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image-embedded.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image-buffer.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image-buffer.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image-buffer-embedded.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image-buffer-embedded.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image-notfound.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image-basis.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image-basis.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image-basis-embedded.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/image-basis-embedded.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/light.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/light.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/light-invalid.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/light-invalid-color-size.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/light-missing-type.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/light-missing-spot.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-common.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-clearcoat.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-invalid-alpha-mode.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-invalid-clearcoat-factor-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-invalid-clearcoat-normal-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-invalid-clearcoat-roughness-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-invalid-emissive-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-invalid-normal-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-invalid-occlusion-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-invalid-pbr-base-color-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-invalid-pbr-diffuse-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-invalid-pbr-metallic-roughness-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-invalid-pbr-specular-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-metallicroughness.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-phong-fallback.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-specularglossiness.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-texcoord-flip.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-texcoord-flip.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-texcoord-flip-unnormalized.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/material-unlit.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-colors.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-colors.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-custom-attributes.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-custom-attributes.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-duplicate-attributes.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-embedded.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-embedded.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-index-accessor-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-invalid.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-invalid.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-invalid-accessor-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-invalid-buffer-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-invalid-bufferview-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-multiple-primitives.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-primitives-types.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-primitives-types.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-skin-attributes.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-skin-attributes.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/mesh-unordered-attributes.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/přívodní-šňůra.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/přívodní-šňůra.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/přívodní-šňůra.png
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/required-extensions.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/required-extensions-unsupported.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-cycle.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-cycle-deep.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-nodefault.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-nodefault.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-invalid-camera-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-invalid-child-not-root.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-invalid-child-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-invalid-default-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-invalid-light-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-invalid-material-oob-multi-primitive.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-invalid-material-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-invalid-mesh-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-invalid-multiple-parents.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-invalid-node-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-invalid-skin-oob-multi-primitive.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/scene-invalid-skin-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/skin-embedded.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/skin-embedded.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/skin-invalid.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/skin-invalid-accessor-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/skin-invalid-joint-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/skin-no-joints.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/skin.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/skin.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/skin.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/object-transformation.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/object-transformation.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/object-transformation-patching.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture.basis
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture.png
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture-default-sampler.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture-default-sampler.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture-empty-sampler.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture-empty-sampler.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture-extensions.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture-extensions-invalid.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture-extensions-invalid-basisu-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture-invalid.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture-invalid-image-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture-invalid-sampler-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/texture-missing-source.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/version-legacy.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/version-supported.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/version-unsupported.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/TinyGltfImporter/Test/version-unsupported-min.gltf)
target_include_directories(CgltfImporterTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
if(MAGNUM_CGLTFIMPORTER_BUILD_STATIC)
    target_link_libraries(CgltfImporterTest PRIVATE CgltfImporter)
    if(WITH_BASISIMPORTER)
        target_link_libraries(CgltfImporterTest PRIVATE BasisImporter)
    endif()
    if(WITH_STBIMAGEIMPORTER)
        target_link_libraries(CgltfImporterTest PRIVATE StbImageImporter)
    endif()
else()
    # So the plugins get properly built when building the test
    add_dependencies(CgltfImporterTest CgltfImporter)
    if(WITH_BASISIMPORTER)
        add_dependencies(CgltfImporterTest BasisImporter)
    endif()
    if(WITH_STBIMAGEIMPORTER)
        add_dependencies(CgltfImporterTest StbImageImporter)
    endif()
endif()
set_target_properties(CgltfImporterTest PROPERTIES FOLDER "MagnumPlugins/CgltfImporter/Test")
if(CORRADE_BUILD_STATIC AND NOT MAGNUM_CGLTFIMPORTER_BUILD_STATIC)
    # CMake < 3.4 does this implicitly, but 3.4+ not anymore (see CMP0065).
    # That's generally okay, *except if* the build is static, the executable
    # uses a plugin manager and needs to share globals with the plugins (such
    # as output redirection and so on).
    set_target_properties(CgltfImporterTest PROPERTIES ENABLE_EXPORTS ON)
endif()
