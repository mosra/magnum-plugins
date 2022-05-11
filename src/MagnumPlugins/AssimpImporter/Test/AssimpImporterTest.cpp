/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
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

#include <cctype>
#include <sstream>
#include <unordered_map>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Path.h>
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
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/SkinData.h>
#include <Magnum/Trade/TextureData.h>

#include <assimp/defs.h> /* in assimp 3.0, version.h is missing this include for ASSIMP_API */
#include <assimp/Importer.hpp>
#include <assimp/importerdesc.h>
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
    void cameraOrthographic();
    void light();
    void lightDirectionalBlender();
    void lightUnsupported();
    void cameraLightReferencedByTwoNodes();

    void materialColor();
    void materialTexture();
    void materialColorTexture();
    void materialStlWhiteAmbientPatch();
    void materialWhiteAmbientTexture();
    void materialMultipleTextures();
    void materialTextureCoordinateSets();
    void materialTextureLayers();
    void materialRawUnrecognized();
    void materialRaw();
    void materialRawTextureLayers();

    void mesh();
    void pointMesh();
    void lineMesh();
    void polygonMesh();
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
    void sceneName();
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

    void openTwice();
    void importTwice();

    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;

    unsigned int _assimpVersion;
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
              &AssimpImporterTest::cameraOrthographic,
              &AssimpImporterTest::light,
              &AssimpImporterTest::lightDirectionalBlender,
              &AssimpImporterTest::lightUnsupported,
              &AssimpImporterTest::cameraLightReferencedByTwoNodes,

              &AssimpImporterTest::materialColor,
              &AssimpImporterTest::materialTexture,
              &AssimpImporterTest::materialColorTexture,
              &AssimpImporterTest::materialStlWhiteAmbientPatch,
              &AssimpImporterTest::materialWhiteAmbientTexture,
              &AssimpImporterTest::materialMultipleTextures,
              &AssimpImporterTest::materialTextureCoordinateSets,
              &AssimpImporterTest::materialTextureLayers,
              &AssimpImporterTest::materialRawUnrecognized,
              &AssimpImporterTest::materialRaw,
              &AssimpImporterTest::materialRawTextureLayers,

              &AssimpImporterTest::mesh,
              &AssimpImporterTest::pointMesh,
              &AssimpImporterTest::lineMesh,
              &AssimpImporterTest::polygonMesh,
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
              &AssimpImporterTest::sceneName,
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
              &AssimpImporterTest::fileCallbackImageNotFound,

              &AssimpImporterTest::openTwice,
              &AssimpImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. */
    #ifdef ASSIMPIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(ASSIMPIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
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

    _assimpVersion = aiGetVersionMajor()*100 + aiGetVersionMinor()*10
        #if ASSIMP_HAS_VERSION_PATCH
        + aiGetVersionPatch()
        #endif
        ;

    /* Assimp 5.0.0 reports itself as 4.1.0 */
    #if ASSIMP_IS_VERSION_5_OR_GREATER
    _assimpVersion =  Math::max(_assimpVersion, 500u);
    #endif
}

using namespace Containers::Literals;

void AssimpImporterTest::openFile() {
    auto&& data = VerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->setFlags(data.flags);

    std::ostringstream out;
    {
        Debug redirectOutput{&out};

        CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae")));
        CORRADE_VERIFY(importer->importerState());
        CORRADE_COMPARE(importer->sceneCount(), 1);
        CORRADE_COMPARE(importer->objectCount(), 2);
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

    Containers::Optional<Containers::Array<char>> data = Utility::Path::read(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae"));
    CORRADE_VERIFY(importer->openData(*data));
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 2);
    CORRADE_COMPARE(importer->meshCount(), 0);
    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->skin3DCount(), 0);

    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void AssimpImporterTest::openDataFailed() {
    /* This fired an assert in <vector> because Assimp tried to load *anything*
       with X3DImporter and it didn't perform any checks:
       https://github.com/assimp/assimp/issues/4177 */
    if(_assimpVersion >= 510 && _assimpVersion < 513 && aiGetImporterDesc("x3d"))
        CORRADE_SKIP("Assimp 5.1.0 to 5.1.2 would assert on this test.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    std::ostringstream out;
    Error redirectError{&out};

    constexpr const char data[] = "what";
    CORRADE_VERIFY(!importer->openData({data, sizeof(data)}));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openData(): loading failed: No suitable reader found for the file format of file \"$$$___magic___$$$.\".\n");
}

/* This does not indicate general assimp animation support, only used to skip
   tests on certain versions and test files. */
bool supportsAnimation(const Containers::StringView fileName, unsigned int assimpVersion) {
    /* 5.0.0 supports all of Collada, FBX, glTF */
    if(assimpVersion >= 500) {
        return true;
    } else if(fileName.hasSuffix(".gltf"_s)) {
        return false;
    } else {
        CORRADE_INTERNAL_ASSERT(fileName.hasSuffix(".dae"_s) || fileName.hasSuffix(".fbx"_s));
        /* That's as far back as I checked, both Collada and FBX animations
           supported */
        return assimpVersion >= 320;
    }
}

void AssimpImporterTest::animation() {
    auto&& data = ExportedFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(!supportsAnimation(data.suffix, _assimpVersion))
        CORRADE_SKIP("Animation for this file type is not supported with the current version of Assimp");

    /* Animation created and exported with Blender. Most animation tracks got
       resampled and/or merged during export, so there's no use comparing
       against exact key/value arrays. Just apply all tracks and check if the
       transformation roughly matches at specific points in time.
       animationGltf() test covers that AssimpImporter correctly passes on what
       Assimp outputs. */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "exported-animation"_s + data.suffix)));

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

    const UnsignedInt objectCount = importer->objectCount();
    CORRADE_COMPARE(objectCount, Containers::arraySize(nodes));

    for(UnsignedInt i = 0; i < objectCount; i++) {
        const std::string name = importer->objectName(i);
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
        Containers::Optional<Trade::AnimationData> animation = importer->animation(i);
        CORRADE_VERIFY(animation);
        CORRADE_VERIFY(animation->importerState());

        for(UnsignedInt j = 0; j < animation->trackCount(); j++) {
            const Animation::TrackViewStorage<const Float>& track = animation->track(j);
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

    CORRADE_VERIFY(player.duration().contains({keys[0], keys[Containers::arraySize(keys) - 1]}));
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
        CORRADE_ITERATION(i);

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

        /* FBX files reported incorrect mTicksPerSecond in versions 5.1.0 to
           5.1.3, but the fix in 5.1.4 broke the detection and workaround we
           had so we no longer patch anything for these versions.
           https://github.com/assimp/assimp/issues/4197 */
        const bool fbxBroken = i == 1 && data.name == "FBX"_s && ASSIMP_VERSION >= 20210102 && _assimpVersion < 514;

        CORRADE_EXPECT_FAIL_IF(fbxBroken,
            "FBX animations are broken in Assimp 5.1.0 to 5.1.3.");

        CORRADE_COMPARE_WITH(rotation, rotationData[i],
            TestSuite::Compare::around(Epsilon));
        CORRADE_COMPARE_WITH(scaling, scalingData[i],
            TestSuite::Compare::around(Epsilon));
        CORRADE_COMPARE_WITH(translation, translationData[i],
            TestSuite::Compare::around(Epsilon));

        /* Don't spam the output, all following keys are broken */
        if(fbxBroken) return;
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
    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    /* Using the same files as CgltfImporterTest, but modified to include a
       scene, because Assimp refuses to import animations if there is no scene. */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "animation.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 3);
    CORRADE_COMPARE(importer->animationName(0), "empty");
    CORRADE_COMPARE(importer->animationForName("empty"), 0);
    CORRADE_COMPARE(importer->animationForName("nonexistent"), -1);

    /* Empty animation */
    {
        Containers::Optional<Trade::AnimationData> animation = importer->animation("empty");
        CORRADE_VERIFY(animation);
        CORRADE_VERIFY(animation->data().isEmpty());
        CORRADE_COMPARE(animation->trackCount(), 0);

    /* Translation/rotation/scaling animation */
    } {
        std::ostringstream out;
        Debug redirectDebug{&out};

        Containers::Optional<Trade::AnimationData> animation = importer->animation("TRS animation");
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
    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");
    if(_assimpVersion >= 510)
        CORRADE_SKIP("Current version of assimp wouldn't load this file.");

    /* This reuses the CgltfImporter test files, not the corrected ones used by other tests. */
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "animation-no-scene.gltf")));

    CORRADE_EXPECT_FAIL("Assimp refuses to import glTF animations if the file has no scenes.");
    CORRADE_COMPARE(importer->animationCount(), 3);
}

void AssimpImporterTest::animationGltfBrokenSplineWarning() {
    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    #if !ASSIMP_HAS_BROKEN_GLTF_SPLINES
    CORRADE_SKIP("Current version of assimp correctly imports glTF spline-interpolated animations.");
    #else
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "animation.gltf")));
    }
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openData(): spline-interpolated animations imported "
        "from this file are most likely broken using this version of Assimp. Consult the "
        "importer documentation for more information.\n");
    #endif
}

void AssimpImporterTest::animationGltfSpline() {
    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "animation.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 3);
    CORRADE_COMPARE(importer->animationName(2), "TRS animation, splines");

    constexpr Float keys[]{ 0.5f, 3.5f, 4.0f, 5.0f };

    Containers::Optional<Trade::AnimationData> animation = importer->animation(2);
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
            {{ 0.780076f,  0.0260025f, 0.598059f},  0.182018f},
            {{ 0.711568f, -0.391362f, -0.355784f}, -0.462519f},
            {{-0.598059f, -0.182018f, -0.026003f}, -0.780076f},
            {{-0.711568f,  0.355784f,  0.462519f},  0.391362f}
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

    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    if(_assimpVersion > 500)
        CORRADE_SKIP("Current version of assimp correctly sets glTF ticks per second.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->setFlags(data.flags);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "animation.gltf")));

    std::ostringstream out;
    {
        Debug redirectDebug{&out};
        CORRADE_VERIFY(importer->animation(1));
    }

    if(data.flags >= ImporterFlag::Verbose) {
        CORRADE_COMPARE_AS(out.str(),
            " ticks per second is incorrect for glTF, patching to 1000\n",
            TestSuite::Compare::StringHasSuffix);
    } else
        CORRADE_COMPARE(out.str(), "");
}

void AssimpImporterTest::animationDummyTracksRemovalEnabled() {
    auto&& data = VerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    /* Correct removal is already implicitly tested in animationGltf(),
       only check for track count/size and the message here */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->setFlags(data.flags);
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("removeDummyAnimationTracks"));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "animation.gltf")));

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
        CORRADE_COMPARE_AS(out.str(), Utility::format(
            "Trade::AssimpImporter::animation(): ignoring dummy translation track in animation 1, channel {}\n"
            "Trade::AssimpImporter::animation(): ignoring dummy scaling track in animation 1, channel {}\n",
            targets[0].channel, targets[0].channel),
            TestSuite::Compare::StringContains);
        CORRADE_COMPARE_AS(out.str(), Utility::format(
            "Trade::AssimpImporter::animation(): ignoring dummy rotation track in animation 1, channel {}\n"
            "Trade::AssimpImporter::animation(): ignoring dummy scaling track in animation 1, channel {}\n",
            targets[1].channel, targets[1].channel),
            TestSuite::Compare::StringContains);
        CORRADE_COMPARE_AS(out.str(), Utility::format(
            "Trade::AssimpImporter::animation(): ignoring dummy translation track in animation 1, channel {}\n"
            "Trade::AssimpImporter::animation(): ignoring dummy rotation track in animation 1, channel {}\n",
            targets[2].channel, targets[2].channel),
            TestSuite::Compare::StringContains);
    } else
        CORRADE_COMPARE(out.str(), "");
}

