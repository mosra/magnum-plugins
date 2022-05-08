/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
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
#include <Corrade/Containers/Triple.h>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/Resource.h>
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
    void openExternalDataNoUri();

    void requiredExtensions();
    void requiredExtensionsUnsupported();

    void animation();
    void animationInvalid();
    void animationTrackSizeMismatch();
    void animationMissingTargetNode();

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
    void cameraInvalidType();

    void light();
    void lightInvalid();
    void lightMissingType();
    void lightMissingSpot();

    void scene();
    void sceneInvalid();
    void sceneInvalidHierarchy();
    void sceneDefaultNoScenes();
    void sceneDefaultNoDefault();
    void sceneDefaultOutOfBounds();
    void sceneTransformation();
    void sceneTransformationQuaternionNormalizationEnabled();
    void sceneTransformationQuaternionNormalizationDisabled();

    void skin();
    void skinInvalid();
    void skinNoJointsProperty();

    void mesh();
    void meshNoAttributes();
    void meshNoIndices();
    void meshNoIndicesNoAttributes();
    void meshColors();
    void meshSkinAttributes();
    void meshCustomAttributes();
    void meshCustomAttributesNoFileOpened();
    void meshDuplicateAttributes();
    void meshUnorderedAttributes();
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
    void textureExtensions();
    void textureInvalid();

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
    void escapedStrings();
    void encodedUris();

    void versionSupported();
    void versionUnsupported();

    void openTwice();
    void importTwice();

    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

using namespace Containers::Literals;

/* The external-data.* files are packed in via a resource, filename mapping
   done in resources.conf */

constexpr struct {
    const char* name;
    Containers::ArrayView<const char> shortData;
    const char* shortDataError;
} OpenErrorData[]{
    {"ascii", {"?", 1}, "JSON string too short."},
    {"binary", {"glTF?", 5}, "Too short data size for glTF Binary."}
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
    {"invalid input accessor", "accessor 3 needs 40 bytes but bufferView 0 has only 0"},
    {"invalid output accessor", "accessor 4 needs 120 bytes but bufferView 0 has only 0"},
    {"sampler index out of bounds", "sampler 1 out of bounds for 1 samplers"},
    {"node index out of bounds", "target node 2 out of bounds for 2 nodes"},
    {"sampler input accessor index out of bounds", "accessor 5 out of bounds for 5 accessors"},
    {"sampler output accessor index out of bounds", "accessor 6 out of bounds for 5 accessors"}
};

constexpr struct {
    const char* name;
    const char* message;
} LightInvalidData[]{
    {"unknown type", "invalid light type what"},
    {"directional with range", "range can't be defined for a directional light"},
    {"spot with too small inner angle", "inner and outer cone angle Deg(-0.572958) and Deg(45) out of allowed bounds"},
    /* These are kinda silly (not sure why inner can't be the same as outer),
       but let's follow the spec */
    {"spot with too large outer angle", "inner and outer cone angle Deg(0) and Deg(90.5273) out of allowed bounds"},
    {"spot with inner angle same as outer", "inner and outer cone angle Deg(14.3239) and Deg(14.3239) out of allowed bounds"},
    {"invalid color size", "expected three values for a color, got 4"}
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
    {"wrong accessor count", "invalid inverse bind matrix count, expected 2 but got 3"},
    {"invalid accessor", "accessor 3 needs 196 bytes but bufferView 0 has only 192"}
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
        VertexFormat::Vector2us, VertexFormat::UnsignedByte, "_SEMANTIC"}
};

const struct {
    const char* name;
    Containers::String file;
    const char* message;
} MeshInvalidData[]{
    {"invalid primitive", "mesh-invalid.gltf", "unrecognized primitive 666"},
    {"different vertex count for each accessor", "mesh-invalid.gltf", "mismatched vertex count for attribute TEXCOORD_0, expected 3 but got 4"},
    {"unexpected position type", "mesh-invalid.gltf", "unexpected POSITION type 2"},
    {"unsupported position component type", "mesh-invalid.gltf", "unsupported POSITION component type unnormalized 5125"},
    {"unexpected normal type", "mesh-invalid.gltf", "unexpected NORMAL type 2"},
    {"unsupported normal component type", "mesh-invalid.gltf", "unsupported NORMAL component type unnormalized 5125"},
    {"unexpected tangent type", "mesh-invalid.gltf", "unexpected TANGENT type 3"},
    {"unsupported tangent component type", "mesh-invalid.gltf", "unsupported TANGENT component type unnormalized 5120"},
    {"unexpected texcoord type", "mesh-invalid.gltf", "unexpected TEXCOORD type 3"},
    {"unsupported texcoord component type", "mesh-invalid.gltf", "unsupported TEXCOORD component type unnormalized 5125"},
    {"unexpected color type", "mesh-invalid.gltf", "unexpected COLOR type 2"},
    {"unsupported color component type", "mesh-invalid.gltf", "unsupported COLOR component type unnormalized 5120"},
    {"unexpected joints type", "mesh-invalid.gltf", "unexpected JOINTS type 3"},
    {"unsupported joints component type", "mesh-invalid.gltf", "unsupported JOINTS component type unnormalized 5120"},
    {"unexpected weights type", "mesh-invalid.gltf", "unexpected WEIGHTS type 65"},
    {"unsupported weights component type", "mesh-invalid.gltf", "unsupported WEIGHTS component type unnormalized 5120"},
    {"unexpected object id type", "mesh-invalid.gltf", "unexpected object ID type 2"},
    {"unsupported object id component type", "mesh-invalid.gltf", "unsupported object ID component type unnormalized 5122"},
    {"unexpected index type", "mesh-invalid.gltf", "unexpected index type 2"},
    {"unsupported index component type", "mesh-invalid.gltf", "unexpected index component type 5122"},
    {"normalized index type", "mesh-invalid.gltf", "index type can't be normalized"},
    {"strided index view", "mesh-invalid.gltf", "index bufferView is not contiguous"},
    {"accessor type size larger than buffer stride", "mesh-invalid.gltf", "16-byte type defined by accessor 10 can't fit into bufferView 0 stride of 12"},
    {"normalized float", "mesh-invalid.gltf", "component type 5126 can't be normalized"},
    {"normalized double", "mesh-invalid.gltf", "component type 5130 can't be normalized"},
    {"normalized int", "mesh-invalid.gltf", "component type 5125 can't be normalized"},
    {"non-normalized byte matrix", "mesh-invalid.gltf", "unsupported matrix component type unnormalized 5120"},
    {"sparse accessor", "mesh-invalid.gltf", "accessor 14 is using sparse storage, which is unsupported"},
    {"no bufferview", "mesh-invalid.gltf", "accessor 15 has no bufferView"},
    {"accessor range out of bounds", "mesh-invalid.gltf", "accessor 18 needs 48 bytes but bufferView 0 has only 36"},
    {"buffer view range out of bounds", "mesh-invalid.gltf", "bufferView 3 needs 60 bytes but buffer 1 has only 59"},
    {"buffer index out of bounds", "mesh-invalid.gltf", "buffer 2 out of bounds for 2 buffers"},
    {"buffer view index out of bounds", "mesh-invalid.gltf", "bufferView 5 out of bounds for 5 views"},
    {"accessor index out of bounds", "mesh-invalid-accessor-oob.gltf", "accessor 2 out of bounds for 2 accessors"},
    {"multiple buffers", "mesh-invalid.gltf", "meshes spanning multiple buffers are not supported"},
    {"invalid index accessor", "mesh-invalid.gltf", "accessor 17 needs 40 bytes but bufferView 0 has only 36"}
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
    {"invalid texture index clearcoat normal", "clearcoatNormalTexture index 2 out of bounds for 2 textures"}
};

constexpr struct {
    const char* name;
    const char* message;
} SceneInvalidData[]{
    {"camera out of bounds", "camera index 1 in node 3 out of bounds for 1 cameras"},
    {"child out of bounds", "child index 11 in node 10 out of bounds for 11 nodes"},
    {"light out of bounds", "light index 2 in node 4 out of bounds for 2 lights"},
    {"material out of bounds", "material index 4 in node 5 out of bounds for 4 materials"},
    {"material in a multi-primitive mesh out of bounds", "material index 5 in node 6 out of bounds for 4 materials"},
    {"mesh out of bounds", "mesh index 4 in node 7 out of bounds for 4 meshes"},
    {"node out of bounds", "node index 11 out of bounds for 11 nodes"},
    {"skin out of bounds", "skin index 3 in node 8 out of bounds for 3 skins"},
    {"skin for a multi-primitive mesh out of bounds", "skin index 3 in node 9 out of bounds for 3 skins"}
};

constexpr struct {
    const char* name;
    const char* file;
    const char* message;
} SceneInvalidHierarchyData[]{
    {"scene node has parent", "scene-invalid-child-not-root.gltf", "node 1 in scene 0 is not a root node"},
    {"node has multiple parents", "scene-invalid-multiple-parents.gltf", "node 2 has multiple parents"},
    {"child is self", "scene-invalid-cycle.gltf", "node tree contains cycle starting at node 0"},
    {"great-grandchild is self", "scene-invalid-cycle-deep.gltf", "node tree contains cycle starting at node 0"},
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
    const UnsignedInt id;
} TextureExtensionsData[]{
    {"GOOGLE_texture_basis", 1},
    {"KHR_texture_basisu", 2},
    /* unknown extension, falls back to default source */
    {"MSFT_texture_dds", 0},
    {"MSFT_texture_dds and GOOGLE_texture_basis", 1},
    /* KHR_texture_basisu has preference */
    {"GOOGLE_texture_basis and KHR_texture_basisu", 2},
    {"unknown extension", 0},
    {"GOOGLE_texture_basis and unknown", 1}
};

