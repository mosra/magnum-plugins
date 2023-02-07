/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2022 Samuli Raivio <bqqbarbhg@gmail.com>

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
#include <string>
#include <unordered_map>

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/Containers/BitArray.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Containers/StridedBitArrayView.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/String.h>
#include <Magnum/FileCallback.h>
#include <Magnum/Animation/Player.h>
#include <Magnum/DebugTools/CompareMaterial.h>
#include <Magnum/Math/Angle.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Math/PackingBatch.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/AnimationData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>
#include <Magnum/Trade/PbrSpecularGlossinessMaterialData.h>
#include <Magnum/Trade/PbrClearCoatMaterialData.h>

#include "../UfbxMaterials.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct UfbxImporterTest: TestSuite::Tester {
    explicit UfbxImporterTest();

    void openFile();
    void openData();
    void openFileFailed();
    void openDataFailed();

    void maxTemporaryMemory();
    void maxResultMemory();

    void fileCallback();
    void fileCallbackNotFound();
    void fileCallbackEmpty();
    void fileCallbackEmptyVerbose();

    void scene();
    void mesh();
    void meshPointLine();
    void camera();
    void cameraName();
    void cameraOrientation();
    void light();
    void lightName();
    void lightBadDecay();
    void lightOrientation();

    void geometricTransformHelperNodes();
    void geometricTransformModifyGeometry();
    void geometricTransformPreserve();
    void geometricTransformInvalidHandling();

    void materialMapping();

    void blenderMaterials();
    void materialMayaPhong();
    void materialMayaPhongFactors();
    void materialMayaArnold();
    void materialMayaArnoldFactors();
    void materialMaxPhysical();
    void materialMaxPhysicalFactors();
    void materialMaxPbrSpecGloss();
    void materialLayeredPbrTextures();
    void meshMaterials();
    void instancedMaterial();
    void textureTransform();
    void textureWrapModes();

    void imageEmbedded();
    void imageExternal();
    void imageExternalFromData();
    void imageFileCallback();
    void imageFileCallbackNotFound();
    void imageDeduplication();
    void imageNonExistentName();
    void imageAbsolutePath();
    void imageNot2D();
    void imageBrokenExternal();
    void imageBrokenEmbedded();

    void objCube();
    void objCubeFileCallback();
    void objMissingMtl();
    void objMissingMtlFileCallback();

    void mtl();

    void normalizeUnitsAdjustTransforms();
    void normalizeUnitsTransformRoot();
    void normalizeUnitsInvalidHandling();

    void geometryCache();
    void geometryCacheFileCallback();

    void staticSkin();
    void multiWarning();
    void multiWarningData();

    void animationInterpolation();
    void animationRotationPivot();
    void animationVisibility();
    void animationLayersMerged();
    void animationLayersRetained();
    void animationLayersNonLinearWeight();
    void animationLayersNonLinearWeightNonResampled();
    void animationStacks();
    void animationLayerNames();
    void animationStackNames();

    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

using FileCallbackFiles = std::unordered_map<std::string, Containers::Optional<Containers::Array<char>>>;
Containers::Optional<Containers::ArrayView<const char>> fileCallbackFunc(const std::string& filename, InputFileCallbackPolicy, void* user) {
    auto& files = *static_cast<FileCallbackFiles*>(user);

    Containers::Optional<Containers::Array<char>>& data = files[filename];

    Containers::String path = Utility::Path::join(UFBXIMPORTER_TEST_DIR, filename);
    if(!data && Utility::Path::exists(path))
        data = Utility::Path::read(path);

    if(data)
        return Containers::optional(Containers::ArrayView<const char>(*data));
    return {};
}

using namespace Math::Literals;
using namespace Containers::Literals;

const struct {
    const char* name;
    Containers::Optional<bool> generateIndices;
    bool expectedIndices;
} MeshGenerateIndicesData[]{
    {"", {}, true},
    {"don't generate indices", false, false}
};

const struct {
    const char* name;
    ImporterFlags flags;
    bool quiet;
} QuietData[]{
    {"", {}, false},
    {"quiet", ImporterFlag::Quiet, true}
};

const struct {
    Long maxMemory;
    bool shouldLoad;
} MaxMemoryData[]{
    {0, false},
    {1, false},
    {4096, false},
    {64*1024*1024, true},
    {Long(0x7fffffffu) + 1, true},
    {Long(0xffffffffu) + 1, true},
    {-999, true},
};

constexpr struct {
    bool resampleRotation;
} ResampleRotationData[]{
    {true},
    {false},
};

UfbxImporterTest::UfbxImporterTest() {
    addTests({&UfbxImporterTest::openFile,
              &UfbxImporterTest::openData,
              &UfbxImporterTest::openFileFailed,
              &UfbxImporterTest::openDataFailed});

    addInstancedTests({&UfbxImporterTest::maxTemporaryMemory,
                       &UfbxImporterTest::maxResultMemory},
        Containers::arraySize(MaxMemoryData));

    addTests({&UfbxImporterTest::fileCallback,
              &UfbxImporterTest::fileCallbackNotFound,
              &UfbxImporterTest::fileCallbackEmpty,
              &UfbxImporterTest::fileCallbackEmptyVerbose,

              &UfbxImporterTest::scene});

    addInstancedTests({&UfbxImporterTest::mesh,
                       &UfbxImporterTest::meshPointLine},
        Containers::arraySize(MeshGenerateIndicesData));

    addTests({&UfbxImporterTest::camera,
              &UfbxImporterTest::cameraName,
              &UfbxImporterTest::cameraOrientation,
              &UfbxImporterTest::light,
              &UfbxImporterTest::lightName});

    addInstancedTests({&UfbxImporterTest::lightBadDecay},
        Containers::arraySize(QuietData));

    addTests({&UfbxImporterTest::lightOrientation});

    addTests({&UfbxImporterTest::geometricTransformHelperNodes,
              &UfbxImporterTest::geometricTransformModifyGeometry,
              &UfbxImporterTest::geometricTransformPreserve,
              &UfbxImporterTest::geometricTransformInvalidHandling,

              &UfbxImporterTest::materialMapping,

              &UfbxImporterTest::blenderMaterials,
              &UfbxImporterTest::materialMayaPhong,
              &UfbxImporterTest::materialMayaPhongFactors,
              &UfbxImporterTest::materialMayaArnold,
              &UfbxImporterTest::materialMayaArnoldFactors,
              &UfbxImporterTest::materialMaxPhysical,
              &UfbxImporterTest::materialMaxPhysicalFactors,
              &UfbxImporterTest::materialMaxPbrSpecGloss,
              &UfbxImporterTest::materialLayeredPbrTextures,
              &UfbxImporterTest::meshMaterials,
              &UfbxImporterTest::instancedMaterial,
              &UfbxImporterTest::textureTransform,
              &UfbxImporterTest::textureWrapModes,

              &UfbxImporterTest::imageEmbedded,
              &UfbxImporterTest::imageExternal,
              &UfbxImporterTest::imageExternalFromData,
              &UfbxImporterTest::imageFileCallback,
              &UfbxImporterTest::imageFileCallbackNotFound,
              &UfbxImporterTest::imageDeduplication,
              &UfbxImporterTest::imageNonExistentName,
              &UfbxImporterTest::imageAbsolutePath,
              &UfbxImporterTest::imageNot2D,
              &UfbxImporterTest::imageBrokenExternal,
              &UfbxImporterTest::imageBrokenEmbedded,

              &UfbxImporterTest::objCube,
              &UfbxImporterTest::objCubeFileCallback});

    addInstancedTests({
        &UfbxImporterTest::objMissingMtl,
        &UfbxImporterTest::objMissingMtlFileCallback},
        Containers::arraySize(QuietData));

    addTests({&UfbxImporterTest::mtl,

              &UfbxImporterTest::normalizeUnitsAdjustTransforms,
              &UfbxImporterTest::normalizeUnitsTransformRoot,
              &UfbxImporterTest::normalizeUnitsInvalidHandling,

              &UfbxImporterTest::geometryCache,
              &UfbxImporterTest::geometryCacheFileCallback,

              &UfbxImporterTest::staticSkin});

    addInstancedTests({
        &UfbxImporterTest::multiWarning,
        &UfbxImporterTest::multiWarningData},
        Containers::arraySize(QuietData));

    addTests({&UfbxImporterTest::animationInterpolation});

    addInstancedTests({&UfbxImporterTest::animationRotationPivot},
        Containers::arraySize(ResampleRotationData));

    addTests({&UfbxImporterTest::animationVisibility,
              &UfbxImporterTest::animationLayersMerged,
              &UfbxImporterTest::animationLayersRetained,
              &UfbxImporterTest::animationLayersNonLinearWeight,
              &UfbxImporterTest::animationLayersNonLinearWeightNonResampled,
              &UfbxImporterTest::animationStacks,
              &UfbxImporterTest::animationLayerNames,
              &UfbxImporterTest::animationStackNames});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. */
    #ifdef UFBXIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(UFBXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif

    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    _manager.setPluginDirectory({});
    #endif
    #ifdef DDSIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(DDSIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void UfbxImporterTest::openFile() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-default.fbx")));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 3);
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->lightCount(), 1);
    CORRADE_COMPARE(importer->cameraCount(), 1);
    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->skin3DCount(), 0);

    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void UfbxImporterTest::openData() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    Containers::Optional<Containers::Array<char>> data = Utility::Path::read(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-default.fbx"));
    CORRADE_VERIFY(data);
    CORRADE_VERIFY(importer->openData(*data));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 3);
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->lightCount(), 1);
    CORRADE_COMPARE(importer->cameraCount(), 1);
    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->skin3DCount(), 0);

    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void UfbxImporterTest::openFileFailed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openFile("i-do-not-exist.foo"));
    CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::openFile(): loading failed: File not found: i-do-not-exist.foo\n");
}

void UfbxImporterTest::openDataFailed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    std::ostringstream out;
    Error redirectError{&out};

    constexpr const char data[] = "what";
    CORRADE_VERIFY(!importer->openData({data, sizeof(data)}));
    CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::openData(): loading failed: Unrecognized file format\n");
}

void UfbxImporterTest::maxTemporaryMemory() {
    auto&& data = MaxMemoryData[testCaseInstanceId()];
    setTestCaseDescription(Utility::format("{}", data.maxMemory));

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    importer->configuration().setValue<Long>("maxTemporaryMemory", data.maxMemory);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-default.fbx")), data.shouldLoad);
    if(data.shouldLoad) {
        CORRADE_COMPARE(out.str(), "");
    } else {
        CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::openFile(): loading failed: Memory limit exceeded: temp\n");
    }
}

void UfbxImporterTest::maxResultMemory() {
    auto&& data = MaxMemoryData[testCaseInstanceId()];
    setTestCaseDescription(Utility::format("{}", data.maxMemory));

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    importer->configuration().setValue<Long>("maxResultMemory", data.maxMemory);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-default.fbx")), data.shouldLoad);
    if(data.shouldLoad) {
        CORRADE_COMPARE(out.str(), "");
    } else {
        CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::openFile(): loading failed: Memory limit exceeded: result\n");
    }
}

void UfbxImporterTest::fileCallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    Containers::Optional<Containers::Array<char>> fbx = Utility::Path::read(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-default.fbx"));
    CORRADE_VERIFY(fbx);

    std::unordered_map<std::string, Containers::Array<char>> files;
    files["not/a/path/blender-default.fbx"] = *std::move(fbx);
    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy,
        std::unordered_map<std::string, Containers::Array<char>>& files) {
            return Containers::optional(Containers::ArrayView<const char>(files.at(filename)));
        }, files);

    CORRADE_VERIFY(importer->openFile("not/a/path/blender-default.fbx"));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 3);
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->lightCount(), 1);
    CORRADE_COMPARE(importer->cameraCount(), 1);
    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->skin3DCount(), 0);

    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void UfbxImporterTest::fileCallbackNotFound() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy, void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{};
        });

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile("some-file.fbx"));
    CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::openFile(): loading failed: File not found: some-file.fbx\n");
}

void UfbxImporterTest::fileCallbackEmpty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy, void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{InPlaceInit};
        });

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile("some-file.fbx"));
    CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::openFile(): loading failed: Failed to load\n");
}

void UfbxImporterTest::fileCallbackEmptyVerbose() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFlags(ImporterFlag::Verbose);
    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy, void*) {
            return Containers::Optional<Containers::ArrayView<const char>>{InPlaceInit};
        });

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile("some-file.fbx"));
    std::vector<std::string> lines = Utility::String::splitWithoutEmptyParts(out.str(), '\n');

    /* Output should contain a stack trace on debug */
    #if !defined(CORRADE_IS_DEBUG_BUILD) && defined(NDEBUG)
    CORRADE_COMPARE(lines.size(), 1);
    #else
    CORRADE_COMPARE_AS(lines.size(), 1, TestSuite::Compare::Greater);
    #endif
}

