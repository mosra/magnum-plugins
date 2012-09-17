/*
    Copyright © 2010, 2011, 2012 Vladimír Vondruš <mosra@centrum.cz>

    This file is part of Magnum.

    Magnum is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License version 3
    only, as published by the Free Software Foundation.

    Magnum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License version 3 for more details.
*/

#include "ColladaImporterTest.h"

#include "Utility/Directory.h"
#include <Math/Constants.h>
#include "Trade/PhongMaterialData.h"
#include <Trade/ImageData.h>
#include <Trade/MeshData3D.h>
#include <Trade/MeshObjectData3D.h>
#include "Trade/SceneData.h"
#include "../ColladaImporter.h"

#include "ColladaImporterTestConfigure.h"

using namespace std;
using namespace Corrade::Utility;

CORRADE_TEST_MAIN(Magnum::Trade::ColladaImporter::Test::ColladaImporterTest)

namespace Magnum { namespace Trade { namespace ColladaImporter { namespace Test {

ColladaImporterTest::ColladaImporterTest() {
    addTests(&ColladaImporterTest::openWrongNamespace,
             &ColladaImporterTest::openWrongVersion,
             &ColladaImporterTest::parseSource,
             &ColladaImporterTest::scene,
             &ColladaImporterTest::mesh,
             &ColladaImporterTest::material,
             &ColladaImporterTest::image);
}

void ColladaImporterTest::openWrongNamespace() {
    ColladaImporter importer;
    stringstream debug;
    Error::setOutput(&debug);
    CORRADE_VERIFY(!importer.open(Directory::join(COLLADAIMPORTER_TEST_DIR, "openWrongNamespace.dae")));
    CORRADE_COMPARE(debug.str(), "ColladaImporter: unsupported namespace \"http://www.collada.org/2006/11/COLLADASchema\"\n");
}

void ColladaImporterTest::openWrongVersion() {
    ColladaImporter importer;
    stringstream debug;
    Error::setOutput(&debug);
    CORRADE_VERIFY(!importer.open(Directory::join(COLLADAIMPORTER_TEST_DIR, "openWrongVersion.dae")));
    CORRADE_COMPARE(debug.str(), "ColladaImporter: unsupported version \"1.4.0\"\n");
}

void ColladaImporterTest::parseSource() {
    ColladaImporter importer;
    CORRADE_VERIFY(importer.open(Directory::join(COLLADAIMPORTER_TEST_DIR, "parseSource.dae")));

    stringstream debug;
    Error::setOutput(&debug);
    CORRADE_VERIFY(importer.parseSource<Vector3>("WrongTotalCount").empty());
    CORRADE_COMPARE(debug.str(), "ColladaImporter: wrong total count in source \"WrongTotalCount\"\n");

    {
        CORRADE_EXPECT_FAIL("Swapped coordinates in source are not implemented.");
        CORRADE_COMPARE(importer.parseSource<Vector3>("SwappedCoords"), (vector<Vector3>{Vector3(0, 1, 2)}));
    }

    CORRADE_COMPARE(importer.parseSource<Point3D>("MoreElements"), (vector<Point3D>{
        {0, 1, 2},
        {3, 4, 5}
    }));
}

void ColladaImporterTest::scene() {
    ostringstream debug;
    Error::setOutput(&debug);

    ColladaImporter importer;
    CORRADE_VERIFY(importer.open(Directory::join(COLLADAIMPORTER_TEST_DIR, "scene.dae")));

    CORRADE_COMPARE(importer.defaultScene(), 1);
    CORRADE_COMPARE(importer.sceneCount(), 2);
    CORRADE_COMPARE(importer.object3DCount(), 6);

    SceneData* scene = importer.scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->name(), "Scene");
    CORRADE_COMPARE(scene->children3D(), (vector<unsigned int>{0, 2}));

    ObjectData3D* object = importer.object3D(0);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->name(), "Camera");
    CORRADE_COMPARE(importer.object3DForName("Camera"), 0);
    CORRADE_VERIFY(object->instanceType() == ObjectData3D::InstanceType::Camera);
    CORRADE_COMPARE(object->instanceId(), 2);
    CORRADE_COMPARE(object->children(), vector<unsigned int>{1});

    object = importer.object3D(1);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->name(), "Light");
    CORRADE_COMPARE(importer.object3DForName("Light"), 1);
    CORRADE_VERIFY(object->instanceType() == ObjectData3D::InstanceType::Light);
    CORRADE_COMPARE(object->instanceId(), 1);
    CORRADE_VERIFY(object->children().empty());

