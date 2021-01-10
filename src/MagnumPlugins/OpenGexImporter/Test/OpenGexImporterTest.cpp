/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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
#include <unordered_map>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/String.h>
#include <Magnum/FileCallback.h>
#include <Magnum/Mesh.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/ObjectData3D.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/LightData.h>

#include "Magnum/OpenDdl/Document.h"
#include "Magnum/OpenDdl/Property.h"
#include "Magnum/OpenDdl/Structure.h"
#include "MagnumPlugins/OpenGexImporter/OpenGex.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

using namespace Magnum::Math::Literals;

struct OpenGexImporterTest: public TestSuite::Tester {
    explicit OpenGexImporterTest();

    void open();
    void openParseError();
    void openValidationError();
    void openInvalidMetric();

    void camera();
    void cameraMetrics();
    void cameraInvalid();

    void object();
    void objectCamera();
    void objectLight();
    void objectMesh();
    void objectTransformation();
    void objectTranslation();
    void objectRotation();
    void objectScaling();
    void objectTransformationConcatentation();
    void objectTransformationMetrics();

    void light();
    void lightInvalid();

    void mesh();
    void meshIndexed();
    void meshMetrics();

    void meshInvalidPrimitive();
    void meshUnsupportedSize();
    void meshMismatchedSizes();
    void meshInvalidIndexArraySubArraySize();
    #ifndef CORRADE_TARGET_EMSCRIPTEN
    void meshUnsupportedIndexType();
    #endif

    void materialDefaults();
    void materialColors();
    void materialTextured();
    void materialInvalidColor();

    void texture();
    void textureInvalidCoordinateSet();

    void image();
    void imageNotFound();
    void imageUnique();
    void imageMipLevels();
    void imageNoPathNoCallback();

    void extension();

    void fileCallbackImage();
    void fileCallbackImageNotFound();

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

OpenGexImporterTest::OpenGexImporterTest() {
    addTests({&OpenGexImporterTest::open,
              &OpenGexImporterTest::openParseError,
              &OpenGexImporterTest::openValidationError,
              &OpenGexImporterTest::openInvalidMetric,

              &OpenGexImporterTest::camera,
              &OpenGexImporterTest::cameraMetrics,
              &OpenGexImporterTest::cameraInvalid,

              &OpenGexImporterTest::object,
              &OpenGexImporterTest::objectCamera,
              &OpenGexImporterTest::objectLight,
              &OpenGexImporterTest::objectMesh,
              &OpenGexImporterTest::objectTransformation,
              &OpenGexImporterTest::objectTranslation,
              &OpenGexImporterTest::objectRotation,
              &OpenGexImporterTest::objectScaling,
              &OpenGexImporterTest::objectTransformationConcatentation,
              &OpenGexImporterTest::objectTransformationMetrics,

              &OpenGexImporterTest::light,
              &OpenGexImporterTest::lightInvalid,

              &OpenGexImporterTest::mesh,
              &OpenGexImporterTest::meshIndexed,
              &OpenGexImporterTest::meshMetrics,

              &OpenGexImporterTest::meshInvalidPrimitive,
              &OpenGexImporterTest::meshUnsupportedSize,
              &OpenGexImporterTest::meshMismatchedSizes,
              &OpenGexImporterTest::meshInvalidIndexArraySubArraySize,
              #ifndef CORRADE_TARGET_EMSCRIPTEN
              &OpenGexImporterTest::meshUnsupportedIndexType,
              #endif

              &OpenGexImporterTest::materialDefaults,
              &OpenGexImporterTest::materialColors,
              &OpenGexImporterTest::materialTextured,
              &OpenGexImporterTest::materialInvalidColor,

              &OpenGexImporterTest::texture,
              &OpenGexImporterTest::textureInvalidCoordinateSet,

              &OpenGexImporterTest::image,
              &OpenGexImporterTest::imageNotFound,
              &OpenGexImporterTest::imageUnique,
              &OpenGexImporterTest::imageMipLevels,
              &OpenGexImporterTest::imageNoPathNoCallback,

              &OpenGexImporterTest::extension,

              &OpenGexImporterTest::fileCallbackImage,
              &OpenGexImporterTest::fileCallbackImageNotFound});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. Reset
       the plugin dir after so it doesn't load anything else from the
       filesystem. */
    #ifdef OPENGEXIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(OPENGEXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    _manager.setPluginDirectory({});
    #endif
    /* The DdsImporter (for DDS loading / mip import tests) is optional */
    #ifdef DDSIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(DDSIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* The StbImageImporter (for TGA image loading) is optional */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void OpenGexImporterTest::open() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");

    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = OpenDdl::CharacterLiteral{R"oddl(
Metric (key = "distance") { float { 0.5 } }
Metric (key = "angle") { float { 1.0 } }
Metric (key = "time") { float { 1000 } }
Metric (key = "up") { string { "z" } }
    )oddl"};
    CORRADE_VERIFY(importer->openData(s));
}

void OpenGexImporterTest::openParseError() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");

