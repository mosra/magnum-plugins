/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2018 Tobias Stein <stein.tobi@t-online.de>
    Copyright © 2018, 2020 Jonathan Hale <squareys@googlemail.com>

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
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/FormatStl.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/Array.h>
#include <Magnum/FileCallback.h>
#include <Magnum/Mesh.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/CubicHermite.h>
#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/AnimationData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/ObjectData3D.h>
#include <Magnum/Trade/FlatMaterialData.h>
#include <Magnum/Trade/PbrClearCoatMaterialData.h>
#include <Magnum/Trade/PbrMetallicRoughnessMaterialData.h>
#include <Magnum/Trade/PbrSpecularGlossinessMaterialData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/SkinData.h>
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
    void openExternalDataWrongSize();

    void animation();
    void animationInvalid();

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
    void lightInvalid();
    void lightMissingType();
    void lightMissingSpot();

    void scene();
    void sceneEmpty();
    void sceneNoDefault();
    void sceneInvalidObject();
    void sceneInvalidMesh();
    void sceneInvalidScene();
    void sceneInvalidDefaultScene();

    void objectTransformation();
    void objectTransformationQuaternionNormalizationEnabled();
    void objectTransformationQuaternionNormalizationDisabled();

    void skin();
    void skinInvalid();
    void skinNoJointsProperty();

    void mesh();
    void meshAttributeless();
    void meshIndexed();
    void meshIndexedAttributeless();
    void meshColors();
    void meshCustomAttributes();
    void meshCustomAttributesNoFileOpened();
    void meshMultiplePrimitives();
    void meshPrimitivesTypes();
    /* This is THE ONE AND ONLY OOB check done by tinygltf, so it fails right
       at openData() and thus has to be separate. Everything else is not done
       by it. */
    void meshIndexAccessorOutOfBounds();
    void meshInvalid();

    void materialPbrMetallicRoughness();
    void materialPbrSpecularGlossiness();
    void materialCommon();
    void materialUnlit();
    void materialClearCoat();
    void materialPhongFallback();

    void materialInvalid();
    void materialTexCoordFlip();

    void texture();
    void textureDefaultSampler();
    void textureEmptySampler();
    void textureMissingSource();

    void imageEmbedded();
    void imageExternal();
    void imageExternalNotFound();
    void imageExternalNoPathNoCallback();

    void imageBasis();
    void imageMipLevels();

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
    const char* message;
} AnimationInvalidData[]{
    {"unexpected time type", "time track has unexpected type 4/5126"},
    {"unsupported interpolation type", "unsupported interpolation QUADRATIC"},
    {"unexpected translation type", "translation track has unexpected type 4/5126"},
    {"unexpected rotation type", "rotation track has unexpected type 65/5126"},
    {"unexpected scaling type", "scaling track has unexpected type 4/5126"},
    {"unsupported path", "unsupported track target color"},
    {"sampler index out of bounds", "sampler 1 out of bounds for 1 samplers"},
    {"node index out of bounds", "target node 2 out of bounds for 2 nodes"},
    {"sampler input accessor index out of bounds", "accessor 3 out of bounds for 3 accessors"},
    {"sampler output accessor index out of bounds", "accessor 3 out of bounds for 3 accessors"}
};

constexpr struct {
    const char* name;
    const char* message;
} LightInvalidData[]{
    {"unknown type", "invalid light type what"},
    {"directional with range", "range can't be defined for a directional light"},
    {"spot with too small inner angle", "inner and outer cone angle Deg(-0.572958) and Deg(45) out of allowed bounds"},
    /* These are kinda silly (not sure why we should limit to 90° and why inner
       can't be the same as outer), but let's follow the spec */
    {"spot with too large outer angle", "inner and outer cone angle Deg(0) and Deg(90.5273) out of allowed bounds"},
    {"spot with inner angle same as outer", "inner and outer cone angle Deg(14.3239) and Deg(14.3239) out of allowed bounds"},
    {"four color values", "expected three values for a color, got 4"}
};

constexpr struct {
    const char* name;
    const char* message;
} SkinInvalidData[]{
    {"no joints", "skin has no joints"},
    {"joint out of bounds", "target node 2 out of bounds for 2 nodes"},
    {"accessor out of bounds", "accessor 4 out of bounds for 4 accessors"},
    {"wrong accessor type", "inverse bind matrices have unexpected type 35/5126"},
    {"wrong accessor component type", "inverse bind matrices have unexpected type 36/5123"},
    {"wrong accessor count", "invalid inverse bind matrix count, expected 2 but got 3"}
};

constexpr struct {
    const char* name;
    MeshPrimitive primitive;
    MeshIndexType indexType;
    VertexFormat positionFormat;
    VertexFormat normalFormat, tangentFormat;
    VertexFormat colorFormat;
    VertexFormat textureCoordinateFormat, objectIdFormat;
    const char* objectIdAttribute;
} MeshPrimitivesTypesData[]{
    {"positions byte, color4 unsigned short, texcoords normalized unsigned byte; triangle strip",
        MeshPrimitive::TriangleStrip, MeshIndexType{},
        VertexFormat::Vector3b,
        VertexFormat{}, VertexFormat{},
        VertexFormat::Vector4usNormalized,
        VertexFormat::Vector2ubNormalized, VertexFormat{}, nullptr},
    {"positions short, colors unsigned byte, texcoords normalized unsigned short; lines",
        MeshPrimitive::Lines, MeshIndexType{},
        VertexFormat::Vector3s,
        VertexFormat{}, VertexFormat{},
        VertexFormat::Vector3ubNormalized,
        VertexFormat::Vector2usNormalized, VertexFormat{}, nullptr},
    {"positions unsigned byte, normals byte, texcoords short; indices unsigned int; line loop",
        MeshPrimitive::LineLoop, MeshIndexType::UnsignedInt,
        VertexFormat::Vector3ub,
        VertexFormat::Vector3bNormalized, VertexFormat{},
        VertexFormat{},
        VertexFormat::Vector2s, VertexFormat{}, nullptr},
    {"positions unsigned short, normals short, texcoords byte; indices unsigned byte; triangle fan",
        MeshPrimitive::TriangleFan, MeshIndexType::UnsignedByte,
        VertexFormat::Vector3us,
        VertexFormat::Vector3sNormalized, VertexFormat{},
        VertexFormat{},
        VertexFormat::Vector2b, VertexFormat{}, nullptr},
    {"positions normalized unsigned byte, tangents short, texcoords normalized short; indices unsigned short; line strip",
        MeshPrimitive::LineStrip, MeshIndexType::UnsignedShort,
        VertexFormat::Vector3ubNormalized,
        VertexFormat{}, VertexFormat::Vector4sNormalized,
        VertexFormat{},
        VertexFormat::Vector2sNormalized, VertexFormat{}, nullptr},
    {"positions normalized short, texcoords unsigned byte, tangents byte; triangles",
        MeshPrimitive::Triangles, MeshIndexType{},
        VertexFormat::Vector3sNormalized,
        VertexFormat{}, VertexFormat::Vector4bNormalized,
        VertexFormat{},
        VertexFormat::Vector2ub, VertexFormat{}, nullptr},
    {"positions normalized unsigned short, texcoords normalized byte, objectid unsigned short",
        MeshPrimitive::Triangles, MeshIndexType{},
        VertexFormat::Vector3usNormalized,
        VertexFormat{}, VertexFormat{},
        VertexFormat{},
        VertexFormat::Vector2bNormalized, VertexFormat::UnsignedShort, nullptr},
    {"positions normalized byte, texcoords unsigned short, objectid unsigned byte",
        MeshPrimitive::Triangles, MeshIndexType{},
        VertexFormat::Vector3bNormalized,
        VertexFormat{}, VertexFormat{},
        VertexFormat{},
        VertexFormat::Vector2us, VertexFormat::UnsignedByte, "OBJECTID"}
};

constexpr struct {
    const char* name;
    const char* message;
} MeshInvalidData[]{
    {"invalid primitive", "unrecognized primitive 666"},
    {"different vertex count for each accessor", "mismatched vertex count for attribute TEXCOORD_1, expected 3 but got 4"},
    {"unexpected position type", "unexpected POSITION type 2"},
    {"unsupported position component type", "unsupported POSITION component type unnormalized 5130"},
    {"unexpected normal type", "unexpected NORMAL type 2"},
    {"unsupported normal component type", "unsupported NORMAL component type unnormalized 5130"},
    {"unexpected tangent type", "unexpected TANGENT type 3"},
    {"unsupported tangent component type", "unsupported TANGENT component type unnormalized 5120"},
    {"unexpected texcoord type", "unexpected TEXCOORD type 3"},
    {"unsupported texcoord component type", "unsupported TEXCOORD component type normalized 5125"},
    {"unexpected color type", "unexpected COLOR type 2"},
    {"unsupported color component type", "unsupported COLOR component type unnormalized 5120"},
    {"unexpected object id type", "unexpected object ID type 2"},
    {"unsupported object id component type", "unsupported object ID component type unnormalized 5124"},
    {"unexpected index type", "unexpected index type 2"},
    {"unsupported index component type", "unexpected index component type 5124"},
    {"normalized index type", "index type can't be normalized"},
    {"strided index view", "index bufferView is not contiguous"},
    {"accessor type size larger than buffer stride", "16-byte type defined by accessor 10 can't fit into bufferView 0 stride of 12"},
    {"accessor count larger than buffer size", "accessor 11 needs 33 bytes but bufferView 1 has only 32"},
    {"buffer view range out of bounds", "bufferView 2 needs 72 bytes but buffer 0 has only 68"},
    {"buffer index out of bounds", "buffer 1 out of bounds for 1 buffers"},
    {"buffer view index out of bounds", "bufferView 4 out of bounds for 4 views"},
    {"normalized float", "floating-point component types can't be normalized"},
    {"non-normalized byte matrix", "unsupported matrix component type unnormalized 5120"},
    {"accessor index out of bounds", "accessor 17 out of bounds for 17 accessors"}
};

