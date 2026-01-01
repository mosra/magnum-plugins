#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022, 2023, 2024, 2025, 2026
#             Vladimír Vondruš <mosra@centrum.cz>
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
set(CMAKE_FOLDER "MagnumPlugins/AvifImporter/Test")

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(AVIFIMPORTER_TEST_DIR ".")
else()
    set(AVIFIMPORTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

find_package(Magnum REQUIRED DebugTools)

if(NOT MAGNUM_AVIFIMPORTER_BUILD_STATIC)
    set(AVIFIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:AvifImporter>)
endif()

# First replace ${} variables, then $<> generator expressions
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/configure.h
    INPUT ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)

corrade_add_test(AvifImporterTest AvifImporterTest.cpp
    LIBRARIES Magnum::Trade Magnum::DebugTools
    FILES
        # avifenc ../../PngImporter/Test/gray.png -l -d 8 gray.avif
        gray.avif
        # avifenc ../../PngImporter/Test/gray16.png -l -d 12 gray12.avif
        gray12.avif
        # avifenc ../../PngImporter/Test/ga.png -l -d 8 ga.avif
        ga.avif
        # avifenc ../../PngImporter/Test/rgb.png -l -d 8 rgb.avif
        rgb.avif
        # avifenc ../../PngImporter/Test/rgb16.png -l -d 10 rgb10.avif
        rgb10.avif
        # avifenc ../../PngImporter/Test/rgba.png -l -d 8 rgba.avif
        rgb.avif)

target_include_directories(AvifImporterTest PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
# For version checking in the test. Use SYSTEM to avoid warning spam in C
# headers.
target_include_directories(AvifImporterTest SYSTEM PRIVATE
    $<TARGET_PROPERTY:avif,INTERFACE_INCLUDE_DIRECTORIES>)
if(MAGNUM_AVIFIMPORTER_BUILD_STATIC)
    target_link_libraries(AvifImporterTest PRIVATE AvifImporter)
else()
    # So the plugins get properly built when building the test
    add_dependencies(AvifImporterTest AvifImporter)
endif()
if(CORRADE_BUILD_STATIC AND NOT MAGNUM_AVIFIMPORTER_BUILD_STATIC)
    # CMake < 3.4 does this implicitly, but 3.4+ not anymore (see CMP0065).
    # That's generally okay, *except if* the build is static, the executable
    # uses a plugin manager and needs to share globals with the plugins (such
    # as output redirection and so on).
    set_target_properties(AvifImporterTest PROPERTIES ENABLE_EXPORTS ON)
endif()
