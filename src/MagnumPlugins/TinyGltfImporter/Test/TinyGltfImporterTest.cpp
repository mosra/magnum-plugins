/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
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
#include <Corrade/Containers/Optional.h>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/Array.h>
#include <Magnum/FileCallback.h>
#include <Magnum/Mesh.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/CubicHermite.h>
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

namespace Magnum { namespace Trade { namespace Test { namespace {

struct TinyGltfImporterTest: TestSuite::Tester {
    explicit TinyGltfImporterTest();

    void open();
    void openError();
    void openExternalDataNotFound();
    void openExternalDataNoPathNoCallback();

    void animation();
    void animationWrongTimeType();
    void animationWrongInterpolationType();
    void animationWrongTranslationType();
    void animationWrongRotationType();
    void animationWrongScalingType();
    void animationUnsupportedPath();

    void animationSpline();
    void animationSplineSharedWithSameTimeTrack();
    void animationSplineSharedWithDifferentTimeTrack();

    void animationShortestPathOptimizationEnabled();
    void animationShortestPathOptimizationDisabled();
    void animationQuaternionNormalizationEnabled();
    void animationQuaternionNormalizationDisabled();
    void animationMergeEmpty();
    void animationMerge();

    void camera();

    void light();

    void scene();
    void sceneEmpty();
    void sceneNoDefault();
    void objectTransformation();

    void objectTransformationQuaternionNormalizationEnabled();
    void objectTransformationQuaternionNormalizationDisabled();

    void mesh();
    void meshIndexed();
    void meshUnknownAttribute();
    void meshPrimitives();
    void meshColors();
    void meshWithStride();

    void meshMultiplePrimitives();

    void materialPbrMetallicRoughness();
    void materialPbrSpecularGlossiness();
    void materialBlinnPhong();
    void materialProperties();

    void texture();
    void textureDefaultSampler();
    void textureMissingSource();

    void imageEmbedded();
    void imageExternal();
    void imageExternalNotFound();
    void imageExternalNoPathNoCallback();

    void imageBasis();

    void fileCallbackBuffer();
    void fileCallbackBufferNotFound();
    void fileCallbackImage();
    void fileCallbackImageNotFound();

    void utf8filenames();

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

/* The external-data.* files are packed in via a resource, filename mapping
   done in resources.conf */

constexpr struct {
    const char* name;
    const char* suffix;
    Containers::ArrayView<const char> shortData;
    const char* shortDataError;
} OpenErrorData[]{
    {"ascii", ".gltf", {"?", 1}, "JSON string too short.\n"},
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
} ImageEmbeddedData[]{
    {"ascii", "-embedded.gltf"},
    {"ascii buffer", "-buffer-embedded.gltf"},
    {"binary", "-embedded.glb"},
    {"binary buffer", "-buffer-embedded.glb"}
};

constexpr struct {
    const char* name;
    const char* suffix;
} ImageExternalData[]{
    {"ascii", ".gltf"},
    {"ascii buffer", "-buffer.gltf"},
    {"binary", ".glb"},
    {"binary buffer", "-buffer.glb"},
};

constexpr struct {
    const char* name;
    const char* suffix;
} ImageBasisData[]{
    {"ascii", ".gltf"},
    {"binary", ".glb"},
    {"embedded ascii", "-embedded.gltf"},
    {"embedded binary", "-embedded.glb"},
};

using namespace Magnum::Math::Literals;

TinyGltfImporterTest::TinyGltfImporterTest() {
    addInstancedTests({&TinyGltfImporterTest::open},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&TinyGltfImporterTest::openError},
                      Containers::arraySize(OpenErrorData));

    addInstancedTests({&TinyGltfImporterTest::openExternalDataNotFound,
                       &TinyGltfImporterTest::openExternalDataNoPathNoCallback},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&TinyGltfImporterTest::animation},
                      Containers::arraySize(MultiFileData));

    addTests({&TinyGltfImporterTest::animationWrongTimeType,
              &TinyGltfImporterTest::animationWrongInterpolationType,
              &TinyGltfImporterTest::animationWrongTranslationType,
              &TinyGltfImporterTest::animationWrongRotationType,
              &TinyGltfImporterTest::animationWrongScalingType,
              &TinyGltfImporterTest::animationUnsupportedPath});

    addInstancedTests({&TinyGltfImporterTest::animationSpline},
                      Containers::arraySize(MultiFileData));

    addTests({&TinyGltfImporterTest::animationSplineSharedWithSameTimeTrack,
              &TinyGltfImporterTest::animationSplineSharedWithDifferentTimeTrack,

              &TinyGltfImporterTest::animationShortestPathOptimizationEnabled,
              &TinyGltfImporterTest::animationShortestPathOptimizationDisabled,
              &TinyGltfImporterTest::animationQuaternionNormalizationEnabled,
              &TinyGltfImporterTest::animationQuaternionNormalizationDisabled,
              &TinyGltfImporterTest::animationMergeEmpty,
              &TinyGltfImporterTest::animationMerge});

    addInstancedTests({&TinyGltfImporterTest::camera,

                       &TinyGltfImporterTest::light,

                       &TinyGltfImporterTest::scene,
                       &TinyGltfImporterTest::sceneEmpty,
                       &TinyGltfImporterTest::sceneNoDefault,
                       &TinyGltfImporterTest::objectTransformation},
                      Containers::arraySize(SingleFileData));

    addTests({&TinyGltfImporterTest::objectTransformationQuaternionNormalizationEnabled,
              &TinyGltfImporterTest::objectTransformationQuaternionNormalizationDisabled});

    addInstancedTests({&TinyGltfImporterTest::mesh,
                       &TinyGltfImporterTest::meshIndexed,
                       &TinyGltfImporterTest::meshUnknownAttribute,
                       &TinyGltfImporterTest::meshPrimitives,
                       &TinyGltfImporterTest::meshColors},
                      Containers::arraySize(MultiFileData));

    /* There are no external data for this one at the moment */
    addInstancedTests({&TinyGltfImporterTest::meshWithStride},
                      Containers::arraySize(SingleFileData));

    addTests({&TinyGltfImporterTest::meshMultiplePrimitives});

    addInstancedTests({&TinyGltfImporterTest::materialPbrMetallicRoughness,
                       &TinyGltfImporterTest::materialPbrSpecularGlossiness,
                       &TinyGltfImporterTest::materialBlinnPhong,
                       &TinyGltfImporterTest::materialProperties,

                       &TinyGltfImporterTest::texture,
                       &TinyGltfImporterTest::textureDefaultSampler},
                      Containers::arraySize(SingleFileData));