constexpr struct {
    const char* name;
    const char* message;
} MaterialInvalidData[]{
    {"unknown alpha mode", "unknown alpha mode WAT"},
    {"invalid texture index pbrMetallicRoughness base color", "baseColorTexture index 2 out of bounds for 2 textures"},
    {"invalid texture index pbrMetallicRoughness metallic/roughness", "metallicRoughnessTexture index 2 out of bounds for 2 textures"},
    {"invalid texture index pbrSpecularGlossiness diffuse", "diffuseTexture index 2 out of bounds for 2 textures"},
    {"invalid texture index pbrSpecularGlossiness specular", "specularGlossinessTexture index 2 out of bounds for 2 textures"},
    {"invalid texture index normal", "normalTexture index 2 out of bounds for 2 textures"},
    {"invalid texture index occlusion", "occlusionTexture index 2 out of bounds for 2 textures"},
    {"invalid texture index emissive", "emissiveTexture index 2 out of bounds for 2 textures"},
    {"invalid texture index clearcoat factor", "clearcoatTexture index 2 out of bounds for 2 textures"},
    {"invalid texture index clearcoat roughness", "clearcoatRoughnessTexture index 2 out of bounds for 2 textures"},
    {"invalid texture index clearcoat normal", "clearcoatNormalTexture index 2 out of bounds for 2 textures"},
};

constexpr struct {
    const char* name;
    Int idOffset;
    const char* message;
} SceneInvalidObjectData[]{
    {"camera out of bounds", 0, "camera index 1 out of bounds for 1 cameras"},
    {"child out of bounds", 0, "child index 7 out of bounds for 7 nodes"},
    {"material out of bounds", 0, "material index 4 out of bounds for 4 materials"},
    {"material in a multi-primitive mesh out of bounds", 1, "material index 5 out of bounds for 4 materials"},
    {"skin out of bounds", 0, "skin index 3 out of bounds for 3 skins"},
    /* The skin should be checked for both duplicates of the primitive */
    {"skin for a multi-primitive mesh out of bounds", 0, "skin index 3 out of bounds for 3 skins"},
    {"skin for a multi-primitive mesh out of bounds", 1, "skin index 3 out of bounds for 3 skins"},
    {"light out of bounds", 0, "light index 2 out of bounds for 2 lights"}
};

constexpr struct {
    const char* name;
    const char* fileName;
    const char* meshName;
    bool flipInMaterial;
    bool hasTextureTransformation;
} MaterialTexCoordFlipData[]{
    {"no transform",
        "material-texcoord-flip.gltf", "float", false, false},
    {"no transform",
        "material-texcoord-flip.gltf", "float", true, false},
    {"identity transform",
        "material-texcoord-flip.gltf", "float", false, true},
    {"identity transform",
        "material-texcoord-flip.gltf", "float", true, true},
    {"transform from normalized unsigned byte",
        "material-texcoord-flip.gltf",
        "normalized unsigned byte", false, true},
    {"transform from normalized unsigned byte",
        "material-texcoord-flip.gltf",
        "normalized unsigned byte", true, true},
    {"transform from normalized unsigned short",
        "material-texcoord-flip.gltf",
        "normalized unsigned short", false, true},
    {"transform from normalized unsigned short",
        "material-texcoord-flip.gltf",
        "normalized unsigned short", true, true},
    {"transform from normalized signed integer",
        "material-texcoord-flip-unnormalized.gltf",
        "normalized signed integer", false, true},
    {"transform from normalized signed integer",
        "material-texcoord-flip-unnormalized.gltf",
        "normalized signed integer", true, true},
    {"transform from signed integer",
        "material-texcoord-flip-unnormalized.gltf",
        "signed integer", false, true},
    {"transform from signed integer",
        "material-texcoord-flip-unnormalized.gltf",
        "signed integer", true, true},
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
                       &TinyGltfImporterTest::openExternalDataNoPathNoCallback,
                       &TinyGltfImporterTest::openExternalDataWrongSize},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&TinyGltfImporterTest::animation},
                      Containers::arraySize(MultiFileData));

    addInstancedTests({&TinyGltfImporterTest::animationInvalid},
        Containers::arraySize(AnimationInvalidData));

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

                       &TinyGltfImporterTest::light},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&TinyGltfImporterTest::lightInvalid},
        Containers::arraySize(LightInvalidData));

    addTests({&TinyGltfImporterTest::lightMissingType,
              &TinyGltfImporterTest::lightMissingSpot});

    addInstancedTests({&TinyGltfImporterTest::scene,
                       &TinyGltfImporterTest::sceneEmpty,
                       &TinyGltfImporterTest::sceneNoDefault},
                      Containers::arraySize(SingleFileData));

    addTests({&TinyGltfImporterTest::sceneInvalidMesh});

    addInstancedTests({&TinyGltfImporterTest::sceneInvalidObject},
        Containers::arraySize(SceneInvalidObjectData));

    addTests({&TinyGltfImporterTest::sceneInvalidScene,
              &TinyGltfImporterTest::sceneInvalidDefaultScene});

    addInstancedTests({&TinyGltfImporterTest::objectTransformation},
                      Containers::arraySize(SingleFileData));

    addTests({&TinyGltfImporterTest::objectTransformationQuaternionNormalizationEnabled,
              &TinyGltfImporterTest::objectTransformationQuaternionNormalizationDisabled});

    addInstancedTests({&TinyGltfImporterTest::skin},
        Containers::arraySize(MultiFileData));

    addInstancedTests({&TinyGltfImporterTest::skinInvalid},
        Containers::arraySize(SkinInvalidData));

    addTests({&TinyGltfImporterTest::skinNoJointsProperty});

    addInstancedTests({&TinyGltfImporterTest::mesh},
                      Containers::arraySize(MultiFileData));

    addTests({&TinyGltfImporterTest::meshAttributeless,
              &TinyGltfImporterTest::meshIndexed,
              &TinyGltfImporterTest::meshIndexedAttributeless,
              &TinyGltfImporterTest::meshColors,
              &TinyGltfImporterTest::meshCustomAttributes,
              &TinyGltfImporterTest::meshCustomAttributesNoFileOpened,
              &TinyGltfImporterTest::meshMultiplePrimitives});

    addInstancedTests({&TinyGltfImporterTest::meshPrimitivesTypes},
        Containers::arraySize(MeshPrimitivesTypesData));

    addTests({&TinyGltfImporterTest::meshIndexAccessorOutOfBounds});

    addInstancedTests({&TinyGltfImporterTest::meshInvalid},
        Containers::arraySize(MeshInvalidData));

    addTests({&TinyGltfImporterTest::materialPbrMetallicRoughness,
              &TinyGltfImporterTest::materialPbrSpecularGlossiness,
              &TinyGltfImporterTest::materialCommon,
              &TinyGltfImporterTest::materialUnlit,
              &TinyGltfImporterTest::materialClearCoat,
              &TinyGltfImporterTest::materialPhongFallback});

    addInstancedTests({&TinyGltfImporterTest::materialInvalid},
        Containers::arraySize(MaterialInvalidData));

    addInstancedTests({&TinyGltfImporterTest::materialTexCoordFlip},
        Containers::arraySize(MaterialTexCoordFlipData));

    addInstancedTests({&TinyGltfImporterTest::texture,
                       &TinyGltfImporterTest::textureDefaultSampler,
                       &TinyGltfImporterTest::textureEmptySampler},
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

    addTests({&TinyGltfImporterTest::imageMipLevels});

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
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(TINYGLTFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    _manager.setPluginDirectory({});
    #endif
    #ifdef BASISIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(BASISIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
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

void TinyGltfImporterTest::openExternalDataWrongSize() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    {
        CORRADE_EXPECT_FAIL_IF(data.suffix == std::string{".glb"},
            "tinygltf doesn't check for correct buffer size in GLBs.");
        CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
            "buffer-wrong-size" + std::string{data.suffix})));
        CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: File size mismatch : external-data.bin, requestedBytes 6, but got 12\n");
    }
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
        Animation::TrackView<const Float, const Quaternion> rotation = animation->track<Quaternion>(0);
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
        Animation::TrackView<const Float, const Vector3> translation = animation->track<Vector3>(1);
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
        Animation::TrackView<const Float, const Vector3> scaling = animation->track<Vector3>(2);
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

