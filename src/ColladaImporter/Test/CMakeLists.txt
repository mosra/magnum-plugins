configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ColladaImporterTestConfigure.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ColladaImporterTestConfigure.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

corrade_add_test(ColladaTypeTest ColladaTypeTest.h ColladaTypeTest.cpp)
corrade_add_test(UtilityTest UtilityTest.h UtilityTest.cpp)
corrade_add_test(ColladaImporterTest ColladaImporterTest.h ColladaImporterTest.cpp)
target_link_libraries(ColladaImporterTest ColladaImporterTestLib)