void UfbxImporterTest::scene() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-default.fbx")));

    const SceneField sceneFieldVisibility = importer->sceneFieldForName("Visibility"_s);
    CORRADE_VERIFY(isSceneFieldCustom(sceneFieldVisibility));
    CORRADE_COMPARE(importer->sceneFieldName(sceneFieldVisibility), "Visibility");

    const SceneField sceneFieldInvalid = importer->sceneFieldForName("ThisFieldDoesNotExist"_s);
    CORRADE_COMPARE(sceneFieldInvalid, SceneField{});
    CORRADE_COMPARE(importer->sceneFieldName(sceneFieldCustom(9001)), "");

    CORRADE_COMPARE(importer->defaultScene(), 0);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->fieldCount(), 10);

    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::Translation));
    CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
    CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
    CORRADE_VERIFY(scene->hasField(sceneFieldVisibility));
    CORRADE_VERIFY(scene->hasField(SceneField::Mesh));
    CORRADE_VERIFY(scene->hasField(SceneField::MeshMaterial));
    CORRADE_VERIFY(scene->hasField(SceneField::Light));
    CORRADE_VERIFY(scene->hasField(SceneField::Camera));

    {
        Long optionalId = importer->objectForName("Cube");
        CORRADE_COMPARE_AS(optionalId, 0, TestSuite::Compare::GreaterOrEqual);
        UnsignedLong id = UnsignedLong(optionalId);
        CORRADE_COMPARE(importer->objectName(id), "Cube");

        auto meshMaterials = scene->meshesMaterialsFor(id);
        CORRADE_COMPARE(meshMaterials.size(), 1);

        auto lights = scene->lightsFor(id);
        CORRADE_COMPARE(lights.size(), 0);

        auto cameras = scene->camerasFor(id);
        CORRADE_COMPARE(cameras.size(), 0);
    } {
        Long optionalId = importer->objectForName("Light");
        CORRADE_COMPARE_AS(optionalId, 0, TestSuite::Compare::GreaterOrEqual);
        UnsignedLong id = UnsignedLong(optionalId);
        CORRADE_COMPARE(importer->objectName(id), "Light");

        auto meshMaterials = scene->meshesMaterialsFor(id);
        CORRADE_COMPARE(meshMaterials.size(), 0);

        auto lights = scene->lightsFor(id);
        CORRADE_COMPARE(lights.size(), 1);

        auto cameras = scene->camerasFor(id);
        CORRADE_COMPARE(cameras.size(), 0);
    } {
        Long optionalId = importer->objectForName("Camera");
        CORRADE_COMPARE_AS(optionalId, 0, TestSuite::Compare::GreaterOrEqual);
        UnsignedLong id = UnsignedLong(optionalId);
        CORRADE_COMPARE(importer->objectName(id), "Camera");

        auto meshMaterials = scene->meshesMaterialsFor(id);
        CORRADE_COMPARE(meshMaterials.size(), 0);

        auto lights = scene->lightsFor(id);
        CORRADE_COMPARE(lights.size(), 0);

        auto cameras = scene->camerasFor(id);
        CORRADE_COMPARE(cameras.size(), 1);
    } {
        Long optionalId = importer->objectForName("ThisObjectDoesNotExist");
        CORRADE_COMPARE_AS(optionalId, 0, TestSuite::Compare::Less);
    }
}

void UfbxImporterTest::mesh() {
    auto&& data = MeshGenerateIndicesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    if(data.generateIndices)
        importer->configuration().setValue("generateIndices", *data.generateIndices);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "triangle.fbx")));

    CORRADE_COMPARE(importer->meshCount(), 1);

    /* FBX files don't have reliable mesh names, so go by index */
    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);

    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_COMPARE(mesh->isIndexed(), data.expectedIndices);
    if(data.expectedIndices) {
        CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
            Containers::arrayView<UnsignedInt>({0, 1, 2}),
            TestSuite::Compare::Container);
    }

    /* There isn't anything to deduplicate in the file, so the vertex data are
       the same in both cases */
    /** @todo maybe it might make sense to actually verify that duplicates got
        removed? */
    CORRADE_COMPARE(mesh->attributeCount(), 6);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f}
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
            {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f},
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Color), 1);
    CORRADE_COMPARE_AS(mesh->attribute<Vector4>(MeshAttribute::Color),
        Containers::arrayView<Vector4>({
            {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE(importer->sceneCount(), 1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->fieldCount(), 8);

    const SceneField sceneFieldVisibility = importer->sceneFieldForName("Visibility"_s);

    /* Fields we're not interested in */
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::Translation));
    CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
    CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
    CORRADE_VERIFY(scene->hasField(sceneFieldVisibility));

    CORRADE_VERIFY(scene->hasField(SceneField::Mesh));
    CORRADE_VERIFY(scene->hasField(SceneField::MeshMaterial));
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0,
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0,
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::MeshMaterial), Containers::arrayView<Int>({
        0,
    }), TestSuite::Compare::Container);
}

void UfbxImporterTest::meshPointLine() {
    auto&& data = MeshGenerateIndicesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    if(data.generateIndices)
        importer->configuration().setValue("generateIndices", *data.generateIndices);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "mesh-point-line.fbx")));

    CORRADE_COMPARE(importer->meshCount(), 3);

    {
        Containers::Optional<MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);

        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Points);

        CORRADE_COMPARE(mesh->isIndexed(), data.expectedIndices);
        if(data.expectedIndices) CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
            Containers::arrayView<UnsignedInt>({0}),
            TestSuite::Compare::Container);

        /* There isn't anything to deduplicate in the file, so the vertex data
           are the same in both cases */
        /** @todo maybe it might make sense to actually verify that duplicates
            got removed? */
        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
            Containers::arrayView<Vector3>({
                {-0.5f, 0.5f, 0.5f},
            }), TestSuite::Compare::Container);
    } {
        Containers::Optional<MeshData> mesh = importer->mesh(1);
        CORRADE_VERIFY(mesh);

        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Lines);

        CORRADE_COMPARE(mesh->isIndexed(), data.expectedIndices);
        if(data.expectedIndices) CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
            Containers::arrayView<UnsignedInt>({0, 1}),
            TestSuite::Compare::Container);

        /* There isn't anything to deduplicate in the file, so the vertex data
           are the same in both cases */
        /** @todo maybe it might make sense to actually verify that duplicates
            got removed? */
        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
            Containers::arrayView<Vector3>({
                {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5, -0.5f},
            }), TestSuite::Compare::Container);
    } {
        Containers::Optional<MeshData> mesh = importer->mesh(2);
        CORRADE_VERIFY(mesh);

        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

        CORRADE_COMPARE(mesh->isIndexed(), data.expectedIndices);
        if(data.expectedIndices)
            CORRADE_COMPARE(mesh->indices<UnsignedInt>().size(), 21);
    }

    CORRADE_COMPARE(importer->sceneCount(), 1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->fieldCount(), 8);

    const SceneField sceneFieldVisibility = importer->sceneFieldForName("Visibility"_s);
    const SceneField sceneFieldGeometryTransformHelper = importer->sceneFieldForName("GeometryTransformHelper"_s);

    /* Fields we're not interested in */
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::Translation));
    CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
    CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
    CORRADE_VERIFY(scene->hasField(sceneFieldVisibility));
    CORRADE_VERIFY(scene->hasField(sceneFieldGeometryTransformHelper));

    CORRADE_VERIFY(scene->hasField(SceneField::Mesh));
    CORRADE_VERIFY(scene->hasField(SceneField::MeshMaterial));
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0,0,0,
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0,1,2,
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::MeshMaterial), Containers::arrayView<Int>({
        0,0,0,
    }), TestSuite::Compare::Container);
}

void UfbxImporterTest::camera() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "cameras.fbx")));

    CORRADE_COMPARE(importer->objectCount(), 3);
    CORRADE_COMPARE(importer->cameraCount(), 3);

    {
        Containers::Optional<CameraData> camera = importer->camera("cameraPerspective");
        CORRADE_VERIFY(camera);

        CORRADE_COMPARE(camera->type(), CameraType::Perspective3D);
        CORRADE_COMPARE(camera->near(), 0.123f);
        CORRADE_COMPARE(camera->far(), 123.0f);
        CORRADE_COMPARE(camera->aspectRatio(), 16.0f/9.0f);
    } {
        Containers::Optional<CameraData> camera = importer->camera("cameraOrthographicFill");
        CORRADE_VERIFY(camera);

        CORRADE_COMPARE(camera->type(), CameraType::Orthographic3D);
        CORRADE_COMPARE(camera->near(), 0.456f);
        CORRADE_COMPARE(camera->far(), 456.0f);
        CORRADE_COMPARE(camera->size(), (Vector2{10.0f, 10.0f / (16.0f/9.0f)}));
    } {
        Containers::Optional<CameraData> camera = importer->camera("cameraOrthographicOverscan");
        CORRADE_VERIFY(camera);

        CORRADE_COMPARE(camera->type(), CameraType::Orthographic3D);
        CORRADE_COMPARE(camera->near(), 0.456f);
        CORRADE_COMPARE(camera->far(), 456.0f);
        CORRADE_COMPARE(camera->size(), (Vector2{10.0f * (16.0f/9.0f), 10.0f}));
    }
}

void UfbxImporterTest::cameraName() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-default.fbx")));

    CORRADE_COMPARE(importer->cameraCount(), 1);

    Int cameraId = importer->cameraForName("Camera");
    CORRADE_COMPARE(cameraId, 0);

    Int noncameraId = importer->cameraForName("None");
    CORRADE_COMPARE(noncameraId, -1);

    CORRADE_COMPARE(importer->cameraName(0), "Camera");
}

void UfbxImporterTest::cameraOrientation() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "maya-camera-light-orientation.fbx")));

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    Long objectId = importer->objectForName("forwardCamera");
    CORRADE_COMPARE_AS(objectId, 0, TestSuite::Compare::GreaterOrEqual);

    /* camera pointing towards -Z in the file, should not be rotated */
    Containers::Optional<Containers::Triple<Vector3, Quaternion, Vector3>> trs = scene->translationRotationScaling3DFor(objectId);
    CORRADE_VERIFY(trs);
    CORRADE_COMPARE(trs->first(), (Vector3{0.0f, 0.0f, 0.0f}));
    CORRADE_COMPARE(trs->second(), (Quaternion{{0.0f, 0.0f, 0.0f}, 1.0f}));
    CORRADE_COMPARE(trs->third(), (Vector3{1.0f}));
}

void UfbxImporterTest::light() {
    std::ostringstream out;
    Error redirectError{&out};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "lights.fbx")));

    CORRADE_COMPARE(importer->objectCount(), 6);
    CORRADE_COMPARE(importer->lightCount(), 6);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    const SceneField sceneFieldVisibility = importer->sceneFieldForName("Visibility"_s);
    CORRADE_VERIFY(scene->hasField(sceneFieldVisibility));

    Containers::StridedArrayView1D<const Vector3d> translations = scene->field<Vector3d>(SceneField::Translation);
    CORRADE_VERIFY(scene->fieldFlags(SceneField::Translation) & SceneFieldFlag::ImplicitMapping);

    Containers::StridedBitArrayView1D visibilities = scene->fieldBits(sceneFieldVisibility);
    CORRADE_VERIFY(scene->fieldFlags(sceneFieldVisibility) & SceneFieldFlag::ImplicitMapping);

    /* FBX files often don't have names for "attributes" such as lights so
       we need to go through objects. In theory we could propagate names from
       the objects to attributes but this would break with instancing. */

    {
        /* The plug-in does not support the following light types:
           Ambient light(|ambientLight1)  will be exported as an inactive
           point light with the Visibility value set to off. */
        Long objectId = importer->objectForName("ambientLight1");
        CORRADE_COMPARE(importer->objectName(objectId), "ambientLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{0.0, 0.0, 0.0}));
        CORRADE_COMPARE(visibilities[objectId], false);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{0.11f, 0.12f, 0.13f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f, 0.0f, 0.0f}));
    } {
        Long objectId = importer->objectForName("directionalLight1");
        CORRADE_COMPARE(importer->objectName(objectId), "directionalLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{1.0, 0.0, 0.0}));
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Directional);
        CORRADE_COMPARE(light->color(), (Color3{0.21f, 0.22f, 0.23f}));
        CORRADE_COMPARE(light->intensity(), 2.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f, 0.0f, 0.0f}));
    } {
        Long objectId = importer->objectForName("pointLight1");
        CORRADE_COMPARE(importer->objectName(objectId), "pointLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{2.0, 0.0, 0.0}));
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{0.31f, 0.32f, 0.33f}));
        CORRADE_COMPARE(light->intensity(), 3.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{0.0f, 1.0f, 0.0f}));
    } {
        Long objectId = importer->objectForName("spotLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{3.0f,0.0f,0.0f}));
        CORRADE_COMPARE(importer->objectName(objectId), "spotLight1");
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Spot);
        CORRADE_COMPARE(light->color(), (Color3{0.41f, 0.42f, 0.43f}));
        CORRADE_COMPARE(light->intensity(), 4.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{0.0f,0.0f,1.0f}));
        CORRADE_COMPARE(light->innerConeAngle(), 12.0_degf);
        CORRADE_COMPARE(light->outerConeAngle(), 80.0_degf);
    } {
        Long objectId = importer->objectForName("areaLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{4.0, 0.0, 0.0}));
        CORRADE_COMPARE(importer->objectName(objectId), "areaLight1");
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        /* Area lights are not supported */
        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(!light);
    } {
        Long objectId = importer->objectForName("volumeLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{5.0, 0.0, 0.0}));
        CORRADE_COMPARE(importer->objectName(objectId), "volumeLight1");
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        /* Volume lights are not supported */
        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(!light);
    }

    CORRADE_COMPARE(out.str(),
        "Trade::UfbxImporter::light(): light type 3 is not supported\n"
        "Trade::UfbxImporter::light(): light type 4 is not supported\n");
}

void UfbxImporterTest::lightName() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-default.fbx")));

    CORRADE_COMPARE(importer->lightCount(), 1);

    Int lightId = importer->lightForName("Light");
    CORRADE_COMPARE(lightId, 0);

    Int nonLightId = importer->lightForName("None");
    CORRADE_COMPARE(nonLightId, -1);

    CORRADE_COMPARE(importer->lightName(0), "Light");
}

void UfbxImporterTest::lightBadDecay() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->addFlags(data.flags);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "bad-decay-lights.fbx")));

    CORRADE_COMPARE(importer->lightCount(), 2);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    {
        Long objectId = importer->objectForName("cubic");
        CORRADE_COMPARE_AS(objectId, 0, TestSuite::Compare::GreaterOrEqual);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(UnsignedLong(objectId));
        CORRADE_COMPARE(lights.size(), 1);

        std::ostringstream out;
        Containers::Optional<LightData> light;
        {
            Warning redirectWarning{&out};
            light = importer->light(lights[0]);
        }
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->attenuation(), (Vector3{0.0f, 0.0f, 1.0f}));
        if(data.quiet)
            CORRADE_COMPARE(out.str(), "");
        else
            CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::light(): cubic attenuation not supported, patching to quadratic\n");
    } {
        Long objectId = importer->objectForName("directionalDecay");
        CORRADE_COMPARE_AS(objectId, 0, TestSuite::Compare::GreaterOrEqual);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(UnsignedLong(objectId));
        CORRADE_COMPARE(lights.size(), 1);

        std::ostringstream out;
        Containers::Optional<LightData> light;
        {
            Warning redirectWarning{&out};
            light = importer->light(lights[0]);
        }
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Directional);
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f, 0.0f, 0.0f}));
        if(data.quiet)
            CORRADE_COMPARE(out.str(), "");
        else
            CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::light(): patching attenuation Vector(0, 0, 1) to Vector(1, 0, 0) for Trade::LightData::Type::Directional\n");
    }
}