void TinyGltfImporterTest::animationInvalid() {
    auto&& data = AnimationInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->animationCount(), Containers::arraySize(AnimationInvalidData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::TinyGltfImporter::animation(): {}\n", data.message));
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
    Animation::TrackView<const Float, const CubicHermiteQuaternion> rotation = animation->track<CubicHermiteQuaternion>(0);
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
    Animation::TrackView<const Float, const CubicHermite3D> translation = animation->track<CubicHermite3D>(1);
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
    Animation::TrackView<const Float, const CubicHermite3D> scaling = animation->track<CubicHermite3D>(2);
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
    Animation::TrackView<const Float, const CubicHermite3D> translation = animation->track<CubicHermite3D>(1);
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
    Animation::TrackView<const Float, const CubicHermite3D> scaling = animation->track<CubicHermite3D>(1);
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
    Animation::TrackView<const Float, const Quaternion> track = animation->track<Quaternion>(0);
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
    Animation::TrackView<const Float, const Quaternion> track = animation->track<Quaternion>(0);

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

    Containers::Optional<AnimationData> animation;
    std::ostringstream out;
    {
        Warning warningRedirection{&out};
        animation = importer->animation(1);
    }
    CORRADE_VERIFY(animation);
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::animation(): quaternions in some rotation tracks were renormalized\n");
    CORRADE_COMPARE(animation->trackCount(), 1);
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::Quaternion);

    Animation::TrackView<const Float, const Quaternion> track = animation->track<Quaternion>(0);
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

    Animation::TrackView<const Float, const Quaternion> track = animation->track<Quaternion>(0);
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
    Animation::TrackView<const Float, const Quaternion> rotation = animation->track<Quaternion>(0);
    CORRADE_COMPARE(rotation.interpolation(), Animation::Interpolation::Linear);
    CORRADE_COMPARE(rotation.at(1.875f), Quaternion::rotation(90.0_degf, Vector3::xAxis()));

    /* Translation, constant interpolated, sharing keys with scaling */
    CORRADE_COMPARE(animation->trackType(1), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(1), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(1), 1);
    Animation::TrackView<const Float, const Vector3> translation = animation->track<Vector3>(1);
    CORRADE_COMPARE(translation.interpolation(), Animation::Interpolation::Constant);
    CORRADE_COMPARE(translation.at(1.5f), Vector3::yAxis(2.5f));

    /* Scaling, linearly interpolated, sharing keys with translation */
    CORRADE_COMPARE(animation->trackType(2), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(2), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(2), 2);
    Animation::TrackView<const Float, const Vector3> scaling = animation->track<Vector3>(2);
    CORRADE_COMPARE(scaling.interpolation(), Animation::Interpolation::Linear);
    CORRADE_COMPARE(scaling.at(1.5f), Vector3::zScale(5.2f));

    /* Rotation, spline interpolated */
    CORRADE_COMPARE(animation->trackType(3), AnimationTrackType::CubicHermiteQuaternion);
    CORRADE_COMPARE(animation->trackTargetType(3), AnimationTrackTargetType::Rotation3D);
    CORRADE_COMPARE(animation->trackTarget(3), 3);
    Animation::TrackView<const Float, const CubicHermiteQuaternion> rotation2 = animation->track<CubicHermiteQuaternion>(3);
    CORRADE_COMPARE(rotation2.interpolation(), Animation::Interpolation::Spline);
    /* The same as in CubicHermiteTest::splerpQuaternion() */
    CORRADE_COMPARE(rotation2.at(0.5f + 0.35f*3),
        (Quaternion{{-0.309862f, 0.174831f, 0.809747f}, 0.466615f}));

    /* Translation, spline interpolated */
    CORRADE_COMPARE(animation->trackType(4), AnimationTrackType::CubicHermite3D);
    CORRADE_COMPARE(animation->trackTargetType(4), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(4), 4);
    Animation::TrackView<const Float, const CubicHermite3D> translation2 = animation->track<CubicHermite3D>(4);
    CORRADE_COMPARE(translation2.interpolation(), Animation::Interpolation::Spline);
    /* The same as in CubicHermiteTest::splerpVector() */
    CORRADE_COMPARE(translation2.at(0.5f + 0.35f*3),
        (Vector3{1.04525f, 0.357862f, 0.540875f}));

    /* Scaling, spline interpolated */
    CORRADE_COMPARE(animation->trackType(5), AnimationTrackType::CubicHermite3D);
    CORRADE_COMPARE(animation->trackTargetType(5), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(5), 5);
    Animation::TrackView<const Float, const CubicHermite3D> scaling2 = animation->track<CubicHermite3D>(5);
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

    CORRADE_COMPARE(importer->lightCount(), 4);

    CORRADE_COMPARE(importer->lightForName("Spot"), 1);
    CORRADE_COMPARE(importer->lightName(1), "Spot");

    {
        auto light = importer->light("Point with everything implicit");
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{1.0f, 1.0f, 1.0f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f, 0.0f, 1.0f}));
        CORRADE_COMPARE(light->range(), Constants::inf());
    } {
        auto light = importer->light("Spot");
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Spot);
        CORRADE_COMPARE(light->color(), (Color3{0.28f, 0.19f, 1.0f}));
        CORRADE_COMPARE(light->intensity(), 2.1f);
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f, 0.0f, 1.0f}));
        CORRADE_COMPARE(light->range(), 10.0f);
        /* glTF has half-angles, we have full angles */
        CORRADE_COMPARE(light->innerConeAngle(), 0.25_radf*2.0f);
        CORRADE_COMPARE(light->outerConeAngle(), 0.35_radf*2.0f);
    } {
        auto light = importer->light("Spot with implicit angles");
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Spot);
        CORRADE_COMPARE(light->innerConeAngle(), 0.0_degf);
        /* glTF has half-angles, we have full angles */
        CORRADE_COMPARE(light->outerConeAngle(), 45.0_degf*2.0f);
    } {
        auto light = importer->light("Sun");
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Directional);
        CORRADE_COMPARE(light->color(), (Color3{1.0f, 0.08f, 0.14f}));
        CORRADE_COMPARE(light->intensity(), 0.1f);
    }
}

void TinyGltfImporterTest::lightInvalid() {
    auto&& data = LightInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "light-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->lightCount(), Containers::arraySize(LightInvalidData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->light(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::TinyGltfImporter::light(): {}\n", data.message));
}

void TinyGltfImporterTest::lightMissingType() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "light-missing-type.gltf")));
    /* This error is extremely shitty, but well that's tinygltf, so. */
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: 'type' property is missing.\n");
}

void TinyGltfImporterTest::lightMissingSpot() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "light-missing-spot.gltf")));
    /* This error is extremely shitty, but well that's tinygltf, so. */
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: Spot light description not found.\n");
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

    auto emptyScene = importer->scene(0);
    CORRADE_VERIFY(emptyScene);
    CORRADE_VERIFY(emptyScene->importerState());
    CORRADE_COMPARE(emptyScene->children3D(), std::vector<UnsignedInt>{});

    auto scene = importer->scene(1);
    CORRADE_VERIFY(scene);
    CORRADE_VERIFY(scene->importerState());
    CORRADE_COMPARE(scene->children3D(), (std::vector<UnsignedInt>{2, 4}));

    CORRADE_COMPARE(importer->object3DCount(), 7);

    CORRADE_COMPARE(importer->object3DName(4), "Light");
    CORRADE_COMPARE(importer->object3DForName("Light"), 4);

    {
        auto object = importer->object3D("Camera");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Camera);
        CORRADE_COMPARE(object->instance(), 2);
        CORRADE_VERIFY(object->children().empty());
    } {
        auto object = importer->object3D("Empty with one child");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->instance(), -1);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{0}));
    } {
        auto object = importer->object3D("Mesh w/o material");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 1);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).material(), -1);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).skin(), -1);
        CORRADE_VERIFY(object->children().empty());
    } {
        auto object = importer->object3D("Mesh and a material");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 0);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).material(), 1);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).skin(), -1);
        CORRADE_VERIFY(object->children().empty());
    } {
        auto object = importer->object3D("Mesh and a skin");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 1);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).material(), -1);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).skin(), 1);
        CORRADE_VERIFY(object->children().empty());
    } {
        auto object = importer->object3D("Light");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Light);
        CORRADE_COMPARE(object->instance(), 1);
        CORRADE_VERIFY(object->children().empty());
    } {
        auto object = importer->object3D("Empty with two children");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
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

void TinyGltfImporterTest::sceneInvalidObject() {
    auto&& data = SceneInvalidObjectData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "scene-invalid.gltf")));

    /* Check we didn't forget to test anything. There are some extra nodes for
       multi-primitive meshes, ignore those. */
    CORRADE_COMPARE(importer->object3DCount() - 1, Containers::arraySize(SceneInvalidObjectData));

    /* For testing bounds checks for multi-primitive meshes we need to import
       Nth mesh of the same name */
    Int id = importer->object3DForName(data.name);
    CORRADE_VERIFY(id != -1);
    CORRADE_COMPARE(importer->object3DName(id + data.idOffset), data.name);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->object3D(id + data.idOffset));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::TinyGltfImporter::object3D(): {}\n", data.message));
}

void TinyGltfImporterTest::sceneInvalidMesh() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "scene-invalid-mesh.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): mesh index 1 out of bounds for 1 meshes\n");
}

void TinyGltfImporterTest::sceneInvalidScene() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "scene-invalid.gltf")));

    CORRADE_COMPARE(importer->sceneCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->scene("node out of bounds"));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::scene(): node index 7 out of bounds for 7 nodes\n");
}

void TinyGltfImporterTest::sceneInvalidDefaultScene() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "scene-invalid-default.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): scene index 0 out of bounds for 0 scenes\n");
}

void TinyGltfImporterTest::objectTransformation() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "object-transformation" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->object3DCount(), 8);

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
    } {
        CORRADE_COMPARE(importer->object3DName(7), "Implicit transformation");
        auto object = importer->object3D(7);
        CORRADE_VERIFY(object);
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->instance(), -1);
        CORRADE_COMPARE(object->flags(), ObjectFlag3D::HasTranslationRotationScaling);
        CORRADE_COMPARE(object->translation(), Vector3{});
        CORRADE_COMPARE(object->rotation(), Quaternion{});
        CORRADE_COMPARE(object->scaling(), Vector3{1.0f});
        CORRADE_COMPARE(object->transformation(), Matrix4{Math::IdentityInit});
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

    Containers::Pointer<ObjectData3D> object;
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

void TinyGltfImporterTest::skin() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "skin" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->skin3DCount(), 2);
    CORRADE_COMPARE(importer->skin3DName(1), "explicit inverse bind matrices");
    CORRADE_COMPARE(importer->skin3DForName("explicit inverse bind matrices"), 1);
    CORRADE_COMPARE(importer->skin3DForName("nonexistent"), -1);

    {
        CORRADE_COMPARE(importer->skin3DName(0), "implicit inverse bind matrices");

        auto skin = importer->skin3D(0);
        CORRADE_VERIFY(skin);
        CORRADE_VERIFY(skin->importerState());
        CORRADE_COMPARE_AS(skin->joints(),
            Containers::arrayView<UnsignedInt>({1, 2}),
            TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(skin->inverseBindMatrices(),
            Containers::arrayView({Matrix4{}, Matrix4{}}),
            TestSuite::Compare::Container);

    } {
        CORRADE_COMPARE(importer->skin3DName(1), "explicit inverse bind matrices");

        auto skin = importer->skin3D(1);
        CORRADE_VERIFY(skin);
        CORRADE_VERIFY(skin->importerState());
        CORRADE_COMPARE_AS(skin->joints(),
            Containers::arrayView<UnsignedInt>({0, 2, 1}),
            TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(skin->inverseBindMatrices(),
            Containers::arrayView({
                Matrix4::rotationX(35.0_degf),
                Matrix4::translation({2.0f, 3.0f, 4.0f}),
                Matrix4::scaling({2.0f, 3.0f, 4.0f})
            }), TestSuite::Compare::Container);
    }
}

void TinyGltfImporterTest::skinInvalid() {
    auto&& data = SkinInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "skin-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->skin3DCount(), Containers::arraySize(SkinInvalidData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->skin3D(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::TinyGltfImporter::skin3D(): {}\n", data.message));
}

void TinyGltfImporterTest::skinNoJointsProperty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "skin-no-joints.gltf")));
    {
        CORRADE_EXPECT_FAIL("TinyGLTF doesn't give any usable error message when there's no skin.joints property, sigh.");
        CORRADE_VERIFY(false);
    }

    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: \n");
}

void TinyGltfImporterTest::mesh() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->meshCount(), 4);
    CORRADE_COMPARE(importer->meshName(0), "Non-indexed mesh");
    CORRADE_COMPARE(importer->meshForName("Non-indexed mesh"), 0);

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->importerState());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(!mesh->isIndexed());

    CORRADE_COMPARE(mesh->attributeCount(), 2);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            /* Interleaved with normals (which are in a different mesh) */
            {1.5f, -1.0f, -0.5f},
            {-0.5f, 2.5f, 0.75f},
            {-2.0f, 1.0f, 0.3f}
        }), TestSuite::Compare::Container);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::TextureCoordinates));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::TextureCoordinates), VertexFormat::Vector2);
    CORRADE_COMPARE_AS(mesh->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        Containers::arrayView<Vector2>({
            /* Y-flipped compared to the input */
            {0.3f, 1.0f},
            {0.0f, 0.5f},
            {0.3f, 0.7f}
        }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::meshAttributeless() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh.gltf")));

    auto mesh = importer->mesh("Attribute-less mesh");
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->importerState());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->vertexCount(), 0);
    CORRADE_COMPARE(mesh->attributeCount(), 0);
}

