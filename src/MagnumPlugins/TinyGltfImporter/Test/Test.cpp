/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2018 Tobias Stein <stein.tobi@t-online.de>
    Copyright © 2018 Jonathan Hale <squareys@googlemail.com>

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
#include <Magnum/Array.h>
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
#include <Magnum/Sampler.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

struct TinyGltfImporterTest: TestSuite::Tester {
    explicit TinyGltfImporterTest();

    void open();
    void openError();
    void openFileError();

    void defaultScene();

    void camera();

    void light();

    void object();
    void objectTransformation();
    void objectTranslation();
    void objectRotation();
    void objectScaling();

    void mesh();
    void meshPrimitives();
    void meshColors();
    void meshWithStride();

    void materialPbrMetallicRoughness();
    void materialPbrSpecularGlossiness();
    void materialBlinnPhong();

    void texture();
    void textureDefaultSampler();
    void image();
    void imageEmbedded();

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
                       &TinyGltfImporterTest::openFileError,

                       &TinyGltfImporterTest::defaultScene,

                       &TinyGltfImporterTest::camera,

                       &TinyGltfImporterTest::light,

                       &TinyGltfImporterTest::object,
                       &TinyGltfImporterTest::objectTransformation,
                       &TinyGltfImporterTest::objectTranslation,
                       &TinyGltfImporterTest::objectRotation,
                       &TinyGltfImporterTest::objectScaling,

                       &TinyGltfImporterTest::mesh,
                       &TinyGltfImporterTest::meshPrimitives,
                       &TinyGltfImporterTest::meshColors,
                       &TinyGltfImporterTest::meshWithStride,

                       &TinyGltfImporterTest::materialPbrMetallicRoughness,
                       &TinyGltfImporterTest::materialPbrSpecularGlossiness,
                       &TinyGltfImporterTest::materialBlinnPhong,

                       &TinyGltfImporterTest::texture,
                       &TinyGltfImporterTest::textureDefaultSampler,

                       &TinyGltfImporterTest::image,
                       &TinyGltfImporterTest::imageEmbedded}, InstanceDataCount);

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

void TinyGltfImporterTest::openFileError() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::ostringstream out;
    Error redirectError{&out};

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(!importer->openFile("nope" + std::string(data.extension)));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file nope" + std::string{data.extension} + "\n");
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
        "object" + std::string{data.extension})));

    CORRADE_COMPARE(importer->object3DCount(), 5);

    auto scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), (std::vector<UnsignedInt>{2, 4}));

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
    CORRADE_COMPARE(emptyObject2->children(), (std::vector<UnsignedInt>{3, 1}));
}

