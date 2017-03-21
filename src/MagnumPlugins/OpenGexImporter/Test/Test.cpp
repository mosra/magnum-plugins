/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Quaternion.h>
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

#include "MagnumPlugins/OpenGexImporter/OpenGex.h"
#include "MagnumPlugins/OpenGexImporter/OpenGexImporter.h"
#include "MagnumPlugins/OpenGexImporter/OpenDdl/Document.h"
#include "MagnumPlugins/OpenGexImporter/OpenDdl/Property.h"
#include "MagnumPlugins/OpenGexImporter/OpenDdl/Structure.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

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
    void meshEnlargeShrink();
    void meshMetrics();

    void meshInvalidPrimitive();
    void meshUnsupportedSize();
    void meshNoPositions();
    void meshMismatchedSizes();
    void meshInvalidIndexArraySubArraySize();
    #ifndef MAGNUM_TARGET_WEBGL
    void meshUnsupportedIndexType();
    #endif

    void materialDefaults();
    void materialColors();
    void materialTextured();
    void materialInvalidColor();

    void texture();
    void textureInvalidCoordinateSet();

    void image();
    void imageInvalid();
    void imageUnique();

    void extension();
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
              &OpenGexImporterTest::meshEnlargeShrink,
              &OpenGexImporterTest::meshMetrics,

              &OpenGexImporterTest::meshInvalidPrimitive,
              &OpenGexImporterTest::meshUnsupportedSize,
              &OpenGexImporterTest::meshNoPositions,
              &OpenGexImporterTest::meshMismatchedSizes,
              &OpenGexImporterTest::meshInvalidIndexArraySubArraySize,
              #ifndef MAGNUM_TARGET_WEBGL
              &OpenGexImporterTest::meshUnsupportedIndexType,
              #endif

              &OpenGexImporterTest::materialDefaults,
              &OpenGexImporterTest::materialColors,
              &OpenGexImporterTest::materialTextured,
              &OpenGexImporterTest::materialInvalidColor,

              &OpenGexImporterTest::texture,
              &OpenGexImporterTest::textureInvalidCoordinateSet,

              &OpenGexImporterTest::image,
              &OpenGexImporterTest::imageInvalid,
              &OpenGexImporterTest::imageUnique,

              &OpenGexImporterTest::extension});
}

void OpenGexImporterTest::open() {
    OpenGexImporter importer;

    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = OpenDdl::CharacterLiteral{R"oddl(
Metric (key = "distance") { float { 0.5 } }
Metric (key = "angle") { float { 1.0 } }
Metric (key = "time") { float { 1000 } }
Metric (key = "up") { string { "z" } }
    )oddl"};
    CORRADE_VERIFY(importer.openData(s));
}

void OpenGexImporterTest::openParseError() {
    OpenGexImporter importer;

    std::ostringstream out;
    Error redirectError{&out};
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = OpenDdl::CharacterLiteral{R"oddl(
<collada>THIS IS COLLADA XML</collada>
    )oddl"};
    CORRADE_VERIFY(!importer.openData(s));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): invalid identifier on line 2\n");
}

void OpenGexImporterTest::openValidationError() {
    OpenGexImporter importer;

    std::ostringstream out;
    Error redirectError{&out};
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = OpenDdl::CharacterLiteral{R"oddl(
Metric (key = "distance") { int32 { 1 } }
    )oddl"};
    CORRADE_VERIFY(!importer.openData(s));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): unexpected sub-structure of type OpenDdl::Type::Int in structure Metric\n");
}

void OpenGexImporterTest::openInvalidMetric() {
    OpenGexImporter importer;

    std::ostringstream out;
    Error redirectError{&out};
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = OpenDdl::CharacterLiteral{R"oddl(
Metric (key = "distance") { string { "0.5" } }
    )oddl"};
    CORRADE_VERIFY(!importer.openData(s));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::openData(): invalid value for distance metric\n");
}

void OpenGexImporterTest::camera() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "camera.ogex")));
    CORRADE_COMPARE(importer.cameraCount(), 2);

    /* Everything specified */
    {
        std::optional<Trade::CameraData> camera = importer.camera(0);
        CORRADE_VERIFY(camera);
        CORRADE_COMPARE(camera->fov(), 0.97_radf);
        CORRADE_COMPARE(camera->near(), 1.5f);
        CORRADE_COMPARE(camera->far(), 150.0f);

    /* Nothing specified (defaults) */
    } {
        std::optional<Trade::CameraData> camera = importer.camera(1);
        CORRADE_VERIFY(camera);
        CORRADE_COMPARE(camera->fov(), Rad{35.0_degf});
        CORRADE_COMPARE(camera->near(), 0.01f);
        CORRADE_COMPARE(camera->far(), 100.0f);
    }
}