void AssimpImporterTest::animationDummyTracksRemovalDisabled() {
    auto&& data = VerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->setFlags(data.flags);
    /* Explicitly disable */
    importer->configuration().setValue("removeDummyAnimationTracks", false);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "animation.gltf")));

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

    CORRADE_COMPARE_AS(out.str(),
        "Trade::AssimpImporter::animation(): ignoring dummy translation track in animation",
        TestSuite::Compare::StringNotContains);
    CORRADE_COMPARE_AS(out.str(),
        "Trade::AssimpImporter::animation(): ignoring dummy rotation track in animation",
        TestSuite::Compare::StringNotContains);
    CORRADE_COMPARE_AS(out.str(),
        "Trade::AssimpImporter::animation(): ignoring dummy scaling track in animation",
        TestSuite::Compare::StringNotContains);
}

void AssimpImporterTest::animationShortestPathOptimizationEnabled() {
    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("optimizeQuaternionShortestPath"));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "animation-patching.gltf")));

    Containers::Optional<Trade::AnimationData> animation = importer->animation("Quaternion shortest-path patching");
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
    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Explicitly disable */
    importer->configuration().setValue("optimizeQuaternionShortestPath", false);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "animation-patching.gltf")));

    Containers::Optional<Trade::AnimationData> animation = importer->animation("Quaternion shortest-path patching");
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
    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("normalizeQuaternions"));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "animation-patching.gltf")));

    Containers::Optional<AnimationData> animation;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        animation = importer->animation("Quaternion normalization patching");
    }
    CORRADE_VERIFY(animation);
    CORRADE_COMPARE_AS(out.str(),
        "Trade::AssimpImporter::animation(): quaternions in some rotation tracks were renormalized\n",
        TestSuite::Compare::StringContains);
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
    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Explicitly disable */
    CORRADE_VERIFY(importer->configuration().setValue("normalizeQuaternions", false));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "animation-patching.gltf")));

    Containers::Optional<Trade::AnimationData> animation = importer->animation("Quaternion normalization patching");
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
    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Enable animation merging */
    importer->configuration().setValue("mergeAnimationClips", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "empty.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->animationForName(""), -1);
}

void AssimpImporterTest::animationMerge() {
    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 animation is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Enable animation merging, disabled by default */
    CORRADE_VERIFY(!importer->configuration().value<bool>("mergeAnimationClips"));
    importer->configuration().setValue("mergeAnimationClips", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "animation.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 1);
    CORRADE_COMPARE(importer->animationName(0), "");
    CORRADE_COMPARE(importer->animationForName(""), -1);

    Containers::Optional<Trade::AnimationData> animation = importer->animation(0);
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

/* The checks are identical to animation support so re-use that */
bool supportsSkinning(const Containers::StringView fileName, unsigned int assimpVersion) {
    return supportsAnimation(fileName, assimpVersion);
}

/* Since 5.1.0 the Collada importer uses the mesh ID as the name. Imitate the
   Blender(?) exporter that generated the IDs from the name. A bit hacky but
   still better than special-casing every test that loads meshes by name. */
std::string fixMeshName(const Containers::StringView meshName, const Containers::StringView fileName, unsigned int assimpVersion) {
    std::string fixed = meshName;
    if(assimpVersion >= 510 && fileName.hasSuffix(".dae"_s)) {
        for(char& c: fixed)
            if(c != '-' && !std::isalnum(static_cast<unsigned char>(c))) c = '_';
        fixed += "-mesh";
    }

    return fixed;
}

void AssimpImporterTest::skin() {
    auto&& data = ExportedFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(!supportsSkinning(data.suffix, _assimpVersion))
        CORRADE_SKIP("Skin data for this file type is not supported with the current version of Assimp");

    /* Skinned mesh imported into Blender and then exported */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "skin"_s + data.suffix)));

    /* Two skins with their own meshes, one unskinned mesh */
    CORRADE_COMPARE(importer->meshCount(), 3);
    CORRADE_COMPARE(importer->skin3DCount(), 2);
    CORRADE_COMPARE(importer->skin3DForName("nonexistent"), -1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_VERIFY(scene->hasField(SceneField::Skin));
    CORRADE_COMPARE(scene->fieldFlags(SceneField::Skin), SceneFieldFlag::OrderedMapping);

    /* Get global node transforms, needed for testing inverse bind matrices */
    Containers::Array<Matrix4> globalTransforms{importer->objectCount() + 1};
    Containers::Array<Containers::Pair<UnsignedInt, Int>> parents = scene->parentsAsArray();
    Containers::Array<Containers::Pair<UnsignedInt, Matrix4>> transforms = scene->transformations3DAsArray();
    for(std::size_t i = 0; i != globalTransforms.size() - 1; ++i) {
        /* Abuse the fact that a parent is always defined before its children */
        CORRADE_ITERATION(i);
        CORRADE_VERIFY(parents[i].second() < Int(i));

        globalTransforms[i + 1] = globalTransforms[parents[i].second() + 1]*transforms[i].second();
    }

    /* Some Blender exporters don't transform skin matrices correctly. Also see
       the comment for ExportedFileData. */
    const Matrix4 correction{data.correction.toMatrix()};

    constexpr Containers::StringView objectNames[]{
        "Mesh_1"_s, "Mesh_2"_s, "Plane"_s
    };
    const std::string jointNames[][2]{
        {"Node_1", "Node_2"},
        {"Node_3", "Node_4"}
    };

    for(UnsignedInt i = 0; i != Containers::arraySize(objectNames) - 1; ++i) {
        /* Skin names are taken from mesh names, skin order is arbitrary */
        const std::string meshName = fixMeshName(objectNames[i], data.suffix, _assimpVersion);
        const Int skinIndex = importer->skin3DForName(meshName);
        CORRADE_VERIFY(skinIndex != -1);
        CORRADE_COMPARE(importer->skin3DName(skinIndex), meshName);
        CORRADE_VERIFY(importer->meshForName(meshName) != -1);

        Containers::Optional<Trade::SkinData3D> skin = importer->skin3D(skinIndex);
        CORRADE_VERIFY(skin);
        CORRADE_VERIFY(skin->importerState());

        /* Don't check joint order, only presence */
        Containers::ArrayView<const UnsignedInt> joints = skin->joints();
        CORRADE_COMPARE(joints.size(), Containers::arraySize(jointNames[i]));
        for(const std::string& name: jointNames[i]) {
            auto found = std::find_if(joints.begin(), joints.end(), [&](UnsignedInt joint) {
                    /* Blender's Collada exporter adds an Armature_ prefix to
                       object names */
                    return Utility::String::endsWith(importer->objectName(joint), name);
                });
            CORRADE_VERIFY(found != joints.end());
        }

        /* The exporters transform the inverse bind matrices quite a bit,
           making them hard to compare. Instead, check that their defining
           property holds: they're the inverse of the joint's original global
           transform. */
        Containers::ArrayView<const Matrix4> bindMatrices = skin->inverseBindMatrices();
        CORRADE_COMPARE(bindMatrices.size(), joints.size());
        Long meshObject = importer->objectForName(objectNames[i]);
        CORRADE_VERIFY(meshObject != -1);
        CORRADE_VERIFY(scene->meshesMaterialsFor(meshObject));
        CORRADE_COMPARE_AS(scene->skinsFor(meshObject),
            Containers::arrayView({UnsignedInt(skinIndex)}),
            TestSuite::Compare::Container);
        Containers::Optional<Matrix4> meshTransform = scene->transformation3DFor(meshObject);
        CORRADE_VERIFY(meshTransform);
        for(UnsignedInt j = 0; j != joints.size(); ++j) {
            const Matrix4 invertedTransform = globalTransforms[joints[j] + 1].inverted() * *meshTransform * correction;
            CORRADE_COMPARE(bindMatrices[j], invertedTransform);
        }
    }

    {
        /* Unskinned meshes and mesh nodes shouldn't have a skin */
        const std::string meshName = fixMeshName(objectNames[2], data.suffix, _assimpVersion);
        CORRADE_VERIFY(importer->meshForName(meshName) != -1);
        CORRADE_COMPARE(importer->skin3DForName(meshName), -1);
        Long meshObject = importer->objectForName(objectNames[2]);
        CORRADE_VERIFY(meshObject != -1);
        CORRADE_VERIFY(scene->meshesMaterialsFor(meshObject));
        CORRADE_COMPARE_AS(scene->skinsFor(meshObject),
            Containers::arrayView<UnsignedInt>({}),
            TestSuite::Compare::Container);
    }
}

void AssimpImporterTest::skinNoMeshes() {
    if(!supportsSkinning(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 skinning is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "skin-no-mesh.gltf")));

    /* Assimp only lets us access joints for each mesh. No mesh = no joints. */
    CORRADE_COMPARE(importer->meshCount(), 0);
    CORRADE_COMPARE(importer->skin3DCount(), 0);
}

void AssimpImporterTest::skinMergeEmpty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Enable skin merging */
    importer->configuration().setValue("mergeSkins", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer->skin3DCount(), 0);
    CORRADE_COMPARE(importer->skin3DForName(""), -1);

    /* No skin references in the scene */
    CORRADE_COMPARE(importer->sceneCount(), 1);
    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_VERIFY(!scene->hasField(SceneField::Skin));
}