    addTests({&TinyGltfImporterTest::textureMissingSource});

    addInstancedTests({&TinyGltfImporterTest::imageEmbedded},
                      Containers::arraySize(ImageEmbeddedData));

    addInstancedTests({&TinyGltfImporterTest::imageExternal},
                      Containers::arraySize(ImageExternalData));

    addTests({&TinyGltfImporterTest::imageExternalNotFound,
              &TinyGltfImporterTest::imageExternalNoPathNoCallback});

    addInstancedTests({&TinyGltfImporterTest::imageBasis},
                      Containers::arraySize(ImageBasisData));

    addInstancedTests({&TinyGltfImporterTest::fileCallbackBuffer,
                       &TinyGltfImporterTest::fileCallbackBufferNotFound,
                       &TinyGltfImporterTest::fileCallbackImage,
                       &TinyGltfImporterTest::fileCallbackImageNotFound},
                      Containers::arraySize(SingleFileData));

    addTests({&TinyGltfImporterTest::utf8filenames});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. Reset
       the plugin dir after so it doesn't load anything else from the filesystem. */
    #ifdef TINYGLTFIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(TINYGLTFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    _manager.setPluginDirectory({});
    #endif
    #ifdef BASISIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(BASISIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void TinyGltfImporterTest::open() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

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

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(!importer->openData(data.shortData));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: " + std::string{data.shortDataError});
}

void TinyGltfImporterTest::openExternalDataNotFound() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    auto filename = Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "buffer-notfound" + std::string{data.suffix});

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openFile(filename));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: File read error : /nonexistent.bin : file not found\n");
}

void TinyGltfImporterTest::openExternalDataNoPathNoCallback() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    auto filename = Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "buffer-notfound" + std::string{data.suffix});

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openData(Utility::Directory::read(filename)));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: File read error : /nonexistent.bin : external buffers can be imported only when opening files from the filesystem or if a file callback is present\n");
}

void TinyGltfImporterTest::animation() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->animationCount(), 3);

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
        CORRADE_COMPARE(animation->trackTargetType(0), AnimationTrackTargetType::Rotation3D);
        CORRADE_COMPARE(animation->trackTarget(0), 0);
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
        CORRADE_COMPARE_AS(rotation.keys(), (Containers::StridedArrayView1D<const Float>{rotationKeys}), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(rotation.values(), (Containers::StridedArrayView1D<const Quaternion>{rotationValues}), TestSuite::Compare::Container);
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
        CORRADE_COMPARE(animation->trackTargetType(1), AnimationTrackTargetType::Translation3D);
        CORRADE_COMPARE(animation->trackTarget(1), 1);
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
        CORRADE_COMPARE_AS(translation.keys(), (Containers::StridedArrayView1D<const Float>{translationScalingKeys}), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(translation.values(), (Containers::StridedArrayView1D<const Vector3>{translationData}), TestSuite::Compare::Container);
        CORRADE_COMPARE(translation.at(1.5f), Vector3::yAxis(2.5f));

        /* Scaling, linearly interpolated, sharing keys with translation */
        CORRADE_COMPARE(animation->trackType(2), AnimationTrackType::Vector3);
        CORRADE_COMPARE(animation->trackResultType(2), AnimationTrackType::Vector3);
        CORRADE_COMPARE(animation->trackTargetType(2), AnimationTrackTargetType::Scaling3D);
        CORRADE_COMPARE(animation->trackTarget(2), 2);
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
        CORRADE_COMPARE_AS(scaling.keys(), (Containers::StridedArrayView1D<const Float>{translationScalingKeys}), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scaling.values(), (Containers::StridedArrayView1D<const Vector3>{scalingData}), TestSuite::Compare::Container);
        CORRADE_COMPARE(scaling.at(1.5f), Vector3::zScale(5.2f));
    }
}

void TinyGltfImporterTest::animationWrongTimeType() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-wrong.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 6);
    CORRADE_COMPARE(importer->animationName(5), "unsupported path");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(5));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::animation(): unsupported track target color\n");
}

constexpr Float AnimationSplineTime1Keys[]{ 0.5f, 3.5f, 4.0f, 5.0f };

constexpr CubicHermite3D AnimationSplineTime1TranslationData[]{
    {{0.0f, 0.0f, 0.0f},
     {3.0f, 0.1f, 2.5f},
     {-1.0f, 0.0f, 0.3f}},
    {{5.0f, 0.3f, 1.1f},
     {-2.0f, 1.1f, -4.3f},
     {1.5f, 0.3f, 17.0f}},
    {{1.3f, 0.0f, 0.2f},
     {1.5f, 9.8f, -5.1f},
     {0.1f, 0.2f, -7.1f}},
    {{1.3f, 0.5f, 1.0f},
     {5.1f, 0.1f, -7.3f},
     {0.0f, 0.0f, 0.0f}}
};

