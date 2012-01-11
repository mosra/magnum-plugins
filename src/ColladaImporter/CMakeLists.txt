find_package(Qt4 REQUIRED)

corrade_add_resource(ColladaImporterShaders shaders PhongShader.frag PhongShader.vert)
corrade_add_plugin(ColladaImporter ${MAGNUM_PLUGINS_IMPORTER_INSTALL_DIR} ColladaImporter.conf ColladaImporter.cpp Material.cpp PhongShader.cpp ${ColladaImporterShaders})
target_link_libraries(ColladaImporter ${QT_QTCORE_LIBRARY} ${QT_QTXMLPATTERNS_LIBRARY})

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(Test)
endif()