void OpenGexImporterTest::cameraMetrics() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "camera-metrics.ogex")));
    CORRADE_COMPARE(importer.cameraCount(), 1);

    std::optional<Trade::CameraData> camera = importer.camera(0);
    CORRADE_VERIFY(camera);
    CORRADE_COMPARE(camera->fov(), 0.97_radf);
    CORRADE_COMPARE(camera->near(), 1.5f);
    CORRADE_COMPARE(camera->far(), 150.0f);
}

void OpenGexImporterTest::cameraInvalid() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "camera-invalid.ogex")));
    CORRADE_COMPARE(importer.cameraCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.camera(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::camera(): invalid parameter\n");
}

void OpenGexImporterTest::object() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object.ogex")));
    CORRADE_COMPARE(importer.sceneCount(), 1);
    CORRADE_COMPARE(importer.object3DCount(), 5);

    std::optional<Trade::SceneData> scene = importer.scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), (std::vector<UnsignedInt>{0, 3}));

    std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(importer.object3DName(0), "MyNode");
    CORRADE_COMPARE(importer.object3DForName("MyNode"), 0);
    CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{1, 2}));

    std::unique_ptr<Trade::ObjectData3D> cameraObject = importer.object3D(1);
    CORRADE_VERIFY(cameraObject);
    CORRADE_COMPARE(cameraObject->instanceType(), Trade::ObjectInstanceType3D::Camera);

    std::unique_ptr<Trade::ObjectData3D> meshObject = importer.object3D(2);
    CORRADE_VERIFY(meshObject);
    CORRADE_COMPARE(importer.object3DName(2), "MyGeometryNode");
    CORRADE_COMPARE(importer.object3DForName("MyGeometryNode"), 2);
    CORRADE_COMPARE(meshObject->instanceType(), Trade::ObjectInstanceType3D::Mesh);
    CORRADE_VERIFY(meshObject->children().empty());

    std::unique_ptr<Trade::ObjectData3D> boneObject = importer.object3D(3);
    CORRADE_VERIFY(boneObject);
    CORRADE_COMPARE(boneObject->instanceType(), Trade::ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(boneObject->children(), (std::vector<UnsignedInt>{4}));

    std::unique_ptr<Trade::ObjectData3D> lightObject = importer.object3D(4);
    CORRADE_VERIFY(lightObject);
    CORRADE_COMPARE(lightObject->instanceType(), Trade::ObjectInstanceType3D::Light);
    CORRADE_VERIFY(lightObject->children().empty());
}

void OpenGexImporterTest::objectCamera() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-camera.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 2);

    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Camera);
        CORRADE_COMPARE(object->instance(), 1);
    }

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.object3D(1));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::object3D(): null camera reference\n");
}

void OpenGexImporterTest::objectLight() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-light.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 2);

    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Light);
        CORRADE_COMPARE(object->instance(), 1);
    }

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.object3D(1));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::object3D(): null light reference\n");
}

void OpenGexImporterTest::objectMesh() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-geometry.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 4);

    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Mesh);

        auto&& meshObject = static_cast<Trade::MeshObjectData3D&>(*object);
        CORRADE_COMPARE(meshObject.instance(), 1);
        CORRADE_COMPARE(meshObject.material(), 2);
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(1);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Mesh);

        auto&& meshObject = static_cast<Trade::MeshObjectData3D&>(*object);
        CORRADE_COMPARE(meshObject.material(), -1);
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(2);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Mesh);

        auto&& meshObject = static_cast<Trade::MeshObjectData3D&>(*object);
        CORRADE_COMPARE(meshObject.material(), -1);
    }

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.object3D(3));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::object3D(): null geometry reference\n");
}

void OpenGexImporterTest::objectTransformation() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-transformation.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 3);

    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
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
    CORRADE_VERIFY(!importer.object3D(1));
    CORRADE_VERIFY(!importer.object3D(2));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenGexImporter::object3D(): invalid transformation\n"
        "Trade::OpenGexImporter::object3D(): unsupported object-only transformation\n");
}