void TinyGltfImporterTest::animationSpline() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation" + std::string{data.suffix})));
    CORRADE_COMPARE(importer->animationCount(), 3);
    CORRADE_COMPARE(importer->animationName(2), "TRS animation, splines");

    auto animation = importer->animation(2);
    CORRADE_VERIFY(animation);
    CORRADE_VERIFY(animation->importerState());
    /* Four spline T/R/S keys with one common time track */
    CORRADE_COMPARE(animation->data().size(),
        4*(sizeof(Float) + 3*sizeof(Quaternion) + 2*3*sizeof(Vector3)));
    CORRADE_COMPARE(animation->trackCount(), 3);

    /* Rotation */
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::CubicHermiteQuaternion);
    CORRADE_COMPARE(animation->trackResultType(0), AnimationTrackType::Quaternion);
    CORRADE_COMPARE(animation->trackTargetType(0), AnimationTrackTargetType::Rotation3D);
    CORRADE_COMPARE(animation->trackTarget(0), 3);
    Animation::TrackView<Float, CubicHermiteQuaternion> rotation = animation->track<CubicHermiteQuaternion>(0);
    CORRADE_COMPARE(rotation.interpolation(), Animation::Interpolation::Spline);
    CORRADE_COMPARE(rotation.before(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE(rotation.after(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE_AS(rotation.keys(), (Containers::StridedArrayView1D<const Float>{AnimationSplineTime1Keys}), TestSuite::Compare::Container);
    constexpr CubicHermiteQuaternion rotationValues[]{
        {{{0.0f, 0.0f, 0.0f}, 0.0f},
         {{0.780076f, 0.0260025f, 0.598059f}, 0.182018f},
         {{-1.0f, 0.0f, 0.3f}, 0.4f}},
        {{{5.0f, 0.3f, 1.1f}, 0.5f},
         {{-0.711568f, 0.391362f, 0.355784f}, 0.462519f},
         {{1.5f, 0.3f, 17.0f}, -7.0f}},
        {{{1.3f, 0.0f, 0.2f}, 1.2f},
         {{0.598059f, 0.182018f, 0.0260025f}, 0.780076f},
         {{0.1f, 0.2f, -7.1f}, 1.7f}},
        {{{1.3f, 0.5f, 1.0f}, 0.0f},
         {{0.711568f, -0.355784f, -0.462519f}, -0.391362f},
         {{0.0f, 0.0f, 0.0f}, 0.0f}}
    };
    CORRADE_COMPARE_AS(rotation.values(), (Containers::StridedArrayView1D<const CubicHermiteQuaternion>{rotationValues}), TestSuite::Compare::Container);
    /* The same as in CubicHermiteTest::splerpQuaternion() */
    CORRADE_COMPARE(rotation.at(0.5f + 0.35f*3),
        (Quaternion{{-0.309862f, 0.174831f, 0.809747f}, 0.466615f}));

    /* Translation */
    CORRADE_COMPARE(animation->trackType(1), AnimationTrackType::CubicHermite3D);
    CORRADE_COMPARE(animation->trackResultType(1), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(1), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(1), 4);
    Animation::TrackView<Float, CubicHermite3D> translation = animation->track<CubicHermite3D>(1);
    CORRADE_COMPARE(translation.interpolation(), Animation::Interpolation::Spline);
    CORRADE_COMPARE(translation.before(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE(translation.after(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE_AS(translation.keys(), (Containers::StridedArrayView1D<const Float>{AnimationSplineTime1Keys}), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(translation.values(), (Containers::StridedArrayView1D<const CubicHermite3D>{AnimationSplineTime1TranslationData}), TestSuite::Compare::Container);
    /* The same as in CubicHermiteTest::splerpVector() */
    CORRADE_COMPARE(translation.at(0.5f + 0.35f*3),
        (Vector3{1.04525f, 0.357862f, 0.540875f}));

    /* Scaling */
    CORRADE_COMPARE(animation->trackType(2), AnimationTrackType::CubicHermite3D);
    CORRADE_COMPARE(animation->trackResultType(2), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(2), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(2), 5);
    Animation::TrackView<Float, CubicHermite3D> scaling = animation->track<CubicHermite3D>(2);
    CORRADE_COMPARE(scaling.interpolation(), Animation::Interpolation::Spline);
    CORRADE_COMPARE(scaling.before(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE(scaling.after(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE_AS(scaling.keys(), (Containers::StridedArrayView1D<const Float>{AnimationSplineTime1Keys}), TestSuite::Compare::Container);
    constexpr CubicHermite3D scalingData[]{
        {{0.0f, 0.0f, 0.0f},
         {-2.0f, 1.1f, -4.3f},
         {1.5f, 0.3f, 17.0f}},
        {{1.3f, 0.5f, 1.0f},
         {5.1f, 0.1f, -7.3f},
         {-1.0f, 0.0f, 0.3f}},
        {{0.1f, 0.2f, -7.1f},
         {3.0f, 0.1f, 2.5f},
         {5.0f, 0.3f, 1.1f}},
        {{1.3f, 0.0f, 0.2f},
         {1.5f, 9.8f, -5.1f},
         {0.0f, 0.0f, 0.0f}}
    };
    CORRADE_COMPARE_AS(scaling.values(), (Containers::StridedArrayView1D<const CubicHermite3D>{scalingData}), TestSuite::Compare::Container);
    CORRADE_COMPARE(scaling.at(0.5f + 0.35f*3),
        (Vector3{0.118725f, 0.8228f, -2.711f}));
}

void TinyGltfImporterTest::animationSplineSharedWithSameTimeTrack() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-splines-sharing.gltf")));
    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(0), "TRS animation, splines, sharing data with the same time track");

    auto animation = importer->animation(0);
    CORRADE_VERIFY(animation);
    CORRADE_VERIFY(animation->importerState());
    /* Four spline T keys with one common time track, used as S as well */
    CORRADE_COMPARE(animation->data().size(),
        4*(sizeof(Float) + 3*sizeof(Vector3)));
    CORRADE_COMPARE(animation->trackCount(), 2);

    /* Translation using the translation track and the first time track */
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::CubicHermite3D);
    CORRADE_COMPARE(animation->trackResultType(0), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(0), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(0), 0);
    Animation::TrackView<Float, CubicHermite3D> translation = animation->track<CubicHermite3D>(1);
    CORRADE_COMPARE(translation.interpolation(), Animation::Interpolation::Spline);
    CORRADE_COMPARE(translation.before(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE(translation.after(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE_AS(translation.keys(), (Containers::StridedArrayView1D<const Float>{AnimationSplineTime1Keys}), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(translation.values(), (Containers::StridedArrayView1D<const CubicHermite3D>{AnimationSplineTime1TranslationData}), TestSuite::Compare::Container);
    /* The same as in CubicHermiteTest::splerpVector() */
    CORRADE_COMPARE(translation.at(0.5f + 0.35f*3),
        (Vector3{1.04525f, 0.357862f, 0.540875f}));

    /* Scaling also using the translation track and the first time track. Yes,
       it's weird, but a viable test case verifying the same key/value data
       pair used in two different tracks. The imported data should be
       absolutely the same, not processed twice or anything. */
    CORRADE_COMPARE(animation->trackType(1), AnimationTrackType::CubicHermite3D);
    CORRADE_COMPARE(animation->trackResultType(1), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(1), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(1), 0);
    Animation::TrackView<Float, CubicHermite3D> scaling = animation->track<CubicHermite3D>(1);
    CORRADE_COMPARE(scaling.interpolation(), Animation::Interpolation::Spline);
    CORRADE_COMPARE(scaling.before(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE(scaling.after(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE_AS(scaling.keys(), (Containers::StridedArrayView1D<const Float>{AnimationSplineTime1Keys}), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scaling.values(), (Containers::StridedArrayView1D<const CubicHermite3D>{AnimationSplineTime1TranslationData}), TestSuite::Compare::Container);
    /* The same as in CubicHermiteTest::splerpVector() */
    CORRADE_COMPARE(scaling.at(0.5f + 0.35f*3),
        (Vector3{1.04525f, 0.357862f, 0.540875f}));
}

void TinyGltfImporterTest::animationSplineSharedWithDifferentTimeTrack() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-splines-sharing.gltf")));
    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(1), "TRS animation, splines, sharing data with different time track");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(1));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::animation(): spline track is shared with different time tracks, we don't support that, sorry\n");
}

void TinyGltfImporterTest::animationShortestPathOptimizationEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("optimizeQuaternionShortestPath"));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-patching.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(0), "Quaternion shortest-path patching");

    auto animation = importer->animation(0);
    CORRADE_VERIFY(animation);
    CORRADE_COMPARE(animation->trackCount(), 1);
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::Quaternion);
    Animation::TrackView<Float, Quaternion> track = animation->track<Quaternion>(0);
    const Quaternion rotationValues[]{
        {{0.0f, 0.0f, 0.92388f}, -0.382683f},   // 0 s: 225°
        {{0.0f, 0.0f, 0.707107f}, -0.707107f},  // 1 s: 270°
        {{0.0f, 0.0f, 0.382683f}, -0.92388f},   // 2 s: 315°
        {{0.0f, 0.0f, 0.0f}, -1.0f},            // 3 s: 360° / 0°
        {{0.0f, 0.0f, -0.382683f}, -0.92388f},  // 4 s:  45° (flipped)
        {{0.0f, 0.0f, -0.707107f}, -0.707107f}, // 5 s:  90° (flipped)
        {{0.0f, 0.0f, -0.92388f}, -0.382683f},  // 6 s: 135° (flipped back)
        {{0.0f, 0.0f, -1.0f}, 0.0f},            // 7 s: 180° (flipped back)
        {{0.0f, 0.0f, -0.92388f}, 0.382683f}    // 8 s: 225° (flipped)
    };
    CORRADE_COMPARE_AS(track.values(), (Containers::StridedArrayView1D<const Quaternion>{rotationValues}), TestSuite::Compare::Container);

    CORRADE_COMPARE(track.at(Math::slerp, 0.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 1.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 2.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 3.5f).axis(), -Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 4.5f).axis(), -Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 5.5f).axis(), -Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 6.5f).axis(), -Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 7.5f).axis(), -Vector3::zAxis());

    /* Some are negated because of the flipped axis but other than that it's
       nicely monotonic */
    CORRADE_COMPARE(track.at(Math::slerp, 0.5f).angle(), 247.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 1.5f).angle(), 292.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 2.5f).angle(), 337.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 3.5f).angle(), 360.0_degf - 22.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 4.5f).angle(), 360.0_degf - 67.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 5.5f).angle(), 360.0_degf - 112.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 6.5f).angle(), 360.0_degf - 157.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 7.5f).angle(), 360.0_degf - 202.5_degf);
}

