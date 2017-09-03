#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
#             Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2017 Jonathan Hale <squareys@googlemail.com>
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

find_package(Assimp REQUIRED)

if(BUILD_STATIC)
    set(MAGNUM_ASSIMPIMPORTER_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

set(AssimpImporter_SRCS
    AssimpImporter.cpp)

set(AssimpImporter_HEADERS
    AssimpImporter.h
    visibility.h)

# Objects shared between plugin and test library
add_library(AssimpImporterObjects OBJECT
    ${AssimpImporter_SRCS}
    ${AssimpImporter_HEADERS})
target_include_directories(AssimpImporterObjects PUBLIC
    $<TARGET_PROPERTY:Magnum::Magnum,INTERFACE_INCLUDE_DIRECTORIES>
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_include_directories(AssimpImporterObjects SYSTEM PRIVATE
    $<TARGET_PROPERTY:Assimp::Assimp,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(AssimpImporterObjects PRIVATE "AssimpImporterObjects_EXPORTS")
if(NOT BUILD_STATIC OR BUILD_STATIC_PIC)
    set_target_properties(AssimpImporterObjects PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
set_target_properties(AssimpImporterObjects PROPERTIES FOLDER "MagnumPlugins/AnyImageImporter")

# AssimpImporter plugin
add_plugin(AssimpImporter
    "${MAGNUM_PLUGINS_IMPORTER_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_IMPORTER_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_RELEASE_LIBRARY_INSTALL_DIR}"
    AssimpImporter.conf
    $<TARGET_OBJECTS:AssimpImporterObjects>
    pluginRegistration.cpp)
if(BUILD_STATIC_PIC)
    set_target_properties(AssimpImporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(AssimpImporter PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(AssimpImporter Magnum::Magnum Assimp::Assimp)
if(CORRADE_TARGET_WINDOWS)
    target_link_libraries(AssimpImporter AnyImageImporter)
endif()

install(FILES ${AssimpImporter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/AssimpImporter)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/configure.h DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/AssimpImporter)

if(BUILD_TESTS)
    # On Win32 we need to avoid dllimporting AnyImageImporter symbols, because
    # it would search for the symbols in some DLL even when they were linked
    # statically. However it apparently doesn't matter that they were
    # dllexported when building the static library. EH. And because the
    # -DAssimpImporterObjects_EXPORTS is no longer set in this case, we need
    # to avoid dllimporting AssimpImporter symbols as well.
    if(CORRADE_TARGET_WINDOWS)
        add_library(MagnumAssimpImporterTestLib STATIC
            ${AssimpImporter_SRCS}
            ${AssimpImporter_HEADERS}
            ${AssimpImporter_PRIVATE_HEADERS})
        target_compile_definitions(MagnumAssimpImporterTestLib PRIVATE
            "MAGNUM_ANYIMAGEIMPORTER_BUILD_STATIC"
            "MAGNUM_ASSIMPIMPORTER_BUILD_STATIC")
    else()
        add_library(MagnumAssimpImporterTestLib STATIC
            $<TARGET_OBJECTS:AssimpImporterObjects>
            ${PROJECT_SOURCE_DIR}/src/dummy.cpp) # XCode workaround, see file comment for details
    endif()
    target_include_directories(MagnumAssimpImporterTestLib PUBLIC ${PROJECT_SOURCE_DIR}/src)
    set_target_properties(MagnumAssimpImporterTestLib PROPERTIES FOLDER "MagnumPlugins/AnyImageImporter")
    target_link_libraries(MagnumAssimpImporterTestLib
        Magnum::Magnum
        MagnumAnyImageImporterTestLib
        Assimp::Assimp)

    add_subdirectory(Test)
endif()

# MagnumPlugins AssimpImporter target alias for superprojects
add_library(MagnumPlugins::AssimpImporter ALIAS AssimpImporter)
