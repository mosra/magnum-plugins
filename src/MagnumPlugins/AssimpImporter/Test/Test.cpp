/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2017 Jonathan Hale <squareys@googlemail.com>

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

#define MAGNUM_ASSIMPIMPORTER_DEBUG 0

#include <sstream>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/ObjectData3D.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/LightData.h>

#include <MagnumPlugins/AssimpImporter/AssimpImporter.h>

#if MAGNUM_TRADE_ASSIMPIMPORTER_DEBUG
#include <assimp/Logger.hpp>
#include <assimp/DefaultLogger.hpp>
#endif
#include <assimp/defs.h> /* in assimp 3.0, version.h is missing this include for ASSIMP_API */
#include <assimp/version.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

using namespace Magnum::Math::Literals;

#if MAGNUM_TRADE_ASSIMPIMPORTER_DEBUG
/* Stream implementation for outputting assimp log messages to Debug() */
class MagnumDebugStream: public Assimp::LogStream {
public:
    void write(const char* message) override {
        Debug(Debug::Flag::NoNewlineAtTheEnd) << Debug::color(Debug::Color::Yellow) << "assimp:" << message;
    }
};
#endif

struct AssimpImporterTest: public TestSuite::Tester {
    explicit AssimpImporterTest();

    void open();

    void camera();
    void light();
    void lightUndefined();
    void material();

    void mesh();
    void pointMesh();
    void lineMesh();

    void scene();
    void texture();
    void embeddedTexture();
};

AssimpImporterTest::AssimpImporterTest() {
    #if MAGNUM_TRADE_ASSIMPIMPORTER_DEBUG
    Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
    Assimp::DefaultLogger::get()->attachStream(new MagnumDebugStream,
            Assimp::Logger::Info|Assimp::Logger::Err|Assimp::Logger::Warn|Assimp::Logger::Debugging);
    #endif

    addTests({&AssimpImporterTest::open,

              &AssimpImporterTest::camera,
              &AssimpImporterTest::light,
              &AssimpImporterTest::lightUndefined,
              &AssimpImporterTest::material,

              &AssimpImporterTest::mesh,
              &AssimpImporterTest::pointMesh,
              &AssimpImporterTest::lineMesh,

              &AssimpImporterTest::scene,
              &AssimpImporterTest::texture,
              &AssimpImporterTest::embeddedTexture});
}

void AssimpImporterTest::open() {
    AssimpImporter importer;

    auto data = Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae"));
    CORRADE_VERIFY(importer.openData(data));

    importer.close();
    CORRADE_VERIFY(!importer.isOpened());

    CORRADE_VERIFY(!importer.openFile("i-do-not-exists.foo"));
    CORRADE_VERIFY(!importer.isOpened());
}

void AssimpImporterTest::camera() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "camera.dae")));

    CORRADE_COMPARE(importer.cameraCount(), 1);
    std::optional<Trade::CameraData> camera = importer.camera(0);
    CORRADE_VERIFY(camera);
    CORRADE_COMPARE(camera->fov(), 49.13434_degf);
    CORRADE_COMPARE(camera->near(), 0.123f);
    CORRADE_COMPARE(camera->far(), 123.0f);

    CORRADE_COMPARE(importer.object3DCount(), 2);

    std::unique_ptr<Trade::ObjectData3D> cameraObject = importer.object3D(1);
    CORRADE_COMPARE(cameraObject->instanceType(), ObjectInstanceType3D::Camera);
    CORRADE_COMPARE(cameraObject->instance(), 0);
}

void AssimpImporterTest::light() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "light.dae")));

    CORRADE_COMPARE(importer.lightCount(), 3);
    CORRADE_COMPARE(importer.object3DCount(), 4); /* root + 3 light objects */

    constexpr Trade::LightData::Type types[3]{
        Trade::LightData::Type::Spot,
        Trade::LightData::Type::Point,
        Trade::LightData::Type::Infinite};
    constexpr Color3 colors[3]{
        {0.12f, 0.24f, 0.36f},
        {0.5f, 0.25f, 0.05f},
        {1.0f, 0.15f, 0.45f}};

    for (int i : {0, 1, 2}) {
        std::optional<Trade::LightData> light = importer.light(i);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), types[i]);
        CORRADE_COMPARE(light->color(), colors[i]);
        CORRADE_COMPARE(light->intensity(), 1.0f);

        std::unique_ptr<Trade::ObjectData3D> lightObject = importer.object3D(i + 1);
        CORRADE_COMPARE(lightObject->instanceType(), ObjectInstanceType3D::Light);
        CORRADE_COMPARE(lightObject->instance(), i);
    }
}

