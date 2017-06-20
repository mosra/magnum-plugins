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

find_package(Magnum REQUIRED Text)

if(BUILD_STATIC)
    set(MAGNUM_STBTRUETYPEFONT_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

set(StbTrueTypeFont_SRCS
    StbTrueTypeFont.cpp)

set(StbTrueTypeFont_HEADERS
    StbTrueTypeFont.h)

# Objects shared between plugin and test library
add_library(StbTrueTypeFontObjects OBJECT
    ${StbTrueTypeFont_SRCS}
    ${StbTrueTypeFont_HEADERS})
target_include_directories(StbTrueTypeFontObjects PUBLIC
    $<TARGET_PROPERTY:Magnum::Text,INTERFACE_INCLUDE_DIRECTORIES>
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_compile_definitions(StbTrueTypeFontObjects PRIVATE "StbTrueTypeFontObjects_EXPORTS")
# Include the stb_*.h files as a system directory to supress warnings
target_include_directories(StbTrueTypeFontObjects SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/src/external/stb)
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(StbTrueTypeFontObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
set_target_properties(StbTrueTypeFontObjects PROPERTIES FOLDER "MagnumPlugins/StbTrueTypeFont")

# StbTrueTypeFont plugin
add_plugin(StbTrueTypeFont
    "${MAGNUM_PLUGINS_FONT_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_FONT_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_FONT_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_FONT_RELEASE_LIBRARY_INSTALL_DIR}"
    StbTrueTypeFont.conf
    $<TARGET_OBJECTS:StbTrueTypeFontObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(StbTrueTypeFont PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(StbTrueTypeFont PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_link_libraries(StbTrueTypeFont
    Magnum::Magnum
    Magnum::Text
    Magnum::TextureTools
    ${FREETYPE_LIBRARIES})

install(FILES ${StbTrueTypeFont_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/StbTrueTypeFont)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/configure.h DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/StbTrueTypeFont)

# The lib is needed by non-GL tests
if(BUILD_TESTS)
    add_library(MagnumStbTrueTypeFontTestLib STATIC
        $<TARGET_OBJECTS:StbTrueTypeFontObjects>
        ${PROJECT_SOURCE_DIR}/src/dummy.cpp) # XCode workaround, see file comment for details
    target_include_directories(MagnumStbTrueTypeFontTestLib PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src)
    set_target_properties(MagnumStbTrueTypeFontTestLib PROPERTIES FOLDER "MagnumPlugins/StbTrueTypeFont")
    target_link_libraries(MagnumStbTrueTypeFontTestLib
        Magnum::Magnum
        Magnum::Text
        Magnum::TextureTools
        ${FREETYPE_LIBRARIES})
endif()

if(BUILD_GL_TESTS)
    add_subdirectory(Test)
endif()

# MagnumPlugins StbTrueTypeFont target alias for superprojects
add_library(MagnumPlugins::StbTrueTypeFont ALIAS StbTrueTypeFont)