void TinyGltfImporterTest::meshIndexed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh.gltf")));

    CORRADE_COMPARE(importer->meshCount(), 4);
    CORRADE_COMPARE(importer->meshName(1), "Indexed mesh");
    CORRADE_COMPARE(importer->meshForName("Indexed mesh"), 1);

    auto mesh = importer->mesh(1);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->importerState());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedByte);
    CORRADE_COMPARE_AS(mesh->indices<UnsignedByte>(),
        Containers::arrayView<UnsignedByte>({0, 1, 2}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 4);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {1.5f, -1.0f, -0.5f},
            {-0.5f, 2.5f, 0.75f},
            {-2.0f, 1.0f, 0.3f}
        }), TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Normal));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Normal), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Normal),
        Containers::arrayView<Vector3>({
            {0.1f, 0.2f, 0.3f},
            {0.4f, 0.5f, 0.6f},
            {0.7f, 0.8f, 0.9f}
        }), TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Tangent));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Tangent), VertexFormat::Vector4);
    CORRADE_COMPARE_AS(mesh->attribute<Vector4>(MeshAttribute::Tangent),
        Containers::arrayView<Vector4>({
            {-0.1f, -0.2f, -0.3f, 1.0f},
            {-0.4f, -0.5f, -0.6f, -1.0f},
            {-0.7f, -0.8f, -0.9f, 1.0f}
        }), TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::ObjectId));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::ObjectId), VertexFormat::UnsignedInt);
    CORRADE_COMPARE_AS(mesh->attribute<UnsignedInt>(MeshAttribute::ObjectId),
        Containers::arrayView<UnsignedInt>({
            215, 71, 133
        }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::meshIndexedAttributeless() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh.gltf")));

    auto mesh = importer->mesh("Attribute-less indexed mesh");
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->importerState());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->indicesAsArray(),
        Containers::arrayView<UnsignedInt>({0, 1, 2}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->vertexCount(), 0);
    CORRADE_COMPARE(mesh->attributeCount(), 0);
}

