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

# Include the stb_*.h files as a system directory to supress warnings
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/src/external/stb)

set(StbPngImageConverter_SRCS
    StbPngImageConverter.cpp)

set(StbPngImageConverter_HEADERS
    StbPngImageConverter.h)

# Objects shared between plugin and test library
add_library(StbPngImageConverterObjects OBJECT
    ${StbPngImageConverter_SRCS}
    ${StbPngImageConverter_HEADERS})
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(StbPngImageConverterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# StbPngImageConverter plugin
add_plugin(StbPngImageConverter ${MAGNUM_PLUGINS_IMAGECONVERTER_DEBUG_INSTALL_DIR} ${MAGNUM_PLUGINS_IMAGECONVERTER_RELEASE_INSTALL_DIR}
    StbPngImageConverter.conf
    $<TARGET_OBJECTS:StbPngImageConverterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(StbPngImageConverter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_link_libraries(StbPngImageConverter ${MAGNUM_LIBRARIES})

install(FILES ${StbPngImageConverter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/StbPngImageConverter)

if(BUILD_TESTS)
    add_library(MagnumStbPngImageConverterTestLib STATIC $<TARGET_OBJECTS:StbPngImageConverterObjects>)
    target_link_libraries(MagnumStbPngImageConverterTestLib ${MAGNUM_LIBRARIES} ${PNG_LIBRARIES})
    add_subdirectory(Test)
endif()
