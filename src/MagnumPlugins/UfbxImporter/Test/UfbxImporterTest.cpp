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
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/String.h>
#include <Magnum/FileCallback.h>
#include <Magnum/DebugTools/CompareMaterial.h>
#include <Magnum/Math/Angle.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>
#include <Magnum/Trade/PbrSpecularGlossinessMaterialData.h>
#include <Magnum/Trade/PbrClearCoatMaterialData.h>

#include "../UfbxMaterials.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

void matchStackTraceLine(Containers::StringView line)
{
    /* Manual parsing for regex " +[0-9]+:[A-Za-z0-9_]:.*"
       Originally used std::regex but that didn't compile on some
       platforms on CI.. */

    std::size_t i = 0;
    while (i < line.size()) {
        char c = line[i];
        if (c != ' ') break;
        ++i;
    }

    std::size_t lineNumberChars = 0;
    while (i < line.size()) {
        char c = line[i];
        if (!(c >= '0' && c <= '9')) break;
        ++i;
        ++lineNumberChars;
    }
    CORRADE_COMPARE_AS(lineNumberChars, 0, TestSuite::Compare::Greater);

    CORRADE_COMPARE_AS(i, line.size(), TestSuite::Compare::Less);
    CORRADE_COMPARE(line[i], ':');
    ++i;

    std::size_t functionChars = 0;
    while (i < line.size()) {
        char c = line[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) break;
        ++i;
        ++functionChars;
    }
    CORRADE_COMPARE_AS(functionChars, 0, TestSuite::Compare::Greater);

    CORRADE_COMPARE_AS(i, line.size(), TestSuite::Compare::Less);
    CORRADE_COMPARE(line[i], ':');
    ++i;
}

using FileCallbackFiles = std::unordered_map<std::string, Containers::Optional<Containers::Array<char>>>;
Containers::Optional<Containers::ArrayView<const char>> fileCallbackFunc(const std::string& filename, InputFileCallbackPolicy, void *user) {
    auto &files = *static_cast<FileCallbackFiles*>(user);

    Containers::Optional<Containers::Array<char>> &data = files[filename];

    Containers::String path = Utility::Path::join(UFBXIMPORTER_TEST_DIR, filename);
    if (!data && Utility::Path::exists(path))
        data = Utility::Path::read(path);

    if (data)
        return Containers::optional(Containers::ArrayView<const char>(*data));
    return {};
}

using namespace Math::Literals;
using namespace Containers::Literals;

constexpr struct {
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
    void light();
    void lightName();

    void geometricTransform();
    void geometricTransformPreserveRoot();
    void geometricTransformNoGeometric();
    void geometricTransformNoGeometricPreserveRoot();

    void materialMapping();

    void blenderMaterials();
    void materialMayaArnold();
    void materialMaxPhysical();
    void materialMaxPbrSpecGloss();
    void materialLayeredPbrTextures();
    void meshMaterials();
    void textureTransform();

    void imageEmbedded();
    void imageExternal();
    void imageFileCallback();
    void imageFileCallbackNotFound();
    void imageDeduplication();
    void imageNonExistentName();

    void objCube();
    void objCubeFileCallback();
    void objMissingMtl();
    void objMissingMtlFileCallback();

    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
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
              &UfbxImporterTest::fileCallbackEmptyVerbose});

    addTests({&UfbxImporterTest::scene,
              &UfbxImporterTest::mesh,
              &UfbxImporterTest::light,
              &UfbxImporterTest::lightName});

    addTests({&UfbxImporterTest::geometricTransform,
              &UfbxImporterTest::geometricTransformPreserveRoot,
              &UfbxImporterTest::geometricTransformNoGeometric,
              &UfbxImporterTest::geometricTransformNoGeometricPreserveRoot});

    addTests({&UfbxImporterTest::materialMapping});

    addTests({&UfbxImporterTest::blenderMaterials,
              &UfbxImporterTest::materialMayaArnold,
              &UfbxImporterTest::materialMaxPhysical,
              &UfbxImporterTest::materialMaxPbrSpecGloss,
              &UfbxImporterTest::materialLayeredPbrTextures,
              &UfbxImporterTest::meshMaterials,
              &UfbxImporterTest::textureTransform});

    addTests({&UfbxImporterTest::imageEmbedded,
              &UfbxImporterTest::imageExternal,
              &UfbxImporterTest::imageFileCallback,
              &UfbxImporterTest::imageFileCallbackNotFound,
              &UfbxImporterTest::imageDeduplication,
              &UfbxImporterTest::imageNonExistentName});

    addTests({&UfbxImporterTest::objCube,
              &UfbxImporterTest::objCubeFileCallback,
              &UfbxImporterTest::objMissingMtl,
              &UfbxImporterTest::objMissingMtlFileCallback});

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
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    importer->configuration().setValue<Long>("maxTemporaryMemory", data.maxMemory);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-default.fbx")), data.shouldLoad);
    if (data.shouldLoad) {
        CORRADE_COMPARE(out.str(), "");
    } else {
        CORRADE_COMPARE(out.str(), "Trade::UfbxImporter::openFile(): loading failed: Memory limit exceeded: temp\n");
    }
}

