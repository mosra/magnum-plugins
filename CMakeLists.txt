# CMake 2.8.8 required for OBJECT library target
cmake_minimum_required(VERSION 2.8.8)
project(MagnumPlugins)

option(BUILD_TESTS "Build unit tests (requires Qt4)." OFF)

if(BUILD_TESTS)
    find_package(Qt4)

    if(NOT QT4_FOUND)
        message(WARNING "Qt4 is required for building unit tests. No tests will be build.")
        set(BUILD_TESTS OFF)
    else()
        enable_testing()
    endif()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${MagnumPlugins_SOURCE_DIR}/modules/")
find_package(Magnum REQUIRED)

add_subdirectory(modules)
add_subdirectory(src)

install(DIRECTORY src/ DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}
                 FILES_MATCHING PATTERN "*.h"
                 PATTERN "*/Test" EXCLUDE)
