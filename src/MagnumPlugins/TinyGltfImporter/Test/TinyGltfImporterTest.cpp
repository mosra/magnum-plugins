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
#include <Magnum/Trade/AnimationData.h>
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

    void animation();
    void animationWrongTimeType();
    void animationWrongInterpolationType();
    void animationWrongTranslationType();
    void animationWrongRotationType();
    void animationWrongScalingType();
    void animationUnsupportedPath();

    void camera();

    void light();

    void scene();
    void objectTransformation();

    void mesh();
    void meshIndexed();
    void meshUnknownAttribute();
    void meshPrimitives();
    void meshColors();
    void meshWithStride();

    void materialPbrMetallicRoughness();
    void materialPbrSpecularGlossiness();
    void materialBlinnPhong();

    void texture();
    void textureDefaultSampler();

    void image();

    void fileCallbackBuffer();
    void fileCallbackImage();

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

namespace {

constexpr struct {
    const char* name;
    const char* suffix;
    Containers::ArrayView<const char> shortData;
    const char* shortDataError;
} OpenErrorData[]{
    {"ascii", ".gltf", {"?", 1}, "JSON string too short.\n\n"},
    {"binary", ".glb", {"glTF?", 5}, "Too short data size for glTF Binary.\n"}
};

constexpr struct {
    const char* name;
    const char* suffix;
} SingleFileData[]{
    {"ascii", ".gltf"},
    {"binary", ".glb"}
};

constexpr struct {
    const char* name;
    const char* suffix;
} MultiFileData[]{
    {"ascii external", ".gltf"},
    {"ascii embedded", "-embedded.gltf"},
    {"binary external", ".glb"},
    {"binary embedded", "-embedded.glb"}
};

constexpr struct {
    const char* name;
    const char* suffix;
} ImageData[]{
    {"ascii external", ".gltf"},
    {"ascii embedded", "-embedded.gltf"},
    {"ascii buffer external", "-buffer.gltf"},
    {"ascii buffer embedded", "-buffer-embedded.gltf"},
    {"binary external", ".glb"},
    {"binary embedded", "-embedded.glb"},
    {"binary buffer external", "-buffer.glb"},
    {"binary buffer embedded", "-buffer-embedded.glb"}
};

}

using namespace Magnum::Math::Literals;