void AssimpImporterTest::skinMerge() {
    if(!supportsAnimation(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("GLTF skinning is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Disabled by default */
    CORRADE_VERIFY(!importer->configuration().value<bool>("mergeSkins"));
    /* Enable skin merging */
    importer->configuration().setValue("mergeSkins", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "skin-shared.gltf")));

    CORRADE_COMPARE(importer->skin3DCount(), 1);
    CORRADE_COMPARE(importer->skin3DName(0), "");
    CORRADE_COMPARE(importer->skin3DForName(""), -1);

    CORRADE_COMPARE(importer->sceneCount(), 1);
    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    for(const char* meshName: {"Mesh_1", "Mesh_2"}) {
        CORRADE_ITERATION(meshName);

        Long meshObject = importer->objectForName(meshName);
        CORRADE_VERIFY(meshObject != -1);
        CORRADE_VERIFY(scene->meshesMaterialsFor(meshObject));
        CORRADE_COMPARE_AS(scene->skinsFor(meshObject),
            Containers::arrayView({UnsignedInt(0)}),
            TestSuite::Compare::Container);
    }

    Containers::Optional<Trade::SkinData3D> skin = importer->skin3D(0);
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

    Containers::ArrayView<const UnsignedInt> joints = skin->joints();
    Containers::ArrayView<const Matrix4> inverseBindMatrices = skin->inverseBindMatrices();
    CORRADE_COMPARE(joints.size(), inverseBindMatrices.size());
    CORRADE_COMPARE(joints.size(), Containers::arraySize(mergeOrder));

    for(UnsignedInt i = 0; i != joints.size(); ++i) {
        CORRADE_ITERATION(i);
        CORRADE_COMPARE(joints[i], expectedObjects[mergeOrder[i]]);
        CORRADE_VERIFY(!scene->meshesMaterialsFor(joints[i]));
        CORRADE_COMPARE(inverseBindMatrices[i], expectedBones[mergeOrder[i]]);
    }
}

void AssimpImporterTest::camera() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "camera.dae")));

    /* The first camera is a dummy one to test non-trivial camera assignment in
       the scene */
    CORRADE_COMPARE(importer->cameraCount(), 3);
    CORRADE_COMPARE(importer->cameraName(2), "Camera-no-aspect");
    CORRADE_COMPARE(importer->cameraForName("Camera-no-aspect"), 2);
    CORRADE_COMPARE(importer->cameraForName("nonexistent"), -1);

    Containers::Optional<CameraData> camera1 = importer->camera(1);
    CORRADE_VERIFY(camera1);
    CORRADE_COMPARE(camera1->type(), CameraType::Perspective3D);
    CORRADE_COMPARE(camera1->fov(), 49.13434_degf);
    CORRADE_COMPARE(camera1->aspectRatio(), 1.77777f);
    CORRADE_COMPARE(camera1->near(), 0.123f);
    CORRADE_COMPARE(camera1->far(), 123.0f);

    Containers::Optional<CameraData> camera2 = importer->camera(2);
    CORRADE_VERIFY(camera2);
    CORRADE_COMPARE(camera2->type(), CameraType::Perspective3D);
    CORRADE_COMPARE(camera2->fov(), 12.347_degf);
    /* No aspect ratio, defaults to 1 */
    CORRADE_COMPARE(camera2->aspectRatio(), 1.0f);
    CORRADE_COMPARE(camera2->near(), 0.1f);
    CORRADE_COMPARE(camera2->far(), 2.0f);

    /* Check camera assignment in the scene. The first two objects are dummy
       nodes to test non-trivial assignment; however unfortunately the cameras
       are parsed by Assimp ONLY if referenced from a node so one of the dummy
       nodes has to reference the other dummy camera. SIGH. */
    CORRADE_COMPARE(importer->sceneCount(), 1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->fieldCount(), 4);

    /* Fields we're not interested in */
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
    CORRADE_VERIFY(scene->hasField(SceneField::Transformation));

    CORRADE_VERIFY(scene->hasField(SceneField::Camera));
    CORRADE_COMPARE(scene->fieldFlags(SceneField::Camera), SceneFieldFlag::OrderedMapping);
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Camera), Containers::arrayView<UnsignedInt>({
        1, 2, 3
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Camera), Containers::arrayView<UnsignedInt>({
        0, 1, 2
    }), TestSuite::Compare::Container);
}

void AssimpImporterTest::cameraOrthographic() {
    if(_assimpVersion < 410)
        CORRADE_SKIP("glTF 2 is supported since Assimp 4.1.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Assimp 5.1.0 refuses to load glTF files without a default scene so we
       can't reuse CgltfImporter's test file */
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "camera-orthographic.gltf")));
    CORRADE_COMPARE(importer->cameraCount(), 1);

    Containers::Optional<CameraData> camera = importer->camera(0);
    CORRADE_VERIFY(camera);
    {
        #if !ASSIMP_HAS_ORTHOGRAPHIC_CAMERA
        CORRADE_EXPECT_FAIL("Current version of assimp imports orthograpic cameras as perspective cameras");
        #endif
        CORRADE_COMPARE(camera->type(), CameraType::Orthographic3D);
        CORRADE_COMPARE(camera->size(), (Vector2{4.0f, 3.0f}));
    }

    #if !ASSIMP_HAS_ORTHOGRAPHIC_CAMERA
    CORRADE_COMPARE(camera->type(), CameraType::Perspective3D);
    /* FOV and aspect ratio arbitrarily differ between importers. No use
       testing specific values as they'll break with the next version anyway.  */
    #endif

    CORRADE_COMPARE(camera->near(), 0.01f);
    CORRADE_COMPARE(camera->far(), 100.0f);
}

void AssimpImporterTest::light() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "light.dae")));

    CORRADE_COMPARE(importer->lightCount(), 4);
    CORRADE_COMPARE(importer->lightName(1), "Spot");
    CORRADE_COMPARE(importer->lightForName("Spot"), 1);
    CORRADE_COMPARE(importer->lightForName("nonexistent"), -1);

    /* Collada light import is broken in Assimp 5.1.0:
       https://github.com/assimp/assimp/issues/4179
       This was fixed a few days later in 5.1.1 but (as is customary with that
       library) they didn't update the version so both report as 5.1.0.
       To keep our sanity, we assume 5.1.0 means 5.1.1 and ignore this was
       ever broken. */

    /* The order of lights depends on the order in which they're referenced
       from nodes, not the order in which they're defined. FFS. */

    /* Spot light */
    {
        Containers::Optional<Trade::LightData> light = importer->light("Spot");
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Spot);
        CORRADE_COMPARE(light->color(), (Color3{0.12f, 0.24f, 0.36f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{0.1f, 0.3f, 0.5f}));
        CORRADE_COMPARE(light->innerConeAngle(), 45.0_degf);
        /* Not sure how it got calculated from 0.15 falloff exponent, but
           let's just trust Assimp for once */
        CORRADE_COMPARE(light->outerConeAngle(), 135.0_degf);

    /* Point light */
    } {
        Containers::Optional<Trade::LightData> light = importer->light("Point");
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{0.5f, 0.25f, 0.05f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{0.1f, 0.7f, 0.9f}));

    /* Directional light */
    } {
        Containers::Optional<Trade::LightData> light = importer->light("Sun");
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Directional);
        /* This one has intensity of 10, which gets premultiplied to the
           color */
        CORRADE_COMPARE(light->color(), (Color3{1.0f, 0.15f, 0.45f})*10.0f);
        CORRADE_COMPARE(light->intensity(), 1.0f);
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f, 0.0f, 0.0f}));

    /* Ambient light */
    } {
        Containers::Optional<Trade::LightData> light = importer->light("Ambient");
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Ambient);
        CORRADE_COMPARE(light->color(), (Color3{0.01f, 0.02f, 0.05f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f, 0.0f, 0.0f}));
    }

    /* Check light assignment in the scene. The first object is a dummy node
       to test non-trivial assignment. */
    CORRADE_COMPARE(importer->sceneCount(), 1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->fieldCount(), 4);

    /* Fields we're not interested in */
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
    CORRADE_VERIFY(scene->hasField(SceneField::Transformation));

    CORRADE_VERIFY(scene->hasField(SceneField::Light));
    CORRADE_COMPARE(scene->fieldFlags(SceneField::Light), SceneFieldFlag::OrderedMapping);
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Light), Containers::arrayView<UnsignedInt>({
        1, 2, 3, 4
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Light), Containers::arrayView<UnsignedInt>({
        0, 1, 2, 3
    }), TestSuite::Compare::Container);
}

void AssimpImporterTest::lightDirectionalBlender() {
    /* Versions before 5.1.3 say "BLEND: Expected at least one object with no
       parent". */
    if(_assimpVersion < 513)
        CORRADE_SKIP("Blender 2.8+ files are supported only since Assimp 5.1.3.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "light-directional.blend")));

    /* While COLLADA files (the light() test above) have the attenuation as
       expected, Assimp's Blender importer encodes max distance into it, which
       has to be patched away */
    std::ostringstream out;
    Warning redirectWarning{&out};
    Containers::Optional<LightData> light = importer->light("Sun");
    CORRADE_VERIFY(light);
    CORRADE_COMPARE(light->type(), LightData::Type::Directional);
    CORRADE_COMPARE(light->color(), (Color3{0.3f, 0.4f, 0.5f}));
    CORRADE_COMPARE(light->intensity(), 1.0f);
    CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f, 0.0f, 0.0f}));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::light(): patching attenuation Vector(1, 0.16, 0.0064) to Vector(1, 0, 0) for Trade::LightData::Type::Directional\n");
}

void AssimpImporterTest::lightUnsupported() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    /* The light-area.blend file contains an area light, but Assimp can't open
       Blender 2.8 files yet it seems. So I saved it from Blender as FBX and
       opening that, but somehow the light lost its area type in process and
       it's now UNKNOWN instead. Which is fine I guess as I want to test just
       the failure anyway. */
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "light-area.fbx")));
    CORRADE_COMPARE(importer->lightCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->light(0));
    CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::light(): light type 0 is not supported\n");
}

void AssimpImporterTest::cameraLightReferencedByTwoNodes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    /* Single camera / light referenced by two nodes. According to Assimp docs,
       this should result in the two nodes having the same name in order to
       reference the same camera. But in reality, the camera / light just gets
       duplicated (at least in case of COLLADA) so I have no way to test the
       behavior mentioned in the docs. */
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "camera-light-referenced-by-two-nodes.dae")));

    {
        CORRADE_EXPECT_FAIL("Assimp duplicates cameras / lights referenced by multiple nodes.");
        CORRADE_COMPARE(importer->cameraCount(), 1);
        CORRADE_COMPARE(importer->lightCount(), 1);
    } {
        CORRADE_COMPARE(importer->cameraCount(), 2);
        CORRADE_COMPARE(importer->lightCount(), 2);
    }

    /* The two duplicates should be exactly the same */
    for(UnsignedInt id: {0, 1}) {
        CORRADE_ITERATION(id);

        Containers::Optional<LightData> light = importer->light(id);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Directional);
        /* This one has intensity of 10, which gets premultiplied to the
           color */
        CORRADE_COMPARE(light->color(), (Color3{1.0f, 0.15f, 0.45f})*10.0f);
        CORRADE_COMPARE(light->intensity(), 1.0f);

        Containers::Optional<CameraData> camera = importer->camera(id);
        CORRADE_VERIFY(camera);
        CORRADE_COMPARE(camera->fov(), 49.13434_degf);
        CORRADE_COMPARE(camera->near(), 0.123f);
        CORRADE_COMPARE(camera->far(), 123.0f);
    }

    CORRADE_COMPARE(importer->sceneCount(), 1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->fieldCount(), 5);

    /* Fields we're not interested in */
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
    CORRADE_VERIFY(scene->hasField(SceneField::Transformation));

    CORRADE_VERIFY(scene->hasField(SceneField::Camera));
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Camera), Containers::arrayView<UnsignedInt>({
        0, 2
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Camera), Containers::arrayView<UnsignedInt>({
        0, 1
    }), TestSuite::Compare::Container);

    CORRADE_VERIFY(scene->hasField(SceneField::Light));
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Light), Containers::arrayView<UnsignedInt>({
        1, 3
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Light), Containers::arrayView<UnsignedInt>({
        0, 1
    }), TestSuite::Compare::Container);
}