constexpr struct {
    const char* name;
    const char* message;
} TextureInvalidData[]{
    {"invalid sampler minFilter", "invalid minFilter 1"},
    {"invalid sampler magFilter", "invalid magFilter 2"},
    {"invalid sampler wrapS", "invalid wrap mode 3"},
    {"invalid sampler wrapT", "invalid wrap mode 4"},
    {"sampler out of bounds", "sampler 5 out of bounds for 5 samplers"},
    {"image out of bounds", "image 1 out of bounds for 1 images"},
    {"missing source", "no image source found"},
    {"out of bounds GOOGLE_texture_basis", "GOOGLE_texture_basis image 3 out of bounds for 1 images"},
    {"out of bounds KHR_texture_basisu", "KHR_texture_basisu image 4 out of bounds for 1 images"},
    {"unknown extension, no fallback", "no image source found"}
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

constexpr struct {
    const char* name;
    const char* file;
    const char* message;
} UnsupportedVersionData[]{
    {"legacy major version", "version-legacy.gltf", "unsupported version 1.0, expected 2.x"},
    {"unknown major version", "version-unsupported.gltf", "unsupported version 3.0, expected 2.x"},
    {"unknown minor version", "version-unsupported-min.gltf", "unsupported minVersion 2.1, expected 2.0"}
};

using namespace Magnum::Math::Literals;

TinyGltfImporterTest::TinyGltfImporterTest() {
    addInstancedTests({&TinyGltfImporterTest::open},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&TinyGltfImporterTest::openError},
                      Containers::arraySize(OpenErrorData));

    addInstancedTests({&TinyGltfImporterTest::openExternalDataNotFound,
                       &TinyGltfImporterTest::openExternalDataNoPathNoCallback,
                       &TinyGltfImporterTest::openExternalDataWrongSize,
                       &TinyGltfImporterTest::openExternalDataNoUri},
                      Containers::arraySize(SingleFileData));

    addTests({&TinyGltfImporterTest::requiredExtensions,
              &TinyGltfImporterTest::requiredExtensionsUnsupported});

    addInstancedTests({&TinyGltfImporterTest::animation},
                      Containers::arraySize(MultiFileData));

    addInstancedTests({&TinyGltfImporterTest::animationInvalid},
        Containers::arraySize(AnimationInvalidData));

    addTests({&TinyGltfImporterTest::animationTrackSizeMismatch,
              &TinyGltfImporterTest::animationMissingTargetNode});

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

    addTests({&TinyGltfImporterTest::camera,
              &TinyGltfImporterTest::cameraInvalidType,
              &TinyGltfImporterTest::light});

    addInstancedTests({&TinyGltfImporterTest::lightInvalid},
        Containers::arraySize(LightInvalidData));

    addTests({&TinyGltfImporterTest::lightMissingType,
              &TinyGltfImporterTest::lightMissingSpot,

              &TinyGltfImporterTest::scene});

    addInstancedTests({&TinyGltfImporterTest::sceneInvalid},
        Containers::arraySize(SceneInvalidData));

    addInstancedTests({&TinyGltfImporterTest::sceneInvalidHierarchy},
        Containers::arraySize(SceneInvalidHierarchyData));

    addTests({&TinyGltfImporterTest::sceneDefaultNoScenes,
              &TinyGltfImporterTest::sceneDefaultNoDefault,
              &TinyGltfImporterTest::sceneDefaultOutOfBounds,
              &TinyGltfImporterTest::sceneTransformation,
              &TinyGltfImporterTest::sceneTransformationQuaternionNormalizationEnabled,
              &TinyGltfImporterTest::sceneTransformationQuaternionNormalizationDisabled});

    addInstancedTests({&TinyGltfImporterTest::skin},
        Containers::arraySize(MultiFileData));

    addInstancedTests({&TinyGltfImporterTest::skinInvalid},
        Containers::arraySize(SkinInvalidData));

    addTests({&TinyGltfImporterTest::skinNoJointsProperty});

    addInstancedTests({&TinyGltfImporterTest::mesh},
                      Containers::arraySize(MultiFileData));

    addTests({&TinyGltfImporterTest::meshNoAttributes,
              &TinyGltfImporterTest::meshNoIndices,
              &TinyGltfImporterTest::meshNoIndicesNoAttributes,
              &TinyGltfImporterTest::meshColors,
              &TinyGltfImporterTest::meshSkinAttributes,
              &TinyGltfImporterTest::meshCustomAttributes,
              &TinyGltfImporterTest::meshCustomAttributesNoFileOpened,
              &TinyGltfImporterTest::meshDuplicateAttributes,
              &TinyGltfImporterTest::meshUnorderedAttributes,
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

    addTests({&TinyGltfImporterTest::texture});

    addInstancedTests({&TinyGltfImporterTest::textureExtensions},
                      Containers::arraySize(TextureExtensionsData));

    addInstancedTests({&TinyGltfImporterTest::textureInvalid},
                      Containers::arraySize(TextureInvalidData));

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

    addTests({&TinyGltfImporterTest::utf8filenames,
              &TinyGltfImporterTest::escapedStrings,
              &TinyGltfImporterTest::encodedUris,

              &TinyGltfImporterTest::versionSupported});

    addInstancedTests({&TinyGltfImporterTest::versionUnsupported},
                      Containers::arraySize(UnsupportedVersionData));

    addTests({&TinyGltfImporterTest::openTwice,
              &TinyGltfImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. */
    #ifdef TINYGLTFIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(TINYGLTFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
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

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "empty"_s + data.suffix);
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_VERIFY(importer->importerState());

    Containers::Optional<Containers::Array<char>> file = Utility::Path::read(filename);
    CORRADE_VERIFY(file);
    CORRADE_VERIFY(importer->openData(*file));
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
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::TinyGltfImporter::openData(): error opening file: {}\n", data.shortDataError));
}

void TinyGltfImporterTest::openExternalDataNotFound() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "buffer-invalid-notfound"_s + data.suffix)));
    /* There's an error from Path::read() before */
    CORRADE_COMPARE_AS(out.str(),
        "\nTrade::TinyGltfImporter::openData(): error opening file: File read error : /nonexistent.bin : file reading failed\n",
        TestSuite::Compare::StringHasSuffix);
}

void TinyGltfImporterTest::openExternalDataNoPathNoCallback() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    Containers::Optional<Containers::Array<char>> file = Utility::Path::read(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "buffer-invalid-notfound"_s + data.suffix));
    CORRADE_VERIFY(file);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(*file));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: File read error : /nonexistent.bin : external buffers can be imported only when opening files from the filesystem or if a file callback is present\n");
}

void TinyGltfImporterTest::openExternalDataWrongSize() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    {
        /* These files are actually valid, but it's tinygltf we're using here,
           so bugs are expected */
        CORRADE_EXPECT_FAIL_IF(data.suffix == std::string{".glb"},
            "tinygltf doesn't check for correct buffer size in GLBs.");
        CORRADE_VERIFY(!importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "buffer-long-size"_s + data.suffix)));
        CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: File size mismatch : external-data.bin, requestedBytes 6, but got 12\n");
    }
}

void TinyGltfImporterTest::openExternalDataNoUri() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "buffer-invalid-no-uri"_s + data.suffix)));
    {
        CORRADE_EXPECT_FAIL_IF(data.suffix == ".glb"_s,
            "tinygltf incorrectly detects all buffers without URI as GLB BIN buffer.");
        CORRADE_COMPARE(out.str(),
            "Trade::TinyGltfImporter::openData(): error opening file: 'uri' is missing from non binary glTF file buffer.\n"
            "File not found :\n" /* tinygltf seems to continue to try to load a file from an empty URI */);
    }
}

void TinyGltfImporterTest::requiredExtensions() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "required-extensions.gltf")));
}

void TinyGltfImporterTest::requiredExtensionsUnsupported() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    {
        CORRADE_EXPECT_FAIL("TinyGltfImporter ignores required extensions.");
        CORRADE_VERIFY(!importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "required-extensions-unsupported.gltf")));
    }
}

void TinyGltfImporterTest::animation() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation"_s + data.suffix)));

    CORRADE_COMPARE(importer->animationCount(), 4);
    CORRADE_COMPARE(importer->animationName(2), "TRS animation");
    CORRADE_COMPARE(importer->animationForName("TRS animation"), 2);
    CORRADE_COMPARE(importer->animationForName("Nonexistent"), -1);

    /* Empty animation */
    {
        Containers::Optional<Trade::AnimationData> animation = importer->animation("empty");
        CORRADE_VERIFY(animation);
        CORRADE_VERIFY(animation->data().isEmpty());
        CORRADE_COMPARE(animation->trackCount(), 0);

    /* Empty translation/rotation/scaling animation */
    } {
        Containers::Optional<Trade::AnimationData> animation = importer->animation("empty TRS animation");
        CORRADE_VERIFY(animation);
        CORRADE_VERIFY(animation->importerState());

        CORRADE_COMPARE(animation->data().size(), 0);
        CORRADE_COMPARE(animation->trackCount(), 3);

        /* Not really checking much here, just making sure that this is handled
           gracefully */

        CORRADE_COMPARE(animation->trackTargetType(0), AnimationTrackTargetType::Rotation3D);
        const Animation::TrackViewStorage<const Float>& rotation = animation->track(0);
        CORRADE_VERIFY(rotation.keys().isEmpty());
        CORRADE_VERIFY(rotation.values().isEmpty());

        CORRADE_COMPARE(animation->trackTargetType(1), AnimationTrackTargetType::Translation3D);
        const Animation::TrackViewStorage<const Float>& translation = animation->track(1);
        CORRADE_VERIFY(translation.keys().isEmpty());
        CORRADE_VERIFY(translation.values().isEmpty());

        CORRADE_COMPARE(animation->trackTargetType(2), AnimationTrackTargetType::Scaling3D);
        const Animation::TrackViewStorage<const Float>& scaling = animation->track(2);
        CORRADE_VERIFY(scaling.keys().isEmpty());
        CORRADE_VERIFY(scaling.values().isEmpty());

    /* Translation/rotation/scaling animation */
    } {
        Containers::Optional<Trade::AnimationData> animation = importer->animation("TRS animation");
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
        CORRADE_COMPARE_AS(rotation.keys(), Containers::stridedArrayView(rotationKeys), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(rotation.values(), Containers::stridedArrayView(rotationValues), TestSuite::Compare::Container);
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
        CORRADE_COMPARE_AS(translation.keys(), Containers::stridedArrayView(translationScalingKeys), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(translation.values(), Containers::stridedArrayView(translationData), TestSuite::Compare::Container);
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
        CORRADE_COMPARE_AS(scaling.keys(), Containers::stridedArrayView(translationScalingKeys), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scaling.values(), Containers::stridedArrayView(scalingData), TestSuite::Compare::Container);
        CORRADE_COMPARE(scaling.at(1.5f), Vector3::zScale(5.2f));
    }

    /* Fourth animation tested in animationSpline() */
}

void TinyGltfImporterTest::animationInvalid() {
    auto&& data = AnimationInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "animation-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(Containers::arraySize(AnimationInvalidData), importer->animationCount());

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::TinyGltfImporter::animation(): {}\n", data.message));
}

void TinyGltfImporterTest::animationTrackSizeMismatch() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "animation-invalid-track-size-mismatch.gltf")));

    std::ostringstream out;
    Error redirectError{&out};
    {
        CORRADE_EXPECT_FAIL("TinyGLTF doesn't complain when track time and target tracks don't have the same size.");
        CORRADE_VERIFY(!importer->animation(0));
        CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::animation(): target track size doesn't match time track size, expected 3 but got 2\n");
    }
}

