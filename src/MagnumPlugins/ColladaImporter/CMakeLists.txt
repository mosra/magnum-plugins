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

find_package(Magnum REQUIRED MeshTools)
find_package(Qt4 REQUIRED)

if(BUILD_STATIC)
    set(MAGNUM_COLLADAIMPORTER_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

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
target_include_directories(ColladaImporterObjects PUBLIC
    $<TARGET_PROPERTY:Magnum::Magnum,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:AnyImageImporter,INTERFACE_INCLUDE_DIRECTORIES>
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_compile_definitions(ColladaImporterObjects PRIVATE "ColladaImporterObjects_EXPORTS")
# Supress warnings from Qt includes
target_include_directories(ColladaImporterObjects SYSTEM PRIVATE ${QT_INCLUDE_DIR})
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(ColladaImporterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
set_target_properties(ColladaImporterObjects PROPERTIES FOLDER "MagnumPlugins/ColladaImporter")

# ColladaImporter plugin
add_plugin(ColladaImporter
    "${MAGNUM_PLUGINS_IMPORTER_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_IMPORTER_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_RELEASE_LIBRARY_INSTALL_DIR}"
    ColladaImporter.conf
    $<TARGET_OBJECTS:ColladaImporterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(ColladaImporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(ColladaImporter PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
# Supress warnings from Qt includes
target_include_directories(ColladaImporter SYSTEM PRIVATE ${QT_INCLUDE_DIR})
target_link_libraries(ColladaImporter
    Magnum::Magnum
    Magnum::MeshTools
    ${QT_QTCORE_LIBRARY}
    ${QT_QTXMLPATTERNS_LIBRARY})
if(CORRADE_TARGET_WINDOWS)
    target_link_libraries(ColladaImporter AnyImageImporter)
endif()

install(FILES ${ColladaImporter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/ColladaImporter)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/configure.h DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/ColladaImporter)

if(BUILD_TESTS)
    add_library(MagnumColladaImporterTestLib STATIC
        $<TARGET_OBJECTS:ColladaImporterObjects>
        ${PROJECT_SOURCE_DIR}/src/dummy.cpp) # XCode workaround, see file comment for details
    target_include_directories(MagnumColladaImporterTestLib PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src)
    # Supress warnings from Qt includes
    target_include_directories(MagnumColladaImporterTestLib SYSTEM PUBLIC ${QT_INCLUDE_DIR})
    set_target_properties(MagnumColladaImporterTestLib PROPERTIES FOLDER "MagnumPlugins/ColladaImporter")
    target_link_libraries(MagnumColladaImporterTestLib
        Magnum::Magnum
        Magnum::MeshTools
        ${QT_QTCORE_LIBRARY}
        ${QT_QTXMLPATTERNS_LIBRARY}
        MagnumAnyImageImporterTestLib)
    add_subdirectory(Test)
endif()

# MagnumPlugins ColladaImporter target alias for superprojects
add_library(MagnumPlugins::ColladaImporter ALIAS ColladaImporter)
