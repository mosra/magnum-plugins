/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2017, 2020, 2021 Jonathan Hale <squareys@googlemail.com>
    Copyright © 2018 Konstantinos Chatzilygeroudis <costashatz@gmail.com>
    Copyright © 2019, 2020 Max Schwarz <max.schwarz@ais.uni-bonn.de>
    Copyright © 2021 Pablo Escobar <mail@rvrs.in>

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
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/FormatStl.h>
#include <Corrade/Utility/String.h>
#include <Magnum/FileCallback.h>
#include <Magnum/Mesh.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Animation/Player.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/AnimationData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/ObjectData3D.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/SkinData.h>
#include <Magnum/Trade/TextureData.h>

#include <assimp/defs.h> /* in assimp 3.0, version.h is missing this include for ASSIMP_API */
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>

#include "configure.h"
#include "MagnumPlugins/AssimpImporter/configureInternal.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

using namespace Math::Literals;

struct AssimpImporterTest: TestSuite::Tester {
    explicit AssimpImporterTest();

    void openFile();
    void openFileFailed();
    void openData();
    void openDataFailed();

    void animation();
    void animationGltf();

    void animationGltfNoScene();
    void animationGltfBrokenSplineWarning();
    void animationGltfSpline();
    void animationGltfTicksPerSecondPatching();

    void animationDummyTracksRemovalEnabled();
    void animationDummyTracksRemovalDisabled();
    void animationShortestPathOptimizationEnabled();
    void animationShortestPathOptimizationDisabled();
    void animationQuaternionNormalizationEnabled();
    void animationQuaternionNormalizationDisabled();
    void animationMergeEmpty();
    void animationMerge();

    void skin();
    void skinNoMeshes();
    void skinMergeEmpty();
    void skinMerge();

    void camera();
    void light();
    void lightUnsupported();
    void materialColor();
    void materialTexture();
    void materialColorTexture();
    void materialStlWhiteAmbientPatch();
    void materialWhiteAmbientTexture();
    void materialMultipleTextures();
    void materialTextureCoordinateSets();

    void mesh();
    void pointMesh();
    void lineMesh();
    void meshCustomAttributes();
    void meshSkinningAttributes();
    void meshSkinningAttributesMultiple();
    void meshSkinningAttributesMultipleGltf();
    void meshSkinningAttributesMaxJointWeights();
    void meshSkinningAttributesDummyWeightRemoval();
    void meshSkinningAttributesMerge();
    void meshMultiplePrimitives();

    void emptyCollada();
    void emptyGltf();
    void scene();
    void sceneCollapsedNode();
    void upDirectionPatching();
    void upDirectionPatchingPreTransformVertices();

    void imageEmbedded();
    void imageExternal();
    void imageExternalNotFound();
    void imageExternalNoPathNoCallback();
    void imagePathMtlSpaceAtTheEnd();
    void imagePathBackslash();
    void imageMipLevels();

    void texture();

    void openState();
    void openStateTexture();

    void configurePostprocessFlipUVs();

    void fileCallback();
    void fileCallbackNotFound();
    void fileCallbackEmptyFile();
    void fileCallbackReset();
    void fileCallbackImage();
    void fileCallbackImageNotFound();

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

constexpr struct {
    const char* name;
    ImporterFlags flags;
} VerboseData[]{
    {"", {}},
    {"verbose", ImporterFlag::Verbose}
};

const struct {
    const char* name;
    const char* suffix;
    const Quaternion correction;
} ExportedFileData[]{
    /* Blender's Collada exporter doesn't seem to apply axis change
       inside animations/skins correctly. Do it manually, consistent
       with export up and forward axis (see exported-animation.md):
       y = z, z = -y */
    {"Collada", ".dae", Quaternion::rotation(-90.0_degf, Vector3::xAxis())},
    {"FBX", ".fbx", {}},
    {"glTF", ".gltf", {}}
};

constexpr struct {
    const char* name;
    const char* file;
    bool importColladaIgnoreUpDirection;
    bool expectFail;
} UpDirectionPatchingData[]{
    {"Y up", "y-up.dae", false, false},
    {"Y up, ignored", "y-up.dae", true, false},
    {"Z up", "z-up.dae", false, false},
    {"Z up, ignored", "z-up.dae", true, true}
};

AssimpImporterTest::AssimpImporterTest() {
    addInstancedTests({&AssimpImporterTest::openFile},
        Containers::arraySize(VerboseData));

    addTests({&AssimpImporterTest::openFileFailed,
              &AssimpImporterTest::openData,
              &AssimpImporterTest::openDataFailed});

    addInstancedTests({&AssimpImporterTest::animation},
        Containers::arraySize(ExportedFileData));

    addTests({&AssimpImporterTest::animationGltf,
              &AssimpImporterTest::animationGltfNoScene,
              &AssimpImporterTest::animationGltfBrokenSplineWarning,
              &AssimpImporterTest::animationGltfSpline});

    addInstancedTests({&AssimpImporterTest::animationGltfTicksPerSecondPatching,
                       &AssimpImporterTest::animationDummyTracksRemovalEnabled,
                       &AssimpImporterTest::animationDummyTracksRemovalDisabled},
        Containers::arraySize(VerboseData));

    addTests({&AssimpImporterTest::animationShortestPathOptimizationEnabled,
              &AssimpImporterTest::animationShortestPathOptimizationDisabled,
              &AssimpImporterTest::animationQuaternionNormalizationEnabled,
              &AssimpImporterTest::animationQuaternionNormalizationDisabled,
              &AssimpImporterTest::animationMergeEmpty,
              &AssimpImporterTest::animationMerge});

    addInstancedTests({&AssimpImporterTest::skin},
        Containers::arraySize(ExportedFileData));

    addTests({&AssimpImporterTest::skinNoMeshes,
              &AssimpImporterTest::skinMergeEmpty,
              &AssimpImporterTest::skinMerge,

              &AssimpImporterTest::camera,

              &AssimpImporterTest::light,
              &AssimpImporterTest::lightUnsupported,

              &AssimpImporterTest::materialColor,
              &AssimpImporterTest::materialTexture,
              &AssimpImporterTest::materialColorTexture,
              &AssimpImporterTest::materialStlWhiteAmbientPatch,
              &AssimpImporterTest::materialWhiteAmbientTexture,
              &AssimpImporterTest::materialMultipleTextures,
              &AssimpImporterTest::materialTextureCoordinateSets,

              &AssimpImporterTest::mesh,
              &AssimpImporterTest::pointMesh,
              &AssimpImporterTest::lineMesh,
              &AssimpImporterTest::meshCustomAttributes});

    addInstancedTests({&AssimpImporterTest::meshSkinningAttributes},
        Containers::arraySize(ExportedFileData));

    addTests({&AssimpImporterTest::meshSkinningAttributesMultiple,
              &AssimpImporterTest::meshSkinningAttributesMultipleGltf,
              &AssimpImporterTest::meshSkinningAttributesMaxJointWeights,
              &AssimpImporterTest::meshSkinningAttributesDummyWeightRemoval,
              &AssimpImporterTest::meshSkinningAttributesMerge,
              &AssimpImporterTest::meshMultiplePrimitives,
              &AssimpImporterTest::emptyCollada,
              &AssimpImporterTest::emptyGltf,
              &AssimpImporterTest::scene,
              &AssimpImporterTest::sceneCollapsedNode});

    addInstancedTests({&AssimpImporterTest::upDirectionPatching,
                       &AssimpImporterTest::upDirectionPatchingPreTransformVertices},
        Containers::arraySize(UpDirectionPatchingData));

    addTests({&AssimpImporterTest::imageEmbedded,
              &AssimpImporterTest::imageExternal,
              &AssimpImporterTest::imageExternalNotFound,
              &AssimpImporterTest::imageExternalNoPathNoCallback,
              &AssimpImporterTest::imagePathMtlSpaceAtTheEnd,
              &AssimpImporterTest::imagePathBackslash,
              &AssimpImporterTest::imageMipLevels,

              &AssimpImporterTest::texture,

              &AssimpImporterTest::openState,
              &AssimpImporterTest::openStateTexture,

              &AssimpImporterTest::configurePostprocessFlipUVs,

              &AssimpImporterTest::fileCallback,
              &AssimpImporterTest::fileCallbackNotFound,
              &AssimpImporterTest::fileCallbackEmptyFile,
              &AssimpImporterTest::fileCallbackReset,
              &AssimpImporterTest::fileCallbackImage,
              &AssimpImporterTest::fileCallbackImageNotFound});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. Reset
       the plugin dir after so it doesn't load anything else from the
       filesystem. */
    #ifdef ASSIMPIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(ASSIMPIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    _manager.setPluginDirectory({});
    #endif
    /* The DdsImporter (for DDS loading / mip import tests) is optional */
    #ifdef DDSIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(DDSIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* The StbImageImporter (for PNG image loading) is optional */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void AssimpImporterTest::openFile() {
    auto&& data = VerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->setFlags(data.flags);

    std::ostringstream out;
    {
        Debug redirectOutput{&out};

        CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae")));
        CORRADE_VERIFY(importer->importerState());
        CORRADE_COMPARE(importer->sceneCount(), 1);
        CORRADE_COMPARE(importer->object3DCount(), 2);
        CORRADE_COMPARE(importer->meshCount(), 0);
        CORRADE_COMPARE(importer->animationCount(), 0);
        CORRADE_COMPARE(importer->skin3DCount(), 0);

        importer->close();
        CORRADE_VERIFY(!importer->isOpened());
    }

    /* It should be noisy if and only if verbose output is enabled */
    Debug{Debug::Flag::NoNewlineAtTheEnd} << out.str();
    CORRADE_COMPARE(!out.str().empty(), data.flags >= ImporterFlag::Verbose);
}

void AssimpImporterTest::openFileFailed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openFile("i-do-not-exist.foo"));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openFile(): failed to open i-do-not-exist.foo: Unable to open file \"i-do-not-exist.foo\".\n");
}

void AssimpImporterTest::openData() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    auto data = Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae"));
    CORRADE_VERIFY(importer->openData(data));
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 2);
    CORRADE_COMPARE(importer->meshCount(), 0);
    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->skin3DCount(), 0);

    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void AssimpImporterTest::openDataFailed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    std::ostringstream out;
    Error redirectError{&out};

    constexpr const char data[] = "what";
    CORRADE_VERIFY(!importer->openData({data, sizeof(data)}));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openData(): loading failed: No suitable reader found for the file format of file \"$$$___magic___$$$.\".\n");
}

/* This does not indicate general assimp animation support, only used to skip
   tests on certain versions and test files. */
bool supportsAnimation(const Containers::StringView fileName) {
    /* 5.0.0 supports all of Collada, FBX, glTF */
    #if ASSIMP_IS_VERSION_5
    static_cast<void>(fileName);
    return true;
    #else
    if(fileName.hasSuffix(".gltf")) return false;

    const unsigned int version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    CORRADE_INTERNAL_ASSERT(fileName.hasSuffix(".dae") || fileName.hasSuffix(".fbx"));
    /* That's as far back as I checked, both Collada and FBX animations
       supported */
    return version >= 302;
    #endif
}

