/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/FormatStl.h>
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

struct CgltfImporterTest: TestSuite::Tester {
    explicit CgltfImporterTest();

    void open();
    void openError();
    void openExternalDataOrder();
    void openExternalDataNotFound();
    void openExternalDataNoPathNoCallback();
    void openExternalDataTooLong();
    void openExternalDataTooShort();
    void openExternalDataNoUri();
    void openExternalDataInvalidUri();

    void requiredExtensions();
    void requiredExtensionsUnsupported();
    void requiredExtensionsUnsupportedDisabled();

    void animation();
    void animationOutOfBounds();
    void animationInvalid();
    void animationInvalidBufferNotFound();
    void animationInvalidInterpolation();
    void animationInvalidTypes();
    void animationMismatchingCount();
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
    void lightInvalidColorSize();
    void lightMissingType();
    void lightMissingSpot();

    void scene();
    void sceneEmpty();
    void sceneNoDefault();
    void sceneOutOfBounds();
    void sceneInvalid();
    void sceneCycle();

    void objectTransformation();
    void objectTransformationQuaternionNormalizationEnabled();
    void objectTransformationQuaternionNormalizationDisabled();

    void skin();
    void skinOutOfBounds();
    void skinInvalid();
    void skinInvalidBufferNotFound();
    void skinInvalidTypes();
    void skinNoJointsProperty();

    void mesh();
    void meshAttributeless();
    void meshIndexed();
    void meshIndexedAttributeless();
    void meshColors();
    void meshSkinAttributes();
    void meshCustomAttributes();
    void meshCustomAttributesNoFileOpened();
    void meshDuplicateAttributes();
    void meshUnorderedAttributes();
    void meshMultiplePrimitives();
    void meshPrimitivesTypes();
    void meshOutOfBounds();
    void meshInvalid();
    void meshInvalidIndicesBufferNotFound();
    void meshInvalidTypes();

    void materialPbrMetallicRoughness();
    void materialPbrSpecularGlossiness();
    void materialCommon();
    void materialUnlit();
    void materialClearCoat();
    void materialPhongFallback();

    void materialOutOfBounds();
    void materialInvalidAlphaMode();
    void materialTexCoordFlip();

    void texture();
    void textureOutOfBounds();
    void textureInvalid();
    void textureDefaultSampler();
    void textureEmptySampler();
    void textureMissingSource();
    void textureExtensions();
    void textureExtensionsOutOfBounds();
    void textureExtensionsInvalid();

