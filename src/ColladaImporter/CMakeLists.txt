find_package(Qt4 REQUIRED)

set(ColladaImporter_SRCS
    ColladaImporter.cpp
)
add_library(ColladaImporterObjects OBJECT ${ColladaImporter_SRCS})
if(USE_FPIC)
    set_target_properties(ColladaImporterObjects PROPERTIES COMPILE_FLAGS -fPIC)
endif()

corrade_add_plugin(ColladaImporter ${MAGNUM_PLUGINS_IMPORTER_INSTALL_DIR}
    ColladaImporter.conf
    $<TARGET_OBJECTS:ColladaImporterObjects>
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
