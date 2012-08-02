# CMake 2.8.8 required for OBJECT library target
cmake_minimum_required(VERSION 2.8.8)
project(MagnumPlugins)

option(BUILD_TESTS "Build unit tests." OFF)

if(BUILD_TESTS)
    enable_testing()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${MagnumPlugins_SOURCE_DIR}/modules/")

# Populate MAGNUM_*_INSTALL_DIR variables, check for dependencies
set(MAGNUMPLUGINS_INCLUDE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/src")
include(FindMagnumPlugins)

add_subdirectory(modules)
add_subdirectory(src)

install(DIRECTORY src/ DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}
                 FILES_MATCHING PATTERN "*.h"
                 PATTERN "*/Test" EXCLUDE)