void TinyGltfImporterTest::animationShortestPathOptimizationDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Explicitly disable */
    importer->configuration().setValue("optimizeQuaternionShortestPath", false);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-patching.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(0), "Quaternion shortest-path patching");

    auto animation = importer->animation(0);
    CORRADE_VERIFY(animation);
    CORRADE_COMPARE(animation->trackCount(), 1);
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::Quaternion);
    Animation::TrackView<Float, Quaternion> track = animation->track<Quaternion>(0);

    /* Should be the same as in animation-patching.bin.in */
    const Quaternion rotationValues[]{
        {{0.0f, 0.0f, 0.92388f}, -0.382683f},   // 0 s: 225°
        {{0.0f, 0.0f, 0.707107f}, -0.707107f},  // 1 s: 270°
        {{0.0f, 0.0f, 0.382683f}, -0.92388f},   // 2 s: 315°
        {{0.0f, 0.0f, 0.0f}, -1.0f},            // 3 s: 360° / 0°
        {{0.0f, 0.0f, 0.382683f}, 0.92388f},    // 4 s:  45° (longer path)
        {{0.0f, 0.0f, 0.707107f}, 0.707107f},   // 5 s:  90°
        {{0.0f, 0.0f, -0.92388f}, -0.382683f},  // 6 s: 135° (longer path)
        {{0.0f, 0.0f, -1.0f}, 0.0f},            // 7 s: 180°
        {{0.0f, 0.0f, 0.92388f}, -0.382683f}    // 8 s: 225° (longer path)
    };
    CORRADE_COMPARE_AS(track.values(), (Containers::StridedArrayView1D<const Quaternion>{rotationValues}), TestSuite::Compare::Container);

    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 0.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 1.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 2.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 3.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 4.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 5.5f).axis(), -Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 6.5f).axis(), -Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 7.5f).axis(), Vector3::zAxis());

    /* Some are negated because of the flipped axis but other than that it's
       nicely monotonic because slerpShortestPath() ensures that */
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 0.5f).angle(), 247.5_degf);
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 1.5f).angle(), 292.5_degf);
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 2.5f).angle(), 337.5_degf);
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 3.5f).angle(), 22.5_degf);
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 4.5f).angle(), 67.5_degf);
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 5.5f).angle(), 360.0_degf - 112.5_degf);
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 6.5f).angle(), 360.0_degf - 157.5_degf);
    CORRADE_COMPARE(track.at(Math::slerpShortestPath, 7.5f).angle(), 202.5_degf);

    CORRADE_COMPARE(track.at(Math::slerp, 0.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 1.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 2.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 3.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 4.5f).axis(), Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 5.5f).axis(), -Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 6.5f).axis(), -Vector3::zAxis());
    CORRADE_COMPARE(track.at(Math::slerp, 7.5f).axis(), -Vector3::zAxis(1.00004f)); /* ?! */

    /* Things are a complete chaos when using non-SP slerp */
    CORRADE_COMPARE(track.at(Math::slerp, 0.5f).angle(), 247.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 1.5f).angle(), 292.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 2.5f).angle(), 337.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 3.5f).angle(), 202.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 4.5f).angle(), 67.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 5.5f).angle(), 67.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 6.5f).angle(), 202.5_degf);
    CORRADE_COMPARE(track.at(Math::slerp, 7.5f).angle(), 337.5_degf);
}