void UfbxImporterTest::maxResultMemory() {
    auto&& data = MaxMemoryData[testCaseInstanceId()];
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    importer->configuration().setValue<Long>("maxResultMemory", data.maxMemory);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-default.fbx")), data.shouldLoad);
    if (data.shouldLoad) {
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

    for (std::size_t i = 1; i < lines.size(); ++i) {
        CORRADE_ITERATION(lines[i]);
        matchStackTraceLine(lines[i]);
    }
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

    Int defaultScene = importer->defaultScene();
    CORRADE_COMPARE(defaultScene, 0);

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(scene->fieldCount(), 9);

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
    }

    {
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
    }

    {
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
    }

    {
        Long optionalId = importer->objectForName("ThisObjectDoesNotExist");
        CORRADE_COMPARE_AS(optionalId, 0, TestSuite::Compare::Less);
    }
}

void UfbxImporterTest::mesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "triangle.fbx")));

    CORRADE_COMPARE(importer->meshCount(), 1);

    /* FBX files don't have reliable mesh names, so go by index */
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
    CORRADE_COMPARE(scene->fieldCount(), 7);

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

    Containers::StridedArrayView1D<const UnsignedByte> visibilities = scene->field<UnsignedByte>(sceneFieldVisibility);
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
        CORRADE_COMPARE(translations[objectId], (Vector3d{0.0f,0.0f,0.0f}));
        CORRADE_COMPARE(visibilities[objectId], false);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{0.11f, 0.12f, 0.13f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f,0.0f,0.0f}));
    }

    {
        Long objectId = importer->objectForName("directionalLight1");
        CORRADE_COMPARE(importer->objectName(objectId), "directionalLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{1.0f,0.0f,0.0f}));
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Directional);
        CORRADE_COMPARE(light->color(), (Color3{0.21f, 0.22f, 0.23f}));
        CORRADE_COMPARE(light->intensity(), 2.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f,0.0f,0.0f}));
    }

    {
        Long objectId = importer->objectForName("pointLight1");
        CORRADE_COMPARE(importer->objectName(objectId), "pointLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{2.0f,0.0f,0.0f}));
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{0.31f, 0.32f, 0.33f}));
        CORRADE_COMPARE(light->intensity(), 3.0f);
        CORRADE_COMPARE(light->range(), Constants::inf());
        CORRADE_COMPARE(light->attenuation(), (Vector3{0.0f,1.0f,0.0f}));
    }

    {
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
    }

    {
        Long objectId = importer->objectForName("areaLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{4.0f,0.0f,0.0f}));
        CORRADE_COMPARE(importer->objectName(objectId), "areaLight1");
        CORRADE_COMPARE(visibilities[objectId], true);

        Containers::Array<UnsignedInt> lights = scene->lightsFor(objectId);
        CORRADE_COMPARE(lights.size(), 1);

        /* Area lights are not supported */
        Containers::Optional<LightData> light = importer->light(lights[0]);
        CORRADE_VERIFY(!light);
    }

    {
        Long objectId = importer->objectForName("volumeLight1");
        CORRADE_COMPARE(translations[objectId], (Vector3d{5.0f,0.0f,0.0f}));
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

void UfbxImporterTest::geometricTransform() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "geometric-transform.fbx")));

    /* Two real objects and two geometric transform helpers */
    CORRADE_COMPARE(importer->objectCount(), 4);
    CORRADE_COMPARE(importer->objectName(0), "Box001");
    CORRADE_COMPARE(importer->objectName(1), "Box002");
    CORRADE_COMPARE(importer->objectName(2), "");
    CORRADE_COMPARE(importer->objectName(3), "");

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::Translation));
    CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
    CORRADE_VERIFY(scene->hasField(SceneField::Scaling));

    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
        -1, 0, 0, 1,
    }), TestSuite::Compare::Container);
 
    /* The meshes should be parented under their geometric transform helpers */
    CORRADE_COMPARE_AS(scene->meshesMaterialsAsArray(), (Containers::arrayView<Containers::Pair<UnsignedInt, Containers::Pair<UnsignedInt, Int>>>({
        {2, {0, -1}},
        {3, {1, -1}},
    })), TestSuite::Compare::Container);

    const Containers::Pair<UnsignedInt, Containers::Triple<Vector3,Quaternion,Vector3>> referenceTrs[] = {
        /* Box001 */
        {0, {{0.0f, -10.0f, 0.0f},
             {{0.0f, 0.0f, -0.707107}, 0.707107f}, /* Euler (0, 0, -90) */
             {1.0f, 2.0f, 1.0f}}}, 
        /* Box002 */
        {1, {{0.0f, 0.0f, 20.0f},
             {{0.0f, 0.0f, -1.0f}, 0.0f}, /* Euler (0, 0, -180) */
             {1.0f, 0.5f, 1.0f}}},
        /* Box001 Geometric */
        {2, {{0.0f, 0.0f, 10.0f},
             {{0.0f, 0.707107f, 0.0f}, 0.707107f}, /* Euler (0, 90, 0) */
             {1.0f, 1.0f, 2.0f}}},
        /* Box002 Geometric */
        {3, {{10.0f, 0.0f, 0.0f},
             {},
             {1.0f, 1.0f, 1.0f}}}, 
    };
    CORRADE_COMPARE_AS(scene->translationsRotationsScalings3DAsArray(),
        Containers::arrayView(referenceTrs), TestSuite::Compare::Container);
}

