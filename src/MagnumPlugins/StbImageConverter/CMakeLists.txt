#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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
    set(MAGNUM_STBIMAGECONVERTER_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

set(StbImageConverter_SRCS
    StbImageConverter.cpp)

set(StbImageConverter_HEADERS
    StbImageConverter.h)

# Objects shared between plugin and test library
add_library(StbImageConverterObjects OBJECT
    ${StbImageConverter_SRCS}
    ${StbImageConverter_HEADERS})
target_include_directories(StbImageConverterObjects PUBLIC
    $<TARGET_PROPERTY:Magnum::Magnum,INTERFACE_INCLUDE_DIRECTORIES>
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_compile_definitions(StbImageConverterObjects PRIVATE "StbImageConverterObjects_EXPORTS")
# Include the stb_*.h files as a system directory to supress warnings
target_include_directories(StbImageConverterObjects SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/src/external/stb)
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(StbImageConverterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
set_target_properties(StbImageConverterObjects PROPERTIES FOLDER "MagnumPlugins/StbImageConverter")

# StbImageConverter plugin
add_plugin(StbImageConverter
    "${MAGNUM_PLUGINS_IMAGECONVERTER_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMAGECONVERTER_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_IMAGECONVERTER_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMAGECONVERTER_RELEASE_LIBRARY_INSTALL_DIR}"
    StbImageConverter.conf
    $<TARGET_OBJECTS:StbImageConverterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(StbImageConverter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(StbImageConverter PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_link_libraries(StbImageConverter Magnum::Magnum)

install(FILES ${StbImageConverter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/StbImageConverter)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/configure.h DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/StbImageConverter)

if(BUILD_TESTS)
    add_library(MagnumStbImageConverterTestLib STATIC
        $<TARGET_OBJECTS:StbImageConverterObjects>
        ${PROJECT_SOURCE_DIR}/src/dummy.cpp) # XCode workaround, see file comment for details
    target_include_directories(MagnumStbImageConverterTestLib PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src)
    set_target_properties(MagnumStbImageConverterTestLib PROPERTIES FOLDER "MagnumPlugins/StbImageConverter")
    target_link_libraries(MagnumStbImageConverterTestLib Magnum::Magnum)
    add_subdirectory(Test)
endif()

# MagnumPlugins StbImageConverter target alias for superprojects
add_library(MagnumPlugins::StbImageConverter ALIAS StbImageConverter)
