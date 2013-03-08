find_package(Qt4 REQUIRED)

include_directories(${QT_INCLUDE_DIR})

set(ColladaImporter_SRCS
    ColladaImporter.cpp
)
add_library(ColladaImporterObjects OBJECT ${ColladaImporter_SRCS})
set_target_properties(ColladaImporterObjects PROPERTIES COMPILE_FLAGS ${CMAKE_SHARED_LIBRARY_CXX_FLAGS})

corrade_add_plugin(ColladaImporter ${MAGNUM_PLUGINS_IMPORTER_INSTALL_DIR}
    ColladaImporter.conf
    $<TARGET_OBJECTS:ColladaImporterObjects>
    pluginRegistrationColladaImporter.cpp
)
target_link_libraries(ColladaImporter ${QT_QTCORE_LIBRARY} ${QT_QTXMLPATTERNS_LIBRARY})

if(BUILD_TESTS)
    add_library(ColladaImporterTestLib $<TARGET_OBJECTS:ColladaImporterObjects>)
    target_link_libraries(ColladaImporterTestLib TgaImporterTestLib ${MAGNUM_LIBRARY} ${CORRADE_UTILITY_LIBRARY} ${CORRADE_PLUGINMANAGER_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTXMLPATTERNS_LIBRARY} ${GLUT_LIBRARY} ${OPENGL_gl_LIBRARY} ${GLEW_LIBRARY})
    enable_testing()
    add_subdirectory(Test)
endif()

if(WIN32)
    target_link_libraries(ColladaImporter ${MAGNUM_LIBRARY} ${CORRADE_UTILITY_LIBRARY} ${CORRADE_PLUGINMANAGER_LIBRARY} ${OPENGL_gl_LIBRARY} ${GLEW_LIBRARY} TgaImporter)
endif()
