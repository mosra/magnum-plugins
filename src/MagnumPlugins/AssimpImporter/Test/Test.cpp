/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
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
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/ObjectData3D.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/LightData.h>

#if MAGNUM_TRADE_ASSIMPIMPORTER_DEBUG
#include <assimp/Logger.hpp>
#include <assimp/DefaultLogger.hpp>
#endif
#include <assimp/defs.h> /* in assimp 3.0, version.h is missing this include for ASSIMP_API */
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

using namespace Math::Literals;

#if MAGNUM_TRADE_ASSIMPIMPORTER_DEBUG
/* Stream implementation for outputting assimp log messages to Debug() */
class MagnumDebugStream: public Assimp::LogStream {
public:
    void write(const char* message) override {
        Debug(Debug::Flag::NoNewlineAtTheEnd) << Debug::color(Debug::Color::Yellow) << "assimp:" << message;
    }
};
#endif

struct AssimpImporterTest: TestSuite::Tester {
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

    void openState();
    void openStateTexture();

    void configurePostprocessFlipUVs();

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

namespace {
    enum: std::size_t { LightInstanceCount = 3 };

    constexpr struct {
        Trade::LightData::Type type;
        Color3 color;
    } LightInstanceData[LightInstanceCount]{
        {Trade::LightData::Type::Spot, {0.12f, 0.24f, 0.36f}},
        {Trade::LightData::Type::Point, {0.5f, 0.25f, 0.05f}},
        {Trade::LightData::Type::Infinite, {1.0f, 0.15f, 0.45f}}
    };
}

AssimpImporterTest::AssimpImporterTest() {
    #if MAGNUM_TRADE_ASSIMPIMPORTER_DEBUG
    Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
    Assimp::DefaultLogger::get()->attachStream(new MagnumDebugStream,
            Assimp::Logger::Info|Assimp::Logger::Err|Assimp::Logger::Warn|Assimp::Logger::Debugging);
    #endif

    addTests({&AssimpImporterTest::open,

              &AssimpImporterTest::camera});

    addInstancedTests({&AssimpImporterTest::light}, LightInstanceCount);

    addTests({&AssimpImporterTest::lightUndefined,
              &AssimpImporterTest::material,

              &AssimpImporterTest::mesh,
              &AssimpImporterTest::pointMesh,
              &AssimpImporterTest::lineMesh,

              &AssimpImporterTest::scene,
              &AssimpImporterTest::texture,
              &AssimpImporterTest::embeddedTexture,

              &AssimpImporterTest::openState,
              &AssimpImporterTest::openStateTexture,

              &AssimpImporterTest::configurePostprocessFlipUVs});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. Reset
       the plugin dir after so it doesn't load anything else from the
       filesystem. */
    #ifdef ASSIMPIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(ASSIMPIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    _manager.setPluginDirectory({});
    #endif
    /* The StbImageImporter (for PNG image loading) is optional */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void AssimpImporterTest::open() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    auto data = Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae"));
    CORRADE_VERIFY(importer->openData(data));

    importer->close();
    CORRADE_VERIFY(!importer->isOpened());

    CORRADE_VERIFY(!importer->openFile("i-do-not-exists.foo"));
    CORRADE_VERIFY(!importer->isOpened());
}

void AssimpImporterTest::camera() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "camera.dae")));

    CORRADE_COMPARE(importer->cameraCount(), 1);
    Containers::Optional<Trade::CameraData> camera = importer->camera(0);
    CORRADE_VERIFY(camera);
    CORRADE_COMPARE(camera->fov(), 49.13434_degf);
    CORRADE_COMPARE(camera->near(), 0.123f);
    CORRADE_COMPARE(camera->far(), 123.0f);

    CORRADE_COMPARE(importer->object3DCount(), 2);

    std::unique_ptr<Trade::ObjectData3D> cameraObject = importer->object3D(1);
    CORRADE_COMPARE(cameraObject->instanceType(), ObjectInstanceType3D::Camera);
    CORRADE_COMPARE(cameraObject->instance(), 0);
}