void OpenGexImporterTest::objectTranslation() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-translation.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 8);

    /* XYZ */
    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::translation({7.5f, -1.5f, 1.0f}));

    /* Default, which is also XYZ */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(1);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::translation({7.5f, -1.5f, 1.0f}));

    /* X */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(2);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::translation(Vector3::xAxis(7.5f)));

    /* Y */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(3);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::translation(Vector3::yAxis(-1.5f)));

    /* Z */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(4);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::translation(Vector3::zAxis(1.0f)));
    }

    /* Invalid kind, invalid array size, object-only transformation */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.object3D(5));
    CORRADE_VERIFY(!importer.object3D(6));
    CORRADE_VERIFY(!importer.object3D(7));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenGexImporter::object3D(): invalid translation\n"
        "Trade::OpenGexImporter::object3D(): invalid translation\n"
        "Trade::OpenGexImporter::object3D(): unsupported object-only transformation\n");
}

void OpenGexImporterTest::objectRotation() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-rotation.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 9);

    /* Axis + angle */
    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::rotation(90.0_degf, Vector3::zAxis()));

    /* Default, which is also axis + angle */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(1);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::rotation(-90.0_degf, Vector3::zAxis(-1.0f)));

    /* Quaternion */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(2);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::from(Quaternion::rotation(90.0_degf, Vector3::zAxis()).toMatrix(), {}));

    /* X */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(3);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::rotationX(90.0_degf));

    /* Y */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(4);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::rotationY(90.0_degf));

    /* Z */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(5);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::rotationZ(90.0_degf));
    }

    /* Invalid kind, invalid array size, object-only transformation */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.object3D(6));
    CORRADE_VERIFY(!importer.object3D(7));
    CORRADE_VERIFY(!importer.object3D(8));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenGexImporter::object3D(): invalid rotation\n"
        "Trade::OpenGexImporter::object3D(): invalid rotation\n"
        "Trade::OpenGexImporter::object3D(): unsupported object-only transformation\n");
}

void OpenGexImporterTest::objectScaling() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-scaling.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 8);

    /* XYZ */
    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling({7.5f, -1.5f, 2.0f}));

    /* Default, which is also XYZ */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(1);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling({7.5f, -1.5f, 2.0f}));

    /* X */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(2);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling(Vector3::xScale(7.5f)));

    /* Y */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(3);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling(Vector3::yScale(-1.5f)));

    /* Z */
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(4);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling(Vector3::zScale(2.0f)));
    }

    /* Invalid kind, invalid array size, object-only transformation */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.object3D(5));
    CORRADE_VERIFY(!importer.object3D(6));
    CORRADE_VERIFY(!importer.object3D(7));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenGexImporter::object3D(): invalid scaling\n"
        "Trade::OpenGexImporter::object3D(): invalid scaling\n"
        "Trade::OpenGexImporter::object3D(): unsupported object-only transformation\n");
}

void OpenGexImporterTest::objectTransformationConcatentation() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-transformation-concatenation.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 1);

    std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->transformation(),
        Matrix4::translation({7.5f, -1.5f, 1.0f})*
        Matrix4::scaling({1.0f, 2.0f, -1.0f})*
        Matrix4::rotationX(-90.0_degf));
}

void OpenGexImporterTest::objectTransformationMetrics() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "object-transformation-metrics.ogex")));
    CORRADE_COMPARE(importer.object3DCount(), 7);

    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(),
            Matrix4::translation({100.0f, 550.0f, 200.0f})*
            Matrix4::scaling({1.0f, 5.5f, -2.0f})
        );
    }

    /* Each pair describes the same transformation using given operation and
       transformation matrix */
    {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(1);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::translation({100.0f, 550.0f, 200.0f}));
        std::unique_ptr<Trade::ObjectData3D> matrix = importer.object3D(2);
        CORRADE_VERIFY(matrix);
        CORRADE_COMPARE(matrix->transformation(), Matrix4::translation({100.0f, 550.0f, 200.0f}));
    } {
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(3);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::rotationZ(-90.0_degf));
        std::unique_ptr<Trade::ObjectData3D> matrix = importer.object3D(4);
        CORRADE_VERIFY(matrix);
        CORRADE_COMPARE(matrix->transformation(), Matrix4::rotationZ(-90.0_degf));
    } {
        /* This won't be multiplied by 100, as the original mesh data are adjusted already */
        std::unique_ptr<Trade::ObjectData3D> object = importer.object3D(5);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling({1.0f, 5.5f, -2.0f}));
        std::unique_ptr<Trade::ObjectData3D> matrix = importer.object3D(6);
        CORRADE_VERIFY(matrix);
        CORRADE_COMPARE(matrix->transformation(), Matrix4::scaling({1.0f, 5.5f, -2.0f}));
    }
}