TinyGltfImporterTest::TinyGltfImporterTest() {
    addInstancedTests({&TinyGltfImporterTest::open},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&TinyGltfImporterTest::openError,
                       &TinyGltfImporterTest::openFileError},
                      Containers::arraySize(OpenErrorData));

    addInstancedTests({&TinyGltfImporterTest::animation},
                      Containers::arraySize(MultiFileData));

    addTests({&TinyGltfImporterTest::animationWrongTimeType,
              &TinyGltfImporterTest::animationWrongInterpolationType,
              &TinyGltfImporterTest::animationWrongTranslationType,
              &TinyGltfImporterTest::animationWrongRotationType,
              &TinyGltfImporterTest::animationWrongScalingType,
              &TinyGltfImporterTest::animationUnsupportedPath});

    addInstancedTests({&TinyGltfImporterTest::camera,

                       &TinyGltfImporterTest::light,

                       &TinyGltfImporterTest::scene,
                       &TinyGltfImporterTest::objectTransformation},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&TinyGltfImporterTest::mesh,
                       &TinyGltfImporterTest::meshIndexed,
                       &TinyGltfImporterTest::meshUnknownAttribute,
                       &TinyGltfImporterTest::meshPrimitives,
                       &TinyGltfImporterTest::meshColors},
                      Containers::arraySize(MultiFileData));

    /* There are no external data for this one at the moment */
    addInstancedTests({&TinyGltfImporterTest::meshWithStride},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&TinyGltfImporterTest::materialPbrMetallicRoughness,
                       &TinyGltfImporterTest::materialPbrSpecularGlossiness,
                       &TinyGltfImporterTest::materialBlinnPhong,

                       &TinyGltfImporterTest::texture,
                       &TinyGltfImporterTest::textureDefaultSampler},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&TinyGltfImporterTest::image},
                      Containers::arraySize(ImageData));

    addInstancedTests({&TinyGltfImporterTest::fileCallbackBuffer,
                       &TinyGltfImporterTest::fileCallbackImage},
                      Containers::arraySize(SingleFileData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. Reset
       the plugin dir after so it doesn't load anything else from the filesystem. */
    #ifdef TINYGLTFIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(TINYGLTFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    _manager.setPluginDirectory({});
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void TinyGltfImporterTest::open() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    auto filename = Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "empty" + std::string{data.suffix});
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_VERIFY(importer->importerState());

    CORRADE_VERIFY(importer->openData(Utility::Directory::read(filename)));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_VERIFY(importer->importerState());

    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void TinyGltfImporterTest::openError() {
    auto&& data = OpenErrorData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::ostringstream out;
    Error redirectError{&out};

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(!importer->openData(data.shortData));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openFile(): error opening file: " + std::string{data.shortDataError});
}

void TinyGltfImporterTest::openFileError() {
    auto&& data = OpenErrorData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::ostringstream out;
    Error redirectError{&out};

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(!importer->openFile("nope" + std::string{data.suffix}));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file nope" + std::string{data.suffix} + "\n");
}

void TinyGltfImporterTest::animation() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->animationCount(), 2);

    /* Empty animation */
    {
        CORRADE_COMPARE(importer->animationName(0), "empty");
        CORRADE_COMPARE(importer->animationForName("empty"), 0);

        auto animation = importer->animation(0);
        CORRADE_VERIFY(animation);
        CORRADE_VERIFY(animation->data().empty());
        CORRADE_COMPARE(animation->trackCount(), 0);

    /* Translation/rotation/scaling animation */
    } {
        CORRADE_COMPARE(importer->animationName(1), "TRS animation");
        CORRADE_COMPARE(importer->animationForName("TRS animation"), 1);

        auto animation = importer->animation(1);
        CORRADE_VERIFY(animation);
        CORRADE_VERIFY(animation->importerState());
        /* Two rotation keys, four translation and scaling keys with common
           time track */
        CORRADE_COMPARE(animation->data().size(),
            2*(sizeof(Float) + sizeof(Quaternion)) +
            4*(sizeof(Float) + 2*sizeof(Vector3)));
        CORRADE_COMPARE(animation->trackCount(), 3);

        /* Rotation, linearly interpolated */
        CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::Quaternion);
        CORRADE_COMPARE(animation->trackResultType(0), AnimationTrackType::Quaternion);
        CORRADE_COMPARE(animation->trackTarget(0), AnimationTrackTarget::Rotation3D);
        CORRADE_COMPARE(animation->trackTargetId(0), 1337);
        Animation::TrackView<Float, Quaternion> rotation = animation->track<Quaternion>(0);
        CORRADE_COMPARE(rotation.interpolation(), Animation::Interpolation::Linear);
        CORRADE_COMPARE(rotation.before(), Animation::Extrapolation::Constant);
        CORRADE_COMPARE(rotation.after(), Animation::Extrapolation::Constant);
        const Float rotationKeys[]{
            1.25f,
            2.50f
        };
        const Quaternion rotationValues[]{
            Quaternion::rotation(0.0_degf, Vector3::xAxis()),
            Quaternion::rotation(180.0_degf, Vector3::xAxis())
        };
        CORRADE_COMPARE_AS(rotation.keys(), (Containers::StridedArrayView<const Float>{rotationKeys}), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(rotation.values(), (Containers::StridedArrayView<const Quaternion>{rotationValues}), TestSuite::Compare::Container);
        CORRADE_COMPARE(rotation.at(1.875f), Quaternion::rotation(90.0_degf, Vector3::xAxis()));

        const Float translationScalingKeys[]{
            0.0f,
            1.25f,
            2.5f,
            3.75f
        };

        /* Translation, constant interpolated, sharing keys with scaling */
        CORRADE_COMPARE(animation->trackType(1), AnimationTrackType::Vector3);
        CORRADE_COMPARE(animation->trackResultType(1), AnimationTrackType::Vector3);
        CORRADE_COMPARE(animation->trackTarget(1), AnimationTrackTarget::Translation3D);
        CORRADE_COMPARE(animation->trackTargetId(1), 42);
        Animation::TrackView<Float, Vector3> translation = animation->track<Vector3>(1);
        CORRADE_COMPARE(translation.interpolation(), Animation::Interpolation::Constant);
        CORRADE_COMPARE(translation.before(), Animation::Extrapolation::Constant);
        CORRADE_COMPARE(translation.after(), Animation::Extrapolation::Constant);
        const Vector3 translationData[]{
            Vector3::yAxis(0.0f),
            Vector3::yAxis(2.5f),
            Vector3::yAxis(2.5f),
            Vector3::yAxis(0.0f)
        };
        CORRADE_COMPARE_AS(translation.keys(), (Containers::StridedArrayView<const Float>{translationScalingKeys}), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(translation.values(), (Containers::StridedArrayView<const Vector3>{translationData}), TestSuite::Compare::Container);
        CORRADE_COMPARE(translation.at(1.5f), Vector3::yAxis(2.5f));

        /* Scaling, linearly interpolated, sharing keys with translation */
        CORRADE_COMPARE(animation->trackType(2), AnimationTrackType::Vector3);
        CORRADE_COMPARE(animation->trackResultType(2), AnimationTrackType::Vector3);
        CORRADE_COMPARE(animation->trackTarget(2), AnimationTrackTarget::Scaling3D);
        CORRADE_COMPARE(animation->trackTargetId(2), 12);
        Animation::TrackView<Float, Vector3> scaling = animation->track<Vector3>(2);
        CORRADE_COMPARE(scaling.interpolation(), Animation::Interpolation::Linear);
        CORRADE_COMPARE(scaling.before(), Animation::Extrapolation::Constant);
        CORRADE_COMPARE(scaling.after(), Animation::Extrapolation::Constant);
        const Vector3 scalingData[]{
            Vector3{1.0f},
            Vector3::zScale(5.0f),
            Vector3::zScale(6.0f),
            Vector3(1.0f),
        };
        CORRADE_COMPARE_AS(scaling.keys(), (Containers::StridedArrayView<const Float>{translationScalingKeys}), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scaling.values(), (Containers::StridedArrayView<const Vector3>{scalingData}), TestSuite::Compare::Container);
        CORRADE_COMPARE(scaling.at(1.5f), Vector3::zScale(5.2f));
    }
}