    std::ostringstream out;
    Error redirectError{&out};
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = OpenDdl::CharacterLiteral{R"oddl(
<collada>THIS IS COLLADA XML</collada>
    )oddl"};
    CORRADE_VERIFY(!importer->openData(s));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): invalid identifier on line 2\n");
}

void OpenGexImporterTest::openValidationError() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");

    std::ostringstream out;
    Error redirectError{&out};
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = OpenDdl::CharacterLiteral{R"oddl(
Metric (key = "distance") { int32 { 1 } }
    )oddl"};
    CORRADE_VERIFY(!importer->openData(s));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): unexpected sub-structure of type OpenDdl::Type::Int in structure Metric\n");
}

void OpenGexImporterTest::openInvalidMetric() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");

    std::ostringstream out;
    Error redirectError{&out};
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = OpenDdl::CharacterLiteral{R"oddl(
Metric (key = "distance") { string { "0.5" } }
    )oddl"};
    CORRADE_VERIFY(!importer->openData(s));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::openData(): invalid value for distance metric\n");
}

void OpenGexImporterTest::camera() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "camera.ogex")));
    CORRADE_COMPARE(importer->cameraCount(), 2);

    /* Everything specified */
    {
        Containers::Optional<CameraData> camera = importer->camera(0);
        CORRADE_VERIFY(camera);
        CORRADE_COMPARE(camera->fov(), 0.97_radf);
        CORRADE_COMPARE(camera->near(), 1.5f);
        CORRADE_COMPARE(camera->far(), 150.0f);

    /* Nothing specified (defaults) */
    } {
        Containers::Optional<CameraData> camera = importer->camera(1);
        CORRADE_VERIFY(camera);
        CORRADE_COMPARE(camera->fov(), Rad{35.0_degf});
        CORRADE_COMPARE(camera->near(), 0.01f);
        CORRADE_COMPARE(camera->far(), 100.0f);
    }
}

void OpenGexImporterTest::cameraMetrics() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "camera-metrics.ogex")));
    CORRADE_COMPARE(importer->cameraCount(), 1);

    Containers::Optional<CameraData> camera = importer->camera(0);
    CORRADE_VERIFY(camera);
    CORRADE_COMPARE(camera->fov(), 0.97_radf);
    CORRADE_COMPARE(camera->near(), 1.5f);
    CORRADE_COMPARE(camera->far(), 150.0f);
}