void TinyGltfImporterTest::animationMissingTargetNode() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation-missing-target-node.gltf")));
    CORRADE_COMPARE(importer->animationCount(), 1);

    /* tinygltf skips channels that don't have a target node */

    Containers::Optional<Trade::AnimationData> animation = importer->animation(0);
    CORRADE_VERIFY(animation);
    CORRADE_COMPARE(animation->trackCount(), 2);

    CORRADE_COMPARE(animation->trackTargetType(0), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(0), 1);
    CORRADE_COMPARE(animation->trackTargetType(1), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(1), 0);
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation"_s + data.suffix)));

    Containers::Optional<Trade::AnimationData> animation = importer->animation("TRS animation, splines");
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
    CORRADE_COMPARE_AS(rotation.keys(), Containers::stridedArrayView(AnimationSplineTime1Keys), TestSuite::Compare::Container);
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
    CORRADE_COMPARE_AS(rotation.values(), Containers::stridedArrayView(rotationValues), TestSuite::Compare::Container);
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
    CORRADE_COMPARE_AS(translation.keys(), Containers::stridedArrayView(AnimationSplineTime1Keys), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(translation.values(), Containers::stridedArrayView(AnimationSplineTime1TranslationData), TestSuite::Compare::Container);
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
    CORRADE_COMPARE_AS(scaling.keys(), Containers::stridedArrayView(AnimationSplineTime1Keys), TestSuite::Compare::Container);
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
    CORRADE_COMPARE_AS(scaling.values(), Containers::stridedArrayView(scalingData), TestSuite::Compare::Container);
    CORRADE_COMPARE(scaling.at(0.5f + 0.35f*3),
        (Vector3{0.118725f, 0.8228f, -2.711f}));
}

void TinyGltfImporterTest::animationSplineSharedWithSameTimeTrack() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation-splines-sharing.gltf")));

    Containers::Optional<Trade::AnimationData> animation = importer->animation("TRS animation, splines, sharing data with the same time track");
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
    CORRADE_COMPARE_AS(translation.keys(), Containers::stridedArrayView(AnimationSplineTime1Keys), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(translation.values(), Containers::stridedArrayView(AnimationSplineTime1TranslationData), TestSuite::Compare::Container);
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
    CORRADE_COMPARE_AS(scaling.keys(), Containers::stridedArrayView(AnimationSplineTime1Keys), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scaling.values(), Containers::stridedArrayView(AnimationSplineTime1TranslationData), TestSuite::Compare::Container);
    /* The same as in CubicHermiteTest::splerpVector() */
    CORRADE_COMPARE(scaling.at(0.5f + 0.35f*3),
        (Vector3{1.04525f, 0.357862f, 0.540875f}));
}

void TinyGltfImporterTest::animationSplineSharedWithDifferentTimeTrack() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation-splines-sharing.gltf")));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation("TRS animation, splines, sharing data with different time track"));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::animation(): spline track is shared with different time tracks, we don't support that, sorry\n");
}

void TinyGltfImporterTest::animationShortestPathOptimizationEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("optimizeQuaternionShortestPath"));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation-patching.gltf")));

    Containers::Optional<Trade::AnimationData> animation = importer->animation("Quaternion shortest-path patching");
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

void TinyGltfImporterTest::animationShortestPathOptimizationDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Explicitly disable */
    importer->configuration().setValue("optimizeQuaternionShortestPath", false);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation-patching.gltf")));

    Containers::Optional<Trade::AnimationData> animation = importer->animation("Quaternion shortest-path patching");
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

void TinyGltfImporterTest::animationQuaternionNormalizationEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("normalizeQuaternions"));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation-patching.gltf")));

    Containers::Optional<AnimationData> animation;
    std::ostringstream out;
    {
        Warning warningRedirection{&out};
        animation = importer->animation("Quaternion normalization patching");
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
    CORRADE_COMPARE_AS(track.values(), Containers::stridedArrayView(rotationValues), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::animationQuaternionNormalizationDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Explicitly disable */
    CORRADE_VERIFY(importer->configuration().setValue("normalizeQuaternions", false));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation-patching.gltf")));

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
    CORRADE_COMPARE_AS(track.values(), Containers::stridedArrayView(rotationValues), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::animationMergeEmpty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Enable animation merging */
    importer->configuration().setValue("mergeAnimationClips", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "empty.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->animationForName(""), -1);
}

void TinyGltfImporterTest::animationMerge() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Enable animation merging */
    importer->configuration().setValue("mergeAnimationClips", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 1);
    CORRADE_COMPARE(importer->animationName(0), "");
    CORRADE_COMPARE(importer->animationForName(""), -1);

    Containers::Optional<Trade::AnimationData> animation = importer->animation(0);
    CORRADE_VERIFY(animation);
    CORRADE_VERIFY(!animation->importerState()); /* No particular clip */
    /*
        -   Nothing from the first animation
        -   Empty T/R/S tracks from the second animation
        -   Two rotation keys, four translation and scaling keys with common
            time track from the third animation
        -   Four T/R/S spline-interpolated keys with a common time tracks
            from the fourth animation
    */
    CORRADE_COMPARE(animation->data().size(),
        2*(sizeof(Float) + sizeof(Quaternion)) +
        4*(sizeof(Float) + 2*sizeof(Vector3)) +
        4*(sizeof(Float) + 3*(sizeof(Quaternion) + 2*sizeof(Vector3))));
    /* Or also the same size as the animation binary file, except the time
       sharing part that's tested elsewhere */
    CORRADE_COMPARE(animation->data().size(), 664 - 4*sizeof(Float));
    CORRADE_COMPARE(animation->trackCount(), 9);

    /* Rotation, empty */
    CORRADE_COMPARE(animation->trackType(0), AnimationTrackType::Quaternion);
    CORRADE_COMPARE(animation->trackTargetType(0), AnimationTrackTargetType::Rotation3D);
    CORRADE_COMPARE(animation->trackTarget(0), 0);
    Animation::TrackViewStorage<const Float> rotation = animation->track(0);
    CORRADE_COMPARE(rotation.interpolation(), Animation::Interpolation::Linear);
    CORRADE_VERIFY(rotation.keys().isEmpty());
    CORRADE_VERIFY(rotation.values().isEmpty());

    /* Translation, empty */
    CORRADE_COMPARE(animation->trackType(1), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(1), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(1), 1);
    Animation::TrackViewStorage<const Float> translation = animation->track(1);
    CORRADE_COMPARE(translation.interpolation(), Animation::Interpolation::Constant);
    CORRADE_VERIFY(translation.keys().isEmpty());
    CORRADE_VERIFY(translation.values().isEmpty());

    /* Scaling, empty */
    CORRADE_COMPARE(animation->trackType(2), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(2), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(2), 2);
    Animation::TrackViewStorage<const Float> scaling = animation->track(2);
    CORRADE_COMPARE(scaling.interpolation(), Animation::Interpolation::Linear);
    CORRADE_VERIFY(scaling.keys().isEmpty());
    CORRADE_VERIFY(scaling.values().isEmpty());

    /* Rotation, linearly interpolated */
    CORRADE_COMPARE(animation->trackType(3), AnimationTrackType::Quaternion);
    CORRADE_COMPARE(animation->trackTargetType(3), AnimationTrackTargetType::Rotation3D);
    CORRADE_COMPARE(animation->trackTarget(3), 0);
    Animation::TrackView<const Float, const Quaternion> rotation2 = animation->track<Quaternion>(3);
    CORRADE_COMPARE(rotation2.interpolation(), Animation::Interpolation::Linear);
    CORRADE_COMPARE(rotation2.at(1.875f), Quaternion::rotation(90.0_degf, Vector3::xAxis()));

    /* Translation, constant interpolated, sharing keys with scaling */
    CORRADE_COMPARE(animation->trackType(4), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(4), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(4), 1);
    Animation::TrackView<const Float, const Vector3> translation2 = animation->track<Vector3>(4);
    CORRADE_COMPARE(translation2.interpolation(), Animation::Interpolation::Constant);
    CORRADE_COMPARE(translation2.at(1.5f), Vector3::yAxis(2.5f));

    /* Scaling, linearly interpolated, sharing keys with translation */
    CORRADE_COMPARE(animation->trackType(5), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(5), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(5), 2);
    Animation::TrackView<const Float, const Vector3> scaling2 = animation->track<Vector3>(5);
    CORRADE_COMPARE(scaling2.interpolation(), Animation::Interpolation::Linear);
    CORRADE_COMPARE(scaling2.at(1.5f), Vector3::zScale(5.2f));

    /* Rotation, spline interpolated */
    CORRADE_COMPARE(animation->trackType(6), AnimationTrackType::CubicHermiteQuaternion);
    CORRADE_COMPARE(animation->trackTargetType(6), AnimationTrackTargetType::Rotation3D);
    CORRADE_COMPARE(animation->trackTarget(6), 3);
    Animation::TrackView<const Float, const CubicHermiteQuaternion> rotation3 = animation->track<CubicHermiteQuaternion>(6);
    CORRADE_COMPARE(rotation3.interpolation(), Animation::Interpolation::Spline);
    /* The same as in CubicHermiteTest::splerpQuaternion() */
    CORRADE_COMPARE(rotation3.at(0.5f + 0.35f*3),
        (Quaternion{{-0.309862f, 0.174831f, 0.809747f}, 0.466615f}));

    /* Translation, spline interpolated */
    CORRADE_COMPARE(animation->trackType(7), AnimationTrackType::CubicHermite3D);
    CORRADE_COMPARE(animation->trackTargetType(7), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(7), 4);
    Animation::TrackView<const Float, const CubicHermite3D> translation3 = animation->track<CubicHermite3D>(7);
    CORRADE_COMPARE(translation3.interpolation(), Animation::Interpolation::Spline);
    /* The same as in CubicHermiteTest::splerpVector() */
    CORRADE_COMPARE(translation3.at(0.5f + 0.35f*3),
        (Vector3{1.04525f, 0.357862f, 0.540875f}));

    /* Scaling, spline interpolated */
    CORRADE_COMPARE(animation->trackType(8), AnimationTrackType::CubicHermite3D);
    CORRADE_COMPARE(animation->trackTargetType(8), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(8), 5);
    Animation::TrackView<const Float, const CubicHermite3D> scaling3 = animation->track<CubicHermite3D>(8);
    CORRADE_COMPARE(scaling3.interpolation(), Animation::Interpolation::Spline);
    CORRADE_COMPARE(scaling3.at(0.5f + 0.35f*3),
        (Vector3{0.118725f, 0.8228f, -2.711f}));
}

void TinyGltfImporterTest::camera() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "camera.gltf")));

    CORRADE_COMPARE(importer->cameraCount(), 4);
    CORRADE_COMPARE(importer->cameraName(2), "Perspective 4:3 75° hFoV");
    CORRADE_COMPARE(importer->cameraForName("Perspective 4:3 75° hFoV"), 2);
    CORRADE_COMPARE(importer->cameraForName("Nonexistent"), -1);

    {
        Containers::Optional<Trade::CameraData> cam = importer->camera("Orthographic 4:3");
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->type(), CameraType::Orthographic3D);
        CORRADE_COMPARE(cam->size(), (Vector2{4.0f, 3.0f}));
        CORRADE_COMPARE(cam->aspectRatio(), 1.333333f);
        CORRADE_COMPARE(cam->near(), 0.01f);
        CORRADE_COMPARE(cam->far(), 100.0f);
    } {
        Containers::Optional<Trade::CameraData> cam = importer->camera("Perspective 1:1 75° hFoV");
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->type(), CameraType::Perspective3D);
        CORRADE_COMPARE(cam->fov(), 75.0_degf);
        CORRADE_COMPARE(cam->aspectRatio(), 1.0f);
        CORRADE_COMPARE(cam->near(), 0.1f);
        CORRADE_COMPARE(cam->far(), 150.0f);
    } {
        Containers::Optional<Trade::CameraData> cam = importer->camera("Perspective 4:3 75° hFoV");
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->type(), CameraType::Perspective3D);
        CORRADE_COMPARE(cam->fov(), 75.0_degf);
        CORRADE_COMPARE(cam->aspectRatio(), 4.0f/3.0f);
        CORRADE_COMPARE(cam->near(), 0.1f);
        CORRADE_COMPARE(cam->far(), 150.0f);
    } {
        Containers::Optional<Trade::CameraData> cam = importer->camera("Perspective 16:9 75° hFoV infinite");
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->type(), CameraType::Perspective3D);
        CORRADE_COMPARE(cam->fov(), 75.0_degf);
        CORRADE_COMPARE(cam->aspectRatio(), 16.0f/9.0f);
        CORRADE_COMPARE(cam->near(), 0.1f);
        CORRADE_COMPARE(cam->far(), Constants::inf());
    }
}

