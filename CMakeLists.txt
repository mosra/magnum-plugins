# CMake 2.8.8 required for OBJECT library target
cmake_minimum_required(VERSION 2.8.8)
project(MagnumPlugins)

option(BUILD_TESTS "Build unit tests." OFF)
if(BUILD_TESTS)
    enable_testing()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/modules/")

add_subdirectory(src)
