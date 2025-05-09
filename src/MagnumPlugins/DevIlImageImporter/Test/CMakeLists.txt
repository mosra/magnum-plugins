#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022, 2023, 2024, 2025
#             Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2016 Alice Margatroid <loveoverwhelming@gmail.com>
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
set(CMAKE_FOLDER "MagnumPlugins/DevIlImageImporter/Test")

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(ICOIMPORTER_TEST_DIR ".")
    set(PNGIMPORTER_TEST_DIR ".")
    set(JPEGIMPORTER_TEST_DIR ".")
    set(STBIMAGEIMPORTER_TEST_DIR ".")
else()
    set(ICOIMPORTER_TEST_DIR ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/IcoImporter/Test)
    set(PNGIMPORTER_TEST_DIR ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/PngImporter/Test)
    set(JPEGIMPORTER_TEST_DIR ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/JpegImporter/Test)
    set(STBIMAGEIMPORTER_TEST_DIR ${PROJECT_SOURCE_DIR}/src/MagnumPlugins/StbImageImporter/Test)
endif()

if(NOT MAGNUM_DEVILIMAGEIMPORTER_BUILD_STATIC)
    set(DEVILIMAGEIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:DevIlImageImporter>)
endif()

# First replace ${} variables, then $<> generator expressions
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/configure.h
    INPUT ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)

corrade_add_test(DevIlImageImporterTest DevIlImageImporterTest.cpp
    LIBRARIES Magnum::Trade
    FILES
        ../../IcoImporter/Test/bmp+png.ico
        ../../IcoImporter/Test/pngs.ico
        ../../PngImporter/Test/gray.png
        ../../PngImporter/Test/rgb.png
        ../../PngImporter/Test/rgba.png
        ../../JpegImporter/Test/gray.jpg
        ../../JpegImporter/Test/rgb.jpg
        ../../StbImageImporter/Test/dispose_bgnd.gif)
target_include_directories(DevIlImageImporterTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
if(MAGNUM_DEVILIMAGEIMPORTER_BUILD_STATIC)
    target_link_libraries(DevIlImageImporterTest PRIVATE DevIlImageImporter)
else()
    # So the plugins get properly built when building the test
    add_dependencies(DevIlImageImporterTest DevIlImageImporter)
endif()
if(CORRADE_BUILD_STATIC AND NOT MAGNUM_DEVILIMAGEIMPORTER_BUILD_STATIC)
    # CMake < 3.4 does this implicitly, but 3.4+ not anymore (see CMP0065).
    # That's generally okay, *except if* the build is static, the executable
    # uses a plugin manager and needs to share globals with the plugins (such
    # as output redirection and so on).
    set_target_properties(DevIlImageImporterTest PROPERTIES ENABLE_EXPORTS ON)
endif()