void AssimpImporterTest::materialColor() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Unrecognized materials are tested separately in materialRaw() */
    importer->configuration().setValue("ignoreUnrecognizedMaterialData", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-color.dae")));

    /* The first material is a dummy one to test non-trivial material
       assignment in the scene */
    CORRADE_COMPARE(importer->materialCount(), 2);

    Containers::Optional<MaterialData> material = importer->material(1);
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

    /* Ancient assimp version add "-material" suffix */
    if(_assimpVersion < 302) {
        CORRADE_COMPARE(importer->materialName(1), "Material-material");
        CORRADE_COMPARE(importer->materialForName("Material-material"), 1);
    } else {
        CORRADE_COMPARE(importer->materialName(1), "Material");
        CORRADE_COMPARE(importer->materialForName("Material"), 1);
    }
    CORRADE_COMPARE(importer->materialForName("Nonexistent"), -1);

    /* Check material assignment in the scene. The first two objects are dummy
       nodes to test non-trivial assignment. */
    CORRADE_COMPARE(importer->sceneCount(), 1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->fieldCount(), 5);

    /* Fields we're not interested in */
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
    CORRADE_VERIFY(scene->hasField(SceneField::Transformation));

    CORRADE_VERIFY(scene->hasField(SceneField::Mesh));
    CORRADE_VERIFY(scene->hasField(SceneField::MeshMaterial));
    CORRADE_COMPARE(scene->fieldFlags(SceneField::Mesh), SceneFieldFlag::OrderedMapping);
    CORRADE_COMPARE(scene->fieldFlags(SceneField::MeshMaterial), SceneFieldFlag::OrderedMapping);
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        2
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::MeshMaterial), Containers::arrayView<Int>({
        1
    }), TestSuite::Compare::Container);
}

void AssimpImporterTest::materialTexture() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().setValue("ignoreUnrecognizedMaterialData", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae")));

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
    importer->configuration().setValue("ignoreUnrecognizedMaterialData", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-color-texture.obj")));

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
    if(_assimpVersion < 400)
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "quad.stl")));

    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<MaterialData> material;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        material = importer->material(0);
    }

    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType::Phong);
    {
        CORRADE_EXPECT_FAIL_IF(_assimpVersion < 410 || _assimpVersion >= 500,
            "Assimp < 4.1 and >= 5.0 behaves properly regarding STL material ambient");
        CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::material(): white ambient detected, forcing back to black\n");
    }

    const auto& phong = material->as<PhongMaterialData>();
    CORRADE_VERIFY(!phong.hasAttribute(MaterialAttribute::AmbientTexture));
    /* WHY SO COMPLICATED, COME ON */
    if(_assimpVersion < 410 || _assimpVersion >= 500)
        CORRADE_COMPARE(phong.ambientColor(), Color3{0.05f});
    else
        CORRADE_COMPARE(phong.ambientColor(), 0x000000_srgbf);

    /* ASS IMP WHAT?! WHY 3.2 is different from 3.0 and 4.0?! */
    if(_assimpVersion == 320) {
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "texture-ambient.obj")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "multiple-textures.obj")));

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

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-coordinate-sets.dae")));

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

void AssimpImporterTest::materialTextureLayers() {
    if(_assimpVersion < 500)
        CORRADE_SKIP("This version of assimp doesn't imported layered FBX materials.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().setValue("ignoreUnrecognizedMaterialData", true);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-layers.fbx")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<Trade::MaterialData> material = importer->material(0);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->layerCount(), 3);

    /* Layer 0. Material attributes + diffuse texture + ambient texture. */
    {
        CORRADE_VERIFY(material->hasAttribute(0, MaterialAttribute::AmbientColor));
        CORRADE_COMPARE(material->attribute<Vector4>(0, MaterialAttribute::AmbientColor),
            (Color4{0.1f, 0.2f, 0.3f, 1.0f}));

        CORRADE_VERIFY(material->hasAttribute(0, MaterialAttribute::DiffuseColor));
        CORRADE_COMPARE(material->attribute<Vector4>(0, MaterialAttribute::DiffuseColor),
            (Color4{0.7f, 0.6f, 0.5f, 1.0f}));

        CORRADE_VERIFY(material->hasAttribute(0, MaterialAttribute::AmbientTexture));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(0, MaterialAttribute::AmbientTexture), 0);

        CORRADE_VERIFY(material->hasAttribute(0, MaterialAttribute::AmbientTextureCoordinates));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(0, MaterialAttribute::AmbientTextureCoordinates), 0);

        CORRADE_VERIFY(material->hasAttribute(0, MaterialAttribute::DiffuseTexture));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(0, MaterialAttribute::DiffuseTexture), 1);

        CORRADE_VERIFY(material->hasAttribute(0, MaterialAttribute::DiffuseTextureCoordinates));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(0, MaterialAttribute::DiffuseTextureCoordinates), 0);

    /* Layer 1. Diffuse texture. Can't have any material attributes. */
    } {
        CORRADE_VERIFY(!material->hasAttribute(1, MaterialAttribute::AmbientColor));
        CORRADE_VERIFY(!material->hasAttribute(1, MaterialAttribute::DiffuseColor));

        CORRADE_VERIFY(material->hasAttribute(0, MaterialAttribute::DiffuseTexture));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(1, MaterialAttribute::DiffuseTexture), 2);

        CORRADE_VERIFY(material->hasAttribute(1, MaterialAttribute::DiffuseTextureCoordinates));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(1, MaterialAttribute::DiffuseTextureCoordinates), 0);

    /* Layer 2. Diffuse texture. Can't have any material attributes. */
    } {
        CORRADE_VERIFY(!material->hasAttribute(2, MaterialAttribute::AmbientColor));
        CORRADE_VERIFY(!material->hasAttribute(2, MaterialAttribute::DiffuseColor));

        CORRADE_VERIFY(material->hasAttribute(2, MaterialAttribute::DiffuseTexture));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(2, MaterialAttribute::DiffuseTexture), 3);

        CORRADE_VERIFY(material->hasAttribute(2, MaterialAttribute::DiffuseTextureCoordinates));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(2, MaterialAttribute::DiffuseTextureCoordinates), 0);
    }
}

Containers::StringView extractMaterialKey(const char* data, int, int) {
    return data;
}

void AssimpImporterTest::materialRawUnrecognized() {
    if(_assimpVersion < 500)
            CORRADE_SKIP("This version of Assimp doesn't import AI_MATKEY_COLOR_TRANSPARENT from FBX files.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Disabled by default */
    CORRADE_VERIFY(!importer->configuration().value<bool>("ignoreUnrecognizedMaterialData"));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-raw.fbx")));

    CORRADE_COMPARE(importer->materialCount(), 2);

    Containers::Optional<MaterialData> material = importer->material("Standard_Types");
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType::Phong);
    CORRADE_COMPARE(material->layerCount(), 1);
    CORRADE_COMPARE(material->attributeCount(), 4);

    /* Recognized attributes */
    {
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::AmbientColor));
        CORRADE_COMPARE(material->attributeId(MaterialAttribute::AmbientColor), 2);
        CORRADE_COMPARE(material->attribute<Vector4>(2), (Color4{0.1f, 0.05f, 0.1f, 1.0f}));
    } {
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::DiffuseColor));
        CORRADE_COMPARE(material->attributeId(MaterialAttribute::DiffuseColor), 3);
        CORRADE_COMPARE(material->attribute<Vector4>(3), (Color4{0.4f, 0.2f, 0.1f, 1.0f}));

    /* Unrecognized attributes */
    } {
        CORRADE_COMPARE(material->attributeName(0), extractMaterialKey(AI_MATKEY_COLOR_TRANSPARENT));
        CORRADE_COMPARE(material->attributeType(0), MaterialAttributeType::Vector3);
        CORRADE_COMPARE(material->attribute<Vector3>(0), (Vector3{0.3f, 0.2f, 0.1f}));
    } {
        CORRADE_COMPARE(material->attributeName(1), extractMaterialKey(AI_MATKEY_OPACITY));
        CORRADE_COMPARE(material->attributeType(1), MaterialAttributeType::Float);
        CORRADE_COMPARE(material->attribute<Float>(1), 0.4f);
    }
}