void OpenGexImporterTest::cameraInvalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "camera-invalid.ogex")));
    CORRADE_COMPARE(importer->cameraCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->camera(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::camera(): invalid parameter\n");
}

void OpenGexImporterTest::object() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object.ogex")));
    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 5);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), (std::vector<UnsignedInt>{0, 3}));

    Containers::Pointer<ObjectData3D> object = importer->object3D(0);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(importer->object3DName(0), "MyNode");
    CORRADE_COMPARE(importer->object3DForName("MyNode"), 0);
    CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{1, 2}));

    Containers::Pointer<ObjectData3D> cameraObject = importer->object3D(1);
    CORRADE_VERIFY(cameraObject);
    CORRADE_COMPARE(cameraObject->instanceType(), ObjectInstanceType3D::Camera);

    Containers::Pointer<ObjectData3D> meshObject = importer->object3D(2);
    CORRADE_VERIFY(meshObject);
    CORRADE_COMPARE(importer->object3DName(2), "MyGeometryNode");
    CORRADE_COMPARE(importer->object3DForName("MyGeometryNode"), 2);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_VERIFY(meshObject->children().empty());

    Containers::Pointer<ObjectData3D> boneObject = importer->object3D(3);
    CORRADE_VERIFY(boneObject);
    CORRADE_COMPARE(boneObject->instanceType(), ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(boneObject->children(), (std::vector<UnsignedInt>{4}));

    Containers::Pointer<ObjectData3D> lightObject = importer->object3D(4);
    CORRADE_VERIFY(lightObject);
    CORRADE_COMPARE(lightObject->instanceType(), ObjectInstanceType3D::Light);
    CORRADE_VERIFY(lightObject->children().empty());
}

void OpenGexImporterTest::objectCamera() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-camera.ogex")));
    CORRADE_COMPARE(importer->object3DCount(), 2);

    {
        Containers::Pointer<ObjectData3D> object = importer->object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Camera);
        CORRADE_COMPARE(object->instance(), 1);
    }

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->object3D(1));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::object3D(): null camera reference\n");
}

void OpenGexImporterTest::objectLight() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-light.ogex")));
    CORRADE_COMPARE(importer->object3DCount(), 2);

    {
        Containers::Pointer<ObjectData3D> object = importer->object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Light);
        CORRADE_COMPARE(object->instance(), 1);
    }

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->object3D(1));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::object3D(): null light reference\n");
}

void OpenGexImporterTest::objectMesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-geometry.ogex")));
    CORRADE_COMPARE(importer->object3DCount(), 4);

    {
        Containers::Pointer<ObjectData3D> object = importer->object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);

        auto&& meshObject = static_cast<MeshObjectData3D&>(*object);
        CORRADE_COMPARE(meshObject.instance(), 1);
        CORRADE_COMPARE(meshObject.material(), 2);
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(1);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);

        auto&& meshObject = static_cast<MeshObjectData3D&>(*object);
        CORRADE_COMPARE(meshObject.material(), -1);
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(2);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);

        auto&& meshObject = static_cast<MeshObjectData3D&>(*object);
        CORRADE_COMPARE(meshObject.material(), -1);
    }

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->object3D(3));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::object3D(): null geometry reference\n");
}

void OpenGexImporterTest::objectTransformation() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-transformation.ogex")));
    CORRADE_COMPARE(importer->object3DCount(), 3);

    {
        Containers::Pointer<ObjectData3D> object = importer->object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), (Matrix4{
            {3.0f,  0.0f, 0.0f, 0.0f},
            {0.0f, -2.0f, 0.0f, 0.0f},
            {0.0f,  0.0f, 0.5f, 0.0f},
            {7.5f, -1.5f, 1.0f, 1.0f}
        }));
    }

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->object3D(1));
    CORRADE_VERIFY(!importer->object3D(2));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenGexImporter::object3D(): invalid transformation\n"
        "Trade::OpenGexImporter::object3D(): unsupported object-only transformation\n");
}

void OpenGexImporterTest::objectTranslation() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-translation.ogex")));
    CORRADE_COMPARE(importer->object3DCount(), 8);

    /* XYZ */
    {
        Containers::Pointer<ObjectData3D> object = importer->object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::translation({7.5f, -1.5f, 1.0f}));

    /* Default, which is also XYZ */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(1);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::translation({7.5f, -1.5f, 1.0f}));

    /* X */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(2);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::translation(Vector3::xAxis(7.5f)));

    /* Y */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(3);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::translation(Vector3::yAxis(-1.5f)));

    /* Z */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(4);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::translation(Vector3::zAxis(1.0f)));
    }

    /* Invalid kind, invalid array size, object-only transformation */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->object3D(5));
    CORRADE_VERIFY(!importer->object3D(6));
    CORRADE_VERIFY(!importer->object3D(7));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenGexImporter::object3D(): invalid translation\n"
        "Trade::OpenGexImporter::object3D(): invalid translation\n"
        "Trade::OpenGexImporter::object3D(): unsupported object-only transformation\n");
}

