/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
              Vladimír Vondruš <mosra@centrum.cz>

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
#include <QtCore/QString>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Constants.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

struct ColladaImporterTest: TestSuite::Tester {
    explicit ColladaImporterTest();

    void openWrongNamespace();
    void openWrongVersion();

    void scene();
    void objectNoMaterial();
    void objectMultipleMaterials();

    void mesh();
    void material();
    void texture();
    void image();

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

ColladaImporterTest::ColladaImporterTest() {
    addTests({&ColladaImporterTest::openWrongNamespace,
              &ColladaImporterTest::openWrongVersion,

              &ColladaImporterTest::scene,
              &ColladaImporterTest::objectNoMaterial,
              &ColladaImporterTest::objectMultipleMaterials,

              &ColladaImporterTest::mesh,
              &ColladaImporterTest::material,
              &ColladaImporterTest::texture,
              &ColladaImporterTest::image});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. Reset
       the plugin dir after so it doesn't load anything else from the
       filesystem. */
    #ifdef COLLADAIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(COLLADAIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    _manager.setPluginDirectory({});
    #endif
    /* The StbImageImporter (for PNG image loading) is optional */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void ColladaImporterTest::openWrongNamespace() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("ColladaImporter");
    std::stringstream debug;
    Error redirectError{&debug};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(COLLADAIMPORTER_TEST_DIR, "openWrongNamespace.dae")));
    CORRADE_COMPARE(debug.str(), "Trade::ColladaImporter::openFile(): unsupported namespace \"http://www.collada.org/2006/11/COLLADASchema\"\n");
}

void ColladaImporterTest::openWrongVersion() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("ColladaImporter");
    std::stringstream debug;
    Error redirectError{&debug};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(COLLADAIMPORTER_TEST_DIR, "openWrongVersion.dae")));
    CORRADE_COMPARE(debug.str(), "Trade::ColladaImporter::openFile(): unsupported version \"1.4.0\"\n");
}

void ColladaImporterTest::scene() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("ColladaImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(COLLADAIMPORTER_TEST_DIR, "scene.dae")));

    CORRADE_COMPARE(importer->defaultScene(), 1);
    CORRADE_COMPARE(importer->sceneCount(), 2);
    CORRADE_COMPARE(importer->object3DCount(), 6);

    CORRADE_COMPARE(importer->sceneName(1), "Scene2");
    CORRADE_COMPARE(importer->sceneForName("Scene2"), 1);

    CORRADE_COMPARE(importer->sceneName(0), "Scene");
    CORRADE_COMPARE(importer->sceneForName("Scene"), 0);
    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), (std::vector<UnsignedInt>{0, 2}));

    CORRADE_COMPARE(importer->object3DName(0), "Camera");
    CORRADE_COMPARE(importer->object3DForName("Camera"), 0);
    std::unique_ptr<ObjectData3D> object = importer->object3D(0);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Camera);
    CORRADE_COMPARE(object->instance(), 2);
    CORRADE_COMPARE(object->children(), std::vector<UnsignedInt>{1});

    CORRADE_COMPARE(importer->object3DName(1), "Light");
    CORRADE_COMPARE(importer->object3DForName("Light"), 1);
    object = importer->object3D(1);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Light);
    CORRADE_COMPARE(object->instance(), 1);
    CORRADE_VERIFY(object->children().empty());

    CORRADE_COMPARE(importer->object3DName(2), "Mesh");
    CORRADE_COMPARE(importer->object3DForName("Mesh"), 2);
    object = importer->object3D(2);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(object->instance(), 2);
    Matrix4 transformation =
        Matrix4::translation({1, 2, 3})*
        Matrix4::rotationZ(Deg(60.0f))*
        Matrix4::rotationY(Deg(90.0f))*
        Matrix4::rotationX(Deg(120.0f))*
        Matrix4::scaling({3, 4, 5});
    CORRADE_COMPARE(object->transformation(), transformation);
    CORRADE_COMPARE(static_cast<MeshObjectData3D*>(object.get())->material(), 1);

    std::ostringstream debug;
    Error redirectError{&debug};
    CORRADE_VERIFY(!importer->object3D(3));
    CORRADE_VERIFY(!importer->object3D(4));
    CORRADE_VERIFY(!importer->object3D(5));
    CORRADE_COMPARE(debug.str(), "Trade::ColladaImporter::object3D(): \"instance_wrong\" instance type not supported\n"
                                 "Trade::ColladaImporter::object3D(): mesh \"NonexistentMesh\" was not found\n"
                                 "Trade::ColladaImporter::object3D(): material \"NonexistentMaterial\" was not found\n");
}

void ColladaImporterTest::objectNoMaterial() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("ColladaImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(COLLADAIMPORTER_TEST_DIR, "object-no-material.dae")));
    CORRADE_COMPARE(importer->object3DCount(), 1);

    std::unique_ptr<ObjectData3D> object = importer->object3D(0);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(static_cast<MeshObjectData3D*>(object.get())->material(), -1);
}