void UfbxImporterTest::lightOrientation() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "maya-camera-light-orientation.fbx")));

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    Long objectId = importer->objectForName("forwardLight");
    CORRADE_COMPARE_AS(objectId, 0, TestSuite::Compare::GreaterOrEqual);

    /* light pointing towards -Z in the file, should not be rotated */
    Containers::Optional<Containers::Triple<Vector3, Quaternion, Vector3>> trs = scene->translationRotationScaling3DFor(objectId);
    CORRADE_VERIFY(trs);
    CORRADE_COMPARE(trs->first(), (Vector3{0.0f, 0.0f, 0.0f}));
    CORRADE_COMPARE(trs->second(), (Quaternion{{0.0f, 0.0f, 0.0f}, 1.0f}));
    CORRADE_COMPARE(trs->third(), (Vector3{1.0f}));
}

void UfbxImporterTest::geometricTransformHelperNodes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "geometric-transform.fbx")));

    /* Two real objects and two geometric transform helpers */
    CORRADE_COMPARE(importer->objectCount(), 4);
    CORRADE_COMPARE(importer->objectName(0), "Box001");
    CORRADE_COMPARE(importer->objectName(1), "");
    CORRADE_COMPARE(importer->objectName(2), "Box002");
    CORRADE_COMPARE(importer->objectName(3), "");

    const SceneField sceneFieldGeometryTransformHelper = importer->sceneFieldForName("GeometryTransformHelper"_s);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::Translation));
    CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
    CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
    CORRADE_VERIFY(scene->hasField(sceneFieldGeometryTransformHelper));

    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
        -1, 0, 0, 2,
    }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(scene->fieldBits(sceneFieldGeometryTransformHelper), Containers::stridedArrayView({
        false, true, false, true
    }).sliceBit(0), TestSuite::Compare::Container);

    /* The meshes should be parented under their geometric transform helpers */
    CORRADE_COMPARE_AS(scene->meshesMaterialsAsArray(), (Containers::arrayView<Containers::Pair<UnsignedInt, Containers::Pair<UnsignedInt, Int>>>({
        {1, {0, -1}},
        {3, {1, -1}},
    })), TestSuite::Compare::Container);

    const Containers::Pair<UnsignedInt, Containers::Triple<Vector3,Quaternion,Vector3>> referenceTrs[]{
        /* Box001 */
        {0, {{0.0f, -10.0f, 0.0f},
             {{0.0f, 0.0f, -0.707107f}, 0.707107f}, /* Euler (0, 0, -90) */
             {1.0f, 2.0f, 1.0f}}},
        /* Box001 Geometric */
        {1, {{0.0f, 0.0f, 10.0f},
             {{0.0f, 0.707107f, 0.0f}, 0.707107f}, /* Euler (0, 90, 0) */
             {1.0f, 1.0f, 2.0f}}},
        /* Box002 */
        {2, {{0.0f, 0.0f, 20.0f},
             {{0.0f, 0.0f, -1.0f}, 0.0f}, /* Euler (0, 0, -180) */
             {1.0f, 0.5f, 1.0f}}},
        /* Box002 Geometric */
        {3, {{10.0f, 0.0f, 0.0f},
             {},
             {1.0f, 1.0f, 1.0f}}},
    };
    CORRADE_COMPARE_AS(scene->translationsRotationsScalings3DAsArray(),
        Containers::arrayView(referenceTrs), TestSuite::Compare::Container);
}

void UfbxImporterTest::geometricTransformModifyGeometry() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("geometryTransformHandling", "modifyGeometry");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "geometric-transform.fbx")));

    /* Two real objects only */
    CORRADE_COMPARE(importer->objectCount(), 2);
    CORRADE_COMPARE(importer->objectName(0), "Box001");
    CORRADE_COMPARE(importer->objectName(1), "Box002");

    const SceneField sceneFieldGeometryTransformHelper = importer->sceneFieldForName("GeometryTransformHelper"_s);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::Translation));
    CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
    CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
    CORRADE_VERIFY(scene->hasField(sceneFieldGeometryTransformHelper));

    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
        -1, 0,
    }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(scene->fieldBits(sceneFieldGeometryTransformHelper), Containers::stridedArrayView({
        false, false
    }).sliceBit(0), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(scene->meshesMaterialsAsArray(), (Containers::arrayView<Containers::Pair<UnsignedInt, Containers::Pair<UnsignedInt, Int>>>({
        {0, {0, -1}},
        {1, {1, -1}},
    })), TestSuite::Compare::Container);

    const Containers::Pair<UnsignedInt, Containers::Triple<Vector3,Quaternion,Vector3>> referenceTrs[]{
        /* Box001 */
        {0, {{0.0f, -10.0f, 0.0f},
             {{0.0f, 0.0f, -0.707107f}, 0.707107f}, /* Euler (0, 0, -90) */
             {1.0f, 2.0f, 1.0f}}},
        /* Box002 */
        {1, {{0.0f, 0.0f, 20.0f},
             {{0.0f, 0.0f, -1.0f}, 0.0f}, /* Euler (0, 0, -180) */
             {1.0f, 0.5f, 1.0f}}},
    };
    CORRADE_COMPARE_AS(scene->translationsRotationsScalings3DAsArray(),
        Containers::arrayView(referenceTrs), TestSuite::Compare::Container);
}

void UfbxImporterTest::geometricTransformPreserve() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("geometryTransformHandling", "preserve");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "geometric-transform.fbx")));

    /* Two real objects only */
    CORRADE_COMPARE(importer->objectCount(), 2);
    CORRADE_COMPARE(importer->objectName(0), "Box001");
    CORRADE_COMPARE(importer->objectName(1), "Box002");

    const SceneField sceneFieldGeometryTransformHelper = importer->sceneFieldForName("GeometryTransformHelper"_s);
    const SceneField sceneFieldGeometryTranslation = importer->sceneFieldForName("GeometryTranslation"_s);
    const SceneField sceneFieldGeometryRotation = importer->sceneFieldForName("GeometryRotation"_s);
    const SceneField sceneFieldGeometryScaling = importer->sceneFieldForName("GeometryScaling"_s);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::Translation));
    CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
    CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
    CORRADE_VERIFY(scene->hasField(sceneFieldGeometryTransformHelper));
    CORRADE_VERIFY(scene->hasField(sceneFieldGeometryTranslation));
    CORRADE_VERIFY(scene->hasField(sceneFieldGeometryRotation));
    CORRADE_VERIFY(scene->hasField(sceneFieldGeometryScaling));

    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
        -1, 0,
    }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(scene->fieldBits(sceneFieldGeometryTransformHelper), Containers::stridedArrayView({
        false, false
    }).sliceBit(0), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(scene->meshesMaterialsAsArray(), (Containers::arrayView<Containers::Pair<UnsignedInt, Containers::Pair<UnsignedInt, Int>>>({
        {0, {0, -1}},
        {1, {1, -1}},
    })), TestSuite::Compare::Container);

    const Containers::Pair<UnsignedInt, Containers::Triple<Vector3,Quaternion,Vector3>> referenceTrs[]{
        /* Box001 */
        {0, {{0.0f, -10.0f, 0.0f},
             {{0.0f, 0.0f, -0.707107f}, 0.707107f}, /* Euler (0, 0, -90) */
             {1.0f, 2.0f, 1.0f}}},
        /* Box002 */
        {1, {{0.0f, 0.0f, 20.0f},
             {{0.0f, 0.0f, -1.0f}, 0.0f}, /* Euler (0, 0, -180) */
             {1.0f, 0.5f, 1.0f}}},
    };
    CORRADE_COMPARE_AS(scene->translationsRotationsScalings3DAsArray(),
        Containers::arrayView(referenceTrs), TestSuite::Compare::Container);

    Containers::StridedArrayView1D<const Vector3d> geometryTranslations = scene->field<Vector3d>(sceneFieldGeometryTranslation);
    Containers::StridedArrayView1D<const Quaterniond> geometryRotations = scene->field<Quaterniond>(sceneFieldGeometryRotation);
    Containers::StridedArrayView1D<const Vector3d> geometryScalings = scene->field<Vector3d>(sceneFieldGeometryScaling);

    /* Compare geometry transforms in 32-bit as the file is noisy */
    Containers::Array<Vector3> geometryTranslationsFloat{geometryTranslations.size()};
    Containers::Array<Quaternion> geometryRotationsFloat{geometryRotations.size()};
    Containers::Array<Vector3> geometryScalingsFloat{geometryScalings.size()};
    Math::castInto(Containers::arrayCast<2, const Double>(geometryTranslations), Containers::arrayCast<2, Float>(Containers::arrayView(geometryTranslationsFloat)));
    Math::castInto(Containers::arrayCast<2, const Double>(geometryRotations), Containers::arrayCast<2, Float>(Containers::arrayView(geometryRotationsFloat)));
    Math::castInto(Containers::arrayCast<2, const Double>(geometryScalings), Containers::arrayCast<2, Float>(Containers::arrayView(geometryScalingsFloat)));

    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(sceneFieldGeometryTranslation), Containers::arrayView<UnsignedInt>(
        {0, 1}), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(sceneFieldGeometryRotation), Containers::arrayView<UnsignedInt>(
        {0, 1}), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(sceneFieldGeometryScaling), Containers::arrayView<UnsignedInt>(
        {0, 1}), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(geometryTranslationsFloat, Containers::arrayView<Vector3>({
        {0.0f, 0.0f, 10.0f}, /* Box001 */
        {10.0f, 0.0f, 0.0f}, /* Box002 */
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(geometryRotationsFloat, Containers::arrayView<Quaternion>({
         {{0.0f, 0.707107f, 0.0f}, 0.707107f}, /* Box001: Euler (0, 90, 0) */
         {},                                   /* Box002: identity */
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(geometryScalingsFloat, Containers::arrayView<Vector3>({
         {1.0f, 1.0f, 2.0f},
         {1.0f, 1.0f, 1.0f},
    }), TestSuite::Compare::Container);
}

void UfbxImporterTest::geometricTransformInvalidHandling() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("geometryTransformHandling", "invalidGeometryHandling");

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "geometric-transform.fbx")));
    CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::openFile(): Unsupported geometryTransformHandling configuration: invalidGeometryHandling\n");
}

void UfbxImporterTest::materialMapping() {
    /* Make sure all the ufbx maps are used at least once and that there are
       no duplicate Magnum attributes without MaterialExclusionGroup */

    Containers::ArrayView<const MaterialMapping> mappingLists[]{
        Containers::arrayView(materialMappingFbx),
        Containers::arrayView(materialMappingPbr),
        Containers::arrayView(materialMappingFbxFactor),
        Containers::arrayView(materialMappingPbrFactor),
    };

    Containers::StaticArray<UfbxMaterialLayerCount, std::unordered_map<std::string, MaterialExclusionGroup>> usedAttributeNames;

    Containers::BitArray usedUfbxMaps[]{
        Containers::BitArray{ValueInit, UFBX_MATERIAL_FBX_MAP_COUNT},
        Containers::BitArray{ValueInit, UFBX_MATERIAL_PBR_MAP_COUNT},
        Containers::BitArray{ValueInit, UFBX_MATERIAL_FBX_MAP_COUNT},
        Containers::BitArray{ValueInit, UFBX_MATERIAL_PBR_MAP_COUNT},
    };

    Containers::BitArray usedFactorMaps[]{
        Containers::BitArray{ValueInit, UFBX_MATERIAL_FBX_MAP_COUNT},
        Containers::BitArray{ValueInit, UFBX_MATERIAL_PBR_MAP_COUNT},
    };

    for(UnsignedInt type = 0; type < 4; ++type) {
        for(const MaterialMapping& mapping: mappingLists[type]) {
            std::size_t layer = std::size_t(mapping.layer);

            if(mapping.valueMap >= 0)
                usedUfbxMaps[type].set(std::size_t(mapping.valueMap));
            if(mapping.factorMap >= 0) {
                CORRADE_COMPARE_AS(type, 2, TestSuite::Compare::Less);
                usedUfbxMaps[type].set(std::size_t(mapping.factorMap));
                usedFactorMaps[type].set(std::size_t(mapping.factorMap));
            }

            /* Copy to std::string so we don't do unnecessary conversions on
               lookups, also this is far from performance critical */
            std::string attribute = mapping.attribute;
            std::string textureAttribute = mapping.textureAttribute;

            if(!attribute.empty()) {
                CORRADE_ITERATION(attribute);

                CORRADE_COMPARE_AS(mapping.attributeType, MaterialAttributeType{}, TestSuite::Compare::NotEqual);

                switch(mapping.attributeType) {
                    case MaterialAttributeType::Float:
                    case MaterialAttributeType::Vector3:
                    case MaterialAttributeType::Vector4:
                    case MaterialAttributeType::Long:
                    case MaterialAttributeType::Bool:
                        break;
                    default:
                        CORRADE_FAIL("Unimplemented MaterialAttributeType: " << mapping.attributeType);
                        break;
                }

                auto found = usedAttributeNames[layer].find(attribute);
                if(found != usedAttributeNames[layer].end()) {
                    /* If we have a duplicate material attribute name it must
                       be defined under the same exclusion group */
                    CORRADE_VERIFY(mapping.exclusionGroup != MaterialExclusionGroup{});
                    CORRADE_COMPARE(UnsignedInt(mapping.exclusionGroup), UnsignedInt(found->second));
                } else {
                    usedAttributeNames[layer].emplace(attribute, mapping.exclusionGroup);
                }

                if(textureAttribute.empty())
                    textureAttribute = attribute + "Texture";
            }

            if(!textureAttribute.empty() && textureAttribute != MaterialMapping::DisallowTexture) {
                CORRADE_ITERATION(textureAttribute);

                auto found = usedAttributeNames[layer].find(textureAttribute);
                if(found != usedAttributeNames[layer].end()) {
                    /* If we have a duplicate material attribute name it must
                       be defined under the same exclusion group */
                    CORRADE_VERIFY(mapping.exclusionGroup != MaterialExclusionGroup{});
                    CORRADE_COMPARE(UnsignedInt(mapping.exclusionGroup), UnsignedInt(found->second));
                } else {
                    usedAttributeNames[layer].emplace(textureAttribute, mapping.exclusionGroup);
                }
            }
        }
    }

    /* Make sure all the ufbx maps are accounted for */
    for(UnsignedInt i = 0; i < UFBX_MATERIAL_FBX_MAP_COUNT; ++i) {
        CORRADE_ITERATION(i);
        CORRADE_VERIFY(usedUfbxMaps[0][i]);
    }
    for(UnsignedInt i = 0; i < UFBX_MATERIAL_PBR_MAP_COUNT; ++i) {
        CORRADE_ITERATION(i);
        CORRADE_VERIFY(usedUfbxMaps[1][i]);
    }

    /* Make sure each factor property has a matching factor */
    for(UnsignedInt i = 0; i < UFBX_MATERIAL_FBX_MAP_COUNT; ++i) {
        CORRADE_ITERATION(i);
        if(usedFactorMaps[0][i]) {
            CORRADE_VERIFY(usedUfbxMaps[2][i]);
        }
    }
    for(UnsignedInt i = 0; i < UFBX_MATERIAL_PBR_MAP_COUNT; ++i) {
        CORRADE_ITERATION(i);
        if(usedFactorMaps[1][i]) {
            CORRADE_VERIFY(usedUfbxMaps[3][i]);
        }
    }
}

void UfbxImporterTest::blenderMaterials() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-materials.fbx")));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->materialCount(), 3);

    {
        Containers::Optional<MaterialData> material = importer->material("Default");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::PbrMetallicRoughness);

        {
            const PhongMaterialData& phong = material->as<PhongMaterialData>();
            CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.8f, 0.8f, 0.8f, 1.0f}));
            CORRADE_COMPARE(phong.shininess(), 25.0f);
        } {
            const PbrMetallicRoughnessMaterialData& pbr = material->as<PbrMetallicRoughnessMaterialData>();
            CORRADE_COMPARE(pbr.baseColor(), (Color4{0.8f, 0.8f, 0.8f, 1.0f}));
            CORRADE_COMPARE(pbr.emissiveColor(), (Color3{0.0f, 0.0f, 0.0f}));
            CORRADE_COMPARE(pbr.metalness(), 0.0f);
            CORRADE_COMPARE(pbr.roughness(), 0.5f);
        }
    } {
        Containers::Optional<MaterialData> material = importer->material("Top");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::PbrMetallicRoughness);

        {
            const PhongMaterialData& phong = material->as<PhongMaterialData>();
            CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.1f, 0.2f, 0.3f, 1.0f}));
            CORRADE_COMPARE(phong.shininess(), 4.0f);
        } {
            const PbrMetallicRoughnessMaterialData& pbr = material->as<PbrMetallicRoughnessMaterialData>();
            CORRADE_COMPARE(pbr.baseColor(), (Color4{0.1f, 0.2f, 0.3f, 0.9f}));
            CORRADE_COMPARE(pbr.emissiveColor(), (Color3{0.4f, 0.5f, 0.6f}));
            CORRADE_COMPARE(pbr.metalness(), 0.7f);
            CORRADE_COMPARE(pbr.roughness(), 0.8f);
        }
    } {
        Containers::Optional<MaterialData> material = importer->material("Textures");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::PbrMetallicRoughness);

        {
            const PhongMaterialData& phong = material->as<PhongMaterialData>();

            {
                CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
                Containers::Optional<TextureData> texture = importer->texture(phong.diffuseTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_diffuse.png");
            }
        } {
            const PbrMetallicRoughnessMaterialData& pbr = material->as<PbrMetallicRoughnessMaterialData>();

            {
                CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::BaseColorTexture));
                Containers::Optional<TextureData> texture = importer->texture(pbr.baseColorTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_diffuse.png");
            } {
                CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::EmissiveTexture));
                Containers::Optional<TextureData> texture = importer->texture(pbr.emissiveTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_emissive.png");
            } {
                CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::RoughnessTexture));
                Containers::Optional<TextureData> texture = importer->texture(pbr.roughnessTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_roughness.png");
            } {
                CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::MetalnessTexture));
                Containers::Optional<TextureData> texture = importer->texture(pbr.metalnessTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_metallic.png");
            }
        } {
            CORRADE_VERIFY(material->hasAttribute("opacityTexture"));
            UnsignedInt textureId = material->attribute<UnsignedInt>("opacityTexture");
            Containers::Optional<TextureData> texture = importer->texture(textureId);
            CORRADE_VERIFY(texture);
            CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_transparency.png");
        }
    }
}