void OpenGexImporterTest::objectRotation() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-rotation.ogex")));
    CORRADE_COMPARE(importer->object3DCount(), 9);

    /* Axis + angle */
    {
        Containers::Pointer<ObjectData3D> object = importer->object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::rotation(90.0_degf, Vector3::zAxis()));

    /* Default, which is also axis + angle */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(1);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::rotation(-90.0_degf, Vector3::zAxis(-1.0f)));

    /* Quaternion */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(2);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::from(Quaternion::rotation(90.0_degf, Vector3::zAxis()).toMatrix(), {}));

    /* X */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(3);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::rotationX(90.0_degf));

    /* Y */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(4);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::rotationY(90.0_degf));

    /* Z */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(5);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::rotationZ(90.0_degf));
    }

    /* Invalid kind, invalid array size, object-only transformation */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->object3D(6));
    CORRADE_VERIFY(!importer->object3D(7));
    CORRADE_VERIFY(!importer->object3D(8));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenGexImporter::object3D(): invalid rotation\n"
        "Trade::OpenGexImporter::object3D(): invalid rotation\n"
        "Trade::OpenGexImporter::object3D(): unsupported object-only transformation\n");
}

void OpenGexImporterTest::objectScaling() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-scaling.ogex")));
    CORRADE_COMPARE(importer->object3DCount(), 8);

    /* XYZ */
    {
        Containers::Pointer<ObjectData3D> object = importer->object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling({7.5f, -1.5f, 2.0f}));

    /* Default, which is also XYZ */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(1);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling({7.5f, -1.5f, 2.0f}));

    /* X */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(2);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling(Vector3::xScale(7.5f)));

    /* Y */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(3);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling(Vector3::yScale(-1.5f)));

    /* Z */
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(4);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling(Vector3::zScale(2.0f)));
    }

    /* Invalid kind, invalid array size, object-only transformation */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->object3D(5));
    CORRADE_VERIFY(!importer->object3D(6));
    CORRADE_VERIFY(!importer->object3D(7));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenGexImporter::object3D(): invalid scaling\n"
        "Trade::OpenGexImporter::object3D(): invalid scaling\n"
        "Trade::OpenGexImporter::object3D(): unsupported object-only transformation\n");
}

void OpenGexImporterTest::objectTransformationConcatentation() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-transformation-concatenation.ogex")));
    CORRADE_COMPARE(importer->object3DCount(), 1);

    Containers::Pointer<ObjectData3D> object = importer->object3D(0);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->transformation(),
        Matrix4::translation({7.5f, -1.5f, 1.0f})*
        Matrix4::scaling({1.0f, 2.0f, -1.0f})*
        Matrix4::rotationX(-90.0_degf));
}

void OpenGexImporterTest::objectTransformationMetrics() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-transformation-metrics.ogex")));
    CORRADE_COMPARE(importer->object3DCount(), 7);

    {
        Containers::Pointer<ObjectData3D> object = importer->object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(),
            Matrix4::translation({100.0f, 550.0f, 200.0f})*
            Matrix4::scaling({1.0f, 5.5f, -2.0f})
        );
    }

    /* Each pair describes the same transformation using given operation and
       transformation matrix */
    {
        Containers::Pointer<ObjectData3D> object = importer->object3D(1);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::translation({100.0f, 550.0f, 200.0f}));
        Containers::Pointer<ObjectData3D> matrix = importer->object3D(2);
        CORRADE_VERIFY(matrix);
        CORRADE_COMPARE(matrix->transformation(), Matrix4::translation({100.0f, 550.0f, 200.0f}));
    } {
        Containers::Pointer<ObjectData3D> object = importer->object3D(3);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::rotationZ(-90.0_degf));
        Containers::Pointer<ObjectData3D> matrix = importer->object3D(4);
        CORRADE_VERIFY(matrix);
        CORRADE_COMPARE(matrix->transformation(), Matrix4::rotationZ(-90.0_degf));
    } {
        /* This won't be multiplied by 100, as the original mesh data are adjusted already */
        Containers::Pointer<ObjectData3D> object = importer->object3D(5);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling({1.0f, 5.5f, -2.0f}));
        Containers::Pointer<ObjectData3D> matrix = importer->object3D(6);
        CORRADE_VERIFY(matrix);
        CORRADE_COMPARE(matrix->transformation(), Matrix4::scaling({1.0f, 5.5f, -2.0f}));
    }
}