    void imageEmbedded();
    void imageExternal();
    void imageExternalNotFound();
    void imageExternalBufferNotFound();
    void imageExternalNoPathNoCallback();
    void imageNoData();

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

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

/* The external-data.* files are packed in via a resource, filename mapping
   done in resources.conf */

using namespace Containers::Literals;
using namespace Magnum::Math::Literals;


constexpr struct {
    const char* name;
    const Containers::StringView data;
    const char* message;
} OpenErrorData[]{
    {"short ascii", "?"_s, "data too short"},
    {"short binary", "glTF?"_s, "data too short"},
    {"short binary chunk", "glTF\x02\x00\x00\x00\x66\x00\x00\x00"_s, "data too short"},
    {"unknown binary version", "glTF\x10\x00\x00\x00\x0c\x00\x00\x00"_s, "unknown binary glTF format"},
    {"unknown binary JSON version", "glTF\x02\x00\x00\x00\x16\x00\x00\x00\x02\x00\x00\x00JSUN{}"_s, "unknown binary glTF format"},
    {"unknown binary GLB version", "glTF\x02\x00\x00\x00\x22\x00\x00\x00\x02\x00\x00\x00JSON{}\x04\x00\x00\0BIB\x00\xff\xff\xff\xff"_s, "unknown binary glTF format"},
    {"invalid JSON ascii", "{\"asset\":{\"version\":\"2.0\"}"_s, "invalid JSON"},
    {"invalid JSON binary", "glTF\x02\x00\x00\x00\x16\x00\x00\x00\x02\x00\x00\x00JSON{{"_s, "invalid JSON"}
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
} InvalidUriData[]{
    {"no payload", "data URI has no base64 payload"},
    {"no base64", "data URI has no base64 payload"},
    {"empty base64", "data URI has no base64 payload"},
    {"invalid base64", "invalid base64 string in data URI"}
};

constexpr struct {
    const char* name;
    const char* file;
} AnimationOutOfBoundsData[]{
    {"sampler index out of bounds", "animation-invalid-sampler-oob.gltf"},
    {"node index out of bounds", "animation-invalid-node-oob.gltf"},
    {"sampler input accessor index out of bounds", "animation-invalid-input-accessor-oob.gltf"},
    {"sampler output accessor index out of bounds", "animation-invalid-output-accessor-oob.gltf"}
};

constexpr struct {
    const char* name;
    const char* message;
} AnimationInvalidData[]{
    {"unexpected time type", "time track has unexpected type VEC4 / FLOAT (5126)"},
    {"unexpected translation type", "translation track has unexpected type VEC4 / FLOAT (5126)"},
    {"unexpected rotation type", "rotation track has unexpected type SCALAR / FLOAT (5126)"},
    {"unexpected scaling type", "scaling track has unexpected type VEC4 / FLOAT (5126)"},
    {"unsupported path", "unsupported track target 0"},
    {"invalid input accessor", "accessor 3 needs 40 bytes but buffer view 0 has only 0"},
    {"invalid output accessor", "accessor 4 needs 120 bytes but buffer view 0 has only 0"}
};

constexpr struct {
    const char* name;
    const char* message;
} AnimationInvalidTypesData[]{
    {"unknown type", "rotation track has unexpected type UNKNOWN / UNSIGNED_BYTE (5121)"},
    {"unknown component type", "time track has unexpected type MAT2 / UNKNOWN"},
    {"normalized float", "scaling track has unexpected type normalized VEC3 / FLOAT (5126)"}
};

constexpr struct {
    const char* name;
    const char* message;
} AnimationInvalidBufferNotFoundData[]{
    {"input buffer not found", "error opening file: /nonexistent1.bin : file not found"},
    {"output buffer not found", "error opening file: /nonexistent2.bin : file not found"}
};

constexpr struct {
    const char* name;
    const char* message;
} LightInvalidData[]{
    {"unknown type", "invalid light type"},
    {"directional with range", "range can't be defined for a directional light"},
    {"spot with too small inner angle", "inner and outer cone angle Deg(-0.572958) and Deg(45) out of allowed bounds"},
    /* These are kinda silly (not sure why we should limit to 90° and why inner
       can't be the same as outer), but let's follow the spec */
    {"spot with too large outer angle", "inner and outer cone angle Deg(0) and Deg(90.5273) out of allowed bounds"},
    {"spot with inner angle same as outer", "inner and outer cone angle Deg(14.3239) and Deg(14.3239) out of allowed bounds"}
};

constexpr struct {
    const char* name;
    const char* file;
} SkinOutOfBoundsData[]{
    {"joint out of bounds", "skin-invalid-joint-oob.gltf"},
    {"accessor out of bounds", "skin-invalid-accessor-oob.gltf"}
};

constexpr struct {
    const char* name;
    const char* message;
} SkinInvalidData[]{
    {"no joints", "skin has no joints"},
    {"wrong accessor type", "inverse bind matrices have unexpected type MAT3 / FLOAT (5126)"},
    {"wrong accessor component type", "inverse bind matrices have unexpected type MAT4 / UNSIGNED_SHORT (5123)"},
    {"wrong accessor count", "invalid inverse bind matrix count, expected 2 but got 3"},
    {"invalid accessor", "accessor 3 needs 196 bytes but buffer view 0 has only 192"}
};

constexpr struct {
    const char* name;
    const char* message;
} SkinInvalidTypesData[]{
    {"unknown type", "inverse bind matrices have unexpected type UNKNOWN / FLOAT (5126)"},
    {"unknown component type", "inverse bind matrices have unexpected type MAT4 / UNKNOWN"},
    {"normalized float", "inverse bind matrices have unexpected type normalized MAT4 / FLOAT (5126)"}
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
    const char* file;
} MeshOutOfBoundsData[]{
    {"buffer index out of bounds", "mesh-invalid-buffer-oob.gltf"},
    {"buffer view index out of bounds", "mesh-invalid-bufferview-oob.gltf"},
    {"accessor index out of bounds", "mesh-invalid-accessor-oob.gltf"},
    {"mesh index accessor out of bounds", "mesh-index-accessor-oob.gltf"}
};

constexpr struct {
    const char* name;
    const char* message;
} MeshInvalidData[]{
    {"invalid primitive", "unrecognized primitive 666"},
    {"different vertex count for each accessor", "mismatched vertex count for attribute TEXCOORD, expected 3 but got 4"},
    {"unexpected position type", "unexpected POSITION type VEC2"},
    {"unsupported position component type", "unsupported POSITION component type unnormalized UNSIGNED_INT (5125)"},
    {"unexpected normal type", "unexpected NORMAL type VEC2"},
    {"unsupported normal component type", "unsupported NORMAL component type unnormalized UNSIGNED_INT (5125)"},
    {"unexpected tangent type", "unexpected TANGENT type VEC3"},
    {"unsupported tangent component type", "unsupported TANGENT component type unnormalized BYTE (5120)"},
    {"unexpected texcoord type", "unexpected TEXCOORD type VEC3"},
    {"unsupported texcoord component type", "unsupported TEXCOORD component type unnormalized UNSIGNED_INT (5125)"},
    {"unexpected color type", "unexpected COLOR type VEC2"},
    {"unsupported color component type", "unsupported COLOR component type unnormalized BYTE (5120)"},
    {"unexpected joints type", "unexpected JOINTS type VEC3"},
    {"unsupported joints component type", "unsupported JOINTS component type unnormalized BYTE (5120)"},
    {"unexpected weights type", "unexpected WEIGHTS type SCALAR"},
    {"unsupported weights component type", "unsupported WEIGHTS component type unnormalized BYTE (5120)"},
    {"unexpected object id type", "unexpected object ID type VEC2"},
    {"unsupported object id component type", "unsupported object ID component type unnormalized SHORT (5122)"},
    {"unexpected index type", "unexpected index type VEC2"},
    {"unsupported index component type", "unexpected index component type SHORT (5122)"},
    {"normalized index type", "index type can't be normalized"},
    {"strided index view", "index buffer view is not contiguous"},
    {"accessor type size larger than buffer stride", "16-byte type defined by accessor 10 can't fit into buffer view 0 stride of 12"},
    {"normalized float", "attribute _THING component type FLOAT (5126) can't be normalized"},
    {"normalized int", "attribute _THING component type UNSIGNED_INT (5125) can't be normalized"},
    {"non-normalized byte matrix", "attribute _THING has an unsupported matrix component type unnormalized BYTE (5120)"},
    {"sparse accessor", "accessor 14 is using sparse storage, which is unsupported"},
    {"no bufferview", "accessor 15 has no buffer view"},
    {"accessor range out of bounds", "accessor 18 needs 48 bytes but buffer view 0 has only 36"},
    {"buffer view range out of bounds", "buffer view 3 needs 164 bytes but buffer 1 has only 160"},
    {"multiple buffers", "meshes spanning multiple buffers are not supported"},
    {"invalid index accessor", "accessor 17 needs 40 bytes but buffer view 0 has only 36"}
};

constexpr struct {
    const char* name;
    const char* message;
} MeshInvalidTypesData[]{
    {"unknown type", "attribute _THING has an invalid type"},
    {"unknown component type", "attribute _THING has an invalid component type"}
};

constexpr struct {
    const char* name;
    const char* file;
} MaterialOutOfBoundsData[]{
    {"invalid texture index pbrMetallicRoughness base color", "material-invalid-pbr-base-color-oob.gltf"},
    {"invalid texture index pbrMetallicRoughness metallic/roughness", "material-invalid-pbr-metallic-roughness-oob.gltf"},
    {"invalid texture index pbrSpecularGlossiness diffuse", "material-invalid-pbr-diffuse-oob.gltf"},
    {"invalid texture index pbrSpecularGlossiness specular", "material-invalid-pbr-specular-oob.gltf"},
    {"invalid texture index normal", "material-invalid-normal-oob.gltf"},
    {"invalid texture index occlusion", "material-invalid-occlusion-oob.gltf"},
    {"invalid texture index emissive", "material-invalid-emissive-oob.gltf"},
    {"invalid texture index clearcoat factor", "material-invalid-clearcoat-factor-oob.gltf"},
    {"invalid texture index clearcoat roughness", "material-invalid-clearcoat-roughness-oob.gltf"},
    {"invalid texture index clearcoat normal", "material-invalid-clearcoat-normal-oob.gltf"}
};

constexpr struct {
    const char* name;
    const char* file;
} SceneOutOfBoundsData[]{
    {"camera out of bounds", "scene-invalid-camera-oob.gltf"},
    {"child out of bounds", "scene-invalid-child-oob.gltf"},
    {"material out of bounds", "scene-invalid-material-oob.gltf"},
    {"material in a multi-primitive mesh out of bounds", "scene-invalid-material-oob-multi-primitive.gltf"},
    {"skin out of bounds", "scene-invalid-skin-oob.gltf"},
    {"skin for a multi-primitive mesh out of bounds", "scene-invalid-skin-oob-multi-primitive.gltf"},
    {"light out of bounds", "scene-invalid-light-oob.gltf"},
    {"default scene out of bounds", "scene-invalid-default-oob.gltf"},
    {"node out of bounds", "scene-invalid-node-oob.gltf"}
};

constexpr struct {
    const char* name;
    const char* file;
} SceneInvalidData[]{
    {"scene node has parent", "scene-invalid-child-not-root.gltf"},
    {"node has multiple parents", "scene-invalid-multiple-parents.gltf"}
};

constexpr struct {
    const char* name;
    const char* file;
} SceneCycleData[]{
    {"child is self", "scene-cycle.gltf"},
    {"great-grandchild is self", "scene-cycle-deep.gltf"}
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
    const char* file;
} TextureOutOfBoundsData[]{
    {"image out of bounds", "texture-invalid-image-oob.gltf"},
    {"sampler out of bounds", "texture-invalid-sampler-oob.gltf"}
};

constexpr struct {
    const char* name;
    const char* message;
} TextureInvalidData[]{
    {"invalid sampler minFilter", "invalid minFilter 1"},
    {"invalid sampler magFilter", "invalid magFilter 2"},
    {"invalid sampler wrapS", "invalid wrap mode 3"},
    {"invalid sampler wrapT", "invalid wrap mode 4"}
};

constexpr struct {
    const char* name;
    const UnsignedInt id;
} TextureExtensionsData[]{
    {"GOOGLE_texture_basis", 1},
    {"KHR_texture_basisu", 2},
    {"MSFT_texture_dds", 3},
    /* declaration order decides preference */
    {"MSFT_texture_dds and GOOGLE_texture_basis", 3},
    /* KHR_texture_basisu has preference before all other extensions */
    {"GOOGLE_texture_basis and KHR_texture_basisu", 2},
    {"unknown extension", 0},
    {"GOOGLE_texture_basis and unknown", 1}
};

constexpr struct {
    const char* name;
    const char* message;
} TextureExtensionsInvalidData[]{
    {"out of bounds GOOGLE_texture_basis", "GOOGLE_texture_basis image 3 out of bounds for 3 images"},
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
    {"legacy major version", "version-legacy.gltf", "error opening file: legacy glTF version"},
    {"unknown major version", "version-unsupported.gltf", "unsupported version 3.0, expected 2.x"},
    {"unknown minor version", "version-unsupported-min.gltf", "unsupported minVersion 2.1, expected 2.0"}
};

CgltfImporterTest::CgltfImporterTest() {
    addInstancedTests({&CgltfImporterTest::open},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&CgltfImporterTest::openError},
                      Containers::arraySize(OpenErrorData));

