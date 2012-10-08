set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wold-style-cast -Winit-self -pedantic -std=c++0x -fvisibility=hidden")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdouble-promotion")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CORRADE_INCLUDE_DIR} ${MAGNUM_INCLUDE_DIRS})

add_subdirectory(ColladaImporter)
add_subdirectory(TgaImporter)

if(BUILD_TESTS)
    enable_testing()
endif()