void OpenGexImporterTest::light() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "light.ogex")));
    CORRADE_COMPARE(importer->lightCount(), 3);

    /* Infinite light, everything specified */
    {
        Containers::Optional<LightData> light = importer->light(0);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Directional);
        CORRADE_COMPARE(light->color(), (Color3{0.7f, 1.0f, 0.1f}));
        CORRADE_COMPARE(light->intensity(), 3.0f);

    /* Point light, default color */
    } {
        Containers::Optional<LightData> light = importer->light(1);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{1.0f, 1.0f, 1.0f}));
        CORRADE_COMPARE(light->intensity(), 0.5f);

    /* Spot light, default intensity */
    } {
        Containers::Optional<LightData> light = importer->light(2);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Spot);
        CORRADE_COMPARE(light->color(), (Color3{0.1f, 0.0f, 0.1f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
    }
}

void OpenGexImporterTest::lightInvalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "light-invalid.ogex")));
    CORRADE_COMPARE(importer->lightCount(), 4);

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer->light(0));
        CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::light(): invalid type\n");
    } {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer->light(1));
        CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::light(): invalid parameter\n");
    } {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer->light(2));
        CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::light(): invalid color\n");
    } {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer->light(3));
        CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::light(): invalid color structure\n");
    }
}

void OpenGexImporterTest::mesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh.ogex")));

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::TriangleStrip);

    CORRADE_VERIFY(!mesh->isIndexed());

    CORRADE_COMPARE(mesh->attributeCount(), 4);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {0.0f, 1.0f, 3.0f}, {-1.0f, 2.0f, 2.0f}, {3.0f, 3.0f, 1.0f}
        }), TestSuite::Compare::Container);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Normal));
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Normal),
        Containers::arrayView<Vector3>({
            {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::TextureCoordinates), 2);
    CORRADE_COMPARE_AS(mesh->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        Containers::arrayView<Vector2>({
            {0.5f, 0.5f}, {0.5f, 1.0f}, {1.0f, 1.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(mesh->attribute<Vector2>(MeshAttribute::TextureCoordinates, 1),
        Containers::arrayView<Vector2>({
            {0.5f, 1.0f}, {1.0f, 0.5f}, {0.5f, 0.5f}
        }), TestSuite::Compare::Container);
}

void OpenGexImporterTest::meshIndexed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh.ogex")));

    Containers::Optional<MeshData> mesh = importer->mesh(1);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedShort);
    CORRADE_COMPARE_AS(mesh->indices<UnsignedShort>(),
        Containers::arrayView<UnsignedShort>({
            2, 0, 1, 1, 2, 3
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {0.0f, 1.0f, 3.0f}, {-1.0f, 2.0f, 2.0f}, {3.0f, 3.0f, 1.0f}, {5.0f, 7.0f, 0.5f}
        }), TestSuite::Compare::Container);
}

void OpenGexImporterTest::meshMetrics() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-metrics.ogex")));
    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedByte);
    CORRADE_COMPARE_AS(mesh->indices<UnsignedByte>(),
        Containers::arrayView<UnsignedByte>({
            2
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 3);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {100.0f, -200.0f, -50.0f} /* swapped for Y up, multiplied */
        }), TestSuite::Compare::Container);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Normal));
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Normal),
        Containers::arrayView<Vector3>({
            {1.0, -1.0, -2.5} /* swapped for Y up */
        }), TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::TextureCoordinates));
    CORRADE_COMPARE_AS(mesh->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        Containers::arrayView<Vector2>({
            {1.0, 0.5} /* no change */
        }), TestSuite::Compare::Container);
}