void AssimpImporterTest::lightUndefined() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "light-undefined.dae")));

    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /* @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 303)
        CORRADE_SKIP("Current version of assimp cannot load lights with undefined light type yet.");

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer.light(0));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::light(): light type 4 is not supported\n");
}

void AssimpImporterTest::material() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "mesh-material.dae")));

    CORRADE_COMPARE(importer.materialCount(), 1);
    std::unique_ptr<Trade::AbstractMaterialData> material = importer.material(0);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), MaterialType::Phong);

    Trade::PhongMaterialData* phongMaterial = static_cast<Trade::PhongMaterialData*>(material.get());
    CORRADE_VERIFY(phongMaterial);
    CORRADE_VERIFY(phongMaterial->flags() == Trade::PhongMaterialData::Flags{}); /* @todo No debug << operator for the PhongMaterialData::Flag enum */
    CORRADE_COMPARE(phongMaterial->ambientColor(), Color3(0.0f, 0.0f, 0.0f));
    CORRADE_COMPARE(phongMaterial->specularColor(), Color3(0.15f, 0.1f, 0.05f));
    CORRADE_COMPARE(phongMaterial->diffuseColor(), Color3(0.08f, 0.16f, 0.24f));
    CORRADE_COMPARE(phongMaterial->shininess(), 50.0f);

    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /* Ancient assimp version add "-material" suffix */
    if(version < 303) {
        CORRADE_COMPARE(importer.materialForName("Material-material"), 0);
        CORRADE_COMPARE(importer.materialName(0), "Material-material");
    } else {
        CORRADE_COMPARE(importer.materialForName("Material"), 0);
        CORRADE_COMPARE(importer.materialName(0), "Material");
    }
    CORRADE_COMPARE(importer.materialForName("Ghost"), -1);
}

void AssimpImporterTest::mesh() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer.mesh3DCount(), 1);

    std::optional<Trade::MeshData3D> mesh = importer.mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->normalArrayCount(), 1);
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 1);
    CORRADE_COMPARE(mesh->colorArrayCount(), 1);

    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
            {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}}));
    CORRADE_COMPARE(mesh->normals(0), (std::vector<Vector3>{
            {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}));
    CORRADE_COMPARE(mesh->textureCoords2D(0), (std::vector<Vector2>{
            {0.5f, 1.0f}, {0.75f, 0.5f}, {0.5f, 0.9f}}));
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /* Skip for assimp < 3.3, which loads some incorrect alpha value for the last color */
    if(version >= 303) {
        CORRADE_COMPARE(mesh->colors(0), (std::vector<Color4>{
            {1.0f, 0.25f, 0.24f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.1f, 0.2f, 0.3f, 1.0f}}));
    }
    CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{0, 1, 2}));

    std::unique_ptr<Trade::ObjectData3D> meshObject = importer.object3D(1);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(meshObject->instance(), 0);
}

void AssimpImporterTest::pointMesh() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "points.obj")));

    CORRADE_COMPARE(importer.mesh3DCount(), 1);

    std::optional<Trade::MeshData3D> mesh = importer.mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Points);
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->normalArrayCount(), 0);
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 0);
    CORRADE_COMPARE(mesh->colorArrayCount(), 0);

    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
            {0.5f, 2.0f, 3.0f}, {2.0f, 3.0f, 5.0f}, {0.0f, 1.5f, 1.0f}}));
    CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{0, 1, 2, 0}));

    std::unique_ptr<Trade::ObjectData3D> meshObject = importer.object3D(1);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(meshObject->instance(), 0);
}