void TinyGltfImporterTest::animationWrongTimeType() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-wrong.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 6);
    CORRADE_COMPARE(importer->animationName(0), "wrong time type");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(0));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::animation(): time track has unexpected type 4/5126\n");
}

void TinyGltfImporterTest::animationWrongInterpolationType() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-wrong.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 6);
    CORRADE_COMPARE(importer->animationName(1), "wrong interpolation type");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(1));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::animation(): unsupported interpolation QUADRATIC\n");
}

void TinyGltfImporterTest::animationWrongTranslationType() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-wrong.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 6);
    CORRADE_COMPARE(importer->animationName(2), "wrong translation type");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(2));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::animation(): translation track has unexpected type 4/5126\n");
}

void TinyGltfImporterTest::animationWrongRotationType() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-wrong.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 6);
    CORRADE_COMPARE(importer->animationName(3), "wrong rotation type");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(3));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::animation(): rotation track has unexpected type 65/5126\n");
}

void TinyGltfImporterTest::animationWrongScalingType() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-wrong.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 6);
    CORRADE_COMPARE(importer->animationName(4), "wrong scaling type");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(4));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::animation(): scaling track has unexpected type 4/5126\n");
}

void TinyGltfImporterTest::animationUnsupportedPath() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-wrong.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 6);
    CORRADE_COMPARE(importer->animationName(5), "unsupported path");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(5));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::animation(): unsupported track target color\n");
}

void TinyGltfImporterTest::camera() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "camera" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->cameraCount(), 2);

    {
        CORRADE_COMPARE(importer->cameraForName("Orthographic"), 0);
        CORRADE_COMPARE(importer->cameraName(0), "Orthographic");

        auto cam = importer->camera(0);
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->near(), 0.1f);
        CORRADE_COMPARE(cam->far(), 100.0f);
    } {
        CORRADE_COMPARE(importer->cameraForName("Perspective"), 1);
        CORRADE_COMPARE(importer->cameraName(1), "Perspective");

        auto cam = importer->camera(1);
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->fov(), 0.5033799372418416_radf);
        CORRADE_COMPARE(cam->near(), 2.0f);
        CORRADE_COMPARE(cam->far(), 94.7f);
    }
}