void UfbxImporterTest::geometricTransformPreserveRoot() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("preserveRootNode", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "geometric-transform.fbx")));

    /* Two real objects, root, and two geometric transform helpers */
    CORRADE_COMPARE(importer->objectCount(), 5);
    CORRADE_COMPARE(importer->objectName(0), "");
    CORRADE_COMPARE(importer->objectName(1), "Box001");
    CORRADE_COMPARE(importer->objectName(2), "Box002");
    CORRADE_COMPARE(importer->objectName(3), "");
    CORRADE_COMPARE(importer->objectName(4), "");

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::Translation));
    CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
    CORRADE_VERIFY(scene->hasField(SceneField::Scaling));

    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
        -1, 0, 1, 1, 2,
    }), TestSuite::Compare::Container);
 
    /* The meshes should be parented under their geometric transform helpers */
    CORRADE_COMPARE_AS(scene->meshesMaterialsAsArray(), (Containers::arrayView<Containers::Pair<UnsignedInt, Containers::Pair<UnsignedInt, Int>>>({
        {3, {0, -1}},
        {4, {1, -1}},
    })), TestSuite::Compare::Container);

    const Containers::Pair<UnsignedInt, Containers::Triple<Vector3,Quaternion,Vector3>> referenceTrs[] = {
        /* (root) */
        {0, {{0.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}}},
        /* Box001 */
        {1, {{0.0f, -10.0f, 0.0f},
             {{0.0f, 0.0f, -0.707107}, 0.707107f}, /* Euler (0, 0, -90) */
             {1.0f, 2.0f, 1.0f}}}, 
        /* Box002 */
        {2, {{0.0f, 0.0f, 20.0f},
             {{0.0f, 0.0f, -1.0f}, 0.0f}, /* Euler (0, 0, -180) */
             {1.0f, 0.5f, 1.0f}}},
        /* Box001 Geometric */
        {3, {{0.0f, 0.0f, 10.0f},
             {{0.0f, 0.707107f, 0.0f}, 0.707107f}, /* Euler (0, 90, 0) */
             {1.0f, 1.0f, 2.0f}}},
        /* Box002 Geometric */
        {4, {{10.0f, 0.0f, 0.0f},
             {},
             {1.0f, 1.0f, 1.0f}}}, 
    };
    CORRADE_COMPARE_AS(scene->translationsRotationsScalings3DAsArray(),
        Containers::arrayView(referenceTrs), TestSuite::Compare::Container);
}