    addInstancedTests({&CgltfImporterTest::openExternalDataOrder,
                       &CgltfImporterTest::openExternalDataNotFound,
                       &CgltfImporterTest::openExternalDataNoPathNoCallback,
                       &CgltfImporterTest::openExternalDataTooLong},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&CgltfImporterTest::openExternalDataTooShort},
                      Containers::arraySize(MultiFileData));

    addInstancedTests({&CgltfImporterTest::openExternalDataNoUri},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&CgltfImporterTest::openExternalDataInvalidUri},
                      Containers::arraySize(InvalidUriData));

    addTests({&CgltfImporterTest::requiredExtensions,
              &CgltfImporterTest::requiredExtensionsUnsupported,
              &CgltfImporterTest::requiredExtensionsUnsupportedDisabled});

    addInstancedTests({&CgltfImporterTest::animation},
                      Containers::arraySize(MultiFileData));

    addInstancedTests({&CgltfImporterTest::animationOutOfBounds},
        Containers::arraySize(AnimationOutOfBoundsData));

    addInstancedTests({&CgltfImporterTest::animationInvalid},
        Containers::arraySize(AnimationInvalidData));

    addInstancedTests({&CgltfImporterTest::animationInvalidBufferNotFound},
        Containers::arraySize(AnimationInvalidBufferNotFoundData));

    addTests({&CgltfImporterTest::animationInvalidInterpolation});

    addInstancedTests({&CgltfImporterTest::animationInvalidTypes},
        Containers::arraySize(AnimationInvalidTypesData));

    addTests({&CgltfImporterTest::animationMismatchingCount,
              &CgltfImporterTest::animationMissingTargetNode});

    addInstancedTests({&CgltfImporterTest::animationSpline},
                      Containers::arraySize(MultiFileData));

    addTests({&CgltfImporterTest::animationSplineSharedWithSameTimeTrack,
              &CgltfImporterTest::animationSplineSharedWithDifferentTimeTrack,

              &CgltfImporterTest::animationShortestPathOptimizationEnabled,
              &CgltfImporterTest::animationShortestPathOptimizationDisabled,
              &CgltfImporterTest::animationQuaternionNormalizationEnabled,
              &CgltfImporterTest::animationQuaternionNormalizationDisabled,
              &CgltfImporterTest::animationMergeEmpty,
              &CgltfImporterTest::animationMerge});

    addInstancedTests({&CgltfImporterTest::camera},
                      Containers::arraySize(SingleFileData));

    addTests({&CgltfImporterTest::cameraInvalidType});

    addInstancedTests({&CgltfImporterTest::light},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&CgltfImporterTest::lightInvalid},
        Containers::arraySize(LightInvalidData));

    addTests({&CgltfImporterTest::lightInvalidColorSize,
              &CgltfImporterTest::lightMissingType,
              &CgltfImporterTest::lightMissingSpot});

    addInstancedTests({&CgltfImporterTest::scene,
                       &CgltfImporterTest::sceneEmpty,
                       &CgltfImporterTest::sceneNoDefault},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&CgltfImporterTest::sceneOutOfBounds},
        Containers::arraySize(SceneOutOfBoundsData));

    addInstancedTests({&CgltfImporterTest::sceneInvalid},
        Containers::arraySize(SceneInvalidData));

    addInstancedTests({&CgltfImporterTest::sceneCycle},
        Containers::arraySize(SceneCycleData));

    addInstancedTests({&CgltfImporterTest::objectTransformation},
                      Containers::arraySize(SingleFileData));

    addTests({&CgltfImporterTest::objectTransformationQuaternionNormalizationEnabled,
              &CgltfImporterTest::objectTransformationQuaternionNormalizationDisabled});

    addInstancedTests({&CgltfImporterTest::skin},
        Containers::arraySize(MultiFileData));

    addInstancedTests({&CgltfImporterTest::skinInvalid},
        Containers::arraySize(SkinInvalidData));

    addTests({&CgltfImporterTest::skinInvalidBufferNotFound});

    addInstancedTests({&CgltfImporterTest::skinInvalidTypes},
        Containers::arraySize(SkinInvalidTypesData));

    addInstancedTests({&CgltfImporterTest::skinOutOfBounds},
        Containers::arraySize(SkinOutOfBoundsData));

    addTests({&CgltfImporterTest::skinNoJointsProperty});

    addInstancedTests({&CgltfImporterTest::mesh},
                      Containers::arraySize(MultiFileData));

    addTests({&CgltfImporterTest::meshAttributeless,
              &CgltfImporterTest::meshIndexed,
              &CgltfImporterTest::meshIndexedAttributeless,
              &CgltfImporterTest::meshColors,
              &CgltfImporterTest::meshSkinAttributes,
              &CgltfImporterTest::meshCustomAttributes,
              &CgltfImporterTest::meshCustomAttributesNoFileOpened,
              &CgltfImporterTest::meshDuplicateAttributes,
              &CgltfImporterTest::meshUnorderedAttributes,
              &CgltfImporterTest::meshMultiplePrimitives});

    addInstancedTests({&CgltfImporterTest::meshPrimitivesTypes},
        Containers::arraySize(MeshPrimitivesTypesData));

    addInstancedTests({&CgltfImporterTest::meshOutOfBounds},
        Containers::arraySize(MeshOutOfBoundsData));

    addInstancedTests({&CgltfImporterTest::meshInvalid},
        Containers::arraySize(MeshInvalidData));

    addTests({&CgltfImporterTest::meshInvalidIndicesBufferNotFound});

    addInstancedTests({&CgltfImporterTest::meshInvalidTypes},
        Containers::arraySize(MeshInvalidTypesData));

    addTests({&CgltfImporterTest::materialPbrMetallicRoughness,
              &CgltfImporterTest::materialPbrSpecularGlossiness,
              &CgltfImporterTest::materialCommon,
              &CgltfImporterTest::materialUnlit,
              &CgltfImporterTest::materialClearCoat,
              &CgltfImporterTest::materialPhongFallback});

    addInstancedTests({&CgltfImporterTest::materialOutOfBounds},
        Containers::arraySize(MaterialOutOfBoundsData));

    addTests({&CgltfImporterTest::materialInvalidAlphaMode});

    addInstancedTests({&CgltfImporterTest::materialTexCoordFlip},
        Containers::arraySize(MaterialTexCoordFlipData));

    addInstancedTests({&CgltfImporterTest::texture},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&CgltfImporterTest::textureInvalid},
                      Containers::arraySize(TextureInvalidData));

    addInstancedTests({&CgltfImporterTest::textureDefaultSampler,
                       &CgltfImporterTest::textureEmptySampler},
                      Containers::arraySize(SingleFileData));

    addTests({&CgltfImporterTest::textureMissingSource});

    addInstancedTests({&CgltfImporterTest::textureExtensions},
                      Containers::arraySize(TextureExtensionsData));

    addTests({&CgltfImporterTest::textureExtensionsOutOfBounds});

    addInstancedTests({&CgltfImporterTest::textureExtensionsInvalid},
                      Containers::arraySize(TextureExtensionsInvalidData));

    addInstancedTests({&CgltfImporterTest::imageEmbedded},
                      Containers::arraySize(ImageEmbeddedData));

    addInstancedTests({&CgltfImporterTest::imageExternal},
                      Containers::arraySize(ImageExternalData));

    addTests({&CgltfImporterTest::imageExternalNotFound,
              &CgltfImporterTest::imageExternalBufferNotFound,
              &CgltfImporterTest::imageExternalNoPathNoCallback,
              &CgltfImporterTest::imageNoData});

    addInstancedTests({&CgltfImporterTest::imageBasis},
                      Containers::arraySize(ImageBasisData));

    addTests({&CgltfImporterTest::imageMipLevels});

    addInstancedTests({&CgltfImporterTest::fileCallbackBuffer,
                       &CgltfImporterTest::fileCallbackBufferNotFound,
                       &CgltfImporterTest::fileCallbackImage,
                       &CgltfImporterTest::fileCallbackImageNotFound},
                      Containers::arraySize(SingleFileData));

    addTests({&CgltfImporterTest::utf8filenames,
              &CgltfImporterTest::escapedStrings,
              &CgltfImporterTest::encodedUris,

              &CgltfImporterTest::versionSupported});

    addInstancedTests({&CgltfImporterTest::versionUnsupported},
                      Containers::arraySize(UnsupportedVersionData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. Reset
       the plugin dir after so it doesn't load anything else from the filesystem. */
    #ifdef CGLTFIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(CGLTFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    _manager.setPluginDirectory({});
    #endif
    #ifdef BASISIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(BASISIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void CgltfImporterTest::open() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    auto filename = Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "empty" + std::string{data.suffix});
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_VERIFY(!importer->importerState());

    CORRADE_VERIFY(importer->openData(Utility::Directory::read(filename)));
    CORRADE_VERIFY(importer->isOpened());
    CORRADE_VERIFY(!importer->importerState());

    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void CgltfImporterTest::openError() {
    auto&& data = OpenErrorData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::ostringstream out;
    Error redirectError{&out};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(!importer->openData(data.data));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::openData(): error opening file: {}\n", data.message));
}

void CgltfImporterTest::openExternalDataOrder() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    struct CallbackData {
        Containers::StaticArray<3, std::size_t> counts{ValueInit};
        Containers::StaticArray<3, InputFileCallbackPolicy> policies{ValueInit};
        Containers::StaticArray<3, bool> closed{ValueInit};
        Utility::Resource rs{"data"};
    } callbackData{};

    importer->setFileCallback([](const std::string& filename, InputFileCallbackPolicy policy, CallbackData& callbackData)
            -> Containers::Optional<Containers::ArrayView<const char>>
        {
            std::size_t index = 0;
            if(filename.find("data1.bin") == 0)
                index = 0;
            else if(filename.find("data2.bin") == 0)
                index = 1;
            else if(filename.find("data.png") == 0)
                index = 2;

            if(policy == InputFileCallbackPolicy::Close)
                callbackData.closed[index] = true;
            else {
                callbackData.closed[index] = false;
                callbackData.policies[index] = policy;
            }
            ++callbackData.counts[index];

            return callbackData.rs.getRaw(Utility::Directory::join("some/path", filename));
        }, callbackData);

    /* Prevent the file callback being used for the main glTF content */
    const auto content = Utility::Directory::read(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "external-data-order" + std::string{data.suffix}));
    CORRADE_VERIFY(importer->openData(content));

    CORRADE_COMPARE(importer->meshCount(), 4);
    CORRADE_COMPARE(importer->image2DCount(), 2);

    /* Buffers and images are only loaded on demand */
    CORRADE_COMPARE_AS(callbackData.counts, Containers::arrayView<std::size_t>({0, 0, 0}), TestSuite::Compare::Container);

    CORRADE_VERIFY(importer->mesh(0));
    CORRADE_COMPARE_AS(callbackData.counts, Containers::arrayView<std::size_t>({1, 0, 0}), TestSuite::Compare::Container);
    CORRADE_COMPARE(callbackData.policies[0], InputFileCallbackPolicy::LoadPermanent);

    CORRADE_VERIFY(importer->mesh(1));
    CORRADE_COMPARE_AS(callbackData.counts, Containers::arrayView<std::size_t>({1, 1, 0}), TestSuite::Compare::Container);
    CORRADE_COMPARE(callbackData.policies[1], InputFileCallbackPolicy::LoadPermanent);

    /* Buffer content is cached. An already loaded buffer should not invoke the
       file callback again. */

    /* Mesh already loaded */
    CORRADE_VERIFY(importer->mesh(0));
    CORRADE_COMPARE_AS(callbackData.counts, Containers::arrayView<std::size_t>({1, 1, 0}), TestSuite::Compare::Container);
    /* Different mesh, same buffer as mesh 0 */
    CORRADE_VERIFY(importer->mesh(2));
    CORRADE_COMPARE_AS(callbackData.counts, Containers::arrayView<std::size_t>({1, 1, 0}), TestSuite::Compare::Container);
    /* Different mesh, different buffer, but same URI. The caching does not
       use URI, only buffer id. */
    CORRADE_VERIFY(importer->mesh(3));
    CORRADE_COMPARE_AS(callbackData.counts, Containers::arrayView<std::size_t>({2, 1, 0}), TestSuite::Compare::Container);
    CORRADE_COMPARE(callbackData.policies[0], InputFileCallbackPolicy::LoadPermanent);

    /* Image content is not cached. Requesting the same image later should
       result in two callback invocations. However, the image importer is
       cached, so the file callback is only called again if we load a different
       image in between. */
    CORRADE_VERIFY(importer->image2D(0));
    /* Count increases by 2 because file callback is invoked with LoadTemporary
       followed by Close */
    CORRADE_COMPARE_AS(callbackData.counts, Containers::arrayView<std::size_t>({2, 1, 2}), TestSuite::Compare::Container);
    CORRADE_COMPARE(callbackData.policies[2], InputFileCallbackPolicy::LoadTemporary);

    /* Same importer */
    CORRADE_VERIFY(importer->image2D(0));
    CORRADE_COMPARE_AS(callbackData.counts, Containers::arrayView<std::size_t>({2, 1, 2}), TestSuite::Compare::Container);
    /* Same URI, but different image. Importer caching uses the image id, not
       the URI. */
    CORRADE_VERIFY(importer->image2D(1));
    CORRADE_COMPARE_AS(callbackData.counts, Containers::arrayView<std::size_t>({2, 1, 4}), TestSuite::Compare::Container);
    CORRADE_VERIFY(importer->image2D(0));
    CORRADE_COMPARE_AS(callbackData.counts, Containers::arrayView<std::size_t>({2, 1, 6}), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(callbackData.closed, Containers::arrayView<bool>({false, false, true}), TestSuite::Compare::Container);
}

void CgltfImporterTest::openExternalDataNotFound() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    auto filename = Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "buffer-notfound" + std::string{data.suffix});

    /* Importing should succeed, buffers are loaded on demand */
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_COMPARE(importer->meshCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::mesh(): error opening file: /nonexistent.bin : file not found\n");
}