void AssimpImporterTest::lineMesh() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "line.dae")));

    CORRADE_COMPARE(importer.mesh3DCount(), 1);

    std::optional<Trade::MeshData3D> mesh = importer.mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Lines);
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->normalArrayCount(), 0);
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 0);
    CORRADE_COMPARE(mesh->colorArrayCount(), 0);

    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
            {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}}));
    CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{0, 1}));

    std::unique_ptr<Trade::ObjectData3D> meshObject = importer.object3D(1);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(meshObject->instance(), 0);
}

void AssimpImporterTest::scene() {
    AssimpImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae")));

    CORRADE_COMPARE(importer.sceneCount(), 1);
    CORRADE_COMPARE(importer.defaultScene(), 0);
    std::optional<Trade::SceneData> data = importer.scene(0);
    CORRADE_VERIFY(data);

    CORRADE_COMPARE(data->children2D(), {});
    CORRADE_COMPARE(data->children3D(), {1});

    std::unique_ptr<Trade::ObjectData3D> explicitRootObject = importer.object3D(1);
    CORRADE_COMPARE(explicitRootObject->children(), {2});
    CORRADE_COMPARE(explicitRootObject->instanceType(), ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(explicitRootObject->transformation(), Matrix4());

    std::unique_ptr<Trade::ObjectData3D> childObject = importer.object3D(2);
    CORRADE_COMPARE(childObject->transformation(),
                    Matrix4({0.813798f, -0.44097f, 0.378522f, 1.0f},
                            {0.469846f, 0.882564f, 0.0180283f, 2.0f},
                            {-0.34202f, 0.163176f, 0.925417f, 3.0f},
                            {0.0f, 0.0f, 0.0f, 1.0f}));

    CORRADE_COMPARE(importer.object3DForName("Root"), 1);
    CORRADE_COMPARE(importer.object3DForName("Child"), 2);
    CORRADE_COMPARE(importer.object3DName(1), "Root");
    CORRADE_COMPARE(importer.object3DName(2), "Child");

    CORRADE_COMPARE(importer.object3DForName("Ghost"), -1);
}

void AssimpImporterTest::texture() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /* @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 303)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");


    PluginManager::Manager<AbstractImporter> manager{MAGNUM_PLUGINS_IMPORTER_DIR};

    if(manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    AssimpImporter importer{manager};
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "texture.dae")));

    CORRADE_COMPARE(importer.textureCount(), 1);
    std::optional<Trade::TextureData> texture = importer.texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->type(), Trade::TextureData::Type::Texture2D);
    CORRADE_COMPARE(texture->wrapping(),
        Array3D<Sampler::Wrapping>(Sampler::Wrapping::ClampToEdge, Sampler::Wrapping::ClampToEdge, Sampler::Wrapping::ClampToEdge));
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->minificationFilter(), Sampler::Filter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), Sampler::Filter::Linear);

    CORRADE_COMPARE(importer.image2DCount(), 1);
    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    const char pixels[] = {
        '\xb3', '\x69', '\x00', '\xff'
    };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::embeddedTexture() {
    PluginManager::Manager<AbstractImporter> manager{MAGNUM_PLUGINS_IMPORTER_DIR};

    if(manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    AssimpImporter importer{manager};
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "embedded-texture.blend")));

    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /* @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 303)
        CORRADE_SKIP("Current version of assimp cannot load embedded textures from blender files.");

    CORRADE_COMPARE(importer.textureCount(), 1);
    std::optional<Trade::TextureData> texture = importer.texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->type(), Trade::TextureData::Type::Texture2D);
    CORRADE_COMPARE(texture->wrapping(),
        Array3D<Sampler::Wrapping>(Sampler::Wrapping::ClampToEdge, Sampler::Wrapping::ClampToEdge, Sampler::Wrapping::ClampToEdge));
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->minificationFilter(), Sampler::Filter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), Sampler::Filter::Linear);

    CORRADE_COMPARE(importer.image2DCount(), 1);
    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    const char pixels[] = {
        '\xb3', '\x69', '\x00', '\xff'
    };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::AssimpImporterTest)