void AssimpImporterTest::animation() {
    auto&& data = ExportedFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(!supportsAnimation(data.suffix))
        CORRADE_SKIP("Animation for this file type is not supported with the current version of Assimp");

    /* Animation created and exported with Blender. Most animation tracks got
       resampled and/or merged during export, so there's no use comparing
       against exact key/value arrays. Just apply all tracks and check if the
       transformation roughly matches at specific points in time.
       animationGltf() test covers that AssimpImporter correctly passes on what
       Assimp outputs. */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "exported-animation" + std::string{data.suffix})));

    struct Node {
        const char* name;
        Vector3 translation;
        Quaternion rotation;
        Vector3 scaling;
    };

    /* Some Blender exporters don't transform animations correctly.
       Also see the comment for ExportedFileData. */
    auto correctNode = [&](Node& node) {
        node.translation = data.correction.transformVector(node.translation);
        node.rotation = data.correction * node.rotation;
    };

    /* Find animation target nodes by their name */
    Node nodes[3]{
        {"Rotating", {}, {}, {}},
        {"Scaling", {}, {}, {}},
        {"Translating", {}, {}, {}}
    };
    Node* nodeMap[3]{};

    const UnsignedInt objectCount = importer->object3DCount();
    CORRADE_COMPARE(objectCount, Containers::arraySize(nodes));

    for(UnsignedInt i = 0; i < objectCount; i++) {
        const std::string name = importer->object3DName(i);
        for(Node& n: nodes) {
            /* Exported Collada files have spaces replaced with underscores,
               so check for the first words only */
            if(name.find(n.name) == 0) {
                /* Node names in the test files are unique */
                CORRADE_VERIFY(!nodeMap[i]);
                nodeMap[i] = &n;
            }
        }
        CORRADE_VERIFY(nodeMap[i]);
    }

    CORRADE_VERIFY(importer->animationCount() > 0);

    Animation::Player<Float, Float> player;
    Containers::Array<Containers::Array<char>> animationData{importer->animationCount()};

    for(UnsignedInt i = 0; i < importer->animationCount(); i++) {
        auto animation = importer->animation(i);
        CORRADE_VERIFY(animation);
        CORRADE_VERIFY(animation->importerState());

        for(UnsignedInt j = 0; j < animation->trackCount(); j++) {
            const auto track = animation->track(j);
            /* All imported animations are linear */
            CORRADE_COMPARE(track.interpolation(), Animation::Interpolation::Linear);

            const UnsignedInt target = animation->trackTarget(j);
            switch(animation->trackTargetType(j)) {
                case AnimationTrackTargetType::Translation3D:
                    player.add(animation->track<Vector3>(j), nodeMap[target]->translation);
                    break;
                case AnimationTrackTargetType::Rotation3D:
                    player.add(animation->track<Quaternion>(j), nodeMap[target]->rotation);
                    break;
                case AnimationTrackTargetType::Scaling3D:
                    player.add(animation->track<Vector3>(j), nodeMap[target]->scaling);
                    break;
                default: CORRADE_FAIL_IF(true, "Unexpected track target type");
            }
        }

        animationData[i] = animation->release();
    }

    /* Check values at known keyframes:
       4 keyframes at 0/60/120/180, exported at 24fps (60/24 = 2.5, etc.). */
    constexpr UnsignedInt KeyCount = 4;
    constexpr Float keys[KeyCount]{0.0f, 2.5f, 5.0f, 7.5f};

    CORRADE_VERIFY(player.duration().contains({ keys[0], keys[Containers::arraySize(keys) - 1] }));
    player.play(0.0f);

    /* Some Blender exporters (e.g. FBX) and our manual Collada correction
       flip axes by adding a non-identity default rotation to all nodes. Store
       them so we can later compare against the rotation from key 0 to key i. */
    Quaternion initialRotation[Containers::arraySize(nodes)];

    player.advance(keys[0]);
    for(UnsignedInt i = 0; i < Containers::arraySize(nodes); i++) {
        correctNode(nodes[i]);
        initialRotation[i] = nodes[i].rotation;
    }

    constexpr Vector3 translationData[KeyCount]{
        {0.0f, 0.0f, 3.0f},
        {5.0f, 0.0f, 3.0f},
        {5.0f, 0.0f, 8.0f},
        {0.0f, 0.0f, 8.0f}
    };
    constexpr Vector3 rotationData[KeyCount]{
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, Float(Rad(-90.0_degf))},
        {Float(Rad(90.0_degf)), 0.0f, Float(Rad(-90.0_degf))},
        {Float(Rad(135.0_degf)), 0.0f, Float(Rad(-90.0_degf))}
    };
    constexpr Vector3 scalingData[KeyCount]{
        {1.0f, 1.0f, 1.0f},
        {5.0f, 1.0f, 1.0f},
        {1.0f, 5.0f, 1.0f},
        {0.5f, 0.5f, 0.5f}
    };

    for(UnsignedInt i = 0; i < Containers::arraySize(keys); i++) {
        player.advance(keys[i]);
        for(Node& n: nodes)
            correctNode(n);

        /* Rotation from initial to current key */
        const Vector3 rotation{(nodes[0].rotation * initialRotation[0].inverted()).toEuler()};
        /* Apply possible non-identity rotation to get the correct scale */
        const Vector3 scaling = Math::abs(nodes[1].rotation.transformVector(nodes[1].scaling));
        const Vector3& translation = nodes[2].translation;

        /* Be lenient, resampling during export takes its toll */
        constexpr Vector3 Epsilon{0.005f};
        CORRADE_COMPARE_WITH(rotation, rotationData[i],
            TestSuite::Compare::around(Epsilon));
        CORRADE_COMPARE_WITH(scaling, scalingData[i],
            TestSuite::Compare::around(Epsilon));
        CORRADE_COMPARE_WITH(translation, translationData[i],
            TestSuite::Compare::around(Epsilon));
    }
}

struct AnimationTarget {
    AnimationTrackTargetType type;
    UnsignedInt keyCount;
    UnsignedInt channel;
};

const Containers::StaticArray<3, AnimationTarget> AnimationGltfLinearTargets{
    AnimationTarget{AnimationTrackTargetType::Rotation3D, 2, ~0u},
    AnimationTarget{AnimationTrackTargetType::Translation3D, 4, ~0u},
    AnimationTarget{AnimationTrackTargetType::Scaling3D, 4, ~0u}
};

void AssimpImporterTest::animationGltf() {
    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    /* Using the same files as TinyGltfImporterTest, but modified to include a
       scene, because Assimp refuses to import animations if there is no scene. */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "animation.gltf")));

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

        std::ostringstream out;
        Debug redirectDebug{&out};

        auto animation = importer->animation(1);
        CORRADE_VERIFY(animation);
        CORRADE_VERIFY(animation->importerState());
        /* Two rotation keys, four translation and scaling keys. */
        CORRADE_COMPARE(animation->data().size(),
            2*(sizeof(Float) + sizeof(Quaternion)) +
            2*4*(sizeof(Float) + sizeof(Vector3)));
        CORRADE_COMPARE(animation->trackCount(), 3);

        /* Channel order is not preserved by Assimp so we can't directly
           compare tracks by their glTF sampler id. Find channel through
           target id, then use that. */

        Containers::StaticArray<3, AnimationTarget> targets = AnimationGltfLinearTargets;

        for(UnsignedInt i = 0; i < animation->trackCount(); i++) {
            CORRADE_VERIFY(animation->trackTarget(i) < Containers::arraySize(targets));
            AnimationTarget& target = targets[animation->trackTarget(i)];
            CORRADE_COMPARE(target.channel, ~0u);
            target.channel = i;
        }

        /* Rotation, linearly interpolated */
        const UnsignedInt ch0 = targets[0].channel;
        CORRADE_COMPARE(animation->trackType(ch0), AnimationTrackType::Quaternion);
        CORRADE_COMPARE(animation->trackResultType(ch0), AnimationTrackType::Quaternion);
        CORRADE_COMPARE(animation->trackTargetType(ch0), AnimationTrackTargetType::Rotation3D);
        CORRADE_COMPARE(animation->trackTarget(ch0), 0);
        Animation::TrackView<const Float, const Quaternion> rotation = animation->track<Quaternion>(ch0);
        CORRADE_COMPARE(rotation.interpolation(), Animation::Interpolation::Linear);
        CORRADE_COMPARE(rotation.before(), Animation::Extrapolation::Constant);
        CORRADE_COMPARE(rotation.after(), Animation::Extrapolation::Constant);
        constexpr Float rotationKeys[]{
            1.25f,
            2.50f
        };
        const Quaternion rotationValues[]{
            Quaternion::rotation(0.0_degf, Vector3::xAxis()),
            Quaternion::rotation(180.0_degf, Vector3::xAxis())
        };
        CORRADE_COMPARE_AS(rotation.keys(), Containers::stridedArrayView(rotationKeys), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(rotation.values(), Containers::stridedArrayView(rotationValues), TestSuite::Compare::Container);
        CORRADE_COMPARE(rotation.at(1.875f), Quaternion::rotation(90.0_degf, Vector3::xAxis()));

        constexpr Float translationScalingKeys[]{
            0.0f,
            1.25f,
            2.5f,
            3.75f
        };

        /* Translation, constant interpolated, sharing keys with scaling */
        const UnsignedInt ch1 = targets[1].channel;
        CORRADE_COMPARE(animation->trackType(ch1), AnimationTrackType::Vector3);
        CORRADE_COMPARE(animation->trackResultType(ch1), AnimationTrackType::Vector3);
        CORRADE_COMPARE(animation->trackTargetType(ch1), AnimationTrackTargetType::Translation3D);
        CORRADE_COMPARE(animation->trackTarget(ch1), 1);
        Animation::TrackView<const Float, const Vector3> translation = animation->track<Vector3>(ch1);
        CORRADE_COMPARE(translation.interpolation(), Animation::Interpolation::Linear);
        CORRADE_COMPARE(translation.before(), Animation::Extrapolation::Constant);
        CORRADE_COMPARE(translation.after(), Animation::Extrapolation::Constant);
        constexpr Vector3 translationData[]{
            Vector3::yAxis(0.0f),
            Vector3::yAxis(2.5f),
            Vector3::yAxis(2.5f),
            Vector3::yAxis(0.0f)
        };
        CORRADE_COMPARE_AS(translation.keys(), Containers::stridedArrayView(translationScalingKeys), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(translation.values(), Containers::stridedArrayView(translationData), TestSuite::Compare::Container);
        CORRADE_COMPARE(translation.at(1.5f), Vector3::yAxis(2.5f));

        /* Scaling, linearly interpolated, sharing keys with translation */
        const UnsignedInt ch2 = targets[2].channel;
        CORRADE_COMPARE(animation->trackType(ch2), AnimationTrackType::Vector3);
        CORRADE_COMPARE(animation->trackResultType(ch2), AnimationTrackType::Vector3);
        CORRADE_COMPARE(animation->trackTargetType(ch2), AnimationTrackTargetType::Scaling3D);
        CORRADE_COMPARE(animation->trackTarget(ch2), 2);
        Animation::TrackView<const Float, const Vector3> scaling = animation->track<Vector3>(ch2);
        CORRADE_COMPARE(scaling.interpolation(), Animation::Interpolation::Linear);
        CORRADE_COMPARE(scaling.before(), Animation::Extrapolation::Constant);
        CORRADE_COMPARE(scaling.after(), Animation::Extrapolation::Constant);
        constexpr Vector3 scalingData[]{
            Vector3{1.0f},
            Vector3::zScale(5.0f),
            Vector3::zScale(6.0f),
            Vector3(1.0f),
        };
        CORRADE_COMPARE_AS(scaling.keys(), Containers::stridedArrayView(translationScalingKeys), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scaling.values(), Containers::stridedArrayView(scalingData), TestSuite::Compare::Container);
        CORRADE_COMPARE(scaling.at(1.5f), Vector3::zScale(5.2f));
    }
}

void AssimpImporterTest::animationGltfNoScene() {
    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    /* This reuses the TinyGltfImporter test files, not the corrected ones used by other tests. */
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation.gltf")));

    CORRADE_EXPECT_FAIL("Assimp refuses to import glTF animations if the file has no scenes.");
    CORRADE_COMPARE(importer->animationCount(), 3);
}

void AssimpImporterTest::animationGltfBrokenSplineWarning() {
    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    if(!ASSIMP_HAS_BROKEN_GLTF_SPLINES)
        CORRADE_SKIP("Current version of assimp correctly imports glTF spline-interpolated animations.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
            "animation.gltf")));
    }
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openData(): spline-interpolated animations imported "
        "from this file are most likely broken using this version of Assimp. Consult the "
        "importer documentation for more information.\n");
}

