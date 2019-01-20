#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
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

find_package(Magnum REQUIRED Trade AnyImageImporter)

if(BUILD_PLUGINS_STATIC)
    set(MAGNUM_TINYGLTFIMPORTER_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

# TinyGltfImporter plugin
add_plugin(TinyGltfImporter
    "${MAGNUM_PLUGINS_IMPORTER_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_IMPORTER_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_RELEASE_LIBRARY_INSTALL_DIR}"
    TinyGltfImporter.conf
    TinyGltfImporter.cpp
    TinyGltfImporter.h)
if(BUILD_PLUGINS_STATIC AND BUILD_STATIC_PIC)
    set_target_properties(TinyGltfImporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
# I could include tinygltf directly, but I want to suppress the warnings from
# it, so doing it this way. Also: it's PRIVATE, because the file is not (and
# should *never* be) included in a public header due to its extreme size.
target_include_directories(TinyGltfImporter SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/src/MagnumExternal/TinyGltf)
target_include_directories(TinyGltfImporter PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_link_libraries(TinyGltfImporter PUBLIC Magnum::Trade)
if(CORRADE_TARGET_WINDOWS)
    target_link_libraries(TinyGltfImporter PUBLIC Magnum::AnyImageImporter)
elseif(BUILD_PLUGINS_STATIC)
    target_link_libraries(TinyGltfImporter INTERFACE Magnum::AnyImageImporter)
endif()

install(FILES TinyGltfImporter.h ${CMAKE_CURRENT_BINARY_DIR}/configure.h
    DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/TinyGltfImporter)

# Automatic static plugin import
if(BUILD_PLUGINS_STATIC)
    install(FILES importStaticPlugin.cpp DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/TinyGltfImporter)
    target_sources(TinyGltfImporter INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/importStaticPlugin.cpp)
endif()

if(BUILD_TESTS)
    add_subdirectory(Test)
endif()

# MagnumPlugins TinyGltfImporter target alias for superprojects
add_library(MagnumPlugins::TinyGltfImporter ALIAS TinyGltfImporter)
