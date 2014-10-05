#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014
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

include_directories(${PNG_INCLUDE_DIRS})

set(PngImporter_SRCS
    PngImporter.cpp)

set(PngImporter_HEADERS
    PngImporter.h)

# Objects shared between plugin and test library
add_library(PngImporterObjects OBJECT
    ${PngImporter_SRCS}
    ${PngImporter_HEADERS})
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(PngImporterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# PngImporter plugin
add_plugin(PngImporter ${MAGNUM_PLUGINS_IMPORTER_DEBUG_INSTALL_DIR} ${MAGNUM_PLUGINS_IMPORTER_RELEASE_INSTALL_DIR}
    PngImporter.conf
    $<TARGET_OBJECTS:PngImporterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(PngImporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_link_libraries(PngImporter
    ${MAGNUM_LIBRARIES}
    ${PNG_LIBRARIES})

install(FILES ${PngImporter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/PngImporter)

if(BUILD_TESTS)
    add_library(MagnumPngImporterTestLib STATIC $<TARGET_OBJECTS:PngImporterObjects>)
    target_link_libraries(MagnumPngImporterTestLib ${MAGNUM_LIBRARIES} ${PNG_LIBRARIES})
    add_subdirectory(Test)
endif()