    object = importer.object3D(2);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->name(), "Mesh");
    CORRADE_COMPARE(importer.object3DForName("Mesh"), 2);
    CORRADE_VERIFY(object->instanceType() == ObjectData3D::InstanceType::Mesh);
    CORRADE_COMPARE(object->instanceId(), 2);
    Matrix4 transformation =
        Matrix4::translation({1, 2, 3})*
        Matrix4::rotation(deg(60.0f), Vector3::zAxis())*
        Matrix4::rotation(deg(90.0f), Vector3::yAxis())*
        Matrix4::rotation(deg(120.0f), Vector3::xAxis())*
        Matrix4::scaling({3, 4, 5});
    CORRADE_COMPARE(object->transformation(), transformation);
    CORRADE_COMPARE(static_cast<MeshObjectData3D*>(object)->material(), 1);

    CORRADE_VERIFY(!importer.object3D(3));
    CORRADE_VERIFY(!importer.object3D(4));
    CORRADE_VERIFY(!importer.object3D(5));
    CORRADE_VERIFY(debug.str() == "ColladaImporter: \"instance_wrong\" instance type not supported\n"
                                  "ColladaImporter: mesh \"InexistentMesh\" was not found\n"
                                  "ColladaImporter: material \"InexistentMaterial\" was not found\n");
}

void ColladaImporterTest::mesh() {
    ColladaImporter importer;
    CORRADE_VERIFY(importer.open(Directory::join(COLLADAIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer.mesh3DCount(), 5);

    stringstream debug;
    Error::setOutput(&debug);
    CORRADE_VERIFY(!importer.mesh3D(0));
    CORRADE_COMPARE(importer.mesh3DForName("WrongPrimitives"), 0);
    CORRADE_COMPARE(debug.str(), "ColladaImporter: 5 vertices per face not supported\n");

    /* Vertex only mesh */
    CORRADE_VERIFY(importer.mesh3D(1));
    MeshData3D* mesh = importer.mesh3D(1);
    CORRADE_COMPARE(mesh->name(), "MeshVertexOnly");
    CORRADE_COMPARE(importer.mesh3DForName("MeshVertexOnly"), 1);
    CORRADE_VERIFY(mesh->primitive() == Mesh::Primitive::Triangles);
    CORRADE_COMPARE(*mesh->indices(), (vector<unsigned int>{
        0, 1, 2, 0, 2, 3, 4, 0, 3, 4, 3, 5
    }));
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(*mesh->positions(0), (vector<Point3D>{
        {1, -1, 1},
        {1, -1, -1},
        {1, 1, -1},
        {1, 1, 1},
        {-1, -1, 1},
        {-1, 1, 1}
    }));
    CORRADE_COMPARE(mesh->normalArrayCount(), 0);
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 0);

    /* Mesh with quads */
    CORRADE_VERIFY(importer.mesh3D(2));
    mesh = importer.mesh3D(2);
    CORRADE_COMPARE(mesh->name(), "MeshQuads");
    CORRADE_COMPARE(importer.mesh3DForName("MeshQuads"), 2);
    CORRADE_COMPARE(*mesh->indices(), (vector<unsigned int>{
        0, 1, 2, 0, 2, 3, 4, 0, 3, 4, 3, 5, 0, 1, 2, 0, 2, 3, 4, 0, 3
    }));

    /* Vertex and normal mesh */
    CORRADE_VERIFY(importer.mesh3D(3));
    mesh = importer.mesh3D(3);
    CORRADE_COMPARE(mesh->name(), "MeshVertexNormals");
    CORRADE_COMPARE(importer.mesh3DForName("MeshVertexNormals"), 3);
    CORRADE_VERIFY(mesh->primitive() == Mesh::Primitive::Triangles);
    CORRADE_COMPARE(*mesh->indices(), (vector<unsigned int>{
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7
    }));
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(*mesh->positions(0), (vector<Point3D>{
        {1, -1, 1},
        {1, -1, -1},
        {1, 1, -1},
        {1, 1, 1},
        {-1, -1, 1},
        {1, -1, 1},
        {1, 1, 1},
        {-1, 1, 1}
    }));
    CORRADE_COMPARE(mesh->normalArrayCount(), 1);
    CORRADE_COMPARE(*mesh->normals(0), (vector<Vector3>{
        {1, 0, 0},
        {1, 0, 0},
        {1, 0, 0},
        {1, 0, 0},
        {0, 0, 1},
        {0, 0, 1},
        {0, 0, 1},
        {0, 0, 1}
    }));
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 0);

    /* Vertex, normal and texture mesh */
    CORRADE_VERIFY(importer.mesh3D(4));
    mesh = importer.mesh3D(4);
    CORRADE_COMPARE(mesh->name(), "Mesh");
    CORRADE_COMPARE(importer.mesh3DForName("Mesh"), 4);
    CORRADE_VERIFY(mesh->primitive() == Mesh::Primitive::Triangles);
    CORRADE_COMPARE(*mesh->indices(), (vector<unsigned int>{
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7
    }));
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(*mesh->positions(0), (vector<Point3D>{
        {1, -1, 1},
        {1, -1, -1},
        {1, 1, -1},
        {1, 1, 1},
        {-1, -1, 1},
        {1, -1, 1},
        {1, 1, 1},
        {-1, 1, 1}
    }));
    CORRADE_COMPARE(mesh->normalArrayCount(), 1);
    CORRADE_COMPARE(*mesh->normals(0), (vector<Vector3>{
        {1, 0, 0},
        {1, 0, 0},
        {1, 0, 0},
        {1, 0, 0},
        {0, 0, 1},
        {0, 0, 1},
        {0, 0, 1},
        {0, 0, 1}
    }));
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 2);
    CORRADE_COMPARE(*mesh->textureCoords2D(0), (vector<Vector2>{
        {0.5, 1},
        {1, 1},
        {1, 0},
        {0.5, 0},
        {0, 1},
        {0.5, 1},
        {0.5, 0},
        {0, 0}
    }));
    CORRADE_COMPARE(*mesh->textureCoords2D(1), vector<Vector2>(8));
}

