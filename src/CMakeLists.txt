set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -std=c++0x")

include_directories(${CORRADE_INCLUDE_DIR} ${MAGNUM_INCLUDE_DIR})

add_subdirectory(ColladaImporter)
add_subdirectory(TgaImporter)

if(BUILD_TESTS)
    enable_testing()
endif()
