#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
#             Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2018 Tobias Stein <stein.tobi@t-online.de>
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

if(BUILD_STATIC)
    set(MAGNUM_TINYGLTFIMPORTER_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

set(TinyGltfImporter_SRCS
    TinyGltfImporter.cpp)

set(TinyGltfImporter_HEADERS
    TinyGltfImporter.h
    visibility.h)

# Objects shared between plugin and test library
add_library(TinyGltfImporterObjects OBJECT
    ${TinyGltfImporter_SRCS}
    ${TinyGltfImporter_HEADERS})
target_include_directories(TinyGltfImporterObjects PUBLIC
    $<TARGET_PROPERTY:Magnum::Magnum,INTERFACE_INCLUDE_DIRECTORIES>
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_compile_definitions(TinyGltfImporterObjects PRIVATE "TinyGltfImporterObjects_EXPORTS")

target_include_directories(TinyGltfImporterObjects SYSTEM PRIVATE
    ${PROJECT_SOURCE_DIR}/src/external/tinygltf)

if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(TinyGltfImporterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
set_target_properties(TinyGltfImporterObjects PROPERTIES FOLDER "MagnumPlugins/TinyGltfImporter")

# TinyGltfImporter plugin
add_plugin(TinyGltfImporter
    "${MAGNUM_PLUGINS_IMPORTER_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_IMPORTER_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_RELEASE_LIBRARY_INSTALL_DIR}"
    TinyGltfImporter.conf
    $<TARGET_OBJECTS:TinyGltfImporterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(TinyGltfImporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(TinyGltfImporter PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_link_libraries(TinyGltfImporter Magnum::Magnum)
if(CORRADE_TARGET_WINDOWS)
    target_link_libraries(TinyGltfImporter AnyImageImporter StbImageImporter)
endif()

install(FILES ${TinyGltfImporter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/TinyGltfImporter)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/configure.h DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/TinyGltfImporter)

# Automatic static plugin import
if(BUILD_STATIC)
    install(FILES importStaticPlugin.cpp DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/TinyGltfImporter)
    if(NOT CMAKE_VERSION VERSION_LESS 3.1)
        target_sources(TinyGltfImporter INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/importStaticPlugin.cpp)
    endif()
endif()

if(BUILD_TESTS)
    if(CORRADE_TARGET_WINDOWS)
        add_library(MagnumTinyGltfImporterTestLib STATIC
            ${TinyGltfImporter_SRCS}
            ${TinyGltfImporter_HEADERS})
        target_include_directories(MagnumTinyGltfImporterTestLib SYSTEM PRIVATE
            ${PROJECT_SOURCE_DIR}/src/external/tinygltf)
        target_compile_definitions(MagnumTinyGltfImporterTestLib PRIVATE
            "MAGNUM_ANYIMAGEIMPORTER_BUILD_STATIC"
            "MAGNUM_STBIMAGEIMPORTER_BUILD_STATIC"
            "MAGNUM_TINYGLTFIMPORTER_BUILD_STATIC")
    else()
        add_library(MagnumTinyGltfImporterTestLib STATIC
            $<TARGET_OBJECTS:TinyGltfImporterObjects>
            ${PROJECT_SOURCE_DIR}/src/dummy.cpp) # XCode workaround, see file comment for details
    endif()

    target_include_directories(MagnumTinyGltfImporterTestLib PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src)
    set_target_properties(MagnumTinyGltfImporterTestLib PROPERTIES FOLDER "MagnumPlugins/TinyGltfImporter")
    target_link_libraries(MagnumTinyGltfImporterTestLib
        Magnum::Magnum
        MagnumAnyImageImporterTestLib
        MagnumStbImageImporterTestLib)
    add_subdirectory(Test)
endif()

# MagnumPlugins TinyGltfImporter target alias for superprojects
add_library(MagnumPlugins::TinyGltfImporter ALIAS TinyGltfImporter)