void UfbxImporterTest::materialMayaPhong() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "maya-material-phong.fbx")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<MaterialData> material = importer->material("phong1");
    CORRADE_VERIFY(material);

    MaterialData reference{MaterialType::Phong, {
        {MaterialAttribute::DiffuseColor, Color4{0.01f, 0.02f, 0.03f, 1.0f} * Color4{0.13f, 1.0f}},
        {"transparencyColor", Color4{0.04f, 0.05f, 0.06f, 1.0f}},
        {MaterialAttribute::AmbientColor, Color4{0.07f, 0.08f, 0.09f, 1.0f}},
        {MaterialAttribute::EmissiveColor, Color3{0.10f, 0.11f, 0.12f}},
        {MaterialAttribute::Shininess, 17.0f},
        {MaterialAttribute::SpecularColor, Color4{0.18f, 0.19f, 0.20f, 1.0f}},
        {"reflectionColor", Color4{0.22f, 0.23f, 0.24f, 1.0f} * Color4{0.21f, 1.0f}},
        {"bumpFactor", 1.0f},
        {"displacementFactor", 1.0f},
        {"vectorDisplacementFactor", 1.0f},
    }};

    CORRADE_COMPARE_AS(*material, reference, DebugTools::CompareMaterial);
}

void UfbxImporterTest::materialMayaPhongFactors() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("preserveMaterialFactors", true);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "maya-material-phong.fbx")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<MaterialData> material = importer->material("phong1");
    CORRADE_VERIFY(material);

    MaterialData reference{MaterialType::Phong, {
        {MaterialAttribute::DiffuseColor, Color4{0.01f, 0.02f, 0.03f, 1.0f}},
        {"diffuseColorFactor", 0.13f},
        {"transparencyColor", Color4{0.04f, 0.05f, 0.06f, 1.0f}},
        {MaterialAttribute::AmbientColor, Color4{0.07f, 0.08f, 0.09f, 1.0f}},
        {"ambientColorFactor", 1.0f},
        {MaterialAttribute::EmissiveColor, Color3{0.10f, 0.11f, 0.12f}},
        {"emissiveColorFactor", 1.0f},
        {MaterialAttribute::Shininess, 17.0f},
        {MaterialAttribute::SpecularColor, Color4{0.18f, 0.19f, 0.20f, 1.0f}},
        {"specularColorFactor", 1.0f},
        {"reflectionColor", Color4{0.22f, 0.23f, 0.24f, 1.0f}},
        {"reflectionColorFactor", 0.21f},
        {"transparencyColorFactor", 1.0f},
        {"bumpFactor", 1.0f},
        {"displacementFactor", 1.0f},
        {"vectorDisplacementFactor", 1.0f},
    }};

    CORRADE_COMPARE_AS(*material, reference, DebugTools::CompareMaterial);
}

void UfbxImporterTest::materialMayaArnold() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "maya-material-arnold.fbx")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<MaterialData> material = importer->material("aiStandardSurface1");
    CORRADE_VERIFY(material);

    MaterialData reference{MaterialType::PbrMetallicRoughness|MaterialType::PbrClearCoat, {
        {MaterialAttribute::BaseColor, Color4{0.02f, 0.03f, 0.04f, 1.0f} * Color4{0.01f, 1.0f}},
        {"diffuseRoughness", 0.05f},
        {MaterialAttribute::Metalness, 0.06f},
        {MaterialAttribute::SpecularColor, Color4{0.08f, 0.09f, 0.10f, 1.0f} * Color4{0.07f, 1.0f}},
        {MaterialAttribute::Roughness, 0.11f},
        {"specularIor", 0.12f},
        {"specularAnisotropy", 0.13f},
        {"specularRotation", 0.14f},
        {MaterialAttribute::EmissiveColor, Color3{0.52f, 0.5299f, 0.54f} * Vector3{0.51f}}, /* bad rounding in file */
        {"thinFilmThickness", 0.55f},
        {"thinFilmIor", 0.56f},
        {"opacity", Color3{0.57f, 0.58f, 0.5899f}}, /* bad rounding in file */
        {"indirectDiffuse", 0.63f},
        {"indirectSpecular", 0.64f},

        {MaterialAttribute::LayerName, "ClearCoat"},
        {MaterialAttribute::LayerFactor, 0.35f},
        {"color", Color4{0.36f, 0.37f, 0.38f, 1.0f}},
        {MaterialAttribute::Roughness, 0.39f},
        {"ior", 0.40f},
        {"anisotropy", 0.41f},
        {"rotation", 0.42f},

        {MaterialAttribute::LayerName, "transmission"},
        {MaterialAttribute::LayerFactor, 0.15f},
        {"color", Vector4{0.16f, 0.17f, 0.18f, 1.0f}},
        {"depth", 0.19f},
        {"scatter", Vector3{0.20f, 0.21f, 0.22f}},
        {"scatterAnisotropy", 0.23f},
        {"dispersion", 0.24f},
        {"extraRoughness", 0.25f},
        {"enableInAov", true},
        {"priority", Long(69)},

        {MaterialAttribute::LayerName, "subsurface"},
        {MaterialAttribute::LayerFactor, 0.26f},
        {"color", Color4{0.27f, 0.28f, 0.29f, 1.0f}},
        {"radius", Color3{0.30f, 0.31f, 0.32f}},
        {"scale", 0.33f},
        {"anisotropy", 0.34f},
        {"type", Long(1)},

        {MaterialAttribute::LayerName, "sheen"},
        {MaterialAttribute::LayerFactor, 0.46f},
        {"color", Color3{0.47f, 0.48f, 0.49f}},
        {MaterialAttribute::Roughness, 0.50f},

        {MaterialAttribute::LayerName, "matte"},
        {MaterialAttribute::LayerFactor, 0.68f},
        {"color", Color3{0.65f, 0.66f, 0.67f}},
    }, {14, 21, 31, 38, 42, 45}};

    CORRADE_COMPARE_AS(*material, reference, DebugTools::CompareMaterial);
}

void UfbxImporterTest::materialMayaArnoldFactors() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("preserveMaterialFactors", true);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "maya-material-arnold.fbx")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<MaterialData> material = importer->material("aiStandardSurface1");
    CORRADE_VERIFY(material);

    MaterialData reference{MaterialType::PbrMetallicRoughness|MaterialType::PbrClearCoat, {
        {MaterialAttribute::BaseColor, Color4{0.02f, 0.03f, 0.04f, 1.0f}},
        {"baseColorFactor", 0.01f},
        {"diffuseRoughness", 0.05f},
        {MaterialAttribute::Metalness, 0.06f},
        {MaterialAttribute::SpecularColor, Color4{0.08f, 0.09f, 0.10f, 1.0f}},
        {"specularColorFactor", 0.07f},
        {MaterialAttribute::Roughness, 0.11f},
        {"specularIor", 0.12f},
        {"specularAnisotropy", 0.13f},
        {"specularRotation", 0.14f},
        {MaterialAttribute::EmissiveColor, Color3{0.52f, 0.5299f, 0.54f}}, /* bad rounding in file */
        {"emissiveColorFactor", 0.51f},
        {"thinFilmThickness", 0.55f},
        {"thinFilmIor", 0.56f},
        {"opacity", Color3{0.57f, 0.58f, 0.5899f}}, /* bad rounding in file */
        {"indirectDiffuse", 0.63f},
        {"indirectSpecular", 0.64f},

        {MaterialAttribute::LayerName, "ClearCoat"},
        {MaterialAttribute::LayerFactor, 0.35f},
        {"color", Color4{0.36f, 0.37f, 0.38f, 1.0f}},
        {MaterialAttribute::Roughness, 0.39f},
        {"ior", 0.40f},
        {"anisotropy", 0.41f},
        {"rotation", 0.42f},

        {MaterialAttribute::LayerName, "transmission"},
        {MaterialAttribute::LayerFactor, 0.15f},
        {"color", Vector4{0.16f, 0.17f, 0.18f, 1.0f}},
        {"depth", 0.19f},
        {"scatter", Vector3{0.20f, 0.21f, 0.22f}},
        {"scatterAnisotropy", 0.23f},
        {"dispersion", 0.24f},
        {"extraRoughness", 0.25f},
        {"enableInAov", true},
        {"priority", Long(69)},

        {MaterialAttribute::LayerName, "subsurface"},
        {MaterialAttribute::LayerFactor, 0.26f},
        {"color", Color4{0.27f, 0.28f, 0.29f, 1.0f}},
        {"radius", Color3{0.30f, 0.31f, 0.32f}},
        {"scale", 0.33f},
        {"anisotropy", 0.34f},
        {"type", Long(1)},

        {MaterialAttribute::LayerName, "sheen"},
        {MaterialAttribute::LayerFactor, 0.46f},
        {"color", Color3{0.47f, 0.48f, 0.49f}},
        {MaterialAttribute::Roughness, 0.50f},

        {MaterialAttribute::LayerName, "matte"},
        {MaterialAttribute::LayerFactor, 0.68f},
        {"color", Color3{0.65f, 0.66f, 0.67f}},
    }, {17, 24, 34, 41, 45, 48}};

    CORRADE_COMPARE_AS(*material, reference, DebugTools::CompareMaterial);
}