void AssimpImporterTest::animationGltfSpline() {
    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "animation.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 3);
    CORRADE_COMPARE(importer->animationName(2), "TRS animation, splines");

    constexpr Float keys[]{ 0.5f, 3.5f, 4.0f, 5.0f };

    auto animation = importer->animation(2);
    CORRADE_VERIFY(animation);
    CORRADE_VERIFY(animation->importerState());
    /* Four T/R/S keys */
    CORRADE_COMPARE(animation->data().size(),
        4*(sizeof(Float) + sizeof(Quaternion)) +
        2*4*(sizeof(Float) + sizeof(Vector3)));
    CORRADE_COMPARE(animation->trackCount(), 3);

    /* Map from target to channel id */
    Containers::StaticArray<3, AnimationTarget> targets = AnimationGltfLinearTargets;

    for(UnsignedInt i = 0; i < animation->trackCount(); i++) {
        /* Nodes 3-5 */
        CORRADE_VERIFY(animation->trackTarget(i) >= Containers::arraySize(targets) &&
            animation->trackTarget(i) < (2*Containers::arraySize(targets)));
        AnimationTarget& target = targets[animation->trackTarget(i) - Containers::arraySize(targets)];
        CORRADE_COMPARE(target.channel, ~0u);
        target.channel = i;
    }

    /* Rotation */
    const UnsignedInt ch0 = targets[0].channel;
    CORRADE_COMPARE(animation->trackType(ch0), AnimationTrackType::Quaternion);
    CORRADE_COMPARE(animation->trackResultType(ch0), AnimationTrackType::Quaternion);
    CORRADE_COMPARE(animation->trackTargetType(ch0), AnimationTrackTargetType::Rotation3D);
    CORRADE_COMPARE(animation->trackTarget(ch0), 3);
    Animation::TrackView<const Float, const Quaternion> rotation = animation->track<Quaternion>(ch0);
    CORRADE_COMPARE(rotation.interpolation(), Animation::Interpolation::Linear);
    CORRADE_COMPARE(rotation.before(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE(rotation.after(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE_AS(rotation.keys(), Containers::stridedArrayView(keys), TestSuite::Compare::Container);
    {
        #if ASSIMP_HAS_BROKEN_GLTF_SPLINES
        CORRADE_EXPECT_FAIL("Current version of assimp incorrectly imports glTF spline-interpolated animations.");
        #endif

        constexpr Quaternion rotationValues[]{
            {{0.780076f, 0.0260025f, 0.598059f}, 0.182018f},
            {{-0.711568f, 0.391362f, 0.355784f}, 0.462519f},
            {{0.598059f, 0.182018f, 0.0260025f}, 0.780076f},
            {{0.711568f, -0.355784f, -0.462519f}, -0.391362f}
        };
        CORRADE_COMPARE_AS(rotation.values(), Containers::stridedArrayView(rotationValues), TestSuite::Compare::Container);
    }

    /* Translation */
    const UnsignedInt ch1 = targets[1].channel;
    CORRADE_COMPARE(animation->trackType(ch1), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackResultType(ch1), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(ch1), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(ch1), 4);
    Animation::TrackView<const Float, const Vector3> translation = animation->track<Vector3>(ch1);
    CORRADE_COMPARE(translation.interpolation(), Animation::Interpolation::Linear);
    CORRADE_COMPARE(translation.before(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE(translation.after(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE_AS(translation.keys(), Containers::stridedArrayView(keys), TestSuite::Compare::Container);
    {
        #if ASSIMP_HAS_BROKEN_GLTF_SPLINES
        CORRADE_EXPECT_FAIL("Current version of assimp incorrectly imports glTF spline-interpolated animations.");
        #endif

        constexpr Vector3 translationValues[]{
            {3.0f, 0.1f, 2.5f},
            {-2.0f, 1.1f, -4.3f},
            {1.5f, 9.8f, -5.1f},
            {5.1f, 0.1f, -7.3f}
        };
        CORRADE_COMPARE_AS(translation.values(), Containers::stridedArrayView(translationValues), TestSuite::Compare::Container);
    }

    /* Scaling */
    const UnsignedInt ch2 = targets[2].channel;
    CORRADE_COMPARE(animation->trackType(ch2), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackResultType(ch2), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(ch2), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(ch2), 5);
    Animation::TrackView<const Float, const Vector3> scaling = animation->track<Vector3>(ch2);
    CORRADE_COMPARE(scaling.interpolation(), Animation::Interpolation::Linear);
    CORRADE_COMPARE(scaling.before(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE(scaling.after(), Animation::Extrapolation::Constant);
    CORRADE_COMPARE_AS(scaling.keys(), Containers::stridedArrayView(keys), TestSuite::Compare::Container);
    {
        #if ASSIMP_HAS_BROKEN_GLTF_SPLINES
        CORRADE_EXPECT_FAIL("Current version of assimp incorrectly imports glTF spline-interpolated animations.");
        #endif

        constexpr Vector3 scalingData[]{
            {-2.0f, 1.1f, -4.3f},
            {5.1f, 0.1f, -7.3f},
            {3.0f, 0.1f, 2.5f},
            {1.5f, 9.8f, -5.1f}
        };
        CORRADE_COMPARE_AS(scaling.values(), Containers::stridedArrayView(scalingData), TestSuite::Compare::Container);
    }
}

void AssimpImporterTest::animationGltfTicksPerSecondPatching() {
    auto&& data = VerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    /* This was fixed right after 5.0.0, but 5.0.1 only selected compilation
       fixes and didn't bump the minor version. Boldly assuming the next
       minor version will have fixes from 2019. */
    const unsigned int version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    const bool hasInvalidTicksPerSecond = version <= 500;
    if(!hasInvalidTicksPerSecond)
        CORRADE_SKIP("Current version of assimp correctly sets glTF ticks per second.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->setFlags(data.flags);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "animation.gltf")));

    std::ostringstream out;
    {
        Debug redirectDebug{&out};
        CORRADE_VERIFY(importer->animation(1));
    }

    if(data.flags >= ImporterFlag::Verbose) {
        CORRADE_VERIFY(Containers::StringView{out.str()}.contains(
            " ticks per second is incorrect for glTF, patching to 1000\n"));
    } else
        CORRADE_VERIFY(out.str().empty());
}

void AssimpImporterTest::animationDummyTracksRemovalEnabled() {
    auto&& data = VerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    /* Correct removal is already implicitly tested in animationGltf(),
       only check for track count/size and the message here */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->setFlags(data.flags);
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("removeDummyAnimationTracks"));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "animation.gltf")));

    Containers::Optional<AnimationData> animation;
    std::ostringstream out;
    {
        Debug redirectDebug{&out};
        animation = importer->animation(1);
        CORRADE_VERIFY(animation);
    }

    /* Animation for each target object id */
    Containers::StaticArray<3, AnimationTarget> targets = AnimationGltfLinearTargets;

    /* 1 track for each object */
    CORRADE_COMPARE(animation->trackCount(), Containers::arraySize(targets));
    for(UnsignedInt i = 0; i < animation->trackCount(); i++) {
        CORRADE_VERIFY(animation->trackTarget(i) < Containers::arraySize(targets));
        AnimationTarget& target = targets[animation->trackTarget(i)];
        CORRADE_COMPARE(target.channel, ~0u);
        target.channel = i;
        CORRADE_COMPARE(animation->trackTargetType(i), target.type);
        CORRADE_COMPARE(animation->track(i).size(), target.keyCount);
    }

    if(data.flags >= ImporterFlag::Verbose) {
        const std::string str = out.str();
        CORRADE_VERIFY(Containers::StringView{str}.contains(
            Utility::formatString(
                "Trade::AssimpImporter::animation(): ignoring dummy translation track in animation 1, channel {}\n"
                "Trade::AssimpImporter::animation(): ignoring dummy scaling track in animation 1, channel {}\n",
                targets[0].channel, targets[0].channel)));
        CORRADE_VERIFY(Containers::StringView{str}.contains(
            Utility::formatString(
                "Trade::AssimpImporter::animation(): ignoring dummy rotation track in animation 1, channel {}\n"
                "Trade::AssimpImporter::animation(): ignoring dummy scaling track in animation 1, channel {}\n",
                targets[1].channel, targets[1].channel)));
        CORRADE_VERIFY(Containers::StringView{str}.contains(
            Utility::formatString(
                "Trade::AssimpImporter::animation(): ignoring dummy translation track in animation 1, channel {}\n"
                "Trade::AssimpImporter::animation(): ignoring dummy rotation track in animation 1, channel {}\n",
                targets[2].channel, targets[2].channel)));
    } else
        CORRADE_VERIFY(out.str().empty());
}

void AssimpImporterTest::animationDummyTracksRemovalDisabled() {
    auto&& data = VerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->setFlags(data.flags);
    /* Explicitly disable */
    importer->configuration().setValue("removeDummyAnimationTracks", false);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "animation.gltf")));

    Containers::Optional<AnimationData> animation;
    std::ostringstream out;
    {
        Debug redirectDebug{&out};
        animation = importer->animation(1);
        CORRADE_VERIFY(animation);
    }

    /* Non-dummy animation for each target object id */
    Containers::StaticArray<3, AnimationTarget> targets = AnimationGltfLinearTargets;

    /* Animation type for each track within a channel.
       Tracks within channels are always added in the order T,R,S */
    constexpr AnimationTrackTargetType trackAnimationType[]{
        AnimationTrackTargetType::Translation3D,
        AnimationTrackTargetType::Rotation3D,
        AnimationTrackTargetType::Scaling3D
    };

    /* T/R/S tracks (1 original + 2 dummy tracks) for each object */
    CORRADE_COMPARE(animation->trackCount(),
        Containers::arraySize(targets)*Containers::arraySize(trackAnimationType));

    for(UnsignedInt i = 0; i < animation->trackCount(); i++) {
        CORRADE_VERIFY(animation->trackTarget(i) < Containers::arraySize(targets));
        const UnsignedInt channel = i / Containers::arraySize(trackAnimationType);
        const UnsignedInt indexWithinChannel = i % Containers::arraySize(trackAnimationType);
        AnimationTarget& target = targets[animation->trackTarget(i)];
        CORRADE_VERIFY(target.channel == ~0u || target.channel == channel);
        target.channel = channel;
        CORRADE_COMPARE(animation->trackTargetType(i), trackAnimationType[indexWithinChannel]);
        const UnsignedInt keyCount = animation->trackTargetType(i) == target.type
            ? target.keyCount : 1;
        CORRADE_COMPARE(animation->track(i).size(), keyCount);
    }

    const std::string str = out.str();
    CORRADE_VERIFY(!Containers::StringView{str}.contains(
        "Trade::AssimpImporter::animation(): ignoring dummy translation track in animation"));
    CORRADE_VERIFY(!Containers::StringView{str}.contains(
        "Trade::AssimpImporter::animation(): ignoring dummy rotation track in animation"));
    CORRADE_VERIFY(!Containers::StringView{str}.contains(
        "Trade::AssimpImporter::animation(): ignoring dummy scaling track in animation"));
}

void AssimpImporterTest::animationShortestPathOptimizationEnabled() {
    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("optimizeQuaternionShortestPath"));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "animation-patching.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(0), "Quaternion shortest-path patching");

    auto animation = importer->animation(0);
    CORRADE_VERIFY(animation);
    CORRADE_COMPARE(animation->trackCount(), 1);
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::Quaternion);
    Animation::TrackView<const Float, const Quaternion> track = animation->track<Quaternion>(0);
    constexpr Quaternion rotationValues[]{
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
    CORRADE_COMPARE_AS(track.values(), Containers::stridedArrayView(rotationValues), TestSuite::Compare::Container);

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

void AssimpImporterTest::animationShortestPathOptimizationDisabled() {
    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Explicitly disable */
    importer->configuration().setValue("optimizeQuaternionShortestPath", false);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "animation-patching.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(0), "Quaternion shortest-path patching");

    auto animation = importer->animation(0);
    CORRADE_VERIFY(animation);
    CORRADE_COMPARE(animation->trackCount(), 1);
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::Quaternion);
    Animation::TrackView<const Float, const Quaternion> track = animation->track<Quaternion>(0);

    /* Should be the same as in animation-patching.bin.in */
    constexpr Quaternion rotationValues[]{
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
    CORRADE_COMPARE_AS(track.values(), Containers::stridedArrayView(rotationValues), TestSuite::Compare::Container);

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

void AssimpImporterTest::animationQuaternionNormalizationEnabled() {
    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("normalizeQuaternions"));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "animation-patching.gltf")));
    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(1), "Quaternion normalization patching");

    Containers::Optional<AnimationData> animation;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        animation = importer->animation(1);
    }
    CORRADE_VERIFY(animation);
    CORRADE_VERIFY(Containers::StringView{out.str()}.contains(
        "Trade::AssimpImporter::animation(): quaternions in some rotation tracks were renormalized\n"));
    CORRADE_COMPARE(animation->trackCount(), 1);
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::Quaternion);

    Animation::TrackView<const Float, const Quaternion> track = animation->track<Quaternion>(0);
    constexpr Quaternion rotationValues[]{
        {{0.0f, 0.0f, 0.382683f}, 0.92388f},    // is normalized
        {{0.0f, 0.0f, 0.707107f}, 0.707107f},   // is not, renormalized
        {{0.0f, 0.0f, 0.382683f}, 0.92388f},    // is not, renormalized
    };
    /* There is a *ridiculous* bug in Assimp 5.0.1(?) with glTF animations that makes it
       ignore the value sampler size and always uses the key sampler size
       (instead of using the minimum of the two). Wouldn't be surprised
       if this produces an out-of-bounds access somewhere, too. */
    CORRADE_COMPARE_AS(track.values().prefix(Containers::arraySize(rotationValues)),
        Containers::stridedArrayView(rotationValues), TestSuite::Compare::Container);
}

void AssimpImporterTest::animationQuaternionNormalizationDisabled() {
    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Explicitly disable */
    CORRADE_VERIFY(importer->configuration().setValue("normalizeQuaternions", false));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "animation-patching.gltf")));
    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(1), "Quaternion normalization patching");

    auto animation = importer->animation(1);
    CORRADE_VERIFY(animation);
    CORRADE_COMPARE(animation->trackCount(), 1);
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::Quaternion);

    Animation::TrackView<const Float, const Quaternion> track = animation->track<Quaternion>(0);
    const Quaternion rotationValues[]{
        Quaternion{{0.0f, 0.0f, 0.382683f}, 0.92388f},      // is normalized
        Quaternion{{0.0f, 0.0f, 0.707107f}, 0.707107f}*2,   // is not
        Quaternion{{0.0f, 0.0f, 0.382683f}, 0.92388f}*2,    // is not
    };
    CORRADE_COMPARE_AS(track.values().prefix(Containers::arraySize(rotationValues)),
        Containers::stridedArrayView(rotationValues), TestSuite::Compare::Container);
}

void AssimpImporterTest::animationMergeEmpty() {
    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Enable animation merging */
    importer->configuration().setValue("mergeAnimationClips", true);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "empty.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->animationForName(""), -1);
}

