#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2018 Tobias Stein <stein.tobi@t-online.de>
#   Copyright © 2018 Jonathan Hale <squareys@googlemail.com>
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
set(CMAKE_FOLDER "MagnumPlugins/TinyGltfImporter/Test")

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(TINYGLTFIMPORTER_TEST_DIR ".")
    set(GLTFIMPORTER_TEST_DIR ".")
else()
    set(TINYGLTFIMPORTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    set(GLTFIMPORTER_TEST_DIR ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test)
endif()

# CMake before 3.8 has broken $<TARGET_FILE*> expressions for iOS (see
# https://gitlab.kitware.com/cmake/cmake/merge_requests/404) and since Corrade
# doesn't support dynamic plugins on iOS, this sorta works around that. Should
# be revisited when updating Travis to newer Xcode (xcode7.3 has CMake 3.6).
if(NOT MAGNUM_TINYGLTFIMPORTER_BUILD_STATIC)
    set(TINYGLTFIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:TinyGltfImporter>)
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

corrade_add_resource(TinyGltfImporterTest_RESOURCES resources.conf)

corrade_add_test(TinyGltfImporterTest
    TinyGltfImporterTest.cpp
    ${TinyGltfImporterTest_RESOURCES}
    LIBRARIES Magnum::Trade
    FILES
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/animation.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/animation.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/animation.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/animation-embedded.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/animation-embedded.glb
        animation-invalid.gltf
        animation-invalid-track-size-mismatch.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/animation-missing-target-node.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/animation-patching.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/animation-patching.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/animation-splines-sharing.gltf
        buffer-invalid-notfound.gltf
        buffer-invalid-notfound.glb
        buffer-invalid-no-uri.gltf
        buffer-invalid-no-uri.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/buffer-long-size.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/buffer-long-size.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/camera.gltf
        camera-invalid-type.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/empty.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/empty.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/encoded-uris.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/escaped-strings.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/external-data.bin
        external-data.bin
        # external-data.gl* packed via a resource (but *.bin needed also by
        # buffer-wrong-size.gltf from GltfImporter and
        # buffer-invalid-no-uri.gltf from here, so added *twice* separately as
        # well)
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image.basis
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image.png
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image-embedded.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image-embedded.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image-buffer.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image-buffer.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image-buffer-embedded.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image-buffer-embedded.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image-basis.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image-basis.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image-basis-embedded.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/image-basis-embedded.glb
        image-invalid-notfound.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/light.gltf
        light-invalid.gltf
        light-invalid-missing-spot.gltf
        light-invalid-missing-type.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/material-common.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/material-clearcoat.gltf
        material-invalid.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/material-metallicroughness.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/material-phong-fallback.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/material-specularglossiness.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/material-texcoord-flip.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/material-texcoord-flip.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/material-texcoord-flip-unnormalized.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/material-unlit.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-colors.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-colors.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-custom-attributes.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-custom-attributes.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-duplicate-attributes.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-embedded.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-embedded.glb
        mesh-invalid.gltf
        mesh-invalid-accessor-oob.gltf
        mesh-invalid-index-accessor-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-multiple-primitives.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-primitives-types.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-primitives-types.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-skin-attributes.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-skin-attributes.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/mesh-unordered-attributes.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/přívodní-šňůra.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/přívodní-šňůra.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/přívodní-šňůra.png
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/required-extensions.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/required-extensions-unsupported.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/scene.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/scene-default-none.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/scene-default-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/scene-invalid-child-not-root.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/scene-invalid-cycle.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/scene-invalid-cycle-deep.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/scene-invalid-multiple-parents.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/scene-invalid-child-oob.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/scene-invalid-node-oob.gltf
        scene-invalid.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/scene-transformation.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/scene-transformation-patching.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/skin-embedded.glb
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/skin-embedded.gltf
        skin-invalid.gltf
        skin-invalid-no-joints.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/skin.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/skin.bin
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/skin.glb
        texture.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/texture-extensions.gltf
        texture-invalid.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/version-legacy.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/version-supported.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/version-unsupported.gltf
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/GltfImporter/Test/version-unsupported-min.gltf)
target_include_directories(TinyGltfImporterTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
if(MAGNUM_TINYGLTFIMPORTER_BUILD_STATIC)
    target_link_libraries(TinyGltfImporterTest PRIVATE TinyGltfImporter)
    if(WITH_BASISIMPORTER)
        target_link_libraries(TinyGltfImporterTest PRIVATE BasisImporter)
    endif()
    if(WITH_STBIMAGEIMPORTER)
        target_link_libraries(TinyGltfImporterTest PRIVATE StbImageImporter)
    endif()
else()
    # So the plugins get properly built when building the test
    add_dependencies(TinyGltfImporterTest TinyGltfImporter)
    if(WITH_BASISIMPORTER)
        add_dependencies(TinyGltfImporterTest BasisImporter)
    endif()
    if(WITH_STBIMAGEIMPORTER)
        add_dependencies(TinyGltfImporterTest StbImageImporter)
    endif()
endif()
if(CORRADE_BUILD_STATIC AND NOT MAGNUM_TINYGLTFIMPORTER_BUILD_STATIC)
    # CMake < 3.4 does this implicitly, but 3.4+ not anymore (see CMP0065).
    # That's generally okay, *except if* the build is static, the executable
    # uses a plugin manager and needs to share globals with the plugins (such
    # as output redirection and so on).
    set_target_properties(TinyGltfImporterTest PROPERTIES ENABLE_EXPORTS ON)
endif()