void AssimpImporterTest::materialRaw() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Disabled by default */
    CORRADE_VERIFY(!importer->configuration().value<bool>("forceRawMaterialData"));
    importer->configuration().setValue("forceRawMaterialData", true);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-raw.fbx")));
    CORRADE_COMPARE(importer->materialCount(), 2);

    Containers::Optional<MaterialData> material;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        material = importer->material("Custom_Types");
    }

    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType{});
    CORRADE_COMPARE(material->layerCount(), 1);
    {
        CORRADE_EXPECT_FAIL_IF(_assimpVersion < 500,
            "This version of Assimp doesn't import raw FBX material properties.");
        CORRADE_COMPARE(material->attributeCount(), 5);
    }

    /* Not all types and sizes are tested:
       - no importers currently output int2/3/4/5+ or float2/5+
       - to get double properties, we'd need to compile assimp with
         ASSIMP_DOUBLE_PRECISION, which breaks the plugin  */

    /* Attributes that would normally be recognized */
    CORRADE_VERIFY(!material->hasAttribute(MaterialAttribute::AmbientColor));
    CORRADE_VERIFY(!material->hasAttribute(MaterialAttribute::DiffuseColor));

    /* Raw Assimp attributes */
    {
        CORRADE_COMPARE(material->attributeName(0), extractMaterialKey(AI_MATKEY_COLOR_AMBIENT));
        CORRADE_COMPARE(material->attributeType(0), MaterialAttributeType::Vector3);
        CORRADE_COMPARE(material->attribute<Vector3>(0), (Color3{0.1f, 0.05f, 0.1f}));
    } {
        CORRADE_COMPARE(material->attributeName(1), extractMaterialKey(AI_MATKEY_COLOR_DIFFUSE));
        CORRADE_COMPARE(material->attributeType(1), MaterialAttributeType::Vector3);
        CORRADE_COMPARE(material->attribute<Vector3>(1), (Color3{0.4f, 0.2f, 0.1f}));
    } {
        CORRADE_COMPARE(material->attributeName(2), extractMaterialKey(AI_MATKEY_OPACITY));
        CORRADE_COMPARE(material->attributeType(2), MaterialAttributeType::Float);
        CORRADE_COMPARE(material->attribute<Float>(2), 0.25f);
    }

    if(_assimpVersion < 500) {
        CORRADE_WARN("This version of Assimp doesn't import raw FBX material properties.");

        CORRADE_COMPARE(out.str(), "");
    } else {
        /* Raw attributes taken directly from the FBX file, prefixed with "$raw.".
           Seems to be the only importer that supports that. */
        {
            CORRADE_COMPARE(material->attributeName(3), "$raw.SomeColor"_s);
            CORRADE_COMPARE(material->attributeType(3), MaterialAttributeType::Vector3);
            CORRADE_COMPARE(material->attribute<Vector3>(3), (Vector3{0.1f, 0.2f, 0.3f}));
        } {
            CORRADE_COMPARE(material->attributeName(4), "$raw.SomeString"_s);
            CORRADE_COMPARE(material->attributeType(4), MaterialAttributeType::String);
            CORRADE_COMPARE(material->attribute<Containers::StringView>(4), "Ministry of Finance (Turkmenistan)");
        }

        CORRADE_COMPARE(out.str(),
            "Trade::AssimpImporter::material(): property $raw.LongNameLongNameLongNameLongNameLongNameLongNameLongName is too large with 67 bytes, skipping\n"
            "Trade::AssimpImporter::material(): property $raw.LongValue is too large with 70 bytes, skipping\n");
    }

    if(_assimpVersion < 410)
        CORRADE_SKIP("glTF 2 is supported since Assimp 4.1.");

    /* glTF covers a few types/sizes not covered by FBX */
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-raw.gltf")));
    {
        /* There's an extra material with only default values */
        CORRADE_EXPECT_FAIL("glTF files are imported with a dummy material.");
        CORRADE_COMPARE(importer->materialCount(), 1);
    }

    material = importer->material("raw");
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType{});
    {
        CORRADE_EXPECT_FAIL_IF(_assimpVersion < 510,
            "glTF files with non-0 texture coordinate set add an extra diffuse-only material layer.");
        CORRADE_COMPARE(material->layerCount(), 1);
    }

    /* Attributes that would normally be recognized */
    CORRADE_VERIFY(!material->hasAttribute(MaterialAttribute::DiffuseColor));

    /* Raw attributes. Only checking interesting attributes, because this would
       be a nightmare to maintain across multiple Assimp versions. */
    {
        const Containers::StringView name = extractMaterialKey(AI_MATKEY_COLOR_DIFFUSE);
        CORRADE_VERIFY(material->hasAttribute(name));
        CORRADE_COMPARE(material->attributeType(name), MaterialAttributeType::Vector4);
        CORRADE_COMPARE(material->attribute<Vector4>(name), (Color4{0.8f, 0.2f, 0.4f, 0.3f}));
    } {
        /* For some reason Assimp adds an alpha channel to the emissive color */
        const Containers::StringView name = extractMaterialKey(AI_MATKEY_COLOR_EMISSIVE);
        CORRADE_VERIFY(material->hasAttribute(name));
        CORRADE_COMPARE(material->attributeType(name), MaterialAttributeType::Vector4);
        CORRADE_COMPARE(material->attribute<Vector4>(name), (Color4{0.1f, 0.2f, 0.3f}));
    } {
        /* The glTF importer writes bool properties as buffers with size 1,
           when all the other importers write them as ints */
        const Containers::StringView name = extractMaterialKey(AI_MATKEY_TWOSIDED);
        CORRADE_VERIFY(material->hasAttribute(name));
        CORRADE_COMPARE(material->attributeType(name), MaterialAttributeType::String);
        Containers::StringView value = material->attribute<Containers::StringView>(name);
        CORRADE_COMPARE(value.size(), 1);
        CORRADE_VERIFY(value.front() != 0);
    } {
        constexpr Containers::StringView name = _AI_MATKEY_TEXTURE_BASE ".NORMALS"_s;
        CORRADE_VERIFY(material->hasAttribute(name));
        CORRADE_COMPARE(material->attributeType(name), MaterialAttributeType::String);
        CORRADE_COMPARE(material->attribute<Containers::StringView>(name), "normals.png"_s);
    } {
        /* Testing that newer texture types are handled correctly by the SFINAE
           magic that produces fallback switch values */
        CORRADE_EXPECT_FAIL_IF(_assimpVersion < 510,
            "Versions before Assimp 5.1.0 don't import BASE_COLOR textures.");
        constexpr Containers::StringView name = _AI_MATKEY_TEXTURE_BASE ".BASE_COLOR"_s;
        const bool hasAttribute = material->hasAttribute(name);
        CORRADE_VERIFY(hasAttribute);
        /* Still have to skip the checks to not trigger asserts for missing
           attribute names in attributeType() and attribute() */
        if(hasAttribute) {
            CORRADE_COMPARE(material->attributeType(name), MaterialAttributeType::String);
            CORRADE_COMPARE(material->attribute<Containers::StringView>(name), "basecolor.png"_s);
        }
    } {
        CORRADE_EXPECT_FAIL_IF(_assimpVersion < 510,
            "Versions before Assimp 5.1.0 don't import AI_MATKEY_UVWSRC.");
        constexpr Containers::StringView name = _AI_MATKEY_UVWSRC_BASE ".NORMALS"_s;
        const bool hasAttribute = material->hasAttribute(name);
        CORRADE_VERIFY(hasAttribute);
        if(hasAttribute) {
            CORRADE_COMPARE(material->attributeType(name), MaterialAttributeType::Int);
            CORRADE_COMPARE(material->attribute<Int>(name), 1);
        }
    } {
        CORRADE_EXPECT_FAIL_IF(_assimpVersion < 510,
            "Versions before Assimp 5.1.0 don't import AI_MATKEY_UVTRANSFORM.");
        constexpr Containers::StringView name = _AI_MATKEY_UVTRANSFORM_BASE ".NORMALS"_s;
        const bool hasAttribute = material->hasAttribute(name);
        CORRADE_VERIFY(hasAttribute);
        if(hasAttribute) {
            /* Opaque buffer converted to String */
            CORRADE_COMPARE(material->attributeType(name), MaterialAttributeType::String);
            Containers::StringView value = material->attribute<Containers::StringView>(name);
            /* +1 is null byte */
            CORRADE_COMPARE(value.size(), sizeof(aiUVTransform));
            const aiUVTransform& transform = *reinterpret_cast<const aiUVTransform*>(value.data());
            const Vector2 scaling{transform.mScaling.x, transform.mScaling.y};
            CORRADE_COMPARE(scaling, (Vector2{0.25f, 0.75f}));
        }
    }
}

void AssimpImporterTest::materialRawTextureLayers() {
    if(_assimpVersion < 500)
        CORRADE_SKIP("This version of assimp doesn't imported layered FBX materials.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().setValue("forceRawMaterialData", true);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-layers.fbx")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<Trade::MaterialData> material = importer->material(0);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->layerCount(), 3);

    /* Layer 0. Material attributes + diffuse texture + ambient texture. */
    {
        {
            CORRADE_VERIFY(!material->hasAttribute(0, MaterialAttribute::AmbientColor));
            const Containers::StringView name = extractMaterialKey(AI_MATKEY_COLOR_AMBIENT);
            CORRADE_VERIFY(material->hasAttribute(0, name));
            CORRADE_COMPARE(material->attributeType(0, name), MaterialAttributeType::Vector3);
            CORRADE_COMPARE(material->attribute<Vector3>(0, name), (Color3{0.1f, 0.2f, 0.3f}));
        } {
            CORRADE_VERIFY(!material->hasAttribute(0, MaterialAttribute::DiffuseColor));
            const Containers::StringView name = extractMaterialKey(AI_MATKEY_COLOR_DIFFUSE);
            CORRADE_VERIFY(material->hasAttribute(0, name));
            CORRADE_COMPARE(material->attributeType(0, name), MaterialAttributeType::Vector3);
            CORRADE_COMPARE(material->attribute<Vector3>(0, name), (Color3{0.7f, 0.6f, 0.5f}));
        }{
            /* Texture indices are calculated from the attribute order inside
               AssimpImporter, but they're not material properties. Just check
               the name property. */
            CORRADE_VERIFY(!material->hasAttribute(0, MaterialAttribute::AmbientTexture));
            constexpr Containers::StringView name = _AI_MATKEY_TEXTURE_BASE ".AMBIENT"_s;
            CORRADE_VERIFY(material->hasAttribute(0, name));
            CORRADE_COMPARE(material->attributeType(0, name), MaterialAttributeType::String);
            CORRADE_COMPARE(material->attribute<Containers::StringView>(0, name), "ambient.png"_s);
        } {
            CORRADE_VERIFY(!material->hasAttribute(0, MaterialAttribute::AmbientTextureCoordinates));
            constexpr Containers::StringView name = _AI_MATKEY_UVWSRC_BASE ".AMBIENT"_s;
            CORRADE_VERIFY(material->hasAttribute(0, name));
            CORRADE_COMPARE(material->attributeType(0, name), MaterialAttributeType::Int);
            CORRADE_COMPARE(material->attribute<Int>(0, name), 0);
        } {
            CORRADE_VERIFY(!material->hasAttribute(0, MaterialAttribute::DiffuseTexture));
            constexpr Containers::StringView name = _AI_MATKEY_TEXTURE_BASE ".DIFFUSE"_s;
            CORRADE_VERIFY(material->hasAttribute(0, name));
            CORRADE_COMPARE(material->attributeType(0, name), MaterialAttributeType::String);
            CORRADE_COMPARE(material->attribute<Containers::StringView>(0, name), "one.jpg"_s);
        } {
            CORRADE_VERIFY(!material->hasAttribute(0, MaterialAttribute::DiffuseTextureCoordinates));
            constexpr Containers::StringView name = _AI_MATKEY_UVWSRC_BASE ".DIFFUSE"_s;
            CORRADE_VERIFY(material->hasAttribute(0, name));
            CORRADE_COMPARE(material->attributeType(0, name), MaterialAttributeType::Int);
            CORRADE_COMPARE(material->attribute<Int>(0, name), 0);
        }

    /* Layer 1. Diffuse texture. */
    } {
        /* We check that there are no non-texture attributes in this layer at
           the end of the test */
        {
            CORRADE_VERIFY(!material->hasAttribute(1, MaterialAttribute::DiffuseTexture));
            constexpr Containers::StringView name = _AI_MATKEY_TEXTURE_BASE ".DIFFUSE"_s;
            CORRADE_VERIFY(material->hasAttribute(1, name));
            CORRADE_COMPARE(material->attributeType(1, name), MaterialAttributeType::String);
            CORRADE_COMPARE(material->attribute<Containers::StringView>(1, name), "two.jpg"_s);
        } {
            CORRADE_VERIFY(!material->hasAttribute(1, MaterialAttribute::DiffuseTextureCoordinates));
            constexpr Containers::StringView name = _AI_MATKEY_UVWSRC_BASE ".DIFFUSE"_s;
            CORRADE_VERIFY(material->hasAttribute(1, name));
            CORRADE_COMPARE(material->attributeType(1, name), MaterialAttributeType::Int);
            CORRADE_COMPARE(material->attribute<Int>(1, name), 0);
        }

    /* Layer 2. Diffuse texture. */
    } {
        /* We check that there are no non-texture attributes in this layer at
           the end of the test */
        {
            CORRADE_VERIFY(!material->hasAttribute(2, MaterialAttribute::DiffuseTexture));
            constexpr Containers::StringView name = _AI_MATKEY_TEXTURE_BASE ".DIFFUSE"_s;
            CORRADE_VERIFY(material->hasAttribute(2, name));
            CORRADE_COMPARE(material->attributeType(2, name), MaterialAttributeType::String);
            CORRADE_COMPARE(material->attribute<Containers::StringView>(2, name), "three.jpg"_s);
        } {
            CORRADE_VERIFY(!material->hasAttribute(2, MaterialAttribute::DiffuseTextureCoordinates));
            constexpr Containers::StringView name = _AI_MATKEY_UVWSRC_BASE ".DIFFUSE"_s;
            CORRADE_VERIFY(material->hasAttribute(2, name));
            CORRADE_COMPARE(material->attributeType(2, name), MaterialAttributeType::Int);
            CORRADE_COMPARE(material->attribute<Int>(2, name), 0);
        }
    }

    /* Test that layers > 0 only contains texture attributes. Those all start
       with "$tex.":
       https://github.com/assimp/assimp/blob/889e55969647b9bd9e832d6208b41973156ce46b/include/assimp/material.h#L1051
       Non-texture attributes always have mIndex set to 0 so shouldn't be here. */
    for(UnsignedInt l = 1; l != material->layerCount(); ++l)
        for(UnsignedInt i = 0; i != material->attributeCount(l); ++i)
            CORRADE_VERIFY(material->attributeName(l, i).hasPrefix("$tex."_s));
}