void AssimpImporterTest::animationMerge() {
    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Enable animation merging, disabled by default */
    CORRADE_VERIFY(!importer->configuration().value<bool>("mergeAnimationClips"));
    importer->configuration().setValue("mergeAnimationClips", true);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "animation.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 1);
    CORRADE_COMPARE(importer->animationName(0), "");
    CORRADE_COMPARE(importer->animationForName(""), -1);

    auto animation = importer->animation(0);
    CORRADE_VERIFY(animation);
    CORRADE_VERIFY(!animation->importerState()); /* No particular clip */
    /*
        -   Nothing from the first animation
        -   Two rotation keys, four translation and scaling keys
            from the second animation
        -   Four T/R/S keys from the third animation
    */
    CORRADE_COMPARE(animation->data().size(),
        2*(sizeof(Float) + sizeof(Quaternion)) +
        2*4*(sizeof(Float) + sizeof(Vector3)) +
        4*(sizeof(Float) + sizeof(Quaternion)) +
        2*4*(sizeof(Float) + sizeof(Vector3)));

    CORRADE_COMPARE(animation->trackCount(), 6);

    /* Map from target to channel id */

    Containers::StaticArray<3, AnimationTarget> linearTargets = AnimationGltfLinearTargets;
    /* The order is the same (target = linear target + 3), we can re-use
       the data as long as we don't use keyCount */
    Containers::StaticArray<3, AnimationTarget> splineTargets = AnimationGltfLinearTargets;

    for(UnsignedInt i = 0; i < animation->trackCount(); i++) {
        const UnsignedInt targetId = animation->trackTarget(i);
        CORRADE_VERIFY(targetId < (Containers::arraySize(linearTargets) +
            Containers::arraySize(splineTargets)));
        AnimationTarget& target = targetId < Containers::arraySize(linearTargets)
            ? linearTargets[targetId]
            : splineTargets[targetId - Containers::arraySize(linearTargets)];
        CORRADE_COMPARE(target.channel, ~0u);
        target.channel = i;
    }

    /* Rotation, linearly interpolated */
    const UnsignedInt ch0 = linearTargets[0].channel;
    CORRADE_COMPARE(animation->trackType(ch0), AnimationTrackType::Quaternion);
    CORRADE_COMPARE(animation->trackTargetType(ch0), AnimationTrackTargetType::Rotation3D);
    CORRADE_COMPARE(animation->trackTarget(ch0), 0);
    Animation::TrackView<const Float, const Quaternion> rotation = animation->track<Quaternion>(ch0);
    CORRADE_COMPARE(rotation.interpolation(), Animation::Interpolation::Linear);
    CORRADE_COMPARE(rotation.at(1.875f), Quaternion::rotation(90.0_degf, Vector3::xAxis()));

    /* Translation, constant interpolated, sharing keys with scaling */
    const UnsignedInt ch1 = linearTargets[1].channel;
    CORRADE_COMPARE(animation->trackType(ch1), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(ch1), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(ch1), 1);
    Animation::TrackView<const Float, const Vector3> translation = animation->track<Vector3>(ch1);
    CORRADE_COMPARE(translation.interpolation(), Animation::Interpolation::Linear);
    CORRADE_COMPARE(translation.at(1.5f), Vector3::yAxis(2.5f));

    /* Scaling, linearly interpolated, sharing keys with translation */
    const UnsignedInt ch2 = linearTargets[2].channel;
    CORRADE_COMPARE(animation->trackType(ch2), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(ch2), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(ch2), 2);
    Animation::TrackView<const Float, const Vector3> scaling = animation->track<Vector3>(ch2);
    CORRADE_COMPARE(scaling.interpolation(), Animation::Interpolation::Linear);
    CORRADE_COMPARE(scaling.at(1.5f), Vector3::zScale(5.2f));

    /* Rotation, spline interpolated */
    const UnsignedInt ch3 = splineTargets[0].channel;
    CORRADE_COMPARE(animation->trackType(ch3), AnimationTrackType::Quaternion);
    CORRADE_COMPARE(animation->trackTargetType(ch3), AnimationTrackTargetType::Rotation3D);
    CORRADE_COMPARE(animation->trackTarget(ch3), 3);
    Animation::TrackView<const Float, const Quaternion> rotation2 = animation->track<Quaternion>(ch3);
    CORRADE_COMPARE(rotation2.interpolation(), Animation::Interpolation::Linear);

    /* Translation, spline interpolated */
    const UnsignedInt ch4 = splineTargets[1].channel;
    CORRADE_COMPARE(animation->trackType(ch4), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(ch4), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(ch4), 4);
    Animation::TrackView<const Float, const Vector3> translation2 = animation->track<Vector3>(ch4);
    CORRADE_COMPARE(translation2.interpolation(), Animation::Interpolation::Linear);

    /* Scaling, spline interpolated */
    const UnsignedInt ch5 = splineTargets[2].channel;
    CORRADE_COMPARE(animation->trackType(ch5), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(ch5), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(ch5), 5);
    Animation::TrackView<const Float, const Vector3> scaling2 = animation->track<Vector3>(ch5);
    CORRADE_COMPARE(scaling2.interpolation(), Animation::Interpolation::Linear);
}

/* The checks are identical to animation support so re-use that. */
bool supportsSkinning(const Containers::StringView fileName) {
    return supportsAnimation(fileName);
}

void calculateTransforms(Containers::ArrayView<Matrix4> transforms, Containers::ArrayView<ObjectData3D> objects, UnsignedInt objectId, const Matrix4& parentTransform = {}) {
    const Matrix4 transform = objects[objectId].transformation() * parentTransform;
    transforms[objectId] = transform;
    for(UnsignedInt childId: objects[objectId].children())
        calculateTransforms(transforms, objects, childId, transform);
}

void AssimpImporterTest::skin() {
    auto&& data = ExportedFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(!supportsSkinning(data.suffix))
        CORRADE_SKIP("Skin data for this file type is not supported with the current version of Assimp");

    /* Skinned mesh imported into Blender and then exported */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "skin" + std::string{data.suffix})));

    /* Two skins with their own meshes, one unskinned mesh */
    CORRADE_COMPARE(importer->meshCount(), 3);
    CORRADE_COMPARE(importer->skin3DCount(), 2);
    CORRADE_COMPARE(importer->skin3DForName("nonexistent"), -1);

    /* Get global node transforms, needed for testing inverse bind matrices */
    Containers::Array<ObjectData3D> objects{NoInit, importer->object3DCount()};
    Containers::Array<Matrix4> globalTransforms{importer->object3DCount()};

    for(UnsignedInt i = 0; i < importer->object3DCount(); ++i) {
        auto object = importer->object3D(i);
        CORRADE_VERIFY(object);
        new (&objects[i]) ObjectData3D{std::move(*object)};
    }

    const Int sceneId = importer->defaultScene();
    CORRADE_VERIFY(sceneId != -1);
    auto scene = importer->scene(sceneId);
    CORRADE_VERIFY(scene);

    for(UnsignedInt i: scene->children3D()) {
        calculateTransforms(globalTransforms, objects, i);
    }

    constexpr const char* meshNames[]{"Mesh_1", "Mesh_2"};
    constexpr const char* jointNames[][2]{
        {"Node_1", "Node_2"},
        {"Node_3", "Node_4"}
    };

    /* Some Blender exporters don't transform skin matrices correctly. Also see
       the comment for ExportedFileData. */
    const Matrix4 correction{data.correction.toMatrix()};

    for(UnsignedInt i = 0; i != Containers::arraySize(meshNames); ++i) {
        /* Skin names are taken from mesh names, skin order is arbitrary */
        const Int index = importer->skin3DForName(meshNames[i]);
        CORRADE_VERIFY(index != -1);
        CORRADE_COMPARE(importer->skin3DName(index), meshNames[i]);
        CORRADE_VERIFY(importer->meshForName(meshNames[i]) != -1);

        auto skin = importer->skin3D(index);
        CORRADE_VERIFY(skin);
        CORRADE_VERIFY(skin->importerState());

        /* Don't check joint order, only presence */
        auto joints = skin->joints();
        CORRADE_COMPARE(joints.size(), Containers::arraySize(jointNames[i]));
        for(const char* name: jointNames[i]) {
            auto found = std::find_if(joints.begin(), joints.end(), [&](UnsignedInt joint) {
                    /* Blender's Collada exporter adds an Armature_ prefix to
                       object names */
                    return Utility::String::endsWith(importer->object3DName(joint), name);
                });
            CORRADE_VERIFY(found != joints.end());
        }

        /* The exporters transform the inverse bind matrices quite a bit,
           making them hard to compare. Instead, check that their defining
           property holds: they're the inverse of the joint's original global
           transform. */
        auto bindMatrices = skin->inverseBindMatrices();
        CORRADE_COMPARE(bindMatrices.size(), joints.size());
        auto meshObject = importer->object3D(meshNames[i]);
        CORRADE_VERIFY(meshObject);
        CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*meshObject).skin(), index);
        const Matrix4 meshTransform = meshObject->transformation();
        for(UnsignedInt j = 0; j != joints.size(); ++j) {
            const Matrix4 invertedTransform = correction * meshTransform * globalTransforms[joints[j]].inverted();
            CORRADE_COMPARE(bindMatrices[j], invertedTransform);
        }
    }

    {
        /* Unskinned meshes and mesh nodes shouldn't have a skin */
        CORRADE_VERIFY(importer->meshForName("Plane") != -1);
        CORRADE_COMPARE(importer->skin3DForName("Plane"), -1);
        auto meshObject = importer->object3D("Plane");
        CORRADE_VERIFY(meshObject);
        CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*meshObject).skin(), -1);
    }
}

void AssimpImporterTest::skinNoMeshes() {
    if(!supportsSkinning(".gltf"))
        CORRADE_SKIP("glTF 2 skinning is not supported with the current version of Assimp");

    /* Reusing the TinyGltfImporter test file without meshes */
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, "skin.gltf")));

    /* Assimp only lets us access joints for each mesh. No mesh = no joints. */
    CORRADE_COMPARE(importer->meshCount(), 0);
    CORRADE_COMPARE(importer->skin3DCount(), 0);
}

void AssimpImporterTest::skinMergeEmpty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Enable skin merging */
    importer->configuration().setValue("mergeSkins", true);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer->skin3DCount(), 0);
    CORRADE_COMPARE(importer->skin3DForName(""), -1);

    for(UnsignedInt i = 0; i != importer->object3DCount(); ++i) {
        auto object = importer->object3D(i);
        if(object->instanceType() == ObjectInstanceType3D::Mesh)
            CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).skin(), -1);
    }
}

void AssimpImporterTest::skinMerge() {
    if(!supportsAnimation(".gltf"))
        CORRADE_SKIP("GLTF skinning is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Disabled by default */
    CORRADE_VERIFY(!importer->configuration().value<bool>("mergeSkins"));
    /* Enable skin merging */
    importer->configuration().setValue("mergeSkins", true);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "skin-shared.gltf")));

    CORRADE_COMPARE(importer->skin3DCount(), 1);
    CORRADE_COMPARE(importer->skin3DName(0), "");
    CORRADE_COMPARE(importer->skin3DForName(""), -1);

    for(const char* meshName: {"Mesh_1", "Mesh_2"}) {
        CORRADE_ITERATION(meshName);
        auto meshObject = importer->object3D(meshName);
        CORRADE_VERIFY(meshObject);
        CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*meshObject).skin(), 0);
    }

    auto skin = importer->skin3D(0);
    CORRADE_VERIFY(skin);
    CORRADE_VERIFY(!skin->importerState()); /* No particular skin */

    constexpr UnsignedInt expectedObjects[]{0, 0, 3, 4, 5, 6};
    constexpr Matrix4 expectedBones[] {
        /* 0 and 1: same name and matrix */
        Matrix4::translation(Vector3::xAxis()),
        Matrix4::translation(Vector3::xAxis()),
        /* 2: common name with 0 and 1, but different matrix */
        Matrix4::translation(Vector3::zAxis()),
        /* 3 and 4: same matrix, but different name */
        Matrix4::translation(Vector3::yAxis()),
        Matrix4::translation(Vector3::yAxis()),
        /* unique */
        Matrix4::translation(Vector3::zAxis())
    };
    /* Relying on the skin order here, this *might* break in the future */
    constexpr UnsignedInt mergeOrder[]{1, 2, 3, 4, 5};

    auto joints = skin->joints();
    auto inverseBindMatrices = skin->inverseBindMatrices();
    CORRADE_COMPARE(joints.size(), inverseBindMatrices.size());
    CORRADE_COMPARE(joints.size(), Containers::arraySize(mergeOrder));

    for(UnsignedInt i = 0; i != joints.size(); ++i) {
        CORRADE_ITERATION(i);
        CORRADE_COMPARE(joints[i], expectedObjects[mergeOrder[i]]);
        CORRADE_COMPARE(importer->object3D(joints[i])->instanceType(),
            ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(inverseBindMatrices[i], expectedBones[mergeOrder[i]]);
    }
}