void UfbxImporterTest::materialMaxPhysical() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "max-material-physical.fbx")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<MaterialData> material = importer->material("PhysicalMaterial");
    CORRADE_VERIFY(material);

    MaterialData reference{MaterialType::Phong|MaterialType::PbrMetallicRoughness|MaterialType::PbrClearCoat, {
        /* Phong */
        {MaterialAttribute::AmbientColor, Color4{0.0002f, 0.0003f, 0.0004f, 1.0f}},
        {MaterialAttribute::DiffuseColor, Color4{0.0002f, 0.0003f, 0.0004f, 1.0f}},
        {MaterialAttribute::SpecularColor, Color4{1.0f, 1.0f, 1.0f, 1.0f}},
        {MaterialAttribute::Shininess, 675.587890625f},

        /* Physical */
        {MaterialAttribute::BaseColor, Color4{0.02f, 0.03f, 0.04f, 0.05f} * Color4{0.01f, 1.0f}},
        {MaterialAttribute::Roughness, 0.06f},
        {MaterialAttribute::Metalness, 0.07f},
        {"specularIor", 0.80f}, /* 3ds Max doesn't allow IOR less than 0.1 */
        {MaterialAttribute::EmissiveColor, Color3{0.28f, 0.29f, 0.30f} * Vector3{0.27f}},
        {"specularAnisotropy", 0.32f},
        {"specularRotation", 0.33f},
        {"displacementFactor", 0.36f},
        {"diffuseRoughness", 0.37f},

        {MaterialAttribute::LayerName, "ClearCoat"},
        {MaterialAttribute::LayerFactor, 0.38f},
        {"color", Color4{0.39f, 0.40f, 0.41f, 0.42f}},
        {MaterialAttribute::Roughness, 0.43f},
        {"ior", 0.44f},
        {"affectBaseColor", 0.45f},
        {"affectBaseRoughness", 0.46f},

        {MaterialAttribute::LayerName, "transmission"},
        {MaterialAttribute::LayerFactor, 0.09f},
        {"color", Vector4{0.10f, 0.11f, 0.12f, 0.13f}},
        {MaterialAttribute::Roughness, 0.14f},
        {"depth", 0.059055f}, /* unit conversion in file */

        {MaterialAttribute::LayerName, "subsurface"},
        {MaterialAttribute::LayerFactor, 0.16f},
        {"tintColor", Color4{0.17f, 0.18f, 0.19f, 0.20f}},
        {"color", Color4{0.21f, 0.22f, 0.23f, 0.24f}},
        {"radius", Color3{0.098425f}}, /* unit conversion in file */
        {"scale", 0.26f},
    }, {13, 20, 25, 31}};

    CORRADE_COMPARE_AS(*material, reference, DebugTools::CompareMaterial);
}

void UfbxImporterTest::materialMaxPhysicalFactors() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("preserveMaterialFactors", true);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "max-material-physical.fbx")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<MaterialData> material = importer->material("PhysicalMaterial");
    CORRADE_VERIFY(material);

    MaterialData reference{MaterialType::Phong|MaterialType::PbrMetallicRoughness|MaterialType::PbrClearCoat, {
        /* Phong */
        {MaterialAttribute::AmbientColor, Color4{0.0002f, 0.0003f, 0.0004f, 1.0f}},
        {MaterialAttribute::DiffuseColor, Color4{0.0002f, 0.0003f, 0.0004f, 1.0f}},
        {MaterialAttribute::SpecularColor, Color4{1.0f, 1.0f, 1.0f, 1.0f}},
        {MaterialAttribute::Shininess, 675.587890625f},
        {"specularColorFactor", 1.0f},
        {"transparencyColorFactor", 0.00594f},

        /* Physical */
        {MaterialAttribute::BaseColor, Color4{0.02f, 0.03f, 0.04f, 0.05f}},
        {"baseColorFactor", 0.01f},
        {MaterialAttribute::Roughness, 0.06f},
        {MaterialAttribute::Metalness, 0.07f},
        {"specularIor", 0.80f}, /* 3ds Max doesn't allow IOR less than 0.1 */
        {MaterialAttribute::EmissiveColor, Color3{0.28f, 0.29f, 0.30f}},
        {"emissiveColorFactor", 0.27f},
        {"specularAnisotropy", 0.32f},
        {"specularRotation", 0.33f},
        {"displacementFactor", 0.36f},
        {"diffuseRoughness", 0.37f},

        {MaterialAttribute::LayerName, "ClearCoat"},
        {MaterialAttribute::LayerFactor, 0.38f},
        {"color", Color4{0.39f, 0.40f, 0.41f, 0.42f}},
        {MaterialAttribute::Roughness, 0.43f},
        {"ior", 0.44f},
        {"affectBaseColor", 0.45f},
        {"affectBaseRoughness", 0.46f},

        {MaterialAttribute::LayerName, "transmission"},
        {MaterialAttribute::LayerFactor, 0.09f},
        {"color", Vector4{0.10f, 0.11f, 0.12f, 0.13f}},
        {MaterialAttribute::Roughness, 0.14f},
        {"depth", 0.059055f}, /* unit conversion in file */

        {MaterialAttribute::LayerName, "subsurface"},
        {MaterialAttribute::LayerFactor, 0.16f},
        {"tintColor", Color4{0.17f, 0.18f, 0.19f, 0.20f}},
        {"color", Color4{0.21f, 0.22f, 0.23f, 0.24f}},
        {"radius", Color3{0.098425f}}, /* unit conversion in file */
        {"scale", 0.26f},
    }, {17, 24, 29, 35}};

    CORRADE_COMPARE_AS(*material, reference, DebugTools::CompareMaterial);
}

void UfbxImporterTest::materialMaxPbrSpecGloss() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "max-material-spec-gloss.fbx")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<MaterialData> material = importer->material("02 - Default");
    CORRADE_VERIFY(material);

    Int texDiffuse = importer->textureForName("Map #18");
    Int texSpecular = importer->textureForName("Map #19");
    Int texWeight = importer->textureForName("Map #20");
    Int texAmbient = importer->textureForName("Map #21");
    Int texNormal = importer->textureForName("Map #22");
    Int texEmissive = importer->textureForName("Map #23");
    Int texDisplacement = importer->textureForName("Map #24");
    Int texTransparency = importer->textureForName("Map #25");

    CORRADE_COMPARE_AS(texDiffuse, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(texSpecular, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(texWeight, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(texAmbient, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(texNormal, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(texEmissive, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(texDisplacement, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(texTransparency, 0, TestSuite::Compare::GreaterOrEqual);

    MaterialData reference{MaterialType::Phong|MaterialType::PbrSpecularGlossiness, {
        /* Phong */
        {MaterialAttribute::AmbientColor, Color4{0.01f, 0.02f, 0.03f, 1.0f}},
        {MaterialAttribute::DiffuseColor, Color4{0.01f, 0.02f, 0.03f, 1.0f}},
        {MaterialAttribute::Shininess, 1024.0f},

        /* Physical */
        {MaterialAttribute::BaseColor, Color4{0.01f, 0.02f, 0.03f, 0.04f}},
        {MaterialAttribute::BaseColorTexture, UnsignedInt(texDiffuse)},
        {MaterialAttribute::SpecularColor, Color4{0.05f, 0.06f, 0.07f, 0.08f}},
        {MaterialAttribute::SpecularTexture, UnsignedInt(texSpecular)},
        {MaterialAttribute::Glossiness, 0.09f},
        {MaterialAttribute::GlossinessTexture, UnsignedInt(texWeight)},
        {MaterialAttribute::EmissiveColor, Color3{0.12f, 0.13f, 0.14f}},
        {MaterialAttribute::EmissiveTexture, UnsignedInt(texEmissive)},
        {"displacementFactor", 0.16f},
        {"displacementTexture", UnsignedInt(texDisplacement)},
        {MaterialAttribute::NormalTexture, UnsignedInt(texNormal)},
        {MaterialAttribute::OcclusionTexture, UnsignedInt(texAmbient)},
        {"opacityTexture", UnsignedInt(texTransparency)},
    }};

    CORRADE_COMPARE_AS(*material, reference, DebugTools::CompareMaterial);
}

void UfbxImporterTest::materialLayeredPbrTextures() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "layered-pbr-textures.fbx")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<MaterialData> material = importer->material("LayeredMaterial");
    CORRADE_VERIFY(material);

    Containers::Array<UnsignedInt> baseLayerIds;
    Containers::Array<UnsignedInt> coatLayerIds;
    Containers::Array<UnsignedInt> sheenLayerIds;

    UnsignedInt layerCount = material->layerCount();
    for(UnsignedInt id = 0; id < layerCount; ++id) {
        Containers::StringView name = material->layerName(id);
        if(name.isEmpty()) {
            arrayAppend(baseLayerIds, id);
        } else if(name == "ClearCoat") {
            arrayAppend(coatLayerIds, id);
        } else if(name == "sheen") {
            arrayAppend(sheenLayerIds, id);
        }
    }

    CORRADE_COMPARE(baseLayerIds.size(), 3);
    CORRADE_COMPARE(coatLayerIds.size(), 3);
    CORRADE_COMPARE(sheenLayerIds.size(), 2);

    const struct {
        UnsignedInt layer;
        Containers::StringView attribute;
        Containers::StringView imageName;
        Containers::StringView blendMode;
        Float blendAlpha;
    } attributeTextureLayers[]{
        {baseLayerIds[0], "BaseColorTexture", "tex-red.png", "over", 1.0f},
        {baseLayerIds[1], "BaseColorTexture", "tex-green.png", "over", 0.25f},
        {baseLayerIds[2], "BaseColorTexture", "tex-blue.png", "additive", 0.75f},
        {coatLayerIds[0], "colorTexture", "tex-cyan.png", "over", 1.0f},
        {coatLayerIds[1], "colorTexture", "tex-pink.png", "softLight", 0.33f},
        {coatLayerIds[2], "colorTexture", "tex-yellow.png", "multiply", 0.66f},
        {coatLayerIds[0], "RoughnessTexture", "tex-black.png", "over", 1.0f},
        {coatLayerIds[1], "RoughnessTexture", "tex-white.png", "screen", 0.5f},
        {sheenLayerIds[0], "LayerFactorTexture", "tex-black.png", "over", 1.0f},
        {sheenLayerIds[1], "LayerFactorTexture", "tex-white.png", "replace",0.5f},
    };

    for(UnsignedInt i = 0; i < Containers::arraySize(attributeTextureLayers); ++i) {
        CORRADE_ITERATION(i);

        const auto& texLayer = attributeTextureLayers[i];
        CORRADE_VERIFY(material->hasAttribute(texLayer.layer, texLayer.attribute));
        UnsignedInt textureId = material->attribute<UnsignedInt>(texLayer.layer, texLayer.attribute);

        Containers::Optional<TextureData> texture = importer->texture(textureId);
        CORRADE_VERIFY(texture);

        CORRADE_COMPARE(importer->image2DName(texture->image()), texLayer.imageName);

        const Containers::String blendModeAttribute = texLayer.attribute + "BlendMode"_s;
        CORRADE_VERIFY(material->hasAttribute(texLayer.layer, blendModeAttribute));
        CORRADE_COMPARE(material->attribute<Containers::StringView>(texLayer.layer, blendModeAttribute), texLayer.blendMode);

        const Containers::String blendAlphaAttribute = texLayer.attribute + "BlendAlpha"_s;
        CORRADE_VERIFY(material->hasAttribute(texLayer.layer, blendAlphaAttribute));
        CORRADE_COMPARE(material->attribute<Float>(texLayer.layer, blendAlphaAttribute), texLayer.blendAlpha);
    }
}

void UfbxImporterTest::meshMaterials() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-materials.fbx")));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 1);
    CORRADE_COMPARE(importer->materialCount(), 3);
    CORRADE_COMPARE(importer->meshCount(), 3);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    CORRADE_VERIFY(scene->hasField(SceneField::MeshMaterial));
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0, 0, 0,
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0, 1, 2,
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::MeshMaterial), Containers::arrayView<Int>({
        0, 1, 2,
    }), TestSuite::Compare::Container);

    CORRADE_COMPARE(importer->materialName(0), "Default");
    CORRADE_COMPARE(importer->materialName(1), "Top");
    CORRADE_COMPARE(importer->materialName(2), "Textures");

    /* 'Default' material is on the sides */
    {
        Containers::Optional<MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->indexCount(), 4*2*3);

        UnsignedInt vertexCount = mesh->vertexCount();
        Containers::StridedArrayView1D<const Vector3> normal = mesh->attribute<Vector3>(MeshAttribute::Normal);
        for(UnsignedInt i = 0; i < vertexCount; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE(normal[i].z(), 0.0f);
        }

    /* 'Top' material is on the top face (+Z in Blender local) */
    } {
        Containers::Optional<MeshData> mesh = importer->mesh(1);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->indexCount(), 2*3);

        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Normal), 1);

        UnsignedInt vertexCount = mesh->vertexCount();
        Containers::StridedArrayView1D<const Vector3> position = mesh->attribute<Vector3>(MeshAttribute::Position);
        Containers::StridedArrayView1D<const Vector3> normal = mesh->attribute<Vector3>(MeshAttribute::Normal);
        for(UnsignedInt i = 0; i < vertexCount; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE(position[i].z(), 1.0f);
            CORRADE_COMPARE(normal[i], (Vector3{0.0f,0.0f,1.0f}));
        }

    /* 'Textures' material is on the bottom face (-Z in Blender local) */
    } {
        Containers::Optional<MeshData> mesh = importer->mesh(2);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->indexCount(), 2*3);

        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Normal), 1);

        UnsignedInt vertexCount = mesh->vertexCount();
        Containers::StridedArrayView1D<const Vector3> position = mesh->attribute<Vector3>(MeshAttribute::Position);
        Containers::StridedArrayView1D<const Vector3> normal = mesh->attribute<Vector3>(MeshAttribute::Normal);
        for(UnsignedInt i = 0; i < vertexCount; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE(position[i].z(), -1.0f);
            CORRADE_COMPARE(normal[i], (Vector3{0.0f,0.0f,-1.0f}));
        }
    }
}

