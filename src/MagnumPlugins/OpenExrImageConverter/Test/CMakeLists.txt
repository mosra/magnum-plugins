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

find_package(Magnum REQUIRED DebugTools)

if(CORRADE_TARGET_EMSCRIPTEN OR CORRADE_TARGET_ANDROID)
    set(OPENEXRIMPORTER_TEST_DIR ".")
    set(OPENEXRIMAGECONVERTER_TEST_DIR ".")
else()
    set(OPENEXRIMPORTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../OpenExrImporter/Test/)
    set(OPENEXRIMAGECONVERTER_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# CMake before 3.8 has broken $<TARGET_FILE*> expressions for iOS (see
# https://gitlab.kitware.com/cmake/cmake/merge_requests/404) and since Corrade
# doesn't support dynamic plugins on iOS, this sorta works around that. Should
# be revisited when updating Travis to newer Xcode (xcode7.3 has CMake 3.6).
if(NOT MAGNUM_OPENEXRIMAGECONVERTER_BUILD_STATIC)
    set(OPENEXRIMAGECONVERTER_PLUGIN_FILENAME $<TARGET_FILE:OpenExrImageConverter>)
    if(WITH_OPENEXRIMPORTER)
        set(OPENEXRIMPORTER_PLUGIN_FILENAME $<TARGET_FILE:OpenExrImporter>)
    endif()
endif()

# First replace ${} variables, then $<> generator expressions
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/configure.h
    INPUT ${CMAKE_CURRENT_BINARY_DIR}/configure.h.in)

corrade_add_test(OpenExrImageConverterTest OpenExrImageConverterTest.cpp
    LIBRARIES Magnum::Trade Magnum::DebugTools
    FILES
        ../../OpenExrImporter/Test/depth32f.exr
        ../../OpenExrImporter/Test/depth32f-custom-channels.exr
        ../../OpenExrImporter/Test/rgb16f.exr
        ../../OpenExrImporter/Test/rgb16f-custom-windows.exr
        ../../OpenExrImporter/Test/rgba32f.exr
        ../../OpenExrImporter/Test/rgba32f-custom-channels.exr
        ../../OpenExrImporter/Test/rg32ui.exr
        rb32f-custom-channels.exr)
target_include_directories(OpenExrImageConverterTest PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>
    $<TARGET_PROPERTY:OpenEXR::IlmImf,INTERFACE_INCLUDE_DIRECTORIES>)
if(MAGNUM_OPENEXRIMAGECONVERTER_BUILD_STATIC)
    target_link_libraries(OpenExrImageConverterTest PRIVATE OpenExrImageConverter)
    if(WITH_JPEGIMPORTER)
        target_link_libraries(OpenExrImageConverterTest PRIVATE OpenExrImporter)
    endif()
else()
    # So the plugins get properly built when building the test
    add_dependencies(OpenExrImageConverterTest OpenExrImageConverter)
    if(WITH_OPENEXRIMPORTER)
        add_dependencies(OpenExrImageConverterTest OpenExrImporter)
    endif()
endif()
set_target_properties(OpenExrImageConverterTest PROPERTIES FOLDER "MagnumPlugins/OpenExrImageConverter/Test")
if(CORRADE_BUILD_STATIC AND NOT MAGNUM_OPENEXRIMAGECONVERTER_BUILD_STATIC)
    # CMake < 3.4 does this implicitly, but 3.4+ not anymore (see CMP0065).
    # That's generally okay, *except if* the build is static, the executable
    # uses a plugin manager and needs to share globals with the plugins (such
    # as output redirection and so on).
    set_target_properties(OpenExrImageConverterTest PROPERTIES ENABLE_EXPORTS ON)
endif()