void AssimpImporterTest::camera() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "camera.dae")));

    CORRADE_COMPARE(importer->cameraCount(), 1);
    Containers::Optional<CameraData> camera = importer->camera(0);
    CORRADE_VERIFY(camera);
    CORRADE_COMPARE(camera->fov(), 49.13434_degf);
    CORRADE_COMPARE(camera->near(), 0.123f);
    CORRADE_COMPARE(camera->far(), 123.0f);

    CORRADE_COMPARE(importer->object3DCount(), 1);

    Containers::Pointer<ObjectData3D> cameraObject = importer->object3D(0);
    CORRADE_COMPARE(cameraObject->instanceType(), ObjectInstanceType3D::Camera);
    CORRADE_COMPARE(cameraObject->instance(), 0);
}

void AssimpImporterTest::light() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "light.dae")));

    CORRADE_COMPARE(importer->lightCount(), 4);

    /* Spot light */
    {
        auto light = importer->light(0);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Spot);
        CORRADE_COMPARE(light->color(), (Color3{0.12f, 0.24f, 0.36f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
        CORRADE_COMPARE(light->attenuation(), (Vector3{0.1f, 0.3f, 0.5f}));
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->innerConeAngle(), 45.0_degf);
        /* Not sure how it got calculated from 0.15 falloff exponent, but let's
           just trust Assimp for once */
        CORRADE_COMPARE(light->outerConeAngle(), 135.0_degf);

    /* Point light */
    } {
        auto light = importer->light(1);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{0.5f, 0.25f, 0.05f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
        CORRADE_COMPARE(light->attenuation(), (Vector3{0.1f, 0.7f, 0.9f}));
        CORRADE_COMPARE(light->range(), Constants::inf());

    /* Directional light */
    } {
        auto light = importer->light(2);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Directional);
        /* This one has intensity of 10, which gets premultiplied to the
           color */
        CORRADE_COMPARE(light->color(), (Color3{1.0f, 0.15f, 0.45f})*10.0f);
        CORRADE_COMPARE(light->intensity(), 1.0f);

    /* Ambient light -- imported as Point with no attenuation */
    } {
        auto light = importer->light(3);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{0.01f, 0.02f, 0.05f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f, 0.0f, 0.0f}));
    }
}

void AssimpImporterTest::lightUnsupported() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    /* The light-area.blend file contains an area light, but Assimp can't open
       Blender 2.8 files yet it seems. So I saved it from Blender as FBX and
       opening that, but somehow the light lost its area type in process and
       it's now UNKNOWN instead. Which is fine I guess as I want to test just
       the failure anyway. */
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "light-area.fbx")));
    CORRADE_COMPARE(importer->lightCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->light(0));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::light(): light type 0 is not supported\n");
}

void AssimpImporterTest::materialColor() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-color.dae")));

    CORRADE_COMPARE(importer->materialCount(), 1);
    Containers::Optional<MaterialData> material = importer->material(0);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType::Phong);
    CORRADE_COMPARE(material->layerCount(), 1);
    CORRADE_COMPARE(material->attributeCount(), 4);

    const auto& phong = material->as<PhongMaterialData>();
    {
        CORRADE_EXPECT_FAIL("Assimp sets ambient alpha to 0, ignoring the actual value (for COLLADA at least).");
        CORRADE_COMPARE(phong.ambientColor(), (Color4{0.1f, 0.05f, 0.1f, 0.9f}));
    } {
        CORRADE_COMPARE(phong.ambientColor(), (Color4{0.1f, 0.05f, 0.1f, 0.0f}));
    }
    CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.08f, 0.16f, 0.24f, 0.7f}));
    CORRADE_COMPARE(phong.specularColor(), (Color4{0.15f, 0.1f, 0.05f, 0.5f}));
    CORRADE_COMPARE(phong.shininess(), 50.0f);

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

void AssimpImporterTest::materialTexture() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae")));

    CORRADE_COMPARE(importer->materialCount(), 1);
    Containers::Optional<MaterialData> material = importer->material(0);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType::Phong);
    CORRADE_COMPARE(material->layerCount(), 1);
    CORRADE_COMPARE(material->attributeCount(), 10); /* includes zero texcoords */

    const auto& phong = material->as<PhongMaterialData>();
    {
        CORRADE_EXPECT_FAIL("Assimp, the stupid thing, imports ambient textures in COLLADA files as LIGHTMAP.");
        CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::AmbientTexture));
    }
    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
    CORRADE_VERIFY(phong.hasSpecularTexture());
    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::NormalTexture));

    /* Ambient texture *is* there, but treated as LIGHTMAP and thus unknown to
       the material. */
    CORRADE_COMPARE(importer->textureCount(), 4);
    /* (This would assert now) */
    //CORRADE_COMPARE(phong.ambientTexture(), 1);
    CORRADE_COMPARE(phong.diffuseTexture(), 2);
    CORRADE_COMPARE(phong.specularTexture(), 1);
    CORRADE_COMPARE(phong.normalTexture(), 3);

    /* Colors should stay at their defaults as these aren't provided in the
       file */
    CORRADE_COMPARE(phong.ambientColor(), (Color4{0.0f, 0.0f, 0.0f, 1.0f}));
    CORRADE_COMPARE(phong.diffuseColor(), (Color4{1.0f, 1.0f, 1.0f, 1.0f}));
    CORRADE_COMPARE(phong.specularColor(), (Color4{1.0f, 1.0f, 1.0f, 1.0f}));
}

void AssimpImporterTest::materialColorTexture() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-color-texture.obj")));

    {
        CORRADE_EXPECT_FAIL("Assimp reports one material more than it should for OBJ and the first is always useless.");
        CORRADE_COMPARE(importer->materialCount(), 1);
    }
    CORRADE_COMPARE(importer->materialCount(), 2);

    Containers::Optional<MaterialData> material = importer->material(1);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType::Phong);
    CORRADE_COMPARE(material->layerCount(), 1);

    /* Newer versions import also useless zero texcoords. Not sure if it's
       since 4.0 or 5.0, but definitely 3.2 returns 7. */
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    if(version < 400)
        CORRADE_COMPARE(material->attributeCount(), 7);
    else
        CORRADE_COMPARE(material->attributeCount(), 10);

    const auto& phong = material->as<PhongMaterialData>();
    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::AmbientTexture));
    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
    CORRADE_VERIFY(phong.hasSpecularTexture());
    CORRADE_COMPARE(phong.ambientTexture(), 1);
    CORRADE_COMPARE(phong.diffuseTexture(), 0);
    CORRADE_COMPARE(phong.specularTexture(), 2);

    /* Alpha not supported by OBJ, should be set to 1 */
    CORRADE_COMPARE(phong.ambientColor(), (Color4{0.1f, 0.05f, 0.1f, 1.0f}));
    CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.08f, 0.16f, 0.24f, 1.0f}));
    CORRADE_COMPARE(phong.specularColor(), (Color4{0.15f, 0.1f, 0.05f, 1.0f}));
}

void AssimpImporterTest::materialStlWhiteAmbientPatch() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "quad.stl")));

    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<MaterialData> material;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        material = importer->material(0);
    }

    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType::Phong);
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    {
        /* aiGetVersion*() returns 401 for assimp 5, FFS, so we have to check
           differently. See CMakeLists.txt for details. */
        CORRADE_EXPECT_FAIL_IF(version < 401 || ASSIMP_IS_VERSION_5,
            "Assimp < 4.1 and >= 5.0 behaves properly regarding STL material ambient");
        CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::material(): white ambient detected, forcing back to black\n");
    }

    const auto& phong = material->as<PhongMaterialData>();
    CORRADE_VERIFY(!phong.hasAttribute(MaterialAttribute::AmbientTexture));
    /* WHY SO COMPLICATED, COME ON */
    if(version < 401 || ASSIMP_IS_VERSION_5)
        CORRADE_COMPARE(phong.ambientColor(), Color3{0.05f});
    else
        CORRADE_COMPARE(phong.ambientColor(), 0x000000_srgbf);

    /* ASS IMP WHAT?! WHY 3.2 is different from 3.0 and 4.0?! */
    if(version == 302) {
        CORRADE_COMPARE(phong.specularColor(), Color3{0.6f});
        CORRADE_COMPARE(phong.diffuseColor(), Color3{0.6f});
    } else {
        CORRADE_COMPARE(phong.specularColor(), 0xffffff_srgbf);
        CORRADE_COMPARE(phong.diffuseColor(), 0xffffff_srgbf);
    }
    /* This value is not supplied by Assimp for STL models, so we keep it at
       default */
    CORRADE_COMPARE(phong.shininess(), 80.0f);
}

void AssimpImporterTest::materialWhiteAmbientTexture() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "texture-ambient.obj")));

    /* ASS IMP reports TWO materials for an OBJ. The parser code is so lazy
       that it just has the first material totally empty. Wonderful. Lost one
       hour on this and my hair is even greyer now. */
    {
        CORRADE_EXPECT_FAIL("Assimp reports one material more than it should for OBJ and the first is always useless.");
        CORRADE_COMPARE(importer->materialCount(), 1);
    } {
        CORRADE_COMPARE(importer->materialCount(), 2);
    }

    Containers::Optional<MaterialData> material;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        material = importer->material(1);
    }

    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType::Phong);
    CORRADE_COMPARE(importer->textureCount(), 1);
    CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::AmbientTexture));
    /* It shouldn't be complaining about white ambient in this case */
    CORRADE_COMPARE(out.str(), "");
}

void AssimpImporterTest::materialMultipleTextures() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "multiple-textures.obj")));

    /* See materialWhiteAmbientTexture() for a rant. */
    {
        CORRADE_EXPECT_FAIL("Assimp reports one material more than it should for OBJ and the first is always useless.");
        CORRADE_COMPARE(importer->materialCount(), 3);
    } {
        CORRADE_COMPARE(importer->materialCount(), 3 + 1);
    }

    /* Seven textures, but using just four distinct images */
    CORRADE_COMPARE(importer->textureCount(), 7);
    CORRADE_COMPARE(importer->image2DCount(), 4);

    /* Check that texture ID assignment is correct */
    {
        Containers::Optional<MaterialData> material = importer->material("ambient_diffuse");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->types(), MaterialType::Phong);

        const auto& phong = material->as<PhongMaterialData>();
        CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::AmbientTexture));
        CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
        CORRADE_COMPARE(phong.ambientTexture(), 1); /* r.png */
        CORRADE_COMPARE(phong.diffuseTexture(), 0); /* g.png */
    } {
        Containers::Optional<MaterialData> material = importer->material("diffuse_specular");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->types(), MaterialType::Phong);

        const auto& phong = material->as<PhongMaterialData>();
        CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
        CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::SpecularTexture));
        CORRADE_COMPARE(phong.diffuseTexture(), 2); /* b.png */
        CORRADE_COMPARE(phong.specularTexture(), 3); /* y.png */
    } {
        Containers::Optional<MaterialData> material = importer->material("all");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->types(), MaterialType::Phong);

        const auto& phong = material->as<PhongMaterialData>();
        CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::AmbientTexture));
        CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
        CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::SpecularTexture));
        CORRADE_COMPARE(phong.ambientTexture(), 5); /* y.png */
        CORRADE_COMPARE(phong.diffuseTexture(), 4); /* r.png */
        CORRADE_COMPARE(phong.specularTexture(), 6); /* g.png */
    }

    /* Check that image ID assignment is correct */
    {
        Containers::Optional<TextureData> texture = importer->texture(0);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 0); /* g.png */
    } {
        Containers::Optional<TextureData> texture = importer->texture(1);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 1); /* r.png */
    } {
        Containers::Optional<TextureData> texture = importer->texture(2);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 2); /* b.png */
    } {
        Containers::Optional<TextureData> texture = importer->texture(3);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 3); /* y.png */
    } {
        Containers::Optional<TextureData> texture = importer->texture(4);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 1); /* r.png */
    } {
        Containers::Optional<TextureData> texture = importer->texture(5);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 3); /* y.png */
    } {
        Containers::Optional<TextureData> texture = importer->texture(6);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 0); /* g.png */
    }

    /* Check that correct images are imported */
    {
        Containers::Optional<ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), Vector2i(1));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0x00ff00_rgb); /* g.png */
    } {
        Containers::Optional<ImageData2D> image = importer->image2D(1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), Vector2i(1));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0xff0000_rgb); /* r.png */
    } {
        Containers::Optional<ImageData2D> image = importer->image2D(2);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), Vector2i(1));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0x0000ff_rgb); /* b.png */
    } {
        Containers::Optional<ImageData2D> image = importer->image2D(3);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), Vector2i(1));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0xffff00_rgb); /* y.png */
    }
}

