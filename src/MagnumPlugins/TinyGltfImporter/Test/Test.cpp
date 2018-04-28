/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2018 Tobias Stein <stein.tobi@t-online.de>

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
#include <Corrade/Containers/Array.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Mesh.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/AbstractMaterialData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/ObjectData3D.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

struct TinyGltfImporterTest: TestSuite::Tester {
    explicit TinyGltfImporterTest();

    void open();
    void openError();

    void defaultScene();

    void camera();

    void light();

    void object();
    void objectTransformation();
    void objectTranslation();
    void objectRotation();
    void objectScaling();

    void mesh();

    void material();

    void image();

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

namespace {

enum: std::size_t { InstanceDataCount = 2 };

constexpr struct {
    const char* name;
    const char* extension;
    Containers::ArrayView<const char> shortData;
    const char* shortDataError;
} InstanceData[InstanceDataCount]{
    {"ascii", ".gltf", {"?", 1}, "JSON string too short.\n\n"},
    {"binary", ".glb", {"glTF?", 5}, "Too short data size for glTF Binary.\n"}
};

}

using namespace Magnum::Math::Literals;

TinyGltfImporterTest::TinyGltfImporterTest() {
    addInstancedTests({&TinyGltfImporterTest::open,
                       &TinyGltfImporterTest::openError,

                       &TinyGltfImporterTest::defaultScene,

                       &TinyGltfImporterTest::camera,

                       &TinyGltfImporterTest::light,

                       &TinyGltfImporterTest::object,
                       &TinyGltfImporterTest::objectTransformation,
                       &TinyGltfImporterTest::objectTranslation,
                       &TinyGltfImporterTest::objectRotation,
                       &TinyGltfImporterTest::objectScaling,

                       &TinyGltfImporterTest::mesh,

                       &TinyGltfImporterTest::material,

                       &TinyGltfImporterTest::image}, InstanceDataCount);

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. Besides the explicit StbImageImporter it also pulls in
       the AnyImageImporter dependency. Reset the plugin dir after so it
       doesn't load anything else from the filesystem. */
    #if defined(STBIMAGEIMPORTER_PLUGIN_FILENAME) && defined(TINYGLTFIMPORTER_PLUGIN_FILENAME)
    CORRADE_INTERNAL_ASSERT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    CORRADE_INTERNAL_ASSERT(_manager.load(TINYGLTFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    _manager.setPluginDirectory({});
    #endif
}

void TinyGltfImporterTest::open() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    auto filename = Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "test-scene" + std::string{data.extension});
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_VERIFY(importer->openData(Utility::Directory::read(filename)));
}

void TinyGltfImporterTest::openError() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::ostringstream out;
    Error redirectError{&out};

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(!importer->openData(data.shortData));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openFile(): error opening file: " + std::string{data.shortDataError});
}

void TinyGltfImporterTest::defaultScene() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh" + std::string{data.extension})));

    Int id = importer->defaultScene();
    CORRADE_VERIFY(id > -1);

    UnsignedInt count = importer->sceneCount();
    CORRADE_COMPARE(count, 1);

    auto scene = importer->scene(id);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), (std::vector<UnsignedInt>{1, 0}));
}

void TinyGltfImporterTest::camera() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "camera" + std::string{data.extension})));

    CORRADE_COMPARE(importer->cameraCount(), 2);

    /* orthographic */
    auto cam1 = importer->camera(0);
    CORRADE_VERIFY(cam1);
    CORRADE_COMPARE(cam1->near(), 0.1f);
    CORRADE_COMPARE(cam1->far(), 100.0f);

    /* perspective */
    auto cam2 = importer->camera(1);
    CORRADE_VERIFY(cam2);
    CORRADE_COMPARE(cam2->fov(), 0.5033799372418416_radf);
    CORRADE_COMPARE(cam2->near(), 2.0f);
    CORRADE_COMPARE(cam2->far(), 94.7f);
}

void TinyGltfImporterTest::light() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "light" + std::string{data.extension})));

    CORRADE_COMPARE(importer->lightCount(), 4); /* 3 + 1 (ambient light) */

    /* DirectionalLight */
    auto light1 = importer->light(0);
    CORRADE_VERIFY(light1);
    CORRADE_COMPARE(light1->type(), LightData::Type::Point);
    CORRADE_COMPARE(light1->color(), (Color3{0.062826968729496f, 0.8879325985908508f, 1.0f}));
    CORRADE_COMPARE(light1->intensity(), 1.0f);

    /* SpotLight */
    auto light2 = importer->light(1);
    CORRADE_VERIFY(light2);
    CORRADE_COMPARE(light2->type(), LightData::Type::Spot);
    CORRADE_COMPARE(light2->color(), (Color3{0.28446972370147705f, 0.19345591962337494f, 1.0f}));
    CORRADE_COMPARE(light2->intensity(), 1.0f);

    /* DirectionalLight */
    auto light3 = importer->light(2);
    CORRADE_VERIFY(light3);
    CORRADE_COMPARE(light3->type(), LightData::Type::Infinite);
    CORRADE_COMPARE(light3->color(), (Color3{1.0f, 0.08723420649766922f, 0.14454050362110138f}));
    CORRADE_COMPARE(light3->intensity(), 1.0f);
}