void UfbxImporterTest::geometricTransformNoGeometric() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("geometricTransformNodes", false);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "geometric-transform.fbx")));

    /* Two real objects only */
    CORRADE_COMPARE(importer->objectCount(), 2);
    CORRADE_COMPARE(importer->objectName(0), "Box001");
    CORRADE_COMPARE(importer->objectName(1), "Box002");

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::Translation));
    CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
    CORRADE_VERIFY(scene->hasField(SceneField::Scaling));

    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
        -1, 0,
    }), TestSuite::Compare::Container);
 
    /* The meshes should be parented under their geometric transform helpers */
    CORRADE_COMPARE_AS(scene->meshesMaterialsAsArray(), (Containers::arrayView<Containers::Pair<UnsignedInt, Containers::Pair<UnsignedInt, Int>>>({
        {0, {0, -1}},
        {1, {1, -1}},
    })), TestSuite::Compare::Container);

    const Containers::Pair<UnsignedInt, Containers::Triple<Vector3,Quaternion,Vector3>> referenceTrs[] = {
        /* Box001 */
        {0, {{0.0f, -10.0f, 0.0f},
             {{0.0f, 0.0f, -0.707107}, 0.707107f}, /* Euler (0, 0, -90) */
             {1.0f, 2.0f, 1.0f}}}, 
        /* Box002 */
        {1, {{0.0f, 0.0f, 20.0f},
             {{0.0f, 0.0f, -1.0f}, 0.0f}, /* Euler (0, 0, -180) */
             {1.0f, 0.5f, 1.0f}}},
    };
    CORRADE_COMPARE_AS(scene->translationsRotationsScalings3DAsArray(),
        Containers::arrayView(referenceTrs), TestSuite::Compare::Container);
}

void UfbxImporterTest::geometricTransformNoGeometricPreserveRoot() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    importer->configuration().setValue("geometricTransformNodes", false);
    importer->configuration().setValue("preserveRootNode", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "geometric-transform.fbx")));

    /* Two real objects and root */
    CORRADE_COMPARE(importer->objectCount(), 3);
    CORRADE_COMPARE(importer->objectName(0), "");
    CORRADE_COMPARE(importer->objectName(1), "Box001");
    CORRADE_COMPARE(importer->objectName(2), "Box002");

    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_VERIFY(scene);

    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::Translation));
    CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
    CORRADE_VERIFY(scene->hasField(SceneField::Scaling));

    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
        -1, 0, 1,
    }), TestSuite::Compare::Container);
 
    /* The meshes should be parented under their geometric transform helpers */
    CORRADE_COMPARE_AS(scene->meshesMaterialsAsArray(), (Containers::arrayView<Containers::Pair<UnsignedInt, Containers::Pair<UnsignedInt, Int>>>({
        {1, {0, -1}},
        {2, {1, -1}},
    })), TestSuite::Compare::Container);

    const Containers::Pair<UnsignedInt, Containers::Triple<Vector3,Quaternion,Vector3>> referenceTrs[] = {
        /* (root) */
        {0, {{0.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}}},
        /* Box001 */
        {1, {{0.0f, -10.0f, 0.0f},
             {{0.0f, 0.0f, -0.707107}, 0.707107f}, /* Euler (0, 0, -90) */
             {1.0f, 2.0f, 1.0f}}}, 
        /* Box002 */
        {2, {{0.0f, 0.0f, 20.0f},
             {{0.0f, 0.0f, -1.0f}, 0.0f}, /* Euler (0, 0, -180) */
             {1.0f, 0.5f, 1.0f}}},
    };
    CORRADE_COMPARE_AS(scene->translationsRotationsScalings3DAsArray(),
        Containers::arrayView(referenceTrs), TestSuite::Compare::Container);
}

