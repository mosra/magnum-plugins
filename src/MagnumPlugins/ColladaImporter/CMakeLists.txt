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

find_package(Magnum REQUIRED MeshTools)
find_package(Qt4 REQUIRED)

# Silence pedantic warnings in Qt headers
include_directories(SYSTEM ${QT_INCLUDE_DIR})

set(ColladaImporter_SRCS
    ColladaImporter.cpp)

set(ColladaImporter_HEADERS
    ColladaImporter.h)

# Header files to display in project view of IDEs only
set(ColladaImporter_PRIVATE_HEADERS
    ColladaType.h
    Utility.h)

# Objects shared between plugin and test library
add_library(ColladaImporterObjects OBJECT
    ${ColladaImporter_SRCS}
    ${ColladaImporter_HEADERS}
    ${ColladaImporter_PRIVATE_HEADERS})
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(ColladaImporterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# ColladaImporter plugin
add_plugin(ColladaImporter ${MAGNUM_PLUGINS_IMPORTER_DEBUG_INSTALL_DIR} ${MAGNUM_PLUGINS_IMPORTER_RELEASE_INSTALL_DIR}
    ColladaImporter.conf
    $<TARGET_OBJECTS:ColladaImporterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(ColladaImporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

target_link_libraries(ColladaImporter
    ${MAGNUM_LIBRARIES}
    ${MAGNUM_MESHTOOLS_LIBRARIES}
    ${QT_QTCORE_LIBRARY}
    ${QT_QTXMLPATTERNS_LIBRARY})

if(CORRADE_TARGET_WINDOWS)
    target_link_libraries(ColladaImporter AnyImageImporter)
endif()

install(FILES ${ColladaImporter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/ColladaImporter)

if(BUILD_TESTS)
    add_library(MagnumColladaImporterTestLib STATIC $<TARGET_OBJECTS:ColladaImporterObjects>)
    target_link_libraries(MagnumColladaImporterTestLib
        ${MAGNUM_LIBRARIES}
        ${MAGNUM_MESHTOOLS_LIBRARIES}
        ${QT_QTCORE_LIBRARY}
        ${QT_QTXMLPATTERNS_LIBRARY}
        MagnumAnyImageImporterTestLib)
    add_subdirectory(Test)
endif()

# Magnum ColladaImporter library for superprojects
set(MAGNUMPLUGINS_COLLADAIMPORTER_LIBRARY ColladaImporter CACHE INTERNAL "")
