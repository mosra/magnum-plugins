find_package(Magnum REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CORRADE_CXX_FLAGS}")
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CORRADE_INCLUDE_DIRS} ${MAGNUM_INCLUDE_DIRS})

add_subdirectory(ColladaImporter)
add_subdirectory(TgaImporter)
