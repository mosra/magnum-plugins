configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ColladaImporterTestConfigure.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ColladaImporterTestConfigure.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

corrade_add_test2(ColladaTypeTest ColladaTypeTest.cpp LIBRARIES ${QT_QTCORE_LIBRARY})
corrade_add_test2(UtilityTest UtilityTest.cpp LIBRARIES ${QT_QTCORE_LIBRARY})
corrade_add_test2(ColladaImporterTest ColladaImporterTest.cpp LIBRARIES ColladaImporterTestLib)