void OpenGexImporterTest::meshInvalidPrimitive() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer->meshCount(), 6);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh(): unsupported primitive quads\n");
}

void OpenGexImporterTest::meshUnsupportedSize() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer->meshCount(), 6);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(1));
    CORRADE_VERIFY(!importer->mesh(2));
    CORRADE_VERIFY(!importer->mesh(3));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenGexImporter::mesh(): unsupported position vector size 4\n"
        "Trade::OpenGexImporter::mesh(): unsupported normal vector size 2\n"
        "Trade::OpenGexImporter::mesh(): unsupported texture coordinate vector size 3\n");
}

void OpenGexImporterTest::meshMismatchedSizes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer->meshCount(), 6);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(4));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh(): mismatched vertex count for attribute normal, expected 2 but got 1\n");
}

void OpenGexImporterTest::meshInvalidIndexArraySubArraySize() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer->meshCount(), 6);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(5));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh(): invalid index array subarray size 3 for MeshPrimitive::Lines\n");
}

#ifndef CORRADE_TARGET_EMSCRIPTEN
void OpenGexImporterTest::meshUnsupportedIndexType() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid-int64.ogex")));
    CORRADE_COMPARE(importer->meshCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh(): 64bit indices are not supported\n");
}
#endif

void OpenGexImporterTest::materialDefaults() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material.ogex")));

    Containers::Optional<MaterialData> material = importer->material(0);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType::Phong);
    CORRADE_COMPARE(material->layerCount(), 1);
    CORRADE_COMPARE(material->attributeCount(), 0);
    CORRADE_COMPARE(importer->materialName(0), "");

    /* Not checking any attributes as the defaults are handled by
       PhongMaterialData itself anyway */
}

void OpenGexImporterTest::materialColors() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material.ogex")));
    CORRADE_COMPARE(importer->materialCount(), 4);

    Containers::Optional<MaterialData> material = importer->material(1);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType::Phong);
    CORRADE_COMPARE(material->layerCount(), 1);
    CORRADE_COMPARE(material->attributeCount(), 3);
    CORRADE_COMPARE(importer->materialName(1), "colors");
    CORRADE_COMPARE(importer->materialForName("colors"), 1);

    auto&& phong = material->as<PhongMaterialData>();
    CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.0f, 0.8f, 0.5f, 1.0f}));
    CORRADE_COMPARE(phong.specularColor(), (Color4{0.5f, 0.2f, 1.0f, 0.8f}));
    CORRADE_COMPARE(phong.shininess(), 10.0f);
}

void OpenGexImporterTest::materialTextured() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material.ogex")));
    CORRADE_COMPARE(importer->materialCount(), 4);
    CORRADE_COMPARE(importer->textureCount(), 4);

    {
        Containers::Optional<MaterialData> material = importer->material(2);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 2);
        CORRADE_COMPARE(importer->materialName(2), "diffuse_texture");

        auto&& phong = material->as<PhongMaterialData>();
        CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
        CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.0f, 0.8f, 0.5f, 1.1f}));
        CORRADE_COMPARE(phong.diffuseTexture(), 1);
    } {
        Containers::Optional<MaterialData> material = importer->material(3);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 3);
        CORRADE_COMPARE(importer->materialName(3), "both_textures");

        auto&& phong = material->as<PhongMaterialData>();
        CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
        CORRADE_VERIFY(phong.hasSpecularTexture());
        CORRADE_COMPARE(phong.diffuseTexture(), 2);
        CORRADE_COMPARE(phong.specularColor(), (Color4{0.5f, 0.2f, 1.0f, 0.8f}));
        CORRADE_COMPARE(phong.specularTexture(), 3);
    }
}

void OpenGexImporterTest::materialInvalidColor() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material-invalid.ogex")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->material(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::material(): invalid color structure\n");
}

void OpenGexImporterTest::texture() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture.ogex")));
    CORRADE_COMPARE(importer->textureCount(), 2);

    Containers::Optional<TextureData> texture = importer->texture(1);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->wrapping(), SamplerWrapping::ClampToEdge);
    CORRADE_COMPARE(texture->image(), 1);
}