void AssimpImporterTest::materialTextureCoordinateSets() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-coordinate-sets.dae")));

    Containers::Optional<MaterialData> material = importer->material(0);
    CORRADE_VERIFY(material);
    const auto& phong = material->as<PhongMaterialData>();

    CORRADE_VERIFY(phong.hasTextureCoordinates());
    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::SpecularTexture));
    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::NormalTexture));
    CORRADE_COMPARE(phong.diffuseTextureCoordinates(), 2);
    CORRADE_COMPARE(phong.specularTextureCoordinates(), 3);
    CORRADE_COMPARE(phong.normalTextureCoordinates(), 2);
}

void AssimpImporterTest::mesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 1);

    CORRADE_COMPARE(importer->meshName(0), "Cube");
    CORRADE_COMPARE(importer->meshForName("Cube"), 0);
    CORRADE_COMPARE(importer->meshForName("nonexistent"), -1);

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1, 2}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 6);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Normal), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Normal),
        Containers::arrayView<Vector3>({
            {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Tangent), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Tangent),
        Containers::arrayView<Vector3>({
            {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Bitangent), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Bitangent),
        Containers::arrayView<Vector3>({
            {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::TextureCoordinates), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        Containers::arrayView<Vector2>({
            {0.5f, 1.0f}, {0.75f, 0.5f}, {0.5f, 0.9f}
        }), TestSuite::Compare::Container);

    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    {
        CORRADE_EXPECT_FAIL_IF(version < 302,
            "Assimp < 3.2 loads incorrect alpha value for the last color.");
        CORRADE_COMPARE_AS(mesh->attribute<Vector4>(MeshAttribute::Color),
        Containers::arrayView<Vector4>({
            {1.0f, 0.25f, 0.24f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.1f, 0.2f, 0.3f, 1.0f}
        }), TestSuite::Compare::Container);
    }

    Containers::Pointer<ObjectData3D> meshObject = importer->object3D(0);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(meshObject->instance(), 0);
    CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*meshObject).skin(), -1);
}

void AssimpImporterTest::pointMesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "points.obj")));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 1);

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Points);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1, 2, 0}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {0.5f, 2.0f, 3.0f}, {2.0f, 3.0f, 5.0f}, {0.0f, 1.5f, 1.0f}
        }), TestSuite::Compare::Container);

    Containers::Pointer<ObjectData3D> meshObject = importer->object3D(0);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(meshObject->instance(), 0);
}

void AssimpImporterTest::lineMesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "line.dae")));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 1);

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Lines);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}
        }), TestSuite::Compare::Container);

    Containers::Pointer<ObjectData3D> meshObject = importer->object3D(0);
    CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(meshObject->instance(), 0);
}

void AssimpImporterTest::meshCustomAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "mesh.dae")));

    /* Custom attributes should be available right after loading a file,
       even for files without joint weights */
    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    CORRADE_COMPARE(jointsAttribute, meshAttributeCustom(0));
    CORRADE_COMPARE(importer->meshAttributeName(jointsAttribute), "JOINTS");

    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");
    CORRADE_COMPARE(weightsAttribute, meshAttributeCustom(1));
    CORRADE_COMPARE(importer->meshAttributeName(weightsAttribute), "WEIGHTS");

    /* These two are the only possible custom attributes */
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(2)), "");
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(564)), "");
    CORRADE_COMPARE(importer->meshAttributeForName("thing"), MeshAttribute{});
}

constexpr Vector4ui MeshSkinningAttributesJointData[]{
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 0, 0},
    {1, 0, 0, 0},
    {1, 0, 0, 0}
};

constexpr Vector4 MeshSkinningAttributesWeightData[]{
    {1.0f, 0.0f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.75f, 0.25f, 0.0f, 0.0f},
    {0.75f, 0.25f, 0.0f, 0.0f},
    {0.5f, 0.5f, 0.0f, 0.0f},
    {0.5f, 0.5f, 0.0f, 0.0f},
    {0.25f, 0.75f, 0.0f, 0.0f},
    {0.25f, 0.75f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.0f, 0.0f}
};

void AssimpImporterTest::meshSkinningAttributes() {
    auto&& data = ExportedFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(!supportsSkinning(data.suffix))
        CORRADE_SKIP("Skin data for this file type is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "skin" + std::string{data.suffix})));

    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");

    for(const char* meshName: {"Mesh_1", "Mesh_2"}) {
        Containers::Optional<MeshData> mesh;
        std::ostringstream out;
        {
            Warning redirectWarning{&out};
            mesh = importer->mesh(meshName);
        }
        /* No warning about glTF dropping sets of weights */
        CORRADE_VERIFY(out.str().empty());

        CORRADE_VERIFY(mesh);
        CORRADE_VERIFY(mesh->hasAttribute(jointsAttribute));
        CORRADE_COMPARE(mesh->attributeCount(jointsAttribute), 1);
        CORRADE_COMPARE(mesh->attributeFormat(jointsAttribute), VertexFormat::Vector4ui);
        CORRADE_COMPARE_AS(mesh->attribute<Vector4ui>(jointsAttribute),
            Containers::arrayView(MeshSkinningAttributesJointData),
            TestSuite::Compare::Container);
        CORRADE_VERIFY(mesh->hasAttribute(weightsAttribute));
        CORRADE_COMPARE(mesh->attributeCount(weightsAttribute), 1);
        CORRADE_COMPARE(mesh->attributeFormat(weightsAttribute), VertexFormat::Vector4);
        CORRADE_COMPARE_AS(mesh->attribute<Vector4>(weightsAttribute),
            Containers::arrayView(MeshSkinningAttributesWeightData),
            TestSuite::Compare::Container);
    }

    /* No skin joint node using this mesh */
    auto mesh = importer->mesh("Plane");
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->hasAttribute(jointsAttribute));
}

void AssimpImporterTest::meshSkinningAttributesMultiple() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Disable default limit, 0 = no limit  */
    CORRADE_COMPARE(importer->configuration().value<UnsignedInt>("maxJointWeights"), 4);
    importer->configuration().setValue("maxJointWeights", 0);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "skin-multiple-sets.dae")));

    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");

    constexpr UnsignedInt AttributeCount = 3;

    auto mesh = importer->mesh("Mesh_1");
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->hasAttribute(jointsAttribute));
    CORRADE_COMPARE(mesh->attributeCount(jointsAttribute), AttributeCount);
    CORRADE_COMPARE(mesh->attributeFormat(jointsAttribute), VertexFormat::Vector4ui);
    CORRADE_VERIFY(mesh->hasAttribute(weightsAttribute));
    CORRADE_COMPARE(mesh->attributeCount(weightsAttribute), AttributeCount);
    CORRADE_COMPARE(mesh->attributeFormat(weightsAttribute), VertexFormat::Vector4);

    for(UnsignedInt i = 0; i != AttributeCount; ++i) {
        auto joints = mesh->attribute<Vector4ui>(jointsAttribute, i);
        auto weights = mesh->attribute<Vector4>(weightsAttribute, i);
        const Vector4ui jointValues = Vector4ui{0, 1, 2, 3} + Vector4ui{i*4};
        constexpr Vector4 weightValues = Vector4{0.083333f};
        for(UnsignedInt v = 0; v != joints.size(); ++v) {
            CORRADE_COMPARE(joints[v], jointValues);
            CORRADE_COMPARE(weights[v], weightValues);
        }
    }
}

void AssimpImporterTest::meshSkinningAttributesMultipleGltf() {
    if(!supportsSkinning(".gltf"))
        CORRADE_SKIP("glTF 2 skinning is not supported with the current version of Assimp");

    /* Assimp glTF 2 importer only reads the last(!) set of joint weights */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().setValue("maxJointWeights", 0);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "skin-multiple-sets.gltf")));

    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");

    Containers::Optional<MeshData> mesh;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        mesh = importer->mesh("Mesh");
    }

    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->hasAttribute(jointsAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(jointsAttribute), VertexFormat::Vector4ui);
    CORRADE_VERIFY(mesh->hasAttribute(weightsAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(weightsAttribute), VertexFormat::Vector4);

    {
        CORRADE_EXPECT_FAIL("glTF 2 importer only reads one set of joint weights.");
        CORRADE_COMPARE(mesh->attributeCount(jointsAttribute), 2);
        CORRADE_COMPARE(mesh->attributeCount(weightsAttribute), 2);
    }

    auto joints = mesh->attribute<Vector4ui>(jointsAttribute);
    CORRADE_VERIFY(joints);
    auto weights = mesh->attribute<Vector4>(weightsAttribute);
    CORRADE_VERIFY(weights);

    CORRADE_COMPARE(out.str(),
        "Trade::AssimpImporter::mesh(): found non-normalized joint weights, possibly "
        "a result of Assimp reading joint weights incorrectly. Consult the importer "
        "documentation for more information\n");

    {
        CORRADE_EXPECT_FAIL("glTF 2 importer only reads the last set of joint weights.");
        constexpr Vector4ui joint{0, 1, 2, 3};
        constexpr Vector4 weight{0.125f};
        CORRADE_COMPARE(joints.front(), joint);
        CORRADE_COMPARE(weights.front(), weight);
    }

    /* Colors, on the other hand, work */
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Color));
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Color), 2);

    auto colors1 = mesh->attribute<Vector4>(MeshAttribute::Color, 0);
    auto colors2 = mesh->attribute<Vector4>(MeshAttribute::Color, 1);
    CORRADE_VERIFY(colors1);
    CORRADE_VERIFY(colors2);
    CORRADE_COMPARE(colors1.front(), Color4{1.0f});
    CORRADE_COMPARE(colors2.front(), Color4{0.5f});
}

void AssimpImporterTest::meshSkinningAttributesMaxJointWeights() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().setValue("maxJointWeights", 6);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "skin-multiple-sets.dae")));

    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");

    /* 6 weights = 2 sets of 4, last two weights zero */
    constexpr UnsignedInt AttributeCount = 2;

    auto mesh = importer->mesh("Mesh_1");
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->hasAttribute(jointsAttribute));
    CORRADE_COMPARE(mesh->attributeCount(jointsAttribute), AttributeCount);
    CORRADE_COMPARE(mesh->attributeFormat(jointsAttribute), VertexFormat::Vector4ui);
    CORRADE_VERIFY(mesh->hasAttribute(weightsAttribute));
    CORRADE_COMPARE(mesh->attributeCount(weightsAttribute), AttributeCount);
    CORRADE_COMPARE(mesh->attributeFormat(weightsAttribute), VertexFormat::Vector4);

    constexpr Vector4ui jointValues[]{{0, 1, 2, 3}, {4, 5, 0, 0}};
    /* Assimp normalized the weights */
    constexpr Float Weight = 0.083333f * 2.0f;
    constexpr Vector4 weightValues[]{Vector4{Weight}, Vector4::pad(Vector2{Weight})};

    for(UnsignedInt i = 0; i != AttributeCount; ++i) {
        auto joints = mesh->attribute<Vector4ui>(jointsAttribute, i);
        auto weights = mesh->attribute<Vector4>(weightsAttribute, i);
        for(UnsignedInt v = 0; v != joints.size(); ++v) {
            CORRADE_COMPARE(joints[v], jointValues[i]);
            CORRADE_COMPARE(weights[v], weightValues[i]);
        }
    }
}

