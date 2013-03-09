find_package(Qt4 REQUIRED)

include_directories(${QT_INCLUDE_DIR})

set(ColladaImporter_SRCS
    ColladaImporter.cpp)

set(ColladaImporter_HEADERS
    ColladaImporter.h
    ColladaType.h
    Utility.h)

add_library(ColladaImporterObjects OBJECT ${ColladaImporter_SRCS})
set_target_properties(ColladaImporterObjects PROPERTIES COMPILE_FLAGS ${CMAKE_SHARED_LIBRARY_CXX_FLAGS})

corrade_add_plugin(ColladaImporter ${MAGNUM_PLUGINS_IMPORTER_INSTALL_DIR}
    ColladaImporter.conf
    $<TARGET_OBJECTS:ColladaImporterObjects>
    pluginRegistrationColladaImporter.cpp)
target_link_libraries(ColladaImporter
    ${MAGNUM_LIBRARIES}
    ${QT_QTCORE_LIBRARY}
    ${QT_QTXMLPATTERNS_LIBRARY})

install(FILES ${ColladaImporter_HEADERS} DESTINATION ${MAGNUM_PLUGINS_INCLUDE_INSTALL_DIR}/ColladaImporter)

if(BUILD_TESTS)
    add_library(ColladaImporterTestLib $<TARGET_OBJECTS:ColladaImporterObjects>)
    target_link_libraries(ColladaImporterTestLib TgaImporterTestLib
        ${MAGNUM_LIBRARIES}
        ${QT_QTCORE_LIBRARY}
        ${QT_QTXMLPATTERNS_LIBRARY})
    add_subdirectory(Test)
endif()

if(WIN32)
    target_link_libraries(ColladaImporter TgaImporter)
endif()