void TinyGltfImporterTest::animationQuaternionNormalizationEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("normalizeQuaternions"));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-patching.gltf")));
    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(1), "Quaternion normalization patching");

    Containers::Optional<Trade::AnimationData> animation;
    std::ostringstream out;
    {
        Warning warningRedirection{&out};
        animation = importer->animation(1);
    }
    CORRADE_VERIFY(animation);
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::animation(): quaternions in some rotation tracks were renormalized\n");
    CORRADE_COMPARE(animation->trackCount(), 1);
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::Quaternion);

    Animation::TrackView<Float, Quaternion> track = animation->track<Quaternion>(0);
    const Quaternion rotationValues[]{
        {{0.0f, 0.0f, 0.382683f}, 0.92388f},    // is normalized
        {{0.0f, 0.0f, 0.707107f}, 0.707107f},   // is not, renormalized
        {{0.0f, 0.0f, 0.382683f}, 0.92388f},    // is not, renormalized
    };
    CORRADE_COMPARE_AS(track.values(), (Containers::StridedArrayView1D<const Quaternion>{rotationValues}), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::animationQuaternionNormalizationDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Explicitly disable */
    CORRADE_VERIFY(importer->configuration().setValue("normalizeQuaternions", false));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-patching.gltf")));
    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(1), "Quaternion normalization patching");

    auto animation = importer->animation(1);
    CORRADE_VERIFY(animation);
    CORRADE_COMPARE(animation->trackCount(), 1);
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::Quaternion);

    Animation::TrackView<Float, Quaternion> track = animation->track<Quaternion>(0);
    const Quaternion rotationValues[]{
        Quaternion{{0.0f, 0.0f, 0.382683f}, 0.92388f},      // is normalized
        Quaternion{{0.0f, 0.0f, 0.707107f}, 0.707107f}*2,   // is not
        Quaternion{{0.0f, 0.0f, 0.382683f}, 0.92388f}*2,    // is not
    };
    CORRADE_COMPARE_AS(track.values(), (Containers::StridedArrayView1D<const Quaternion>{rotationValues}), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::animationMergeEmpty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Enable animation merging */
    importer->configuration().setValue("mergeAnimationClips", true);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "empty.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->animationForName(""), -1);
}

void TinyGltfImporterTest::animationMerge() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Enable animation merging */
    importer->configuration().setValue("mergeAnimationClips", true);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 1);
    CORRADE_COMPARE(importer->animationName(0), "");
    CORRADE_COMPARE(importer->animationForName(""), -1);

    auto animation = importer->animation(0);
    CORRADE_VERIFY(animation);
    CORRADE_VERIFY(!animation->importerState()); /* No particular clip */
    /*
        -   Nothing from the first animation
        -   Two rotation keys, four translation and scaling keys with common
            time track from the second animation
        -   Four T/R/S spline-interpolated keys with a common time tracks
            from the third animation
    */
    CORRADE_COMPARE(animation->data().size(),
        2*(sizeof(Float) + sizeof(Quaternion)) +
        4*(sizeof(Float) + 2*sizeof(Vector3)) +
        4*(sizeof(Float) + 3*(sizeof(Quaternion) + 2*sizeof(Vector3))));
    /* Or also the same size as the animation binary file, except the time
       sharing part that's tested elsewhere */
    CORRADE_COMPARE(animation->data().size(), 664 - 4*sizeof(Float));
    CORRADE_COMPARE(animation->trackCount(), 6);

    /* Rotation, linearly interpolated */
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::Quaternion);
    CORRADE_COMPARE(animation->trackTargetType(0), AnimationTrackTargetType::Rotation3D);
    CORRADE_COMPARE(animation->trackTarget(0), 0);
    Animation::TrackView<Float, Quaternion> rotation = animation->track<Quaternion>(0);
    CORRADE_COMPARE(rotation.interpolation(), Animation::Interpolation::Linear);
    CORRADE_COMPARE(rotation.at(1.875f), Quaternion::rotation(90.0_degf, Vector3::xAxis()));

    /* Translation, constant interpolated, sharing keys with scaling */
    CORRADE_COMPARE(animation->trackType(1), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(1), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(1), 1);
    Animation::TrackView<Float, Vector3> translation = animation->track<Vector3>(1);
    CORRADE_COMPARE(translation.interpolation(), Animation::Interpolation::Constant);
    CORRADE_COMPARE(translation.at(1.5f), Vector3::yAxis(2.5f));

    /* Scaling, linearly interpolated, sharing keys with translation */
    CORRADE_COMPARE(animation->trackType(2), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(2), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(2), 2);
    Animation::TrackView<Float, Vector3> scaling = animation->track<Vector3>(2);
    CORRADE_COMPARE(scaling.interpolation(), Animation::Interpolation::Linear);
    CORRADE_COMPARE(scaling.at(1.5f), Vector3::zScale(5.2f));

    /* Rotation, spline interpolated */
    CORRADE_COMPARE(animation->trackType(3), AnimationTrackType::CubicHermiteQuaternion);
    CORRADE_COMPARE(animation->trackTargetType(3), AnimationTrackTargetType::Rotation3D);
    CORRADE_COMPARE(animation->trackTarget(3), 3);
    Animation::TrackView<Float, CubicHermiteQuaternion> rotation2 = animation->track<CubicHermiteQuaternion>(3);
    CORRADE_COMPARE(rotation2.interpolation(), Animation::Interpolation::Spline);
    /* The same as in CubicHermiteTest::splerpQuaternion() */
    CORRADE_COMPARE(rotation2.at(0.5f + 0.35f*3),
        (Quaternion{{-0.309862f, 0.174831f, 0.809747f}, 0.466615f}));

    /* Translation, spline interpolated */
    CORRADE_COMPARE(animation->trackType(4), AnimationTrackType::CubicHermite3D);
    CORRADE_COMPARE(animation->trackTargetType(4), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(4), 4);
    Animation::TrackView<Float, CubicHermite3D> translation2 = animation->track<CubicHermite3D>(4);
    CORRADE_COMPARE(translation2.interpolation(), Animation::Interpolation::Spline);
    /* The same as in CubicHermiteTest::splerpVector() */
    CORRADE_COMPARE(translation2.at(0.5f + 0.35f*3),
        (Vector3{1.04525f, 0.357862f, 0.540875f}));

    /* Scaling, spline interpolated */
    CORRADE_COMPARE(animation->trackType(5), AnimationTrackType::CubicHermite3D);
    CORRADE_COMPARE(animation->trackTargetType(5), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(5), 5);
    Animation::TrackView<Float, CubicHermite3D> scaling2 = animation->track<CubicHermite3D>(5);
    CORRADE_COMPARE(scaling2.interpolation(), Animation::Interpolation::Spline);
    CORRADE_COMPARE(scaling2.at(0.5f + 0.35f*3),
        (Vector3{0.118725f, 0.8228f, -2.711f}));
}