void TinyGltfImporterTest::meshColors() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-colors.gltf")));

    CORRADE_COMPARE(importer->meshCount(), 1);

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->isIndexed());

    CORRADE_COMPARE(mesh->attributeCount(), 3);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {1.5f, -1.0f, -0.5f},
            {-0.5f, 2.5f, 0.75f},
            {-2.0f, 1.0f, 0.3f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Color), 2);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Color, 0), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Color),
        Containers::arrayView<Vector3>({
            {0.1f, 0.2f, 0.3f},
            {0.4f, 0.5f, 0.6f},
            {0.7f, 0.8f, 0.9f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Color, 1), VertexFormat::Vector4);
    CORRADE_COMPARE_AS(mesh->attribute<Vector4>(MeshAttribute::Color, 1),
        Containers::arrayView<Vector4>({
            {0.1f, 0.2f, 0.3f, 0.4f},
            {0.5f, 0.6f, 0.7f, 0.8f},
            {0.9f, 1.0f, 1.1f, 1.2f}
        }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::meshCustomAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-custom-attributes.gltf")));
    CORRADE_COMPARE(importer->meshCount(), 1);

    /* The mapping should be available even before the mesh is imported.
       Attributes are sorted by name by JSON so the ID isn't in declaration
       order. */
    const MeshAttribute tbnAttribute = importer->meshAttributeForName("_TBN");
    CORRADE_COMPARE(tbnAttribute, meshAttributeCustom(4));
    CORRADE_COMPARE(importer->meshAttributeName(tbnAttribute), "_TBN");

    const MeshAttribute uvRotation = importer->meshAttributeForName("_UV_ROTATION");
    CORRADE_COMPARE(uvRotation, meshAttributeCustom(6));
    CORRADE_COMPARE(importer->meshAttributeName(uvRotation), "_UV_ROTATION");

    const MeshAttribute tbnPreciserAttribute = importer->meshAttributeForName("_TBN_PRECISER");
    const MeshAttribute doubleShotAttribute = importer->meshAttributeForName("_DOUBLE_SHOT");
    const MeshAttribute objectIdAttribute = importer->meshAttributeForName("_OBJECT_ID3");
    const MeshAttribute negativePaddingAttribute = importer->meshAttributeForName("_NEGATIVE_PADDING");
    const MeshAttribute notAnIdentityAttribute = importer->meshAttributeForName("_NOT_AN_IDENTITY");

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);

    CORRADE_COMPARE(mesh->attributeCount(), 7);
    CORRADE_VERIFY(mesh->hasAttribute(tbnAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(tbnAttribute), VertexFormat::Matrix3x3bNormalizedAligned);
    CORRADE_COMPARE_AS(mesh->attribute<Matrix3x4b>(tbnAttribute),
        Containers::arrayView<Matrix3x4b>({{
            Vector4b{1, 2, 3, 0},
            Vector4b{4, 5, 6, 0},
            Vector4b{7, 8, 9, 0}
        }}), TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(uvRotation));
    CORRADE_COMPARE(mesh->attributeFormat(uvRotation), VertexFormat::Matrix2x2bNormalizedAligned);
    CORRADE_COMPARE_AS(mesh->attribute<Matrix2x4b>(uvRotation),
        Containers::arrayView<Matrix2x4b>({{
            Vector4b{10, 11, 0, 0},
            Vector4b{12, 13, 0, 0},
        }}), TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(tbnPreciserAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(tbnPreciserAttribute), VertexFormat::Matrix3x3sNormalizedAligned);
    CORRADE_COMPARE_AS(mesh->attribute<Matrix3x4s>(tbnPreciserAttribute),
        Containers::arrayView<Matrix3x4s>({{
            Vector4s{-1, -2, -3, 0},
            Vector4s{-4, -5, -6, 0},
            Vector4s{-7, -8, -9, 0}
        }}), TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(doubleShotAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(doubleShotAttribute), VertexFormat::Vector2d);
    CORRADE_COMPARE_AS(mesh->attribute<Vector2d>(doubleShotAttribute),
        Containers::arrayView<Vector2d>({{31.2, 28.8}}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(objectIdAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(objectIdAttribute), VertexFormat::UnsignedInt);
    CORRADE_COMPARE_AS(mesh->attribute<UnsignedInt>(objectIdAttribute),
        Containers::arrayView<UnsignedInt>({5678125}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(negativePaddingAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(negativePaddingAttribute), VertexFormat::Int);
    CORRADE_COMPARE_AS(mesh->attribute<Int>(negativePaddingAttribute),
        Containers::arrayView<Int>({-3548415}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(notAnIdentityAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(notAnIdentityAttribute), VertexFormat::Matrix4x4d);
    CORRADE_COMPARE_AS(mesh->attribute<Matrix4d>(notAnIdentityAttribute),
        Containers::arrayView<Matrix4d>({{
            {0.1, 0.2, 0.3, 0.4},
            {0.5, 0.6, 0.7, 0.8},
            {0.9, 1.0, 1.1, 1.2},
            {1.3, 1.4, 1.5, 1.6}
        }}), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::meshCustomAttributesNoFileOpened() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    /* These should return nothing (and not crash) */
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(564)), "");
    CORRADE_COMPARE(importer->meshAttributeForName("thing"), MeshAttribute{});
}

void TinyGltfImporterTest::meshMultiplePrimitives() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-multiple-primitives.gltf")));

    /* Four meshes, but one has three primitives and one two. Distinguishing
       using the primitive type, hopefully that's enough. */
    CORRADE_COMPARE(importer->meshCount(), 7);
    {
        CORRADE_COMPARE(importer->meshName(0), "Single-primitive points");
        CORRADE_COMPARE(importer->meshForName("Single-primitive points"), 0);
        auto mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Points);
    } {
        CORRADE_COMPARE(importer->meshName(1), "Multi-primitive lines, triangles, triangle strip");
        CORRADE_COMPARE(importer->meshName(2), "Multi-primitive lines, triangles, triangle strip");
        CORRADE_COMPARE(importer->meshName(3), "Multi-primitive lines, triangles, triangle strip");
        CORRADE_COMPARE(importer->meshForName("Multi-primitive lines, triangles, triangle strip"), 1);
        auto mesh1 = importer->mesh(1);
        CORRADE_VERIFY(mesh1);
        CORRADE_COMPARE(mesh1->primitive(), MeshPrimitive::Lines);
        auto mesh2 = importer->mesh(2);
        CORRADE_VERIFY(mesh2);
        CORRADE_COMPARE(mesh2->primitive(), MeshPrimitive::Triangles);
        auto mesh3 = importer->mesh(3);
        CORRADE_VERIFY(mesh3);
        CORRADE_COMPARE(mesh3->primitive(), MeshPrimitive::TriangleStrip);
    } {
        CORRADE_COMPARE(importer->meshName(4), "Single-primitive line loop");
        CORRADE_COMPARE(importer->meshForName("Single-primitive line loop"), 4);
        auto mesh = importer->mesh(4);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::LineLoop);
    } {
        CORRADE_COMPARE(importer->meshName(5), "Multi-primitive triangle fan, line strip");
        CORRADE_COMPARE(importer->meshName(6), "Multi-primitive triangle fan, line strip");
        CORRADE_COMPARE(importer->meshForName("Multi-primitive triangle fan, line strip"), 5);
        auto mesh5 = importer->mesh(5);
        CORRADE_VERIFY(mesh5);
        CORRADE_COMPARE(mesh5->primitive(), MeshPrimitive::TriangleFan);
        auto mesh6 = importer->mesh(6);
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

void TinyGltfImporterTest::meshPrimitivesTypes() {
    auto&& data = MeshPrimitivesTypesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Disable Y-flipping to have consistent results. Tested separately for all
       types in materialTexCoordFlip(). */
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    importer->configuration().setValue("textureCoordinateYFlipInMaterial", true);

    if(data.objectIdAttribute)
        importer->configuration().setValue("objectIdAttribute", data.objectIdAttribute);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-primitives-types.gltf")));

    /* Ensure we didn't forget to test any case */
    CORRADE_COMPARE(importer->meshCount(), Containers::arraySize(MeshPrimitivesTypesData));

    auto mesh = importer->mesh(data.name);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), data.primitive);

    if(data.indexType != MeshIndexType{}) {
        CORRADE_VERIFY(mesh->isIndexed());
        CORRADE_COMPARE(mesh->indexType(), data.indexType);
        CORRADE_COMPARE_AS(mesh->indicesAsArray(),
            Containers::arrayView<UnsignedInt>({0, 2, 1, 4, 3, 0}),
            TestSuite::Compare::Container);
    } else CORRADE_VERIFY(!mesh->isIndexed());

    /* Positions */
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), data.positionFormat);
    if(isVertexFormatNormalized(data.positionFormat)) {
        if(vertexFormatComponentFormat(data.positionFormat) == VertexFormat::UnsignedByte ||
           vertexFormatComponentFormat(data.positionFormat) == VertexFormat::UnsignedShort) {
            CORRADE_COMPARE_AS(mesh->positions3DAsArray(),
                Containers::arrayView<Vector3>({
                    {0.8f, 0.4f, 0.2f},
                    {1.0f, 0.333333f, 0.666667f},
                    {0.733333f, 0.866667f, 0.0f},
                    {0.066667f, 0.133333f, 0.933333f},
                    {0.6f, 0.266667f, 0.466667f}
                }), TestSuite::Compare::Container);
        } else if(vertexFormatComponentFormat(data.positionFormat) == VertexFormat::Byte ||
                  vertexFormatComponentFormat(data.positionFormat) == VertexFormat::Short) {

            constexpr Vector3 expected[]{
                    {-0.133333f, -0.333333f, -0.2f},
                    {-0.8f, -0.133333f, -0.4f},
                    {-1.0f, -0.933333f, -0.0f},
                    {-0.4f, -0.6f, -0.333333f},
                    {-0.666667f, -0.733333f, -0.933333f}
            };

            /* Because the signed packed formats are extremely imprecise, we
               increase the fuzziness a bit */
            auto positions = mesh->positions3DAsArray();
            const Float precision = Math::pow(10.0f, -1.5f*vertexFormatSize(vertexFormatComponentFormat(data.positionFormat)));
            CORRADE_COMPARE_AS(precision, 5.0e-2f, TestSuite::Compare::Less);
            CORRADE_COMPARE_AS(precision, 1.0e-6f, TestSuite::Compare::GreaterOrEqual);
            CORRADE_COMPARE(positions.size(), Containers::arraySize(expected));
            CORRADE_ITERATION("precision" << precision);
            for(std::size_t i = 0; i != positions.size(); ++i) {
                CORRADE_ITERATION(i);
                CORRADE_COMPARE_WITH(positions[i], expected[i],
                    TestSuite::Compare::around(Vector3{precision}));
            }
        } else {
            CORRADE_ITERATION(data.positionFormat);
            CORRADE_VERIFY(false);
        }
    } else {
        CORRADE_COMPARE_AS(mesh->positions3DAsArray(),
            Containers::arrayView<Vector3>({
                {1.0f, 3.0f, 2.0f},
                {1.0f, 1.0f, 2.0f},
                {3.0f, 3.0f, 2.0f},
                {3.0f, 1.0f, 2.0f},
                {5.0f, 3.0f, 9.0f}
            }), TestSuite::Compare::Container);
    }

    /* Normals */
    if(data.normalFormat != VertexFormat{}) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Normal));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Normal), data.normalFormat);

        constexpr Vector3 expected[]{
            {-0.333333f, -0.6666667f, -0.933333f},
            {-0.0f, -0.133333f, -1.0f},
            {-0.6f, -0.8f, -0.2f},
            {-0.4f, -0.733333f, -0.933333f},
            {-0.133333f, -0.733333f, -0.4f}
        };

        /* Because the signed packed formats are extremely imprecise, we
           increase the fuzziness a bit */
        auto normals = mesh->normalsAsArray();
        const Float precision = Math::pow(10.0f, -1.5f*vertexFormatSize(vertexFormatComponentFormat(data.normalFormat)));
        CORRADE_COMPARE_AS(precision, 5.0e-2f, TestSuite::Compare::Less);
        CORRADE_COMPARE_AS(precision, 1.0e-6f, TestSuite::Compare::GreaterOrEqual);
        CORRADE_COMPARE(normals.size(), Containers::arraySize(expected));
        CORRADE_ITERATION("precision" << precision);
        for(std::size_t i = 0; i != normals.size(); ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE_WITH(normals[i], expected[i],
                TestSuite::Compare::around(Vector3{precision}));
        }
    } else CORRADE_VERIFY(!mesh->hasAttribute(MeshAttribute::Normal));

    /* Tangents */
    if(data.tangentFormat != VertexFormat{}) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Tangent));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Tangent), data.tangentFormat);

        constexpr Vector3 expected[]{
            {-0.933333f, -0.333333f, -0.6666667f},
            {-1.0f, -0.0f, -0.133333f},
            {-0.2f, -0.6f, -0.8f},
            {-0.933333f, -0.4f, -0.733333f},
            {-0.4f, -0.133333f, -0.733333f}
        };

        /* Because the signed packed formats are extremely imprecise, we
           increase the fuzziness a bit */
        auto tangents = mesh->tangentsAsArray();
        const Float precision = Math::pow(10.0f, -1.5f*vertexFormatSize(vertexFormatComponentFormat(data.tangentFormat)));
        CORRADE_COMPARE_AS(precision, 5.0e-2f, TestSuite::Compare::Less);
        CORRADE_COMPARE_AS(precision, 1.0e-6f, TestSuite::Compare::GreaterOrEqual);
        CORRADE_COMPARE(tangents.size(), Containers::arraySize(expected));
        CORRADE_ITERATION("precision" << precision);
        for(std::size_t i = 0; i != tangents.size(); ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE_WITH(tangents[i], expected[i],
                TestSuite::Compare::around(Vector3{precision}));
        }

        /* However the bitangents signs are just 1 or -1, so no need to take
           extreme measures */
        CORRADE_COMPARE_AS(mesh->bitangentSignsAsArray(),
            Containers::arrayView<Float>({1.0f, -1.0f, 1.0f, -1.0f, 1.0f}),
            TestSuite::Compare::Container);
    } else CORRADE_VERIFY(!mesh->hasAttribute(MeshAttribute::Tangent));

    /* Colors */
    if(data.colorFormat == VertexFormat{}) {
        CORRADE_VERIFY(!mesh->hasAttribute(MeshAttribute::Color));
    } else if(vertexFormatComponentCount(data.colorFormat) == 3) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Color));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Color), data.colorFormat);
        CORRADE_COMPARE_AS(Containers::arrayCast<Color3>(Containers::stridedArrayView(mesh->colorsAsArray())),
            Containers::stridedArrayView<Color3>({
                {0.8f, 0.2f, 0.4f},
                {0.6f, 0.666667f, 1.0f},
                {0.0f, 0.0666667f, 0.9333333f},
                {0.733333f, 0.8666666f, 0.133333f},
                {0.266667f, 0.3333333f, 0.466667f}
            }), TestSuite::Compare::Container);
    } else if(vertexFormatComponentCount(data.colorFormat) == 4) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Color));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Color), data.colorFormat);
        CORRADE_COMPARE_AS(mesh->colorsAsArray(),
            Containers::arrayView<Color4>({
                {0.8f, 0.2f, 0.4f, 0.266667f},
                {0.6f, 0.666667f, 1.0f, 0.8666667f},
                {0.0f, 0.0666667f, 0.9333333f, 0.466667f},
                {0.733333f, 0.8666667f, 0.133333f, 0.666667f},
                {0.266667f, 0.3333333f, 0.466666f, 0.0666667f}
            }), TestSuite::Compare::Container);
    } else CORRADE_VERIFY(false);

    /* Texture coordinates */
    if(data.textureCoordinateFormat == VertexFormat{}) {
        CORRADE_VERIFY(!mesh->hasAttribute(MeshAttribute::TextureCoordinates));

    } else if(isVertexFormatNormalized(data.textureCoordinateFormat)) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::TextureCoordinates));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::TextureCoordinates), data.textureCoordinateFormat);
        if(vertexFormatComponentFormat(data.textureCoordinateFormat) == VertexFormat::UnsignedByte ||
           vertexFormatComponentFormat(data.textureCoordinateFormat) == VertexFormat::UnsignedShort) {
            CORRADE_COMPARE_AS(mesh->textureCoordinates2DAsArray(),
                Containers::arrayView<Vector2>({
                    {0.933333f, 0.3333333f},
                    {0.133333f, 0.9333333f},
                    {0.666667f, 0.2666667f},
                    {0.466666f, 0.3333333f},
                    {0.866666f, 0.0666667f}
                }), TestSuite::Compare::Container);
        } else if(vertexFormatComponentFormat(data.textureCoordinateFormat) == VertexFormat::Byte ||
                  vertexFormatComponentFormat(data.textureCoordinateFormat) == VertexFormat::Short) {
            constexpr Vector2 expected[]{
                {-0.666667f, -0.9333333f},
                {-0.4f, -0.7333333f},
                {-0.8f, -0.2f},
                {-0.0f, -0.1333333f},
                {-0.6f, -0.3333333f}
            };

            /* Because the signed packed formats are extremely imprecise, we
               increase the fuzziness a bit */
            auto textureCoordinates = mesh->textureCoordinates2DAsArray();
            const Float precision = Math::pow(10.0f, -1.5f*vertexFormatSize(vertexFormatComponentFormat(data.textureCoordinateFormat)));
            CORRADE_COMPARE_AS(precision, 5.0e-2f, TestSuite::Compare::Less);
            CORRADE_COMPARE_AS(precision, 1.0e-6f, TestSuite::Compare::GreaterOrEqual);
            CORRADE_COMPARE(textureCoordinates.size(), Containers::arraySize(expected));
            CORRADE_ITERATION("precision" << precision);
            for(std::size_t i = 0; i != textureCoordinates.size(); ++i) {
                CORRADE_ITERATION(i);
                CORRADE_COMPARE_WITH(textureCoordinates[i], expected[i],
                    TestSuite::Compare::around(Vector2{precision}));
            }
        } else {
            CORRADE_ITERATION(data.positionFormat);
            CORRADE_VERIFY(false);
        }
    } else {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::TextureCoordinates));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::TextureCoordinates), data.textureCoordinateFormat);
        CORRADE_COMPARE_AS(mesh->textureCoordinates2DAsArray(),
            Containers::arrayView<Vector2>({
                {75.0f, 13.0f},
                {98.0f, 22.0f},
                {15.0f, 125.0f},
                {12.0f, 33.0f},
                {24.0f, 57.0f}
            }), TestSuite::Compare::Container);
    }

    /* Object ID */
    if(data.objectIdFormat != VertexFormat{}) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::ObjectId));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::ObjectId), data.objectIdFormat);
        CORRADE_COMPARE_AS(mesh->objectIdsAsArray(),
            Containers::stridedArrayView<UnsignedInt>({
                215, 71, 133, 5, 196
            }), TestSuite::Compare::Container);
    } else CORRADE_VERIFY(!mesh->hasAttribute(MeshAttribute::ObjectId));
}