void CgltfImporterTest::openExternalDataNoPathNoCallback() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    auto filename = Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "buffer-notfound" + std::string{data.suffix});

    CORRADE_VERIFY(importer->openData(Utility::Directory::read(filename)));
    CORRADE_COMPARE(importer->meshCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::mesh(): external buffers can be imported only when opening files from the filesystem or if a file callback is present\n");
}

void CgltfImporterTest::openExternalDataTooLong() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "buffer-wrong-size" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_VERIFY(importer->mesh(0));
}

void CgltfImporterTest::openExternalDataTooShort() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "buffer-short-size" + std::string{data.suffix})));
    CORRADE_COMPARE(importer->meshCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::mesh(): buffer 0 is too short, expected 24 bytes but got 12\n");
}

void CgltfImporterTest::openExternalDataNoUri() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "buffer-no-uri" + std::string{data.suffix})));
    CORRADE_COMPARE(importer->meshCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::mesh(): buffer 1 has no URI\n");
}

void CgltfImporterTest::openExternalDataInvalidUri() {
    auto&& data = InvalidUriData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "uri-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->image2DCount(), Containers::arraySize(InvalidUriData));

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->image2D(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::image2D(): {}\n", data.message));
}

void CgltfImporterTest::requiredExtensions() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "required-extensions.gltf")));
}

void CgltfImporterTest::requiredExtensionsUnsupported() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    /* Disabled by default */
    CORRADE_VERIFY(!importer->configuration().value<bool>("ignoreRequiredExtensions"));

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "required-extensions-unsupported.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::openData(): required extension EXT_lights_image_based not supported\n");
}

void CgltfImporterTest::requiredExtensionsUnsupportedDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->configuration().setValue("ignoreRequiredExtensions", true));

    std::ostringstream out;
    Warning redirectError{&out};

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "required-extensions-unsupported.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::openData(): required extension EXT_lights_image_based not supported\n");
}

void CgltfImporterTest::animation() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->animationCount(), 4);

    /* Empty animation */
    {
        CORRADE_COMPARE(importer->animationName(0), "empty");
        CORRADE_COMPARE(importer->animationForName("empty"), 0);

        auto animation = importer->animation(0);
        CORRADE_VERIFY(animation);
        CORRADE_VERIFY(animation->data().empty());
        CORRADE_COMPARE(animation->trackCount(), 0);

    /* Empty translation/rotation/scaling animation */
    } {
        CORRADE_COMPARE(importer->animationName(1), "empty TRS animation");
        CORRADE_COMPARE(importer->animationForName("empty TRS animation"), 1);

        auto animation = importer->animation(1);
        CORRADE_VERIFY(animation);
        CORRADE_VERIFY(!animation->importerState());

        CORRADE_COMPARE(animation->data().size(), 0);
        CORRADE_COMPARE(animation->trackCount(), 3);

        /* Not really checking much here, just making sure that this is handled
           gracefully */

        CORRADE_COMPARE(animation->trackTargetType(0), AnimationTrackTargetType::Rotation3D);
        auto rotation = animation->track(0);
        CORRADE_VERIFY(rotation.keys().empty());
        CORRADE_VERIFY(rotation.values().empty());

        CORRADE_COMPARE(animation->trackTargetType(1), AnimationTrackTargetType::Translation3D);
        auto translation = animation->track(1);
        CORRADE_VERIFY(translation.keys().empty());
        CORRADE_VERIFY(translation.values().empty());

        CORRADE_COMPARE(animation->trackTargetType(2), AnimationTrackTargetType::Scaling3D);
        auto scaling = animation->track(2);
        CORRADE_VERIFY(scaling.keys().empty());
        CORRADE_VERIFY(scaling.values().empty());

    /* Translation/rotation/scaling animation */
    } {
        CORRADE_COMPARE(importer->animationName(2), "TRS animation");
        CORRADE_COMPARE(importer->animationForName("TRS animation"), 2);

        auto animation = importer->animation(2);
        CORRADE_VERIFY(animation);
        CORRADE_VERIFY(!animation->importerState());
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
}

void CgltfImporterTest::animationOutOfBounds() {
    auto&& data = AnimationOutOfBoundsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, data.file)));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::openData(): error opening file: invalid glTF, usually caused by invalid indices or missing required attributes\n");
}

void CgltfImporterTest::animationInvalid() {
    auto&& data = AnimationInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-invalid.gltf")));

    /* Check we didn't forget to test anything. We skip the invalid
       interpolation mode because that imports without errors and defaults to
       linear interpolation, tested in animationInvalidInterpolation(). */
    CORRADE_COMPARE(importer->animationCount(), Containers::arraySize(AnimationInvalidData) + 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::animation(): {}\n", data.message));
}

void CgltfImporterTest::animationInvalidBufferNotFound() {
    auto&& data = AnimationInvalidBufferNotFoundData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* These tests have to be separate from TinyGltfImporter because it errors
       out during import trying to load the buffer */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "animation-buffer-notfound.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->animationCount(), Containers::arraySize(AnimationInvalidBufferNotFoundData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::animation(): {}\n", data.message));
}

void CgltfImporterTest::animationInvalidInterpolation() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-invalid.gltf")));

    auto animation = importer->animation("unsupported interpolation type");
    {
        CORRADE_EXPECT_FAIL("Cgltf parses an invalid interpolation mode as linear, without any error.");
        CORRADE_VERIFY(!animation);
    }
    CORRADE_COMPARE(animation->trackCount(), 1);
    auto track = animation->track(0);
    CORRADE_COMPARE(track.interpolation(), Animation::Interpolation::Linear);
}