void AssimpImporterTest::light() {
    auto&& data = LightInstanceData[testCaseInstanceId()];

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "light.dae")));

    CORRADE_COMPARE(importer->lightCount(), 3);
    CORRADE_COMPARE(importer->object3DCount(), 4); /* root + 3 light objects */

    Containers::Optional<Trade::LightData> light = importer->light(testCaseInstanceId());
    CORRADE_VERIFY(light);
    CORRADE_COMPARE(light->type(), data.type);
    CORRADE_COMPARE(light->color(), data.color);
    CORRADE_COMPARE(light->intensity(), 1.0f);

    std::unique_ptr<Trade::ObjectData3D> lightObject = importer->object3D(testCaseInstanceId() + 1);
    CORRADE_COMPARE(lightObject->instanceType(), ObjectInstanceType3D::Light);
    CORRADE_COMPARE(lightObject->instance(), testCaseInstanceId());
}

void AssimpImporterTest::lightUndefined() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "light-undefined.dae")));

    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp cannot load lights with undefined light type yet.");

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->light(0));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::light(): light type 4 is not supported\n");
}

void AssimpImporterTest::material() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "mesh-material.dae")));

    CORRADE_COMPARE(importer->materialCount(), 1);
    std::unique_ptr<Trade::AbstractMaterialData> material = importer->material(0);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), MaterialType::Phong);

    Trade::PhongMaterialData* phongMaterial = static_cast<Trade::PhongMaterialData*>(material.get());
    CORRADE_VERIFY(phongMaterial);
    CORRADE_COMPARE(phongMaterial->flags(), Trade::PhongMaterialData::Flags{});
    CORRADE_COMPARE(phongMaterial->ambientColor(), Color3(0.0f, 0.0f, 0.0f));
    CORRADE_COMPARE(phongMaterial->specularColor(), Color3(0.15f, 0.1f, 0.05f));
    CORRADE_COMPARE(phongMaterial->diffuseColor(), Color3(0.08f, 0.16f, 0.24f));
    CORRADE_COMPARE(phongMaterial->shininess(), 50.0f);

    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /* Ancient assimp version add "-material" suffix */
    if(version < 302) {
        CORRADE_COMPARE(importer->materialForName("Material-material"), 0);
        CORRADE_COMPARE(importer->materialName(0), "Material-material");
    } else {
        CORRADE_COMPARE(importer->materialForName("Material"), 0);
        CORRADE_COMPARE(importer->materialName(0), "Material");
    }
    CORRADE_COMPARE(importer->materialForName("Ghost"), -1);
}

void AssimpImporterTest::mesh() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer->mesh3DCount(), 1);

    Containers::Optional<Trade::MeshData3D> mesh = importer->mesh3D(0);
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
    {
        CORRADE_EXPECT_FAIL_IF(version < 302,
            "Assimp < 3.2 loads incorrect alpha value for the last color");
        CORRADE_COMPARE(mesh->colors(0), (std::vector<Color4>{
            {1.0f, 0.25f, 0.24f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.1f, 0.2f, 0.3f, 1.0f}}));
    }
    CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{0, 1, 2}));

    std::unique_ptr<Trade::ObjectData3D> meshObject = importer->object3D(1);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(meshObject->instance(), 0);
}

void AssimpImporterTest::pointMesh() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "points.obj")));

    CORRADE_COMPARE(importer->mesh3DCount(), 1);

    Containers::Optional<Trade::MeshData3D> mesh = importer->mesh3D(0);
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

    std::unique_ptr<Trade::ObjectData3D> meshObject = importer->object3D(1);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(meshObject->instance(), 0);
}

void AssimpImporterTest::lineMesh() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "line.dae")));

    CORRADE_COMPARE(importer->mesh3DCount(), 1);

    Containers::Optional<Trade::MeshData3D> mesh = importer->mesh3D(0);
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

    std::unique_ptr<Trade::ObjectData3D> meshObject = importer->object3D(1);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(meshObject->instance(), 0);
}