void UfbxImporterTest::instancedMaterial() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "instanced-material.fbx")));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 3);
    CORRADE_COMPARE(importer->materialCount(), 6);
    CORRADE_COMPARE(importer->meshCount(), 4);

    Long modelA = importer->objectForName("ModelA");
    Long modelB = importer->objectForName("ModelB");
    Long modelC = importer->objectForName("ModelC");
    Int materialA = importer->materialForName("MaterialA");
    Int materialB = importer->materialForName("MaterialB");
    Int materialC = importer->materialForName("MaterialC");
    Int topA = importer->materialForName("TopA");
    Int topB = importer->materialForName("TopB");
    Int topC = importer->materialForName("TopC");

    CORRADE_COMPARE_AS(modelA, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(modelB, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(modelC, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(materialA, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(materialB, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(materialC, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(topA, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(topB, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(topC, 0, TestSuite::Compare::GreaterOrEqual);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    CORRADE_COMPARE_AS(scene->meshesMaterialsFor(UnsignedLong(modelA)), (Containers::arrayView<Containers::Pair<UnsignedInt, Int>>({
        {0, materialA},
        {1, topA},
    })), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(scene->meshesMaterialsFor(UnsignedLong(modelB)), (Containers::arrayView<Containers::Pair<UnsignedInt, Int>>({
        {2, materialB},
        {3, topB},
    })), TestSuite::Compare::Container);

    /* Note: Re-uses mesh from A with different materials! */
    CORRADE_COMPARE_AS(scene->meshesMaterialsFor(UnsignedLong(modelC)), (Containers::arrayView<Containers::Pair<UnsignedInt, Int>>({
        {0, materialC},
        {1, topC},
    })), TestSuite::Compare::Container);
}

void UfbxImporterTest::textureTransform() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "texture-transform.fbx")));

    Containers::Optional<MaterialData> material = importer->material("lambert1");
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType::Phong);

    const PhongMaterialData& phong = material->as<PhongMaterialData>();

    Int texFile1 = importer->textureForName("file1");
    Int texFile2 = importer->textureForName("file2");
    Int texFile3 = importer->textureForName("file3");

    CORRADE_COMPARE_AS(texFile1, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(texFile2, 0, TestSuite::Compare::GreaterOrEqual);
    CORRADE_COMPARE_AS(texFile3, 0, TestSuite::Compare::GreaterOrEqual);

    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTextureMatrix));

    CORRADE_COMPARE(phong.diffuseTexture(), UnsignedInt(texFile1));
    CORRADE_COMPARE(phong.diffuseTextureMatrix(),
        Matrix3::scaling(Vector2{2.0f, 3.0f}).inverted());

    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::AmbientTexture));
    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::AmbientTextureMatrix));

    CORRADE_COMPARE(phong.ambientTexture(), UnsignedInt(texFile2));
    CORRADE_COMPARE(phong.ambientTextureMatrix(),
        Matrix3::translation(Vector2{1.0f, 2.0f}).inverted());

    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::EmissiveTexture));
    CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::EmissiveTextureMatrix));

    CORRADE_COMPARE(phong.findAttribute<UnsignedInt>(MaterialAttribute::EmissiveTexture), UnsignedInt(texFile3));
    CORRADE_COMPARE(phong.findAttribute<Matrix3>(MaterialAttribute::EmissiveTextureMatrix),
        Matrix3::rotation(90.0_degf).inverted());

}

void UfbxImporterTest::textureWrapModes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "texture-wrap-modes.fbx")));

    {
        Containers::Optional<TextureData> texture = importer->texture("repeatRepeat");
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->wrapping(), (Math::Vector3<SamplerWrapping>{SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping::ClampToEdge}));
    } {
        Containers::Optional<TextureData> texture = importer->texture("clampRepeat");
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->wrapping(), (Math::Vector3<SamplerWrapping>{SamplerWrapping::ClampToEdge, SamplerWrapping::Repeat, SamplerWrapping::ClampToEdge}));
    } {
        Containers::Optional<TextureData> texture = importer->texture("repeatClamp");
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->wrapping(), (Math::Vector3<SamplerWrapping>{SamplerWrapping::Repeat, SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge}));
    } {
        Containers::Optional<TextureData> texture = importer->texture("clampClamp");
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->wrapping(), (Math::Vector3<SamplerWrapping>{SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge}));
    }
}

void UfbxImporterTest::imageEmbedded() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-materials.fbx")));

    UnsignedInt imageCount = importer->image2DCount();
    CORRADE_COMPARE(imageCount, 5);

    const struct {
        Containers::StringView name;
        Color4ub topLeftPixelColor;
        Color4ub bottomLeftPixelColor;
    } testImages[]{
        {"blender-materials.fbm\\checkerboard_diffuse.png",      0x7c5151ff_rgba, 0x3d2828ff_rgba},
        {"blender-materials.fbm\\checkerboard_emissive.png",     0x44743bff_rgba, 0x223a1dff_rgba},
        {"blender-materials.fbm\\checkerboard_roughness.png",    0x5f3228ff_rgba, 0x2f1914ff_rgba},
        {"blender-materials.fbm\\checkerboard_metallic.png",     0x63422cff_rgba, 0x312116ff_rgba},
        {"blender-materials.fbm\\checkerboard_transparency.png", 0x407177ff_rgba, 0x1f373aff_rgba},
    };

    for(const auto& testImage: testImages) {
        CORRADE_ITERATION(testImage.name);

        Int optionalId = importer->image2DForName(testImage.name);
        CORRADE_VERIFY(optionalId >= 0);
        UnsignedInt id = UnsignedInt(optionalId);

        CORRADE_COMPARE(importer->image2DLevelCount(id), 1);
        Containers::Optional<ImageData2D> image = importer->image2D(id);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{128,128}));
        CORRADE_COMPARE(image->pixels<Color4ub>()[0][0], testImage.bottomLeftPixelColor);
        CORRADE_COMPARE(image->pixels<Color4ub>()[127][0], testImage.topLeftPixelColor);
    }
}

void UfbxImporterTest::imageExternal() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "layered-pbr-textures.fbx")));

    Containers::Pair<Containers::StringView, Color3ub> imageColors[]{
        {"tex-red.png",    0xff0000_rgb},
        {"tex-green.png",  0x00ff00_rgb},
        {"tex-blue.png",   0x0000ff_rgb},
        {"tex-cyan.png",   0x00ffff_rgb},
        {"tex-pink.png",   0xff00ff_rgb},
        {"tex-yellow.png", 0xffff00_rgb},
        {"tex-black.png",  0x000000_rgb},
        {"tex-white.png",  0xffffff_rgb},
    };

    CORRADE_COMPARE(importer->image2DCount(), 8);
    for(UnsignedInt i = 0; i < Containers::arraySize(imageColors); ++i) {
        CORRADE_ITERATION(i);

        Containers::StringView imageName = imageColors[i].first();
        Color4ub imageColor = imageColors[i].second();

        Containers::Optional<ImageData2D> image = importer->image2D(imageName);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{1, 1}));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], imageColor);
    }
}

void UfbxImporterTest::imageExternalFromData() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    Containers::Optional<Containers::Array<char>> data = Utility::Path::read(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "layered-pbr-textures.fbx"));
    CORRADE_VERIFY(data);
    CORRADE_VERIFY(importer->openData(*data));

    CORRADE_COMPARE(importer->image2DCount(), 8);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::image2D(): external images can be imported only when opening files from the filesystem or if a file callback is present\n");
}

void UfbxImporterTest::imageFileCallback() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    FileCallbackFiles files;
    importer->setFileCallback(&fileCallbackFunc, &files);

    Containers::Pair<Containers::StringView, Color3ub> imageColors[]{
        {"tex-red.png",    0xff0000_rgb},
        {"tex-green.png",  0x00ff00_rgb},
        {"tex-blue.png",   0x0000ff_rgb},
        {"tex-cyan.png",   0x00ffff_rgb},
        {"tex-pink.png",   0xff00ff_rgb},
        {"tex-yellow.png", 0xffff00_rgb},
        {"tex-black.png",  0x000000_rgb},
        {"tex-white.png",  0xffffff_rgb},
    };

    CORRADE_VERIFY(importer->openFile("layered-pbr-textures.fbx"));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->image2DCount(), 8);
    for(UnsignedInt i = 0; i < Containers::arraySize(imageColors); ++i) {
        CORRADE_ITERATION(i);

        Containers::StringView imageName = imageColors[i].first();
        Color4ub imageColor = imageColors[i].second();

        Containers::Optional<ImageData2D> image = importer->image2D(imageName);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{1, 1}));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], imageColor);

        CORRADE_VERIFY(files.find(imageName) != files.end());
    }
}

void UfbxImporterTest::imageFileCallbackNotFound() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    UnsignedInt callCount = 0;
    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy, UnsignedInt& callCount) {
            ++callCount;
            return Containers::Optional<Containers::ArrayView<const char>>{};
        }, callCount);

    Containers::Optional<Containers::Array<char>> data = Utility::Path::read(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "layered-pbr-textures.fbx"));
    CORRADE_VERIFY(data);
    CORRADE_VERIFY(importer->openData(*data));
    CORRADE_COMPARE(importer->image2DCount(), 8);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file tex-red.png\n");
    CORRADE_COMPARE(callCount, 1);
}

void UfbxImporterTest::imageDeduplication() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "layered-pbr-textures.fbx")));

    Containers::Pair<Containers::StringView, Containers::StringView> textureToImage[]{
        {"Map #2", "tex-red.png"},
        {"Map #3", "tex-green.png"},
        {"Map #4", "tex-blue.png"},
        {"Map #9", "tex-cyan.png"},
        {"Map #10", "tex-pink.png"},
        {"Map #11", "tex-yellow.png"},
        {"Map #14", "tex-black.png"},
        {"Map #15", "tex-white.png"},
        {"Map #16", "tex-black.png"},
        {"Map #17", "tex-white.png"},
    };

    CORRADE_COMPARE(importer->image2DCount(), 8);

    for(UnsignedInt i = 0; i < Containers::arraySize(textureToImage); ++i) {
        CORRADE_ITERATION(i);

        Containers::StringView textureName = textureToImage[i].first();
        Containers::StringView imageName = textureToImage[i].second();

        Int id = importer->textureForName(textureName);
        CORRADE_COMPARE_AS(id, 0, TestSuite::Compare::GreaterOrEqual);
        CORRADE_COMPARE(importer->textureName(UnsignedInt(id)), textureName);

        Containers::Optional<TextureData> texture = importer->texture(UnsignedInt(id));
        CORRADE_VERIFY(texture);

        CORRADE_COMPARE(importer->image2DName(texture->image()), imageName);
        CORRADE_COMPARE(importer->image2DForName(imageName), texture->image());
    }
}

void UfbxImporterTest::imageNonExistentName() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "layered-pbr-textures.fbx")));

    CORRADE_COMPARE(importer->image2DCount(), 8);
    Int id = importer->image2DForName("not-an-image");
    CORRADE_COMPARE_AS(id, 0, TestSuite::Compare::Less);
}

void UfbxImporterTest::imageAbsolutePath() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "image-absolute-path.fbx")));

    CORRADE_COMPARE(importer->image2DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE_AS(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /absolute/path/to/tex-red.png", TestSuite::Compare::StringContains);
}

void UfbxImporterTest::imageNot2D() {
    if(_manager.loadState("DdsImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("DdsImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "image-3d.obj")));

    CORRADE_COMPARE(importer->image2DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::image2D(): expected exactly one 2D image in an image file but got 0\n");
}

void UfbxImporterTest::imageBrokenExternal() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "image-broken-external.fbx")));

    CORRADE_COMPARE(importer->image2DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));

    CORRADE_COMPARE_AS(out.str(),
        "Trade::StbImageImporter::image2D(): cannot open the image: ",
        TestSuite::Compare::StringHasPrefix);
}

void UfbxImporterTest::imageBrokenEmbedded() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "image-broken-embedded.fbx")));

    CORRADE_COMPARE(importer->image2DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));

    CORRADE_COMPARE(out.str(),
        "Trade::AnyImageImporter::openData(): cannot determine the format from signature 0xbadbadfb\n");
}

void UfbxImporterTest::objCube() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "cube.obj")));
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->objectCount(), 1);
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->materialCount(), 1);

    CORRADE_COMPARE(importer->objectForName("Cube"), 0);

    Int materialId = importer->materialForName("Material");
    CORRADE_COMPARE(materialId, 0);
    Containers::Optional<MaterialData> material = importer->material(UnsignedInt(materialId));
    CORRADE_VERIFY(material);

    MaterialData reference{MaterialType::Phong, {
        {MaterialAttribute::AmbientColor, Color4{1.0f, 0.0f, 0.0f, 1.0f}},
        {MaterialAttribute::DiffuseColor, Color4{0.0f, 1.0f, 0.0f, 1.0f}},
        {MaterialAttribute::SpecularColor, Color4{0.0f, 0.0f, 1.0f, 1.0f}},
        {MaterialAttribute::EmissiveColor, Color3{0.5f, 0.5f, 0.5f}},
        {MaterialAttribute::Shininess, 250.0f},
    }};
    CORRADE_COMPARE_AS(*material, reference, DebugTools::CompareMaterial);
}

void UfbxImporterTest::objCubeFileCallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    FileCallbackFiles files;
    importer->setFileCallback(&fileCallbackFunc, &files);

    CORRADE_VERIFY(importer->openFile("cube.obj"));
    CORRADE_COMPARE(files.size(), 2);
    CORRADE_VERIFY(files.find("cube.obj") != files.end());
    CORRADE_VERIFY(files.find("cube.mtl") != files.end());

    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 1);
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->materialCount(), 1);

    CORRADE_COMPARE(importer->objectForName("Cube"), 0);

    Int materialId = importer->materialForName("Material");
    CORRADE_COMPARE(materialId, 0);
    Containers::Optional<MaterialData> material = importer->material(UnsignedInt(materialId));
    CORRADE_VERIFY(material);

    MaterialData reference{MaterialType::Phong, {
        {MaterialAttribute::AmbientColor, Color4{1.0f, 0.0f, 0.0f, 1.0f}},
        {MaterialAttribute::DiffuseColor, Color4{0.0f, 1.0f, 0.0f, 1.0f}},
        {MaterialAttribute::SpecularColor, Color4{0.0f, 0.0f, 1.0f, 1.0f}},
        {MaterialAttribute::EmissiveColor, Color3{0.5f, 0.5f, 0.5f}},
        {MaterialAttribute::Shininess, 250.0f},
    }};
    CORRADE_COMPARE_AS(*material, reference, DebugTools::CompareMaterial);
}

void UfbxImporterTest::objMissingMtl() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->addFlags(data.flags);

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "missing-mtl.obj")));
    }
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else {
        CORRADE_COMPARE_AS(out.str(), "Trade::UfbxImporter::openFile(): Could not open .mtl file:", TestSuite::Compare::StringHasPrefix);
        CORRADE_COMPARE_AS(out.str(), "missing-mtl.mtl", TestSuite::Compare::StringContains);
    }

    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 1);
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->materialCount(), 1);

    CORRADE_COMPARE(importer->objectForName("Cube"), 0);

    Int materialId = importer->materialForName("Material");
    CORRADE_COMPARE(materialId, 0);
    Containers::Optional<MaterialData> material = importer->material(UnsignedInt(materialId));
    CORRADE_VERIFY(material);

    MaterialData reference{MaterialType{}, {}};
    CORRADE_COMPARE_AS(*material, reference, DebugTools::CompareMaterial);
}

