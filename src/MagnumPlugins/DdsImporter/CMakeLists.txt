#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016
#             Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2015 Jonathan Hale <squareys@googlemail.com>
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

set(DdsImporter_SRCS
    DdsImporter.cpp)

set(DdsImporter_HEADERS
    DdsImporter.h)

# Objects shared between plugin and test library
add_library(DdsImporterObjects OBJECT
    ${DdsImporter_SRCS}
    ${DdsImporter_HEADERS})
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(DdsImporterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# DdsImporter plugin
add_plugin(DdsImporter ${MAGNUM_PLUGINS_IMPORTER_DEBUG_INSTALL_DIR} ${MAGNUM_PLUGINS_IMPORTER_RELEASE_INSTALL_DIR}
    DdsImporter.conf
    $<TARGET_OBJECTS:DdsImporterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(DdsImporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_link_libraries(DdsImporter
    ${MAGNUM_LIBRARIES})

install(FILES ${DdsImporter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/DdsImporter)

if(BUILD_TESTS)
    add_library(MagnumDdsImporterTestLib STATIC $<TARGET_OBJECTS:DdsImporterObjects>)
    target_link_libraries(MagnumDdsImporterTestLib ${MAGNUM_LIBRARIES})
    add_subdirectory(Test)
endif()

# Magnum DdsImporter library for superprojects
set(MAGNUMPLUGINS_DDSIMPORTER_LIBRARY DdsImporter CACHE INTERNAL "")