void CgltfImporterTest::animationInvalidTypes() {
    auto&& data = AnimationInvalidTypesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "animation-invalid-types.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->animationCount(), Containers::arraySize(AnimationInvalidTypesData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::animation(): {}\n", data.message));
}

void CgltfImporterTest::animationMismatchingCount() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    /* Different input/output accessor counts are not allowed. This
       TinyGltfImporter test file has them, so we repurpose it. */
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-patching.gltf")));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation("Quaternion normalization patching"));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::animation(): target track size doesn't match time track size, expected 3 but got 9\n");
}

void CgltfImporterTest::animationMissingTargetNode() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-missing-target-node.gltf")));
    CORRADE_COMPARE(importer->animationCount(), 1);

    /* The importer skips channels that don't have a target node */

    auto animation = importer->animation(0);
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

void CgltfImporterTest::animationSpline() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation" + std::string{data.suffix})));
    CORRADE_COMPARE(importer->animationCount(), 4);
    CORRADE_COMPARE(importer->animationName(3), "TRS animation, splines");

    auto animation = importer->animation(3);
    CORRADE_VERIFY(animation);
    CORRADE_VERIFY(!animation->importerState());
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

void CgltfImporterTest::animationSplineSharedWithSameTimeTrack() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-splines-sharing.gltf")));
    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(0), "TRS animation, splines, sharing data with the same time track");

    auto animation = importer->animation(0);
    CORRADE_VERIFY(animation);
    CORRADE_VERIFY(!animation->importerState());
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

void CgltfImporterTest::animationSplineSharedWithDifferentTimeTrack() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "animation-splines-sharing.gltf")));
    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(1), "TRS animation, splines, sharing data with different time track");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(1));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::animation(): spline track is shared with different time tracks, we don't support that, sorry\n");
}

void CgltfImporterTest::animationShortestPathOptimizationEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("optimizeQuaternionShortestPath"));
    /* tinygltf allows animation samplers with different input and output sizes
       and picks the smaller size, but cgltf complains about it, nor is it
       allowed by the spec. So we need our own test file. */
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "animation-patching-fixed.gltf")));

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

void CgltfImporterTest::animationShortestPathOptimizationDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    /* Explicitly disable */
    importer->configuration().setValue("optimizeQuaternionShortestPath", false);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "animation-patching-fixed.gltf")));

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

void CgltfImporterTest::animationQuaternionNormalizationEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("normalizeQuaternions"));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "animation-patching-fixed.gltf")));
    CORRADE_COMPARE(importer->animationCount(), 2);
    CORRADE_COMPARE(importer->animationName(1), "Quaternion normalization patching");

    Containers::Optional<AnimationData> animation;
    std::ostringstream out;
    {
        Warning warningRedirection{&out};
        animation = importer->animation(1);
    }
    CORRADE_VERIFY(animation);
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::animation(): quaternions in some rotation tracks were renormalized\n");
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

void CgltfImporterTest::animationQuaternionNormalizationDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    /* Explicitly disable */
    CORRADE_VERIFY(importer->configuration().setValue("normalizeQuaternions", false));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "animation-patching-fixed.gltf")));
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
    CORRADE_COMPARE_AS(track.values(), Containers::stridedArrayView(rotationValues), TestSuite::Compare::Container);
}

void CgltfImporterTest::animationMergeEmpty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    /* Enable animation merging */
    importer->configuration().setValue("mergeAnimationClips", true);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "empty.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->animationForName(""), -1);
}

void CgltfImporterTest::animationMerge() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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
    CORRADE_VERIFY(rotation.keys().empty());
    CORRADE_VERIFY(rotation.values().empty());

    /* Translation, empty */
    CORRADE_COMPARE(animation->trackType(1), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(1), AnimationTrackTargetType::Translation3D);
    CORRADE_COMPARE(animation->trackTarget(1), 1);
    Animation::TrackViewStorage<const Float> translation = animation->track(1);
    CORRADE_COMPARE(translation.interpolation(), Animation::Interpolation::Constant);
    CORRADE_VERIFY(translation.keys().empty());
    CORRADE_VERIFY(translation.values().empty());

    /* Scaling, empty */
    CORRADE_COMPARE(animation->trackType(2), AnimationTrackType::Vector3);
    CORRADE_COMPARE(animation->trackTargetType(2), AnimationTrackTargetType::Scaling3D);
    CORRADE_COMPARE(animation->trackTarget(2), 2);
    Animation::TrackViewStorage<const Float> scaling = animation->track(2);
    CORRADE_COMPARE(scaling.interpolation(), Animation::Interpolation::Linear);
    CORRADE_VERIFY(scaling.keys().empty());
    CORRADE_VERIFY(scaling.values().empty());

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

void CgltfImporterTest::camera() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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

void CgltfImporterTest::cameraInvalidType() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "camera-invalid-type.gltf")));
    CORRADE_COMPARE(importer->cameraCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->camera(0));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::camera(): invalid camera type\n");
}

void CgltfImporterTest::light() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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

void CgltfImporterTest::lightInvalid() {
    auto&& data = LightInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "light-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->lightCount(), Containers::arraySize(LightInvalidData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->light(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::light(): {}\n", data.message));
}

void CgltfImporterTest::lightInvalidColorSize() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "light-invalid-color-size.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::openData(): error opening file: invalid glTF, usually caused by invalid indices or missing required attributes\n");
}

void CgltfImporterTest::lightMissingType() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "light-missing-type.gltf")));
    CORRADE_COMPARE(importer->lightCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->light(0));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::light(): invalid light type\n");
}

void CgltfImporterTest::lightMissingSpot() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "light-missing-spot.gltf")));
    CORRADE_COMPARE(importer->lightCount(), 1);

    auto light = importer->light(0);
    {
        CORRADE_EXPECT_FAIL("The spot object is required for lights of type spot but cgltf doesn't care if it's missing. It just sets everything to default values.");
        CORRADE_VERIFY(!light);
    }

    CORRADE_COMPARE(light->type(), LightData::Type::Spot);
    CORRADE_COMPARE(light->color(), (Color3{1.0f, 1.0f, 1.0f}));
    CORRADE_COMPARE(light->intensity(), 1.0f);
    CORRADE_COMPARE(light->attenuation(), (Vector3{1.0f, 0.0f, 1.0f}));
    CORRADE_COMPARE(light->range(), Constants::inf());
    CORRADE_COMPARE(light->innerConeAngle(), 0.0_radf);
    /* Magnum uses full angles, glTF uses half angles */
    CORRADE_COMPARE(light->outerConeAngle(), Rad{45.0_degf*2.0f});
}

void CgltfImporterTest::scene() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "scene" + std::string{data.suffix})));

    /* Explicit default scene */
    CORRADE_COMPARE(importer->defaultScene(), 1);
    CORRADE_COMPARE(importer->sceneCount(), 2);
    CORRADE_COMPARE(importer->sceneName(1), "Scene");
    CORRADE_COMPARE(importer->sceneForName("Scene"), 1);

    auto emptyScene = importer->scene(0);
    CORRADE_VERIFY(emptyScene);
    CORRADE_VERIFY(!emptyScene->importerState());
    CORRADE_COMPARE(emptyScene->children3D(), std::vector<UnsignedInt>{});

    auto scene = importer->scene(1);
    CORRADE_VERIFY(scene);
    CORRADE_VERIFY(!scene->importerState());
    CORRADE_COMPARE(scene->children3D(), (std::vector<UnsignedInt>{2, 4}));

    CORRADE_COMPARE(importer->object3DCount(), 7);

    CORRADE_COMPARE(importer->object3DName(4), "Light");
    CORRADE_COMPARE(importer->object3DForName("Light"), 4);

    {
        auto object = importer->object3D("Camera");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(!object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Camera);
        CORRADE_COMPARE(object->instance(), 2);
        CORRADE_VERIFY(object->children().empty());
    } {
        auto object = importer->object3D("Empty with one child");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(!object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->instance(), -1);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{0}));
    } {
        auto object = importer->object3D("Mesh w/o material");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(!object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 1);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).material(), -1);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).skin(), -1);
        CORRADE_VERIFY(object->children().empty());
    } {
        auto object = importer->object3D("Mesh and a material");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(!object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 0);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).material(), 1);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).skin(), -1);
        CORRADE_VERIFY(object->children().empty());
    } {
        auto object = importer->object3D("Mesh and a skin");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(!object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Mesh);
        CORRADE_COMPARE(object->instance(), 1);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).material(), -1);
        CORRADE_COMPARE(static_cast<MeshObjectData3D&>(*object).skin(), 1);
        CORRADE_VERIFY(object->children().empty());
    } {
        auto object = importer->object3D("Light");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(!object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Light);
        CORRADE_COMPARE(object->instance(), 1);
        CORRADE_VERIFY(object->children().empty());
    } {
        auto object = importer->object3D("Empty with two children");
        CORRADE_VERIFY(object);
        CORRADE_VERIFY(!object->importerState());
        CORRADE_COMPARE(object->instanceType(), ObjectInstanceType3D::Empty);
        CORRADE_COMPARE(object->children(), (std::vector<UnsignedInt>{3, 1}));
    }
}