void UfbxImporterTest::objMissingMtlFileCallback() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->addFlags(data.flags);

    FileCallbackFiles files;
    importer->setFileCallback(&fileCallbackFunc, &files);

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile("missing-mtl.obj"));
    }
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::openFile(): Could not open .mtl file: missing-mtl.mtl\n");

    CORRADE_COMPARE(files.size(), 2);
    CORRADE_VERIFY(files.find("missing-mtl.obj") != files.end());
    CORRADE_VERIFY(files.find("missing-mtl.mtl") != files.end());
    CORRADE_VERIFY(!files.find("missing-mtl.mtl")->second);

    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 1);
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->materialCount(), 1);

    CORRADE_COMPARE(importer->objectForName("Cube"), 0);

    Int materialId = importer->materialForName("Material");
    CORRADE_COMPARE(materialId, 0);
    Containers::Optional<MaterialData> material = importer->material(UnsignedInt(materialId));
    CORRADE_VERIFY(material);

    MaterialData reference{MaterialType{}, {}};
    CORRADE_COMPARE_AS(*material, reference, DebugTools::CompareMaterial);
}

void UfbxImporterTest::mtl() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    /* Same as objCube(), but importing the referenced `*.mtl` file directly --
       should result in just the material properties being present, no scene or
       mesh data */
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "cube.mtl")));
    CORRADE_COMPARE(importer->sceneCount(), 0);
    CORRADE_COMPARE(importer->defaultScene(), -1);
    CORRADE_COMPARE(importer->objectCount(), 0);
    CORRADE_COMPARE(importer->meshCount(), 0);
    CORRADE_COMPARE(importer->materialCount(), 1);

    Int materialId = importer->materialForName("Material");
    CORRADE_COMPARE(materialId, 0);
    Containers::Optional<MaterialData> material = importer->material(UnsignedInt(materialId));
    CORRADE_VERIFY(material);

    MaterialData reference{MaterialType::Phong, {
        {MaterialAttribute::AmbientColor, Color4{1.0f, 0.0f, 0.0f, 1.0f}},
        {MaterialAttribute::DiffuseColor, Color4{0.0f, 1.0f, 0.0f, 1.0f}},
        {MaterialAttribute::SpecularColor, Color4{0.0f, 0.0f, 1.0f, 1.0f}},
        {MaterialAttribute::EmissiveColor, Color3{0.5f, 0.5f, 0.5f}},
        {MaterialAttribute::Shininess, 250.0f},
    }};
    CORRADE_COMPARE_AS(*material, reference, DebugTools::CompareMaterial);
}

void UfbxImporterTest::normalizeUnitsAdjustTransforms() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("normalizeUnits", true);
    importer->configuration().setValue("unitNormalizationHandling", "adjustTransforms");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "units-cm-z-up.fbx")));
    CORRADE_COMPARE(importer->objectCount(), 1);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    {
        CORRADE_COMPARE(importer->objectName(0), "Empty");
        Containers::Optional<Containers::Triple<Vector3, Quaternion, Vector3>> trs = scene->translationRotationScaling3DFor(0);
        CORRADE_VERIFY(trs);
        CORRADE_COMPARE(trs->first(), (Vector3{1.0f, 2.0f, 3.0f}));
        CORRADE_COMPARE(trs->second(), (Quaternion{{0.0f, 0.707107f, 0.0f}, 0.707107f})); /* Euler (0, 90, 0) */
        CORRADE_COMPARE(trs->third(), (Vector3{0.2f, 0.4f, 0.6f}));
    }
}

void UfbxImporterTest::normalizeUnitsTransformRoot() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("normalizeUnits", true);
    importer->configuration().setValue("unitNormalizationHandling", "transformRoot");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "units-cm-z-up.fbx")));
    CORRADE_COMPARE(importer->objectCount(), 2);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    {
        /* Root node */
        CORRADE_COMPARE(importer->objectName(0), "");
        Containers::Optional<Containers::Triple<Vector3, Quaternion, Vector3>> trs = scene->translationRotationScaling3DFor(0);
        CORRADE_VERIFY(trs);
        CORRADE_COMPARE(trs->first(), (Vector3{0.0f, 0.0f, 0.0f}));
        CORRADE_COMPARE(trs->second(), (Quaternion{{-0.707107f, 0.0f, 0.0f}, 0.707107f})); /* Euler (-90, 0, 0) */
        CORRADE_COMPARE(trs->third(), (Vector3{0.01f})); /* centimeters to meters */
    } {
        CORRADE_COMPARE(importer->objectName(1), "Empty");
        Containers::Optional<Containers::Triple<Vector3, Quaternion, Vector3>> trs = scene->translationRotationScaling3DFor(1);
        CORRADE_VERIFY(trs);
        CORRADE_COMPARE(trs->first(), (Vector3{100.0f, -300.0f, 200.0f}));
        CORRADE_COMPARE(trs->second(), (Quaternion{{0.5f, 0.5f, 0.5f}, 0.5f})); /* Euler (90, 90, 0) */
        CORRADE_COMPARE(trs->third(), (Vector3{20.0f, 40.0f, 60.0f}));
    }
}

void UfbxImporterTest::normalizeUnitsInvalidHandling() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("normalizeUnits", true);
    importer->configuration().setValue("unitNormalizationHandling", "invalidUnitHandling");

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "units-cm-z-up.fbx")));
    CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::openFile(): Unsupported unitNormalizationHandling configuration: invalidUnitHandling\n");
}

void UfbxImporterTest::geometryCache() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "geometry-cache.fbx")));
}

void UfbxImporterTest::geometryCacheFileCallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    FileCallbackFiles files;
    importer->setFileCallback(&fileCallbackFunc, &files);

    CORRADE_VERIFY(importer->openFile("geometry-cache.fbx"));

    /* Should not attempt to load geometry caches */
    CORRADE_COMPARE(files.size(), 1);
    CORRADE_VERIFY(files.find("geometry-cache.fbx") != files.end());
}

void UfbxImporterTest::staticSkin() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "static-skin.fbx")));

    CORRADE_COMPARE(importer->objectCount(), 5);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    Containers::Pair<Containers::StringView, Containers::StringView> parents[]{
        {"pCube1", ""},
        {"joint1", ""},
        {"joint2", "joint1"},
        {"joint3", "joint2"},
        {"joint4", "joint3"},
    };

    for(const auto& pair: parents) {
        Long objectId = importer->objectForName(pair.first());
        CORRADE_COMPARE_AS(objectId, 0, TestSuite::Compare::GreaterOrEqual);

        Containers::Optional<Long> parentId = scene->parentFor(objectId);
        CORRADE_VERIFY(parentId);
        if(!pair.second().isEmpty()) {
            CORRADE_COMPARE(importer->objectName(*parentId), pair.second());
        } else {
            CORRADE_COMPARE_AS(*parentId, 0, TestSuite::Compare::Less);
        }
    }
}

void UfbxImporterTest::multiWarning() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->addFlags(data.flags);

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "warning-cube.fbx")));
    }
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(),
            "Trade::UfbxImporter::openFile(): Clamped index (x4)\n"
            "Trade::UfbxImporter::openFile(): Bad UTF-8 string\n");
}

void UfbxImporterTest::multiWarningData() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->addFlags(data.flags);

    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "warning-cube.fbx"));
    CORRADE_VERIFY(fileData);

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openData(*fileData));
    }
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(),
            "Trade::UfbxImporter::openData(): Clamped index (x4)\n"
            "Trade::UfbxImporter::openData(): Bad UTF-8 string\n");
}

namespace {

template<class V, class R = Animation::ResultOf<V>>
inline const Animation::TrackView<const Float, const V, R> trackByTarget(const AnimationData &animation, UnsignedInt target, AnimationTrackTarget targetType)
{
    for(UnsignedInt i = 0; i < animation.trackCount(); ++i) {
        if(animation.trackTarget(i) == target && animation.trackTargetName(i) == targetType) {
            return animation.track<V, R>(i);
        }
    }
    CORRADE_FAIL("Track not found for target" << target << "type" << targetType);
    return {};
}

}