void TinyGltfImporterTest::camera() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "camera" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->cameraCount(), 4);

    {
        CORRADE_COMPARE(importer->cameraName(0), "Orthographic 4:3");
        CORRADE_COMPARE(importer->cameraForName("Orthographic 4:3"), 0);

        auto cam = importer->camera(0);
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->type(), CameraType::Orthographic3D);
        CORRADE_COMPARE(cam->size(), (Vector2{4.0f, 3.0f}));
        CORRADE_COMPARE(cam->aspectRatio(), 1.333333f);
        CORRADE_COMPARE(cam->near(), 0.01f);
        CORRADE_COMPARE(cam->far(), 100.0f);
    } {
        CORRADE_COMPARE(importer->cameraName(1), "Perspective 1:1 75° hFoV");

        auto cam = importer->camera(1);
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->type(), CameraType::Perspective3D);
        CORRADE_COMPARE(cam->fov(), 75.0_degf);
        CORRADE_COMPARE(cam->aspectRatio(), 1.0f);
        CORRADE_COMPARE(cam->near(), 0.1f);
        CORRADE_COMPARE(cam->far(), 150.0f);
    } {
        CORRADE_COMPARE(importer->cameraName(2), "Perspective 4:3 75° hFoV");
        CORRADE_COMPARE(importer->cameraForName("Perspective 4:3 75° hFoV"), 2);

        auto cam = importer->camera(2);
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->type(), CameraType::Perspective3D);
        CORRADE_COMPARE(cam->fov(), 75.0_degf);
        CORRADE_COMPARE(cam->aspectRatio(), 4.0f/3.0f);
        CORRADE_COMPARE(cam->near(), 0.1f);
        CORRADE_COMPARE(cam->far(), 150.0f);
    } {
        CORRADE_COMPARE(importer->cameraName(3), "Perspective 16:9 75° hFoV infinite");
        CORRADE_COMPARE(importer->cameraForName("Perspective 16:9 75° hFoV infinite"), 3);

        auto cam = importer->camera(3);
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->type(), CameraType::Perspective3D);
        CORRADE_COMPARE(cam->fov(), 75.0_degf);
        CORRADE_COMPARE(cam->aspectRatio(), 16.0f/9.0f);
        CORRADE_COMPARE(cam->near(), 0.1f);
        CORRADE_COMPARE(cam->far(), Constants::inf());
    }
}

void TinyGltfImporterTest::light() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "scene" + std::string{data.suffix})));

    /* Explicit default scene */
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

void TinyGltfImporterTest::sceneEmpty() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "empty" + std::string{data.suffix})));

    /* There is no scene, can't have any default */
    CORRADE_COMPARE(importer->defaultScene(), -1);
    CORRADE_COMPARE(importer->sceneCount(), 0);
}

void TinyGltfImporterTest::sceneNoDefault() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "scene-nodefault" + std::string{data.suffix})));

    /* There is at least one scene, it's made default */
    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->sceneCount(), 1);

    auto scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_VERIFY(scene->children3D().empty());
}

void TinyGltfImporterTest::objectTransformation() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

void TinyGltfImporterTest::objectTransformationQuaternionNormalizationEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("normalizeQuaternions"));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "object-transformation-patching.gltf")));

    CORRADE_COMPARE(importer->object3DCount(), 1);
    CORRADE_COMPARE(importer->object3DName(0), "Non-normalized rotation");

    Containers::Pointer<Trade::ObjectData3D> object;
    std::ostringstream out;
    {
        Warning warningRedirection{&out};
        object = importer->object3D(0);
    }
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::object3D(): rotation quaternion was renormalized\n");
    CORRADE_COMPARE(object->flags(), ObjectFlag3D::HasTranslationRotationScaling);
    CORRADE_COMPARE(object->rotation(), Quaternion::rotation(45.0_degf, Vector3::yAxis()));
}

void TinyGltfImporterTest::objectTransformationQuaternionNormalizationDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Explicity disable */
    importer->configuration().setValue("normalizeQuaternions", false);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "object-transformation-patching.gltf")));

    CORRADE_COMPARE(importer->object3DCount(), 1);
    CORRADE_COMPARE(importer->object3DName(0), "Non-normalized rotation");

    auto object = importer->object3D(0);
    CORRADE_VERIFY(object);
    CORRADE_COMPARE(object->flags(), ObjectFlag3D::HasTranslationRotationScaling);
    CORRADE_COMPARE(object->rotation(), Quaternion::rotation(45.0_degf, Vector3::yAxis())*2.0f);
}

void TinyGltfImporterTest::mesh() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