void TinyGltfImporterTest::cameraInvalidType() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "camera-invalid-type.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: Invalid camera type: \"oblique\". Must be \"perspective\" or \"orthographic\"\n");
}

void TinyGltfImporterTest::light() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "light.gltf")));

    CORRADE_COMPARE(importer->lightCount(), 4);
    CORRADE_COMPARE(importer->lightName(1), "Spot");
    CORRADE_COMPARE(importer->lightForName("Spot"), 1);
    CORRADE_COMPARE(importer->lightForName("Nonexistent"), -1);

    {
        Containers::Optional<Trade::LightData> light = importer->light("Point with everything implicit");
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Point);
        CORRADE_COMPARE(light->color(), (Color3{1.0f, 1.0f, 1.0f}));
        CORRADE_COMPARE(light->intensity(), 1.0f);
        CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f, 0.0f, 1.0f}));
        CORRADE_COMPARE(light->range(), Constants::inf());
    } {
        Containers::Optional<Trade::LightData> light = importer->light("Spot");
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
        Containers::Optional<Trade::LightData> light = importer->light("Spot with implicit angles");
        CORRADE_VERIFY(light);
        CORRADE_COMPARE(light->type(), LightData::Type::Spot);
        CORRADE_COMPARE(light->innerConeAngle(), 0.0_degf);
        /* glTF has half-angles, we have full angles */
        CORRADE_COMPARE(light->outerConeAngle(), 45.0_degf*2.0f);
    } {
        Containers::Optional<Trade::LightData> light = importer->light("Sun");
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "light-invalid.gltf")));

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
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "light-invalid-missing-type.gltf")));
    /* This error is extremely shitty, but well that's tinygltf, so. */
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: 'type' property is missing.\n");
}

void TinyGltfImporterTest::lightMissingSpot() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "light-invalid-missing-spot.gltf")));
    /* This error is extremely shitty, but well that's tinygltf, so. */
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: Spot light description not found.\n");
}

void TinyGltfImporterTest::scene() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene.gltf")));

    /* Explicit default scene */
    CORRADE_COMPARE(importer->defaultScene(), 1);

    CORRADE_COMPARE(importer->sceneCount(), 3);
    CORRADE_COMPARE(importer->sceneName(1), "Scene");
    CORRADE_COMPARE(importer->sceneForName("Scene"), 1);
    CORRADE_COMPARE(importer->sceneForName("Nonexistent"), -1);

    CORRADE_COMPARE(importer->objectCount(), 8);
    CORRADE_COMPARE(importer->objectName(4), "Light");
    CORRADE_COMPARE(importer->objectForName("Light"), 4);
    CORRADE_COMPARE(importer->objectForName("Nonexistent"), -1);

    /* Empty scene should have no fields except empty transformation (which
       distinguishes between 2D and 3D) and empty parent (which is there always
       to tell which objects belong to the scene), and an empty ImporterState
       (which is there always as well) */
    {
        Containers::Optional<SceneData> scene = importer->scene(0);
        CORRADE_VERIFY(scene);
        CORRADE_VERIFY(scene->importerState());
        CORRADE_VERIFY(scene->is3D());
        CORRADE_COMPARE(scene->fieldCount(), 3);
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_COMPARE(scene->fieldSize(SceneField::Parent), 0);
        CORRADE_VERIFY(scene->hasField(SceneField::Transformation));
        CORRADE_COMPARE(scene->fieldType(SceneField::Transformation), SceneFieldType::Matrix4x4);
        CORRADE_COMPARE(scene->fieldSize(SceneField::Transformation), 0);
        CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
        CORRADE_COMPARE(scene->fieldSize(SceneField::ImporterState), 0);

    /* Testing mainly the hierarchy and light / camera / ... references here.
       Transformations tested in sceneTransformation() and others. */
    } {
        Containers::Optional<SceneData> scene = importer->scene(1);
        CORRADE_VERIFY(scene);
        CORRADE_VERIFY(scene->importerState());
        CORRADE_COMPARE(scene->mappingType(), SceneMappingType::UnsignedInt);
        /* There's object 7 but only in scene 2, so this scene should have
           object count only as a max of all referenced objects  */
        CORRADE_COMPARE(scene->mappingBound(), 7);
        CORRADE_COMPARE(scene->fieldCount(), 8);

        /* Parents. Importer state shares the same object mapping and it's all
           non-null pointers. */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Parent), Containers::arrayView<UnsignedInt>({
            2, 4, 5, 6, /* root */
            3, 1, /* children of node 5 */
            0 /* child of node 1 */
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(
            scene->mapping<UnsignedInt>(SceneField::ImporterState),
            scene->mapping<UnsignedInt>(SceneField::Parent),
            TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
            -1, -1, -1, -1,
            5, 5,
            1
        }), TestSuite::Compare::Container);
        for(const void* a: scene->field<const void*>(SceneField::ImporterState)) {
            CORRADE_VERIFY(a);
        }

        /* No transformations here (tested separately in sceneTransformation()
           and others), however an empty field is still present to annotate a
           3D scene */
        CORRADE_VERIFY(scene->hasField(SceneField::Transformation));
        CORRADE_COMPARE(scene->fieldType(SceneField::Transformation), SceneFieldType::Matrix4x4);
        CORRADE_COMPARE(scene->fieldSize(SceneField::Transformation), 0);
        CORRADE_VERIFY(scene->is3D());

        /* Object 0 has a camera */
        CORRADE_VERIFY(scene->hasField(SceneField::Camera));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Camera), Containers::arrayView<UnsignedInt>({
            0
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Camera), Containers::arrayView<UnsignedInt>({
            2
        }), TestSuite::Compare::Container);

        /* Objects 2, 6, 3 (in order they were discovered) have a mesh, only
           object 3 has a material */
        CORRADE_VERIFY(scene->hasField(SceneField::Mesh));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
            2, 6, 3
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
            1, 1, 0
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Int>(SceneField::MeshMaterial), Containers::arrayView<Int>({
            -1, -1, 1
        }), TestSuite::Compare::Container);

        /* Object 6 has a skin */
        CORRADE_VERIFY(scene->hasField(SceneField::Skin));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Skin), Containers::arrayView<UnsignedInt>({
            6
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Skin), Containers::arrayView<UnsignedInt>({
            1
        }), TestSuite::Compare::Container);

        /* Object 4 has a light */
        CORRADE_VERIFY(scene->hasField(SceneField::Light));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Light), Containers::arrayView<UnsignedInt>({
            4
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Light), Containers::arrayView<UnsignedInt>({
            1
        }), TestSuite::Compare::Container);

    /* Another scene, with no material assignments, so there should be no
       material field. It also references an object that's not in scene 1,
       so the objectCount should account for it. */
    } {
        Containers::Optional<SceneData> scene = importer->scene(2);
        CORRADE_VERIFY(scene);
        CORRADE_VERIFY(scene->importerState());
        CORRADE_COMPARE(scene->mappingType(), SceneMappingType::UnsignedInt);
        CORRADE_COMPARE(scene->mappingBound(), 8);
        CORRADE_COMPARE(scene->fieldCount(), 4);

        /* Parents, importer state, transformation. Assume it behaves like
           above, no need to test again. */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
        CORRADE_VERIFY(scene->hasField(SceneField::Transformation));

        /* Object 2 has a mesh, but since it has no material and there's no
           other mesh with a material, the material field is not present */
        CORRADE_VERIFY(scene->hasField(SceneField::Mesh));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
            2
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
            1
        }), TestSuite::Compare::Container);
        CORRADE_VERIFY(!scene->hasField(SceneField::MeshMaterial));
    }
}