void UfbxImporterTest::animationInterpolation() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "animation-interpolation.fbx")));

    CORRADE_COMPARE(importer->animationCount(), 1);

    Containers::Optional<AnimationData> animation = importer->animation(0);
    CORRADE_VERIFY(animation);

    const float epsilon = 0.001f;

    auto track = trackByTarget<Vector3>(*animation, 0, AnimationTrackTarget::Translation3D);

    CORRADE_COMPARE_AS(track.keys(),
        Containers::arrayView<Float>({
            /* Resampled cubic */
            0.0f / 30.0f,
            1.0f / 30.0f,
            2.0f / 30.0f,
            3.0f / 30.0f,
            4.0f / 30.0f,
            5.0f / 30.0f,
            6.0f / 30.0f,
            7.0f / 30.0f,
            8.0f / 30.0f,
            9.0f / 30.0f,
            /* Linear */
            10.0f / 30.0f,
            /* Constant previous */
            20.0f / 30.0f,
            25.0f / 30.0f - epsilon,
            /* Constant next */
            25.0f / 30.0f,
            25.0f / 30.0f + epsilon,
            /* Constant previous */
            30.0f / 30.0f,
            35.0f / 30.0f - epsilon,
            /* Linear */
            35.0f / 30.0f,
            /* Constant next */
            40.0f / 30.0f,
            40.0f / 30.0f + epsilon,
            /* Constant previous */
            45.0f / 30.0f,
            50.0f / 30.0f - epsilon,
            /* Constant next */
            50.0f / 30.0f,
            50.0f / 30.0f + epsilon,
            /* Final */
            55.0f / 30.0f,
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(track.values(),
        Containers::arrayView<Vector3>({
            /* Resampled cubic */
            {0.0f, 0.0f, 0.0f},
            {-0.855245f, 0.0f, 0.0f},
            {-1.13344f, 0.0f, 0.0f},
            {-1.17802f, 0.0f, 0.0f},
            {-1.10882f, 0.0f, 0.0f},
            {-0.991537f, 0.0f, 0.0f},
            {-0.875223f, 0.0f, 0.0f},
            {-0.808958f, 0.0f, 0.0f},
            {-0.858419f, 0.0f, 0.0f},
            {-1.14293f, 0.0f, 0.0f},
            /* Linear */
            {-2.0f, 0.0f, 0.0f},
            /* Constant previous */
            {-4.0f, 0.0f, 0.0f},
            {-4.0f, 0.0f, 0.0f},
            /* Constant next */
            {-6.0f, 0.0f, 0.0f},
            {-8.0f, 0.0f, 0.0f},
            /* Constant previous */
            {-8.0f, 0.0f, 0.0f},
            {-8.0f, 0.0f, 0.0f},
            /* Linear */
            {-10.0f, 0.0f, 0.0f},
            /* Constant next */
            {-12.0f, 0.0f, 0.0f},
            {-14.0f, 0.0f, 0.0f},
            /* Constant previous */
            {-14.0f, 0.0f, 0.0f},
            {-14.0f, 0.0f, 0.0f},
            /* Constant next */
            {-16.0f, 0.0f, 0.0f},
            {-14.0f, 0.0f, 0.0f},
            /* Final */
            {-14.0f, 0.0f, 0.0f},
        }), TestSuite::Compare::Container);
}

void UfbxImporterTest::animationRotationPivot() {
    auto&& data = ResampleRotationData[testCaseInstanceId()];

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("resampleRotation", data.resampleRotation);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "animation-rotation-pivot.fbx")));

    CORRADE_COMPARE(importer->animationCount(), 1);

    Containers::Optional<AnimationData> animation = importer->animation(0);
    CORRADE_VERIFY(animation);

    auto rotation = trackByTarget<Quaternion>(*animation, 0, AnimationTrackTarget::Rotation3D);
    auto translation = trackByTarget<Vector3>(*animation, 0, AnimationTrackTarget::Translation3D);

    if(data.resampleRotation) {
        CORRADE_COMPARE_AS(rotation.keys(),
            Containers::arrayView<Float>({
                /* Resampled Euler */
                0.0f / 30.0f,
                1.0f / 30.0f,
                2.0f / 30.0f,
                3.0f / 30.0f,
                4.0f / 30.0f,
                5.0f / 30.0f,
            }), TestSuite::Compare::Container);

        CORRADE_COMPARE_AS(rotation.values(),
            Containers::arrayView<Quaternion>({
                /* Resampled Euler */
                Quaternion::rotation(Deg{0.0f}, Vector3::yAxis()),
                Quaternion::rotation(Deg{18.0f}, Vector3::yAxis()),
                Quaternion::rotation(Deg{36.0f}, Vector3::yAxis()),
                Quaternion::rotation(Deg{54.0f}, Vector3::yAxis()),
                Quaternion::rotation(Deg{72.0f}, Vector3::yAxis()),
                Quaternion::rotation(Deg{90.0f}, Vector3::yAxis()),
            }), TestSuite::Compare::Container);
    } else {
        CORRADE_COMPARE_AS(rotation.keys(),
            Containers::arrayView<Float>({
                /* Non-resampled Euler */
                0.0f / 30.0f,
                5.0f / 30.0f,
            }), TestSuite::Compare::Container);

        CORRADE_COMPARE_AS(rotation.values(),
            Containers::arrayView<Quaternion>({
                /* Non-resampled Euler */
                Quaternion::rotation(Deg{0.0f}, Vector3::yAxis()),
                Quaternion::rotation(Deg{90.0f}, Vector3::yAxis()),
            }), TestSuite::Compare::Container);
    }

    /* Rotation is always resampled for complex translations */
    CORRADE_COMPARE_AS(translation.keys(),
        Containers::arrayView<Float>({
            /* Resampled Euler (complex due to pivot) */
            0.0f / 30.0f,
            1.0f / 30.0f,
            2.0f / 30.0f,
            3.0f / 30.0f,
            4.0f / 30.0f,
            5.0f / 30.0f,
            /* Linear */
            15.0f / 30.0f,
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(translation.values(),
        Containers::arrayView<Vector3>({
            /* Resampled Euler */
            (Vector3{0.0f, 0.0f, 0.0f}),
            (Vector3{0.097887f, 0.133333f, 0.618034f}),
            (Vector3{0.381966f, 0.266667f, 1.17557f}),
            (Vector3{0.824429f, 0.4f, 1.61803f}),
            (Vector3{1.38197f, 0.533333f, 1.90211f}),
            (Vector3{2.0f, 0.666667f, 2.0f}),
            /* Linear */
            (Vector3{2.0f, 2.0f, 2.0f}),
        }), TestSuite::Compare::Container);
}

void UfbxImporterTest::animationVisibility() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "animation-visibility.fbx")));

    constexpr AnimationTrackTarget CustomAnimationTrackTargetVisibility = animationTrackTargetCustom(0);

    CORRADE_COMPARE(importer->objectName(0), "pCube1");
    CORRADE_COMPARE(importer->objectName(1), "pCube2");

    Containers::Optional<AnimationData> animation = importer->animation(0);
    CORRADE_VERIFY(animation);

    auto visibility1 = trackByTarget<bool>(*animation, 0, CustomAnimationTrackTargetVisibility);
    auto visibility2 = trackByTarget<bool>(*animation, 1, CustomAnimationTrackTargetVisibility);

    const float epsilon = 0.001f;

    CORRADE_COMPARE_AS(visibility1.keys(),
        Containers::arrayView<Float>({
            0.0f / 24.0f,
            4.0f / 24.0f - epsilon,
            4.0f / 24.0f,
            8.0f / 24.0f - epsilon,
            8.0f / 24.0f,
            12.0f / 24.0f - epsilon,
            12.0f / 24.0f,
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(visibility1.values(),
        Containers::arrayView<bool>({
            false, false, true, true, false, false, true,
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(visibility2.keys(),
        Containers::arrayView<Float>({
            0.0f / 24.0f,
            6.0f / 24.0f - epsilon,
            6.0f / 24.0f,
            12.0f / 24.0f - epsilon,
            12.0f / 24.0f,
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(visibility2.values(),
        Containers::arrayView<bool>({
            true, true, false, false, true,
        }), TestSuite::Compare::Container);

    /* Make sure that the translation channel of the second node is correctly
       aligned. As all value data is packed contiguously the visibility bools
       from the first node misalign the buffer, needing padding between */
    auto translation2 = trackByTarget<Vector3>(*animation, 1, AnimationTrackTarget::Translation3D);
    CORRADE_COMPARE_AS(uintptr_t(translation2.values().data()), alignof(Vector3), TestSuite::Compare::Divisible);
}

void UfbxImporterTest::animationLayersMerged() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "animation-layer.fbx")));

    CORRADE_COMPARE(importer->animationCount(), 1);

    const float epsilon = 0.001f;

    Containers::Optional<AnimationData> animation = importer->animation(0);
    CORRADE_VERIFY(animation);

    auto track = trackByTarget<Vector3>(*animation, 0, AnimationTrackTarget::Translation3D);

    CORRADE_COMPARE_AS(track.keys(),
        Containers::arrayView<Float>({
            /* Initial */
            0.0f / 24.0f,
            /* Layer 1 X */
            6.0f / 24.0f,
            /* Layer 1 Weight */
            8.0f / 24.0f - epsilon,
            8.0f / 24.0f,
            /* Layer 0 X */
            12.0f / 24.0f,
            /* Layer 1 Weight */
            14.0f / 24.0f - epsilon,
            14.0f / 24.0f,
            /* Layer 1 X */
            18.0f / 24.0f,
            /* Layer 1 Weight */
            22.0f / 24.0f - epsilon,
            22.0f / 24.0f,
            /* Final */
            24.0f / 24.0f,
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(track.values(),
        Containers::arrayView<Vector3>({
            /* Initial */
            {0.0f, 0.0f, 0.0f},
            /* Layer 1 X */
            {2.0f, 0.0f, 0.0f},
            /* Layer 1 Weight */
            {3.15267f, 0.0f, 0.0f},
            {3.041667f, 0.0f, 0.0f},
            /* Layer 0 X */
            {5.125f, 0.0f, 0.0f},
            /* Layer 1 Weight */
            {4.83683f, 0.0f, 0.0f},
            {3.833333f, 0.0f, 0.0f},
            /* Layer 1 X */
            {2.75f, 0.0f, 0.0f},
            /* Layer 1 Weight */
            {1.42467f, 0.0f, 0.0f},
            {2.166667f, 0.0f, 0.0f},
            /* Final */
            {1.5f, 0.0f, 0.0f},
        }), TestSuite::Compare::Container);

    /* X evaluated at frames in Maya */
    constexpr UnsignedInt KeyCount = 25;
    const Float reference[KeyCount] = {
        0.0f,
        0.333333f,
        0.666667f,
        1.0f,
        1.333333f,
        1.666667f,
        2.0f,
        2.583333f,
        3.041667f,
        3.5625f,
        4.083333f,
        4.604167f,
        5.125f,
        4.979167f,
        3.833333f,
        3.5625f,
        3.291667f,
        3.020833f,
        2.75f,
        2.416667f,
        2.083333f,
        1.75f,
        2.166667f,
        1.833333f,
        1.5f,
    };

    size_t hint = 0;
    for(UnsignedInt i = 0; i < KeyCount; ++i) {
        Float t = Float(i) / 24.0f;
        CORRADE_ITERATION(i);
        CORRADE_COMPARE(track.at(t, hint), (Vector3{reference[i], 0.0f, 0.0f}));
    }
}

void UfbxImporterTest::animationLayersRetained() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("animationLayers", true);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "animation-layer.fbx")));

    CORRADE_COMPARE(importer->animationCount(), 2);

    {
        Containers::Optional<AnimationData> animation = importer->animation("BaseLayer");
        CORRADE_VERIFY(animation);

        auto track = trackByTarget<Vector3>(*animation, 0, AnimationTrackTarget::Translation3D);

        CORRADE_COMPARE_AS(track.keys(),
            Containers::arrayView<Float>({
                0.0f / 24.0f,
                12.0f / 24.0f,
                24.0f / 24.0f,
            }), TestSuite::Compare::Container);

        CORRADE_COMPARE_AS(track.values(),
            Containers::arrayView<Vector3>({
                {0.0f, 0.0f, 0.0f},
                {4.0f, 0.0f, 0.0f},
                {0.0f, 0.0f, 0.0f},
            }), TestSuite::Compare::Container);
    }

    {
        Containers::Optional<AnimationData> animation = importer->animation("AnimLayer1");
        CORRADE_VERIFY(animation);

        auto track = trackByTarget<Vector3>(*animation, 0, AnimationTrackTarget::Translation3D);

        CORRADE_COMPARE_AS(track.keys(),
            Containers::arrayView<Float>({
                0.0f / 24.0f,
                6.0f / 24.0f,
                18.0f / 24.0f,
                24.0f / 24.0f,
            }), TestSuite::Compare::Container);

        CORRADE_COMPARE_AS(track.values(),
            Containers::arrayView<Vector3>({
                {0.0f, 0.0f, 0.0f},
                {0.0f, 0.0f, 0.0f},
                {3.0f, 0.0f, 0.0f},
                {3.0f, 0.0f, 0.0f},
            }), TestSuite::Compare::Container);
    }
}

void UfbxImporterTest::animationLayersNonLinearWeight() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("minimumSampleRate", 10.0f);
    importer->configuration().setValue("resampleRate", 10.0f);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "animation-layer-nonlinear-weight.fbx")));
    CORRADE_COMPARE(importer->animationCount(), 1);

    {
        Containers::Optional<AnimationData> animation = importer->animation(0);
        CORRADE_VERIFY(animation);

        auto track = trackByTarget<Vector3>(*animation, 0, AnimationTrackTarget::Translation3D);

        CORRADE_COMPARE_AS(track.keys(),
            Containers::arrayView<Float>({
                0.0f,
                0.1f,
                0.2f,
                0.3f,
                0.4f,
                0.5f,
                0.6f,
                0.7f,
                0.8f,
                0.9f,
                1.0f,
            }), TestSuite::Compare::Container);

        CORRADE_COMPARE_AS(track.values(),
            Containers::arrayView<Vector3>({
                {0.0f, 0.0f, 0.0f},
                {0.1f, 0.0f, 0.0f},
                {0.4f, 0.0f, 0.0f},
                {0.9f, 0.0f, 0.0f},
                {1.6f, 0.0f, 0.0f},
                {2.5f, 0.0f, 0.0f},
                {3.6f, 0.0f, 0.0f},
                {4.9f, 0.0f, 0.0f},
                {6.4f, 0.0f, 0.0f},
                {8.1f, 0.0f, 0.0f},
                {10.0f, 0.0f, 0.0f},
            }), TestSuite::Compare::Container);
    }
}

void UfbxImporterTest::animationLayersNonLinearWeightNonResampled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("minimumSampleRate", 0.0f);
    importer->configuration().setValue("resampleRate", 0.0f);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "animation-layer-nonlinear-weight.fbx")));
    CORRADE_COMPARE(importer->animationCount(), 1);

    {
        Containers::Optional<AnimationData> animation = importer->animation(0);
        CORRADE_VERIFY(animation);

        auto track = trackByTarget<Vector3>(*animation, 0, AnimationTrackTarget::Translation3D);

        CORRADE_COMPARE_AS(track.keys(),
            Containers::arrayView<Float>({
                0.0f,
                1.0f,
            }), TestSuite::Compare::Container);

        CORRADE_COMPARE_AS(track.values(),
            Containers::arrayView<Vector3>({
                {0.0f, 0.0f, 0.0f},
                {10.0f, 0.0f, 0.0f},
            }), TestSuite::Compare::Container);
    }
}

namespace {

struct AnimTarget {
    const UnsignedLong objectId;
    Vector3 translation;
    Quaternion rotation;
    Vector3 scaling;
};

Containers::Optional<Animation::Player<Float>> createAnimationPlayer(const AnimationData &animation, Containers::ArrayView<AnimTarget> targets) {
    Animation::Player<Float> player;
    for(UnsignedInt j = 0; j < animation.trackCount(); j++) {
        AnimTarget* target = nullptr;
        const UnsignedInt targetId = animation.trackTarget(j);
        for(UnsignedInt i = 0; i < targets.size(); ++i) {
            if(targets[i].objectId == targetId) {
                target = &targets[i];
                break;
            }
        }
        CORRADE_ASSERT(target != nullptr, "Expected to find a target", Containers::NullOpt);

        switch(animation.trackTargetName(j)) {
            case AnimationTrackTarget::Translation3D:
                player.add(animation.track<Vector3>(j), target->translation);
                break;
            case AnimationTrackTarget::Rotation3D:
                player.add(animation.track<Quaternion>(j), target->rotation);
                break;
            case AnimationTrackTarget::Scaling3D:
                player.add(animation.track<Vector3>(j), target->scaling);
                break;
            default:
                CORRADE_ASSERT(false, "Unhandled target type", Containers::NullOpt);
        }
    }
    return std::move(player);
}

}

void UfbxImporterTest::animationStacks() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "animation-stacks.fbx")));

    CORRADE_COMPARE(importer->animationCount(), 3);

    Long cubeId = importer->objectForName("Cube");
    CORRADE_COMPARE_AS(cubeId, 0, TestSuite::Compare::GreaterOrEqual);

    {
        Containers::Optional<AnimationData> animation = importer->animation("Cube|Lift");
        CORRADE_VERIFY(animation);

        AnimTarget targets[] = {{UnsignedLong(cubeId)}};
        Containers::Optional<Animation::Player<Float>> player = createAnimationPlayer(*animation, Containers::arrayView(targets));
        CORRADE_VERIFY(player);

        player->play(0.0f);
        player->advance(10.0f/24.0f);

        CORRADE_COMPARE(targets[0].translation, (Vector3{0.0f, 1.0f, 0.0f}));
        CORRADE_COMPARE(targets[0].rotation, (Quaternion{{-0.707107f, 0.0f, 0.0f}, 0.707107f})); /* Euler (-90, 0, 0) */
        CORRADE_COMPARE(targets[0].scaling, (Vector3{1.0f, 1.0f, 1.0f}));
    }

    {
        Containers::Optional<AnimationData> animation = importer->animation("Cube|Spin");
        CORRADE_VERIFY(animation);

        AnimTarget targets[] = {{UnsignedLong(cubeId)}};
        Containers::Optional<Animation::Player<Float>> player = createAnimationPlayer(*animation, Containers::arrayView(targets));
        CORRADE_VERIFY(player);

        player->play(0.0f);
        player->advance(10.0f/24.0f);

        CORRADE_COMPARE(targets[0].translation, (Vector3{0.0f, 0.0f, 0.0f}));
        CORRADE_COMPARE(targets[0].rotation, (Quaternion{{-0.5, 0.5, 0.5}, 0.5})); /* Euler (-90, 90, 0) */
        CORRADE_COMPARE(targets[0].scaling, (Vector3{1.0f, 1.0f, 1.0f}));
    }

    {
        Containers::Optional<AnimationData> animation = importer->animation("Cube|Grow");
        CORRADE_VERIFY(animation);

        AnimTarget targets[] = {{UnsignedLong(cubeId)}};
        Containers::Optional<Animation::Player<Float>> player = createAnimationPlayer(*animation, Containers::arrayView(targets));
        CORRADE_VERIFY(player);

        player->play(0.0f);
        player->advance(10.0f/24.0f);

        CORRADE_COMPARE(targets[0].translation, (Vector3{0.0f, 0.0f, 0.0f}));
        CORRADE_COMPARE(targets[0].rotation, (Quaternion{{-0.707107f, 0.0f, 0.0f}, 0.707107f})); /* Euler (-90, 0, 0) */
        CORRADE_COMPARE(targets[0].scaling, (Vector3{2.0f, 2.0f, 2.0f}));
    }
}

void UfbxImporterTest::animationLayerNames() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("animationLayers", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "animation-layer.fbx")));

    CORRADE_COMPARE(importer->animationName(0), "BaseLayer");
    CORRADE_COMPARE(importer->animationName(1), "AnimLayer1");
    CORRADE_COMPARE(importer->animationForName("BaseLayer"), 0);
    CORRADE_COMPARE(importer->animationForName("AnimLayer1"), 1);
    CORRADE_COMPARE(importer->animationForName("Nonexistent"), -1);
}

void UfbxImporterTest::animationStackNames() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "animation-stacks.fbx")));

    CORRADE_COMPARE(importer->animationName(0), "Cube|Grow");
    CORRADE_COMPARE(importer->animationName(1), "Cube|Lift");
    CORRADE_COMPARE(importer->animationName(2), "Cube|Spin");
    CORRADE_COMPARE(importer->animationForName("Cube|Grow"), 0);
    CORRADE_COMPARE(importer->animationForName("Cube|Lift"), 1);
    CORRADE_COMPARE(importer->animationForName("Cube|Spin"), 2);
    CORRADE_COMPARE(importer->animationForName("Nonexistent"), -1);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::UfbxImporterTest)
