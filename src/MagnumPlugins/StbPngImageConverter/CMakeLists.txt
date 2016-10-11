#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016
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

if(BUILD_STATIC)
    set(MAGNUM_STBPNGIMAGECONVERTER_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

set(StbPngImageConverter_SRCS
    StbPngImageConverter.cpp)

set(StbPngImageConverter_HEADERS
    StbPngImageConverter.h)

# Objects shared between plugin and test library
add_library(StbPngImageConverterObjects OBJECT
    ${StbPngImageConverter_SRCS}
    ${StbPngImageConverter_HEADERS})
target_include_directories(StbPngImageConverterObjects PUBLIC
    $<TARGET_PROPERTY:Magnum::Magnum,INTERFACE_INCLUDE_DIRECTORIES>
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_compile_definitions(StbPngImageConverterObjects PRIVATE "StbPngImageConverterObjects_EXPORTS")
# Include the stb_*.h files as a system directory to supress warnings
target_include_directories(StbPngImageConverterObjects SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/src/external/stb)
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(StbPngImageConverterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# StbPngImageConverter plugin
add_plugin(StbPngImageConverter
    "${MAGNUM_PLUGINS_IMAGECONVERTER_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMAGECONVERTER_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_IMAGECONVERTER_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMAGECONVERTER_RELEASE_LIBRARY_INSTALL_DIR}"
    StbPngImageConverter.conf
    $<TARGET_OBJECTS:StbPngImageConverterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(StbPngImageConverter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(StbPngImageConverter PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_link_libraries(StbPngImageConverter Magnum::Magnum)

install(FILES ${StbPngImageConverter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/StbPngImageConverter)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/configure.h DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/StbPngImageConverter)

if(BUILD_TESTS)
    add_library(MagnumStbPngImageConverterTestLib STATIC
        $<TARGET_OBJECTS:StbPngImageConverterObjects>
        ${PROJECT_SOURCE_DIR}/src/dummy.cpp) # XCode workaround, see file comment for details
    target_include_directories(MagnumStbPngImageConverterTestLib PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src)
    target_link_libraries(MagnumStbPngImageConverterTestLib Magnum::Magnum)
    add_subdirectory(Test)
endif()

# MagnumPlugins StbPngImageConverter target alias for superprojects
add_library(MagnumPlugins::StbPngImageConverter ALIAS StbPngImageConverter)
