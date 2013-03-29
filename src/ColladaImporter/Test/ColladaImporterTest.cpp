/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013 Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <sstream>
#include <TestSuite/Tester.h>
#include <Utility/Directory.h>
#include <Math/Constants.h>
#include <Trade/PhongMaterialData.h>
#include <Trade/ImageData.h>
#include <Trade/MeshData3D.h>
#include <Trade/MeshObjectData3D.h>
#include <Trade/SceneData.h>

#include "ColladaImporter/ColladaImporter.h"
#include "ColladaImporterTestConfigure.h"

using Corrade::Utility::Directory;

namespace Magnum { namespace Trade { namespace ColladaImporter { namespace Test {

class ColladaImporterTest: public Corrade::TestSuite::Tester {
    public:
        ColladaImporterTest();

        void openWrongNamespace();
        void openWrongVersion();
        void parseSource();

        void scene();
        void mesh();
        void material();
        void image();
};

ColladaImporterTest::ColladaImporterTest() {
    addTests({&ColladaImporterTest::openWrongNamespace,
              &ColladaImporterTest::openWrongVersion,
              &ColladaImporterTest::parseSource,
              &ColladaImporterTest::scene,
              &ColladaImporterTest::mesh,
              &ColladaImporterTest::material,
              &ColladaImporterTest::image});
}

void ColladaImporterTest::openWrongNamespace() {
    ColladaImporter importer;
    std::stringstream debug;
    Error::setOutput(&debug);
    CORRADE_VERIFY(!importer.openFile(Directory::join(COLLADAIMPORTER_TEST_DIR, "openWrongNamespace.dae")));
    CORRADE_COMPARE(debug.str(), "ColladaImporter: unsupported namespace \"http://www.collada.org/2006/11/COLLADASchema\"\n");
}

void ColladaImporterTest::openWrongVersion() {
    ColladaImporter importer;
    std::stringstream debug;
    Error::setOutput(&debug);
    CORRADE_VERIFY(!importer.openFile(Directory::join(COLLADAIMPORTER_TEST_DIR, "openWrongVersion.dae")));
    CORRADE_COMPARE(debug.str(), "ColladaImporter: unsupported version \"1.4.0\"\n");
}

void ColladaImporterTest::parseSource() {
    ColladaImporter importer;
    CORRADE_VERIFY(importer.openFile(Directory::join(COLLADAIMPORTER_TEST_DIR, "parseSource.dae")));

    std::stringstream debug;
    Error::setOutput(&debug);
    CORRADE_VERIFY(importer.parseSource<Vector3>("WrongTotalCount").empty());
    CORRADE_COMPARE(debug.str(), "ColladaImporter: wrong total count in source \"WrongTotalCount\"\n");

    {
        CORRADE_EXPECT_FAIL("Swapped coordinates in source are not implemented.");
        CORRADE_COMPARE(importer.parseSource<Vector3>("SwappedCoords"), (std::vector<Vector3>{Vector3(0, 1, 2)}));
    }

    CORRADE_COMPARE(importer.parseSource<Vector3>("MoreElements"), (std::vector<Vector3>{
        {0, 1, 2},
        {3, 4, 5}
    }));
}

void ColladaImporterTest::scene() {
    std::ostringstream debug;
    Error::setOutput(&debug);

    ColladaImporter importer;
    CORRADE_VERIFY(importer.openFile(Directory::join(COLLADAIMPORTER_TEST_DIR, "scene.dae")));

    CORRADE_COMPARE(importer.defaultScene(), 1);
    CORRADE_COMPARE(importer.sceneCount(), 2);
    CORRADE_COMPARE(importer.object3DCount(), 6);

    CORRADE_COMPARE(importer.sceneName(0), "Scene");
    SceneData* scene = importer.scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), (std::vector<UnsignedInt>{0, 2}));

    CORRADE_COMPARE(importer.object3DName(0), "Camera");
    CORRADE_COMPARE(importer.object3DForName("Camera"), 0);
    ObjectData3D* object = importer.object3D(0);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->instanceType(), ObjectData3D::InstanceType::Camera);
    CORRADE_COMPARE(object->instanceId(), 2);
    CORRADE_COMPARE(object->children(), std::vector<UnsignedInt>{1});

    CORRADE_COMPARE(importer.object3DName(1), "Light");
    CORRADE_COMPARE(importer.object3DForName("Light"), 1);
    object = importer.object3D(1);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->instanceType(), ObjectData3D::InstanceType::Light);
    CORRADE_COMPARE(object->instanceId(), 1);
    CORRADE_VERIFY(object->children().empty());

    CORRADE_COMPARE(importer.object3DName(2), "Mesh");
    CORRADE_COMPARE(importer.object3DForName("Mesh"), 2);
    object = importer.object3D(2);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->instanceType(), ObjectData3D::InstanceType::Mesh);
    CORRADE_COMPARE(object->instanceId(), 2);
    Matrix4 transformation =
        Matrix4::translation({1, 2, 3})*
        Matrix4::rotationZ(Deg(60.0f))*
        Matrix4::rotationY(Deg(90.0f))*
        Matrix4::rotationX(Deg(120.0f))*
        Matrix4::scaling({3, 4, 5});
    CORRADE_COMPARE(object->transformation(), transformation);
    CORRADE_COMPARE(static_cast<MeshObjectData3D*>(object)->material(), 1);

    CORRADE_VERIFY(!importer.object3D(3));
    CORRADE_VERIFY(!importer.object3D(4));
    CORRADE_VERIFY(!importer.object3D(5));
    CORRADE_COMPARE(debug.str(), "ColladaImporter: \"instance_wrong\" instance type not supported\n"
                                 "ColladaImporter: mesh \"InexistentMesh\" was not found\n"
                                 "ColladaImporter: material \"InexistentMaterial\" was not found\n");
}