void AssimpImporterTest::meshSkinningAttributesDummyWeightRemoval() {
    if(!supportsSkinning(".gltf"))
        CORRADE_SKIP("glTF 2 skinning is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().setValue("maxJointWeights", 0);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "skin-dummy-weights.gltf")));

    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");

    auto mesh = importer->mesh("Mesh");
    CORRADE_VERIFY(mesh);

    /* Without ignoring dummy weights, the max joint count per vertex
       would be too high, giving us extra sets of (empty) weights. */
    CORRADE_COMPARE(mesh->attributeCount(jointsAttribute), 1);
    CORRADE_COMPARE(mesh->attributeCount(weightsAttribute), 1);

    auto joints = mesh->attribute<Vector4ui>(jointsAttribute);
    CORRADE_VERIFY(joints);
    auto weights = mesh->attribute<Vector4>(weightsAttribute);
    CORRADE_VERIFY(weights);

    constexpr Vector4ui joint{0, 1, 2, 5};
    constexpr Vector4 weight{0.25f};
    CORRADE_COMPARE(joints.front(), joint);
    CORRADE_COMPARE(weights.front(), weight);
}

void AssimpImporterTest::meshSkinningAttributesMerge() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().setValue("mergeSkins", true);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "skin.dae")));

    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");

    /* The first mesh (inside aiScene::mMeshes, order is arbitrary) has its
       bones added to the global bone list first, only the second one has
       shifted joint indices */
    Containers::Array<Vector4ui> shiftedJointData{Containers::arraySize(MeshSkinningAttributesJointData)};
    for(UnsignedInt i = 0; i != shiftedJointData.size(); ++i) {
        /* Shift by 2 where weight is non-zero */
        const BoolVector4 nonZero = Math::notEqual(MeshSkinningAttributesWeightData[i], Vector4{0.0f});
        const Vector4ui mask{nonZero[0], nonZero[1], nonZero[2], nonZero[3]};
        shiftedJointData[i] = MeshSkinningAttributesJointData[i] + (mask * 2);
    }

    {
        const Int id = importer->meshForName("Mesh_1");
        CORRADE_VERIFY(id != -1);
        auto mesh = importer->mesh(id);
        CORRADE_VERIFY(mesh);
        CORRADE_VERIFY(mesh->hasAttribute(jointsAttribute));
        CORRADE_COMPARE(mesh->attributeFormat(jointsAttribute), VertexFormat::Vector4ui);
        auto jointData = id == 0
            ? Containers::arrayView(MeshSkinningAttributesJointData)
            : Containers::arrayView(shiftedJointData);
        CORRADE_COMPARE_AS(mesh->attribute<Vector4ui>(jointsAttribute),
            Containers::arrayView(jointData), TestSuite::Compare::Container);
        CORRADE_VERIFY(mesh->hasAttribute(weightsAttribute));
        CORRADE_COMPARE(mesh->attributeFormat(weightsAttribute), VertexFormat::Vector4);
        CORRADE_COMPARE_AS(mesh->attribute<Vector4>(weightsAttribute),
            Containers::arrayView(MeshSkinningAttributesWeightData),
            TestSuite::Compare::Container);
    } {
        const Int id = importer->meshForName("Mesh_2");
        CORRADE_VERIFY(id != -1);
        auto mesh = importer->mesh(id);
        CORRADE_VERIFY(mesh);
        CORRADE_VERIFY(mesh->hasAttribute(jointsAttribute));
        CORRADE_COMPARE(mesh->attributeFormat(jointsAttribute), VertexFormat::Vector4ui);
        auto jointData = id == 0
            ? Containers::arrayView(MeshSkinningAttributesJointData)
            : Containers::arrayView(shiftedJointData);
        CORRADE_COMPARE_AS(mesh->attribute<Vector4ui>(jointsAttribute),
            Containers::arrayView(jointData), TestSuite::Compare::Container);
        /* Weights should stay the same */
        CORRADE_VERIFY(mesh->hasAttribute(weightsAttribute));
        CORRADE_COMPARE(mesh->attributeFormat(weightsAttribute), VertexFormat::Vector4);
        CORRADE_COMPARE_AS(mesh->attribute<Vector4>(weightsAttribute),
            Containers::arrayView(MeshSkinningAttributesWeightData),
            TestSuite::Compare::Container);
    }
}

void AssimpImporterTest::meshMultiplePrimitives() {
    /* Possibly broken in other versions too (4.1 and 5 works, 3.2 doesn't) */
    if(aiGetVersionMajor()*100 + aiGetVersionMinor() <= 302)
        CORRADE_SKIP("Assimp 3.2 doesn't recognize primitives used in the test COLLADA file.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR,
        "mesh-multiple-primitives.dae")));

    /* Four meshes, but one has three primitives and one two. */
    CORRADE_COMPARE(importer->meshCount(), 5);
    {
        CORRADE_COMPARE(importer->meshName(0), "Multi-primitive triangle fan, line strip");
        CORRADE_COMPARE(importer->meshName(1), "Multi-primitive triangle fan, line strip");
        CORRADE_COMPARE(importer->meshForName("Multi-primitive triangle fan, line strip"), 0);

        auto mesh0 = importer->mesh(0);
        CORRADE_VERIFY(mesh0);
        CORRADE_COMPARE(mesh0->primitive(), MeshPrimitive::Triangles);
        auto mesh1 = importer->mesh(1);
        CORRADE_VERIFY(mesh1);
        CORRADE_COMPARE(mesh1->primitive(), MeshPrimitive::Lines);
    } {
        CORRADE_COMPARE(importer->meshName(2), "Multi-primitive lines, triangles, triangle strip");
        CORRADE_COMPARE(importer->meshName(3), "Multi-primitive lines, triangles, triangle strip");
        CORRADE_COMPARE(importer->meshName(4), "Multi-primitive lines, triangles, triangle strip");
        CORRADE_COMPARE(importer->meshForName("Multi-primitive lines, triangles, triangle strip"), 2);

        auto mesh2 = importer->mesh(2);
        CORRADE_VERIFY(mesh2);
        CORRADE_COMPARE(mesh2->primitive(), MeshPrimitive::Lines);
        auto mesh3 = importer->mesh(3);
        CORRADE_VERIFY(mesh3);
        CORRADE_COMPARE(mesh3->primitive(), MeshPrimitive::Triangles);
        auto mesh4 = importer->mesh(4);
        CORRADE_VERIFY(mesh4);
        CORRADE_COMPARE(mesh4->primitive(), MeshPrimitive::Triangles);
    }

    /* Five objects, but two refer a three-primitive mesh and one refers a
       two-primitive one */
    CORRADE_COMPARE(importer->object3DCount(), 9);
    {
        CORRADE_COMPARE(importer->object3DName(0), "Using_the_second_mesh__should_have_4_children");
        CORRADE_COMPARE(importer->object3DName(1), "Using_the_second_mesh__should_have_4_children");
        CORRADE_COMPARE(importer->object3DName(2), "Using_the_second_mesh__should_have_4_children");
        CORRADE_COMPARE(importer->object3DForName("Using_the_second_mesh__should_have_4_children"), 0);
        auto object = importer->object3D(0);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 2);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{1, 2, 7}));

        auto child1 = importer->object3D(1);
        CORRADE_VERIFY(child1);
        CORRADE_COMPARE(child1->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child1->instance(), 3);
        CORRADE_COMPARE(child1->children(), {});
        CORRADE_COMPARE(child1->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child1->translation(), Vector3{});
        CORRADE_COMPARE(child1->rotation(), Quaternion{});
        CORRADE_COMPARE(child1->scaling(), Vector3{1.0f});

        auto child2 = importer->object3D(2);
        CORRADE_VERIFY(child2);
        CORRADE_COMPARE(child2->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child2->instance(), 4);
        CORRADE_COMPARE(child2->children(), {});
        CORRADE_COMPARE(child2->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child2->translation(), Vector3{});
        CORRADE_COMPARE(child2->rotation(), Quaternion{});
        CORRADE_COMPARE(child2->scaling(), Vector3{1.0f});
    } {
        CORRADE_COMPARE(importer->object3DName(3), "Just_a_non-mesh_node");
        CORRADE_COMPARE(importer->object3DForName("Just_a_non-mesh_node"), 3);
        auto object = importer->object3D(3);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->instance(), -1);
        CORRADE_COMPARE(object->children(), {});
    } {
        CORRADE_COMPARE(importer->object3DName(4), "Using_the_second_mesh_again__again_2_children");
        CORRADE_COMPARE(importer->object3DName(5), "Using_the_second_mesh_again__again_2_children");
        CORRADE_COMPARE(importer->object3DName(6), "Using_the_second_mesh_again__again_2_children");
        CORRADE_COMPARE(importer->object3DForName("Using_the_second_mesh_again__again_2_children"), 4);
        auto object = importer->object3D(4);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 2);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{5, 6}));

        auto child5 = importer->object3D(5);
        CORRADE_VERIFY(child5);
        CORRADE_COMPARE(child5->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child5->instance(), 3);
        CORRADE_COMPARE(child5->children(), {});
        CORRADE_COMPARE(child5->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child5->translation(), Vector3{});
        CORRADE_COMPARE(child5->rotation(), Quaternion{});
        CORRADE_COMPARE(child5->scaling(), Vector3{1.0f});

        auto child6 = importer->object3D(6);
        CORRADE_VERIFY(child6);
        CORRADE_COMPARE(child6->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child6->instance(), 4);
        CORRADE_COMPARE(child6->children(), {});
        CORRADE_COMPARE(child6->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child6->translation(), Vector3{});
        CORRADE_COMPARE(child6->rotation(), Quaternion{});
        CORRADE_COMPARE(child6->scaling(), Vector3{1.0f});
    } {
        CORRADE_COMPARE(importer->object3DName(7), "Using_the_fourth_mesh__1_child");
        CORRADE_COMPARE(importer->object3DName(8), "Using_the_fourth_mesh__1_child");
        CORRADE_COMPARE(importer->object3DForName("Using_the_fourth_mesh__1_child"), 7);
        auto object = importer->object3D(7);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 0);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{8}));

        auto child8 = importer->object3D(8);
        CORRADE_VERIFY(child8);
        CORRADE_COMPARE(child8->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(child8->instance(), 1);
        CORRADE_COMPARE(child8->children(), {});
        CORRADE_COMPARE(child8->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(child8->translation(), Vector3{});
        CORRADE_COMPARE(child8->rotation(), Quaternion{});
        CORRADE_COMPARE(child8->scaling(), Vector3{1.0f});
    }
}

void AssimpImporterTest::emptyCollada() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    /* Instead of giving out an empty file, assimp fails on opening, but only
       for COLLADA, not for e.g. glTF. I have a different opinion about the
       behavior, but whatever. It's also INTERESTING that supplying an empty
       DAE through file callbacks results in a completely different message --
       see fileCallbackEmptyFile(). */
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "empty.dae")));
}

void AssimpImporterTest::emptyGltf() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    if(version < 401)
        CORRADE_SKIP("glTF 2 is supported since Assimp 4.1.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, "empty.gltf")));
    CORRADE_COMPARE(importer->defaultScene(), -1);
    CORRADE_COMPARE(importer->sceneCount(), 0);
    CORRADE_COMPARE(importer->object3DCount(), 0);

    /* No crazy meshes created for an empty glTF file, unlike with COLLADA
       files that have no meshes */
    CORRADE_COMPARE(importer->meshCount(), 0);
}

void AssimpImporterTest::scene() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae")));

    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 2);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), {0});

    Containers::Pointer<ObjectData3D> parent = importer->object3D(0);
    CORRADE_COMPARE(parent->children(), {1});
    CORRADE_COMPARE(parent->instanceType(), ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(parent->transformation(), Matrix4::scaling({1.0f, 2.0f, 3.0f}));

    Containers::Pointer<ObjectData3D> childObject = importer->object3D(1);
    CORRADE_COMPARE(childObject->transformation(), (Matrix4{
        {0.813798f, 0.469846f, -0.34202f, 0.0f},
        {-0.44097f, 0.882564f, 0.163176f, 0.0f},
        {0.378522f, 0.0180283f, 0.925417f, 0.0f},
        {1.0f, 2.0f, 3.0f, 1.0f}}));

    CORRADE_COMPARE(importer->object3DForName("Parent"), 0);
    CORRADE_COMPARE(importer->object3DForName("Child"), 1);
    CORRADE_COMPARE(importer->object3DName(0), "Parent");
    CORRADE_COMPARE(importer->object3DName(1), "Child");

    CORRADE_COMPARE(importer->object3DForName("Ghost"), -1);
}

void AssimpImporterTest::sceneCollapsedNode() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    /* This collapses all nodes into one. Neither OptimizeGraph nor
       OptimizeMeshes does that, but this one does it. Um. It also works only
       if the scene contains at least one mesh, if it doesn't then this
       postprocess step just silently quits, doing nothing. And they ultimately
       "fixed" it by updating the docs to say it doesn't always work:
       https://github.com/assimp/assimp/issues/3820

       It needs to be noted that originally this worked, however that was due
       to AI_CONFIG_IMPORT_NO_SKELETON_MESHES *not* being set, which caused
       COLLADA files to have some random mesh added, which then made this
       option work. Since we had to disable this option to avoid broken
       skins with out-of-bounds vertex references getting added, this file
       didn't have any mesh anymore and this feature stopped working. "Fixed"
       by adding an (otherwise unused/untested) mesh to the file. */
    importer->configuration().group("postprocess")->setValue("PreTransformVertices", true);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene+mesh.dae")));

    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 1); /* Just the root node */

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), {0});

    /* Assimp makes some bogus mesh for this one */
    Containers::Pointer<ObjectData3D> collapsedNode = importer->object3D(0);
    CORRADE_COMPARE(collapsedNode->children(), {});
    CORRADE_COMPARE(collapsedNode->instanceType(), ObjectInstanceType3D::Mesh);
    CORRADE_COMPARE(collapsedNode->transformation(), Matrix4{});

    /* Name of the scene is used for the root object */
    {
        const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
        /** @todo Possibly works with other versions (definitely not 3.0) */
        CORRADE_EXPECT_FAIL_IF(version <= 302,
            "Assimp 3.2 and below doesn't use name of the root node for collapsed nodes.");
        CORRADE_COMPARE(importer->object3DForName("Scene"), 0);
        CORRADE_COMPARE(importer->object3DName(0), "Scene");
    }
}

