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
find_package(Freetype REQUIRED)
find_package(HarfBuzz REQUIRED)

if(BUILD_STATIC)
    set(MAGNUM_HARFBUZZFONT_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

set(HarfBuzzFont_SRCS
    HarfBuzzFont.cpp)

set(HarfBuzzFont_HEADERS
    HarfBuzzFont.h)

# Objects shared between plugin and test library
add_library(HarfBuzzFontObjects OBJECT
    ${HarfBuzzFont_SRCS}
    ${HarfBuzzFont_HEADERS})
target_include_directories(HarfBuzzFontObjects PUBLIC
    $<TARGET_PROPERTY:Magnum::Text,INTERFACE_INCLUDE_DIRECTORIES>
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
# Avoid warning spam with old-style casts
target_include_directories(HarfBuzzFontObjects SYSTEM PRIVATE
    ${FREETYPE_INCLUDE_DIRS}
    $<TARGET_PROPERTY:HarfBuzz::HarfBuzz,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:FreeTypeFont,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(HarfBuzzFontObjects PRIVATE "HarfBuzzFontObjects_EXPORTS")
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(HarfBuzzFontObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
set_target_properties(HarfBuzzFontObjects PROPERTIES FOLDER "MagnumPlugins/HarfBuzzFont")

# HarfBuzzFont plugin
add_plugin(HarfBuzzFont
    "${MAGNUM_PLUGINS_FONT_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_FONT_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_FONT_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_FONT_RELEASE_LIBRARY_INSTALL_DIR}"
    HarfBuzzFont.conf
    $<TARGET_OBJECTS:HarfBuzzFontObjects>
    pluginRegistration.cpp)
target_include_directories(HarfBuzzFont PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src
    $<TARGET_PROPERTY:FreeTypeFont,INTERFACE_INCLUDE_DIRECTORIES>)
if(BUILD_STATIC_PIC)
    set_target_properties(HarfBuzzFont PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_link_libraries(HarfBuzzFont
    Magnum::Magnum
    Magnum::Text
    ${FREETYPE_LIBRARIES}
    HarfBuzz::HarfBuzz)
if(CORRADE_TARGET_WINDOWS)
    target_link_libraries(HarfBuzzFont FreeTypeFont)
endif()

install(FILES ${HarfBuzzFont_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/HarfBuzzFont)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/configure.h DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/HarfBuzzFont)

if(BUILD_GL_TESTS)
    add_library(MagnumHarfBuzzFontTestLib STATIC
        $<TARGET_OBJECTS:HarfBuzzFontObjects>
        ${PROJECT_SOURCE_DIR}/src/dummy.cpp) # XCode workaround, see file comment for details
    target_include_directories(MagnumHarfBuzzFontTestLib PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src)
    set_target_properties(MagnumHarfBuzzFontTestLib PROPERTIES FOLDER "MagnumPlugins/HarfBuzzFont")
    target_link_libraries(MagnumHarfBuzzFontTestLib
        Magnum::Magnum
        Magnum::Text
        ${FREETYPE_LIBRARIES}
        HarfBuzz::HarfBuzz
        MagnumFreeTypeFontTestLib)
    add_subdirectory(Test)
endif()

# MagnumPlugins HarfBuzzFont target alias for superprojects
add_library(MagnumPlugins::HarfBuzzFont ALIAS HarfBuzzFont)
