#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015
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

find_package(PNG REQUIRED)

if(BUILD_STATIC)
    set(MAGNUM_PNGIMAGECONVERTER_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

set(PngImageConverter_SRCS
    PngImageConverter.cpp)

set(PngImageConverter_HEADERS
    PngImageConverter.h)

# Objects shared between plugin and test library
add_library(PngImageConverterObjects OBJECT
    ${PngImageConverter_SRCS}
    ${PngImageConverter_HEADERS})
target_include_directories(PngImageConverterObjects
    PUBLIC
        $<TARGET_PROPERTY:Magnum::Magnum,INTERFACE_INCLUDE_DIRECTORIES>
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src
    PRIVATE
        ${PNG_INCLUDE_DIRS})
target_compile_definitions(PngImageConverterObjects PRIVATE "PngImageConverterObjects_EXPORTS")
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(PngImageConverterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
set_target_properties(PngImageConverterObjects PROPERTIES FOLDER "MagnumPlugins/PngImageConverter")

# PngImageConverter plugin
add_plugin(PngImageConverter
    "${MAGNUM_PLUGINS_IMAGECONVERTER_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMAGECONVERTER_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_IMAGECONVERTER_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMAGECONVERTER_RELEASE_LIBRARY_INSTALL_DIR}"
    PngImageConverter.conf
    $<TARGET_OBJECTS:PngImageConverterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(PngImageConverter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(PngImageConverter PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_link_libraries(PngImageConverter Magnum::Magnum ${PNG_LIBRARIES})

install(FILES ${PngImageConverter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/PngImageConverter)

if(BUILD_TESTS)
    add_library(MagnumPngImageConverterTestLib STATIC
        $<TARGET_OBJECTS:PngImageConverterObjects>
        ${PROJECT_SOURCE_DIR}/src/dummy.cpp) # XCode workaround, see file comment for details
    target_include_directories(MagnumPngImageConverterTestLib PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src)
    set_target_properties(MagnumPngImageConverterTestLib PROPERTIES FOLDER "MagnumPlugins/PngImageConverter")
    target_link_libraries(MagnumPngImageConverterTestLib Magnum::Magnum ${PNG_LIBRARIES})
    add_subdirectory(Test)
endif()

# MagnumPlugins PngImageConverter target alias for superprojects
add_library(MagnumPlugins::PngImageConverter ALIAS PngImageConverter)