void OpenGexImporterTest::textureInvalidCoordinateSet() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture-invalid.ogex")));
    CORRADE_COMPARE(importer->textureCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->texture(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::texture(): unsupported texture coordinate set\n");
}

void OpenGexImporterTest::image() {
    if(_manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture.ogex")));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    /* Check only size, as it is good enough proof that it is working */
    Containers::Optional<ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
}

void OpenGexImporterTest::imageNotFound() {
    if(_manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture-invalid.ogex")));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    /* The (failed) importer should get cached even in case of failure, so
       the message should get printed just once */
    CORRADE_VERIFY(!importer->image2D(1));
    CORRADE_VERIFY(!importer->image2D(1));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /nonexistent.tga\n");
}

void OpenGexImporterTest::imageUnique() {
    if(_manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture-unique.ogex")));
    CORRADE_COMPARE(importer->textureCount(), 5);
    CORRADE_COMPARE(importer->image2DCount(), 3);

    /* Verify mapping from textures to unique images */
    {
        Containers::Optional<TextureData> texture0 = importer->texture(0);
        CORRADE_VERIFY(texture0);
        CORRADE_COMPARE_AS(texture0->image(), 2, TestSuite::Compare::LessOrEqual);

        Containers::Optional<TextureData> texture4 = importer->texture(4);
        CORRADE_VERIFY(texture4);
        CORRADE_COMPARE(texture4->image(), texture0->image());

        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer->image2D(texture0->image()));
        CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /tex1.tga\n");
    } {
        Containers::Optional<TextureData> texture1 = importer->texture(1);
        CORRADE_VERIFY(texture1);
        CORRADE_COMPARE_AS(texture1->image(), 2, TestSuite::Compare::LessOrEqual);

        Containers::Optional<TextureData> texture3 = importer->texture(3);
        CORRADE_VERIFY(texture3);
        CORRADE_COMPARE(texture3->image(), texture1->image());

        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer->image2D(texture1->image()));
        CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /tex2.tga\n");
    } {
        Containers::Optional<TextureData> texture2 = importer->texture(2);
        CORRADE_VERIFY(texture2);
        CORRADE_COMPARE_AS(texture2->image(), 2, TestSuite::Compare::LessOrEqual);

        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer->image2D(texture2->image()));
        CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /tex3.tga\n");
    }
}

void OpenGexImporterTest::imageMipLevels() {
    if(_manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");
    if(_manager.loadState("DdsImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("DdsImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture-mips.ogex")));
    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 2);
    CORRADE_COMPARE(importer->image2DLevelCount(1), 1);

    /* Verify that loading a different image will properly switch to another
       importer instance */
    Containers::Optional<ImageData2D> image00 = importer->image2D(0);
    Containers::Optional<ImageData2D> image01 = importer->image2D(0, 1);
    Containers::Optional<ImageData2D> image1 = importer->image2D(1);

    CORRADE_VERIFY(image00);
    CORRADE_COMPARE(image00->size(), (Vector2i{3, 2}));
    CORRADE_COMPARE(image00->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image00->data(), Containers::arrayView<char>({
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77'
    }), TestSuite::Compare::Container);

    CORRADE_VERIFY(image01);
    CORRADE_COMPARE(image01->size(), Vector2i{1});
    CORRADE_COMPARE(image01->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image01->data(), Containers::arrayView<char>({
        '\xd4', '\xd5', '\x96'
    }), TestSuite::Compare::Container);

    CORRADE_VERIFY(image1);
    CORRADE_COMPARE(image1->size(), (Vector2i{2, 3}));
    CORRADE_COMPARE(image1->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image1->data(), Containers::arrayView<char>({
        3, 2, 1, 4, 3, 2,
        5, 4, 3, 6, 5, 4,
        7, 6, 5, 8, 7, 6
    }), TestSuite::Compare::Container);
}

void OpenGexImporterTest::imageNoPathNoCallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture.ogex"))));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::image2D(): images can be imported only when opening files from the filesystem or if a file callback is present\n");
}

void OpenGexImporterTest::extension() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "extension.ogex")));

    /* Version info */
    {
        CORRADE_VERIFY(importer->importerState());
        Containers::Optional<OpenDdl::Structure> version = static_cast<const OpenDdl::Document*>(importer->importerState())->findFirstChildOf(OpenGex::Extension);
        CORRADE_VERIFY(version);
        CORRADE_VERIFY(version->findPropertyOf(OpenGex::applic));
        CORRADE_COMPARE(version->propertyOf(OpenGex::applic).as<std::string>(), "Magnum");
        CORRADE_COMPARE(version->propertyOf(OpenGex::type).as<std::string>(), "Version");
        CORRADE_VERIFY(version->hasChildren());
        CORRADE_COMPARE(version->firstChild().type(), OpenDdl::Type::Int);
        CORRADE_COMPARE(version->firstChild().as<Int>(), 123);
    }

    /* Camera name */
    {
        CORRADE_COMPARE(importer->object3DCount(), 2);
        Containers::Pointer<ObjectData3D> cameraObject = importer->object3D(1);
        CORRADE_VERIFY(cameraObject);
        CORRADE_VERIFY(cameraObject->importerState());
        Containers::Optional<OpenDdl::Structure> cameraName = static_cast<const OpenDdl::Structure*>(cameraObject->importerState())->findFirstChildOf(OpenGex::Extension);
        CORRADE_VERIFY(cameraName);
        CORRADE_VERIFY(cameraName->findPropertyOf(OpenGex::applic));
        CORRADE_COMPARE(cameraName->propertyOf(OpenGex::applic).as<std::string>(), "Magnum");
        CORRADE_COMPARE(cameraName->propertyOf(OpenGex::type).as<std::string>(), "CameraName");
        CORRADE_VERIFY(cameraName->hasChildren());
        CORRADE_COMPARE(cameraName->firstChild().type(), OpenDdl::Type::String);
        CORRADE_COMPARE(cameraName->firstChild().as<std::string>(), "My camera");
    }

    /* Camera aperture */
    {
        CORRADE_COMPARE(importer->cameraCount(), 1);
        Containers::Optional<CameraData> camera = importer->camera(0);
        CORRADE_VERIFY(camera);
        CORRADE_VERIFY(camera->importerState());
        Containers::Optional<OpenDdl::Structure> cameraObject = static_cast<const OpenDdl::Structure*>(camera->importerState())->findFirstChildOf(OpenGex::Extension);
        CORRADE_VERIFY(cameraObject);
        CORRADE_VERIFY(cameraObject->findPropertyOf(OpenGex::applic));
        CORRADE_COMPARE(cameraObject->propertyOf(OpenGex::applic).as<std::string>(), "Magnum");
        CORRADE_COMPARE(cameraObject->propertyOf(OpenGex::type).as<std::string>(), "CameraAperture");
        CORRADE_VERIFY(cameraObject->hasChildren());
        CORRADE_COMPARE(cameraObject->firstChild().type(), OpenDdl::Type::Float);
        CORRADE_COMPARE(cameraObject->firstChild().as<Float>(), 1.8f);
    }
}

void OpenGexImporterTest::fileCallbackImage() {
    if(_manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    std::unordered_map<std::string, Containers::Array<char>> files;
    files["not/a/path/something.ogex"] = Utility::Directory::read(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture.ogex"));
    files["not/a/path/image.tga"] = Utility::Directory::read(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "image.tga"));
    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy policy,
        std::unordered_map<std::string, Containers::Array<char>>& files) {
            Debug{} << "Loading" << filename << "with" << policy;
            return Containers::optional(Containers::ArrayView<const char>(files.at(filename)));
        }, files);

    CORRADE_VERIFY(importer->openFile("not/a/path/something.ogex"));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    /* Check only size, as it is good enough proof that it is working */
    Containers::Optional<ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
}

void OpenGexImporterTest::fileCallbackImageNotFound() {
    if(_manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenGexImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy, void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{};
        });

    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture.ogex"))));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(1));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file image.tga\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::OpenGexImporterTest)