void OpenGexImporterTest::light() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "light.ogex")));
    CORRADE_COMPARE(importer.lightCount(), 3);

    /* Infinite light, everything specified */
    {
        std::optional<Trade::LightData> light = importer.light(0);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Infinite);
        CORRADE_COMPARE(light->color(), (Color3{0.7f, 1.0f, 0.1f}));
        CORRADE_COMPARE(light->intensity(), 3.0f);

    /* Point light, default color */
    } {
        std::optional<Trade::LightData> light = importer.light(1);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{1.0f, 1.0f, 1.0f}));
        CORRADE_COMPARE(light->intensity(), 0.5f);

    /* Spot light, default intensity */
    } {
        std::optional<Trade::LightData> light = importer.light(2);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Spot);
        CORRADE_COMPARE(light->color(), (Color3{0.1f, 0.0f, 0.1f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
    }
}

void OpenGexImporterTest::lightInvalid() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "light-invalid.ogex")));
    CORRADE_COMPARE(importer.lightCount(), 4);

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer.light(0));
        CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::light(): invalid type\n");
    } {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer.light(1));
        CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::light(): invalid parameter\n");
    } {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer.light(2));
        CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::light(): invalid color\n");
    } {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer.light(3));
        CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::light(): invalid color structure\n");
    }
}

void OpenGexImporterTest::mesh() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh.ogex")));

    std::optional<Trade::MeshData3D> mesh = importer.mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::TriangleStrip);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
        {0.0f, 1.0f, 3.0f}, {-1.0f, 2.0f, 2.0f}, {3.0f, 3.0f, 1.0f}
    }));
    CORRADE_COMPARE(mesh->normalArrayCount(), 1);
    CORRADE_COMPARE(mesh->normals(0), (std::vector<Vector3>{
        {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}
    }));
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 2);
    CORRADE_COMPARE(mesh->textureCoords2D(0), (std::vector<Vector2>{
        {0.5f, 0.5f}, {0.5f, 1.0f}, {1.0f, 1.0f}
    }));
    CORRADE_COMPARE(mesh->textureCoords2D(1), (std::vector<Vector2>{
        {0.5f, 1.0f}, {1.0f, 0.5f}, {0.5f, 0.5f}
    }));
}

void OpenGexImporterTest::meshIndexed() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh.ogex")));

    std::optional<Trade::MeshData3D> mesh = importer.mesh3D(1);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
        {0.0f, 1.0f, 3.0f}, {-1.0f, 2.0f, 2.0f}, {3.0f, 3.0f, 1.0f}, {5.0f, 7.0f, 0.5f}
    }));
    CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{
        2, 0, 1, 1, 2, 3
    }));
}

void OpenGexImporterTest::meshEnlargeShrink() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh.ogex")));

    std::optional<Trade::MeshData3D> mesh = importer.mesh3D(2);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
        {0.0f, 1.0f, 3.0f}, {-1.0f, 2.0f, 2.0f}, {3.0f, 3.0f, 1.0f}
    }));
    CORRADE_COMPARE(mesh->normalArrayCount(), 1);
    CORRADE_COMPARE(mesh->normals(0), (std::vector<Vector3>{
        {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}
    }));
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 1);
    CORRADE_COMPARE(mesh->textureCoords2D(0), (std::vector<Vector2>{
        {0.5f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}
    }));
}

void OpenGexImporterTest::meshMetrics() {
    OpenGexImporter importer;

    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-metrics.ogex")));
    std::optional<Trade::MeshData3D> mesh = importer.mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->positions(0), (std::vector<Vector3>{
        {100.0f, -200.0f, -50.0f} /* swapped for Y up, multiplied */
    }));
    CORRADE_COMPARE(mesh->normalArrayCount(), 1);
    CORRADE_COMPARE(mesh->normals(0), (std::vector<Vector3>{
        {1.0, -1.0, -2.5} /* swapped for Y up */
    }));
    CORRADE_COMPARE(mesh->textureCoords2DArrayCount(), 1);
    CORRADE_COMPARE(mesh->textureCoords2D(0), (std::vector<Vector2>{
        {1.0, 0.5} /* no change */
    }));
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indices(), (std::vector<UnsignedInt>{
        2 /* no change */
    }));
}

void OpenGexImporterTest::meshInvalidPrimitive() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer.mesh3DCount(), 5);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh3D(): unsupported primitive quads\n");
}

