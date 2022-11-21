#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2022 Samuli Raivio <bqqbarbhg@gmail.com>
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

find_package(Magnum REQUIRED Trade MeshTools AnyImageImporter)

if(MAGNUM_BUILD_PLUGINS_STATIC AND NOT DEFINED MAGNUM_UFBXIMPORTER_BUILD_STATIC)
    set(MAGNUM_UFBXIMPORTER_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

# UfbxImporter plugin
add_plugin(UfbxImporter
    importers
    "${MAGNUM_PLUGINS_IMPORTER_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_IMPORTER_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_RELEASE_LIBRARY_INSTALL_DIR}"
    UfbxImporter.conf
    UfbxImporter.cpp
    UfbxImporter.h)
if(MAGNUM_UFBXIMPORTER_BUILD_STATIC AND MAGNUM_BUILD_STATIC_PIC)
    set_target_properties(UfbxImporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(UfbxImporter PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)

# Include the ufbx files as a system directory to supress warnings
# TODO(bqqbarbhg): This shouldn't be necessary from a warning standpoint at least
# as the library is written to support very strict warning environments.
target_include_directories(UfbxImporter SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/src/external/ufbx)
target_link_libraries(UfbxImporter PUBLIC Magnum::Trade Magnum::MeshTools)
if(CORRADE_TARGET_WINDOWS)
    target_link_libraries(UfbxImporter PUBLIC Magnum::AnyImageImporter)
elseif(MAGNUM_UFBXIMPORTER_BUILD_STATIC)
    target_link_libraries(UfbxImporter INTERFACE Magnum::AnyImageImporter)
endif()

install(FILES UfbxImporter.h ${CMAKE_CURRENT_BINARY_DIR}/configure.h
    DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/UfbxImporter)

# Automatic static plugin import
if(MAGNUM_UFBXIMPORTER_BUILD_STATIC)
    install(FILES importStaticPlugin.cpp DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/UfbxImporter)
    target_sources(UfbxImporter INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/importStaticPlugin.cpp)
endif()

if(MAGNUM_BUILD_TESTS)
    add_subdirectory(Test ${EXCLUDE_FROM_ALL_IF_TEST_TARGET})
endif()

# MagnumPlugins UfbxImporter target alias for superprojects
add_library(MagnumPlugins::UfbxImporter ALIAS UfbxImporter)