void UfbxImporterTest::materialMapping() {
    /* Make sure all the ufbx maps are used at least once and that there are
       no duplicate Magnum attributes without MaterialExclusionGroup */

    Containers::ArrayView<const MaterialMapping> mappingLists[] = {
        Containers::arrayView(materialMappingFbx),
        Containers::arrayView(materialMappingPbr),
    };

    Containers::StaticArray<UfbxMaterialLayerCount, std::unordered_map<std::string, MaterialExclusionGroup>> usedAttributeNames;

    Containers::BitArray usedUfbxMaps[2] = {
        Containers::BitArray{ ValueInit, UFBX_MATERIAL_FBX_MAP_COUNT },
        Containers::BitArray{ ValueInit, UFBX_MATERIAL_PBR_MAP_COUNT },
    };

    for(UnsignedInt type = 0; type < 2; ++type) {
        for (const MaterialMapping &mapping : mappingLists[type]) {
            std::size_t layer = std::size_t(mapping.layer);

            if (mapping.valueMap >= 0)
                usedUfbxMaps[type].set(std::size_t(mapping.valueMap));
            if (mapping.factorMap >= 0)
                usedUfbxMaps[type].set(std::size_t(mapping.factorMap));

            /* Copy to std::string so we don't do unnecessary conversions on
               lookups, also this is far from performance critical */
            std::string attribute = mapping.attribute;
            std::string textureAttribute = mapping.textureAttribute;

            if (!attribute.empty()) {
                CORRADE_ITERATION(attribute);

                CORRADE_COMPARE_AS(mapping.attributeType, MaterialAttributeType{}, TestSuite::Compare::NotEqual);

                switch (mapping.attributeType) {
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
                if (found != usedAttributeNames[layer].end()) {
                    /* If we have a duplicate material attribute name it must
                       be defined under the same exclusion group */
                    CORRADE_VERIFY(mapping.exclusionGroup != MaterialExclusionGroup{});
                    CORRADE_COMPARE(UnsignedInt(mapping.exclusionGroup), UnsignedInt(found->second));
                } else {
                    usedAttributeNames[layer].insert({ attribute, mapping.exclusionGroup });
                }

                if (textureAttribute.empty())
                    textureAttribute = attribute + "Texture";
            }

            if (!textureAttribute.empty() && textureAttribute != MaterialMapping::DisallowTexture()) {
                CORRADE_ITERATION(textureAttribute);

                auto found = usedAttributeNames[layer].find(textureAttribute);
                if (found != usedAttributeNames[layer].end()) {
                    /* If we have a duplicate material attribute name it must
                       be defined under the same exclusion group */
                    CORRADE_VERIFY(mapping.exclusionGroup != MaterialExclusionGroup{});
                    CORRADE_COMPARE(UnsignedInt(mapping.exclusionGroup), UnsignedInt(found->second));
                } else {
                    usedAttributeNames[layer].insert({ textureAttribute, mapping.exclusionGroup });
                }
            }
        }
    }

    /* Make sure all the ufbx maps are accounted for */
    /* @todo: Could we fail with an index in the message here? */
    for(UnsignedInt i = 0; i < UFBX_MATERIAL_FBX_MAP_COUNT; ++i) {
        CORRADE_ITERATION(i);
        CORRADE_VERIFY(usedUfbxMaps[0][i]);
    }
    for(UnsignedInt i = 0; i < UFBX_MATERIAL_PBR_MAP_COUNT; ++i) {
        CORRADE_ITERATION(i);
        CORRADE_VERIFY(usedUfbxMaps[1][i]);
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
        }

        {
            const PbrMetallicRoughnessMaterialData& pbr = material->as<PbrMetallicRoughnessMaterialData>();
            CORRADE_COMPARE(pbr.baseColor(), (Color4{0.8f, 0.8f, 0.8f, 1.0f}));
            CORRADE_COMPARE(pbr.emissiveColor(), (Color3{0.0f, 0.0f, 0.0f}));
            CORRADE_COMPARE(pbr.metalness(), 0.0f);
            CORRADE_COMPARE(pbr.roughness(), 0.5f);
        }
    }

    {
        Containers::Optional<MaterialData> material = importer->material("Top");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::PbrMetallicRoughness);

        {
            const PhongMaterialData& phong = material->as<PhongMaterialData>();
            CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.1f, 0.2f, 0.3f, 1.0f}));
            CORRADE_COMPARE(phong.shininess(), 4.0f);
        }

        {
            const PbrMetallicRoughnessMaterialData& pbr = material->as<PbrMetallicRoughnessMaterialData>();
            CORRADE_COMPARE(pbr.baseColor(), (Color4{0.1f, 0.2f, 0.3f, 0.9f}));
            CORRADE_COMPARE(pbr.emissiveColor(), (Color3{0.4f, 0.5f, 0.6f}));
            CORRADE_COMPARE(pbr.metalness(), 0.7f);
            CORRADE_COMPARE(pbr.roughness(), 0.8f);
        }
    }

    {
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
        }

        {
            const PbrMetallicRoughnessMaterialData& pbr = material->as<PbrMetallicRoughnessMaterialData>();

            {
                CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::BaseColorTexture));
                Containers::Optional<TextureData> texture = importer->texture(pbr.baseColorTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_diffuse.png");
            }

            {
                CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::EmissiveTexture));
                Containers::Optional<TextureData> texture = importer->texture(pbr.emissiveTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_emissive.png");
            }

            {
                CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::RoughnessTexture));
                Containers::Optional<TextureData> texture = importer->texture(pbr.roughnessTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_roughness.png");
            }

            {
                CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::MetalnessTexture));
                Containers::Optional<TextureData> texture = importer->texture(pbr.metalnessTexture());
                CORRADE_VERIFY(texture);
                CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_metallic.png");
            }
        }

        {
            CORRADE_VERIFY(material->hasAttribute("opacityTexture"));
            UnsignedInt textureId = material->attribute<UnsignedInt>("opacityTexture");
            Containers::Optional<TextureData> texture = importer->texture(textureId);
            CORRADE_VERIFY(texture);
            CORRADE_COMPARE(importer->image2DName(texture->image()), "blender-materials.fbm\\checkerboard_transparency.png");
        }
    }
}

