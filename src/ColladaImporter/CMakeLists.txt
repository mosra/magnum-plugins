find_package(Qt4 REQUIRED)

corrade_add_resource(ColladaImporterShaders shaders PhongShader.frag PhongShader.vert)
set(ColladaImporter_SRCS
    ColladaImporter.cpp
    Material.cpp
    PhongShader.cpp
    ${ColladaImporterShaders}
)

corrade_add_plugin(ColladaImporter ${MAGNUM_PLUGINS_IMPORTER_INSTALL_DIR} ColladaImporter.conf ${ColladaImporter_SRCS})
target_link_libraries(ColladaImporter ${QT_QTCORE_LIBRARY} ${QT_QTXMLPATTERNS_LIBRARY})

if(BUILD_TESTS)
    add_library(ColladaImporterTestLib ${ColladaImporter_SRCS})
    target_link_libraries(ColladaImporterTestLib ${MAGNUM_LIBRARY} ${CORRADE_UTILITY_LIBRARY} ${CORRADE_PLUGINMANAGER_LIBRARY} ${QT_QTCORE_LIBRARY} ${QT_QTXMLPATTERNS_LIBRARY} ${GLUT_LIBRARY} ${OPENGL_gl_LIBRARY} ${GLEW_LIBRARY})
    enable_testing()
    add_subdirectory(Test)
endif()
