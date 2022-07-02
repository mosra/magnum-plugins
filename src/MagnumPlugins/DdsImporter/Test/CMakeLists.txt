#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2015 Jonathan Hale <squareys@googlemail.com>
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
set(CMAKE_FOLDER "MagnumPlugins/DdsImporter/Test")

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(DDSIMPORTER_TEST_DIR ".")
else()
    set(DDSIMPORTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if(NOT MAGNUM_DDSIMPORTER_BUILD_STATIC)
    set(DDSIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:DdsImporter>)
endif()

# First replace ${} variables, then $<> generator expressions
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/configure.h
    INPUT ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)

corrade_add_test(DdsImporterTest
    DdsImporterTest.cpp
    ${DXT10_TEST_FILES_RESOURCE}
    LIBRARIES Magnum::Trade
    FILES
        bc4snorm.dds
        bc4unorm.dds
        bc5snorm.dds
        bc5unorm.dds
        cube-flag-set-for-3d.dds
        depth-set-for-non-3d.dds
        dxt1.dds
        dxt1-cube-mips.dds
        dxt10-ayuv.dds
        dxt10-array-size-set-for-3d.dds
        dxt10-astc8x5unorm.dds
        dxt10-bc7.dds
        dxt10-bc7-cube-mips.dds
        dxt10-cube-flag-set-for-1d.dds
        dxt10-cube-flag-set-for-3d.dds
        dxt10-depth-set-for-non-3d.dds
        dxt10-depth24unorm-stencil8ui.dds
        dxt10-depth32f-stencil8ui.dds
        dxt10-format136.dds
        dxt10-format189.dds
        dxt10-height-set-for-1d.dds
        dxt10-invalid-resource-dimension.dds
        dxt10-r8unorm-3d-zero-fields-zero-depth-zero-mips.dds
        dxt10-r16f-cube.dds
        dxt10-r8snorm-cube-array.dds
        dxt10-rg16f-1d-array-mips.dds
        dxt10-rg16f-1d.dds
        dxt10-rgba16snorm.dds
        dxt10-rgba32ui.dds
        dxt10-rgba8srgb.dds
        dxt10-rgba8unorm.dds
        dxt10-bgra8unorm-3d.dds
        dxt10-rgba8unorm-3d.dds
        dxt10-rgba8unorm-array.dds
        dxt10-rgb32i.dds
        dxt10-rg32f.dds
        dxt10-rg8unorm.dds
        dxt10-r32i-mips.dds
        dxt3.dds
        dxt3-incomplete-blocks.dds
        dxt4.dds
        dxt5.dds
        bgr8unorm.dds
        rgb8unorm.dds
        rgb8unorm-yup.dds
        bgrx8unorm.dds
        rgbx8unorm.dds
        bgr8unorm-mips.dds
        bgra8unorm-3d.dds
        r8unorm.dds
        r8unorm-3d-zero-fields-zero-depth-zero-mips.dds
        rgba8unorm-3d.dds
        rgba8unorm-3d-yup-zbackward.dds
        rgba8unorm-cube.dds
        rgba8unorm-cube-mips.dds
        rgba8unorm-cube-incomplete.dds
        unknown-format.dds
        unknown-format-rgb.dds
        unknown-format-rgba.dds
        wrong-signature.dds)
target_include_directories(DdsImporterTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
if(MAGNUM_DDSIMPORTER_BUILD_STATIC)
    target_link_libraries(DdsImporterTest PRIVATE DdsImporter)
else()
    # So the plugins get properly built when building the test
    add_dependencies(DdsImporterTest DdsImporter)
endif()
if(CORRADE_BUILD_STATIC AND NOT MAGNUM_DDSIMPORTER_BUILD_STATIC)
    # CMake < 3.4 does this implicitly, but 3.4+ not anymore (see CMP0065).
    # That's generally okay, *except if* the build is static, the executable
    # uses a plugin manager and needs to share globals with the plugins (such
    # as output redirection and so on).
    set_target_properties(DdsImporterTest PROPERTIES ENABLE_EXPORTS ON)
endif()