void UfbxImporterTest::materialMayaArnold() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "maya-material-arnold.fbx")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<MaterialData> material = importer->material("aiStandardSurface1");
    CORRADE_VERIFY(material);

    MaterialData reference{MaterialType::PbrMetallicRoughness|MaterialType::PbrClearCoat, {
        {MaterialAttribute::BaseColor, Color4{0.02f, 0.03f, 0.04f, 1.0f} * factor(0.01f)},
        {"diffuseRoughness", 0.05f},
        {MaterialAttribute::Metalness, 0.06f},
        {MaterialAttribute::SpecularColor, Color4{0.08f, 0.09f, 0.10f, 1.0f} * factor(0.07f)},
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
        {MaterialAttribute::BaseColor, Color4{0.02f, 0.03f, 0.04f, 0.05f} * factor(0.01f)},
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
    for (UnsignedInt id = 0; id < layerCount; ++id) {
        Containers::StringView name = material->layerName(id);
        if (name.isEmpty()) {
            arrayAppend(baseLayerIds, id);
        } else if (name == "ClearCoat") {
            arrayAppend(coatLayerIds, id);
        } else if (name == "sheen") {
            arrayAppend(sheenLayerIds, id);
        }
    }

    CORRADE_COMPARE(baseLayerIds.size(), 3);
    CORRADE_COMPARE(coatLayerIds.size(), 3);
    CORRADE_COMPARE(sheenLayerIds.size(), 2);

    struct AttributeTextureLayer {
        UnsignedInt layer;
        Containers::StringView attribute;
        Containers::StringView imageName;
        Containers::StringView blendMode;
        Float blendAlpha;
    };

    AttributeTextureLayer attributeTextureLayers[] = {
        { baseLayerIds[0], "BaseColorTexture", "tex-red.png", "over", 1.0f },
        { baseLayerIds[1], "BaseColorTexture", "tex-green.png", "over", 0.25f },
        { baseLayerIds[2], "BaseColorTexture", "tex-blue.png", "additive", 0.75f },
        { coatLayerIds[0], "colorTexture", "tex-cyan.png", "over", 1.0f },
        { coatLayerIds[1], "colorTexture", "tex-pink.png", "softLight", 0.33f },
        { coatLayerIds[2], "colorTexture", "tex-yellow.png", "multiply", 0.66f },
        { coatLayerIds[0], "RoughnessTexture", "tex-black.png", "over", 1.0f },
        { coatLayerIds[1], "RoughnessTexture", "tex-white.png", "screen", 0.5f },
        { sheenLayerIds[0], "LayerFactorTexture", "tex-black.png", "over", 1.0f },
        { sheenLayerIds[1], "LayerFactorTexture", "tex-white.png", "replace", 0.5f },
    };

    for (UnsignedInt i = 0; i < Containers::arraySize(attributeTextureLayers); ++i) {
        CORRADE_ITERATION(i);

        const AttributeTextureLayer &texLayer = attributeTextureLayers[i];
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
        for (UnsignedInt i = 0; i < vertexCount; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE(normal[i].z(), 0.0f);
        }
    }

    /* 'Top' material is on the top face (+Z in Blender local) */
    {
        Containers::Optional<MeshData> mesh = importer->mesh(1);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->indexCount(), 2*3);

        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Normal), 1);

        UnsignedInt vertexCount = mesh->vertexCount();
        Containers::StridedArrayView1D<const Vector3> position = mesh->attribute<Vector3>(MeshAttribute::Position);
        Containers::StridedArrayView1D<const Vector3> normal = mesh->attribute<Vector3>(MeshAttribute::Normal);
        for (UnsignedInt i = 0; i < vertexCount; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE(position[i].z(), 1.0f);
            CORRADE_COMPARE(normal[i], (Vector3{0.0f,0.0f,1.0f}));
        }
    }

    /* 'Textures' material is on the bottom face (-Z in Blender local) */
    {
        Containers::Optional<MeshData> mesh = importer->mesh(2);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->indexCount(), 2*3);

        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Position), 1);
        CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Normal), 1);

        UnsignedInt vertexCount = mesh->vertexCount();
        Containers::StridedArrayView1D<const Vector3> position = mesh->attribute<Vector3>(MeshAttribute::Position);
        Containers::StridedArrayView1D<const Vector3> normal = mesh->attribute<Vector3>(MeshAttribute::Normal);
        for (UnsignedInt i = 0; i < vertexCount; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE(position[i].z(), -1.0f);
            CORRADE_COMPARE(normal[i], (Vector3{0.0f,0.0f,-1.0f}));
        }
    }

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

