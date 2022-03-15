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
set(CMAKE_FOLDER "MagnumPlugins/KtxImporter/Test")

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(BASISIMPORTER_TEST_DIR ".")
    set(KTXIMPORTER_TEST_DIR ".")
else()
    set(BASISIMPORTER_TEST_DIR ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/BasisImporter/Test)
    set(KTXIMPORTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# CMake before 3.8 has broken $<TARGET_FILE*> expressions for iOS (see
# https://gitlab.kitware.com/cmake/cmake/merge_requests/404) and since Corrade
# doesn't support dynamic plugins on iOS, this sorta works around that. Should
# be revisited when updating Travis to newer Xcode (xcode7.3 has CMake 3.6).
if(NOT MAGNUM_KTXIMPORTER_BUILD_STATIC)
    set(KTXIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:KtxImporter>)
    if(WITH_BASISIMPORTER)
        set(BASISIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:BasisImporter>)
    endif()
endif()

# First replace ${} variables, then $<> generator expressions
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/configure.h
    INPUT ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)

corrade_add_test(KtxImporterTest KtxImporterTest.cpp
    LIBRARIES Magnum::Trade
    FILES
        1d-compressed-bc1.bin
        1d-compressed-bc1.ktx2
        1d-compressed-etc2.bin
        1d-compressed-etc2.ktx2
        1d-compressed-mipmaps-mip0.bin
        1d-compressed-mipmaps-mip1.bin
        1d-compressed-mipmaps-mip2.bin
        1d-compressed-mipmaps.ktx2
        1d-layers.ktx2
        1d-mipmaps.ktx2
        1d.ktx2
        2d-compressed-astc.bin
        2d-compressed-astc.ktx2
        2d-compressed-bc1.bin
        2d-compressed-bc1.ktx2
        2d-compressed-bc3.bin
        2d-compressed-bc3.ktx2
        2d-compressed-etc2.bin
        2d-compressed-etc2.ktx2
        2d-compressed-layers.bin
        2d-compressed-layers.ktx2
        2d-compressed-mipmaps-mip0.bin
        2d-compressed-mipmaps-mip1.bin
        2d-compressed-mipmaps-mip2.bin
        2d-compressed-mipmaps-mip3.bin
        2d-compressed-mipmaps.ktx2
        2d-compressed-pvrtc.bin
        2d-compressed-pvrtc.ktx2
        2d-d16.ktx2
        2d-d24s8.ktx2
        2d-d32fs8.ktx2
        2d-layers.ktx2
        2d-mipmaps-and-layers.ktx2
        2d-mipmaps-incomplete.ktx2
        2d-mipmaps.ktx2
        2d-rgb.ktx2
        2d-rgb32.ktx2
        2d-rgba.ktx2
        2d-rgbf32.ktx2
        2d-s8.ktx2
        3d-compressed.bin
        3d-compressed.ktx2
        3d-compressed-mipmaps-mip0.bin
        3d-compressed-mipmaps-mip1.bin
        3d-compressed-mipmaps-mip2.bin
        3d-compressed-mipmaps-mip3.bin
        3d-compressed-mipmaps.ktx2
        3d-layers.ktx2
        3d-mipmaps.ktx2
        3d.ktx2
        bgr-swizzle-bgr-16bit.ktx2
        bgr-swizzle-bgr.ktx2
        bgr.ktx2
        bgra-swizzle-bgra.ktx2
        bgra.ktx2
        cubemap-layers.ktx2
        cubemap-mipmaps.ktx2
        cubemap.ktx2
        swizzle-bgr.ktx2
        swizzle-bgra.ktx2
        swizzle-identity.ktx2
        swizzle-unsupported.ktx2
        version1.ktx
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/BasisImporter/Test/rgba.ktx2
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/BasisImporter/Test/rgba-array.ktx2
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/BasisImporter/Test/rgba-uastc.ktx2
        ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/BasisImporter/Test/rgba-video.ktx2)
target_include_directories(KtxImporterTest PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>
    ${PROJECT_SOURCE_DIR}/src)
if(MAGNUM_KTXIMPORTER_BUILD_STATIC)
    target_link_libraries(KtxImporterTest PRIVATE KtxImporter)
    if(WITH_BASISIMPORTER)
        target_link_libraries(KtxImporterTest PRIVATE BasisImporter)
    endif()
else()
    # So the plugins get properly built when building the test
    add_dependencies(KtxImporterTest KtxImporter)
    if(WITH_BASISIMPORTER)
        add_dependencies(KtxImporterTest BasisImporter)
    endif()
endif()
if(CORRADE_BUILD_STATIC AND NOT MAGNUM_KTXIMPORTER_BUILD_STATIC)
    # CMake < 3.4 does this implicitly, but 3.4+ not anymore (see CMP0065).
    # That's generally okay, *except if* the build is static, the executable
    # uses a plugin manager and needs to share globals with the plugins (such
    # as output redirection and so on).
    set_target_properties(KtxImporterTest PROPERTIES ENABLE_EXPORTS ON)
endif()