void TinyGltfImporterTest::objectTransformation() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "object-transformation" + std::string{data.extension})));

    CORRADE_COMPARE(importer->object3DCount(), 2);

    std::unique_ptr<Trade::ObjectData3D> object = importer->object3D(0);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->transformation(), (Matrix4{
        {0.0f,  1.0f, 0.0f, 0.0f},
        {-0.707107f, 0.0f, -0.707107f, 0.0f},
        {-0.707107f,  0.0f, 0.707107f, 0},
        {1.0f, -2.0f, -2.0f, 1.0f}
    }));

    std::unique_ptr<Trade::ObjectData3D> objectWithMatrix = importer->object3D(1);
    CORRADE_VERIFY(objectWithMatrix);
    CORRADE_COMPARE(objectWithMatrix->transformation(), (Matrix4{
        {-0.99975f, -0.00679829f, 0.0213218f, 0.0f},
        {0.00167596f, 0.927325f, 0.374254f, 0.0f},
        {-0.0223165f, 0.374196f, -0.927081f, 0.0f},
        {-0.0115543f, 0.194711f, -0.478297f, 1.0f}
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
    std::ostringstream out;
    Warning redirectWarning{&out};

    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh" + std::string{data.extension})));

    CORRADE_COMPARE(importer->object3DCount(), 1);
    CORRADE_COMPARE(importer->object3DName(0), "Node");
    CORRADE_COMPARE(importer->object3DForName("Node"), 0);

    CORRADE_COMPARE(importer->mesh3DCount(), 1);
    CORRADE_COMPARE(importer->mesh3DName(0), "Mesh");
    CORRADE_COMPARE(importer->mesh3DForName("Mesh"), 0);

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

    /* Test file contains UNKNOWN attribute which is not imported */
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::mesh3D(): unsupported mesh vertex attribute UNKNOWN\n");
}

void TinyGltfImporterTest::meshPrimitives() {
    std::ostringstream out;
    Warning redirectWarning{&out};

    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-primitives" + std::string{data.extension})));

    CORRADE_COMPARE(importer->mesh3DCount(), 6);

    {
        auto mesh = importer->mesh3D(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Points);

        CORRADE_VERIFY(mesh->isIndexed());
        CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{0, 2, 1}));

        CORRADE_COMPARE_AS(mesh->positions(0), (std::vector<Vector3>{
            {1.0f, 2.0f, 3.0f},
            {4.0f, 5.0f, 6.0f},
            {7.0f, 8.0f, 9.0f}
        }), TestSuite::Compare::Container);
    } {
        auto mesh = importer->mesh3D(1);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Lines);

        CORRADE_VERIFY(mesh->isIndexed());
        CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{0, 2, 1, 3}));

        CORRADE_COMPARE_AS(mesh->positions(0), (std::vector<Vector3>{
            {10.0f, 11.0f, 12.0f},
            {13.0f, 14.0f, 15.0f},
            {16.0f, 17.0f, 18.0f},
            {1.9f, 20.0f, 2.1f}
        }), TestSuite::Compare::Container);
    } {
        auto mesh = importer->mesh3D(2);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::LineLoop);

        CORRADE_VERIFY(mesh->isIndexed());
        CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{0, 1}));

        CORRADE_COMPARE_AS(mesh->positions(0), (std::vector<Vector3>{
            {1.1f, 1.2f, 1.4f},
            {1.5f, 1.6f, 1.7f},
            {1.8f, 1.9f, 2.0f}
        }), TestSuite::Compare::Container);
    } {
        auto mesh = importer->mesh3D(3);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::LineStrip);

        CORRADE_VERIFY(mesh->isIndexed());
        CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{2, 1, 0}));

        CORRADE_COMPARE_AS(mesh->positions(0), (std::vector<Vector3>{
            {2.1f, 2.2f, 2.4f},
            {2.5f, 2.6f, 2.7f},
            {2.8f, 2.9f, 3.0f}
        }), TestSuite::Compare::Container);
    } {
        auto mesh = importer->mesh3D(4);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::TriangleStrip);

        CORRADE_VERIFY(mesh->isIndexed());
        CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{2, 1, 0, 3}));

        CORRADE_COMPARE_AS(mesh->positions(0), (std::vector<Vector3>{
            {3.1f, 3.2f, 3.4f},
            {3.5f, 3.6f, 3.7f},
            {3.8f, 3.9f, 4.0f},
            {4.1f, 4.2f, 4.3f}
        }), TestSuite::Compare::Container);
    } {
        auto mesh = importer->mesh3D(5);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::TriangleFan);

        CORRADE_VERIFY(mesh->isIndexed());
        CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{2, 1, 3, 0}));

        CORRADE_COMPARE_AS(mesh->positions(0), (std::vector<Vector3>{
            {5.1f, 5.2f, 5.3f},
            {6.1f, 6.2f, 6.3f},
            {7.1f, 7.2f, 7.3f},
            {8.1f, 8.2f, 8.3f}
        }), TestSuite::Compare::Container);
    }
}

void TinyGltfImporterTest::meshColors() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-colors" + std::string{data.extension})));

    CORRADE_COMPARE(importer->mesh3DCount(), 1);

    auto meshObject = importer->mesh3D(0);
    CORRADE_VERIFY(meshObject);

    CORRADE_COMPARE(meshObject->colorArrayCount(), 2);
    CORRADE_COMPARE_AS(meshObject->colors(0), (std::vector<Color4>{
        {0.0f, 0.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f}
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(meshObject->colors(1), (std::vector<Color4>{
        {0.501962f, 0.501962f, 0.501962f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}
    }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::meshWithStride() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-with-stride" + std::string{data.extension})));

    std::ostringstream out;
    Error redirectError{&out};

    /* First has a stride of 12 bytes (no interleaving), shouldn't fail */
    CORRADE_VERIFY(importer->mesh3D(0));

    /* Second has a stride of 24 bytes, should fail */
    CORRADE_VERIFY(!importer->mesh3D(1));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::mesh3D(): interleaved buffer views are not supported\n");
}

void TinyGltfImporterTest::materialPbrMetallicRoughness() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-metallicroughness" + std::string{data.extension})));

    CORRADE_COMPARE(importer->materialCount(), 2);

    {
        auto material = importer->material(0);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

        auto& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_VERIFY(phong.flags() & PhongMaterialData::Flag::DiffuseTexture);
        CORRADE_COMPARE(phong.diffuseTexture(), 0);
        CORRADE_COMPARE(phong.specularColor(), 0xffffff_rgbf);
        CORRADE_COMPARE(phong.shininess(), 1.0f);

        CORRADE_COMPARE(importer->materialForName("Metallic/Roughness with textures"), 0);
        CORRADE_COMPARE(importer->materialName(0), "Metallic/Roughness with textures");
    } {
        auto material = importer->material(1);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

        auto& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.3f, 0.4f, 0.5f, 0.8f}));
        CORRADE_COMPARE(phong.specularColor(), 0xffffff_rgbf);
        CORRADE_COMPARE(phong.shininess(), 1.0f);

        CORRADE_COMPARE(importer->materialForName("Metallic/Roughness without textures"), 1);
        CORRADE_COMPARE(importer->materialName(1), "Metallic/Roughness without textures");
    }
}