void UfbxImporterTest::imageEmbedded() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "blender-materials.fbx")));

    UnsignedInt imageCount = importer->image2DCount();
    CORRADE_COMPARE(imageCount, 5);

    struct TestImage {
        Containers::StringView name;
        Color4ub topLeftPixelColor;
        Color4ub bottomLeftPixelColor;
    };

    TestImage testImages[] = {
        { "blender-materials.fbm\\checkerboard_diffuse.png",      0x7c5151ff_rgba, 0x3d2828ff_rgba },
        { "blender-materials.fbm\\checkerboard_emissive.png",     0x44743bff_rgba, 0x223a1dff_rgba },
        { "blender-materials.fbm\\checkerboard_roughness.png",    0x5f3228ff_rgba, 0x2f1914ff_rgba },
        { "blender-materials.fbm\\checkerboard_metallic.png",     0x63422cff_rgba, 0x312116ff_rgba },
        { "blender-materials.fbm\\checkerboard_transparency.png", 0x407177ff_rgba, 0x1f373aff_rgba },
    };

    for (const TestImage& testImage : testImages) {
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

    Containers::Pair<Containers::StringView, Color3ub> imageColors[] = {
        { "tex-red.png",    0xff0000_rgb },
        { "tex-green.png",  0x00ff00_rgb },
        { "tex-blue.png",   0x0000ff_rgb },
        { "tex-cyan.png",   0x00ffff_rgb },
        { "tex-pink.png",   0xff00ff_rgb },
        { "tex-yellow.png", 0xffff00_rgb },
        { "tex-black.png",  0x000000_rgb },
        { "tex-white.png",  0xffffff_rgb },
    };

    CORRADE_COMPARE(importer->image2DCount(), 8);
    for (UnsignedInt i = 0; i < Containers::arraySize(imageColors); ++i) {
        CORRADE_ITERATION(i);

        Containers::StringView imageName = imageColors[i].first();
        Color4ub imageColor = imageColors[i].second();

        Containers::Optional<ImageData2D> image = importer->image2D(imageName);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{1, 1}));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], imageColor);
    }
}