void ColladaImporterTest::mesh() {
    ColladaImporter importer;
    CORRADE_VERIFY(importer.openFile(Directory::join(COLLADAIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer.mesh3DCount(), 5);

    std::stringstream debug;
    Error::setOutput(&debug);
    CORRADE_COMPARE(importer.mesh3DForName("WrongPrimitives"), 0);
    CORRADE_VERIFY(!importer.mesh3D(0));
    CORRADE_COMPARE(debug.str(), "ColladaImporter: 5 vertices per face not supported\n");

    /* Vertex only mesh */
    CORRADE_COMPARE(importer.mesh3DName(1), "MeshVertexOnly");
    CORRADE_COMPARE(importer.mesh3DForName("MeshVertexOnly"), 1);
    MeshData3D* mesh = importer.mesh3D(1);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), Mesh::Primitive::Triangles);
    CORRADE_COMPARE(*mesh->indices(), (std::vector<UnsignedInt>{
        0, 1, 2, 0, 2, 3, 4, 0, 3, 4, 3, 5
    }));
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(*mesh->positions(0), (std::vector<Vector3>{
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
    CORRADE_COMPARE(importer.mesh3DName(2), "MeshQuads");
    CORRADE_COMPARE(importer.mesh3DForName("MeshQuads"), 2);
    mesh = importer.mesh3D(2);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(*mesh->indices(), (std::vector<unsigned int>{
        0, 1, 2, 0, 2, 3, 4, 0, 3, 4, 3, 5, 0, 1, 2, 0, 2, 3, 4, 0, 3
    }));

    /* Vertex and normal mesh */
    CORRADE_COMPARE(importer.mesh3DName(3), "MeshVertexNormals");
    CORRADE_COMPARE(importer.mesh3DForName("MeshVertexNormals"), 3);
    mesh = importer.mesh3D(3);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), Mesh::Primitive::Triangles);
    CORRADE_COMPARE(*mesh->indices(), (std::vector<UnsignedInt>{
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7
    }));
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(*mesh->positions(0), (std::vector<Vector3>{
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
    CORRADE_COMPARE(*mesh->normals(0), (std::vector<Vector3>{
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
    CORRADE_COMPARE(importer.mesh3DName(4), "Mesh");
    CORRADE_COMPARE(importer.mesh3DForName("Mesh"), 4);
    mesh = importer.mesh3D(4);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), Mesh::Primitive::Triangles);
    CORRADE_COMPARE(*mesh->indices(), (std::vector<UnsignedInt>{
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7
    }));
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(*mesh->positions(0), (std::vector<Vector3>{
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
    CORRADE_COMPARE(*mesh->normals(0), (std::vector<Vector3>{
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
    CORRADE_COMPARE(*mesh->textureCoords2D(0), (std::vector<Vector2>{
        {0.5, 1},
        {1, 1},
        {1, 0},
        {0.5, 0},
        {0, 1},
        {0.5, 1},
        {0.5, 0},
        {0, 0}
    }));
    CORRADE_COMPARE(*mesh->textureCoords2D(1), std::vector<Vector2>(8));
}

void ColladaImporterTest::material() {
    ColladaImporter importer;
    CORRADE_VERIFY(importer.openFile(Directory::join(COLLADAIMPORTER_TEST_DIR, "material.dae")));

    CORRADE_COMPARE(importer.materialCount(), 3);

    std::stringstream debug;
    Error::setOutput(&debug);
    CORRADE_COMPARE(importer.materialForName("MaterialWrongProfile"), 0);
    CORRADE_VERIFY(!importer.material(0));
    CORRADE_COMPARE(debug.str(), "ColladaImporter: \"profile_GLSL\" effect profile not supported\n");

    debug.str("");
    CORRADE_COMPARE(importer.materialForName("MaterialWrongShader"), 1);
    CORRADE_VERIFY(!importer.material(1));
    CORRADE_COMPARE(debug.str(), "ColladaImporter: \"lambert\" shader not supported\n");

    CORRADE_COMPARE(importer.materialName(2), "MaterialPhong");
    CORRADE_COMPARE(importer.materialForName("MaterialPhong"), 2);
    PhongMaterialData* material = static_cast<PhongMaterialData*>(importer.material(2));
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->ambientColor(), Vector3(1, 0, 0));
    CORRADE_COMPARE(material->diffuseColor(), Vector3(0, 1, 0));
    CORRADE_COMPARE(material->specularColor(), Vector3(0, 0, 1));
    CORRADE_COMPARE(material->shininess(), 50.0f);
}

void ColladaImporterTest::image() {
    ColladaImporter importer;
    CORRADE_VERIFY(importer.openFile(Directory::join(COLLADAIMPORTER_TEST_DIR, "image.dae")));

    CORRADE_COMPARE(importer.image2DCount(), 2);

    std::stringstream debug;
    Error::setOutput(&debug);
    CORRADE_VERIFY(!importer.image2D(0));
    CORRADE_COMPARE(importer.image2DForName("UnsupportedImage"), 0);
    CORRADE_COMPARE(debug.str(), "ColladaImporter: \"image.jpg\" has unsupported format\n");

    CORRADE_COMPARE(importer.image2DName(1), "Image");
    CORRADE_COMPARE(importer.image2DForName("Image"), 1);
    ImageData2D* image = importer.image2D(1);
    CORRADE_VERIFY(image);

    /* Check only size, as it is good enough proof that it is working */
    CORRADE_COMPARE(image->size(), Math::Vector2<GLsizei>(2, 3));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::ColladaImporter::Test::ColladaImporterTest)