void AssimpImporterTest::mesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae")));

    /* The first mesh is a dummy one to test non-trivial mesh assignment in the
       scene */
    CORRADE_COMPARE(importer->meshCount(), 2);

    const std::string name = fixMeshName("Cube", ".dae", _assimpVersion);
    CORRADE_COMPARE(importer->meshName(1), name);
    CORRADE_COMPARE(importer->meshForName(name), 1);
    CORRADE_COMPARE(importer->meshForName("nonexistent"), -1);

    Containers::Optional<MeshData> mesh = importer->mesh(1);
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

    {
        CORRADE_EXPECT_FAIL_IF(_assimpVersion < 320,
            "Assimp < 3.2 loads incorrect alpha value for the last color.");
        CORRADE_COMPARE_AS(mesh->attribute<Vector4>(MeshAttribute::Color),
        Containers::arrayView<Vector4>({
            {1.0f, 0.25f, 0.24f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.1f, 0.2f, 0.3f, 1.0f}
        }), TestSuite::Compare::Container);
    }

    /* Check mesh assignment in the scene. The first two objects are dummy
       nodes to test non-trivial assignment; however unfortunately the meshes
       are parsed by Assimp ONLY if referenced from a node so one of the dummy
       nodes has to reference the other dummy mesh. SIGH. */
    CORRADE_COMPARE(importer->sceneCount(), 1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->fieldCount(), 5);

    /* Fields we're not interested in */
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
    CORRADE_VERIFY(scene->hasField(SceneField::Transformation));

    CORRADE_VERIFY(scene->hasField(SceneField::Mesh));
    CORRADE_VERIFY(scene->hasField(SceneField::MeshMaterial));
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        1, 2
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0, 1
    }), TestSuite::Compare::Container);
    /* Material assignment tested in materialColor(), skin in skin(). Assimp
       has no concept of an "unassigned material" and it always adds some dummy
       one, so the material is never -1. */
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::MeshMaterial), Containers::arrayView<Int>({
        0, 0
    }), TestSuite::Compare::Container);
}

void AssimpImporterTest::pointMesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "points.obj")));

    CORRADE_COMPARE(importer->meshCount(), 1);

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
}

void AssimpImporterTest::lineMesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "line.dae")));

    CORRADE_COMPARE(importer->meshCount(), 1);

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
}

void AssimpImporterTest::polygonMesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "polygon.obj")));

    /* Just testing that triangulation doesn't break anything */

    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1, 2, 0, 2, 3}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {-1.0f,  1.0f, 0.0f}, { 1.0f,  1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f}
        }), TestSuite::Compare::Container);
}

void AssimpImporterTest::meshCustomAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae")));

    /* Custom attributes should be available right after loading a file,
       even for files without joint weights */
    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    CORRADE_COMPARE(jointsAttribute, meshAttributeCustom(0));
    CORRADE_COMPARE(importer->meshAttributeName(jointsAttribute), "JOINTS");
    CORRADE_COMPARE(importer->meshAttributeForName("Nonexistent"), MeshAttribute{});

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

    if(!supportsSkinning(data.suffix, _assimpVersion))
        CORRADE_SKIP("Skin data for this file type is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "skin"_s + data.suffix)));

    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");

    const std::string meshNames[]{
        fixMeshName("Mesh_1"_s, data.suffix, _assimpVersion),
        fixMeshName("Mesh_2"_s, data.suffix, _assimpVersion)
    };

    for(const std::string& meshName: meshNames) {
        Containers::Optional<MeshData> mesh;
        std::ostringstream out;
        {
            Warning redirectWarning{&out};
            mesh = importer->mesh(meshName);
        }
        /* No warning about glTF dropping sets of weights */
        CORRADE_COMPARE(out.str(), "");

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
    Containers::Optional<Trade::MeshData> mesh = importer->mesh(fixMeshName("Plane"_s, data.suffix, _assimpVersion));
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->hasAttribute(jointsAttribute));
}

void AssimpImporterTest::meshSkinningAttributesMultiple() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    /* Disable default limit, 0 = no limit  */
    CORRADE_COMPARE(importer->configuration().value<UnsignedInt>("maxJointWeights"), 4);
    importer->configuration().setValue("maxJointWeights", 0);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "skin-multiple-sets.dae")));

    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");

    constexpr UnsignedInt AttributeCount = 3;

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(fixMeshName("Mesh_1"_s, ".dae"_s, _assimpVersion));
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->hasAttribute(jointsAttribute));
    CORRADE_COMPARE(mesh->attributeCount(jointsAttribute), AttributeCount);
    CORRADE_COMPARE(mesh->attributeFormat(jointsAttribute), VertexFormat::Vector4ui);
    CORRADE_VERIFY(mesh->hasAttribute(weightsAttribute));
    CORRADE_COMPARE(mesh->attributeCount(weightsAttribute), AttributeCount);
    CORRADE_COMPARE(mesh->attributeFormat(weightsAttribute), VertexFormat::Vector4);

    for(UnsignedInt i = 0; i != AttributeCount; ++i) {
        Containers::StridedArrayView1D<const Vector4ui> joints = mesh->attribute<Vector4ui>(jointsAttribute, i);
        Containers::StridedArrayView1D<const Vector4> weights = mesh->attribute<Vector4>(weightsAttribute, i);
        const Vector4ui jointValues = Vector4ui{0, 1, 2, 3} + Vector4ui{i*4};
        constexpr Vector4 weightValues = Vector4{0.083333f};
        for(UnsignedInt v = 0; v != joints.size(); ++v) {
            CORRADE_COMPARE(joints[v], jointValues);
            CORRADE_COMPARE(weights[v], weightValues);
        }
    }
}

void AssimpImporterTest::meshSkinningAttributesMultipleGltf() {
    if(!supportsSkinning(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 skinning is not supported with the current version of Assimp");

    /* Assimp glTF 2 importer only reads the last(!) set of joint weights. On
       5.1.0 it outright fails to import because of broken extra validation:
       https://github.com/assimp/assimp/issues/4178 */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().setValue("maxJointWeights", 0);

    if(_assimpVersion >= 510) {
        CORRADE_VERIFY(!importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "skin-multiple-sets.gltf")));
        CORRADE_SKIP("Current version of assimp fails to import files with multiple sets of skinning attributes");
    }

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "skin-multiple-sets.gltf")));

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

    Containers::StridedArrayView1D<const Vector4ui> joints = mesh->attribute<Vector4ui>(jointsAttribute);
    CORRADE_VERIFY(joints);
    Containers::StridedArrayView1D<const Vector4> weights = mesh->attribute<Vector4>(weightsAttribute);
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

    Containers::StridedArrayView1D<const Vector4> colors1 = mesh->attribute<Vector4>(MeshAttribute::Color, 0);
    Containers::StridedArrayView1D<const Vector4> colors2 = mesh->attribute<Vector4>(MeshAttribute::Color, 1);
    CORRADE_VERIFY(colors1);
    CORRADE_VERIFY(colors2);
    CORRADE_COMPARE(colors1.front(), Color4{1.0f});
    CORRADE_COMPARE(colors2.front(), Color4{0.5f});
}

void AssimpImporterTest::meshSkinningAttributesMaxJointWeights() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().setValue("maxJointWeights", 6);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "skin-multiple-sets.dae")));

    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");

    /* 6 weights = 2 sets of 4, last two weights zero */
    constexpr UnsignedInt AttributeCount = 2;

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(fixMeshName("Mesh_1"_s, ".dae"_s, _assimpVersion));
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
        Containers::StridedArrayView1D<const Vector4ui> joints = mesh->attribute<Vector4ui>(jointsAttribute, i);
        Containers::StridedArrayView1D<const Vector4> weights = mesh->attribute<Vector4>(weightsAttribute, i);
        for(UnsignedInt v = 0; v != joints.size(); ++v) {
            CORRADE_COMPARE(joints[v], jointValues[i]);
            CORRADE_COMPARE(weights[v], weightValues[i]);
        }
    }
}