void TinyGltfImporterTest::object() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "object-new" + std::string{data.extension})));

    CORRADE_COMPARE(importer->object3DCount(), 5);

    auto scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), (std::vector<UnsignedInt>{2, 4, 1}));

    CORRADE_COMPARE(importer->object3DName(0), "Correction_Camera");
    CORRADE_COMPARE(importer->object3DForName("Correction_Camera"), 0);

    CORRADE_COMPARE(importer->object3DName(1), "Camera");
    CORRADE_COMPARE(importer->object3DForName("Camera"), 1);

    auto cameraObject = importer->object3D(0);
    CORRADE_COMPARE(cameraObject->instanceType(), Trade::ObjectInstanceType3D::Camera);
    CORRADE_VERIFY(cameraObject->children().empty());

    auto emptyObject = importer->object3D(1);
    CORRADE_COMPARE(emptyObject->instanceType(), Trade::ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(emptyObject->children(), (std::vector<UnsignedInt>{0}));

    auto meshObject = importer->object3D(2);
    CORRADE_COMPARE(meshObject->instanceType(), Trade::ObjectInstanceType3D::Mesh);
    CORRADE_VERIFY(meshObject->children().empty());

    auto lightObject = importer->object3D(3);
    CORRADE_COMPARE(lightObject->instanceType(), Trade::ObjectInstanceType3D::Light);
    CORRADE_VERIFY(lightObject->children().empty());

    auto emptyObject2 = importer->object3D(4);
    CORRADE_COMPARE(emptyObject2->instanceType(), Trade::ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(emptyObject2->children(), (std::vector<UnsignedInt>{3, 3}));
}

void TinyGltfImporterTest::objectTransformation() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "object-transformation" + std::string{data.extension})));

    CORRADE_COMPARE(importer->object3DCount(), 1);

    std::unique_ptr<Trade::ObjectData3D> object = importer->object3D(0);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->transformation(), (Matrix4{
        {0.0f,  1.0f, 0.0f, 0.0f},
        {-0.707107f, 0.0f, -0.707107f, 0.0f},
        {-0.707107f,  0.0f, 0.707107f, 0},
        {2.82843f, 1.0f, 0.0f, 1.0f}
    }));
}

void TinyGltfImporterTest::objectTranslation() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "object-translation" + std::string{data.extension})));

    CORRADE_COMPARE(importer->object3DCount(), 3);

    std::unique_ptr<Trade::ObjectData3D> object1 = importer->object3D(0);
    CORRADE_VERIFY(object1);
    CORRADE_COMPARE(object1->transformation(), Matrix4::translation(Vector3::yAxis(-2)));

    std::unique_ptr<Trade::ObjectData3D> object2 = importer->object3D(1);
    CORRADE_VERIFY(object2);
    CORRADE_COMPARE(object2->transformation(), Matrix4::translation(Vector3::zAxis(3)));

    std::unique_ptr<Trade::ObjectData3D> object3 = importer->object3D(2);
    CORRADE_VERIFY(object3);
    CORRADE_COMPARE(object3->transformation(), Matrix4::translation(Vector3::xAxis(4)));
}

void TinyGltfImporterTest::objectRotation() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "object-rotation" + std::string{data.extension})));

    CORRADE_COMPARE(importer->object3DCount(), 3);

    std::unique_ptr<Trade::ObjectData3D> object1 = importer->object3D(0);
    CORRADE_VERIFY(object1);
    CORRADE_COMPARE(object1->transformation(), Matrix4::rotation(45.0_degf, Vector3::zAxis()));

    std::unique_ptr<Trade::ObjectData3D> object2 = importer->object3D(1);
    CORRADE_VERIFY(object2);
    CORRADE_COMPARE(object2->transformation(), Matrix4::from(Quaternion::rotation(85.0_degf, Vector3::yAxis()).toMatrix(), {}));

    std::unique_ptr<Trade::ObjectData3D> object3 = importer->object3D(2);
    CORRADE_VERIFY(object3);
    CORRADE_COMPARE(object3->transformation(), Matrix4::rotation(-35.0_degf, Vector3::xAxis()));
}

