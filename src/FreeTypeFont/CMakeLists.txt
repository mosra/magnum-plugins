#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013 Vladimír Vondruš <mosra@centrum.cz>
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

include_directories(${FREETYPE_INCLUDE_DIRS})

set(FreeTypeFont_SRCS
    FreeTypeFont.cpp)

set(FreeTypeFont_HEADERS
    FreeTypeFont.h)

add_library(FreeTypeFontObjects OBJECT ${FreeTypeFont_SRCS})
set_target_properties(FreeTypeFontObjects PROPERTIES COMPILE_FLAGS "${CMAKE_SHARED_LIBRARY_CXX_FLAGS}")

add_plugin(FreeTypeFont ${MAGNUM_PLUGINS_FONT_INSTALL_DIR}
    FreeTypeFont.conf
    $<TARGET_OBJECTS:FreeTypeFontObjects>
    pluginRegistrationFreeTypeFont.cpp)
target_link_libraries(FreeTypeFont
    ${MAGNUM_LIBRARIES}
    ${MAGNUM_TEXT_LIBRARIES}
    ${FREETYPE_LIBRARIES})

install(FILES ${FreeTypeFont_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/FreeTypeFont)

# The lib is needed by non-GL tests
if(BUILD_TESTS)
    add_library(FreeTypeFontTestLib $<TARGET_OBJECTS:FreeTypeFontObjects>)
    target_link_libraries(FreeTypeFontTestLib
        ${MAGNUM_LIBRARIES}
        ${MAGNUM_TEXT_LIBRARIES}
        ${FREETYPE_LIBRARIES})
endif()

if(BUILD_GL_TESTS)
    add_subdirectory(Test)
endif()
