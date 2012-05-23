set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -std=c++0x -fvisibility=hidden")

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CORRADE_INCLUDE_DIR} ${MAGNUM_INCLUDE_DIR})

# On 64bit Linux with GCC flag -fPIC is needed for building some static libraries
if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_COMPILER_IS_GNUCC AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(USE_FPIC ON)
else()
    set(USE_FPIC OFF)
endif()

add_subdirectory(ColladaImporter)
add_subdirectory(TgaImporter)

if(BUILD_TESTS)
    enable_testing()
endif()