void ColladaImporterTest::objectMultipleMaterials() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("ColladaImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(COLLADAIMPORTER_TEST_DIR, "object-multiple-materials.dae")));
    CORRADE_COMPARE(importer->object3DCount(), 1);

    std::ostringstream debug;
    Error redirectError{&debug};
    CORRADE_VERIFY(!importer->object3D(0));
    CORRADE_COMPARE(debug.str(), "Trade::ColladaImporter::object3D(): multiple materials per object are not supported\n");
}

void ColladaImporterTest::mesh() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("ColladaImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(COLLADAIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer->mesh3DCount(), 5);

    std::stringstream debug;
    Error redirectError{&debug};
    CORRADE_COMPARE(importer->mesh3DForName("WrongPrimitives"), 0);
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(debug.str(), "Trade::ColladaImporter::mesh3D(): 5 vertices per face not supported\n");

    /* Vertex only mesh */
    CORRADE_COMPARE(importer->mesh3DName(1), "MeshVertexOnly");
    CORRADE_COMPARE(importer->mesh3DForName("MeshVertexOnly"), 1);
    Containers::Optional<MeshData3D> mesh = importer->mesh3D(1);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{
        0, 1, 2, 0, 2, 3, 4, 0, 3, 4, 3, 5
    }));
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
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
    CORRADE_COMPARE(importer->mesh3DName(2), "MeshQuads");
    CORRADE_COMPARE(importer->mesh3DForName("MeshQuads"), 2);
    mesh = importer->mesh3D(2);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->indices(), (std::vector<unsigned int>{
        0, 1, 2, 0, 2, 3, 4, 0, 3, 4, 3, 5, 0, 1, 2, 0, 2, 3, 4, 0, 3
    }));

    /* Vertex and normal mesh */
    CORRADE_COMPARE(importer->mesh3DName(3), "MeshVertexNormals");
    CORRADE_COMPARE(importer->mesh3DForName("MeshVertexNormals"), 3);
    mesh = importer->mesh3D(3);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7
    }));
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
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
    CORRADE_COMPARE(mesh->normals(0), (std::vector<Vector3>{
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
    CORRADE_COMPARE(importer->mesh3DName(4), "Mesh");
    CORRADE_COMPARE(importer->mesh3DForName("Mesh"), 4);
    mesh = importer->mesh3D(4);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7
    }));
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
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
    CORRADE_COMPARE(mesh->normals(0), (std::vector<Vector3>{
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
    CORRADE_COMPARE(mesh->textureCoords2D(0), (std::vector<Vector2>{
        {0.5, 1},
        {1, 1},
        {1, 0},
        {0.5, 0},
        {0, 1},
        {0.5, 1},
        {0.5, 0},
        {0, 0}
    }));
    CORRADE_COMPARE(mesh->textureCoords2D(1), std::vector<Vector2>(8));
}

void ColladaImporterTest::material() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("ColladaImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(COLLADAIMPORTER_TEST_DIR, "material.dae")));

    CORRADE_COMPARE(importer->materialCount(), 5);

    std::stringstream debug;
    Error redirectError{&debug};
    CORRADE_COMPARE(importer->materialName(0), "MaterialWrongProfile");
    CORRADE_COMPARE(importer->materialForName("MaterialWrongProfile"), 0);
    CORRADE_VERIFY(!importer->material(0));
    CORRADE_COMPARE(debug.str(), "Trade::ColladaImporter::material(): \"profile_GLSL\" effect profile not supported\n");

    debug.str({});
    CORRADE_COMPARE(importer->materialName(1), "MaterialWrongShader");
    CORRADE_COMPARE(importer->materialForName("MaterialWrongShader"), 1);
    CORRADE_VERIFY(!importer->material(1));
    CORRADE_COMPARE(debug.str(), "Trade::ColladaImporter::material(): \"lambert\" shader not supported\n");

    debug.str({});
    CORRADE_COMPARE(importer->materialName(2), "MaterialPhongUnknownTexture");
    CORRADE_COMPARE(importer->materialForName("MaterialPhongUnknownTexture"), 2);
    CORRADE_VERIFY(!importer->material(2));
    CORRADE_COMPARE(debug.str(), "Trade::ColladaImporter::material(): diffuse texture UnknownTexture not found\n");

    /* Color only material */
    {
        CORRADE_COMPARE(importer->materialName(3), "MaterialPhong");
        CORRADE_COMPARE(importer->materialForName("MaterialPhong"), 3);
        const std::unique_ptr<AbstractMaterialData> abstractMaterial = importer->material(3);
        CORRADE_VERIFY(abstractMaterial);
        CORRADE_VERIFY(abstractMaterial->type() == MaterialType::Phong);

        auto material = static_cast<const PhongMaterialData*>(abstractMaterial.get());
        CORRADE_VERIFY(material->flags() == PhongMaterialData::Flags());
        CORRADE_COMPARE(material->ambientColor(), Vector3(1, 0, 0));
        CORRADE_COMPARE(material->diffuseColor(), Vector3(0, 1, 0));
        CORRADE_COMPARE(material->specularColor(), Vector3(0, 0, 1));
        CORRADE_COMPARE(material->shininess(), 50.0f);
    }

    /* Textured material */
    {
        CORRADE_COMPARE(importer->materialName(4), "MaterialPhongTextured");
        CORRADE_COMPARE(importer->materialForName("MaterialPhongTextured"), 4);
        const std::unique_ptr<AbstractMaterialData> abstractMaterial = importer->material(4);
        CORRADE_VERIFY(abstractMaterial);
        CORRADE_VERIFY(abstractMaterial->type() == MaterialType::Phong);

        auto material = static_cast<const PhongMaterialData*>(abstractMaterial.get());
        CORRADE_VERIFY(material->flags() == (PhongMaterialData::Flag::DiffuseTexture|PhongMaterialData::Flag::SpecularTexture));
        CORRADE_COMPARE(material->ambientColor(), Vector3(1, 1, 0));
        CORRADE_COMPARE(material->diffuseTexture(), 0);
        CORRADE_COMPARE(importer->textureName(0), "DiffuseTexture");
        CORRADE_COMPARE(material->specularTexture(), 1);
        CORRADE_COMPARE(importer->textureName(1), "SpecularTexture");
        CORRADE_COMPARE(material->shininess(), 50.0f);
    }
}

void ColladaImporterTest::texture() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("ColladaImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(COLLADAIMPORTER_TEST_DIR, "texture.dae")));

    CORRADE_COMPARE(importer->textureCount(), 4);

    /* Unsupported sampler type */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(importer->textureName(0), "UnsupportedSampler");
    CORRADE_COMPARE(importer->textureForName("UnsupportedSampler"), 0);
    CORRADE_VERIFY(!importer->texture(0));
    CORRADE_COMPARE(out.str(), "Trade::ColladaImporter::texture(): unsupported sampler type samplerRECT\n");

    /* Unknown image reference */
    out.str({});
    CORRADE_COMPARE(importer->textureName(1), "SamplerWithoutImage");
    CORRADE_COMPARE(importer->textureForName("SamplerWithoutImage"), 1);
    CORRADE_VERIFY(!importer->texture(1));
    CORRADE_COMPARE(out.str(), "Trade::ColladaImporter::texture(): image UnknownImage not found\n");

    /* Proper one */
    {
        CORRADE_COMPARE(importer->textureName(2), "Sampler");
        CORRADE_COMPARE(importer->textureForName("Sampler"), 2);
        const Containers::Optional<TextureData> texture = importer->texture(2);
        CORRADE_VERIFY(texture);

        CORRADE_COMPARE(texture->type(), TextureData::Type::Texture2D);
        CORRADE_COMPARE(texture->wrapping(), Array3D<SamplerWrapping>(SamplerWrapping::ClampToEdge, SamplerWrapping::MirroredRepeat, SamplerWrapping::Repeat));
        CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Nearest);
        CORRADE_COMPARE(texture->image(), 1);
    }

    /* Default sampling values */
    {
        CORRADE_COMPARE(importer->textureName(3), "SamplerDefaults");
        CORRADE_COMPARE(importer->textureForName("SamplerDefaults"), 3);
        const Containers::Optional<TextureData> texture = importer->texture(3);
        CORRADE_VERIFY(texture);

        CORRADE_COMPARE(texture->type(), TextureData::Type::Texture2D);
        CORRADE_COMPARE(texture->wrapping(), Array3D<SamplerWrapping>(SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping::Repeat));
        CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Nearest);
        CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Nearest);
        CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Base);
        CORRADE_COMPARE(texture->image(), 0);
    }
}

void ColladaImporterTest::image() {
    if(_manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("ColladaImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(COLLADAIMPORTER_TEST_DIR, "image.dae")));

    CORRADE_COMPARE(importer->image2DCount(), 2);

    std::stringstream debug;
    Error redirectError{&debug};
    CORRADE_COMPARE(importer->image2DName(0), "UnsupportedImage");
    CORRADE_COMPARE(importer->image2DForName("UnsupportedImage"), 0);
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(debug.str(), "Trade::AnyImageImporter::openFile(): cannot determine type of file /image.xcf\n");

    CORRADE_COMPARE(importer->image2DName(1), "Image");
    CORRADE_COMPARE(importer->image2DForName("Image"), 1);
    Containers::Optional<ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);

    /* Check only size, as it is good enough proof that it is working */
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::ColladaImporterTest)