void TinyGltfImporterTest::meshMultiplePrimitives() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-multiple-primitives.gltf")));

    /* Four meshes, but one has three primitives and one two. Distinguishing
       using the primitive type, hopefully that's enough. */
    CORRADE_COMPARE(importer->mesh3DCount(), 7);
    {
        CORRADE_COMPARE(importer->mesh3DName(0), "Single-primitive points");
        CORRADE_COMPARE(importer->mesh3DForName("Single-primitive points"), 0);
        auto mesh = importer->mesh3D(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Points);
    } {
        CORRADE_COMPARE(importer->mesh3DName(1), "Multi-primitive lines, triangles, triangle strip");
        CORRADE_COMPARE(importer->mesh3DName(2), "Multi-primitive lines, triangles, triangle strip");
        CORRADE_COMPARE(importer->mesh3DName(3), "Multi-primitive lines, triangles, triangle strip");
        CORRADE_COMPARE(importer->mesh3DForName("Multi-primitive lines, triangles, triangle strip"), 1);
        auto mesh1 = importer->mesh3D(1);
        CORRADE_VERIFY(mesh1);
        CORRADE_COMPARE(mesh1->primitive(), MeshPrimitive::Lines);
        auto mesh2 = importer->mesh3D(2);
        CORRADE_VERIFY(mesh2);
        CORRADE_COMPARE(mesh2->primitive(), MeshPrimitive::Triangles);
        auto mesh3 = importer->mesh3D(3);
        CORRADE_VERIFY(mesh3);
        CORRADE_COMPARE(mesh3->primitive(), MeshPrimitive::TriangleStrip);
    } {
        CORRADE_COMPARE(importer->mesh3DName(4), "Single-primitive line loop");
        CORRADE_COMPARE(importer->mesh3DForName("Single-primitive line loop"), 4);
        auto mesh = importer->mesh3D(4);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::LineLoop);
    } {
        CORRADE_COMPARE(importer->mesh3DName(5), "Multi-primitive triangle fan, line strip");
        CORRADE_COMPARE(importer->mesh3DName(6), "Multi-primitive triangle fan, line strip");
        CORRADE_COMPARE(importer->mesh3DForName("Multi-primitive triangle fan, line strip"), 5);
        auto mesh5 = importer->mesh3D(5);
        CORRADE_VERIFY(mesh5);
        CORRADE_COMPARE(mesh5->primitive(), MeshPrimitive::TriangleFan);
        auto mesh6 = importer->mesh3D(6);
        CORRADE_VERIFY(mesh6);
        CORRADE_COMPARE(mesh6->primitive(), MeshPrimitive::LineStrip);
    }

    /* Five objects, but two refer a three-primitive mesh and one refers a
       two-primitive one */
    CORRADE_COMPARE(importer->object3DCount(), 10);
    {
        CORRADE_COMPARE(importer->object3DName(0), "Using the second mesh, should have 4 children");
        CORRADE_COMPARE(importer->object3DName(1), "Using the second mesh, should have 4 children");
        CORRADE_COMPARE(importer->object3DName(2), "Using the second mesh, should have 4 children");
        CORRADE_COMPARE(importer->object3DForName("Using the second mesh, should have 4 children"), 0);
        auto object = importer->object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 1);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{1, 2, 8, 3}));

        auto child1 = importer->object3D(1);
        CORRADE_VERIFY(child1);
        CORRADE_COMPARE(child1->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child1->instance(), 2);
        CORRADE_COMPARE(child1->children(), {});
        CORRADE_COMPARE(child1->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child1->translation(), Vector3{});
        CORRADE_COMPARE(child1->rotation(), Quaternion{});
        CORRADE_COMPARE(child1->scaling(), Vector3{1.0f});

        auto child2 = importer->object3D(2);
        CORRADE_VERIFY(child2);
        CORRADE_COMPARE(child2->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child2->instance(), 3);
        CORRADE_COMPARE(child2->children(), {});
        CORRADE_COMPARE(child2->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child2->translation(), Vector3{});
        CORRADE_COMPARE(child2->rotation(), Quaternion{});
        CORRADE_COMPARE(child2->scaling(), Vector3{1.0f});
    } {
        CORRADE_COMPARE(importer->object3DName(3), "Using the first mesh, no children");
        CORRADE_COMPARE(importer->object3DForName("Using the first mesh, no children"), 3);
        auto object = importer->object3D(3);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 0);
        CORRADE_COMPARE(object->children(), {});
    } {
        CORRADE_COMPARE(importer->object3DName(4), "Just a non-mesh node");
        CORRADE_COMPARE(importer->object3DForName("Just a non-mesh node"), 4);
        auto object = importer->object3D(4);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->instance(), -1);
        CORRADE_COMPARE(object->children(), {});
    } {
        CORRADE_COMPARE(importer->object3DName(5), "Using the second mesh again, again 2 children");
        CORRADE_COMPARE(importer->object3DName(6), "Using the second mesh again, again 2 children");
        CORRADE_COMPARE(importer->object3DName(7), "Using the second mesh again, again 2 children");
        CORRADE_COMPARE(importer->object3DForName("Using the second mesh again, again 2 children"), 5);
        auto object = importer->object3D(5);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 1);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{6, 7}));

        auto child6 = importer->object3D(6);
        CORRADE_VERIFY(child6);
        CORRADE_COMPARE(child6->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child6->instance(), 2);
        CORRADE_COMPARE(child6->children(), {});
        CORRADE_COMPARE(child6->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child6->translation(), Vector3{});
        CORRADE_COMPARE(child6->rotation(), Quaternion{});
        CORRADE_COMPARE(child6->scaling(), Vector3{1.0f});

        auto child7 = importer->object3D(7);
        CORRADE_VERIFY(child7);
        CORRADE_COMPARE(child7->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child7->instance(), 3);
        CORRADE_COMPARE(child7->children(), {});
        CORRADE_COMPARE(child7->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child7->translation(), Vector3{});
        CORRADE_COMPARE(child7->rotation(), Quaternion{});
        CORRADE_COMPARE(child7->scaling(), Vector3{1.0f});
    } {
        CORRADE_COMPARE(importer->object3DName(8), "Using the fourth mesh, 1 child");
        CORRADE_COMPARE(importer->object3DName(9), "Using the fourth mesh, 1 child");
        CORRADE_COMPARE(importer->object3DForName("Using the fourth mesh, 1 child"), 8);
        auto object = importer->object3D(8);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 5);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{9}));

        auto child9 = importer->object3D(9);
        CORRADE_VERIFY(child9);
        CORRADE_COMPARE(child9->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child9->instance(), 6);
        CORRADE_COMPARE(child9->children(), {});
        CORRADE_COMPARE(child9->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child9->translation(), Vector3{});
        CORRADE_COMPARE(child9->rotation(), Quaternion{});
        CORRADE_COMPARE(child9->scaling(), Vector3{1.0f});
    }

    /* Animations -- the instance ID should point to the right expanded nodes */
    CORRADE_COMPARE(importer->animationCount(), 1);
    {
        CORRADE_COMPARE(importer->animationName(0), "Animation affecting multi-primitive nodes");
        CORRADE_COMPARE(importer->animationForName("Animation affecting multi-primitive nodes"), 0);

        auto animation = importer->animation(0);
        CORRADE_VERIFY(animation);
        CORRADE_COMPARE(animation->trackCount(), 4);
        CORRADE_COMPARE(animation->trackTargetType(0), AnimationTrackTargetType::Translation3D);
        CORRADE_COMPARE(animation->trackTargetType(1), AnimationTrackTargetType::Translation3D);
        CORRADE_COMPARE(animation->trackTargetType(2), AnimationTrackTargetType::Translation3D);
        CORRADE_COMPARE(animation->trackTargetType(3), AnimationTrackTargetType::Translation3D);
        CORRADE_COMPARE(animation->trackTarget(0), 5); /* not 3 */
        CORRADE_COMPARE(animation->trackTarget(1), 3); /* not 1 */
        CORRADE_COMPARE(animation->trackTarget(2), 4); /* not 2 */
        CORRADE_COMPARE(animation->trackTarget(3), 8); /* not 4 */
    }
}

void TinyGltfImporterTest::materialPbrMetallicRoughness() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