void AssimpImporterTest::upDirectionPatching() {
    auto&& data = UpDirectionPatchingData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Set only if not false to test correctness of the default as well */
    if(data.importColladaIgnoreUpDirection)
        importer->configuration().setValue("ImportColladaIgnoreUpDirection", true);
    importer->configuration().setValue("ImportColladaIgnoreUpDirection",
        data.importColladaIgnoreUpDirection);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, data.file)));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 2);

    /* First object is directly in the root, second object is a child of the
       first. */
    Matrix4 object0Transformation, object1Transformation;
    {
        Containers::Pointer<Trade::ObjectData3D> meshObject = importer->object3D(0);
        CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(meshObject->instance(), 0);
        CORRADE_COMPARE(meshObject->children(), std::vector<UnsignedInt>{1});
        object0Transformation = meshObject->transformation();
    } {
        Containers::Pointer<Trade::ObjectData3D> meshObject = importer->object3D(1);
        CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(meshObject->instance(), 0);
        CORRADE_COMPARE(meshObject->children(), std::vector<UnsignedInt>{});
        object1Transformation = meshObject->transformation();
    }

    /* The first mesh should have always the same final positions independently
       of how file's Y/Z-up or PreTransformVertices is set */
    {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);

        /* Transform the positions with object transform */
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
        MeshTools::transformPointsInPlace(object0Transformation,
            mesh->mutableAttribute<Vector3>(MeshAttribute::Position));

        CORRADE_EXPECT_FAIL_IF(data.expectFail, "Up direction is ignored.");
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
            Containers::arrayView<Vector3>({
                {-1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, 1.0f}
            }), TestSuite::Compare::Container);

    /* The second mesh is a child of the first, scaled 2x in addition. Verify
       the initial Z-up pretransformation is not applied redundantly to it. */
    } {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);

        /* Transform the positions with object transform and its parent as
           well */
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
        MeshTools::transformPointsInPlace(
            object0Transformation*object1Transformation,
            mesh->mutableAttribute<Vector3>(MeshAttribute::Position));

        CORRADE_EXPECT_FAIL_IF(data.expectFail, "Up direction is ignored.");
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
            Containers::arrayView<Vector3>({
                {-2.0f, 2.0f, -2.0f}, {-2.0f, 2.0f, 2.0f}
            }), TestSuite::Compare::Container);
    }
}

void AssimpImporterTest::upDirectionPatchingPreTransformVertices() {
    auto&& data = UpDirectionPatchingData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Set only if not false to test correctness of the default as well */
    if(data.importColladaIgnoreUpDirection)
        importer->configuration().setValue("ImportColladaIgnoreUpDirection", true);
    importer->configuration().group("postprocess")->setValue("PreTransformVertices", true);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, data.file)));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->object3DCount(), 1);

    /* There's only one object, directly in the root, with no transformation */
    {
        Containers::Pointer<Trade::ObjectData3D> meshObject = importer->object3D(0);
        CORRADE_COMPARE(meshObject->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(meshObject->instance(), 0);
        CORRADE_COMPARE(meshObject->children(), std::vector<UnsignedInt>{});
        CORRADE_COMPARE(meshObject->transformation(), Matrix4{});
    }

    /* There's just one mesh, with all vertices combined and already
       transformed. */
    {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);

        CORRADE_EXPECT_FAIL_IF(data.expectFail, "Up direction is ignored.");
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
            Containers::arrayView<Vector3>({
                {-1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, 1.0f},
                {-2.0f, 2.0f, -2.0f}, {-2.0f, 2.0f, 2.0f}
            }), TestSuite::Compare::Container);
    }
}

void AssimpImporterTest::imageEmbedded() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    if(version <= 302)
        CORRADE_SKIP("Assimp < 3.2 can't load embedded textures in blend files, Assimp 3.2 can't detect blend file format when opening a memory location.");

    /* Open as data, so we verify opening embedded images from data does not
       cause any problems even when no file callbacks are set */
    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "embedded-texture.blend"))));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::imageExternal() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae")));

    CORRADE_COMPARE(importer->image2DCount(), 2);
    Containers::Optional<ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::imageExternalNotFound() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly fails on more versions (definitely w/ 3.0 and 3.2) */
    if(version <= 302)
        CORRADE_SKIP("Assimp <= 3.2 would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "image-not-found.dae")));

    CORRADE_COMPARE(importer->image2DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    /* The (failed) importer should get cached even in case of failure, so
       the message should get printed just once */
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /not-found.png\n");
}

void AssimpImporterTest::imageExternalNoPathNoCallback() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae"))));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::image2D(): external images can be imported only when opening files from the filesystem or if a file callback is present\n");
}

void AssimpImporterTest::imagePathMtlSpaceAtTheEnd() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "image-filename-trailing-space.obj")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::imagePathBackslash() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "image-filename-backslash.obj")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::imageMipLevels() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");
    if(_manager.loadState("DdsImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("DdsImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "image-mips.obj")));
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
    CORRADE_COMPARE(image1->size(), Vector2i{1});
    CORRADE_COMPARE(image1->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image1->data(), Containers::arrayView<char>({
        '\xb3', '\x69', '\x00', '\xff'
    }), TestSuite::Compare::Container);
}

void AssimpImporterTest::texture() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae")));
    CORRADE_COMPARE(importer->textureCount(), 4);

    /* Diffuse texture */
    Containers::Optional<TextureData> texture = importer->texture(2);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->type(), TextureType::Texture2D);
    CORRADE_COMPARE(texture->wrapping(),
        Math::Vector3<SamplerWrapping>(SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge));
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->image(), 1);

    /* Specular texture */
    Containers::Optional<TextureData> texture1 = importer->texture(1);
    CORRADE_VERIFY(texture1);
    CORRADE_COMPARE(texture1->type(), TextureType::Texture2D);
    {
        /* I assume this "don't care for remaining stuff" part is responsible:
           https://github.com/assimp/assimp/blob/0c3933ca7c460644d346d94ecbb1b118f598ced4/code/Collada/ColladaParser.cpp#L1977-L1978 */
        CORRADE_EXPECT_FAIL("Assimp ignores sampler properties (in COLLADA files, at least).");
        CORRADE_COMPARE(texture1->wrapping(),
            Math::Vector3<SamplerWrapping>(SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping::Repeat));
        CORRADE_COMPARE(texture1->minificationFilter(), SamplerFilter::Nearest);
        CORRADE_COMPARE(texture1->magnificationFilter(), SamplerFilter::Nearest);
    } {
        /* It gives out the default always */
        CORRADE_COMPARE(texture->wrapping(),
            Math::Vector3<SamplerWrapping>(SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge));
        CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
    }
    CORRADE_COMPARE(texture1->image(), 0);

    /* Normal texture, reusing the diffuse image (so the same index) */
    Containers::Optional<TextureData> texture2 = importer->texture(3);
    CORRADE_VERIFY(texture2);
    CORRADE_COMPARE(texture2->type(), TextureType::Texture2D);
    CORRADE_COMPARE(texture2->image(), 1);

    CORRADE_COMPARE(importer->image2DCount(), 2);
    Containers::Optional<ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::openState() {
    Assimp::Importer _importer;
    /* Explicitly *not* setting AI_CONFIG_IMPORT_NO_SKELETON_MESHES here to
       verify that we survive all the shit it summons from within the bug
       swamp. */
    const aiScene* sc = _importer.ReadFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae"), aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_JoinIdenticalVertices);
    CORRADE_VERIFY(sc != nullptr);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->openState(sc);
    CORRADE_VERIFY(importer->isOpened());

    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->object3DCount(), 2);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->children3D(), {0});

    Containers::Pointer<ObjectData3D> parent = importer->object3D(0);
    CORRADE_COMPARE(parent->children(), {1});
    CORRADE_COMPARE(parent->instanceType(), ObjectInstanceType3D::Empty);
    CORRADE_COMPARE(parent->transformation(), Matrix4::scaling({1.0f, 2.0f, 3.0f}));

    Containers::Pointer<ObjectData3D> childObject = importer->object3D(1);
    CORRADE_COMPARE(childObject->transformation(), (Matrix4{
        {0.813798f, 0.469846f, -0.34202f, 0.0f},
        {-0.44097f, 0.882564f, 0.163176f, 0.0f},
        {0.378522f, 0.0180283f, 0.925417f, 0.0f},
        {1.0f, 2.0f, 3.0f, 1.0f}}));

    CORRADE_COMPARE(importer->object3DForName("Parent"), 0);
    CORRADE_COMPARE(importer->object3DForName("Child"), 1);
    CORRADE_COMPARE(importer->object3DName(0), "Parent");
    CORRADE_COMPARE(importer->object3DName(1), "Child");

    /* Verify that closing works as well, without double frees or null pointer
       accesses */
    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void AssimpImporterTest::openStateTexture() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Assimp::Importer _importer;
    const aiScene* sc = _importer.ReadFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae"), aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_JoinIdenticalVertices);
    CORRADE_VERIFY(sc != nullptr);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openState(sc, ASSIMPIMPORTER_TEST_DIR));
    CORRADE_COMPARE(importer->importerState(), sc);
    CORRADE_COMPARE(importer->textureCount(), 4);

    /* Diffuse texture */
    Containers::Optional<TextureData> texture = importer->texture(2);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->type(), TextureType::Texture2D);
    CORRADE_COMPARE(texture->wrapping(),
        Math::Vector3<SamplerWrapping>(SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge));
    CORRADE_COMPARE(texture->image(), 1);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);

    CORRADE_COMPARE(importer->image2DCount(), 2);
    Containers::Optional<ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::configurePostprocessFlipUVs() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().group("postprocess")->setValue("FlipUVs", true);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::TextureCoordinates), 1);

    /* The same as in mesh() but with reversed Y */
    CORRADE_COMPARE_AS(mesh->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        Containers::arrayView<Vector2>({
            {0.5f, 0.0f}, {0.75f, 0.5f}, {0.5f, 0.1f}
        }), TestSuite::Compare::Container);
}

void AssimpImporterTest::fileCallback() {
    /* This should verify also formats with external data (such as glTF),
       because Assimp is using the same callbacks for all data loading */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    std::unordered_map<std::string, Containers::Array<char>> files;
    files["not/a/path/mesh.dae"] = Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae"));
    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy policy,
        std::unordered_map<std::string, Containers::Array<char>>& files) {
            Debug{} << "Loading" << filename << "with" << policy;
            return Containers::optional(Containers::ArrayView<const char>(files.at(filename)));
        }, files);

    CORRADE_VERIFY(importer->openFile("not/a/path/mesh.dae"));
    CORRADE_COMPARE(importer->meshCount(), 1);

    /* Same as in mesh(), testing just the basics, no need to repeat everything
       here */
    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1, 2}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 6);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Normal), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Normal),
        Containers::arrayView<Vector3>({
            {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}
        }), TestSuite::Compare::Container);
}

void AssimpImporterTest::fileCallbackNotFound() {
    /* This should verify also formats with external data (such as glTF),
       because Assimp is using the same callbacks for all data loading */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy,
        void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{};
        });

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile("some-file.dae"));

    /* Assimp 5.0 changed the error string. aiGetVersion*() returns 401 for
       assimp 5, FFS, so we have to check differently. See CMakeLists.txt for
       details. */
    #if ASSIMP_IS_VERSION_5
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openFile(): failed to open some-file.dae: Failed to open file 'some-file.dae'.\n");
    #else
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openFile(): failed to open some-file.dae: Failed to open file some-file.dae.\n");
    #endif
}

void AssimpImporterTest::fileCallbackEmptyFile() {
    /* This verifies that we don't do anything silly (like division by zero) in
       IoStream::Read(). Works only with *.dae files, for *.obj Assimp bails
       out with `OBJ-file is too small.` without even calling Read(). */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    #ifdef CORRADE_TARGET_MSVC
    CORRADE_SKIP("This crashes (gets stuck on an assert popup?) on MSVC and clang-cl. Needs further investigation.");
    #endif

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy,
        void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{InPlaceInit};
        });

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile("some-file.dae"));
    /* INTERESTINGLY ENOUGH, a different message is printed when opening a DAE
       file directly w/o callbacks -- see emptyCollada() above. */
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openFile(): failed to open some-file.dae: File is too small\n");
}

void AssimpImporterTest::fileCallbackReset() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy,
        void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{};
        });

    /* Verify that nothing crashes/leaks here ... and also doesn't double free */
    importer->setFileCallback(nullptr);
    CORRADE_VERIFY(true);
}

void AssimpImporterTest::fileCallbackImage() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    std::unordered_map<std::string, Containers::Array<char>> files;
    files["not/a/path/texture.dae"] = Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae"));
    files["not/a/path/diffuse_texture.png"] = Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "diffuse_texture.png"));
    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy policy,
        std::unordered_map<std::string, Containers::Array<char>>& files) {
            Debug{} << "Loading" << filename << "with" << policy;
            return Containers::optional(Containers::ArrayView<const char>(files.at(filename)));
        }, files);

    CORRADE_VERIFY(importer->openFile("not/a/path/texture.dae"));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    /* Check only size, as it is good enough proof that it is working */
    Containers::Optional<ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(1, 1));
}

void AssimpImporterTest::fileCallbackImageNotFound() {
    const UnsignedInt version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(version < 302)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy,
        void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{};
        });

    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae"))));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(1));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file diffuse_texture.png\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::AssimpImporterTest)