void TinyGltfImporterTest::light() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "light" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->lightCount(), 4); /* 3 + 1 (ambient light) */

    {
        CORRADE_COMPARE(importer->lightForName("Point"), 0);
        CORRADE_COMPARE(importer->lightName(0), "Point");

        auto light = importer->light(0);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{0.06f, 0.88f, 1.0f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
    } {
        CORRADE_COMPARE(importer->lightForName("Spot"), 1);
        CORRADE_COMPARE(importer->lightName(1), "Spot");

        auto light = importer->light(1);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Spot);
        CORRADE_COMPARE(light->color(), (Color3{0.28f, 0.19f, 1.0f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
    } {
        CORRADE_COMPARE(importer->lightForName("Sun"), 2);
        CORRADE_COMPARE(importer->lightName(2), "Sun");

        auto light = importer->light(2);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Infinite);
        CORRADE_COMPARE(light->color(), (Color3{1.0f, 0.08f, 0.14f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
    }
}

void TinyGltfImporterTest::scene() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "scene" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->defaultScene(), 1);
    CORRADE_COMPARE(importer->sceneCount(), 2);
    CORRADE_COMPARE(importer->sceneName(1), "Scene");
    CORRADE_COMPARE(importer->sceneForName("Scene"), 1);

    auto scene = importer->scene(1);
    CORRADE_VERIFY(scene);
    CORRADE_VERIFY(scene->importerState());
    CORRADE_COMPARE(scene->children3D(), (std::vector<UnsignedInt>{2, 4}));

    CORRADE_COMPARE(importer->object3DCount(), 5);

    {
        CORRADE_COMPARE(importer->object3DName(0), "Camera");
        CORRADE_COMPARE(importer->object3DForName("Camera"), 0);
        auto object = importer->object3D(0);
        CORRADE_VERIFY(object->importerState());
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Camera);
        CORRADE_VERIFY(object->children().empty());
    } {
        CORRADE_COMPARE(importer->object3DName(1), "Empty");
        CORRADE_COMPARE(importer->object3DForName("Empty"), 1);
        auto object = importer->object3D(1);
        CORRADE_VERIFY(object->importerState());
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{0}));
    } {
        CORRADE_COMPARE(importer->object3DName(2), "Mesh");
        CORRADE_COMPARE(importer->object3DForName("Mesh"), 2);
        auto object = importer->object3D(2);
        CORRADE_VERIFY(object->importerState());
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Mesh);
        CORRADE_VERIFY(object->children().empty());
    } {
        CORRADE_COMPARE(importer->object3DName(3), "Light");
        CORRADE_COMPARE(importer->object3DForName("Light"), 3);
        auto object = importer->object3D(3);
        CORRADE_VERIFY(object->importerState());
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Light);
        CORRADE_VERIFY(object->children().empty());
    } {
        CORRADE_COMPARE(importer->object3DName(4), "Empty 2");
        CORRADE_COMPARE(importer->object3DForName("Empty 2"), 4);
        auto object = importer->object3D(4);
        CORRADE_VERIFY(object->importerState());
        CORRADE_COMPARE(object->instanceType(), Trade::ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{3, 1}));
    }
}