void TinyGltfImporterTest::materialPbrSpecularGlossiness() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-specularglossiness" + std::string{data.extension})));

    CORRADE_COMPARE(importer->materialCount(), 2);

    {
        auto material = importer->material(0);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

        auto& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_VERIFY(phong.flags() & PhongMaterialData::Flag::DiffuseTexture);
        CORRADE_VERIFY(phong.flags() & PhongMaterialData::Flag::SpecularTexture);
        CORRADE_COMPARE(phong.diffuseTexture(), 0);
        CORRADE_COMPARE(phong.specularTexture(), 1);
        CORRADE_COMPARE(phong.shininess(), 1.0f);

        CORRADE_COMPARE(importer->materialForName("Specular/Glossiness with textures"), 0);
        CORRADE_COMPARE(importer->materialName(0), "Specular/Glossiness with textures");
    } {
        auto material = importer->material(1);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

        auto& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.3f, 0.4f, 0.5f, 0.8f}));
        CORRADE_COMPARE(phong.specularColor(), (Color4{0.1f, 0.2f, 0.6f}));
        CORRADE_COMPARE(phong.shininess(), 1.0f);

        CORRADE_COMPARE(importer->materialForName("Specular/Glossiness without textures"), 1);
        CORRADE_COMPARE(importer->materialName(1), "Specular/Glossiness without textures");
    }
}

void TinyGltfImporterTest::materialBlinnPhong() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-blinnphong" + std::string{data.extension})));

    CORRADE_COMPARE(importer->materialCount(), 2);

    {
        auto material = importer->material(0);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

        auto& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_VERIFY(phong.flags() & PhongMaterialData::Flag::DiffuseTexture);
        CORRADE_VERIFY(phong.flags() & PhongMaterialData::Flag::SpecularTexture);
        CORRADE_COMPARE(phong.diffuseTexture(), 0);
        CORRADE_COMPARE(phong.specularTexture(), 1);
        CORRADE_COMPARE(phong.shininess(), 40.5f);

        CORRADE_COMPARE(importer->materialForName("Blinn/Phong with textures"), 0);
        CORRADE_COMPARE(importer->materialName(0), "Blinn/Phong with textures");
    } {
        auto material = importer->material(1);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

        auto& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.3f, 0.4f, 0.5f, 0.8f}));
        CORRADE_COMPARE(phong.specularColor(), (Color4{0.1f, 0.2f, 0.6f}));
        CORRADE_COMPARE(phong.shininess(), 12.7f);

        CORRADE_COMPARE(importer->materialForName("Blinn/Phong without textures"), 1);
        CORRADE_COMPARE(importer->materialName(1), "Blinn/Phong without textures");
    }
}

void TinyGltfImporterTest::texture() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture" + std::string{data.extension})));

    CORRADE_COMPARE(importer->materialCount(), 1);

    auto material = importer->material(0);

    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

    auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
    CORRADE_VERIFY(phong.flags() & PhongMaterialData::Flag::DiffuseTexture);
    CORRADE_COMPARE(phong.diffuseTexture(), 0);
    CORRADE_COMPARE(phong.shininess(), 1.0);

    auto texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->type(), TextureData::Type::Texture2D);

    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Nearest);
    CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Linear);

    CORRADE_COMPARE(texture->wrapping(), Array3D<SamplerWrapping>(SamplerWrapping::MirroredRepeat, SamplerWrapping::ClampToEdge, SamplerWrapping::Repeat));

    /* Texture coordinates */
    auto meshObject = importer->mesh3D(0);
    CORRADE_VERIFY(meshObject);

    CORRADE_COMPARE(meshObject->textureCoords2DArrayCount(), 2);
    CORRADE_COMPARE_AS(meshObject->textureCoords2D(0), (std::vector<Vector2>{
        {0.94991f, 0.05009f}, {0.3f, 0.94991f}, {0.1f, 0.2f}
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(meshObject->textureCoords2D(1), (std::vector<Vector2>{
        {0.5f, 0.5f}, {0.3f, 0.7f}, {0.2f, 0.42f}
    }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::textureDefaultSampler() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture-default-sampler" + std::string{data.extension})));

    auto texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->type(), TextureData::Type::Texture2D);

    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Linear);

    CORRADE_COMPARE(texture->wrapping(), Array3D<SamplerWrapping>(SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping::Repeat));
}

void TinyGltfImporterTest::image() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "image" + std::string{data.extension})));

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

void TinyGltfImporterTest::imageEmbedded() {
    auto&& data = InstanceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "image-embedded" + std::string{data.extension})));

    const char expected[] = "\xff\x00\xff\xff";

    CORRADE_COMPARE(importer->image2DCount(), 1);
    auto image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(1, 1));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(expected).prefix(4), TestSuite::Compare::Container);
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::TinyGltfImporterTest)