void ColladaImporterTest::material() {
    ColladaImporter importer;
    CORRADE_VERIFY(importer.open(Directory::join(COLLADAIMPORTER_TEST_DIR, "material.dae")));

    CORRADE_COMPARE(importer.materialCount(), 3);

    stringstream debug;
    Error::setOutput(&debug);
    CORRADE_VERIFY(!importer.material(0));
    CORRADE_COMPARE(importer.materialForName("MaterialWrongProfile"), 0);
    CORRADE_COMPARE(debug.str(), "ColladaImporter: \"profile_GLSL\" effect profile not supported\n");

    debug.str("");
    CORRADE_VERIFY(!importer.material(1));
    CORRADE_COMPARE(importer.materialForName("MaterialWrongShader"), 1);
    CORRADE_COMPARE(debug.str(), "ColladaImporter: \"lambert\" shader not supported\n");

    CORRADE_VERIFY(importer.material(2));
    PhongMaterialData* material = static_cast<PhongMaterialData*>(importer.material(2));
    CORRADE_COMPARE(material->name(), "MaterialPhong");
    CORRADE_COMPARE(importer.materialForName("MaterialPhong"), 2);
    CORRADE_COMPARE(material->ambientColor(), Vector3(1, 0, 0));
    CORRADE_COMPARE(material->diffuseColor(), Vector3(0, 1, 0));
    CORRADE_COMPARE(material->specularColor(), Vector3(0, 0, 1));
    CORRADE_COMPARE(material->shininess(), 50.0f);
}

void ColladaImporterTest::image() {
    ColladaImporter importer;
    CORRADE_VERIFY(importer.open(Directory::join(COLLADAIMPORTER_TEST_DIR, "image.dae")));

    CORRADE_COMPARE(importer.image2DCount(), 2);

    stringstream debug;
    Error::setOutput(&debug);
    CORRADE_VERIFY(!importer.image2D(0));
    CORRADE_COMPARE(importer.image2DForName("UnsupportedImage"), 0);
    CORRADE_COMPARE(debug.str(), "ColladaImporter: \"image.jpg\" has unsupported format\n");

    CORRADE_VERIFY(importer.image2D(1));
    ImageData2D* image = importer.image2D(1);
    CORRADE_COMPARE(image->name(), "Image");
    CORRADE_COMPARE(importer.image2DForName("Image"), 1);

    /* Check only dimensions, as it is good enough proof that it is working */
    CORRADE_COMPARE(image->dimensions(), Math::Vector2<GLsizei>(2, 3));
}

}}}}