void TinyGltfImporterTest::meshIndexAccessorOutOfBounds() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-index-accessor-oob.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: primitive indices accessor out of bounds\n");
}

void TinyGltfImporterTest::meshInvalid() {
    auto&& data = MeshInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->meshCount(), Containers::arraySize(MeshInvalidData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::TinyGltfImporter::mesh(): {}\n", data.message));
}

void TinyGltfImporterTest::materialPbrMetallicRoughness() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-metallicroughness.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 7);
    CORRADE_COMPARE(importer->materialForName("textures"), 2);
    CORRADE_COMPARE(importer->materialName(2), "textures");

    {
        const char* name = "defaults";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->importerState());
        CORRADE_COMPARE(material->types(), MaterialType::PbrMetallicRoughness);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 0);

        /* These are glTF defaults, just verify those are consistent with
           MaterialData API defaults (if they wouldn't be, we'd need to add
           explicit attributes to override those) */
        const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
        CORRADE_COMPARE(pbr.baseColor(), (Color4{1.0f}));
        CORRADE_COMPARE(pbr.metalness(), 1.0f);
        CORRADE_COMPARE(pbr.roughness(), 1.0f);
    } {
        const char* name = "color";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 3);

        const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
        CORRADE_COMPARE(pbr.baseColor(), (Color4{0.3f, 0.4f, 0.5f, 0.8f}));
        CORRADE_COMPARE(pbr.metalness(), 0.56f);
        CORRADE_COMPARE(pbr.roughness(), 0.89f);
    } {
        const char* name = "textures";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 5);

        const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::BaseColorTexture));
        CORRADE_COMPARE(pbr.baseColor(), (Color4{0.7f, 0.8f, 0.9f, 1.1f}));
        CORRADE_COMPARE(pbr.baseColorTexture(), 0);
        CORRADE_COMPARE(pbr.metalness(), 0.6f);
        CORRADE_COMPARE(pbr.roughness(), 0.9f);
        CORRADE_VERIFY(pbr.hasNoneRoughnessMetallicTexture());
        CORRADE_COMPARE(pbr.metalnessTexture(), 1);
    } {
        const char* name = "identity texture transform";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 5);

        const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
        /* Identity transform, but is present */
        CORRADE_VERIFY(pbr.hasTextureTransformation());
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::BaseColorTexture));
        CORRADE_COMPARE(pbr.baseColorTextureMatrix(), (Matrix3{}));
        CORRADE_VERIFY(pbr.hasNoneRoughnessMetallicTexture());
        CORRADE_COMPARE(pbr.metalnessTextureMatrix(), (Matrix3{}));
    } {
        const char* name = "texture transform";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 5);

        const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
        /* All */
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::BaseColorTexture));
        CORRADE_COMPARE(pbr.baseColorTextureMatrix(), (Matrix3{
            {0.164968f, 0.472002f, 0.0f},
            {-0.472002f, 0.164968f, 0.0f},
            {0.472002f, -0.164968f, 1.0f}
        }));
        /* Offset + scale */
        CORRADE_VERIFY(pbr.hasNoneRoughnessMetallicTexture());
        CORRADE_COMPARE(pbr.metalnessTextureMatrix(), (Matrix3{
            {0.5f, 0.0f, 0.0f},
            {0.0f, 0.5f, 0.0f},
            {0.0f, -0.5f, 1.0f}
        }));
    } {
        const char* name = "texture coordinate sets";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 5);

        const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::BaseColorTexture));
        CORRADE_COMPARE(pbr.baseColorTextureCoordinates(), 7);
        CORRADE_VERIFY(pbr.hasNoneRoughnessMetallicTexture());
        CORRADE_COMPARE(pbr.metalnessTextureCoordinates(), 5);
    } {
        const char* name = "empty texture transform with overriden coordinate set";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 7);

        const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::BaseColorTexture));
        CORRADE_COMPARE(pbr.baseColorTextureMatrix(), Matrix3{});
        CORRADE_VERIFY(pbr.hasNoneRoughnessMetallicTexture());
        CORRADE_COMPARE(pbr.metalnessTextureMatrix(), Matrix3{});
        CORRADE_COMPARE(pbr.metalnessTextureCoordinates(), 2); /* not 5 */
    }
}

void TinyGltfImporterTest::materialPbrSpecularGlossiness() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-specularglossiness.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 7);

    {
        const char* name = "defaults";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->importerState());
        {
            CORRADE_EXPECT_FAIL("Ideally tinygltf wouldn't define metallic/roughness attributes if not present in the material, but well.");
            CORRADE_COMPARE(material->types(), MaterialType::PbrSpecularGlossiness);
        }
        CORRADE_COMPARE(material->types(), MaterialType::PbrSpecularGlossiness|MaterialType::PbrMetallicRoughness);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 0);

        /* These are glTF defaults, just verify those are consistent with
           MaterialData API defaults (if they wouldn't be, we'd need to add
           explicit attributes to override those) */
        const auto& pbr = material->as<PbrSpecularGlossinessMaterialData>();
        CORRADE_COMPARE(pbr.diffuseColor(), (Color4{1.0f}));
        CORRADE_COMPARE(pbr.specularColor(), (Color4{1.0f, 0.0f}));
        CORRADE_COMPARE(pbr.glossiness(), 1.0f);
    } {
        const char* name = "color";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 3);

        const auto& pbr = material->as<PbrSpecularGlossinessMaterialData>();
        CORRADE_COMPARE(pbr.diffuseColor(), (Color4{0.3f, 0.4f, 0.5f, 0.8f}));
        CORRADE_COMPARE(pbr.specularColor(), (Color4{0.1f, 0.2f, 0.6f, 0.0f}));
        CORRADE_COMPARE(pbr.glossiness(), 0.89f);
    } {
        const char* name = "textures";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 5);

        const auto& pbr = material->as<PbrSpecularGlossinessMaterialData>();
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::DiffuseTexture));
        CORRADE_COMPARE(pbr.diffuseColor(), (Color4{0.7f, 0.8f, 0.9f, 1.1f}));
        CORRADE_COMPARE(pbr.diffuseTexture(), 0);
        CORRADE_COMPARE(pbr.specularColor(), (Color4{0.4f, 0.5f, 0.6f, 0.0f}));
        CORRADE_VERIFY(pbr.hasSpecularGlossinessTexture());
        CORRADE_COMPARE(pbr.specularTexture(), 1);        CORRADE_COMPARE(pbr.glossiness(), 0.9f);
    } {
        const char* name = "identity texture transform";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);

        const auto& pbr = material->as<PbrSpecularGlossinessMaterialData>();
        {
            CORRADE_EXPECT_FAIL("tinygltf treats an empty extension object inside an extension as if there was no extension at all. The same works correctly with builtin pbrMetallicRoughness.");
            CORRADE_COMPARE(material->attributeCount(), 5);
            CORRADE_VERIFY(pbr.hasTextureTransformation());
        }
        CORRADE_COMPARE(material->attributeCount(), 2);

        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::DiffuseTexture));
        CORRADE_COMPARE(pbr.diffuseTextureMatrix(), (Matrix3{}));
        CORRADE_VERIFY(pbr.hasSpecularGlossinessTexture());
        CORRADE_COMPARE(pbr.specularTextureMatrix(), (Matrix3{}));
    } {
        const char* name = "texture transform";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 5);

        const auto& pbr = material->as<PbrSpecularGlossinessMaterialData>();
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::DiffuseTexture));
        CORRADE_COMPARE(pbr.diffuseTextureMatrix(), (Matrix3{
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, -1.0f, 1.0f}
        }));
        CORRADE_VERIFY(pbr.hasSpecularGlossinessTexture());
        CORRADE_COMPARE(pbr.specularTextureMatrix(), (Matrix3{
            {0.5f, 0.0f, 0.0f},
            {0.0f, 0.5f, 0.0f},
            {0.0f, 0.5f, 1.0f}
        }));
    } {
        const char* name = "texture coordinate sets";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 5);

        const auto& pbr = material->as<PbrSpecularGlossinessMaterialData>();
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::DiffuseTexture));
        CORRADE_COMPARE(pbr.diffuseTextureCoordinates(), 7);
        CORRADE_VERIFY(pbr.hasSpecularGlossinessTexture());
        CORRADE_COMPARE(pbr.specularTextureCoordinates(), 5);
    } {
        const char* name = "both metallic/roughness and specular/glossiness";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);

        CORRADE_COMPARE(material->types(), MaterialType::PbrSpecularGlossiness|MaterialType::PbrMetallicRoughness);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 6);

        const auto& a = material->as<PbrMetallicRoughnessMaterialData>();
        CORRADE_COMPARE(a.baseColor(), (Color4{0.3f, 0.4f, 0.5f, 0.8f}));
        CORRADE_COMPARE(a.metalness(), 0.56f);
        CORRADE_COMPARE(a.roughness(), 0.89f);

        const auto& b = material->as<PbrSpecularGlossinessMaterialData>();
        CORRADE_COMPARE(b.diffuseColor(), (Color4{0.3f, 0.4f, 0.5f, 0.8f}));
        CORRADE_COMPARE(b.specularColor(), (Color4{0.1f, 0.2f, 0.6f, 0.0f}));
        CORRADE_COMPARE(b.glossiness(), 0.89f);
    }
}

