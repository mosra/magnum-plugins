include_directories(${CMAKE_CURRENT_BINARY_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ColladaImporterTestConfigure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/ColladaImporterTestConfigure.h)

corrade_add_test(ColladaTypeTest ColladaTypeTest.cpp LIBRARIES ${QT_QTCORE_LIBRARY})
corrade_add_test(UtilityTest UtilityTest.cpp LIBRARIES ${QT_QTCORE_LIBRARY} ${MAGNUM_LIBRARIES})
corrade_add_test(ColladaImporterTest ColladaImporterTest.cpp LIBRARIES ColladaImporterTestLib)