void TinyGltfImporterTest::sceneInvalid() {
    auto&& data = SceneInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "scene-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(Containers::arraySize(SceneInvalidData), importer->sceneCount());

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->scene(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString(
        "Trade::TinyGltfImporter::scene(): {}\n", data.message));
}

void TinyGltfImporterTest::sceneInvalidHierarchy() {
    auto&& data = SceneInvalidHierarchyData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, data.file)));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::TinyGltfImporter::openData(): {}\n", data.message));
}

void TinyGltfImporterTest::sceneDefaultNoScenes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "empty.gltf")));

    /* There is no scene, can't have any default */
    CORRADE_COMPARE(importer->defaultScene(), -1);
    CORRADE_COMPARE(importer->sceneCount(), 0);
}

void TinyGltfImporterTest::sceneDefaultNoDefault() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-default-none.gltf")));

    /* There is at least one scene, it's made default */
    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->sceneCount(), 1);
}

void TinyGltfImporterTest::sceneDefaultOutOfBounds() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-default-oob.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): scene index 0 out of bounds for 0 scenes\n");
}

void TinyGltfImporterTest::sceneTransformation() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-transformation.gltf")));

    CORRADE_COMPARE(importer->sceneCount(), 7);

    /* Scene with all four transformation fields */
    {
        Containers::Optional<SceneData> scene = importer->scene("Everything");
        CORRADE_VERIFY(scene);
        CORRADE_COMPARE(scene->mappingBound(), 7);
        CORRADE_COMPARE(scene->fieldCount(), 6);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));

        /* Transformation matrix is populated for all objects that have *some*
           transformation, the last one has nothing */
        CORRADE_VERIFY(scene->hasField(SceneField::Transformation));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Transformation), Containers::arrayView<UnsignedInt>({
            0, 1, 2, 3, 4, 5
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Matrix4>(SceneField::Transformation), Containers::arrayView<Matrix4>({
            {{0.636397f, 0.0f, -0.636395f, 0.0f},
             {0.0f, 0.5f, -0.0f, 0.0f},
             {1.62634f, 0.0f, 1.62635f, 0.0f},
             {1.5f, -2.5f, 0.3f, 1.0f}},

            {{0.636397f, 0.0f, -0.636395f, 0.0f},
             {0.0f, 0.5f, -0.0f, 0.0f},
             {1.62634f, 0.0f, 1.62635f, 0.0f},
             {1.5f, -2.5f, 0.3f, 1.0f}},

            {{0.636397f, 0.0f, -0.636395f, 0.0f},
             {0.0f, 0.5f, -0.0f, 0.0f},
             {1.62634f,  0.0f, 1.62635f, 0},
             {1.5f, -2.5f, 0.3f, 1.0f}},

            Matrix4::translation({1.5f, -2.5f, 0.3f}),
            Matrix4::rotationY(45.0_degf),
            Matrix4::scaling({0.9f, 0.5f, 2.3f})
        }), TestSuite::Compare::Container);

        /* TRS only for some; object mapping of course shared by all */
        CORRADE_VERIFY(scene->hasField(SceneField::Translation));
        {
            CORRADE_EXPECT_FAIL("tinygltf skips parsing TRS if a matrix is set as well.");
            CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Translation), Containers::arrayView<UnsignedInt>({
                0, 2, 3, 4, 5
            }), TestSuite::Compare::Container);
        }
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Translation), Containers::arrayView<UnsignedInt>({
            /*0,*/ 2, 3, 4, 5
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Vector3>(SceneField::Translation), Containers::arrayView<Vector3>({
            /*{1.5f, -2.5f, 0.3f},*/
            {1.5f, -2.5f, 0.3f},
            {1.5f, -2.5f, 0.3f},
            {},
            {}
        }), TestSuite::Compare::Container);
        CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
        CORRADE_COMPARE_AS(scene->field<Quaternion>(SceneField::Rotation), Containers::arrayView<Quaternion>({
            /*Quaternion::rotation(45.0_degf, Vector3::yAxis()),*/
            Quaternion::rotation(45.0_degf, Vector3::yAxis()),
            {},
            Quaternion::rotation(45.0_degf, Vector3::yAxis()),
            {}
        }), TestSuite::Compare::Container);
        CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
        CORRADE_COMPARE_AS(scene->field<Vector3>(SceneField::Scaling), Containers::arrayView<Vector3>({
            /*{0.9f, 0.5f, 2.3f},*/
            {0.9f, 0.5f, 2.3f},
            Vector3{1.0f},
            Vector3{1.0f},
            {0.9f, 0.5f, 2.3f},
        }), TestSuite::Compare::Container);

    /* Both matrices and TRS (and the implicit transformation) */
    } {
        Containers::Optional<SceneData> scene = importer->scene("Matrix + TRS");
        CORRADE_VERIFY(scene);
        {
            CORRADE_EXPECT_FAIL("tinygltf skips parsing TRS if a matrix is set as well.");
            CORRADE_COMPARE(scene->fieldCount(), 5);
        }

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));

        /* Assuming both matrices and TRS represent the same, the matrix is
           considered redundant and so only TRS is present in the output. Well,
           you wish it would. */
        {
            CORRADE_EXPECT_FAIL("tinygltf skips parsing TRS if a matrix is set as well.");
            CORRADE_VERIFY(!scene->hasField(SceneField::Transformation));
            CORRADE_VERIFY(scene->hasField(SceneField::Translation));
            CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
            CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
        }

    /* Just matrices (and the implicit transformation) */
    } {
        Containers::Optional<SceneData> scene = importer->scene("Just matrices");
        CORRADE_VERIFY(scene);
        CORRADE_COMPARE(scene->fieldCount(), 3);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));

        /* Transformation matrix is populated for the first, the second object
           has nothing */
        CORRADE_VERIFY(scene->hasField(SceneField::Transformation));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Transformation), Containers::arrayView<UnsignedInt>({
            1
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Matrix4>(SceneField::Transformation), Containers::arrayView<Matrix4>({
            {{0.636397f, 0.0f, -0.636395f, 0.0f},
             {0.0f, 0.5f, -0.0f, 0.0f},
             {1.62634f, 0.0f, 1.62635f, 0.0f},
             {1.5f, -2.5f, 0.3f, 1.0f}},
        }), TestSuite::Compare::Container);

    /* Just TRS (and the implicit transformation) */
    } {
        Containers::Optional<SceneData> scene = importer->scene("Just TRS");
        CORRADE_VERIFY(scene);
        CORRADE_COMPARE(scene->fieldCount(), 5);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));

        /* The implicit transformation object is not contained in these */
        CORRADE_VERIFY(scene->hasField(SceneField::Translation));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Translation), Containers::arrayView<UnsignedInt>({
            2, 3, 4, 5
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Vector3>(SceneField::Translation), Containers::arrayView<Vector3>({
            {1.5f, -2.5f, 0.3f},
            {1.5f, -2.5f, 0.3f},
            {},
            {}
        }), TestSuite::Compare::Container);
        CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
        CORRADE_COMPARE_AS(scene->field<Quaternion>(SceneField::Rotation), Containers::arrayView<Quaternion>({
            Quaternion::rotation(45.0_degf, Vector3::yAxis()),
            {},
            Quaternion::rotation(45.0_degf, Vector3::yAxis()),
            {}
        }), TestSuite::Compare::Container);
        CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
        CORRADE_COMPARE_AS(scene->field<Vector3>(SceneField::Scaling), Containers::arrayView<Vector3>({
            {0.9f, 0.5f, 2.3f},
            Vector3{1.0f},
            Vector3{1.0f},
            {0.9f, 0.5f, 2.3f},
        }), TestSuite::Compare::Container);

    /* Just translation (and the implicit transformation) */
    } {
        Containers::Optional<SceneData> scene = importer->scene("Just translation");
        CORRADE_VERIFY(scene);
        CORRADE_COMPARE(scene->fieldCount(), 3);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));

        /* The implicit transformation object is not contained in these */
        CORRADE_VERIFY(scene->hasField(SceneField::Translation));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Translation), Containers::arrayView<UnsignedInt>({
            3
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Vector3>(SceneField::Translation), Containers::arrayView<Vector3>({
            {1.5f, -2.5f, 0.3f}
        }), TestSuite::Compare::Container);

    /* Just rotation (and the implicit transformation) */
    } {
        Containers::Optional<SceneData> scene = importer->scene("Just rotation");
        CORRADE_VERIFY(scene);
        CORRADE_COMPARE(scene->fieldCount(), 3);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));

        /* The implicit transformation object is not contained in these */
        CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Rotation), Containers::arrayView<UnsignedInt>({
            4
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Quaternion>(SceneField::Rotation), Containers::arrayView<Quaternion>({
            Quaternion::rotation(45.0_degf, Vector3::yAxis())
        }), TestSuite::Compare::Container);

    /* Just scaling (and the implicit transformation) */
    } {
        Containers::Optional<SceneData> scene = importer->scene("Just scaling");
        CORRADE_VERIFY(scene);
        CORRADE_COMPARE(scene->fieldCount(), 3);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));

        /* The implicit transformation object is not contained in these */
        CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Scaling), Containers::arrayView<UnsignedInt>({
            5
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Vector3>(SceneField::Scaling), Containers::arrayView<Vector3>({
            {0.9f, 0.5f, 2.3f}
        }), TestSuite::Compare::Container);
    }
}

void TinyGltfImporterTest::sceneTransformationQuaternionNormalizationEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("normalizeQuaternions"));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-transformation-patching.gltf")));

    Containers::Optional<SceneData> scene;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        scene = importer->scene(0);
    }
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::scene(): rotation quaternion of node 3 was renormalized\n");

    Containers::Optional<Containers::Triple<Vector3, Quaternion, Vector3>> trs = scene->translationRotationScaling3DFor(3);
    CORRADE_VERIFY(trs);
    CORRADE_COMPARE(trs->second(), Quaternion::rotation(45.0_degf, Vector3::yAxis()));
}