void UfbxImporterTest::imageFileCallback() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    FileCallbackFiles files;
    importer->setFileCallback(&fileCallbackFunc, &files);

    Containers::Pair<Containers::StringView, Color3ub> imageColors[] = {
        { "tex-red.png",    0xff0000_rgb },
        { "tex-green.png",  0x00ff00_rgb },
        { "tex-blue.png",   0x0000ff_rgb },
        { "tex-cyan.png",   0x00ffff_rgb },
        { "tex-pink.png",   0xff00ff_rgb },
        { "tex-yellow.png", 0xffff00_rgb },
        { "tex-black.png",  0x000000_rgb },
        { "tex-white.png",  0xffffff_rgb },
    };

    CORRADE_VERIFY(importer->openFile("layered-pbr-textures.fbx"));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_COMPARE(importer->image2DCount(), 8);
    for (UnsignedInt i = 0; i < Containers::arraySize(imageColors); ++i) {
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
    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy, UnsignedInt &callCount) {
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

    Containers::Pair<Containers::StringView, Containers::StringView> textureToImage[] = {
        { "Map #2", "tex-red.png" },
        { "Map #3", "tex-green.png" },
        { "Map #4", "tex-blue.png" },
        { "Map #9", "tex-cyan.png" },
        { "Map #10", "tex-pink.png" },
        { "Map #11", "tex-yellow.png" },
        { "Map #14", "tex-black.png" },
        { "Map #15", "tex-white.png" },
        { "Map #16", "tex-black.png" },
        { "Map #17", "tex-white.png" },
    };

    CORRADE_COMPARE(importer->image2DCount(), 8);

    for (UnsignedInt i = 0; i < Containers::arraySize(textureToImage); ++i) {
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


void UfbxImporterTest::objCube() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "cube.obj")));
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
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(UFBXIMPORTER_TEST_DIR, "missing-mtl.obj")));
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
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UfbxImporter");

    FileCallbackFiles files;
    importer->setFileCallback(&fileCallbackFunc, &files);

    CORRADE_VERIFY(importer->openFile("missing-mtl.obj"));
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

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::UfbxImporterTest)