void TinyGltfImporterTest::materialCommon() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-common.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 7);

    {
        auto material = importer->material("defaults");
        CORRADE_VERIFY(material);
        {
            CORRADE_EXPECT_FAIL("Ideally tinygltf wouldn't define metallic/roughness attributes if not present in the material, but well.");
            CORRADE_COMPARE(material->types(), MaterialTypes{});
        }
        CORRADE_COMPARE(material->types(), MaterialType::PbrMetallicRoughness);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 0);

        /* These are glTF defaults, just verify those are consistent with
           MaterialData API defaults (if they wouldn't be, we'd need to add
           explicit attributes to override those) */
        CORRADE_COMPARE(material->alphaMode(), MaterialAlphaMode::Opaque);
        CORRADE_COMPARE(material->alphaMask(), 0.5f);
    } {
        auto material = importer->material("alpha mask");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 1);
        CORRADE_COMPARE(material->alphaMode(), MaterialAlphaMode::Mask);
        CORRADE_COMPARE(material->alphaMask(), 0.369f);
    } {
        auto material = importer->material("double-sided alpha blend");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 2);
        CORRADE_VERIFY(material->isDoubleSided());
        CORRADE_COMPARE(material->alphaMode(), MaterialAlphaMode::Blend);
        CORRADE_COMPARE(material->alphaMask(), 0.5f);
    } {
        auto material = importer->material("opaque");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 0);
        CORRADE_COMPARE(material->alphaMode(), MaterialAlphaMode::Opaque);
        CORRADE_COMPARE(material->alphaMask(), 0.5f);
    } {
        const char* name = "normal, occlusion, emissive texture";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 6);

        const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::NormalTexture));
        CORRADE_COMPARE(pbr.normalTexture(), 1);
        CORRADE_COMPARE(pbr.normalTextureScale(), 0.56f);
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::OcclusionTexture));
        CORRADE_COMPARE(pbr.occlusionTexture(), 2);
        CORRADE_COMPARE(pbr.occlusionTextureStrength(), 0.21f);
        CORRADE_COMPARE(pbr.emissiveColor(), (Color3{0.1f, 0.2f, 0.3f}));
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::EmissiveTexture));
        CORRADE_COMPARE(pbr.emissiveTexture(), 0);
    } {
        const char* name = "normal, occlusion, emissive texture identity transform";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 6);

        const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
        /* Identity transform, but is present */
        CORRADE_VERIFY(pbr.hasTextureTransformation());
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::NormalTexture));
        CORRADE_COMPARE(pbr.normalTextureMatrix(), Matrix3{});
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::OcclusionTexture));
        CORRADE_COMPARE(pbr.occlusionTextureMatrix(), Matrix3{});
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::EmissiveTexture));
        CORRADE_COMPARE(pbr.emissiveTextureMatrix(), Matrix3{});
    } {
        const char* name = "normal, occlusion, emissive texture transform + sets";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 9);

        const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::NormalTexture));
        CORRADE_COMPARE(pbr.normalTextureMatrix(), (Matrix3{
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, -1.0f, 1.0f}
        }));
        CORRADE_COMPARE(pbr.normalTextureCoordinates(), 2);
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::OcclusionTexture));
        CORRADE_COMPARE(pbr.occlusionTextureMatrix(), (Matrix3{
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.5f, -1.0f, 1.0f}
        }));
        CORRADE_COMPARE(pbr.occlusionTextureCoordinates(), 3);
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::EmissiveTexture));
        CORRADE_COMPARE(pbr.emissiveTextureMatrix(), (Matrix3{
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.5f, 0.0f, 1.0f}
        }));
        CORRADE_COMPARE(pbr.emissiveTextureCoordinates(), 1);
    }
}

void TinyGltfImporterTest::materialUnlit() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-unlit.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    auto material = importer->material(0);
    CORRADE_VERIFY(material);
    CORRADE_VERIFY(material->importerState());
    /* Metallic/roughness is removed from types */
    CORRADE_COMPARE(material->types(), MaterialType::Flat);
    CORRADE_COMPARE(material->layerCount(), 1);
    CORRADE_COMPARE(material->attributeCount(), 2);

    const auto& flat = material->as<FlatMaterialData>();
    CORRADE_COMPARE(flat.color(), (Color4{0.7f, 0.8f, 0.9f, 1.1f}));
    CORRADE_VERIFY(flat.hasTexture());
    CORRADE_COMPARE(flat.texture(), 1);
}

void TinyGltfImporterTest::materialClearCoat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-clearcoat.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 6);

    {
        const char* name = "defaults";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        {
            CORRADE_EXPECT_FAIL("Ideally tinygltf wouldn't define metallic/roughness attributes if not present in the material, but well.");
            CORRADE_COMPARE(material->types(), MaterialType::PbrClearCoat);
        }
        CORRADE_COMPARE(material->types(), MaterialType::PbrMetallicRoughness|MaterialType::PbrClearCoat);
        CORRADE_COMPARE(material->layerCount(), 2);
        CORRADE_VERIFY(material->hasLayer(MaterialLayer::ClearCoat));

        /* These are glTF defaults, which are *not* consistent with ours */
        const auto& pbr = material->as<PbrClearCoatMaterialData>();
        CORRADE_COMPARE(pbr.attributeCount(), 3);
        CORRADE_COMPARE(pbr.layerFactor(), 0.0f);
        CORRADE_COMPARE(pbr.roughness(), 0.0f);
    } {
        const char* name = "factors";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 2);
        CORRADE_VERIFY(material->hasLayer(MaterialLayer::ClearCoat));

        const auto& pbr = material->as<PbrClearCoatMaterialData>();
        CORRADE_COMPARE(pbr.attributeCount(), 3);
        CORRADE_COMPARE(pbr.layerFactor(), 0.67f);
        CORRADE_COMPARE(pbr.roughness(), 0.34f);
    } {
        const char* name = "textures";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 2);
        CORRADE_VERIFY(material->hasLayer(MaterialLayer::ClearCoat));

        const auto& pbr = material->as<PbrClearCoatMaterialData>();
        CORRADE_COMPARE(pbr.attributeCount(), 8);
        CORRADE_COMPARE(pbr.layerFactor(), 0.7f);
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::LayerFactorTexture));
        CORRADE_COMPARE(pbr.layerFactorTexture(), 2);
        CORRADE_COMPARE(pbr.layerFactorTextureSwizzle(), MaterialTextureSwizzle::R);
        CORRADE_COMPARE(pbr.roughness(), 0.4f);
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::RoughnessTexture));
        CORRADE_COMPARE(pbr.roughnessTexture(), 1);
        CORRADE_COMPARE(pbr.roughnessTextureSwizzle(), MaterialTextureSwizzle::G);
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::NormalTexture));
        CORRADE_COMPARE(pbr.normalTexture(), 0);
        CORRADE_COMPARE(pbr.normalTextureScale(), 0.35f);
    } {
        const char* name = "packed textures";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 2);
        CORRADE_VERIFY(material->hasLayer(MaterialLayer::ClearCoat));

        const auto& pbr = material->as<PbrClearCoatMaterialData>();
        CORRADE_COMPARE(pbr.attributeCount(), 6);
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::LayerFactorTexture));
        CORRADE_COMPARE(pbr.layerFactorTexture(), 1);
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::RoughnessTexture));
        CORRADE_COMPARE(pbr.roughnessTexture(), 1);
        CORRADE_VERIFY(pbr.hasLayerFactorRoughnessTexture());
    } {
        const char* name = "texture identity transform";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 2);
        CORRADE_VERIFY(material->hasLayer(MaterialLayer::ClearCoat));

        const auto& pbr = material->as<PbrClearCoatMaterialData>();
        {
            CORRADE_EXPECT_FAIL("tinygltf treats an empty extension object inside an extension as if there was no extension at all. The same works correctly with builtin pbrMetallicRoughness.");
            CORRADE_COMPARE(pbr.attributeCount(), 7 + 3);
            CORRADE_VERIFY(pbr.hasTextureTransformation());
        }
        CORRADE_COMPARE(pbr.attributeCount(), 7);
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::LayerFactorTexture));
        CORRADE_COMPARE(pbr.layerFactorTextureMatrix(), Matrix3{});
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::RoughnessTexture));
        CORRADE_COMPARE(pbr.roughnessTextureMatrix(), Matrix3{});
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::NormalTexture));
        CORRADE_COMPARE(pbr.normalTextureMatrix(), Matrix3{});
    } {
        const char* name = "texture transform + coordinate set";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 2);
        CORRADE_VERIFY(material->hasLayer(MaterialLayer::ClearCoat));

        const auto& pbr = material->as<PbrClearCoatMaterialData>();
        CORRADE_COMPARE(pbr.attributeCount(), 13);
        /* Identity transform, but is present */
        CORRADE_VERIFY(pbr.hasTextureTransformation());
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::LayerFactorTexture));
        CORRADE_COMPARE(pbr.layerFactorTextureMatrix(), (Matrix3{
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, -1.0f, 1.0f}
        }));
        CORRADE_COMPARE(pbr.layerFactorTextureCoordinates(), 5);
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::RoughnessTexture));
        CORRADE_COMPARE(pbr.roughnessTextureMatrix(), (Matrix3{
            {0.5f, 0.0f, 0.0f},
            {0.0f, 0.5f, 0.0f},
            {0.0f, 0.5f, 1.0f}
        }));
        CORRADE_COMPARE(pbr.roughnessTextureCoordinates(), 1);
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::NormalTexture));
        CORRADE_COMPARE(pbr.normalTextureMatrix(), (Matrix3{
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.5f, 0.0f, 1.0f}
        }));
        CORRADE_COMPARE(pbr.normalTextureCoordinates(), 7);
    }
}

