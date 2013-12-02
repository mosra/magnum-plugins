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

find_package(Magnum REQUIRED TgaImporter)
find_package(Qt4 REQUIRED)

# Silence pedantic warnings in Qt headers
include_directories(SYSTEM ${QT_INCLUDE_DIR})

set(ColladaImporter_SRCS
    ColladaImporter.cpp)

set(ColladaImporter_HEADERS
    ColladaImporter.h)

add_library(ColladaImporterObjects OBJECT ${ColladaImporter_SRCS})
set_target_properties(ColladaImporterObjects PROPERTIES COMPILE_FLAGS "${CMAKE_SHARED_LIBRARY_CXX_FLAGS}")

add_plugin(ColladaImporter ${MAGNUM_PLUGINS_IMPORTER_INSTALL_DIR}
    ColladaImporter.conf
    $<TARGET_OBJECTS:ColladaImporterObjects>
    pluginRegistrationColladaImporter.cpp)
target_link_libraries(ColladaImporter
    ${MAGNUM_LIBRARIES}
    ${QT_QTCORE_LIBRARY}
    ${QT_QTXMLPATTERNS_LIBRARY})

if(WIN32)
    target_link_libraries(ColladaImporter ${MAGNUM_TGAIMPORTER_LIBRARY})
endif()

install(FILES ${ColladaImporter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/ColladaImporter)

if(BUILD_TESTS)
    add_library(ColladaImporterTestLib STATIC $<TARGET_OBJECTS:ColladaImporterObjects>)
    target_link_libraries(ColladaImporterTestLib
        ${MAGNUM_LIBRARIES}
        ${QT_QTCORE_LIBRARY}
        ${QT_QTXMLPATTERNS_LIBRARY}
        ${MAGNUM_TGAIMPORTER_LIBRARY})

    add_subdirectory(Test)
endif()