void TinyGltfImporterTest::objectScaling() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "object-scaling" + std::string{data.extension})));

    CORRADE_COMPARE(importer->object3DCount(), 3);

    std::unique_ptr<Trade::ObjectData3D> object1 = importer->object3D(0);
    CORRADE_VERIFY(object1);
    CORRADE_COMPARE(object1->transformation(), Matrix4::scaling({1.5f, 0.5f, 0.75f}));

    std::unique_ptr<Trade::ObjectData3D> object2 = importer->object3D(1);
    CORRADE_VERIFY(object2);
    CORRADE_COMPARE(object2->transformation(), Matrix4::scaling(Vector3::zScale(1.75f)));

    std::unique_ptr<Trade::ObjectData3D> object3 = importer->object3D(2);
    CORRADE_VERIFY(object3);
    CORRADE_COMPARE(object3->transformation(), Matrix4::scaling(Vector3::yScale(0.5f)));
}

void TinyGltfImporterTest::mesh() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh" + std::string{data.extension})));

    CORRADE_COMPARE(importer->mesh3DCount(), 1);

    auto meshObject = importer->mesh3D(0);
    CORRADE_VERIFY(meshObject);
    CORRADE_COMPARE(meshObject->primitive(), MeshPrimitive::Triangles);
    CORRADE_VERIFY(meshObject->isIndexed());
    CORRADE_COMPARE(meshObject->positionArrayCount(), 1);
    CORRADE_COMPARE(meshObject->normalArrayCount(), 1);

    CORRADE_COMPARE_AS(meshObject->positions(0), (std::vector<Vector3>{
        {0.685616612f, -1.02956f, -0.277003706f},
        {-0.00734680891f, 1.0624f, -0.0872567892f},
        {-0.584888637f, -0.268546f, 0.291010320f}
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE(meshObject->normalArrayCount(), 1);
    CORRADE_COMPARE_AS(meshObject->normals(0), (std::vector<Vector3>{
        {0.439082f, 0.0641624f, 0.896153f},
        {0.439082f, 0.0641624f, 0.896153f},
        {0.439082f, 0.0641624f, 0.896153f}
    }), TestSuite::Compare::Container);

    CORRADE_COMPARE(meshObject->indices(), (std::vector<UnsignedInt>{0, 1, 2}));
}

void TinyGltfImporterTest::material() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material" + std::string{data.extension})));

    CORRADE_COMPARE(importer->materialCount(), 2);

    auto material = importer->material(0);

    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

    auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
    CORRADE_COMPARE(phong.diffuseTexture(), 0);
    CORRADE_COMPARE(phong.specularTexture(), 0);
    CORRADE_COMPARE(phong.shininess(), 12.298039215686275f);

    CORRADE_COMPARE(importer->materialForName("awesomeMaterial"), 0);
    CORRADE_COMPARE(importer->materialName(0), "awesomeMaterial");
    CORRADE_COMPARE(importer->textureCount(), 1);

    auto texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->type(), TextureData::Type::Texture2D);

    auto material2 = importer->material(1);
    CORRADE_VERIFY(material2);
    CORRADE_COMPARE(material2->type(), Trade::MaterialType::Phong);

    auto&& phong2 = static_cast<const Trade::PhongMaterialData&>(*material2);

    CORRADE_COMPARE(phong2.diffuseColor(), (Color3{0.12716870497418498f, 0.26973092957930156f, 0.6392822360885475f}));
    CORRADE_COMPARE(phong2.specularColor(), (Color3{0.11348294466733932f, 0.5f, 0.44396162033081055f}));
    CORRADE_COMPARE(phong2.shininess(), 12.298039215686275f);

    CORRADE_COMPARE(importer->materialForName("secondMaterial"), 1);
    CORRADE_COMPARE(importer->materialName(1), "secondMaterial");
}

void TinyGltfImporterTest::image() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material" + std::string{data.extension})));

    const char expected[] =
        "\xa8\xa7\xac\xff\x9d\x9e\xa0\xff\xad\xad\xac\xff\xbb\xbb\xba\xff\xb3\xb4\xb6\xff"
        "\xb0\xb1\xb6\xff\xa0\xa0\xa1\xff\x9f\x9f\xa0\xff\xbc\xbc\xba\xff\xcc\xcc\xcc\xff"
        "\xb2\xb4\xb9\xff\xb8\xb9\xbb\xff\xc1\xc3\xc2\xff\xbc\xbd\xbf\xff\xb8\xb8\xbc\xff";

    CORRADE_COMPARE(importer->image2DCount(), 1);
    auto image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(expected).prefix(60), TestSuite::Compare::Container);
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::TinyGltfImporterTest)