void CgltfImporterTest::sceneEmpty() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "empty" + std::string{data.suffix})));

    /* There is no scene, can't have any default */
    CORRADE_COMPARE(importer->defaultScene(), -1);
    CORRADE_COMPARE(importer->sceneCount(), 0);
}

void CgltfImporterTest::sceneNoDefault() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "scene-nodefault" + std::string{data.suffix})));

    /* There is at least one scene, it's made default */
    CORRADE_COMPARE(importer->defaultScene(), 0);
    CORRADE_COMPARE(importer->sceneCount(), 1);

    auto scene = importer->scene(0);
    CORRADE_VERIFY(scene);
    CORRADE_VERIFY(scene->children3D().empty());
}

void CgltfImporterTest::sceneOutOfBounds() {
    auto&& data = SceneOutOfBoundsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, data.file)));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::openData(): error opening file: invalid glTF, usually caused by invalid indices or missing required attributes\n");
}

void CgltfImporterTest::sceneInvalid() {
    auto&& data = SceneInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    /* For some reason node relationships are checked in cgltf_parse and not in
       cgltf_validate. Cycles are checked in cgltf_validate again. */

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, data.file)));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::openData(): error opening file: invalid glTF, usually caused by invalid indices or missing required attributes\n");
}

void CgltfImporterTest::sceneCycle() {
    auto&& data = SceneCycleData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, data.file)));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::openData(): node tree contains cycle starting at node 0\n");
}

void CgltfImporterTest::objectTransformation() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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

void CgltfImporterTest::objectTransformationQuaternionNormalizationEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::object3D(): rotation quaternion was renormalized\n");
    CORRADE_COMPARE(object->flags(), ObjectFlag3D::HasTranslationRotationScaling);
    CORRADE_COMPARE(object->rotation(), Quaternion::rotation(45.0_degf, Vector3::yAxis()));
}

void CgltfImporterTest::objectTransformationQuaternionNormalizationDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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

void CgltfImporterTest::skin() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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
        CORRADE_VERIFY(!skin->importerState());
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
        CORRADE_VERIFY(!skin->importerState());
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

void CgltfImporterTest::skinOutOfBounds() {
    auto&& data = SkinOutOfBoundsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, data.file)));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::openData(): error opening file: invalid glTF, usually caused by invalid indices or missing required attributes\n");
}

void CgltfImporterTest::skinInvalid() {
    auto&& data = SkinInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "skin-invalid.gltf")));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->skin3D(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::skin3D(): {}\n", data.message));
}

void CgltfImporterTest::skinInvalidBufferNotFound() {
    /* This test has to be separate from TinyGltfImporter because it errors
       out during import trying to load the buffer */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "skin-buffer-notfound.gltf")));

    CORRADE_COMPARE(importer->skin3DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->skin3D("buffer not found"));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::skin3D(): error opening file: /nonexistent.bin : file not found\n");
}

void CgltfImporterTest::skinInvalidTypes() {
    auto&& data = SkinInvalidTypesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "skin-invalid-types.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->skin3DCount(), Containers::arraySize(AnimationInvalidTypesData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->skin3D(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::skin3D(): {}\n", data.message));
}

void CgltfImporterTest::skinNoJointsProperty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "skin-no-joints.gltf")));
    CORRADE_COMPARE(importer->skin3DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->skin3D(0));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::skin3D(): skin has no joints\n");
}

void CgltfImporterTest::mesh() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->meshCount(), 4);
    CORRADE_COMPARE(importer->meshName(0), "Non-indexed mesh");
    CORRADE_COMPARE(importer->meshForName("Non-indexed mesh"), 0);

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->importerState());
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

void CgltfImporterTest::meshAttributeless() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh.gltf")));

    auto mesh = importer->mesh("Attribute-less mesh");
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->importerState());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->vertexCount(), 0);
    CORRADE_COMPARE(mesh->attributeCount(), 0);
}

void CgltfImporterTest::meshIndexed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh.gltf")));

    CORRADE_COMPARE(importer->meshCount(), 4);
    CORRADE_COMPARE(importer->meshName(1), "Indexed mesh");
    CORRADE_COMPARE(importer->meshForName("Indexed mesh"), 1);

    auto mesh = importer->mesh(1);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->importerState());
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

void CgltfImporterTest::meshIndexedAttributeless() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh.gltf")));

    auto mesh = importer->mesh("Attribute-less indexed mesh");
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->importerState());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE_AS(mesh->indicesAsArray(),
        Containers::arrayView<UnsignedInt>({0, 1, 2}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->vertexCount(), 0);
    CORRADE_COMPARE(mesh->attributeCount(), 0);
}

void CgltfImporterTest::meshColors() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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

void CgltfImporterTest::meshSkinAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-skin-attributes.gltf")));

    /* The mapping should be available even before the mesh is imported */
    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    CORRADE_VERIFY(jointsAttribute != MeshAttribute{});
    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");
    CORRADE_VERIFY(weightsAttribute != MeshAttribute{});

    CORRADE_COMPARE(importer->meshAttributeForName("JOINTS_0"), MeshAttribute{});
    CORRADE_COMPARE(importer->meshAttributeForName("JOINTS_1"), MeshAttribute{});
    CORRADE_COMPARE(importer->meshAttributeForName("WEIGHTS_0"), MeshAttribute{});
    CORRADE_COMPARE(importer->meshAttributeForName("WEIGHTS_1"), MeshAttribute{});

    CORRADE_COMPARE(importer->meshCount(), 1);

    auto mesh = importer->mesh(0);
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

    /* Custom attributes with multiple sets */
    CORRADE_COMPARE(mesh->attributeCount(jointsAttribute), 2);
    CORRADE_COMPARE(mesh->attributeFormat(jointsAttribute, 0), VertexFormat::Vector4ub);
    CORRADE_COMPARE_AS(mesh->attribute<Vector4ub>(jointsAttribute),
        Containers::arrayView<Vector4ub>({
            {1,  2,  3,  4},
            {5,  6,  7,  8},
            {9, 10, 11, 12}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeFormat(jointsAttribute, 1), VertexFormat::Vector4us);
    CORRADE_COMPARE_AS(mesh->attribute<Vector4us>(jointsAttribute, 1),
        Containers::arrayView<Vector4us>({
            {13, 14, 15, 16},
            {17, 18, 19, 20},
            {21, 22, 23, 24}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(weightsAttribute), 2);
    CORRADE_COMPARE(mesh->attributeFormat(weightsAttribute, 0), VertexFormat::Vector4);
    CORRADE_COMPARE_AS(mesh->attribute<Vector4>(weightsAttribute),
        Containers::arrayView<Vector4>({
            {0.125f, 0.25f, 0.375f, 0.0f},
            {0.1f,   0.05f, 0.05f,  0.05f},
            {0.2f,   0.0f,  0.3f,   0.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeFormat(weightsAttribute, 1), VertexFormat::Vector4usNormalized);
    CORRADE_COMPARE_AS(mesh->attribute<Vector4us>(weightsAttribute, 1),
        Containers::arrayView<Vector4us>({
            {       0, 0xffff/8,         0, 0xffff/8},
            {0xffff/2, 0xffff/8, 0xffff/16, 0xffff/16},
            {       0, 0xffff/4, 0xffff/4,  0}
        }), TestSuite::Compare::Container);
}

void CgltfImporterTest::meshCustomAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
            "mesh-custom-attributes.gltf")));
        CORRADE_COMPARE(importer->meshCount(), 2);

        CORRADE_COMPARE(out.str(),
            "Trade::CgltfImporter::openData(): unknown attribute OBJECT_ID3, importing as custom attribute\n"
            "Trade::CgltfImporter::openData(): unknown attribute NOT_AN_IDENTITY, importing as custom attribute\n");
    }

    /* The mapping should be available even before the mesh is imported.
       Attributes are sorted in declaration order. */
    const MeshAttribute tbnAttribute = importer->meshAttributeForName("_TBN");
    CORRADE_COMPARE(tbnAttribute, meshAttributeCustom(0));
    CORRADE_COMPARE(importer->meshAttributeName(tbnAttribute), "_TBN");

    const MeshAttribute uvRotation = importer->meshAttributeForName("_UV_ROTATION");
    CORRADE_COMPARE(uvRotation, meshAttributeCustom(1));
    CORRADE_COMPARE(importer->meshAttributeName(uvRotation), "_UV_ROTATION");

    const MeshAttribute tbnPreciserAttribute = importer->meshAttributeForName("_TBN_PRECISER");
    const MeshAttribute objectIdAttribute = importer->meshAttributeForName("OBJECT_ID3");

    const MeshAttribute doubleShotAttribute = importer->meshAttributeForName("_DOUBLE_SHOT");
    CORRADE_COMPARE(doubleShotAttribute, meshAttributeCustom(6));
    const MeshAttribute negativePaddingAttribute = importer->meshAttributeForName("_NEGATIVE_PADDING");
    CORRADE_COMPARE(negativePaddingAttribute, meshAttributeCustom(4));
    const MeshAttribute notAnIdentityAttribute = importer->meshAttributeForName("NOT_AN_IDENTITY");
    CORRADE_VERIFY(notAnIdentityAttribute != MeshAttribute{});

    auto mesh = importer->mesh("standard types");
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

    /* Not testing import failure of non-core glTF attribute types, that's
       already tested in meshInvalid() */
}

void CgltfImporterTest::meshCustomAttributesNoFileOpened() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    /* These should return nothing (and not crash) */
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(564)), "");
    CORRADE_COMPARE(importer->meshAttributeForName("thing"), MeshAttribute{});
}