void TinyGltfImporterTest::sceneTransformationQuaternionNormalizationDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    /* Explicity disable */
    importer->configuration().setValue("normalizeQuaternions", false);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-transformation-patching.gltf")));

    Containers::Optional<SceneData> scene;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        scene = importer->scene(0);
    }
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(out.str(), "");

    Containers::Optional<Containers::Triple<Vector3, Quaternion, Vector3>> trs = scene->translationRotationScaling3DFor(3);
    CORRADE_VERIFY(trs);
    CORRADE_COMPARE(trs->second(), Quaternion::rotation(45.0_degf, Vector3::yAxis())*2.0f);
}

void TinyGltfImporterTest::skin() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "skin"_s + data.suffix)));

    CORRADE_COMPARE(importer->skin3DCount(), 2);
    CORRADE_COMPARE(importer->skin3DName(1), "explicit inverse bind matrices");
    CORRADE_COMPARE(importer->skin3DForName("explicit inverse bind matrices"), 1);
    CORRADE_COMPARE(importer->skin3DForName("nonexistent"), -1);

    {
        Containers::Optional<Trade::SkinData3D> skin = importer->skin3D("implicit inverse bind matrices");
        CORRADE_VERIFY(skin);
        CORRADE_VERIFY(skin->importerState());
        CORRADE_COMPARE_AS(skin->joints(),
            Containers::arrayView<UnsignedInt>({1, 2}),
            TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(skin->inverseBindMatrices(),
            Containers::arrayView({Matrix4{}, Matrix4{}}),
            TestSuite::Compare::Container);
    } {
        Containers::Optional<Trade::SkinData3D> skin = importer->skin3D("explicit inverse bind matrices");
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "skin-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(Containers::arraySize(SkinInvalidData), importer->skin3DCount());

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->skin3D(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::TinyGltfImporter::skin3D(): {}\n", data.message));
}

void TinyGltfImporterTest::skinNoJointsProperty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "skin-invalid-no-joints.gltf")));
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh"_s + data.suffix)));

    CORRADE_COMPARE(importer->meshName(0), "Indexed mesh");
    CORRADE_COMPARE(importer->meshForName("Indexed mesh"), 0);
    CORRADE_COMPARE(importer->meshForName("Nonexistent"), -1);

    /* _OBJECT_ID should not be registered as a custom attribute, it gets
       reported as MeshAttribute::ObjectId instead */
    CORRADE_COMPARE(importer->meshAttributeForName("_OBJECT_ID"), MeshAttribute{});

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->importerState());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedByte);
    CORRADE_COMPARE_AS(mesh->indices<UnsignedByte>(),
        Containers::arrayView<UnsignedByte>({0, 1, 2}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 5);
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

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::TextureCoordinates));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::TextureCoordinates), VertexFormat::Vector2);
    CORRADE_COMPARE_AS(mesh->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        Containers::arrayView<Vector2>({
            /* Y-flipped compared to the input */
            {0.3f, 1.0f},
            {0.0f, 0.5f},
            {0.3f, 0.7f}
        }), TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::ObjectId));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::ObjectId), VertexFormat::UnsignedInt);
    CORRADE_COMPARE_AS(mesh->attribute<UnsignedInt>(MeshAttribute::ObjectId),
        Containers::arrayView<UnsignedInt>({
            215, 71, 133
        }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::meshNoAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh.gltf")));

    Containers::Optional<Trade::MeshData> mesh = importer->mesh("Attribute-less indexed mesh");
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

void TinyGltfImporterTest::meshNoIndices() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh.gltf")));

    Containers::Optional<Trade::MeshData> mesh = importer->mesh("Non-indexed mesh");
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->importerState());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(!mesh->isIndexed());

    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            /* Interleaved with normals (which are in a different mesh) */
            {1.5f, -1.0f, -0.5f},
            {-0.5f, 2.5f, 0.75f},
            {-2.0f, 1.0f, 0.3f}
        }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::meshNoIndicesNoAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh.gltf")));

    Containers::Optional<Trade::MeshData> mesh = importer->mesh("Attribute-less mesh");
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->importerState());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->vertexCount(), 0);
    CORRADE_COMPARE(mesh->attributeCount(), 0);
}

void TinyGltfImporterTest::meshColors() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-colors.gltf")));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(0)), "");

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
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

void TinyGltfImporterTest::meshSkinAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-skin-attributes.gltf")));

    /* The mapping should be available even before the mesh is imported */
    const MeshAttribute joints0Attribute = importer->meshAttributeForName("JOINTS_0");
    CORRADE_COMPARE(joints0Attribute, meshAttributeCustom(0));
    const MeshAttribute joints1Attribute = importer->meshAttributeForName("JOINTS_1");
    CORRADE_COMPARE(joints1Attribute, meshAttributeCustom(1));
    const MeshAttribute weights0Attribute = importer->meshAttributeForName("WEIGHTS_0");
    CORRADE_COMPARE(weights0Attribute, meshAttributeCustom(2));
    const MeshAttribute weights1Attribute = importer->meshAttributeForName("WEIGHTS_1");
    CORRADE_COMPARE(weights1Attribute, meshAttributeCustom(3));

    /* One attribute for each set, not one for all sets */
    CORRADE_COMPARE(importer->meshAttributeForName("JOINTS"), MeshAttribute{});
    CORRADE_COMPARE(importer->meshAttributeForName("WEIGHTS"), MeshAttribute{});

    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->isIndexed());

    CORRADE_COMPARE(mesh->attributeCount(), 5);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {1.5f, -1.0f, -0.5f},
            {-0.5f, 2.5f, 0.75f},
            {-2.0f, 1.0f, 0.3f}
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(joints0Attribute), 1);
    CORRADE_COMPARE(mesh->attributeFormat(joints0Attribute), VertexFormat::Vector4ub);
    CORRADE_COMPARE_AS(mesh->attribute<Vector4ub>(joints0Attribute),
        Containers::arrayView<Vector4ub>({
            {1,  2,  3,  4},
            {5,  6,  7,  8},
            {9, 10, 11, 12}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(joints1Attribute), 1);
    CORRADE_COMPARE(mesh->attributeFormat(joints1Attribute), VertexFormat::Vector4us);
    CORRADE_COMPARE_AS(mesh->attribute<Vector4us>(joints1Attribute),
        Containers::arrayView<Vector4us>({
            {13, 14, 15, 16},
            {17, 18, 19, 20},
            {21, 22, 23, 24}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(weights0Attribute), 1);
    CORRADE_COMPARE(mesh->attributeFormat(weights0Attribute), VertexFormat::Vector4);
    CORRADE_COMPARE_AS(mesh->attribute<Vector4>(weights0Attribute),
        Containers::arrayView<Vector4>({
            {0.125f, 0.25f, 0.375f, 0.0f},
            {0.1f,   0.05f, 0.05f,  0.05f},
            {0.2f,   0.0f,  0.3f,   0.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(weights1Attribute), 1);
    CORRADE_COMPARE(mesh->attributeFormat(weights1Attribute), VertexFormat::Vector4usNormalized);
    CORRADE_COMPARE_AS(mesh->attribute<Vector4us>(weights1Attribute),
        Containers::arrayView<Vector4us>({
            {       0, 0xffff/8,         0, 0xffff/8},
            {0xffff/2, 0xffff/8, 0xffff/16, 0xffff/16},
            {       0, 0xffff/4, 0xffff/4,  0}
        }), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::meshCustomAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-custom-attributes.gltf")));
    CORRADE_COMPARE(importer->meshCount(), 2);

    /* The mapping should be available even before the mesh is imported.
       Attributes are sorted per-mesh by name by JSON so the ID isn't in
       declaration order. */
    const MeshAttribute tbnAttribute = importer->meshAttributeForName("_TBN");
    CORRADE_COMPARE(tbnAttribute, meshAttributeCustom(1));
    CORRADE_COMPARE(importer->meshAttributeName(tbnAttribute), "_TBN");
    CORRADE_COMPARE(importer->meshAttributeForName("Nonexistent"), MeshAttribute{});

    const MeshAttribute uvRotation = importer->meshAttributeForName("_UV_ROTATION");
    CORRADE_COMPARE(uvRotation, meshAttributeCustom(3));
    CORRADE_COMPARE(importer->meshAttributeName(uvRotation), "_UV_ROTATION");

    const MeshAttribute tbnPreciserAttribute = importer->meshAttributeForName("_TBN_PRECISER");
    const MeshAttribute objectIdAttribute = importer->meshAttributeForName("OBJECT_ID3");

    const MeshAttribute negativePaddingAttribute = importer->meshAttributeForName("_NEGATIVE_PADDING");
    CORRADE_COMPARE(negativePaddingAttribute, meshAttributeCustom(6));

    const MeshAttribute notAnIdentityAttribute = importer->meshAttributeForName("NOT_AN_IDENTITY");
    CORRADE_COMPARE(notAnIdentityAttribute, meshAttributeCustom(4));

    const MeshAttribute doubleShotAttribute = importer->meshAttributeForName("_DOUBLE_SHOT");

    /* Core glTF attribute types */
    {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh("standard types");
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->attributeCount(), 4);

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

        CORRADE_VERIFY(mesh->hasAttribute(objectIdAttribute));
        CORRADE_COMPARE(mesh->attributeFormat(objectIdAttribute), VertexFormat::UnsignedInt);
        CORRADE_COMPARE_AS(mesh->attribute<UnsignedInt>(objectIdAttribute),
            Containers::arrayView<UnsignedInt>({5678125}),
            TestSuite::Compare::Container);

    /* Attribute types not in core glTF but allowed by tinygltf */
    } {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh("non-standard types");
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->attributeCount(), 3);

        CORRADE_VERIFY(mesh->hasAttribute(doubleShotAttribute));
        CORRADE_COMPARE(mesh->attributeFormat(doubleShotAttribute), VertexFormat::Vector2d);
        CORRADE_COMPARE_AS(mesh->attribute<Vector2d>(doubleShotAttribute),
            Containers::arrayView<Vector2d>({{31.2, 28.8}}),
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
}

void TinyGltfImporterTest::meshCustomAttributesNoFileOpened() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    /* These should return nothing (and not crash) */
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(564)), "");
    CORRADE_COMPARE(importer->meshAttributeForName("thing"), MeshAttribute{});
}

void TinyGltfImporterTest::meshDuplicateAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-duplicate-attributes.gltf")));
    CORRADE_COMPARE(importer->meshCount(), 1);

    const MeshAttribute thingAttribute = importer->meshAttributeForName("_THING");
    CORRADE_VERIFY(thingAttribute != MeshAttribute{});

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(), 3);

    /* Duplicate attributes replace previously declared attributes with the
       same name. Checking the formats should be enough to test the right
       accessor is being used. */
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Color));
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Color), 2);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Color, 0), VertexFormat::Vector4);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Color, 1), VertexFormat::Vector3);

    CORRADE_VERIFY(mesh->hasAttribute(thingAttribute));
    CORRADE_COMPARE(mesh->attributeCount(thingAttribute), 1);
    CORRADE_COMPARE(mesh->attributeFormat(thingAttribute), VertexFormat::Vector2);
}