void TinyGltfImporterTest::materialPhongFallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    /* phongMaterialFallback should be on by default */
    //importer->configuration().setValue("phongMaterialFallback", true);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-phong-fallback.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 4);

    {
        const char* name = "none";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->importerState());
        {
            CORRADE_EXPECT_FAIL("Ideally tinygltf wouldn't define metallic/roughness attributes if not present in the material, but well.");
            CORRADE_COMPARE(material->types(), MaterialType::Phong);
        }
        CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::PbrMetallicRoughness);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 0);

        /* These are glTF defaults, just verify those are consistent with
           MaterialData API defaults (if they wouldn't be, we'd need to add
           explicit attributes to override those) */
        const auto& phong = material->as<PhongMaterialData>();
        CORRADE_COMPARE(phong.diffuseColor(), (Color4{1.0f}));
        CORRADE_COMPARE(phong.specularColor(), (Color4{1.0f, 0.0f}));
    } {
        const char* name = "metallic/roughness";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::PbrMetallicRoughness);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 8);

        /* Original properties should stay */
        const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::BaseColorTexture));
        CORRADE_COMPARE(pbr.baseColor(), (Color4{0.7f, 0.8f, 0.9f, 1.1f}));
        CORRADE_COMPARE(pbr.baseColorTexture(), 1);
        CORRADE_COMPARE(pbr.baseColorTextureMatrix(), (Matrix3{
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.5f, -1.0f, 1.0f}
        }));
        CORRADE_COMPARE(pbr.baseColorTextureCoordinates(), 3);

        /* ... and should be copied into phong properties as well */
        const auto& phong = material->as<PhongMaterialData>();
        CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
        CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.7f, 0.8f, 0.9f, 1.1f}));
        CORRADE_COMPARE(phong.diffuseTexture(), 1);
        CORRADE_COMPARE(phong.diffuseTextureMatrix(), (Matrix3{
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.5f, -1.0f, 1.0f}
        }));
        CORRADE_COMPARE(phong.diffuseTextureCoordinates(), 3);
        /* Defaults for specular */
        CORRADE_COMPARE(phong.specularColor(), (Color4{1.0f, 0.0f}));
        CORRADE_VERIFY(!phong.hasSpecularTexture());
    } {
        const char* name = "specular/glossiness";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        {
            CORRADE_EXPECT_FAIL("Ideally tinygltf wouldn't define metallic/roughness attributes if not present in the material, but well.");
            CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::PbrSpecularGlossiness);
        }
        CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::PbrMetallicRoughness|MaterialType::PbrSpecularGlossiness);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 10);

        /* Original properties should stay */
        const auto& pbr = material->as<PbrSpecularGlossinessMaterialData>();
        CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::DiffuseTexture));
        CORRADE_COMPARE(pbr.diffuseColor(), (Color4{0.7f, 0.8f, 0.9f, 1.1f}));
        CORRADE_COMPARE(pbr.diffuseTexture(), 1);
        CORRADE_COMPARE(pbr.diffuseTextureMatrix(), (Matrix3{
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.5f, -1.0f, 1.0f}
        }));
        CORRADE_COMPARE(pbr.diffuseTextureCoordinates(), 3);
        CORRADE_COMPARE(pbr.specularColor(), (Color4{0.1f, 0.2f, 0.6f, 0.0f}));
        CORRADE_COMPARE(pbr.specularTexture(), 0);
        CORRADE_COMPARE(pbr.specularTextureMatrix(), (Matrix3{
            {0.5f, 0.0f, 0.0f},
            {0.0f, 0.5f, 0.0f},
            {0.0f, 0.5f, 1.0f}
        }));
        CORRADE_COMPARE(pbr.specularTextureCoordinates(), 2);

        /* Phong recognizes them directly */
        const auto& phong = material->as<PhongMaterialData>();
        CORRADE_VERIFY(phong.hasAttribute(MaterialAttribute::DiffuseTexture));
        CORRADE_COMPARE(phong.diffuseColor(), (Color4{0.7f, 0.8f, 0.9f, 1.1f}));
        CORRADE_COMPARE(phong.diffuseTexture(), 1);
        CORRADE_COMPARE(phong.diffuseTextureMatrix(), (Matrix3{
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.5f, -1.0f, 1.0f}
        }));
        CORRADE_COMPARE(phong.diffuseTextureCoordinates(), 3);
        CORRADE_COMPARE(phong.specularColor(), (Color4{0.1f, 0.2f, 0.6f, 0.0f}));
        CORRADE_COMPARE(phong.specularTexture(), 0);
        CORRADE_COMPARE(phong.specularTextureMatrix(), (Matrix3{
            {0.5f, 0.0f, 0.0f},
            {0.0f, 0.5f, 0.0f},
            {0.0f, 0.5f, 1.0f}
        }));
        CORRADE_COMPARE(phong.specularTextureCoordinates(), 2);
    } {
        const char* name = "unlit";
        auto material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        /* Phong type is added even for unlit materials, since that's how it
           behaved before */
        CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::Flat);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 0);
    }
}

void TinyGltfImporterTest::materialInvalid() {
    auto&& data = MaterialInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->materialCount(), Containers::arraySize(MaterialInvalidData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->material(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::TinyGltfImporter::material(): {}\n", data.message));
}

void TinyGltfImporterTest::materialTexCoordFlip() {
    auto&& data = MaterialTexCoordFlipData[testCaseInstanceId()];
    setTestCaseDescription(Utility::formatString("{}{}", data.name, data.flipInMaterial ? ", textureCoordinateYFlipInMaterial" : ""));

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    /* This should be implicitly enabled on files that contain non-normalized
       integer texture coordinates */
    if(data.flipInMaterial)
        importer->configuration().setValue("textureCoordinateYFlipInMaterial", true);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        data.fileName)));

    auto mesh = importer->mesh(data.meshName);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::TextureCoordinates));
    Containers::Array<Vector2> texCoords = mesh->textureCoordinates2DAsArray();

    /* Texture transform is added to materials that don't have it yet */
    auto material = importer->material(data.name);
    CORRADE_VERIFY(material);

    auto& pbr = static_cast<PbrMetallicRoughnessMaterialData&>(*material);
    CORRADE_COMPARE(pbr.hasTextureTransformation(), data.flipInMaterial || data.hasTextureTransformation);
    CORRADE_VERIFY(pbr.hasCommonTextureTransformation());

    /* Transformed texture coordinates should be the same regardless of the
       setting */
    MeshTools::transformPointsInPlace(pbr.commonTextureMatrix(), texCoords);
    CORRADE_COMPARE_AS(texCoords, Containers::arrayView<Vector2>({
        {1.0f, 0.5f},
        {0.5f, 1.0f},
        {0.0f, 0.0f}
    }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::texture() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture" + std::string{data.suffix})));
    CORRADE_COMPARE(importer->materialCount(), 1);

    auto material = importer->material(0);

    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->types(), MaterialType::PbrMetallicRoughness);

    const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
    CORRADE_VERIFY(pbr.hasAttribute(MaterialAttribute::BaseColorTexture));
    CORRADE_COMPARE(pbr.baseColorTexture(), 0);

    CORRADE_COMPARE(importer->textureCount(), 2);
    CORRADE_COMPARE(importer->textureForName("Texture"), 1);
    CORRADE_COMPARE(importer->textureName(1), "Texture");

    auto texture = importer->texture(1);
    CORRADE_VERIFY(texture);
    CORRADE_VERIFY(texture->importerState());
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->type(), TextureData::Type::Texture2D);

    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Nearest);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Nearest);
    CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Nearest);

    CORRADE_COMPARE(texture->wrapping(), Array3D<SamplerWrapping>(SamplerWrapping::MirroredRepeat, SamplerWrapping::ClampToEdge, SamplerWrapping::Repeat));

    /* Texture coordinates */
    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);

    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::TextureCoordinates), 2);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::TextureCoordinates), VertexFormat::Vector2);
    CORRADE_COMPARE_AS(mesh->attribute<Vector2>(MeshAttribute::TextureCoordinates, 0),
        Containers::arrayView<Vector2>({
            {0.94991f, 0.05009f}, {0.3f, 0.94991f}, {0.1f, 0.2f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(mesh->attribute<Vector2>(MeshAttribute::TextureCoordinates, 1),
        Containers::arrayView<Vector2>({
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

void TinyGltfImporterTest::textureEmptySampler() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture-empty-sampler" + std::string{data.suffix})));

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
    CORRADE_VERIFY(image->importerState());
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc4x4RGBAUnorm);

    /* The texture refers to the image indirectly via an extension, test the
       mapping */
    auto texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), 1);
}

void TinyGltfImporterTest::imageMipLevels() {
    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    /* Import as RGBA so we can verify the pixels */
    _manager.metadata("BasisImporter")->configuration().setValue("format", "RGBA8");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, "image-basis.gltf")));
    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(1), 2);

    /* Verify that loading a different image will properly switch to another
       importer instance */
    Containers::Optional<ImageData2D> image0 = importer->image2D(0);
    Containers::Optional<ImageData2D> image10 = importer->image2D(1);
    Containers::Optional<ImageData2D> image11 = importer->image2D(1, 1);

    CORRADE_VERIFY(image0);
    CORRADE_VERIFY(image0->importerState());
    CORRADE_VERIFY(!image0->isCompressed());
    CORRADE_COMPARE(image0->size(), (Vector2i{5, 3}));
    CORRADE_COMPARE(image0->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(Containers::arrayCast<const UnsignedByte>(image0->data()),
        Containers::arrayView<UnsignedByte>({
            168, 167, 172, 255, 157, 158, 160, 255, 173, 173, 172, 255,
            187, 187, 186, 255, 179, 180, 182, 255, 176, 177, 182, 255,
            160, 160, 161, 255, 159, 159, 160, 255, 188, 188, 186, 255,
            204, 204, 204, 255, 178, 180, 185, 255, 184, 185, 187, 255,
            193, 195, 194, 255, 188, 189, 191, 255, 184, 184, 188, 255
        }), TestSuite::Compare::Container);

    CORRADE_VERIFY(image10);
    CORRADE_VERIFY(image10->importerState());
    CORRADE_VERIFY(!image10->isCompressed());
    CORRADE_COMPARE(image10->size(), (Vector2i{5, 3}));
    CORRADE_COMPARE(image10->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(Containers::arrayCast<const UnsignedByte>(image10->data()),
        Containers::arrayView<UnsignedByte>({
            /* Should be different from the above because this is
               Basis-encoded, not a PNG */
            168, 168, 168, 255, 156, 156, 156, 255, 168, 168, 168, 255,
            190, 190, 190, 255, 182, 182, 190, 255, 178, 178, 178, 255,
            156, 156, 156, 255, 156, 156, 156, 255, 190, 190, 190, 255,
            202, 202, 210, 255, 178, 178, 178, 255, 190, 190, 190, 255,
            190, 190, 190, 255, 190, 190, 190, 255, 182, 182, 190, 255
        }), TestSuite::Compare::Container);

    CORRADE_VERIFY(image11);
    CORRADE_VERIFY(image11->importerState());
    CORRADE_VERIFY(!image11->isCompressed());
    CORRADE_COMPARE(image11->size(), (Vector2i{2, 1}));
    CORRADE_COMPARE(image11->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(Containers::arrayCast<const UnsignedByte>(image11->data()),
        Containers::arrayView<UnsignedByte>({
            172, 172, 181, 255, 184, 184, 193, 255
        }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::fileCallbackBuffer() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    Utility::Resource rs{"data"};
    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy policy, Utility::Resource& rs) {
        Debug{} << "Loading" << filename << "with" << policy;
        return Containers::optional(rs.getRaw(filename));
    }, rs);

    /* Using a different name from the filesystem to avoid false positive
       when the file gets loaded from a filesystem */
    CORRADE_VERIFY(importer->openFile("some/path/data" + std::string{data.suffix}));

    CORRADE_COMPARE(importer->meshCount(), 1);
    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Points);
    CORRADE_VERIFY(!mesh->isIndexed());

    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_COMPARE_AS(mesh->positions3DAsArray(), Containers::arrayView<Vector3>({
        {1.0f, 2.0f, 3.0f}
    }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::fileCallbackBufferNotFound() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

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
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

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
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

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

    CORRADE_COMPARE(importer->meshCount(), 1);
    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Points);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_COMPARE_AS(mesh->positions3DAsArray(0), Containers::arrayView<Vector3>({
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
