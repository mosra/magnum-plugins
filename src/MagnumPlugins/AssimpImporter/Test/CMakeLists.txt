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

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(ASSIMPIMPORTER_TEST_DIR ".")
else()
    set(ASSIMPIMPORTER_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# CMake before 3.8 has broken $<TARGET_FILE*> expressions for iOS (see
# https://gitlab.kitware.com/cmake/cmake/merge_requests/404) and since Corrade
# doesn't support dynamic plugins on iOS, this sorta works around that. Should
# be revisited when updating Travis to newer Xcode (xcode7.3 has CMake 3.6).
if(NOT MAGNUM_ASSIMPIMPORTER_BUILD_STATIC)
    set(ASSIMPIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:AssimpImporter>)
    if(WITH_DDSIMPORTER)
        set(DDSIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:DdsImporter>)
    endif()
    if(WITH_STBIMAGEIMPORTER)
        set(STBIMAGEIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:StbImageImporter>)
    endif()
endif()

# Assimp 5. Of all the things that could break, this version reports itself as
# 4.1. Since some of the insane awful bugs got fixed in version 5, the test has
# to check against the version in order to adjust expectations. The only way I
# could make this work is checking for the getEpsilon() function added in
# https://github.com/assimp/assimp/commit/8b95479bb00b4bf8fb875f2c5b0605ddfd203b7f
# Related bug: https://github.com/assimp/assimp/issues/2693
try_compile(ASSIMP_IS_VERSION_5 ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/CheckAssimpVersion5.cpp LINK_LIBRARIES Assimp::Assimp)

# First replace ${} variables, then $<> generator expressions
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/configure.h
    INPUT ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)

corrade_add_test(AssimpImporterTest AssimpImporterTest.cpp
    LIBRARIES Magnum::Trade
    FILES
        empty.dae
        empty.gltf
        camera.dae
        diffuse_texture.png
        embedded-texture.blend
        image-filename-trailing-space.mtl
        image-filename-trailing-space.obj
        image-mips.mtl
        image-mips.obj
        light.dae
        light-area.fbx # Converted from light-area.blend
        material-color.dae
        material-texture.dae
        material-color-texture.mtl
        material-color-texture.obj
        material-coordinate-sets.dae
        mesh.dae
        mips.dds
        multiple-textures.mtl r.png g.png b.png y.png
        points.obj
        scene.dae
        texture-ambient.obj
        texture-ambient.mtl
        quad.stl
        y-up.dae
        z-up.dae)
target_link_libraries(AssimpImporterTest PRIVATE Assimp::Assimp)
target_include_directories(AssimpImporterTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
if(MAGNUM_ASSIMPIMPORTER_BUILD_STATIC)
    target_link_libraries(AssimpImporterTest PRIVATE
        AssimpImporter Magnum::AnyImageImporter)
    if(WITH_DDSIMPORTER)
        target_link_libraries(AssimpImporterTest PRIVATE DdsImporter)
    endif()
    if(WITH_STBIMAGEIMPORTER)
        target_link_libraries(AssimpImporterTest PRIVATE StbImageImporter)
    endif()
else()
    # So the plugins get properly built when building the test
    add_dependencies(AssimpImporterTest AssimpImporter)
    if(WITH_DDSIMPORTER)
        add_dependencies(AssimpImporterTest DdsImporter)
    endif()
    if(WITH_STBIMAGEIMPORTER)
        add_dependencies(AssimpImporterTest StbImageImporter)
    endif()
endif()
set_target_properties(AssimpImporterTest PROPERTIES FOLDER "MagnumPlugins/AssimpImporter/Test")
if(CORRADE_BUILD_STATIC AND NOT MAGNUM_ASSIMPIMPORTER_BUILD_STATIC)
    # CMake < 3.4 does this implicitly, but 3.4+ not anymore (see CMP0065).
    # That's generally okay, *except if* the build is static, the executable
    # uses a plugin manager and needs to share globals with the plugins (such
    # as output redirection and so on).
    set_target_properties(AssimpImporterTest PROPERTIES ENABLE_EXPORTS ON)
endif()
