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

# On macOS avoid finding Freetype which is bundled with Mono
set(PREV_CMAKE_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK})
set(CMAKE_FIND_FRAMEWORK LAST)
find_package(Freetype REQUIRED)
set(CMAKE_FIND_FRAMEWORK ${PREV_CMAKE_FIND_FRAMEWORK})

if(BUILD_STATIC)
    set(MAGNUM_FREETYPEFONT_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

set(FreeTypeFont_SRCS
    FreeTypeFont.cpp)

set(FreeTypeFont_HEADERS
    FreeTypeFont.h)

# Objects shared between plugin and test library
add_library(FreeTypeFontObjects OBJECT
    ${FreeTypeFont_SRCS}
    ${FreeTypeFont_HEADERS})
target_include_directories(FreeTypeFontObjects PUBLIC
    $<TARGET_PROPERTY:Magnum::Text,INTERFACE_INCLUDE_DIRECTORIES>
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
# Avoid warning spam with old-style casts
target_include_directories(FreeTypeFontObjects SYSTEM PRIVATE ${FREETYPE_INCLUDE_DIRS})
target_compile_definitions(FreeTypeFontObjects PRIVATE "FreeTypeFontObjects_EXPORTS")
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(FreeTypeFontObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
set_target_properties(FreeTypeFontObjects PROPERTIES FOLDER "MagnumPlugins/FreeTypeFont")

# FreeTypeFont plugin
add_plugin(FreeTypeFont
    "${MAGNUM_PLUGINS_FONT_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_FONT_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_FONT_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_FONT_RELEASE_LIBRARY_INSTALL_DIR}"
    FreeTypeFont.conf
    $<TARGET_OBJECTS:FreeTypeFontObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(FreeTypeFont PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(FreeTypeFont PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_link_libraries(FreeTypeFont
    Magnum::Magnum
    Magnum::Text
    ${FREETYPE_LIBRARIES}
    ${FREETYPE_DEPENDENCY_LIBRARIES})

install(FILES ${FreeTypeFont_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/FreeTypeFont)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/configure.h DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/FreeTypeFont)

# The lib is needed by non-GL tests
if(BUILD_TESTS)
    add_library(MagnumFreeTypeFontTestLib STATIC
        $<TARGET_OBJECTS:FreeTypeFontObjects>
        ${PROJECT_SOURCE_DIR}/src/dummy.cpp) # XCode workaround, see file comment for details
    target_include_directories(MagnumFreeTypeFontTestLib PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src)
    set_target_properties(MagnumFreeTypeFontTestLib PROPERTIES FOLDER "MagnumPlugins/FreeTypeFont")
    target_link_libraries(MagnumFreeTypeFontTestLib
        Magnum::Magnum
        Magnum::Text
        ${FREETYPE_LIBRARIES}
        ${FREETYPE_DEPENDENCY_LIBRARIES})
endif()

if(BUILD_GL_TESTS)
    add_subdirectory(Test)
endif()

# MagnumPlugins FreeTypeFont target alias for superprojects
add_library(MagnumPlugins::FreeTypeFont ALIAS FreeTypeFont)