void OpenGexImporterTest::meshUnsupportedSize() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer.mesh3DCount(), 5);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.mesh3D(1));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh3D(): unsupported vertex array vector size 5\n");
}

void OpenGexImporterTest::meshNoPositions() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer.mesh3DCount(), 5);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.mesh3D(2));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh3D(): no vertex position array found\n");
}

void OpenGexImporterTest::meshMismatchedSizes() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer.mesh3DCount(), 5);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.mesh3D(3));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh3D(): mismatched vertex array sizes\n");
}

void OpenGexImporterTest::meshInvalidIndexArraySubArraySize() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid.ogex")));
    CORRADE_COMPARE(importer.mesh3DCount(), 5);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.mesh3D(4));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh3D(): invalid index array subarray size 3 for MeshPrimitive::Lines\n");
}

#ifndef MAGNUM_TARGET_WEBGL
void OpenGexImporterTest::meshUnsupportedIndexType() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "mesh-invalid-int64.ogex")));
    CORRADE_COMPARE(importer.mesh3DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::mesh3D(): unsupported 64bit indices\n");
}
#endif

void OpenGexImporterTest::materialDefaults() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material.ogex")));

    std::unique_ptr<Trade::AbstractMaterialData> material = importer.material(0);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);
    CORRADE_COMPARE(importer.materialName(0), "");

    auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
    CORRADE_COMPARE(phong.ambientColor(), Vector3{0.0f});
    CORRADE_COMPARE(phong.diffuseColor(), Vector3{1.0f});
    CORRADE_COMPARE(phong.specularColor(), Vector3{0.0f});
    CORRADE_COMPARE(phong.shininess(), 1.0f);
}

void OpenGexImporterTest::materialColors() {
    OpenGexImporter importer;

    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material.ogex")));
    CORRADE_COMPARE(importer.materialCount(), 4);

    std::unique_ptr<Trade::AbstractMaterialData> material = importer.material(1);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);
    CORRADE_COMPARE(importer.materialName(1), "colors");
    CORRADE_COMPARE(importer.materialForName("colors"), 1);

    auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
    CORRADE_VERIFY(!phong.flags());
    CORRADE_COMPARE(phong.diffuseColor(), (Vector3{0.0f, 0.8f, 0.5f}));
    CORRADE_COMPARE(phong.specularColor(), (Vector3{0.5f, 0.2f, 1.0f}));
    CORRADE_COMPARE(phong.shininess(), 80.0f);
}

void OpenGexImporterTest::materialTextured() {
    OpenGexImporter importer;

    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material.ogex")));
    CORRADE_COMPARE(importer.materialCount(), 4);
    CORRADE_COMPARE(importer.textureCount(), 4);

    {
        std::unique_ptr<Trade::AbstractMaterialData> material = importer.material(2);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(importer.materialName(2), "diffuse_texture");

        auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_VERIFY(phong.flags() == Trade::PhongMaterialData::Flag::DiffuseTexture);
        CORRADE_COMPARE(phong.diffuseTexture(), 1);
    } {
        std::unique_ptr<Trade::AbstractMaterialData> material = importer.material(3);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(importer.materialName(3), "both_textures");

        auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_VERIFY(phong.flags() == (Trade::PhongMaterialData::Flag::DiffuseTexture|Trade::PhongMaterialData::Flag::SpecularTexture));
        CORRADE_COMPARE(phong.diffuseTexture(), 2);
        CORRADE_COMPARE(phong.specularTexture(), 3);
    }
}

void OpenGexImporterTest::materialInvalidColor() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "material-invalid.ogex")));
    CORRADE_COMPARE(importer.materialCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.material(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::material(): invalid color structure\n");
}

void OpenGexImporterTest::texture() {
    OpenGexImporter importer;

    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture.ogex")));
    CORRADE_COMPARE(importer.textureCount(), 2);

    std::optional<Trade::TextureData> texture = importer.texture(1);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->minificationFilter(), Sampler::Filter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), Sampler::Filter::Linear);
    CORRADE_COMPARE(texture->wrapping(), Sampler::Wrapping::ClampToEdge);
    CORRADE_COMPARE(texture->image(), 1);
}