void TinyGltfImporterTest::materialProperties() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-properties" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->materialCount(), 5);
    {
        CORRADE_COMPARE(importer->materialName(0), "Implicit values");
        auto material = importer->material(0);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

        auto& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_COMPARE(phong.flags(), PhongMaterialData::Flags{});
        CORRADE_COMPARE(phong.alphaMode(), MaterialAlphaMode::Opaque);
        CORRADE_COMPARE(phong.alphaMask(), 0.5f);
    } {
        CORRADE_COMPARE(importer->materialName(1), "Alpha mask");
        auto material = importer->material(1);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

        auto& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_COMPARE(phong.flags(), PhongMaterialData::Flags{});
        CORRADE_COMPARE(phong.alphaMode(), MaterialAlphaMode::Mask);
        CORRADE_COMPARE(phong.alphaMask(), 0.369f);
    } {
        CORRADE_COMPARE(importer->materialName(2), "Double-sided alpha blend");
        auto material = importer->material(2);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

        auto& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_COMPARE(phong.flags(), PhongMaterialData::Flag::DoubleSided);
        CORRADE_COMPARE(phong.alphaMode(), MaterialAlphaMode::Blend);
        CORRADE_COMPARE(phong.alphaMask(), 0.5f);
    } {
        CORRADE_COMPARE(importer->materialName(3), "Opaque");
        auto material = importer->material(3);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

        auto& phong = static_cast<const Trade::PhongMaterialData&>(*material);
        CORRADE_COMPARE(phong.flags(), PhongMaterialData::Flags{});
        CORRADE_COMPARE(phong.alphaMode(), MaterialAlphaMode::Opaque);
        CORRADE_COMPARE(phong.alphaMask(), 0.5f);
    } {
        CORRADE_COMPARE(importer->materialName(4), "Unknown alpha mode");

        std::ostringstream out;
        Error redirectError{&out};

        CORRADE_VERIFY(!importer->material(4));
        CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::material(): unknown alpha mode WAT\n");
    }
}

void TinyGltfImporterTest::texture() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->materialCount(), 1);

    auto material = importer->material(0);

    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->type(), Trade::MaterialType::Phong);

    auto&& phong = static_cast<const Trade::PhongMaterialData&>(*material);
    CORRADE_VERIFY(phong.flags() & PhongMaterialData::Flag::DiffuseTexture);
    CORRADE_COMPARE(phong.diffuseTexture(), 0);
    CORRADE_COMPARE(phong.shininess(), 1.0f);

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

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

void TinyGltfImporterTest::textureMissingSource() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture-missing-source.gltf")));
    CORRADE_COMPARE(importer->textureCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->texture(0));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::texture(): no image source found\n");
}

constexpr char ExpectedImageData[] =
    "\xa8\xa7\xac\xff\x9d\x9e\xa0\xff\xad\xad\xac\xff\xbb\xbb\xba\xff\xb3\xb4\xb6\xff"
    "\xb0\xb1\xb6\xff\xa0\xa0\xa1\xff\x9f\x9f\xa0\xff\xbc\xbc\xba\xff\xcc\xcc\xcc\xff"
    "\xb2\xb4\xb9\xff\xb8\xb9\xbb\xff\xc1\xc3\xc2\xff\xbc\xbd\xbf\xff\xb8\xb8\xbc\xff";

void TinyGltfImporterTest::imageEmbedded() {
    auto&& data = ImageEmbeddedData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Open as data, so we verify opening embedded images from data does not
       cause any problems even when no file callbacks are set */
    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "image" + std::string{data.suffix}))));

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

void TinyGltfImporterTest::imageExternal() {
    auto&& data = ImageExternalData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
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

void TinyGltfImporterTest::imageExternalNotFound() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, "image-notfound.gltf")));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /nonexistent.png\n");
}

void TinyGltfImporterTest::imageExternalNoPathNoCallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, "image.gltf"))));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::image2D(): external images can be imported only when opening files from the filesystem or if a file callback is present\n");
}

void TinyGltfImporterTest::imageBasis() {
    auto&& data = ImageBasisData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    /* Import as ASTC */
    _manager.metadata("BasisImporter")->configuration().setValue("format", "Astc4x4RGBA");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "image-basis" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->textureCount(), 1);
    CORRADE_COMPARE(importer->image2DCount(), 2);

    auto image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc4x4RGBAUnorm);

    /* The texture refers to the image indirectly via an extension, test the
       mapping */
    auto texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), 1);
}

void TinyGltfImporterTest::fileCallbackBuffer() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->features() & AbstractImporter::Feature::FileCallback);

    Utility::Resource rs{"data"};
    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy policy, Utility::Resource& rs) {
        Debug{} << "Loading" << filename << "with" << policy;
        return Containers::optional(rs.getRaw(filename));
    }, rs);

    /* Using a different name from the filesystem to avoid false positive
       when the file gets loaded from a filesystem */
    CORRADE_VERIFY(importer->openFile("some/path/data" + std::string{data.suffix}));

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

void TinyGltfImporterTest::fileCallbackBufferNotFound() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->features() & AbstractImporter::Feature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy, void*)
        -> Containers::Optional<Containers::ArrayView<const char>> { return {}; });

    std::ostringstream out;
    Error redirectError{&out};

    Utility::Resource rs{"data"};
    CORRADE_VERIFY(!importer->openData(rs.getRaw("some/path/data" + std::string{data.suffix})));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: File read error : data.bin : file callback failed\n");
}

void TinyGltfImporterTest::fileCallbackImage() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->features() & AbstractImporter::Feature::FileCallback);

    Utility::Resource rs{"data"};
    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy  policy, Utility::Resource& rs) {
        Debug{} << "Loading" << filename << "with" << policy;
        return Containers::optional(rs.getRaw(filename));
    }, rs);

    /* Using a different name from the filesystem to avoid false positive
       when the file gets loaded from a filesystem */
    CORRADE_VERIFY(importer->openFile("some/path/data" + std::string{data.suffix}));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    auto image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(ExpectedImageData).prefix(60), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::fileCallbackImageNotFound() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->features() & AbstractImporter::Feature::FileCallback);

    Utility::Resource rs{"data"};
    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy, Utility::Resource& rs)
            -> Containers::Optional<Containers::ArrayView<const char>>
        {
            if(filename == "data.bin")
                return rs.getRaw("some/path/data.bin");
            return {};
        }, rs);

    CORRADE_VERIFY(importer->openData(rs.getRaw("some/path/data" + std::string{data.suffix})));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file data.png\n");
}

void TinyGltfImporterTest::utf8filenames() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "přívodní-šňůra.gltf")));

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

    CORRADE_COMPARE(importer->image2DCount(), 1);
    auto image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(ExpectedImageData).prefix(60), TestSuite::Compare::Container);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::TinyGltfImporterTest)