void TinyGltfImporterTest::objectTransformation() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "object-transformation" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->object3DCount(), 7);

    {
        CORRADE_COMPARE(importer->object3DName(0), "Matrix");
        auto object = importer->object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->instance(), -1);
        CORRADE_COMPARE(object->flags(), ObjectFlags3D{});
        CORRADE_COMPARE(object->transformation(),
            Matrix4::translation({1.5f, -2.5f, 0.3f})*
            Matrix4::rotationY(45.0_degf)*
            Matrix4::scaling({0.9f, 0.5f, 2.3f}));
        CORRADE_COMPARE(object->transformation(), (Matrix4{
            {0.636397f, 0.0f, -0.636395f, 0.0f},
            {0.0f, 0.5f, -0.0f, 0.0f},
            {1.62634f, 0.0f, 1.62635f, 0.0f},
            {1.5f, -2.5f, 0.3f, 1.0f}
        }));
    } {
        CORRADE_COMPARE(importer->object3DName(1), "TRS");
        auto object = importer->object3D(1);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->instance(), -1);
        CORRADE_COMPARE(object->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(object->transformation(),
            Matrix4::translation({1.5f, -2.5f, 0.3f})*
            Matrix4::rotationY(45.0_degf)*
            Matrix4::scaling({0.9f, 0.5f, 2.3f}));
        CORRADE_COMPARE(object->transformation(), (Matrix4{
            {0.636397f, 0.0f, -0.636395f, 0.0f},
            {0.0f, 0.5f, -0.0f, 0.0f},
            {1.62634f,  0.0f, 1.62635f, 0},
            {1.5f, -2.5f, 0.3f, 1.0f}
        }));
    } {
        CORRADE_COMPARE(importer->object3DName(2), "Mesh matrix");
        auto object = importer->object3D(2);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 0);
        CORRADE_COMPARE(object->flags(), ObjectFlags3D{});
        CORRADE_COMPARE(object->transformation(),
            Matrix4::translation({1.5f, -2.5f, 0.3f})*
            Matrix4::rotationY(45.0_degf)*
            Matrix4::scaling({0.9f, 0.5f, 2.3f}));
        CORRADE_COMPARE(object->transformation(), (Matrix4{
            {0.636397f, 0.0f, -0.636395f, 0.0f},
            {0.0f, 0.5f, -0.0f, 0.0f},
            {1.62634f, 0.0f, 1.62635f, 0.0f},
            {1.5f, -2.5f, 0.3f, 1.0f}
        }));
    } {
        CORRADE_COMPARE(importer->object3DName(3), "Mesh TRS");
        auto object = importer->object3D(3);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 0);
        CORRADE_COMPARE(object->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(object->transformation(),
            Matrix4::translation({1.5f, -2.5f, 0.3f})*
            Matrix4::rotationY(45.0_degf)*
            Matrix4::scaling({0.9f, 0.5f, 2.3f}));
        CORRADE_COMPARE(object->transformation(), (Matrix4{
            {0.636397f, 0.0f, -0.636395f, 0.0f},
            {0.0f, 0.5f, -0.0f, 0.0f},
            {1.62634f,  0.0f, 1.62635f, 0},
            {1.5f, -2.5f, 0.3f, 1.0f}
        }));
    } {
        CORRADE_COMPARE(importer->object3DName(4), "Translation");
        auto object = importer->object3D(4);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->instance(), -1);
        CORRADE_COMPARE(object->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(object->translation(), (Vector3{1.5f, -2.5f, 0.3f}));
        CORRADE_COMPARE(object->rotation(), Quaternion{});
        CORRADE_COMPARE(object->scaling(), Vector3{1.0f});
        CORRADE_COMPARE(object->transformation(), Matrix4::translation({1.5f, -2.5f, 0.3f}));
    } {
        CORRADE_COMPARE(importer->object3DName(5), "Rotation");
        auto object = importer->object3D(5);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->instance(), -1);
        CORRADE_COMPARE(object->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(object->rotation(), Quaternion::rotation(45.0_degf, Vector3::yAxis()));
        CORRADE_COMPARE(object->scaling(), Vector3{1.0f});
        CORRADE_COMPARE(object->transformation(), Matrix4::rotationY(45.0_degf));
    } {
        CORRADE_COMPARE(importer->object3DName(6), "Scaling");
        auto object = importer->object3D(6);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->instance(), -1);
        CORRADE_COMPARE(object->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(object->translation(), Vector3{});
        CORRADE_COMPARE(object->rotation(), Quaternion{});
        CORRADE_COMPARE(object->scaling(), (Vector3{0.9f, 0.5f, 2.3f}));
        CORRADE_COMPARE(object->transformation(), Matrix4::scaling({0.9f, 0.5f, 2.3f}));
    }
}