void AssimpImporterTest::meshSkinningAttributesDummyWeightRemoval() {
    if(!supportsSkinning(".gltf"_s, _assimpVersion))
        CORRADE_SKIP("glTF 2 skinning is not supported with the current version of Assimp");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().setValue("maxJointWeights", 0);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "skin-dummy-weights.gltf")));

    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");

    Containers::Optional<Trade::MeshData> mesh = importer->mesh("Mesh");
    CORRADE_VERIFY(mesh);

    /* Without ignoring dummy weights, the max joint count per vertex
       would be too high, giving us extra sets of (empty) weights. */
    CORRADE_COMPARE(mesh->attributeCount(jointsAttribute), 1);
    CORRADE_COMPARE(mesh->attributeCount(weightsAttribute), 1);

    Containers::StridedArrayView1D<const Vector4ui> joints = mesh->attribute<Vector4ui>(jointsAttribute);
    CORRADE_VERIFY(joints);
    Containers::StridedArrayView1D<const Vector4> weights = mesh->attribute<Vector4>(weightsAttribute);
    CORRADE_VERIFY(weights);

    constexpr Vector4ui joint{0, 1, 2, 5};
    constexpr Vector4 weight{0.25f};
    CORRADE_COMPARE(joints.front(), joint);
    CORRADE_COMPARE(weights.front(), weight);
}

void AssimpImporterTest::meshSkinningAttributesMerge() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->configuration().setValue("mergeSkins", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "skin.dae")));

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
        const Int id = importer->meshForName(fixMeshName("Mesh_1"_s, ".dae"_s, _assimpVersion));
        CORRADE_VERIFY(id != -1);
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(id);
        CORRADE_VERIFY(mesh);
        CORRADE_VERIFY(mesh->hasAttribute(jointsAttribute));
        CORRADE_COMPARE(mesh->attributeFormat(jointsAttribute), VertexFormat::Vector4ui);
        Containers::ArrayView<const Vector4ui> jointData = id == 0
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
        const Int id = importer->meshForName(fixMeshName("Mesh_2"_s, ".dae"_s, _assimpVersion));
        CORRADE_VERIFY(id != -1);
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(id);
        CORRADE_VERIFY(mesh);
        CORRADE_VERIFY(mesh->hasAttribute(jointsAttribute));
        CORRADE_COMPARE(mesh->attributeFormat(jointsAttribute), VertexFormat::Vector4ui);
        Containers::ArrayView<const Vector4ui> jointData = id == 0
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
    if(_assimpVersion <= 320)
        CORRADE_SKIP("Assimp 3.2 doesn't recognize primitives used in the test COLLADA file.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "mesh-multiple-primitives.dae")));

    /* Two meshes, but one has two primitives and one three. */
    CORRADE_COMPARE(importer->meshCount(), 5);
    {
        const std::string name = fixMeshName("Multi-primitive triangle fan, line strip", ".dae", _assimpVersion);
        CORRADE_COMPARE(importer->meshName(0), name);
        CORRADE_COMPARE(importer->meshName(1), name);
        CORRADE_COMPARE(importer->meshForName(name), 0);

        Containers::Optional<Trade::MeshData> mesh0 = importer->mesh(0);
        CORRADE_VERIFY(mesh0);
        CORRADE_COMPARE(mesh0->primitive(), MeshPrimitive::Triangles);
        Containers::Optional<Trade::MeshData> mesh1 = importer->mesh(1);
        CORRADE_VERIFY(mesh1);
        CORRADE_COMPARE(mesh1->primitive(), MeshPrimitive::Lines);
    } {
        const std::string name = fixMeshName("Multi-primitive lines, triangles, triangle strip", ".dae", _assimpVersion);
        CORRADE_COMPARE(importer->meshName(2), name);
        CORRADE_COMPARE(importer->meshName(3), name);
        CORRADE_COMPARE(importer->meshName(4), name);
        CORRADE_COMPARE(importer->meshForName(name), 2);

        Containers::Optional<Trade::MeshData> mesh2 = importer->mesh(2);
        CORRADE_VERIFY(mesh2);
        CORRADE_COMPARE(mesh2->primitive(), MeshPrimitive::Lines);
        Containers::Optional<Trade::MeshData> mesh3 = importer->mesh(3);
        CORRADE_VERIFY(mesh3);
        CORRADE_COMPARE(mesh3->primitive(), MeshPrimitive::Triangles);
        Containers::Optional<Trade::MeshData> mesh4 = importer->mesh(4);
        CORRADE_VERIFY(mesh4);
        CORRADE_COMPARE(mesh4->primitive(), MeshPrimitive::Triangles);
    }

    /* Four objects, Two refer a three-primitive mesh and one refers a
       two-primitive one, which is done by having multiple mesh entries for
       them. One is meshless and is thus skipped in the mesh field. */
    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_COMPARE(scene->mappingBound(), 4);
    CORRADE_COMPARE(scene->fieldCount(), 5);
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
    CORRADE_VERIFY(scene->hasField(SceneField::Transformation));
    CORRADE_VERIFY(scene->hasField(SceneField::Mesh));
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0, 0, 0, 2, 2, 2, 3, 3
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        2, 3, 4, 2, 3, 4, 0, 1
    }), TestSuite::Compare::Container);
    CORRADE_VERIFY(scene->hasField(SceneField::MeshMaterial));
    {
        /* If I change the <bind_material> to something that actually
           references an *existing* material, the mesh count explodes to 8.
           Too annoyed to investigate further. */
        CORRADE_EXPECT_FAIL("Can't figure out how to bind the materials to not make the mesh count explode even further.");
        CORRADE_COMPARE_AS(scene->field<Int>(SceneField::MeshMaterial), Containers::arrayView<Int>({
            1, 2, 0, 1, 2, 0, 3, 1
        }), TestSuite::Compare::Container);
    }
}

void AssimpImporterTest::emptyCollada() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    /* Instead of giving out an empty file, assimp fails on opening, but only
       for COLLADA, not for e.g. glTF. I have a different opinion about the
       behavior, but whatever. It's also INTERESTING that supplying an empty
       DAE through file callbacks results in a completely different message --
       see fileCallbackEmptyFile(). */
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "empty.dae")));
}

void AssimpImporterTest::emptyGltf() {
    if(_assimpVersion < 410)
        CORRADE_SKIP("glTF 2 is supported since Assimp 4.1.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    /* We can't reuse CgltfImporter's empty.gltf since Assimp 5.1 complains
       about a missing scene property (which is not required by the glTF spec) */
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "empty.gltf")));
    {
        CORRADE_EXPECT_FAIL_IF(_assimpVersion < 510, "Assimp versions before 5.1.0 ignore empty glTF scenes.");
        CORRADE_COMPARE(importer->defaultScene(), 0);
        CORRADE_COMPARE(importer->sceneCount(), 1);
        CORRADE_COMPARE(importer->objectCount(), 1);
    }

    /* No crazy meshes created for an empty glTF file, unlike with COLLADA
       files that have no meshes */
    CORRADE_COMPARE(importer->meshCount(), 0);
}

void AssimpImporterTest::scene() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae")));

    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->sceneCount(), 1);

    /* Currently only glTF supports scene names */
    CORRADE_COMPARE(importer->sceneName(0), "");

    /* Gotta admit this test is quite underwhelming. I hope it won't fire
       back at some point in the future. */
    CORRADE_COMPARE(importer->objectCount(), 2);
    CORRADE_COMPARE(importer->objectName(1), "Child");
    CORRADE_COMPARE(importer->objectForName("Child"), 1);
    CORRADE_COMPARE(importer->objectForName("Nonexistent"), -1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_VERIFY(scene->importerState());
    CORRADE_VERIFY(scene->is3D());
    CORRADE_COMPARE(scene->mappingBound(), 2);
    CORRADE_COMPARE(scene->mappingType(), SceneMappingType::UnsignedInt);
    CORRADE_COMPARE(scene->fieldCount(), 3);

    /* Parents */
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_COMPARE(scene->fieldFlags(SceneField::Parent), SceneFieldFlag::ImplicitMapping);
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Parent), Containers::arrayView<UnsignedInt>({
        0, 1
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE(scene->fieldType(SceneField::Parent), SceneFieldType::Int);
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
        -1, 0
    }), TestSuite::Compare::Container);

    /* Importer state shares the same object mapping as parents and it's all
       non-null pointers. */
    CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
    CORRADE_COMPARE(scene->fieldFlags(SceneField::ImporterState), SceneFieldFlag::ImplicitMapping);
    CORRADE_COMPARE_AS(
        scene->mapping<UnsignedInt>(SceneField::ImporterState),
        scene->mapping<UnsignedInt>(SceneField::Parent),
        TestSuite::Compare::Container);
    for(const void* a: scene->field<const void*>(SceneField::ImporterState)) {
        CORRADE_VERIFY(a);
    }

    /* Transformation shares the same object mapping as well */
    CORRADE_VERIFY(scene->hasField(SceneField::Transformation));
    CORRADE_COMPARE(scene->fieldFlags(SceneField::Transformation), SceneFieldFlag::ImplicitMapping);
    CORRADE_COMPARE_AS(
        scene->mapping<UnsignedInt>(SceneField::Transformation),
        scene->mapping<UnsignedInt>(SceneField::Parent),
        TestSuite::Compare::Container);
    CORRADE_COMPARE(scene->fieldType(SceneField::Parent), SceneFieldType::Int);
    CORRADE_COMPARE_AS(scene->field<Matrix4>(SceneField::Transformation), Containers::arrayView({
        Matrix4::scaling({1.0f, 2.0f, 3.0f}),
        Matrix4{{0.813798f, 0.469846f, -0.34202f, 0.0f},
                {-0.44097f, 0.882564f, 0.163176f, 0.0f},
                {0.378522f, 0.0180283f, 0.925417f, 0.0f},
                {1.0f, 2.0f, 3.0f, 1.0f}}
    }), TestSuite::Compare::Container);

    /* Camera assignment tested separately in camera(), light assignment in
       light(), mesh in mesh(), material in materialColor() and skin in
       skin() */
}