void OpenGexImporterTest::textureInvalidCoordinateSet() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture-invalid.ogex")));
    CORRADE_COMPARE(importer.textureCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.texture(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenGexImporter::texture(): unsupported texture coordinate set\n");
}

void OpenGexImporterTest::image() {
    PluginManager::Manager<AbstractImporter> manager{MAGNUM_PLUGINS_IMPORTER_DIR};

    if(manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");

    OpenGexImporter importer{manager};
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture.ogex")));
    CORRADE_COMPARE(importer.image2DCount(), 2);

    /* Check only size, as it is good enough proof that it is working */
    std::optional<Trade::ImageData2D> image = importer.image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
}

void OpenGexImporterTest::imageInvalid() {
    PluginManager::Manager<AbstractImporter> manager{MAGNUM_PLUGINS_IMPORTER_DIR};

    if(manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");

    OpenGexImporter importer{manager};
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture-invalid.ogex")));
    CORRADE_COMPARE(importer.image2DCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer.image2D(1));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /nonexistent.tga\n");
}

void OpenGexImporterTest::imageUnique() {
    PluginManager::Manager<AbstractImporter> manager{MAGNUM_PLUGINS_IMPORTER_DIR};

    if(manager.loadState("TgaImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("TgaImporter plugin not found, cannot test");

    OpenGexImporter importer{manager};
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "texture-unique.ogex")));
    CORRADE_COMPARE(importer.textureCount(), 5);
    CORRADE_COMPARE(importer.image2DCount(), 3);

    /* Verify mapping from textures to unique images */
    {
        std::optional<TextureData> texture0 = importer.texture(0);
        CORRADE_VERIFY(texture0);
        CORRADE_COMPARE_AS(texture0->image(), 2, TestSuite::Compare::LessOrEqual);

        std::optional<TextureData> texture4 = importer.texture(4);
        CORRADE_VERIFY(texture4);
        CORRADE_COMPARE(texture4->image(), texture0->image());

        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer.image2D(texture0->image()));
        CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /tex1.tga\n");
    } {
        std::optional<TextureData> texture1 = importer.texture(1);
        CORRADE_VERIFY(texture1);
        CORRADE_COMPARE_AS(texture1->image(), 2, TestSuite::Compare::LessOrEqual);

        std::optional<TextureData> texture3 = importer.texture(3);
        CORRADE_VERIFY(texture3);
        CORRADE_COMPARE(texture3->image(), texture1->image());

        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer.image2D(texture1->image()));
        CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /tex2.tga\n");
    } {
        std::optional<TextureData> texture2 = importer.texture(2);
        CORRADE_VERIFY(texture2);
        CORRADE_COMPARE_AS(texture2->image(), 2, TestSuite::Compare::LessOrEqual);

        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer.image2D(texture2->image()));
        CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /tex3.tga\n");
    }
}

void OpenGexImporterTest::extension() {
    OpenGexImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(OPENGEXIMPORTER_TEST_DIR, "extension.ogex")));

    /* Version info */
    {
        CORRADE_VERIFY(importer.importerState());
        std::optional<OpenDdl::Structure> version = static_cast<const OpenDdl::Document*>(importer.importerState())->findFirstChildOf(OpenGex::Extension);
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
        CORRADE_COMPARE(importer.object3DCount(), 2);
        std::unique_ptr<ObjectData3D> cameraObject = importer.object3D(1);
        CORRADE_VERIFY(cameraObject);
        CORRADE_VERIFY(cameraObject->importerState());
        std::optional<OpenDdl::Structure> cameraName = static_cast<const OpenDdl::Structure*>(cameraObject->importerState())->findFirstChildOf(OpenGex::Extension);
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
        CORRADE_COMPARE(importer.cameraCount(), 1);
        std::optional<CameraData> camera = importer.camera(0);
        CORRADE_VERIFY(camera);
        CORRADE_VERIFY(camera->importerState());
        std::optional<OpenDdl::Structure> cameraObject = static_cast<const OpenDdl::Structure*>(camera->importerState())->findFirstChildOf(OpenGex::Extension);
        CORRADE_VERIFY(cameraObject);
        CORRADE_VERIFY(cameraObject->findPropertyOf(OpenGex::applic));
        CORRADE_COMPARE(cameraObject->propertyOf(OpenGex::applic).as<std::string>(), "Magnum");
        CORRADE_COMPARE(cameraObject->propertyOf(OpenGex::type).as<std::string>(), "CameraAperture");
        CORRADE_VERIFY(cameraObject->hasChildren());
        CORRADE_COMPARE(cameraObject->firstChild().type(), OpenDdl::Type::Float);
        CORRADE_COMPARE(cameraObject->firstChild().as<Float>(), 1.8f);
    }
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::OpenGexImporterTest)
