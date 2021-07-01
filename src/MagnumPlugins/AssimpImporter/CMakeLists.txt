#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2017 Jonathan Hale <squareys@googlemail.com>
#   Copyright © 2021 Pablo Escobar <mail@rvrs.in>
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
find_package(Assimp REQUIRED)

if(BUILD_PLUGINS_STATIC AND NOT DEFINED MAGNUM_ASSIMPIMPORTER_BUILD_STATIC)
    set(MAGNUM_ASSIMPIMPORTER_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

# Use try_compile() to figure out a concrete Assimp insanity level, as most
# relevant bugfixes are either not in any stable version yet or the version
# info is broken. Unfortunately, because try_compile() is run at configure
# time, LINK_LIBRARIES only works for imported targets as targets defined
# *inside* the project as are not built at that point yet. So when Assimp is a
# CMake subproject, it'll complain that Assimp::Assimp doesn't exist. To work
# around that issue, we build a static library (to avoid linking to anything)
# and supply INCLUDE_DIRECTORIES. Static library is only available since CMake
# 3.6, but eh, we can't have everything.
if(TARGET assimp) # Only defined if we have a CMake subproject
    get_target_property(_ASSIMP_INCLUDE_DIRECTORIES assimp SOURCE_DIR)
    get_target_property(_ASSIMP_BINARY_INCLUDE_DIRECTORIES assimp BINARY_DIR)
    get_filename_component(_ASSIMP_INCLUDE_DIRECTORIES ${_ASSIMP_INCLUDE_DIRECTORIES} DIRECTORY)
    get_filename_component(_ASSIMP_BINARY_INCLUDE_DIRECTORIES ${_ASSIMP_BINARY_INCLUDE_DIRECTORIES} DIRECTORY)
    set(_ASSIMP_TRY_COMPILE_LINK_OR_INCLUDE CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${_ASSIMP_INCLUDE_DIRECTORIES}/include\;${_ASSIMP_BINARY_INCLUDE_DIRECTORIES}/include")
    if(CMAKE_VERSION VERSION_LESS 3.6)
        message(FATAL_ERROR "Sorry, using Assimp as a CMake submodule requires CMake 3.6 in order to check for presence of various bugs. To continue, either upgrade CMake or use an external build of Assimp.")
    else()
        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    endif()
else()
    set(_ASSIMP_TRY_COMPILE_LINK_OR_INCLUDE LINK_LIBRARIES Assimp::Assimp)
endif()

# Go through all versions of interest and pick the highest one that compiles
foreach(_version 20201123 20190915 20151031)
    try_compile(_works ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/checkAssimpVersion.cpp
        ${_ASSIMP_TRY_COMPILE_LINK_OR_INCLUDE}
        COMPILE_DEFINITIONS
            -DCHECK_VERSION=${_version}
            # I thought I could make this use C++11 by linking to
            # Corrade::Utility, but for some reason the effect of the
            # CORRADE_CXX_STANDARD property doesn't get passed through. So I
            # have to use an internal variable for that instead.
            ${CORRADE_CXX11_STANDARD_FLAG}
        OUTPUT_VARIABLE _version_output)
    if(_works)
        set(ASSIMP_VERSION ${_version})
        break()
    endif()
endforeach()
if(NOT ASSIMP_VERSION)
    message(SEND_ERROR "Could not detect Assimp version. Maybe even older than 3.2? ${_version_output}")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configureInternal.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configureInternal.h)

# AssimpImporter plugin
add_plugin(AssimpImporter
    "${MAGNUM_PLUGINS_IMPORTER_DEBUG_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_DEBUG_LIBRARY_INSTALL_DIR}"
    "${MAGNUM_PLUGINS_IMPORTER_RELEASE_BINARY_INSTALL_DIR};${MAGNUM_PLUGINS_IMPORTER_RELEASE_LIBRARY_INSTALL_DIR}"
    AssimpImporter.conf
    AssimpImporter.cpp
    AssimpImporter.h)
target_include_directories(AssimpImporter PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
if(MAGNUM_ASSIMPIMPORTER_BUILD_STATIC AND BUILD_STATIC_PIC)
    set_target_properties(AssimpImporter PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_include_directories(AssimpImporter PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
target_link_libraries(AssimpImporter PUBLIC Magnum::Trade Assimp::Assimp)
if(CORRADE_TARGET_WINDOWS)
    target_link_libraries(AssimpImporter PUBLIC Magnum::AnyImageImporter)
elseif(MAGNUM_ASSIMPIMPORTER_BUILD_STATIC)
    target_link_libraries(AssimpImporter INTERFACE Magnum::AnyImageImporter)
endif()
# Modify output location only if all are set, otherwise it makes no sense
if(CMAKE_RUNTIME_OUTPUT_DIRECTORY AND CMAKE_LIBRARY_OUTPUT_DIRECTORY AND CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set_target_properties(AssimpImporter PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/magnum$<$<CONFIG:Debug>:-d>/importers
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/magnum$<$<CONFIG:Debug>:-d>/importers
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/magnum$<$<CONFIG:Debug>:-d>/importers)
endif()

install(FILES AssimpImporter.h ${CMAKE_CURRENT_BINARY_DIR}/configure.h
    DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/AssimpImporter)

# Automatic static plugin import
if(MAGNUM_ASSIMPIMPORTER_BUILD_STATIC)
    install(FILES importStaticPlugin.cpp DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/AssimpImporter)
    target_sources(AssimpImporter INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/importStaticPlugin.cpp)
endif()

if(BUILD_TESTS)
    add_subdirectory(Test)
endif()

# MagnumPlugins AssimpImporter target alias for superprojects
add_library(MagnumPlugins::AssimpImporter ALIAS AssimpImporter)