void TinyGltfImporterTest::mesh() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->mesh3DCount(), 3);
    CORRADE_COMPARE(importer->mesh3DName(0), "Non-indexed mesh");
    CORRADE_COMPARE(importer->mesh3DForName("Non-indexed mesh"), 0);

    auto mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->importerState());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->normalArrayCount(), 0);

    CORRADE_COMPARE_AS(mesh->positions(0), (std::vector<Vector3>{
        {1.5f, -1.0f, -0.5f},
        {-0.5f, 2.5f, 0.75f},
        {-2.0f, 1.0f, 0.3f}
    }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::meshIndexed() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->mesh3DCount(), 3);
    CORRADE_COMPARE(importer->mesh3DName(1), "Indexed mesh");
    CORRADE_COMPARE(importer->mesh3DForName("Indexed mesh"), 1);

    auto meshObject = importer->mesh3D(1);
    CORRADE_VERIFY(meshObject);
    CORRADE_VERIFY(meshObject->importerState());
    CORRADE_COMPARE(meshObject->primitive(), MeshPrimitive::Triangles);
    CORRADE_VERIFY(meshObject->isIndexed());
    CORRADE_COMPARE(meshObject->positionArrayCount(), 1);
    CORRADE_COMPARE(meshObject->normalArrayCount(), 1);

    CORRADE_COMPARE_AS(meshObject->positions(0), (std::vector<Vector3>{
        {1.5f, -1.0f, -0.5f},
        {-0.5f, 2.5f, 0.75f},
        {-2.0f, 1.0f, 0.3f}
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE(meshObject->normalArrayCount(), 1);
    CORRADE_COMPARE_AS(meshObject->normals(0), (std::vector<Vector3>{
        {0.1f, 0.2f, 0.3f},
        {0.4f, 0.5f, 0.6f},
        {0.7f, 0.8f, 0.9f}
    }), TestSuite::Compare::Container);

    CORRADE_COMPARE(meshObject->indices(), (std::vector<UnsignedInt>{0, 1, 2}));
}

void TinyGltfImporterTest::meshUnknownAttribute() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->mesh3DCount(), 3);
    CORRADE_COMPARE(importer->mesh3DName(2), "Mesh with unknown attribute");
    CORRADE_COMPARE(importer->mesh3DForName("Mesh with unknown attribute"), 2);

    std::ostringstream out;
    Warning redirectWarning{&out};

    auto meshObject = importer->mesh3D(2);

    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::mesh3D(): unsupported mesh vertex attribute UNKNOWN\n");

    CORRADE_VERIFY(meshObject);
    CORRADE_VERIFY(meshObject->importerState());
    CORRADE_COMPARE(meshObject->primitive(), MeshPrimitive::Triangles);
    CORRADE_COMPARE(meshObject->positionArrayCount(), 1);
    CORRADE_COMPARE(meshObject->normalArrayCount(), 0);

    CORRADE_COMPARE_AS(meshObject->positions(0), (std::vector<Vector3>{
        {1.5f, -1.0f, -0.5f},
        {-0.5f, 2.5f, 0.75f},
        {-2.0f, 1.0f, 0.3f}
    }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::meshPrimitives() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-primitives" + std::string{data.suffix})));

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
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-colors" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->mesh3DCount(), 1);

    auto mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->positions(0), (std::vector<Vector3>{
        {1.5f, -1.0f, -0.5f},
        {-0.5f, 2.5f, 0.75f},
        {-2.0f, 1.0f, 0.3f}
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->colorArrayCount(), 2);
    CORRADE_COMPARE_AS(mesh->colors(0), (std::vector<Color4>{
        {0.1f, 0.2f, 0.3f, 1.0f},
        {0.4f, 0.5f, 0.6f, 1.0f},
        {0.7f, 0.8f, 0.9f, 1.0f}
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(mesh->colors(1), (std::vector<Color4>{
        {0.1f, 0.2f, 0.3f, 0.4f},
        {0.5f, 0.6f, 0.7f, 0.8f},
        {0.9f, 1.0f, 1.1f, 1.2f}
    }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::meshWithStride() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-with-stride" + std::string{data.suffix})));

    std::ostringstream out;
    Error redirectError{&out};

    /* First has a stride of 12 bytes (no interleaving), shouldn't fail */
    CORRADE_VERIFY(importer->mesh3D(0));

    /* Second has a stride of 24 bytes, should fail */
    {
        CORRADE_EXPECT_FAIL("Tnterleaved buffer views are not supported.");
        CORRADE_VERIFY(importer->mesh3D(1));
    }
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::mesh3D(): interleaved buffer views are not supported\n");
}

void TinyGltfImporterTest::materialPbrMetallicRoughness() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-metallicroughness" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->materialCount(), 2);

    {
        auto material = importer->material(0);
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->importerState());
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
        CORRADE_VERIFY(material->importerState());
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
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-specularglossiness" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->materialCount(), 2);

    {
        auto material = importer->material(0);
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->importerState());
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
        CORRADE_VERIFY(material->importerState());
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
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-blinnphong" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->materialCount(), 2);

    {
        auto material = importer->material(0);
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->importerState());
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
        CORRADE_VERIFY(material->importerState());
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
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->materialCount(), 1);

    auto material = importer->material(0);

    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

    auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
    CORRADE_VERIFY(phong.flags() & PhongMaterialData::Flag::DiffuseTexture);
    CORRADE_COMPARE(phong.diffuseTexture(), 0);
    CORRADE_COMPARE(phong.shininess(), 1.0);

    CORRADE_COMPARE(importer->textureCount(), 2);
    CORRADE_COMPARE(importer->textureForName("Texture"), 1);
    CORRADE_COMPARE(importer->textureName(1), "Texture");

    auto texture = importer->texture(1);
    CORRADE_VERIFY(texture);
    CORRADE_VERIFY(texture->importerState());
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
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture-default-sampler" + std::string{data.suffix})));

    auto texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->type(), TextureData::Type::Texture2D);

    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Linear);

    CORRADE_COMPARE(texture->wrapping(), Array3D<SamplerWrapping>(SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping::Repeat));
}

namespace {
    constexpr char ExpectedImageData[] =
        "\xa8\xa7\xac\xff\x9d\x9e\xa0\xff\xad\xad\xac\xff\xbb\xbb\xba\xff\xb3\xb4\xb6\xff"
        "\xb0\xb1\xb6\xff\xa0\xa0\xa1\xff\x9f\x9f\xa0\xff\xbc\xbc\xba\xff\xcc\xcc\xcc\xff"
        "\xb2\xb4\xb9\xff\xb8\xb9\xbb\xff\xc1\xc3\xc2\xff\xbc\xbd\xbf\xff\xb8\xb8\xbc\xff";
}

void TinyGltfImporterTest::image() {
    auto&& data = ImageData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "image" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_COMPARE(importer->image2DForName("Image"), 1);
    CORRADE_COMPARE(importer->image2DName(1), "Image");

    auto image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->importerState());
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(ExpectedImageData).prefix(60), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::fileCallbackBuffer() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->features() & AbstractImporter::Feature::FileCallback);

    Utility::Resource rs{"data"};
    importer->setFileCallback([](const std::string& filename, ImporterFileCallbackPolicy policy, Utility::Resource& rs) {
        Debug{} << "Loading" << filename << "with" << policy;
        return rs.getRaw(filename);
    }, rs);

    /* Using a different name from the filesystem to avoid false positive
       when the file gets loaded from a filesystem */
    CORRADE_VERIFY(importer->openFile("data" + std::string{data.suffix}));

    CORRADE_COMPARE(importer->mesh3DCount(), 1);
    auto mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Points);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->positionArrayCount(), 1);
    CORRADE_COMPARE(mesh->normalArrayCount(), 0);

    CORRADE_COMPARE_AS(mesh->positions(0), (std::vector<Vector3>{
        {1.0f, 2.0f, 3.0f}
    }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::fileCallbackImage() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->features() & AbstractImporter::Feature::FileCallback);

    Utility::Resource rs{"data"};
    importer->setFileCallback([](const std::string& filename, ImporterFileCallbackPolicy  policy, Utility::Resource& rs) {
        Debug{} << "Loading" << filename << "with" << policy;
        return rs.getRaw(filename);
    }, rs);

    /* Using a different name from the filesystem to avoid false positive
       when the file gets loaded from a filesystem */
    CORRADE_VERIFY(importer->openFile("data" + std::string{data.suffix}));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    auto image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(ExpectedImageData).prefix(60), TestSuite::Compare::Container);
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::TinyGltfImporterTest)