void TinyGltfImporterTest::meshUnorderedAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-unordered-attributes.gltf")));
    CORRADE_COMPARE(importer->meshCount(), 1);

    const MeshAttribute customAttribute4 = importer->meshAttributeForName("_CUSTOM_4");
    CORRADE_VERIFY(customAttribute4 != MeshAttribute{});
    const MeshAttribute customAttribute1 = importer->meshAttributeForName("_CUSTOM_1");
    CORRADE_VERIFY(customAttribute1 != MeshAttribute{});

    /* Custom attributes are sorted in alphabetical order */
    CORRADE_VERIFY(customAttribute1 < customAttribute4);

    std::ostringstream out;
    Warning redirectWarning{&out};

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(), 7);

    /* No warning about _CUSTOM_4 and _CUSTOM_1 */
    CORRADE_COMPARE(out.str(),
        "Trade::TinyGltfImporter::mesh(): found attribute COLOR_3 but expected COLOR_0\n"
        "Trade::TinyGltfImporter::mesh(): found attribute COLOR_9 but expected COLOR_4\n"
    );

    /* Sets of the same attribute are imported in ascending set order. Checking
       the formats should be enough to test the import order. */
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::TextureCoordinates));
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::TextureCoordinates), 3);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::TextureCoordinates, 0), VertexFormat::Vector2usNormalized);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::TextureCoordinates, 1), VertexFormat::Vector2ubNormalized);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::TextureCoordinates, 2), VertexFormat::Vector2);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Color));
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Color), 2);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Color, 0), VertexFormat::Vector4);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Color, 1), VertexFormat::Vector3);

    /* Custom attributes don't have sets */
    CORRADE_VERIFY(mesh->hasAttribute(customAttribute4));
    CORRADE_COMPARE(mesh->attributeCount(customAttribute4), 1);
    CORRADE_COMPARE(mesh->attributeFormat(customAttribute4), VertexFormat::Vector2);

    CORRADE_VERIFY(mesh->hasAttribute(customAttribute1));
    CORRADE_COMPARE(mesh->attributeCount(customAttribute1), 1);
    CORRADE_COMPARE(mesh->attributeFormat(customAttribute1), VertexFormat::Vector3);
}

void TinyGltfImporterTest::meshMultiplePrimitives() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-multiple-primitives.gltf")));

    /* Four meshes, but one has three primitives and one two. Distinguishing
       using the primitive type, hopefully that's enough. */
    CORRADE_COMPARE(importer->meshCount(), 7);
    {
        CORRADE_COMPARE(importer->meshName(0), "Single-primitive points");
        CORRADE_COMPARE(importer->meshForName("Single-primitive points"), 0);
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Points);
    } {
        CORRADE_COMPARE(importer->meshName(1), "Multi-primitive lines, triangles, triangle strip");
        CORRADE_COMPARE(importer->meshName(2), "Multi-primitive lines, triangles, triangle strip");
        CORRADE_COMPARE(importer->meshName(3), "Multi-primitive lines, triangles, triangle strip");
        CORRADE_COMPARE(importer->meshForName("Multi-primitive lines, triangles, triangle strip"), 1);
        Containers::Optional<Trade::MeshData> mesh1 = importer->mesh(1);
        CORRADE_VERIFY(mesh1);
        CORRADE_COMPARE(mesh1->primitive(), MeshPrimitive::Lines);
        Containers::Optional<Trade::MeshData> mesh2 = importer->mesh(2);
        CORRADE_VERIFY(mesh2);
        CORRADE_COMPARE(mesh2->primitive(), MeshPrimitive::Triangles);
        Containers::Optional<Trade::MeshData> mesh3 = importer->mesh(3);
        CORRADE_VERIFY(mesh3);
        CORRADE_COMPARE(mesh3->primitive(), MeshPrimitive::TriangleStrip);
    } {
        CORRADE_COMPARE(importer->meshName(4), "Single-primitive line loop");
        CORRADE_COMPARE(importer->meshForName("Single-primitive line loop"), 4);
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(4);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::LineLoop);
    } {
        CORRADE_COMPARE(importer->meshName(5), "Multi-primitive triangle fan, line strip");
        CORRADE_COMPARE(importer->meshName(6), "Multi-primitive triangle fan, line strip");
        CORRADE_COMPARE(importer->meshForName("Multi-primitive triangle fan, line strip"), 5);
        Containers::Optional<Trade::MeshData> mesh5 = importer->mesh(5);
        CORRADE_VERIFY(mesh5);
        CORRADE_COMPARE(mesh5->primitive(), MeshPrimitive::TriangleFan);
        Containers::Optional<Trade::MeshData> mesh6 = importer->mesh(6);
        CORRADE_VERIFY(mesh6);
        CORRADE_COMPARE(mesh6->primitive(), MeshPrimitive::LineStrip);
    }

    /* Five objects. Two refer a three-primitive mesh and one refers a
       two-primitive one, which is done by having multiple mesh entries for
       them. */
    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_COMPARE(scene->mappingBound(), 5);
    CORRADE_COMPARE(scene->fieldCount(), 5);
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
    CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
    CORRADE_VERIFY(scene->hasField(SceneField::Transformation));
    CORRADE_VERIFY(scene->hasField(SceneField::Mesh));
    CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        0, 0, 0, 1, 3, 3, 3, 4, 4
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(scene->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        1, 2, 3, 0, 1, 2, 3, 5, 6
    }), TestSuite::Compare::Container);
    CORRADE_VERIFY(scene->hasField(SceneField::MeshMaterial));
    CORRADE_COMPARE_AS(scene->field<Int>(SceneField::MeshMaterial), Containers::arrayView<Int>({
        1, 2, 0, 3, 1, 2, 0, -1, 1
    }), TestSuite::Compare::Container);
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

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-primitives-types.gltf")));

    /* Ensure we didn't forget to test any case */
    CORRADE_COMPARE(importer->meshCount(), Containers::arraySize(MeshPrimitivesTypesData));

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(data.name);
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
            Containers::Array<Vector3> positions = mesh->positions3DAsArray();
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
        Containers::Array<Vector3> normals = mesh->normalsAsArray();
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
        Containers::Array<Vector3> tangents = mesh->tangentsAsArray();
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
            Containers::Array<Vector2> textureCoordinates = mesh->textureCoordinates2DAsArray();
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
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "mesh-invalid-index-accessor-oob.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::TinyGltfImporter::openData(): error opening file: primitive indices accessor out of bounds\n");
}

void TinyGltfImporterTest::meshInvalid() {
    auto&& data = MeshInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, data.file)));

    /* Check we didn't forget to test anything */
    CORRADE_VERIFY(Containers::arraySize(MeshInvalidData) >= importer->meshCount());

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

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-metallicroughness.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 7);
    CORRADE_COMPARE(importer->materialName(2), "textures");
    CORRADE_COMPARE(importer->materialForName("textures"), 2);
    CORRADE_COMPARE(importer->materialForName("Nonexistent"), -1);

    {
        const char* name = "defaults";
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-specularglossiness.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 7);

    {
        const char* name = "defaults";
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-common.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 7);

    {
        Containers::Optional<Trade::MaterialData> material = importer->material("defaults");
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
        Containers::Optional<Trade::MaterialData> material = importer->material("alpha mask");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 1);
        CORRADE_COMPARE(material->alphaMode(), MaterialAlphaMode::Mask);
        CORRADE_COMPARE(material->alphaMask(), 0.369f);
    } {
        Containers::Optional<Trade::MaterialData> material = importer->material("double-sided alpha blend");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 2);
        CORRADE_VERIFY(material->isDoubleSided());
        CORRADE_COMPARE(material->alphaMode(), MaterialAlphaMode::Blend);
        CORRADE_COMPARE(material->alphaMask(), 0.5f);
    } {
        Containers::Optional<Trade::MaterialData> material = importer->material("opaque");
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 0);
        CORRADE_COMPARE(material->alphaMode(), MaterialAlphaMode::Opaque);
        CORRADE_COMPARE(material->alphaMask(), 0.5f);
    } {
        const char* name = "normal, occlusion, emissive texture";
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-unlit.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<Trade::MaterialData> material = importer->material(0);
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

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-clearcoat.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 6);

    {
        const char* name = "defaults";
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
    CORRADE_VERIFY(importer->configuration().value<bool>("phongMaterialFallback"));

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-phong-fallback.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 4);

    {
        const char* name = "none";
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "material-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(Containers::arraySize(MaterialInvalidData), importer->materialCount());

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

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, data.fileName)));

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(data.meshName);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::TextureCoordinates));
    Containers::Array<Vector2> texCoords = mesh->textureCoordinates2DAsArray();

    /* Texture transform is added to materials that don't have it yet */
    Containers::Optional<Trade::MaterialData> material = importer->material(data.name);
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
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "texture.gltf")));

    CORRADE_COMPARE(importer->textureCount(), 4);
    CORRADE_COMPARE(importer->textureName(1), "another variant");
    CORRADE_COMPARE(importer->textureForName("another variant"), 1);
    CORRADE_COMPARE(importer->textureForName("nonexistent"), -1);

    {
        Containers::Optional<Trade::TextureData> texture = importer->texture(0);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 1);
        CORRADE_COMPARE(texture->type(), TextureType::Texture2D);

        CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Nearest);
        CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Nearest);

        CORRADE_COMPARE(texture->wrapping(), Math::Vector3<SamplerWrapping>(SamplerWrapping::MirroredRepeat, SamplerWrapping::ClampToEdge, SamplerWrapping::Repeat));
    } {
        Containers::Optional<Trade::TextureData> texture = importer->texture("another variant");
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 0);
        CORRADE_COMPARE(texture->type(), TextureType::Texture2D);

        CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Nearest);
        CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Linear);

        CORRADE_COMPARE(texture->wrapping(), Math::Vector3<SamplerWrapping>(SamplerWrapping::Repeat, SamplerWrapping::ClampToEdge, SamplerWrapping::Repeat));
    }

    /* Both should give the same result */
    for(const char* name: {"empty sampler", "default sampler"}) {
        CORRADE_ITERATION(name);

        Containers::Optional<Trade::TextureData> texture = importer->texture(name);
        CORRADE_VERIFY(texture);

        CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Linear);

        CORRADE_COMPARE(texture->wrapping(), Math::Vector3<SamplerWrapping>{SamplerWrapping::Repeat});
    }
}