void AssimpImporterTest::scene() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae")));

    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->defaultScene(), 0);
    Containers::Optional<Trade::SceneData> data = importer->scene(0);
    CORRADE_VERIFY(data);

    CORRADE_COMPARE(data->children2D(), {});
    CORRADE_COMPARE(data->children3D(), {1});

    std::unique_ptr<Trade::ObjectData3D> explicitRootObject = importer->object3D(1);
    CORRADE_COMPARE(explicitRootObject->children(), {2});
    CORRADE_COMPARE(explicitRootObject->instanceType(), ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(explicitRootObject->transformation(), Matrix4());

    std::unique_ptr<Trade::ObjectData3D> childObject = importer->object3D(2);
    CORRADE_COMPARE(childObject->transformation(), Matrix4(
        {0.813798f, -0.44097f, 0.378522f, 1.0f},
        {0.469846f, 0.882564f, 0.0180283f, 2.0f},
        {-0.34202f, 0.163176f, 0.925417f, 3.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}));

    CORRADE_COMPARE(importer->object3DForName("Root"), 1);
    CORRADE_COMPARE(importer->object3DForName("Child"), 2);
    CORRADE_COMPARE(importer->object3DName(1), "Root");
    CORRADE_COMPARE(importer->object3DName(2), "Child");

    CORRADE_COMPARE(importer->object3DForName("Ghost"), -1);
}

void AssimpImporterTest::texture() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "texture.dae")));

    CORRADE_COMPARE(importer->textureCount(), 1);
    Containers::Optional<Trade::TextureData> texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->type(), Trade::TextureData::Type::Texture2D);
    CORRADE_COMPARE(texture->wrapping(),
        Array3D<SamplerWrapping>(SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge));
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);

    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::embeddedTexture() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "embedded-texture.blend")));

    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp cannot load embedded textures from blender files.");

    CORRADE_COMPARE(importer->textureCount(), 1);
    Containers::Optional<Trade::TextureData> texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->type(), Trade::TextureData::Type::Texture2D);
    CORRADE_COMPARE(texture->wrapping(),
        Array3D<SamplerWrapping>(SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge));
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);

    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::openState() {
    Assimp::Importer _importer;
    const aiScene* sc = _importer.ReadFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae"), aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_JoinIdenticalVertices);
    CORRADE_VERIFY(sc != nullptr);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->openState(sc);
    CORRADE_VERIFY(importer->isOpened());

    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->defaultScene(), 0);
    Containers::Optional<Trade::SceneData> data = importer->scene(0);
    CORRADE_VERIFY(data);

    CORRADE_COMPARE(data->children2D(), {});
    CORRADE_COMPARE(data->children3D(), {1});

    std::unique_ptr<Trade::ObjectData3D> explicitRootObject = importer->object3D(1);
    CORRADE_COMPARE(explicitRootObject->children(), {2});
    CORRADE_COMPARE(explicitRootObject->instanceType(), ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(explicitRootObject->transformation(), Matrix4());

    std::unique_ptr<Trade::ObjectData3D> childObject = importer->object3D(2);
    CORRADE_COMPARE(childObject->transformation(), Matrix4(
        {0.813798f, -0.44097f, 0.378522f, 1.0f},
        {0.469846f, 0.882564f, 0.0180283f, 2.0f},
        {-0.34202f, 0.163176f, 0.925417f, 3.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}));

    CORRADE_COMPARE(importer->object3DForName("Root"), 1);
    CORRADE_COMPARE(importer->object3DForName("Child"), 2);
    CORRADE_COMPARE(importer->object3DName(1), "Root");
    CORRADE_COMPARE(importer->object3DName(2), "Child");

    CORRADE_COMPARE(importer->object3DForName("Ghost"), -1);
}

void AssimpImporterTest::openStateTexture() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Assimp::Importer _importer;
    const aiScene* sc = _importer.ReadFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "texture.dae"), aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_JoinIdenticalVertices);
    CORRADE_VERIFY(sc != nullptr);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->openState(sc, ASSIMPIMPORTER_TEST_DIR);
    CORRADE_VERIFY(importer->isOpened());

    CORRADE_COMPARE(importer->textureCount(), 1);
    Containers::Optional<Trade::TextureData> texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->type(), Trade::TextureData::Type::Texture2D);
    CORRADE_COMPARE(texture->wrapping(),
        Array3D<SamplerWrapping>(SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge));
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);

    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::configurePostprocessFlipUVs() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().group("postprocess")->setValue("FlipUVs", true);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer->mesh3DCount(), 1);

    Containers::Optional<Trade::MeshData3D> mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 1);

    /* The same as in mesh() but with reversed Y */
    CORRADE_COMPARE(mesh->textureCoords2D(0), (std::vector<Vector2>{
        {0.5f, 0.0f}, {0.75f, 0.5f}, {0.5f, 0.1f}}));
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::AssimpImporterTest)