void CgltfImporterTest::meshDuplicateAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-duplicate-attributes.gltf")));
    CORRADE_COMPARE(importer->meshCount(), 1);

    const MeshAttribute thingAttribute = importer->meshAttributeForName("_THING");
    CORRADE_VERIFY(thingAttribute != MeshAttribute{});

    auto mesh = importer->mesh(0);
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

void CgltfImporterTest::meshUnorderedAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-unordered-attributes.gltf")));
    CORRADE_COMPARE(importer->meshCount(), 1);

    const MeshAttribute customAttribute4 = importer->meshAttributeForName("_CUSTOM_4");
    CORRADE_VERIFY(customAttribute4 != MeshAttribute{});
    const MeshAttribute customAttribute1 = importer->meshAttributeForName("_CUSTOM_1");
    CORRADE_VERIFY(customAttribute1 != MeshAttribute{});

    /* Custom attributes are sorted in declaration order */
    CORRADE_VERIFY(customAttribute4 < customAttribute1);

    std::ostringstream out;
    Warning redirectWarning{&out};

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(), 7);

    /* No warning about _CUSTOM_4 and _CUSTOM_1 */
    CORRADE_COMPARE(out.str(),
        "Trade::CgltfImporter::mesh(): found attribute COLOR_3 but expected COLOR_0\n"
        "Trade::CgltfImporter::mesh(): found attribute COLOR_9 but expected COLOR_4\n"
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

    /* Custom attributes (besides JOINTS and WEIGHTS) don't have sets */
    CORRADE_VERIFY(mesh->hasAttribute(customAttribute4));
    CORRADE_COMPARE(mesh->attributeCount(customAttribute4), 1);
    CORRADE_COMPARE(mesh->attributeFormat(customAttribute4), VertexFormat::Vector2);

    CORRADE_VERIFY(mesh->hasAttribute(customAttribute1));
    CORRADE_COMPARE(mesh->attributeCount(customAttribute1), 1);
    CORRADE_COMPARE(mesh->attributeFormat(customAttribute1), VertexFormat::Vector3);
}

void CgltfImporterTest::meshMultiplePrimitives() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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

void CgltfImporterTest::meshPrimitivesTypes() {
    auto&& data = MeshPrimitivesTypesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Disable Y-flipping to have consistent results. Tested separately for all
       types in materialTexCoordFlip(). */
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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

void CgltfImporterTest::meshOutOfBounds() {
    auto&& data = MeshOutOfBoundsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, data.file)));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::openData(): error opening file: invalid glTF, usually caused by invalid indices or missing required attributes\n");
}

void CgltfImporterTest::meshInvalid() {
    auto&& data = MeshInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "mesh-invalid.gltf")));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::mesh(): {}\n", data.message));
}

void CgltfImporterTest::meshInvalidIndicesBufferNotFound() {
    /* This test has to be separate from TinyGltfImporter because it errors
       out during import trying to load the buffer.

       Not testing this for the attribute buffer since that's already done by
       openExternalDataNotFound(). */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "mesh-indices-buffer-notfound.gltf")));

    CORRADE_COMPARE(importer->meshCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh("indices buffer not found"));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::mesh(): error opening file: /nonexistent.bin : file not found\n");
}

void CgltfImporterTest::meshInvalidTypes() {
    auto&& data = MeshInvalidTypesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "mesh-invalid-types.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->meshCount(), Containers::arraySize(MeshInvalidTypesData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::mesh(): {}\n", data.message));
}

void CgltfImporterTest::materialPbrMetallicRoughness() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

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
        CORRADE_VERIFY(!material->importerState());
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

void CgltfImporterTest::materialPbrSpecularGlossiness() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

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
        CORRADE_VERIFY(!material->importerState());
        CORRADE_COMPARE(material->types(), MaterialType::PbrSpecularGlossiness);
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
        CORRADE_COMPARE(material->attributeCount(), 5);
        /* Identity transform, but is present */
        const auto& pbr = material->as<PbrSpecularGlossinessMaterialData>();
        CORRADE_VERIFY(pbr.hasTextureTransformation());
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

void CgltfImporterTest::materialCommon() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-common.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 7);

    {
        auto material = importer->material("defaults");
        CORRADE_COMPARE(material->types(), MaterialTypes{});
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

void CgltfImporterTest::materialUnlit() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-unlit.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    auto material = importer->material(0);
    CORRADE_VERIFY(material);
    CORRADE_VERIFY(!material->importerState());
    /* Metallic/roughness is removed from types */
    CORRADE_COMPARE(material->types(), MaterialType::Flat);
    CORRADE_COMPARE(material->layerCount(), 1);
    CORRADE_COMPARE(material->attributeCount(), 2);

    const auto& flat = material->as<FlatMaterialData>();
    CORRADE_COMPARE(flat.color(), (Color4{0.7f, 0.8f, 0.9f, 1.1f}));
    CORRADE_VERIFY(flat.hasTexture());
    CORRADE_COMPARE(flat.texture(), 1);
}

void CgltfImporterTest::materialClearCoat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

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
        CORRADE_COMPARE(material->types(), MaterialType::PbrClearCoat);
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
        CORRADE_COMPARE(pbr.attributeCount(), 7 + 3);
        CORRADE_VERIFY(pbr.hasTextureTransformation());
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

void CgltfImporterTest::materialPhongFallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

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
        CORRADE_VERIFY(!material->importerState());
        CORRADE_COMPARE(material->types(), MaterialType::Phong);
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
        CORRADE_COMPARE(material->types(), MaterialType::Phong|MaterialType::PbrSpecularGlossiness);
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

void CgltfImporterTest::materialOutOfBounds() {
    auto&& data = MaterialOutOfBoundsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, data.file)));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::openData(): error opening file: invalid glTF, usually caused by invalid indices or missing required attributes\n");
}

void CgltfImporterTest::materialInvalidAlphaMode() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    /* Cgltf parses an invalid alpha mode as opaque, without any error */
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "material-invalid-alpha-mode.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    auto material = importer->material(0);
    CORRADE_VERIFY(material);
    CORRADE_COMPARE(material->alphaMode(), MaterialAlphaMode::Opaque);
}

void CgltfImporterTest::materialTexCoordFlip() {
    auto&& data = MaterialTexCoordFlipData[testCaseInstanceId()];
    setTestCaseDescription(Utility::formatString("{}{}", data.name, data.flipInMaterial ? ", textureCoordinateYFlipInMaterial" : ""));

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

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

void CgltfImporterTest::texture() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

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
    CORRADE_VERIFY(!texture->importerState());
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->type(), TextureType::Texture2D);

    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Nearest);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Nearest);
    CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Nearest);

    CORRADE_COMPARE(texture->wrapping(), Math::Vector3<SamplerWrapping>(SamplerWrapping::MirroredRepeat, SamplerWrapping::ClampToEdge, SamplerWrapping::Repeat));

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

void CgltfImporterTest::textureOutOfBounds() {
    auto&& data = TextureOutOfBoundsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, data.file)));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::openData(): error opening file: invalid glTF, usually caused by invalid indices or missing required attributes\n");
}

void CgltfImporterTest::textureInvalid() {
    auto&& data = TextureInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->textureCount(), Containers::arraySize(TextureInvalidData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->texture(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::texture(): {}\n", data.message));
}

void CgltfImporterTest::textureDefaultSampler() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture-default-sampler" + std::string{data.suffix})));

    auto texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->type(), TextureType::Texture2D);

    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Linear);

    CORRADE_COMPARE(texture->wrapping(), Math::Vector3<SamplerWrapping>(SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping::Repeat));
}