void TinyGltfImporterTest::textureExtensions() {
    auto&& data = TextureExtensionsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "texture-extensions.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->textureCount(), Containers::arraySize(TextureExtensionsData));

    Containers::Optional<Trade::TextureData> texture = importer->texture(data.name);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), data.id);
}

void TinyGltfImporterTest::textureInvalid() {
    auto&& data = TextureInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "texture-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(Containers::arraySize(TextureInvalidData), importer->textureCount());

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->texture(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::TinyGltfImporter::texture(): {}\n", data.message));
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
    Containers::Optional<Containers::Array<char>> file = Utility::Path::read(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "image"_s + data.suffix));
    CORRADE_VERIFY(file);
    CORRADE_VERIFY(importer->openData(*file));

    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_COMPARE(importer->image2DName(1), "Image");
    CORRADE_COMPARE(importer->image2DForName("Image"), 1);
    CORRADE_COMPARE(importer->image2DForName("Nonexistent"), -1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(1);
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "image"_s + data.suffix)));

    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_COMPARE(importer->image2DName(1), "Image");
    CORRADE_COMPARE(importer->image2DForName("Image"), 1);
    CORRADE_COMPARE(importer->image2DForName("Nonexistent"), -1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->importerState());
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(ExpectedImageData).prefix(60), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::imageExternalNotFound() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(TINYGLTFIMPORTER_TEST_DIR, "image-invalid-notfound.gltf")));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    /* There's an error from Path:.read() first */
    CORRADE_COMPARE_AS(out.str(),
        "\nTrade::AbstractImporter::openFile(): cannot open file /nonexistent.png\n",
        TestSuite::Compare::StringHasSuffix);
}

void TinyGltfImporterTest::imageExternalNoPathNoCallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    Containers::Optional<Containers::Array<char>> data = Utility::Path::read(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "image.gltf"));
    CORRADE_VERIFY(data);
    CORRADE_VERIFY(importer->openData(*data));
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "image-basis"_s + data.suffix)));

    CORRADE_COMPARE(importer->textureCount(), 1);
    CORRADE_COMPARE(importer->image2DCount(), 2);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->importerState());
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc4x4RGBAUnorm);

    /* The texture refers to the image indirectly via an extension, test the
       mapping */
    Containers::Optional<Trade::TextureData> texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), 1);
}

void TinyGltfImporterTest::imageMipLevels() {
    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    /* Import as RGBA so we can verify the pixels */
    _manager.metadata("BasisImporter")->configuration().setValue("format", "RGBA8");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "image-basis.gltf")));
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
    Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
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
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "přívodní-šňůra.gltf")));

    CORRADE_COMPARE(importer->meshCount(), 1);
    Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Points);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_COMPARE_AS(mesh->positions3DAsArray(0), Containers::arrayView<Vector3>({
        {1.0f, 2.0f, 3.0f}
    }), TestSuite::Compare::Container);

    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(ExpectedImageData).prefix(60), TestSuite::Compare::Container);
}

void TinyGltfImporterTest::escapedStrings() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "escaped-strings.gltf")));

    CORRADE_COMPARE(importer->objectCount(), 6);
    CORRADE_COMPARE(importer->objectName(0), "");
    CORRADE_COMPARE(importer->objectName(1), "UTF-8: Лорем ипсум долор сит амет");
    CORRADE_COMPARE(importer->objectName(2), "UTF-8 escaped: Лорем ипсум долор сит амет");
    CORRADE_COMPARE(importer->objectName(3), "Special: \"/\\\b\f\r\n\t");
    CORRADE_COMPARE(importer->objectName(4), "Everything: říční člun \t\t\n حليب اللوز");
    /* Tinygltf decodes JSON keys (in this case, "name"). Old versions of the
       the spec used to forbid non-ASCII keys or enums:
       https://github.com/KhronosGroup/glTF/tree/fd3ab461a1114fb0250bd76099153d2af50a7a1d/specification/2.0#json-encoding
       Newer spec versions changed this to "ASCII characters [...] SHOULD be
       written without JSON escaping" */
    CORRADE_COMPARE(importer->objectName(5), "Key UTF-8 escaped");

    /* Test inverse mapping as well -- it should decode the name before
       comparison. */
    CORRADE_COMPARE(importer->objectForName("Everything: říční člun \t\t\n حليب اللوز"), 4);

    /* All user-facing strings are unescaped. URIs are tested in encodedUris(). */
    CORRADE_COMPARE(importer->animationCount(), 1);
    CORRADE_COMPARE(importer->animationName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->animationForName("Everything: říční člun \t\t\n حليب اللوز"), 0);

    CORRADE_COMPARE(importer->cameraCount(), 1);
    CORRADE_COMPARE(importer->cameraName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->cameraForName("Everything: říční člun \t\t\n حليب اللوز"), 0);

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->image2DForName("Everything: říční člun \t\t\n حليب اللوز"), 0);

    CORRADE_COMPARE(importer->lightCount(), 1);
    CORRADE_COMPARE(importer->lightName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->lightForName("Everything: říční člun \t\t\n حليب اللوز"), 0);

    CORRADE_COMPARE(importer->materialCount(), 1);
    CORRADE_COMPARE(importer->materialName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->materialForName("Everything: říční člun \t\t\n حليب اللوز"), 0);

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->meshName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->meshForName("Everything: říční člun \t\t\n حليب اللوز"), 0);

    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->sceneName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->sceneForName("Everything: říční člun \t\t\n حليب اللوز"), 0);

    CORRADE_COMPARE(importer->skin3DCount(), 1);
    CORRADE_COMPARE(importer->skin3DName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->skin3DForName("Everything: říční člun \t\t\n حليب اللوز"), 0);

    CORRADE_COMPARE(importer->textureCount(), 1);
    CORRADE_COMPARE(importer->textureName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->textureForName("Everything: říční člun \t\t\n حليب اللوز"), 0);
}

void TinyGltfImporterTest::encodedUris() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    std::string strings[6];

    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy, std::string (&strings)[6])
            -> Containers::Optional<Containers::ArrayView<const char>>
        {
            static const char bytes[4]{};
            if(filename.find("buffer-unencoded") == 0)
                strings[0] = filename;
            else if(filename.find("buffer-encoded") == 0)
                strings[1] = filename;
            else if(filename.find("buffer-escaped") == 0)
                strings[2] = filename;
            else if(filename.find("image-unencoded") == 0)
                strings[3] = filename;
            else if(filename.find("image-encoded") == 0)
                strings[4] = filename;
            else if(filename.find("image-escaped") == 0)
                strings[5] = filename;
            return Containers::arrayView(bytes);
        }, strings);

    /* Prevent the file callback being used for the main glTF content */
    Containers::Optional<Containers::Array<char>> data = Utility::Path::read(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "encoded-uris.gltf"));
    CORRADE_VERIFY(data);
    CORRADE_VERIFY(importer->openData(*data));

    CORRADE_COMPARE(importer->image2DCount(), 3);
    /* We don't care about the result, only the callback being invoked */
    importer->image2D(0);
    importer->image2D(1);
    importer->image2D(2);

    CORRADE_COMPARE(strings[0], "buffer-unencoded/@file#.bin");
    CORRADE_COMPARE(strings[3], "image-unencoded/image #1.png");

    {
        CORRADE_EXPECT_FAIL("tinygltf doesn't decode special characters in URIs.");

        CORRADE_COMPARE(strings[1], "buffer-encoded/@file#.bin");
        CORRADE_COMPARE(strings[2], "buffer-escaped/říční člun.bin");
        CORRADE_COMPARE(strings[4], "image-encoded/image #1.png");
        CORRADE_COMPARE(strings[5], "image-escaped/říční člun.png");
    }

    CORRADE_COMPARE(strings[1], "buffer-encoded%2F%40file%23.bin");
    CORRADE_COMPARE(strings[2], "buffer-escaped%2Fříční%20člun.bin");
    CORRADE_COMPARE(strings[4], "image-encoded%2Fimage%20%231.png");
    CORRADE_COMPARE(strings[5], "image-escaped%2Fříční%20člun.png");
}

void TinyGltfImporterTest::versionSupported() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "version-supported.gltf")));
}

void TinyGltfImporterTest::versionUnsupported() {
    auto&& data = UnsupportedVersionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, data.file)));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::openData(): {}\n", data.message));
}

void TinyGltfImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "camera.gltf")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "camera.gltf")));

    /* Shouldn't crash, leak or anything */
}

void TinyGltfImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("TinyGltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "camera.gltf")));
    CORRADE_COMPARE(importer->cameraCount(), 4);

    /* Verify that everything is working the same way on second use. It's only
       testing a single data type, but better than nothing at all. */
    {
        Containers::Optional<Trade::CameraData> cam = importer->camera(0);
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->type(), CameraType::Orthographic3D);
        CORRADE_COMPARE(cam->size(), (Vector2{4.0f, 3.0f}));
        CORRADE_COMPARE(cam->aspectRatio(), 1.333333f);
        CORRADE_COMPARE(cam->near(), 0.01f);
        CORRADE_COMPARE(cam->far(), 100.0f);
    } {
        Containers::Optional<Trade::CameraData> cam = importer->camera(0);
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->type(), CameraType::Orthographic3D);
        CORRADE_COMPARE(cam->size(), (Vector2{4.0f, 3.0f}));
        CORRADE_COMPARE(cam->aspectRatio(), 1.333333f);
        CORRADE_COMPARE(cam->near(), 0.01f);
        CORRADE_COMPARE(cam->far(), 100.0f);
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::TinyGltfImporterTest)
