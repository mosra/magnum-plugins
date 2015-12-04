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
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/src/external/stb)

set(StbImageImporter_SRCS
    StbImageImporter.cpp)

set(StbImageImporter_HEADERS
    StbImageImporter.h)

# Objects shared between plugin and test library
add_library(StbImageImporterObjects OBJECT
    ${StbImageImporter_SRCS}
    ${StbImageImporter_HEADERS})
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(StbImageImporterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# StbImageImporter plugin
add_plugin(StbImageImporter ${MAGNUM_PLUGINS_IMPORTER_DEBUG_INSTALL_DIR} ${MAGNUM_PLUGINS_IMPORTER_RELEASE_INSTALL_DIR}
    StbImageImporter.conf
    $<TARGET_OBJECTS:StbImageImporterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(StbImageImporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_link_libraries(StbImageImporter ${MAGNUM_LIBRARIES})

install(FILES ${StbImageImporter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/StbImageImporter)

if(BUILD_TESTS)
    add_library(MagnumStbImageImporterTestLib STATIC $<TARGET_OBJECTS:StbImageImporterObjects>)
    target_link_libraries(MagnumStbImageImporterTestLib ${MAGNUM_LIBRARIES} ${PNG_LIBRARIES})
    add_subdirectory(Test)
endif()