void CgltfImporterTest::textureEmptySampler() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture-empty-sampler" + std::string{data.suffix})));

    auto texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), 0);
    CORRADE_COMPARE(texture->type(), TextureType::Texture2D);

    CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Linear);

    CORRADE_COMPARE(texture->wrapping(), Math::Vector3<SamplerWrapping>(SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping::Repeat));
}

void CgltfImporterTest::textureMissingSource() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture-missing-source.gltf")));
    CORRADE_COMPARE(importer->textureCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->texture(0));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::texture(): no image source found\n");
}

void CgltfImporterTest::textureExtensions() {
    auto&& data = TextureExtensionsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture-extensions.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->textureCount(), Containers::arraySize(TextureExtensionsData));

    auto texture = importer->texture(data.name);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), data.id);
}

void CgltfImporterTest::textureExtensionsOutOfBounds() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    /* Cgltf only supports (and therefore checks) KHR_texture_basisu, so this
       is the only texture extension leading to an error when opening. The rest
       are checked in doTexture(), tested below in textureExtensionsInvalid(). */

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture-extensions-invalid-basisu-oob.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::openData(): error opening file: invalid glTF, usually caused by invalid indices or missing required attributes\n");
}

void CgltfImporterTest::textureExtensionsInvalid() {
    auto&& data = TextureExtensionsInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "texture-extensions-invalid.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->textureCount(), Containers::arraySize(TextureExtensionsInvalidData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->texture(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::texture(): {}\n", data.message));
}

constexpr char ExpectedImageData[] =
    "\xa8\xa7\xac\xff\x9d\x9e\xa0\xff\xad\xad\xac\xff\xbb\xbb\xba\xff\xb3\xb4\xb6\xff"
    "\xb0\xb1\xb6\xff\xa0\xa0\xa1\xff\x9f\x9f\xa0\xff\xbc\xbc\xba\xff\xcc\xcc\xcc\xff"
    "\xb2\xb4\xb9\xff\xb8\xb9\xbb\xff\xc1\xc3\xc2\xff\xbc\xbd\xbf\xff\xb8\xb8\xbc\xff";

void CgltfImporterTest::imageEmbedded() {
    auto&& data = ImageEmbeddedData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    /* Open as data, so we verify opening embedded images from data does not
       cause any problems even when no file callbacks are set */
    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "image" + std::string{data.suffix}))));

    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_COMPARE(importer->image2DForName("Image"), 1);
    CORRADE_COMPARE(importer->image2DName(1), "Image");

    auto image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->importerState());
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(ExpectedImageData).prefix(60), TestSuite::Compare::Container);
}

void CgltfImporterTest::imageExternal() {
    auto&& data = ImageExternalData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "image" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_COMPARE(importer->image2DForName("Image"), 1);
    CORRADE_COMPARE(importer->image2DName(1), "Image");

    auto image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->importerState());
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(ExpectedImageData).prefix(60), TestSuite::Compare::Container);
}

void CgltfImporterTest::imageExternalNotFound() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, "image-notfound.gltf")));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::AbstractImporter::openFile(): cannot open file /nonexistent.png\n");
}

void CgltfImporterTest::imageExternalBufferNotFound() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR, "image-buffer-notfound.gltf")));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::image2D(): error opening file: /nonexistent.bin : file not found\n");
}

void CgltfImporterTest::imageExternalNoPathNoCallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, "image.gltf"))));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::image2D(): external images can be imported only when opening files from the filesystem or if a file callback is present\n");
}

void CgltfImporterTest::imageNoData() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(CGLTFIMPORTER_TEST_DIR,
        "image-no-data.gltf"))));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::image2D(): image has neither a URI nor a buffer view\n");
}

void CgltfImporterTest::imageBasis() {
    auto&& data = ImageBasisData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    /* Import as ASTC */
    _manager.metadata("BasisImporter")->configuration().setValue("format", "Astc4x4RGBA");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "image-basis" + std::string{data.suffix})));

    CORRADE_COMPARE(importer->textureCount(), 1);
    CORRADE_COMPARE(importer->image2DCount(), 2);

    auto image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->importerState());
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc4x4RGBAUnorm);

    /* The texture refers to the image indirectly via an extension, test the
       mapping */
    auto texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), 1);
}

void CgltfImporterTest::imageMipLevels() {
    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    /* Import as RGBA so we can verify the pixels */
    _manager.metadata("BasisImporter")->configuration().setValue("format", "RGBA8");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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
    CORRADE_VERIFY(!image0->importerState());
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
    CORRADE_VERIFY(!image10->importerState());
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
    CORRADE_VERIFY(!image11->importerState());
    CORRADE_VERIFY(!image11->isCompressed());
    CORRADE_COMPARE(image11->size(), (Vector2i{2, 1}));
    CORRADE_COMPARE(image11->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(Containers::arrayCast<const UnsignedByte>(image11->data()),
        Containers::arrayView<UnsignedByte>({
            172, 172, 181, 255, 184, 184, 193, 255
        }), TestSuite::Compare::Container);
}

void CgltfImporterTest::fileCallbackBuffer() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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

void CgltfImporterTest::fileCallbackBufferNotFound() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy, void*)
        -> Containers::Optional<Containers::ArrayView<const char>> { return {}; });

    Utility::Resource rs{"data"};
    CORRADE_VERIFY(importer->openData(rs.getRaw("some/path/data" + std::string{data.suffix})));
    CORRADE_COMPARE(importer->meshCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(), "Trade::CgltfImporter::mesh(): error opening file: data.bin : file callback failed\n");
}

void CgltfImporterTest::fileCallbackImage() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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

void CgltfImporterTest::fileCallbackImageNotFound() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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

void CgltfImporterTest::utf8filenames() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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

void CgltfImporterTest::escapedStrings() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "escaped-strings.gltf")));

    CORRADE_COMPARE(importer->object3DCount(), 6);
    CORRADE_COMPARE(importer->object3DName(0), "");
    CORRADE_COMPARE(importer->object3DName(1), "UTF-8: Лорем ипсум долор сит амет");
    CORRADE_COMPARE(importer->object3DName(2), "UTF-8 escaped: Лорем ипсум долор сит амет");
    CORRADE_COMPARE(importer->object3DName(3), "Special: \"/\\\b\f\r\n\t");
    CORRADE_COMPARE(importer->object3DName(4), "Everything: říční člun \t\t\n حليب اللوز");
    /* Keys (in this case, "name") are not decoded by cgltf. Old versions of
       the spec used to forbid non-ASCII keys or enums:
       https://github.com/KhronosGroup/glTF/tree/fd3ab461a1114fb0250bd76099153d2af50a7a1d/specification/2.0#json-encoding
       Newer spec versions changed this to "ASCII characters [...] SHOULD be
       written without JSON escaping" */
    CORRADE_COMPARE(importer->object3DName(5), "");

    /* All user-facing strings are unescaped. URIs are tested in encodedUris(). */
    CORRADE_COMPARE(importer->animationCount(), 1);
    CORRADE_COMPARE(importer->animationName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->cameraCount(), 1);
    CORRADE_COMPARE(importer->cameraName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->lightCount(), 1);
    CORRADE_COMPARE(importer->lightName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->materialCount(), 1);
    CORRADE_COMPARE(importer->materialName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->meshName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->sceneName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->skin3DCount(), 1);
    CORRADE_COMPARE(importer->skin3DName(0), "Everything: říční člun \t\t\n حليب اللوز");
    CORRADE_COMPARE(importer->textureCount(), 1);
    CORRADE_COMPARE(importer->textureName(0), "Everything: říční člun \t\t\n حليب اللوز");
}

void CgltfImporterTest::encodedUris() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
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
    const auto data = Utility::Directory::read(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "encoded-uris.gltf"));
    CORRADE_VERIFY(importer->openData(data));

    CORRADE_COMPARE(importer->meshCount(), 3);
    /* We don't care about the result, only the callback being invoked */
    importer->mesh(0);
    importer->mesh(1);
    importer->mesh(2);

    CORRADE_COMPARE(importer->image2DCount(), 3);
    importer->image2D(0);
    importer->image2D(1);
    importer->image2D(2);

    CORRADE_COMPARE(strings[0], "buffer-unencoded/@file#.bin");
    CORRADE_COMPARE(strings[1], "buffer-encoded/@file#.bin");
    CORRADE_COMPARE(strings[2], "buffer-escaped/říční člun.bin");
    CORRADE_COMPARE(strings[3], "image-unencoded/image #1.png");
    CORRADE_COMPARE(strings[4], "image-encoded/image #1.png");
    CORRADE_COMPARE(strings[5], "image-escaped/říční člun.png");
}

void CgltfImporterTest::versionSupported() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR,
        "version-supported.gltf")));
}

void CgltfImporterTest::versionUnsupported() {
    auto&& data = UnsupportedVersionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(TINYGLTFIMPORTER_TEST_DIR, data.file)));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::CgltfImporter::openData(): {}\n", data.message));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::CgltfImporterTest)