void AssimpImporterTest::sceneName() {
    #if !ASSIMP_HAS_SCENE_NAME
    CORRADE_SKIP("Current version of assimp doesn't expose scene names.");
    #endif

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "scene-name.gltf")));

    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->sceneName(0), "This is the scene");
    CORRADE_COMPARE(importer->sceneForName("This is the scene"), 0);
    CORRADE_COMPARE(importer->sceneForName("Other scene"), -1);
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

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "scene+mesh.dae")));

    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 1);

    /* Name of the scene is used for the root object */
    {
        /** @todo Possibly works with other versions (definitely not 3.0) */
        CORRADE_EXPECT_FAIL_IF(_assimpVersion <= 320,
            "Assimp 3.2 and below doesn't use name of the root node for collapsed nodes.");
        CORRADE_COMPARE(importer->objectName(0), "Scene");
        CORRADE_COMPARE(importer->objectForName("Scene"), 0);
    }

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->mappingBound(), 1);
    CORRADE_COMPARE(scene->fieldCount(), 5); /* Just the root node */

    /* Field we're not interested in */
    CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));

    /* One object contained in the root in an implicit transformation */
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Parent), Containers::arrayView<UnsignedInt>({
        0
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
        -1
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<Matrix4>(SceneField::Transformation), Containers::arrayView({
        Matrix4{}
    }), TestSuite::Compare::Container);

    /* Assimp makes some bogus mesh for this one */
    CORRADE_VERIFY(scene->hasField(SceneField::Mesh));
    CORRADE_VERIFY(scene->hasField(SceneField::MeshMaterial));
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::MeshMaterial), Containers::arrayView<Int>({
        0
    }), TestSuite::Compare::Container);
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, data.file)));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->sceneCount(), 1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->mappingBound(), 2);

    Containers::StridedArrayView1D<const Matrix4> transformations = scene->field<Matrix4>(SceneField::Transformation);
    CORRADE_COMPARE(transformations.size(), 2);

    /* First object is directly in the root, second object is a child of the
       first. */
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Parent), Containers::arrayView<UnsignedInt>({
        0, 1
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
        -1, 0
    }), TestSuite::Compare::Container);

    /* Both have the same mesh assigned */
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0, 1
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0, 0
    }), TestSuite::Compare::Container);

    /* The first mesh should have always the same final positions independently
       of how file's Y/Z-up or PreTransformVertices is set */
    {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);

        /* Transform the positions with object transform */
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
        MeshTools::transformPointsInPlace(transformations[0],
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
            transformations[0]*transformations[1],
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

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, data.file)));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->sceneCount(), 1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->mappingBound(), 1);

    /* There's only one object, directly in the root, with no transformation */
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Parent), Containers::arrayView<UnsignedInt>({
        0
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
        -1
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<Matrix4>(SceneField::Transformation), Containers::arrayView({
        Matrix4{}
    }), TestSuite::Compare::Container);

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

    if(_assimpVersion <= 320)
        CORRADE_SKIP("Assimp < 3.2 can't load embedded textures in blend files, Assimp 3.2 can't detect blend file format when opening a memory location.");

    /* Open as data, so we verify opening embedded images from data does not
       cause any problems even when no file callbacks are set */
    Containers::Optional<Containers::Array<char>> data = Utility::Path::read(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "embedded-texture.blend"));
    CORRADE_VERIFY(data);
    CORRADE_VERIFY(importer->openData(*data));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::imageExternal() {
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(_assimpVersion < 320)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae")));

    CORRADE_COMPARE(importer->image2DCount(), 2);
    Containers::Optional<ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i{1});
    constexpr char pixels[] = { '\xb3', '\x69', '\x00', '\xff' };
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels), TestSuite::Compare::Container);
}

void AssimpImporterTest::imageExternalNotFound() {
    /** @todo Possibly fails on more versions (definitely w/ 3.0 and 3.2) */
    if(_assimpVersion <= 320)
        CORRADE_SKIP("Assimp <= 3.2 would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "image-not-found.dae")));

    CORRADE_COMPARE(importer->image2DCount(), 1);

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer->image2D(0));
        /* There's an error from Path::read() before */
        CORRADE_COMPARE_AS(out.str(),
            "\nTrade::AbstractImporter::openFile(): cannot open file /not-found.png\n",
            TestSuite::Compare::StringHasSuffix);
    }

    /* The (failed) importer should get cached even in case of failure, so
       the message should get printed just once */
    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer->image2D(0));
        CORRADE_COMPARE(out.str(), "");
    }
}

void AssimpImporterTest::imageExternalNoPathNoCallback() {
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(_assimpVersion < 320)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    Containers::Optional<Containers::Array<char>> data = Utility::Path::read(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae"));
    CORRADE_VERIFY(data);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openData(*data));
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "image-filename-trailing-space.obj")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "image-filename-backslash.obj")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "image-mips.obj")));
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
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(_assimpVersion < 320)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae")));
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
    /* Same as scene(), except that it uses openState() instead of openFile() */

    Assimp::Importer _importer;
    /* Explicitly *not* setting AI_CONFIG_IMPORT_NO_SKELETON_MESHES here to
       verify that we survive all the shit it summons from within the bug
       swamp. */
    const aiScene* sc = _importer.ReadFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae"), aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_JoinIdenticalVertices);
    CORRADE_VERIFY(sc != nullptr);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    importer->openState(sc);
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->objectCount(), 2);
    CORRADE_COMPARE(importer->objectName(1), "Child");
    CORRADE_COMPARE(importer->objectForName("Child"), 1);
    CORRADE_COMPARE(importer->objectForName("Nonexistent"), -1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_VERIFY(scene->importerState());
    CORRADE_VERIFY(scene->is3D());
    CORRADE_COMPARE(scene->mappingBound(), 2);
    CORRADE_COMPARE(scene->mappingType(), SceneMappingType::UnsignedInt);
    CORRADE_COMPARE(scene->fieldCount(), 3);

    /* Parents */
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_COMPARE(scene->fieldFlags(SceneField::Parent), SceneFieldFlag::ImplicitMapping);
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Parent), Containers::arrayView<UnsignedInt>({
        0, 1
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE(scene->fieldType(SceneField::Parent), SceneFieldType::Int);
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
        -1, 0
    }), TestSuite::Compare::Container);

    /* Importer state shares the same object mapping as parents and it's all
       non-null pointers. */
    CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
    CORRADE_COMPARE(scene->fieldFlags(SceneField::ImporterState), SceneFieldFlag::ImplicitMapping);
    CORRADE_COMPARE_AS(
        scene->mapping<UnsignedInt>(SceneField::ImporterState),
        scene->mapping<UnsignedInt>(SceneField::Parent),
        TestSuite::Compare::Container);
    for(const void* a: scene->field<const void*>(SceneField::ImporterState)) {
        CORRADE_VERIFY(a);
    }

    /* Transformation shares the same object mapping as well */
    CORRADE_VERIFY(scene->hasField(SceneField::Transformation));
    CORRADE_COMPARE(scene->fieldFlags(SceneField::Transformation), SceneFieldFlag::ImplicitMapping);
    CORRADE_COMPARE_AS(
        scene->mapping<UnsignedInt>(SceneField::Transformation),
        scene->mapping<UnsignedInt>(SceneField::Parent),
        TestSuite::Compare::Container);
    CORRADE_COMPARE(scene->fieldType(SceneField::Parent), SceneFieldType::Int);
    CORRADE_COMPARE_AS(scene->field<Matrix4>(SceneField::Transformation), Containers::arrayView({
        Matrix4::scaling({1.0f, 2.0f, 3.0f}),
        Matrix4{{0.813798f, 0.469846f, -0.34202f, 0.0f},
                {-0.44097f, 0.882564f, 0.163176f, 0.0f},
                {0.378522f, 0.0180283f, 0.925417f, 0.0f},
                {1.0f, 2.0f, 3.0f, 1.0f}}
    }), TestSuite::Compare::Container);

    /* Verify that closing works as well, without double frees or null pointer
       accesses */
    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void AssimpImporterTest::openStateTexture() {
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(_assimpVersion < 320)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Assimp::Importer _importer;
    const aiScene* sc = _importer.ReadFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae"), aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_JoinIdenticalVertices);
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae")));

    CORRADE_COMPARE(importer->meshCount(), 2);

    Containers::Optional<MeshData> mesh = importer->mesh(1);
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

    Containers::Optional<Containers::Array<char>> dae = Utility::Path::read(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "mesh.dae"));
    CORRADE_VERIFY(dae);

    std::unordered_map<std::string, Containers::Array<char>> files;
    files["not/a/path/mesh.dae"] = *std::move(dae);
    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy policy,
        std::unordered_map<std::string, Containers::Array<char>>& files) {
            Debug{} << "Loading" << filename << "with" << policy;
            return Containers::optional(Containers::ArrayView<const char>(files.at(filename)));
        }, files);

    CORRADE_VERIFY(importer->openFile("not/a/path/mesh.dae"));
    CORRADE_COMPARE(importer->meshCount(), 2);

    /* Same as in mesh(), testing just the basics, no need to repeat everything
       here */
    Containers::Optional<MeshData> mesh = importer->mesh(1);
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
    if(_assimpVersion >= 500)
        CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openFile(): failed to open some-file.dae: Failed to open file 'some-file.dae'.\n");
    else
        CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openFile(): failed to open some-file.dae: Failed to open file some-file.dae.\n");
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
    /* INTERESTINGLY ENOUGH, a completely different message altogether,
       independently on the version, is printed when opening a DAE file
       directly w/o callbacks -- see emptyCollada() above. The message changed
       in 5.1 due to a change from IrrXML to PugiXML:
       https://github.com/assimp/assimp/pull/2966 */
    if(_assimpVersion < 510)
        CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openFile(): failed to open some-file.dae: File is too small\n");
    else
        CORRADE_COMPARE(out.str(), "Trade::AssimpImporter::openFile(): failed to open some-file.dae: Unable to read file, malformed XML\n");
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
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(_assimpVersion < 320)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    Containers::Optional<Containers::Array<char>> dae = Utility::Path::read(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae"));
    Containers::Optional<Containers::Array<char>> png = Utility::Path::read(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "diffuse_texture.png"));
    CORRADE_VERIFY(dae);
    CORRADE_VERIFY(png);

    std::unordered_map<std::string, Containers::Array<char>> files;
    files["not/a/path/texture.dae"] = *std::move(dae);
    files["not/a/path/diffuse_texture.png"] = *std::move(png);
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
    /** @todo Possibly works with earlier versions (definitely not 3.0) */
    if(_assimpVersion < 320)
        CORRADE_SKIP("Current version of assimp would SEGFAULT on this test.");

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy,
        void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{};
        });

    Containers::Optional<Containers::Array<char>> data = Utility::Path::read(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "material-texture.dae"));
    CORRADE_VERIFY(data);
    CORRADE_VERIFY(importer->openData(*data));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(1));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file diffuse_texture.png\n");
}

void AssimpImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "scene.dae")));

    /* Shouldn't crash, leak or anything */
}

void AssimpImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AssimpImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASSIMPIMPORTER_TEST_DIR, "camera.dae")));
    CORRADE_COMPARE(importer->cameraCount(), 3);

    /* Verify that everything is working the same way on second use. It's only
       testing a single data type, but better than nothing at all. */
    {
        Containers::Optional<CameraData> camera = importer->camera(1);
        CORRADE_VERIFY(camera);
        CORRADE_COMPARE(camera->fov(), 49.13434_degf);
        CORRADE_COMPARE(camera->aspectRatio(), 1.77777f);
        CORRADE_COMPARE(camera->near(), 0.123f);
        CORRADE_COMPARE(camera->far(), 123.0f);
    } {
        Containers::Optional<CameraData> camera = importer->camera(1);
        CORRADE_VERIFY(camera);
        CORRADE_COMPARE(camera->fov(), 49.13434_degf);
        CORRADE_COMPARE(camera->aspectRatio(), 1.77777f);
        CORRADE_COMPARE(camera->near(), 0.123f);
        CORRADE_COMPARE(camera->far(), 123.0f);
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::AssimpImporterTest)
