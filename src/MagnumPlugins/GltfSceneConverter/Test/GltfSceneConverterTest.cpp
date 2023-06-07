/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/Containers/BitArray.h>
#include <Corrade/Containers/BitArrayView.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Iterable.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StridedBitArrayView.h>
#include <Corrade/Containers/StringIterable.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/File.h>
#include <Corrade/TestSuite/Compare/FileToString.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/FormatStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/ImageView.h>
#include <Magnum/DebugTools/CompareMaterial.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/MaterialTools/Filter.h>
#include <Magnum/MaterialTools/Merge.h>
#include <Magnum/MeshTools/Transform.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/AbstractSceneConverter.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct GltfSceneConverterTest: TestSuite::Tester {
    explicit GltfSceneConverterTest();

    void empty();
    void outputFormatDetectionToData();
    void outputFormatDetectionToFile();
    void metadata();
    void generatorVersion();

    void abort();

    void addMesh();
    void addMeshBufferViewsNonInterleaved();
    void addMeshBufferViewsInterleavedPaddingBegin();
    void addMeshBufferViewsInterleavedPaddingBeginEnd();
    void addMeshBufferViewsMixed();
    void addMeshNoAttributes();
    void addMeshNoIndices();
    void addMeshNoIndicesNoAttributes();
    void addMeshNoIndicesNoVertices();
    void addMeshAttribute();
    void addMeshSkinningAttributes();
    void addMeshSkinningAttributesUnsignedInt();
    void addMeshDuplicateAttribute();
    void addMeshCustomAttributeResetName();
    void addMeshCustomAttributeNoName();
    void addMeshCustomObjectIdAttributeName();
    void addMeshMultiple();
    void addMeshBufferAlignment();
    void addMeshInvalid();

    void addImage2D();
    void addImageCompressed2D();
    void addImage3D();
    void addImageCompressed3D();
    void addImagePropagateFlags();
    void addImagePropagateConfiguration();
    void addImagePropagateConfigurationUnknown();
    void addImagePropagateConfigurationGroup();
    void addImageMultiple();
    /* Multiple 2D + 3D images tested in addMaterial2DArrayTextures() */
    void addImageNoConverterManager();
    void addImageExternalToData();
    void addImageInvalid2D();
    void addImageInvalid3D();

    void addTexture();
    void addTextureMultiple();
    void addTextureDeduplicatedSamplers();
    /* Multiple 2D + 3D textures tested in addMaterial2DArrayTextures() */
    void addTextureInvalid();

    void addMaterial();
    void addMaterial2DArrayTextures();
    template<SceneConverterFlag flag = SceneConverterFlag{}> void addMaterialUnusedAttributes();
    template<SceneConverterFlag flag = SceneConverterFlag{}> void addMaterialCustom();
    void addMaterialMultiple();
    void addMaterialInvalid();
    void addMaterial2DArrayTextureLayerOutOfBounds();

    void textureCoordinateYFlip();

    void addSceneEmpty();
    void addScene();
    void addSceneMeshesMaterials();
    void addSceneCustomFields();
    void addSceneNoParentField();
    void addSceneMultiple();
    void addSceneInvalid();

    void usedRequiredExtensionsAddedAlready();

    void toDataButExternalBuffer();

    /* Needs to load TgaImageConverter from a system-wide location */
    PluginManager::Manager<AbstractImageConverter> _imageConverterManager;
    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractSceneConverter> _converterManager{"nonexistent"};
    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _importerManager;
    /* Original generator name from config before it gets emptied for smaller
       test files */
    Containers::String _originalGeneratorName;
};

using namespace Containers::Literals;
using namespace Math::Literals;

const struct {
    const char* name;
    bool binary;
    Containers::StringView suffix;
} FileVariantData[] {
    {"*.gltf", false, ".gltf"},
    {"*.glb", true, ".glb"}
};

const struct {
    const char* name;
    bool binary, accessorNames;
    Containers::StringView dataName;
    Containers::StringView suffix;
} FileVariantWithNamesData[] {
    {"*.gltf", false, false, {}, ".gltf"},
    {"*.gltf, name", false, false, "This very cool piece of data", "-name.gltf"},
    {"*.gltf, accessor names", false, true, {}, "-accessor-names.gltf"},
    {"*.gltf, name, accessor names", false, true, "A mesh", "-name-accessor-names.gltf"},
    {"*.glb", true, false, {}, ".glb"}
};

const struct {
    const char* name;
    SceneConverterFlags flags;
    bool quiet;
} QuietData[]{
    {"", {}, false},
    {"quiet", SceneConverterFlag::Quiet, true}
};

const struct {
    const char* name;
    SceneConverterFlags flags;
    bool verbose;
} VerboseData[]{
    {"", {}, false},
    {"verbose", SceneConverterFlag::Verbose, true}
};

const struct {
    const char* name;
    bool binary;
    SceneConverterFlags flags;
    Containers::StringView suffix;
    bool quiet;
} FileVariantStrictWarningData[] {
    {"*.gltf", false, {}, ".gltf", false},
    {"*.gltf, quiet", false, SceneConverterFlag::Quiet, ".gltf", true},
    {"*.glb", true, {}, ".glb", false},
    {"*.glb, quiet", true, SceneConverterFlag::Quiet, ".glb", true}
};

const struct {
    const char* name;
    Containers::Optional<bool> binary;
    const char* expected;
} OutputFormatDetectionToDataData[]{
    {"default", {}, "empty.glb"},
    {"binary=false", false, "empty.gltf"},
    {"binary=true", true, "empty.glb"}
};

const struct {
    const char* name;
    Containers::Optional<bool> binary;
    Containers::StringView suffix;
    const char* expected;
} OutputFormatDetectionToFileData[]{
    {".gltf", {}, ".gltf", "empty.gltf"},
    {".gltf + binary=false", false, ".gltf", "empty.gltf"},
    {".gltf + binary=true", true, ".gltf", "empty.glb"},
    {".glb", {}, ".glb", "empty.glb"},
    {".glb + binary=false", false, ".gltf", "empty.gltf"},
    {".glb + binary=true", true, ".gltf", "empty.glb"},
    {"arbitrary extension", {}, ".foo", "empty.glb"}
};

const struct {
    const char* name;
    MeshAttribute attribute;
    VertexFormat format;
    const char* customName;
    SceneConverterFlags flags;
    Containers::Optional<bool> strict;
    Containers::Optional<bool> textureCoordinateYFlipInMaterial;
    bool expectedKhrMeshQuantization;
    const char* expectCustomName;
    const char* expected;
    const char* expectedWarning;
} AddMeshAttributeData[]{
    /* Enumerating various variants for position attribute types in
       order to cover all branches in the type-dependent min/max calculation
       for POSITION accessors. The assumption is that the minmax() call
       itself is fine, just need to ensure that wrong types aren't used by
       accident, leading to asserts. */
    {"positions, quantized, normalized byte", MeshAttribute::Position, VertexFormat::Vector3bNormalized,
        nullptr, {}, {}, {}, true, nullptr,
        "mesh-attribute-position-quantized-b-normalized.gltf",
        nullptr},
    {"positions, quantized, normalized unsigned byte", MeshAttribute::Position, VertexFormat::Vector3ub,
        nullptr, {}, {}, {}, true, nullptr,
        "mesh-attribute-position-quantized-ub.gltf",
        nullptr},
    {"positions, quantized, short", MeshAttribute::Position, VertexFormat::Vector3s,
        nullptr, {}, {}, {}, true, nullptr,
        "mesh-attribute-position-quantized-s.gltf",
        nullptr},
    {"positions, quantized, normalized unsigned short", MeshAttribute::Position, VertexFormat::Vector3usNormalized,
        nullptr, {}, {}, {}, true, nullptr,
        "mesh-attribute-position-quantized-us-normalized.gltf",
        nullptr},
    {"normals, quantized", MeshAttribute::Normal, VertexFormat::Vector3bNormalized,
        nullptr, {}, {}, {}, true, nullptr,
        "mesh-attribute-normal-quantized.gltf",
        nullptr},
    {"tangents", MeshAttribute::Tangent, VertexFormat::Vector4,
        nullptr, {}, {}, {}, false, nullptr,
        "mesh-attribute-tangent.gltf",
        nullptr},
    {"tangents, quantized", MeshAttribute::Tangent, VertexFormat::Vector4sNormalized,
        nullptr, {}, {}, {}, true, nullptr,
        "mesh-attribute-tangent-quantized.gltf",
        nullptr},
    {"three-component tangents", MeshAttribute::Tangent, VertexFormat::Vector3,
        nullptr, {}, {}, {}, false, "_TANGENT3",
        "mesh-attribute-tangent3.gltf",
        "exporting three-component mesh tangents as a custom _TANGENT3 attribute"},
    {"three-component tangents, quiet", MeshAttribute::Tangent, VertexFormat::Vector3,
        nullptr, SceneConverterFlag::Quiet, {}, {}, false, "_TANGENT3",
        "mesh-attribute-tangent3.gltf",
        nullptr},
    {"bitangents", MeshAttribute::Bitangent, VertexFormat::Vector3,
        nullptr, {}, {}, {}, false, "_BITANGENT",
        "mesh-attribute-bitangent.gltf",
        "exporting separate mesh bitangents as a custom _BITANGENT attribute"},
    {"bitangents, quiet", MeshAttribute::Bitangent, VertexFormat::Vector3,
        nullptr, SceneConverterFlag::Quiet, {}, {}, false, "_BITANGENT",
        "mesh-attribute-bitangent.gltf",
        nullptr},
    {"texture coordinates", MeshAttribute::TextureCoordinates, VertexFormat::Vector2,
        nullptr, {}, {}, {}, false, nullptr,
        "mesh-attribute-texture-coordinates.gltf",
        nullptr},
    {"texture coordinates, quantized", MeshAttribute::TextureCoordinates, VertexFormat::Vector2ub,
        nullptr, {}, {}, true, true, nullptr,
        "mesh-attribute-texture-coordinates-quantized.gltf",
        nullptr},
    {"three-component colors", MeshAttribute::Color, VertexFormat::Vector3,
        nullptr, {}, {}, {}, false, nullptr,
        "mesh-attribute-color3.gltf",
        nullptr},
    {"four-component colors", MeshAttribute::Color, VertexFormat::Vector4,
        nullptr, {}, {}, {}, false, nullptr,
        "mesh-attribute-color4.gltf",
        nullptr},
    {"four-component colors, quantized", MeshAttribute::Color, VertexFormat::Vector4usNormalized,
        nullptr, {}, {}, {}, false, nullptr,
        "mesh-attribute-color4us.gltf",
        nullptr},
    {"8-bit object ID", MeshAttribute::ObjectId, VertexFormat::UnsignedByte,
        nullptr, {}, {}, {}, false, nullptr,
        "mesh-attribute-objectidub.gltf",
        nullptr},
    {"32-bit object ID", MeshAttribute::ObjectId, VertexFormat::UnsignedInt,
        nullptr, {}, false, {}, false, nullptr,
        "mesh-attribute-objectidui.gltf",
        "strict mode disabled, allowing a 32-bit integer attribute _OBJECT_ID"},
    {"32-bit object ID, quiet", MeshAttribute::ObjectId, VertexFormat::UnsignedInt,
        nullptr, SceneConverterFlag::Quiet, false, {}, false, nullptr,
        "mesh-attribute-objectidui.gltf",
        nullptr},
    {"2x2 matrix, quantized, aligned", meshAttributeCustom(2123), VertexFormat::Matrix2x2bNormalizedAligned,
        "_ROTATION2D", {}, {}, {}, false, "_ROTATION2D",
        "mesh-attribute-matrix2x2b.gltf",
        nullptr},
    {"3x3 matrix, quantized, aligned", meshAttributeCustom(4564), VertexFormat::Matrix3x3sNormalizedAligned,
        "_TBN", {}, {}, {}, false, "_TBN",
        "mesh-attribute-matrix3x3s.gltf",
        nullptr},
    {"4x4 matrix, quantized", meshAttributeCustom(0), VertexFormat::Matrix4x4bNormalized,
        "_TRANSFORMATION", {}, {}, {}, false, "_TRANSFORMATION",
        "mesh-attribute-matrix4x4b.gltf",
        nullptr}
};

/** @todo drop this once compatibilitySkinningAttributes no longer exists in
    AssimpImporter and GltfImporter */
const struct {
    const char* name;
    bool compatibilityAttributes;
} AddMeshSkinningAttributesData[]{
    {"", false},
    {"with compatibility skinning attributes", true}
};

const UnsignedInt AddMeshInvalidIndices[4]{};
const Vector4d AddMeshInvalidVertices[4]{};
const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    bool strict;
    MeshData mesh;
    const char* message;
} AddMeshInvalidData[]{
    {"unsupported primitive", false,
        MeshData{MeshPrimitive::Instances, 0},
        "unsupported mesh primitive MeshPrimitive::Instances"},
    {"no attributes, non-zero vertex count", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidIndices, MeshIndexData{AddMeshInvalidIndices}, 5},
        "attribute-less mesh with a non-zero vertex count is unrepresentable in glTF"},
    {"no attributes, strict", true,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidIndices,
            MeshIndexData{AddMeshInvalidIndices}, 0},
        "attribute-less meshes are not valid glTF, set strict=false to allow them"},
    {"zero vertices, strict", true,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::Position, VertexFormat::Vector3, Containers::arrayView(AddMeshInvalidVertices).prefix(0)}
        }},
        "meshes with zero vertices are not valid glTF, set strict=false to allow them"},
    {"implementation-specific index type", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidIndices, MeshIndexData{meshIndexTypeWrap(0xcaca), Containers::stridedArrayView(AddMeshInvalidIndices)}, 4},
        "unsupported mesh index type MeshIndexType::ImplementationSpecific(0xcaca)"},
    {"non-contiguous indices", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidIndices, MeshIndexData{Containers::stridedArrayView(AddMeshInvalidIndices).every(2)}, 0},
        "non-contiguous mesh index arrays are not supported"},
    {"half-float positions", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::Position, VertexFormat::Vector3h, AddMeshInvalidVertices}
        }},
        "unsupported mesh position attribute format VertexFormat::Vector3h"},
    {"2D positions", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::Position, VertexFormat::Vector2, AddMeshInvalidVertices}
        }},
        "unsupported mesh position attribute format VertexFormat::Vector2"},
    {"half-float normals", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::Normal, VertexFormat::Vector3h, AddMeshInvalidVertices}
        }},
        "unsupported mesh normal attribute format VertexFormat::Vector3h"},
    {"half-float tangents", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::Tangent, VertexFormat::Vector4h, AddMeshInvalidVertices}
        }},
        "unsupported mesh tangent attribute format VertexFormat::Vector4h"},
    {"half-float texture coordinates", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::TextureCoordinates, VertexFormat::Vector2h, AddMeshInvalidVertices}
        }},
        "unsupported mesh texture coordinate attribute format VertexFormat::Vector2h"},
    {"half-float colors", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::Color, VertexFormat::Vector3h, AddMeshInvalidVertices}
        }},
        "unsupported mesh color attribute format VertexFormat::Vector3h"},
    {"skin joint ID array size not divisible by four", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::JointIds, VertexFormat::UnsignedByte, AddMeshInvalidVertices, 3},
            MeshAttributeData{MeshAttribute::Weights, VertexFormat::UnsignedByteNormalized, AddMeshInvalidVertices, 3},
        }},
        "glTF only supports skin joint IDs with multiples of four elements, got 3"},
    {"skin weight array size not divisible by four", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::Weights, VertexFormat::UnsignedByteNormalized, AddMeshInvalidVertices, 5},
            MeshAttributeData{MeshAttribute::JointIds, VertexFormat::UnsignedByte, AddMeshInvalidVertices, 5},
        }},
        "glTF only supports skin weights with multiples of four elements, got 5"},
    {"32-bit skin joint IDs, strict", true,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::JointIds, VertexFormat::UnsignedInt, AddMeshInvalidVertices, 4},
            MeshAttributeData{MeshAttribute::Weights, VertexFormat::Float, AddMeshInvalidVertices, 4},
        }},
        "mesh attributes with VertexFormat::UnsignedInt are not valid glTF, set strict=false to allow them"},
    {"half-float skin weights", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::JointIds, VertexFormat::UnsignedByte, AddMeshInvalidVertices, 4},
            MeshAttributeData{MeshAttribute::Weights, VertexFormat::Half, AddMeshInvalidVertices, 4},
        }},
        "unsupported mesh skin weights attribute format VertexFormat::Half"},
    {"32-bit object id, strict", true,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::ObjectId, VertexFormat::UnsignedInt, AddMeshInvalidVertices}
        }},
        "mesh attributes with VertexFormat::UnsignedInt are not valid glTF, set strict=false to allow them"},
    {"implementation-specific vertex format", true,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::Position, vertexFormatWrap(0xcaca), AddMeshInvalidVertices}
        }},
        "implementation-specific vertex format 0xcaca can't be exported"},
    {"custom double attribute", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{meshAttributeCustom(31434), VertexFormat::Vector2d, AddMeshInvalidVertices}
        }},
        "unrepresentable mesh vertex format VertexFormat::Vector2d"},
    {"custom non-square matrix attribute", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{meshAttributeCustom(31434), VertexFormat::Matrix3x2, AddMeshInvalidVertices}
        }},
        "unrepresentable mesh vertex format VertexFormat::Matrix3x2"},
    {"custom non-aligned 2x2 byte matrix attribute", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{meshAttributeCustom(31434), VertexFormat::Matrix2x2bNormalized, AddMeshInvalidVertices}
        }},
        "mesh matrix attributes are required to be four-byte-aligned but got VertexFormat::Matrix2x2bNormalized"},
    {"custom non-aligned 3x3 byte  matrix attribute", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{meshAttributeCustom(31434), VertexFormat::Matrix3x3bNormalized, AddMeshInvalidVertices}
        }},
        "mesh matrix attributes are required to be four-byte-aligned but got VertexFormat::Matrix3x3bNormalized"},
    {"custom non-aligned 3x3 short matrix attribute", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{meshAttributeCustom(31434), VertexFormat::Matrix3x3sNormalized, AddMeshInvalidVertices}
        }},
        "mesh matrix attributes are required to be four-byte-aligned but got VertexFormat::Matrix3x3sNormalized"},
    {"custom array attribute", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{meshAttributeCustom(31434), VertexFormat::UnsignedByte, Containers::arrayView(AddMeshInvalidVertices), 7}
        }},
        "unsupported mesh attribute with array size 7"},
    {"zero attribute stride", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::Position, VertexFormat::Vector3, Containers::stridedArrayView(AddMeshInvalidVertices).prefix(1).broadcasted<0>(5)}
        }},
        "unsupported mesh attribute with stride 0"},
    {"negative attribute stride", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::Position, VertexFormat::Vector3, Containers::stridedArrayView(AddMeshInvalidVertices).flipped<0>()}
        }},
        "unsupported mesh attribute with stride -32"},
    {"non-normalized texture coordinates but textureCoordinateYFlipInMaterial not enabled", false,
        MeshData{MeshPrimitive::Points, {}, AddMeshInvalidVertices, {
            MeshAttributeData{MeshAttribute::TextureCoordinates, VertexFormat::Vector2, Containers::arrayView(AddMeshInvalidVertices)},
            /* The first attribute is okay to ensure it's not just the first
               that gets tested */
            MeshAttributeData{MeshAttribute::TextureCoordinates, VertexFormat::Vector2s, Containers::arrayView(AddMeshInvalidVertices)}
        }},
        "non-normalized mesh texture coordinates can't be Y-flipped, enable textureCoordinateYFlipInMaterial for the whole file instead"},
};

const struct {
    const char* name;
    const char* converterPlugin;
    const char* importerPlugin;
    SceneConverterFlags flags;
    bool accessorNames;
    Containers::StringView dataName;
    Containers::Optional<bool> experimentalKhrTextureKtx;
    Containers::Optional<bool> strict;
    Containers::Optional<bool> bundle;
    const char* expected;
    const char* expectedOtherFile;
    bool expectedExtension;
    const char* expectedWarning;
} AddImage2DData[]{
    {"*.gltf", "PngImageConverter", "PngImporter",
        {}, false, {}, {}, {}, {},
        "image.gltf", "image.0.png", false,
        nullptr},
    /* The image (or the buffer) is the same as image.0.png in these three
       variants, not testing its contents */
    {"*.gltf, name", "PngImageConverter", "PngImporter",
        {}, false, "A very pingy image", {}, {}, {},
        "image-name.gltf", nullptr, false,
        nullptr},
    {"*.gltf, bundled, accessor names", "PngImageConverter", "PngImporter",
        {}, true, {}, {}, {}, true,
        "image-accessor-names.gltf", nullptr, false,
        nullptr},
    {"*.gltf, bundled, name, accessor names", "PngImageConverter", "PngImporter",
        {}, true, "A rather pingy image", {}, {}, true,
        "image-name-accessor-names.gltf", nullptr, false,
        nullptr},
    {"*.glb", "PngImageConverter", "PngImporter",
        {}, false, {}, {}, {}, {},
        "image.glb", nullptr, false,
        nullptr},
    {"*.gltf, bundled", "PngImageConverter", "PngImporter",
        {}, false, {}, {}, {}, true,
        "image-bundled.gltf", "image-bundled.bin", false,
        nullptr},
    {"*.glb, not bundled", "PngImageConverter", "PngImporter",
        {}, false, {}, {}, {}, false,
        "image-not-bundled.glb", "image-not-bundled.0.png", false,
        nullptr},
    {"JPEG", "JpegImageConverter", "JpegImporter",
        {}, false, {}, {}, {}, {},
        "image-jpeg.glb", nullptr, false,
        nullptr},
    {"KTX2+Basis", "BasisKtxImageConverter", "BasisImporter",
        {}, false, {}, {}, {}, {},
        "image-basis.glb", nullptr, true,
        nullptr},
    {"KTX2 with extension", "KtxImageConverter", "KtxImporter",
        {}, false, {}, true, {}, {},
        "image-ktx.glb", nullptr, true,
        nullptr},
    {"KTX2 without extension", "KtxImageConverter", "KtxImporter",
        {}, false, {}, {}, false, {},
        "image-ktx-no-extension.glb", nullptr, false,
        "Trade::GltfSceneConverter::add(): KTX2 images can be saved using the KHR_texture_ktx extension, enable experimentalKhrTextureKtx to use it\n"
        "Trade::GltfSceneConverter::add(): strict mode disabled, allowing image/ktx2 MIME type for an image\n"},
    {"KTX2 without extension, quiet", "KtxImageConverter", "KtxImporter",
        SceneConverterFlag::Quiet, false, {}, {}, false, {},
        "image-ktx-no-extension.glb", nullptr, false,
        nullptr},
    /* Explicitly using TGA converter from stb_image to avoid minor differences
       if Magnum's own TgaImageConverter is present as well */
    {"TGA", "StbTgaImageConverter", "TgaImporter",
        {}, false, {}, {}, false, {},
        "image-tga.glb", nullptr, false,
        "Trade::GltfSceneConverter::add(): strict mode disabled, allowing image/x-tga MIME type for an image\n"},
    {"TGA, quiet", "StbTgaImageConverter", "TgaImporter",
        SceneConverterFlag::Quiet, false, {}, {}, false, {},
        "image-tga.glb", nullptr, false,
        nullptr},
};

const struct {
    const char* name;
    Containers::Optional<bool> bundle;
    const char* expected;
    const char* expectedOtherFile;
} AddImage3DData[]{
    {"*.gltf", {},
        "image-3d.gltf", "image-3d.0.ktx2"},
    {"*.glb", {},
        "image-3d.glb", nullptr},
    {"*.gltf, bundled", true,
        "image-3d-bundled.gltf", "image-3d-bundled.bin"},
    {"*.glb, not bundled", false,
        "image-3d-not-bundled.glb", "image-3d-not-bundled.0.ktx2"},
};

const struct {
    const char* name;
    SceneConverterFlags converterFlags;
    ImageFlags2D imageFlags;
    const char* message;
} AddImagePropagateFlagsData[]{
    {"", {}, ImageFlag2D::Array,
        "Trade::GltfSceneConverter::add(): strict mode disabled, allowing image/x-tga MIME type for an image\n"
        "Trade::TgaImageConverter::convertToData(): 1D array images are unrepresentable in TGA, saving as a regular 2D image\n"},
    {"quiet", SceneConverterFlag::Quiet, ImageFlag2D::Array,
        ""},
    {"verbose", SceneConverterFlag::Verbose, {},
        "Trade::GltfSceneConverter::add(): strict mode disabled, allowing image/x-tga MIME type for an image\n"
        "Trade::TgaImageConverter::convertToData(): converting from RGB to BGR\n"}
};

const struct {
    const char* name;
    const char* plugin;
    Containers::StringView suffix;
    ImageData2D image;
    const char* message;
} AddImageInvalid2DData[]{
    {"can't load plugin", "WhatImageConverter", ".gltf",
        ImageData2D{PixelFormat::RGBA8Unorm, {1, 1}, DataFlags{}, "abc"},
        #ifdef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
        "PluginManager::Manager::load(): plugin WhatImageConverter was not found\n"
        #else
        "PluginManager::Manager::load(): plugin WhatImageConverter is not static and was not found in nonexistent\n"
        #endif
        "Trade::GltfSceneConverter::add(): can't load WhatImageConverter for image conversion\n"},
    {"plugin without file conversion", "StbDxtImageConverter", ".gltf",
        ImageData2D{PixelFormat::RGBA8Unorm, {1, 1}, DataFlags{}, "abc"},
        "StbDxtImageConverter doesn't support Trade::ImageConverterFeature::Convert2DToFile"},
    {"plugin without compressed data conversion", "PngImageConverter", ".glb",
        ImageData2D{CompressedPixelFormat::Astc4x4RGBAUnorm, {1, 1}, DataFlags{}, "abc"},
        "PngImageConverter doesn't support Trade::ImageConverterFeature::ConvertCompressed2DToData"},
    {"plugin without a MIME type", "StbImageConverter", ".gltf",
        ImageData2D{PixelFormat::RGBA8Unorm, {1, 1}, DataFlags{}, "abc"},
        "StbImageConverter doesn't specify any MIME type, can't save an image"},
    {"TGA, strict", "TgaImageConverter", ".gltf",
        ImageData2D{PixelFormat::RGBA8Unorm, {1, 1}, DataFlags{}, "abc"},
        "image/x-tga is not a valid MIME type for a glTF image, set strict=false to allow it"},
    {"conversion to file failed", "PngImageConverter", ".gltf",
        ImageData2D{PixelFormat::R32F, {1, 1}, DataFlags{}, "abc"},
        "Trade::StbImageConverter::convertToData(): PixelFormat::R32F is not supported for BMP/JPEG/PNG/TGA output\n"
        "Trade::GltfSceneConverter::add(): can't convert an image file\n"},
    {"conversion to data failed", "PngImageConverter", ".glb",
        ImageData2D{PixelFormat::R32F, {1, 1}, DataFlags{}, "abc"},
        "Trade::StbImageConverter::convertToData(): PixelFormat::R32F is not supported for BMP/JPEG/PNG/TGA output\n"
        "Trade::GltfSceneConverter::add(): can't convert an image\n"},
    /* This tests that an extension isn't accidentally added even after a
       failure */
    {"conversion failed for a format that needs an extension", "BasisKtxImageConverter", ".gltf",
        ImageData2D{PixelFormat::RG16Unorm, {1, 1}, DataFlags{}, "abc"},
        "Trade::BasisImageConverter::convertToData(): unsupported format PixelFormat::RG16Unorm\n"
        "Trade::GltfSceneConverter::add(): can't convert an image file\n"},
};

const struct {
    const char* name;
    const char* plugin;
    Containers::StringView suffix;
    ImageData3D image;
    const char* message;
} AddImageInvalid3DData[]{
    /* Plugin load failure not tested as that's the same code path as in the
       2D case and the same failure return as the feature checks below */
    {"plugin without data conversion", "StbDxtImageConverter", ".glb",
        ImageData3D{PixelFormat::RGBA8Unorm, {1, 1, 1}, DataFlags{}, "abc", ImageFlag3D::Array},
        "StbDxtImageConverter doesn't support Trade::ImageConverterFeature::Convert3DToData"},
    {"plugin without compressed file conversion", "BasisKtxImageConverter", ".gltf",
        ImageData3D{CompressedPixelFormat::Astc4x4RGBAUnorm, {1, 1, 1}, DataFlags{}, "abc", ImageFlag3D::Array},
        "BasisKtxImageConverter doesn't support Trade::ImageConverterFeature::ConvertCompressed3DToFile"},
    {"plugin without a MIME type", "BasisImageConverter", ".gltf",
        ImageData3D{PixelFormat::RGBA8Unorm, {1, 1, 1}, DataFlags{}, "abc", ImageFlag3D::Array},
        "BasisImageConverter doesn't specify any MIME type, can't save an image"},
    {"invalid MIME type", "OpenExrImageConverter", ".gltf",
        ImageData3D{PixelFormat::RG16F, {1, 1, 1}, DataFlags{}, "abc", ImageFlag3D::Array},
        "image/x-exr is not a valid MIME type for a 3D glTF image"},
    /* Also tests that an extension isn't accidentally added even after a
       failure */
    {"conversion to file failed", "BasisKtxImageConverter", ".gltf",
        ImageData3D{PixelFormat::R32F, {1, 1, 1}, DataFlags{}, "abc", ImageFlag3D::Array},
        "Trade::BasisImageConverter::convertToData(): unsupported format PixelFormat::R32F\n"
        "Trade::GltfSceneConverter::add(): can't convert an image file\n"},
    /* Not testing failed conversion to data as that's the same code path as in
       the 2D case and the same failure return as the file check above */
    {"not an array", "KtxImageConverter", ".gltf",
        ImageData3D{PixelFormat::R32F, {1, 1, 1}, DataFlags{}, "abc"},
        "expected a 2D array image but got ImageFlags3D{}"},
    {"cube map", "KtxImageConverter", ".gltf",
        ImageData3D{PixelStorage{}.setAlignment(1), PixelFormat::R8Unorm, {1, 1, 6}, DataFlags{}, "abcde", ImageFlag3D::CubeMap},
        "expected a 2D array image but got ImageFlag3D::CubeMap"},
};

const struct {
    const char* name;
    const char* converterPlugin;
    Containers::StringView dataName;
    Containers::Optional<bool> experimentalKhrTextureKtx;
    Containers::Optional<bool> strict;
    const char* expected;
} AddTextureData[]{
    {"", "PngImageConverter",
        {}, {}, {},
        "texture.gltf"},
    /* The image (or the buffer) is the same as image.0.png in these three
       variants, not testing its contents */
    {"name", "PngImageConverter",
        "A texty name for a pingy image", {}, {},
        "texture-name.gltf"},
    {"JPEG", "JpegImageConverter",
        {}, {}, {},
        "texture-jpeg.gltf"},
    {"KTX2+Basis", "BasisKtxImageConverter",
        {}, {}, {},
        "texture-basis.gltf"},
    {"KTX2 with extension", "KtxImageConverter",
        {}, true, {},
        "texture-ktx.gltf"},
    {"KTX2 without extension", "KtxImageConverter",
        {}, {}, false,
        "texture-ktx-no-extension.gltf"},
    {"TGA", "TgaImageConverter",
        {}, {}, false,
        "texture-tga.gltf"},
};

const struct {
    const char* name;
    Containers::Optional<bool> experimentalKhrTextureKtx;
    const char* expected;
    TextureData texture;
    const char* message;
} AddTextureInvalidData[]{
    {"2D image out of range", {}, "image.gltf",
        TextureData{TextureType::Texture2D,
            SamplerFilter::Nearest,
            SamplerFilter::Nearest,
            SamplerMipmap::Base,
            SamplerWrapping::ClampToEdge,
            1},
        "texture references 2D image 1 but only 1 were added so far"},
    {"3D image out of range", true, "image-3d-no-texture.gltf",
        TextureData{TextureType::Texture2DArray,
            SamplerFilter::Nearest,
            SamplerFilter::Nearest,
            SamplerMipmap::Base,
            SamplerWrapping::ClampToEdge,
            1},
        "texture references 3D image 1 but only 1 were added so far"},
    {"2D array but no experimentalKhrTextureKtx", false, "image-3d-no-texture.gltf",
        TextureData{TextureType::Texture2DArray,
            SamplerFilter::Nearest,
            SamplerFilter::Nearest,
            SamplerMipmap::Base,
            SamplerWrapping::ClampToEdge,
            0},
        "2D array textures require experimentalKhrTextureKtx to be enabled"},
    {"invalid type", {}, "empty.gltf",
        TextureData{TextureType::Texture1DArray,
            SamplerFilter::Nearest,
            SamplerFilter::Nearest,
            SamplerMipmap::Base,
            SamplerWrapping::ClampToEdge,
            0},
        "expected a 2D or 2D array texture, got Trade::TextureType::Texture1DArray"},
    {"unsupported sampler wrapping", {}, "image.gltf",
        TextureData{TextureType::Texture2D,
            SamplerFilter::Nearest,
            SamplerFilter::Nearest,
            SamplerMipmap::Base,
            SamplerWrapping::ClampToBorder,
            0},
        "unsupported texture wrapping SamplerWrapping::ClampToBorder"},
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    bool needsTexture;
    Containers::Optional<bool> keepDefaults;
    const char* expected;
    Containers::StringView dataName;
    MaterialData material;
    Containers::Array<Containers::Pair<UnsignedInt, MaterialAttribute>> expectedRemove;
    Containers::Optional<MaterialData> expectedAdd;
} AddMaterialData[]{
    {"empty", false, {}, "material-empty.gltf", {},
        MaterialData{{}, {}}, {}, {}},
    {"name", false, {}, "material-name.gltf", "A nicely useless material",
        MaterialData{{}, {}}, {}, {}},
    {"common", true, {}, "material-common.gltf", {},
        MaterialData{{}, {
            /* More than one texture tested in addMaterialMultiple() */
            {MaterialAttribute::AlphaMask, 0.75f}, /* unused */
            {MaterialAttribute::AlphaBlend, true},
            {MaterialAttribute::DoubleSided, true},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 0.375f},
            {MaterialAttribute::NormalTextureMatrix,
                Matrix3::translation({0.5f, 0.5f})},
            {MaterialAttribute::NormalTextureCoordinates, 7u},
            {MaterialAttribute::NormalTextureLayer, 0u}, /* unused */
            {MaterialAttribute::OcclusionTexture, 0u},
            {MaterialAttribute::OcclusionTextureStrength, 1.5f},
            {MaterialAttribute::OcclusionTextureMatrix,
                Matrix3::scaling({1.0f, -1.0f})},
            {MaterialAttribute::OcclusionTextureCoordinates, 8u},
            {MaterialAttribute::OcclusionTextureLayer, 0u}, /* unused */
            {MaterialAttribute::EmissiveColor, Color3{0.5f, 0.6f, 0.7f}},
            {MaterialAttribute::EmissiveTexture, 0u},
            {MaterialAttribute::EmissiveTextureMatrix,
                Matrix3::translation({0.75f, 1.0f})*
                Matrix3::scaling({0.25f, -0.125f})},
            {MaterialAttribute::EmissiveTextureCoordinates, 9u},
            {MaterialAttribute::EmissiveTextureLayer, 0u}, /* unused */
        }}, {InPlaceInit, {
            {0, MaterialAttribute::AlphaMask},
            {0, MaterialAttribute::NormalTextureLayer},
            {0, MaterialAttribute::OcclusionTextureLayer},
            {0, MaterialAttribute::EmissiveTextureLayer}
        }}, {}},
    {"alpha mask", false, {}, "material-alpha-mask.gltf", {},
        MaterialData{{}, {
            {MaterialAttribute::AlphaMask, 0.75f},
        }}, {}, {}},
    {"metallic/roughness", true, {}, "material-metallicroughness.gltf", {},
        MaterialData{{}, {
            {MaterialAttribute::BaseColor, Color4{0.1f, 0.2f, 0.3f, 0.4f}},
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::BaseColorTextureMatrix,
                Matrix3::translation({0.25f, 1.0f})},
            {MaterialAttribute::BaseColorTextureCoordinates, 10u},
            {MaterialAttribute::BaseColorTextureLayer, 0u}, /* unused */
            /* The Swizzle and Coordinates have to be set like this to make
               this a packed texture like glTF wants */
            {MaterialAttribute::Metalness, 0.25f},
            {MaterialAttribute::Roughness, 0.75f},
            {MaterialAttribute::MetalnessTexture, 0u},
            {MaterialAttribute::MetalnessTextureSwizzle, MaterialTextureSwizzle::B},
            {MaterialAttribute::MetalnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::MetalnessTextureCoordinates, 11u},
            {MaterialAttribute::MetalnessTextureLayer, 0u}, /* unused */
            {MaterialAttribute::RoughnessTexture, 0u},
            {MaterialAttribute::RoughnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
            {MaterialAttribute::RoughnessTextureCoordinates, 11u},
            {MaterialAttribute::RoughnessTextureLayer, 0u}, /* unused */
        }}, {InPlaceInit, {
            {0, MaterialAttribute::BaseColorTextureLayer},
            {0, MaterialAttribute::MetalnessTexture},
            {0, MaterialAttribute::MetalnessTextureSwizzle},
            {0, MaterialAttribute::MetalnessTextureLayer},
            {0, MaterialAttribute::RoughnessTexture},
            {0, MaterialAttribute::RoughnessTextureSwizzle},
            {0, MaterialAttribute::RoughnessTextureLayer}
        }}, MaterialData{MaterialType::PbrMetallicRoughness, {
            MaterialAttributeData{MaterialAttribute::NoneRoughnessMetallicTexture, 0u}
        }}},
    {"metallic/roughness, packed texture attribute", true, {}, "material-metallicroughness.gltf", {},
        MaterialData{{}, {
            {MaterialAttribute::BaseColor, Color4{0.1f, 0.2f, 0.3f, 0.4f}},
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::BaseColorTextureMatrix,
                Matrix3::translation({0.25f, 1.0f})},
            {MaterialAttribute::BaseColorTextureCoordinates, 10u},
            {MaterialAttribute::BaseColorTextureLayer, 0u}, /* unused */
            {MaterialAttribute::Metalness, 0.25f},
            {MaterialAttribute::Roughness, 0.75f},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 0u},
            {MaterialAttribute::MetalnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::MetalnessTextureCoordinates, 11u},
            {MaterialAttribute::MetalnessTextureLayer, 0u}, /* unused */
            {MaterialAttribute::RoughnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::RoughnessTextureCoordinates, 11u},
            {MaterialAttribute::RoughnessTextureLayer, 0u}, /* unused */
        }}, {InPlaceInit, {
            {0, MaterialAttribute::BaseColorTextureLayer},
            {0, MaterialAttribute::MetalnessTextureLayer},
            {0, MaterialAttribute::RoughnessTextureLayer}
        }}, MaterialData{MaterialType::PbrMetallicRoughness, {}}},
    {"metallic/roughness, global texture attributes", true, {}, "material-metallicroughness.gltf", {},
        MaterialData{{}, {
            {MaterialAttribute::BaseColor, Color4{0.1f, 0.2f, 0.3f, 0.4f}},
            {MaterialAttribute::BaseColorTexture, 0u},
            /* This one is local, thus overriding the TextureMatrix */
            {MaterialAttribute::BaseColorTextureMatrix,
                Matrix3::translation({0.25f, 1.0f})},
            /* This one is local, thus overriding the TextureCoordinates */
            {MaterialAttribute::BaseColorTextureCoordinates, 10u},
            /* The Swizzle has to be set like this to make this a packed
               texture like glTF wants */
            {MaterialAttribute::Metalness, 0.25f},
            {MaterialAttribute::Roughness, 0.75f},
            {MaterialAttribute::MetalnessTexture, 0u},
            {MaterialAttribute::MetalnessTextureSwizzle, MaterialTextureSwizzle::B},
            {MaterialAttribute::RoughnessTexture, 0u},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
            {MaterialAttribute::TextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::TextureCoordinates, 11u},
            {MaterialAttribute::TextureLayer, 0u}, /* unused */
        }}, {InPlaceInit, {
            {0, MaterialAttribute::MetalnessTexture},
            {0, MaterialAttribute::MetalnessTextureSwizzle},
            {0, MaterialAttribute::RoughnessTextureSwizzle},
            {0, MaterialAttribute::RoughnessTexture},
            {0, MaterialAttribute::TextureMatrix},
            {0, MaterialAttribute::TextureCoordinates},
            {0, MaterialAttribute::TextureLayer},
        }}, MaterialData{MaterialType::PbrMetallicRoughness, {
            MaterialAttributeData{MaterialAttribute::NoneRoughnessMetallicTexture, 0u},
            MaterialAttributeData{MaterialAttribute::MetalnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            MaterialAttributeData{MaterialAttribute::MetalnessTextureCoordinates, 11u},
            MaterialAttributeData{MaterialAttribute::RoughnessTextureCoordinates, 11u},
            MaterialAttributeData{MaterialAttribute::RoughnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
        }}},
    {"explicit default texture swizzle", true, {}, "material-default-texture-swizzle.gltf", {},
        MaterialData{{}, {
            /* The swizzles are just checked but not written anywhere, so this
               is the same as specifying just the textures alone, and it
               shouldn't produce any warning. */
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureSwizzle, MaterialTextureSwizzle::RGB},
            {MaterialAttribute::OcclusionTexture, 0u},
            {MaterialAttribute::OcclusionTextureSwizzle, MaterialTextureSwizzle::R},
            /* No EmissiveTextureSwizzle or BaseColorTextureSwizzle attributes,
               Metallic and Roughness textures won't work with defaults */
        }}, {InPlaceInit, {
            {0, MaterialAttribute::NormalTextureSwizzle},
            {0, MaterialAttribute::OcclusionTextureSwizzle}
        }}, {}},
    {"default values kept", true, true, "material-defaults-kept.gltf", {},
        MaterialData{{}, {
            /* Textures have to be present, otherwise the texture-related
               properties are not saved */
            {MaterialAttribute::AlphaBlend, false},
            {MaterialAttribute::DoubleSided, false},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 1.0f},
            {MaterialAttribute::NormalTextureMatrix, Matrix3{}},
            {MaterialAttribute::NormalTextureCoordinates, 0u},
            {MaterialAttribute::OcclusionTexture, 0u},
            {MaterialAttribute::OcclusionTextureStrength, 1.0f},
            {MaterialAttribute::OcclusionTextureMatrix, Matrix3{}},
            {MaterialAttribute::OcclusionTextureCoordinates, 0u},
            {MaterialAttribute::EmissiveColor, 0x000000_rgbf},
            {MaterialAttribute::EmissiveTexture, 0u},
            {MaterialAttribute::EmissiveTextureMatrix, Matrix3{}},
            {MaterialAttribute::EmissiveTextureCoordinates, 0u},
            {MaterialAttribute::BaseColor, 0xffffffff_rgbaf},
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::BaseColorTextureMatrix, Matrix3{}},
            {MaterialAttribute::BaseColorTextureCoordinates, 0u},
            {MaterialAttribute::Metalness, 1.0f},
            {MaterialAttribute::Roughness, 1.0f},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 0u},
            {MaterialAttribute::MetalnessTextureMatrix, Matrix3{}},
            {MaterialAttribute::MetalnessTextureCoordinates, 0u},
            {MaterialAttribute::RoughnessTextureMatrix, Matrix3{}},
            {MaterialAttribute::RoughnessTextureCoordinates, 0u},
        }}, {}, MaterialData{MaterialType::PbrMetallicRoughness, {}}},
    {"default values omitted", true, {}, "material-defaults-omitted.gltf", {},
        MaterialData{{}, {
            /* Same as above */
            {MaterialAttribute::AlphaBlend, false},
            {MaterialAttribute::DoubleSided, false},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 1.0f},
            {MaterialAttribute::NormalTextureMatrix, Matrix3{}},
            {MaterialAttribute::NormalTextureCoordinates, 0u},
            {MaterialAttribute::OcclusionTexture, 0u},
            {MaterialAttribute::OcclusionTextureStrength, 1.0f},
            {MaterialAttribute::OcclusionTextureMatrix, Matrix3{}},
            {MaterialAttribute::OcclusionTextureCoordinates, 0u},
            {MaterialAttribute::EmissiveColor, 0x000000_rgbf},
            {MaterialAttribute::EmissiveTexture, 0u},
            {MaterialAttribute::EmissiveTextureMatrix, Matrix3{}},
            {MaterialAttribute::EmissiveTextureCoordinates, 0u},
            {MaterialAttribute::BaseColor, 0xffffffff_rgbaf},
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::BaseColorTextureMatrix, Matrix3{}},
            {MaterialAttribute::BaseColorTextureCoordinates, 0u},
            {MaterialAttribute::Metalness, 1.0f},
            {MaterialAttribute::Roughness, 1.0f},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 0u},
            {MaterialAttribute::MetalnessTextureMatrix, Matrix3{}},
            {MaterialAttribute::MetalnessTextureCoordinates, 0u},
            {MaterialAttribute::RoughnessTextureMatrix, Matrix3{}},
            {MaterialAttribute::RoughnessTextureCoordinates, 0u},
        }}, {InPlaceInit, {
            {0, MaterialAttribute::AlphaBlend},
            {0, MaterialAttribute::DoubleSided},
            {0, MaterialAttribute::NormalTextureScale},
            {0, MaterialAttribute::NormalTextureMatrix},
            {0, MaterialAttribute::NormalTextureCoordinates},
            {0, MaterialAttribute::OcclusionTextureStrength},
            {0, MaterialAttribute::OcclusionTextureMatrix},
            {0, MaterialAttribute::OcclusionTextureCoordinates},
            {0, MaterialAttribute::EmissiveColor},
            {0, MaterialAttribute::EmissiveTextureMatrix},
            {0, MaterialAttribute::EmissiveTextureCoordinates},
            {0, MaterialAttribute::BaseColor},
            {0, MaterialAttribute::BaseColorTextureMatrix},
            {0, MaterialAttribute::BaseColorTextureCoordinates},
            {0, MaterialAttribute::Metalness},
            {0, MaterialAttribute::Roughness},
            {0, MaterialAttribute::MetalnessTextureMatrix},
            {0, MaterialAttribute::MetalnessTextureCoordinates},
            {0, MaterialAttribute::RoughnessTextureMatrix},
            {0, MaterialAttribute::RoughnessTextureCoordinates},
        }}, MaterialData{MaterialType::PbrMetallicRoughness, {}}},
    {"alpha mask default values kept", false, true, "material-alpha-mask-defaults-kept.gltf", {},
        MaterialData{{}, {
            {MaterialAttribute::AlphaMask, 0.5f},
        }}, {}, {}},
    {"alpha mask default values omitted", false, {}, "material-alpha-mask-defaults-omitted.gltf", {},
        MaterialData{{}, {
            /* Same as above */
            {MaterialAttribute::AlphaMask, 0.5f},
        }}, {}, {}},
    {"unlit", false, {}, "material-unlit.gltf", {},
        /* PbrMetallicRoughness should not get added on import, only Flat */
        MaterialData{MaterialType::Flat, {
            {MaterialAttribute::BaseColor, Color4{0.1f, 0.2f, 0.3f, 0.4f}},
            /* To avoid data loss, non-flat properties are still written, even
               though they make no sense for a flat-shaded material */
            {MaterialAttribute::Roughness, 0.57f}
        }}, {}, MaterialData{MaterialType::Flat, {}}},
    {"clear coat", true, {}, "material-clearcoat.gltf", {},
        MaterialData{{}, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.7f},
            {MaterialAttribute::LayerFactorTexture, 0u},
            {MaterialAttribute::LayerFactorTextureMatrix,
                Matrix3::translation({0.25f, 1.0f})},
            {MaterialAttribute::LayerFactorTextureCoordinates, 1u},
            {MaterialAttribute::LayerFactorTextureLayer, 0u}, /* unused */
            {MaterialAttribute::Roughness, 0.8f},
            {MaterialAttribute::RoughnessTexture, 0u},
            {MaterialAttribute::RoughnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
            {MaterialAttribute::RoughnessTextureCoordinates, 2u},
            {MaterialAttribute::RoughnessTextureLayer, 0u}, /* unused */
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 0.75f},
            {MaterialAttribute::NormalTextureMatrix,
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::NormalTextureCoordinates, 3u},
            {MaterialAttribute::NormalTextureLayer, 0u}, /* unused */
        }, {0, 17}}, {InPlaceInit, {
            {1, MaterialAttribute::LayerFactorTextureLayer},
            {1, MaterialAttribute::RoughnessTextureLayer},
            {1, MaterialAttribute::NormalTextureLayer}
        }}, MaterialData{MaterialType::PbrClearCoat, {}}},
    {"clear coat, layer-global texture attributes", true, {}, "material-clearcoat.gltf", {},
        /* Priority between global, layer-local and local attributes (and
           messages produced due to that) is tested in a corresponding case in
           addMaterialUnusedAttributes() */
        MaterialData{{}, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.7f},
            {MaterialAttribute::LayerFactorTexture, 0u},
            /* This one is local, this overriding the TextureMatrix in the
               layer */
            {MaterialAttribute::LayerFactorTextureMatrix,
                Matrix3::translation({0.25f, 1.0f})},
            /* This one is local, thus overriding the TextureCoordinates in the
               layer */
            {MaterialAttribute::LayerFactorTextureCoordinates, 1u},
            {MaterialAttribute::LayerFactorTextureLayer, 0u}, /* unused */
            {MaterialAttribute::Roughness, 0.8f},
            {MaterialAttribute::RoughnessTexture, 0u},
            /* This one is again local */
            {MaterialAttribute::RoughnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
            {MaterialAttribute::TextureCoordinates, 2u},
            {MaterialAttribute::RoughnessTextureLayer, 0u}, /* unused */
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 0.75f},
            {MaterialAttribute::TextureMatrix,
                Matrix3::scaling({-0.25f, 0.75f})},
            /* This one is again local */
            {MaterialAttribute::NormalTextureCoordinates, 3u},
            {MaterialAttribute::NormalTextureLayer, 0u}, /* unused */
        }, {0, 17}}, {InPlaceInit, {
            {1, MaterialAttribute::LayerFactorTextureLayer},
            {1, MaterialAttribute::RoughnessTextureLayer},
            {1, MaterialAttribute::NormalTextureLayer},
            /* THese two get replaced by local attributes */
            {1, MaterialAttribute::TextureCoordinates},
            {1, MaterialAttribute::TextureMatrix},
        }}, MaterialData{MaterialType::PbrClearCoat, {
            {MaterialAttribute::NormalTextureMatrix,
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::RoughnessTextureCoordinates, 2u}
        }, {0, 2}}},
    {"clear coat, global texture attributes", true, {}, "material-clearcoat.gltf", {},
        MaterialData{{}, {
            {MaterialAttribute::TextureMatrix,
                Matrix3::translation({0.25f, 1.0f})},
            {MaterialAttribute::TextureCoordinates, 2u},
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.7f},
            {MaterialAttribute::LayerFactorTexture, 0u},
            {MaterialAttribute::LayerFactorTextureLayer, 0u}, /* unused */
            /* This one is local, thus overriding the global
               TextureCoordinates */
            {MaterialAttribute::LayerFactorTextureCoordinates, 1u},
            {MaterialAttribute::Roughness, 0.8f},
            {MaterialAttribute::RoughnessTexture, 0u},
            /* This one is local, this overriding the global TextureMatrix */
            {MaterialAttribute::RoughnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
            {MaterialAttribute::RoughnessTextureLayer, 0u}, /* unused */
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 0.75f},
            /* This one is again local */
            {MaterialAttribute::NormalTextureMatrix,
                Matrix3::scaling({-0.25f, 0.75f})},
            /* This one is again local */
            {MaterialAttribute::NormalTextureCoordinates, 3u},
            {MaterialAttribute::NormalTextureLayer, 0u}, /* unused */
        }, {2, 17}}, {InPlaceInit, {
            {0, MaterialAttribute::TextureMatrix},
            {0, MaterialAttribute::TextureCoordinates},
            {1, MaterialAttribute::LayerFactorTextureLayer},
            {1, MaterialAttribute::RoughnessTextureLayer},
            {1, MaterialAttribute::NormalTextureLayer},
        }}, MaterialData{MaterialType::PbrClearCoat, {
            {MaterialAttribute::LayerFactorTextureMatrix,
                Matrix3::translation({0.25f, 1.0f})},
            {MaterialAttribute::RoughnessTextureCoordinates, 2u},
        }, {0, 2}}},
    {"clear coat, explicit default texture swizzle", true, {}, "material-clearcoat-default-texture-swizzle.gltf", {},
        MaterialData{{}, {
            /* The swizzles are just checked but not written anywhere, so this
               is the same as specifying just the textures alone, and it
               shouldn't produce any warning. */
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.0f},
            {MaterialAttribute::LayerFactorTexture, 0u},
            {MaterialAttribute::LayerFactorTextureSwizzle, MaterialTextureSwizzle::R},
            {MaterialAttribute::Roughness, 0.0f},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureSwizzle, MaterialTextureSwizzle::RGB},
            /* The Roughness texture won't work with the default */
        }, {0, 7}}, {InPlaceInit, {
            {1, MaterialAttribute::LayerFactorTextureSwizzle},
            {1, MaterialAttribute::NormalTextureSwizzle}
        }}, MaterialData{MaterialType::PbrClearCoat, {}}},
    {"clear coat, Magnum defaults", false, {}, "material-clearcoat-magnum-defaults.gltf", {},
        MaterialData{{}, {
            {MaterialLayer::ClearCoat},
            /* The glTF should have factor and roughness set to 1, and the
               imported material as well as the importer doesn't do any
               explicit "defaults cleanup" */
        }, {0, 1}}, {}, MaterialData{MaterialType::PbrClearCoat, {
            {MaterialAttribute::LayerFactor, 1.0f},
            {MaterialAttribute::Roughness, 1.0f},
        }, {0, 2}}},
    {"clear coat, glTF defaults kept", true, true, "material-clearcoat-gltf-defaults-kept.gltf", {},
        MaterialData{{}, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.0f},
            {MaterialAttribute::Roughness, 0.0f},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 1.0f},
        }, {0, 5}}, {}, MaterialData{MaterialType::PbrClearCoat, {}}},
    {"clear coat, glTF defaults omitted", true, {}, "material-clearcoat-gltf-defaults-omitted.gltf", {},
        MaterialData{{}, {
            /* Same as above */
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.0f},
            {MaterialAttribute::Roughness, 0.0f},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 1.0f},
        }, {0, 5}}, {InPlaceInit, {
            {1, MaterialAttribute::NormalTextureScale}
        }}, MaterialData{MaterialType::PbrClearCoat, {}}},
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    bool needsTexture;
    const char* expected;
    MaterialData material;
    const char* expectedWarning;
} AddMaterialUnusedAttributesData[]{
    {"texture properties but no textures", false, "material-empty.gltf",
        MaterialData{{}, {
            /* Sorted, because the warnings are also sorted */
            {MaterialAttribute::BaseColorTextureCoordinates, 5u},
            {MaterialAttribute::BaseColorTextureLayer, 0u},
            {MaterialAttribute::BaseColorTextureMatrix, Matrix3{2.0f}},
            {MaterialAttribute::EmissiveTextureCoordinates, 10u},
            {MaterialAttribute::EmissiveTextureLayer, 0u},
            {MaterialAttribute::EmissiveTextureMatrix, Matrix3{2.0f}},
            {MaterialAttribute::MetalnessTextureCoordinates, 6u},
            {MaterialAttribute::MetalnessTextureLayer, 0u},
            {MaterialAttribute::MetalnessTextureMatrix, Matrix3{2.0f}},
            {MaterialAttribute::NormalTextureCoordinates, 8u},
            {MaterialAttribute::NormalTextureLayer, 0u},
            {MaterialAttribute::NormalTextureMatrix, Matrix3{2.0f}},
            {MaterialAttribute::NormalTextureScale, 1.5f},
            {MaterialAttribute::OcclusionTextureCoordinates, 9u},
            {MaterialAttribute::OcclusionTextureLayer, 0u},
            {MaterialAttribute::OcclusionTextureMatrix, Matrix3{2.0f}},
            {MaterialAttribute::OcclusionTextureStrength, 0.3f},
            {MaterialAttribute::RoughnessTextureCoordinates, 7u},
            {MaterialAttribute::RoughnessTextureLayer, 0u},
            {MaterialAttribute::RoughnessTextureMatrix, Matrix3{2.0f}},
        }},
        "Trade::GltfSceneConverter::add(): material attribute BaseColorTextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute BaseColorTextureLayer was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute BaseColorTextureMatrix was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute EmissiveTextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute EmissiveTextureLayer was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute EmissiveTextureMatrix was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute MetalnessTextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute MetalnessTextureLayer was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute MetalnessTextureMatrix was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute NormalTextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute NormalTextureLayer was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute NormalTextureMatrix was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute NormalTextureScale was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute OcclusionTextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute OcclusionTextureLayer was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute OcclusionTextureMatrix was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute OcclusionTextureStrength was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute RoughnessTextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute RoughnessTextureLayer was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute RoughnessTextureMatrix was not used\n"},
    {"unused attributes and layers", true, "material-unused-attributes-layers.gltf",
        MaterialData{{}, {
            {MaterialAttribute::EmissiveTexture, 0u},
            {MaterialAttribute::EmissiveTextureMatrix,
                Matrix3::translation({1.0f, 2.0f})*Matrix3::rotation(-35.0_degf)},
            {MaterialAttribute::DiffuseColor, 0xff6633aa_rgbaf},
            {MaterialAttribute::Shininess, 15.0f},
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.0f}, /* glTF default, omitted */
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureMatrix,
                Matrix3::rotation(-35.0_degf)},
            {MaterialAttribute::Roughness, 0.0f}, /* glTF default, omitted */
            {MaterialAttribute::SpecularColor, 0xffffff00_rgbaf},
            {MaterialAttribute::LayerName, "ThinFilm"},
            {MaterialAttribute::LayerFactor, 0.5f},
        }, {4, 10, 11, 12}},
        "Trade::GltfSceneConverter::add(): material attribute EmissiveTextureMatrix rotation was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute NormalTextureMatrix in layer 1 (ClearCoat) rotation was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute DiffuseColor was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute Shininess was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute SpecularColor in layer 1 (ClearCoat) was not used\n"
        /* It especially shouldn't warn about unused attribute LayerName */
        "Trade::GltfSceneConverter::add(): material layer 2 (ThinFilm) was not used\n"
        "Trade::GltfSceneConverter::add(): material layer 3 was not used\n"},
    {"clear coat, layer-local/local texture attribute priority", true, "material-clearcoat.gltf",
        MaterialData{{}, {
            /* There's a layer-local TextureMatrix, TextureCoordinates and
               TextureLayer, so these will stay unused */
            {MaterialAttribute::TextureMatrix,
                Matrix3::scaling({0.75f, -0.25f})},
            {MaterialAttribute::TextureCoordinates, 17u},
            {MaterialAttribute::TextureLayer, 33u},
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.7f},
            {MaterialAttribute::LayerFactorTexture, 0u},
            /* This one is local, this overriding the TextureMatrix in the
               layer */
            {MaterialAttribute::LayerFactorTextureMatrix,
                Matrix3::translation({0.25f, 1.0f})},
            /* This one is local, thus overriding the TextureCoordinates in the
               layer */
            {MaterialAttribute::LayerFactorTextureCoordinates, 1u},
            {MaterialAttribute::LayerFactorTextureLayer, 0u}, /* unused */
            {MaterialAttribute::Roughness, 0.8f},
            {MaterialAttribute::RoughnessTexture, 0u},
            /* This one is again local */
            {MaterialAttribute::RoughnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
            {MaterialAttribute::TextureCoordinates, 2u},
            {MaterialAttribute::RoughnessTextureLayer, 0u}, /* unused */
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 0.75f},
            {MaterialAttribute::TextureMatrix,
                Matrix3::scaling({-0.25f, 0.75f})},
            /* This one is again local */
            {MaterialAttribute::NormalTextureCoordinates, 3u},
            {MaterialAttribute::NormalTextureLayer, 0u},
            /* There are all local layers so this one stays unused as well */
            {MaterialAttribute::TextureLayer, 71u},
        }, {3, 21}},
        "Trade::GltfSceneConverter::add(): material attribute TextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute TextureLayer was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute TextureMatrix was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute TextureLayer in layer 1 (ClearCoat) was not used\n"},
    {"clear coat, global texture attributes", true, "material-clearcoat.gltf",
        MaterialData{{}, {
            {MaterialAttribute::TextureMatrix,
                Matrix3::translation({0.25f, 1.0f})},
            {MaterialAttribute::TextureCoordinates, 1u},
            /* There are all local layers so this one stays unused */
            {MaterialAttribute::TextureLayer, 33u},
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.7f},
            {MaterialAttribute::LayerFactorTexture, 0u},
            {MaterialAttribute::LayerFactorTextureLayer, 0u}, /* unused */
            {MaterialAttribute::Roughness, 0.8f},
            {MaterialAttribute::RoughnessTexture, 0u},
            /* This one is local, this overriding the global TextureMatrix */
            {MaterialAttribute::RoughnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
            /* This one is local, thus overriding the global
               TextureCoordinates */
            {MaterialAttribute::RoughnessTextureCoordinates, 2u},
            {MaterialAttribute::RoughnessTextureLayer, 0u}, /* unused */
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 0.75f},
            /* This one is again local */
            {MaterialAttribute::NormalTextureMatrix,
                Matrix3::scaling({-0.25f, 0.75f})},
            /* This one is again local */
            {MaterialAttribute::NormalTextureCoordinates, 3u},
            {MaterialAttribute::NormalTextureLayer, 0u}, /* unused */
        }, {3, 18}},
        "Trade::GltfSceneConverter::add(): material attribute TextureLayer was not used\n"},
    /* These two should get removed once GltfImporter's phongMaterialFallback
       option is gone */
    {"phong diffuse attributes matching base color", true, "material-metallicroughness.gltf",
        MaterialData{{}, {
            {MaterialAttribute::BaseColor, Color4{0.1f, 0.2f, 0.3f, 0.4f}},
            {MaterialAttribute::DiffuseColor, Color4{0.1f, 0.2f, 0.3f, 0.4f}},
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::DiffuseTexture, 0u},
            {MaterialAttribute::BaseColorTextureMatrix,
                Matrix3::translation({0.25f, 1.0f})},
            {MaterialAttribute::DiffuseTextureMatrix,
                Matrix3::translation({0.25f, 1.0f})},
            {MaterialAttribute::BaseColorTextureCoordinates, 10u},
            {MaterialAttribute::DiffuseTextureCoordinates, 10u},
            {MaterialAttribute::BaseColorTextureLayer, 0u},
            {MaterialAttribute::DiffuseTextureLayer, 0u},
            {MaterialAttribute::Metalness, 0.25f},
            {MaterialAttribute::Roughness, 0.75f},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 0u},
            {MaterialAttribute::MetalnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::MetalnessTextureCoordinates, 11u},
            {MaterialAttribute::RoughnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::RoughnessTextureCoordinates, 11u}}},
        "" /* No warnings */},
    {"phong diffuse attributes not matching base color", true, "material-metallicroughness.gltf",
        MaterialData{{}, {
            {MaterialAttribute::BaseColor, Color4{0.1f, 0.2f, 0.3f, 0.4f}},
            {MaterialAttribute::DiffuseColor, Color4{}},
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::DiffuseTexture, 1u},
            {MaterialAttribute::BaseColorTextureMatrix,
                Matrix3::translation({0.25f, 1.0f})},
            {MaterialAttribute::DiffuseTextureMatrix,
                Matrix3{}},
            {MaterialAttribute::BaseColorTextureCoordinates, 10u},
            {MaterialAttribute::DiffuseTextureCoordinates, 11u},
            {MaterialAttribute::BaseColorTextureLayer, 0u},
            {MaterialAttribute::DiffuseTextureLayer, 1u},
            {MaterialAttribute::Metalness, 0.25f},
            {MaterialAttribute::Roughness, 0.75f},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 0u},
            {MaterialAttribute::MetalnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::MetalnessTextureCoordinates, 11u},
            {MaterialAttribute::RoughnessTextureMatrix,
                Matrix3::translation({0.25f, 0.0f})*
                Matrix3::scaling({-0.25f, 0.75f})},
            {MaterialAttribute::RoughnessTextureCoordinates, 11u}}},
        "Trade::GltfSceneConverter::add(): material attribute DiffuseColor was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute DiffuseTexture was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute DiffuseTextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute DiffuseTextureLayer was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute DiffuseTextureMatrix was not used\n"},
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    bool needsTexture, needsTexture3D;
    const char* expected;
    Containers::Array<const char*> explicitUsedExtensions;
    MaterialData material;
    Containers::Array<Containers::Pair<UnsignedInt, const char*>> expectedRemoveAttributes;
    Containers::Array<UnsignedInt> expectedRemoveLayers;
    Containers::Optional<MaterialData> expectedAdd;
    const char* expectedWarning;
} AddMaterialCustomData[]{
    {"", true, true, "material-custom.gltf", {},
        MaterialData{{}, {
            /* Gets wrapped to have each column on a separate line */
            {"category", "FANCY"},
            {"fancinessDirection", Vector3{0.5f, 0.3f, 0.0f}},
            {"fullCircle", 360.0_degf},
            {"unrecognizedTexture", 5u},
            {"unrecognizedTextureLayer", 666u},
            {"veryCustom", true},
            /* Used by layerFactorTexture */
            {MaterialAttribute::TextureCoordinates, 222u},
            /* Used by normalTexture and customTexture */
            {MaterialAttribute::TextureMatrix, Matrix3::scaling({0.5f, 2.0f})},
            /* Used by decalTexture */
            {MaterialAttribute::TextureLayer, 3u},

            {MaterialAttribute::LayerName, "#COMPLETELY_empty_layer"},

            {MaterialAttribute::LayerName, "#EXT_another_extension"},
            {"integerProperty", -1},
            {"layerFactorTexture", 1u},
                /* Uses layer-local texture matrix and layer */
            /* Used by layerTexture */
            {MaterialAttribute::TextureMatrix, Matrix3::translation({3.0f, 4.0f})},
            /* Used by layerTexture */
            {MaterialAttribute::TextureLayer, 2u},

            {MaterialAttribute::LayerName, "#EXT_custom_extension"},
            {"customTexture", 0u},
            {"customTextureCoordinates", 8u},
                /* Uses global matrix and layer-local layer */
            {"floatProperty", 3.14f},
            {"grungeTexture", 0u},
            {"grungeTextureMatrix", Matrix3::translation({1.0f, 2.0f})},
                /* Uses layer-local coordinates */
            {"normalTexture", 1u},
            /* Gets written as a regular attribute, not as a normalTexture
               property */
            {"normalTextureScale", 0.3f},
            {"normalTextureLayer", 4u},
                /* Uses global matrix and layer-local coordinates */
            /* Used by normalTexture and grungeTexture */
            {MaterialAttribute::TextureCoordinates, 777u},
            /* Used by grungeTexture and customTexture */
            {MaterialAttribute::TextureLayer, 0u},

            {MaterialAttribute::LayerName, "#MAGNUM_materials_decal"},
            {"decalTexture", 1u},
                /* Uses global layer and matrix */
        }, {9, 10, 15, 26, 28}}, {InPlaceInit, {
            /* Non-float scalar properties are converted to float */
            {0, "fullCircle"},
            {0, "unrecognizedTexture"},
            {0, "unrecognizedTextureLayer"},
            {2, "integerProperty"},
            /* Global texture properties are converted to local */
            {0, "TextureCoordinates"},
            {0, "TextureLayer"},
            {0, "TextureMatrix"},
            {0, "TextureCoordinates"},
            {2, "TextureLayer"},
            {2, "TextureMatrix"},
            {3, "TextureCoordinates"},
            /* Zero texture layer is omitted */
            {3, "TextureLayer"},
        }}, {}, MaterialData{{}, {
            /* Non-float scalar properties are converted to float */
            {"fullCircle", 360.0f},
            {"unrecognizedTexture", 5.0f},
            {"unrecognizedTextureLayer", 666.0f},
            {"integerProperty", -1.0f},
            /* Global texture properties are converted to local */
            {"layerFactorTextureCoordinates", 222u},
            {"layerFactorTextureLayer", 2u},
            {"layerFactorTextureMatrix", Matrix3::translation({3.0f, 4.0f})},
            {"customTextureMatrix", Matrix3::scaling({0.5f, 2.0f})},
            {"grungeTextureCoordinates", 777u},
            {"normalTextureCoordinates", 777u},
            {"normalTextureMatrix", Matrix3::scaling({0.5f, 2.0f})},
            {"decalTextureCoordinates", 222u},
            {"decalTextureLayer", 3u},
            {"decalTextureMatrix", Matrix3::scaling({0.5f, 2.0f})},
        }, {3, 3, 7, 11, 14}},
        ""},
    {"no KHR_texture_transform, explicit extensionsUsed", true, false, "material-custom-no-transform-explicit-used-extensions.gltf",
        {InPlaceInit, {
            "MAGNUM_is_amazing",
            "KHR_texture_transform_should_not_be_here",
            "AND_no_extension_twice"
        }},
        MaterialData{{}, {
            {MaterialAttribute::LayerName, "#KHR_texture_transform_should_not_be_here"},
            {"withNoTransformTexture", 0u},
        }, {0, 2}}, {}, {}, {},
        ""},
    {"skipped attributes", true, false, "material-custom-skipped.gltf", {},
        MaterialData{{}, {
            /* The unused attributes/layers are reported at the end, after all
               other failures */
            {"NotCustomAttribute", "uppercase!"},
            {"bufferAttribute", Containers::ArrayView<const void>{"yay"}},
            {"matrixAttribute", Matrix2x4{}},
            {"pointerAttribute", &AddMaterialData[0]},

            {MaterialAttribute::LayerName, "#EXT_invalid_attributes"},
            {"NotCustomAttributeEither", "UPPERCASE"},
            /* This one is not a fallback used by any texture */
            {MaterialAttribute::TextureMatrix, Matrix3{}},
            {"pointerAttributeAgain", &AddMaterialData[1]},
            {"unusedTextureLayer", 5u},

            {MaterialAttribute::LayerName, "#EXT_invalid_textures"},
            {"boolCoordinatesTexture", 0u},
            {"boolCoordinatesTextureCoordinates", true},
            {"floatTexture", 15.0f},
            {"intMatrixTexture", 0u},
            {"intMatrixTextureMatrix", -17},
            {"rotatedTexture", 0u},
            {"rotatedTextureMatrix", Matrix3::translation({1.0f, 2.0f})*Matrix3::rotation(35.0_degf)},
            {"stringLayerTexture", 0u},
            {"stringLayerTextureLayer", "second"},

            {MaterialAttribute::LayerName, "#EXT_oob_textures"},
            {"oobLayerInATexture", 0u},
            {"oobLayerInATextureLayer", 1u},
            {"oobGlobalLayerInATexture", 0u},
            {MaterialAttribute::TextureLayer, 2u},
            {"oobTexture", 1u},

            /* A completely empty layer here */

            {MaterialAttribute::LayerName, "notAnExtension"},
            {"thisIsNotWritten", "anywhere"},
        }, {4, 9, 19, 25, 25, 27}}, {InPlaceInit, {
            {0, "NotCustomAttribute"},
            {0, "bufferAttribute"},
            {0, "matrixAttribute"},
            {0, "pointerAttribute"},
            {1, "NotCustomAttributeEither"},
            {1, "TextureMatrix"},
            {1, "pointerAttributeAgain"},
            {1, "unusedTextureLayer"},
            {2, "boolCoordinatesTextureCoordinates"},
            {2, "floatTexture"},
            {2, "intMatrixTextureMatrix"},
            /* Only translation kept from this one */
            {2, "rotatedTextureMatrix"},
            {2, "stringLayerTextureLayer"},
            {3, "TextureLayer"},
            {3, "oobGlobalLayerInATexture"},
            {3, "oobLayerInATexture"},
            {3, "oobLayerInATextureLayer"},
            {3, "oobTexture"},
        }}, {InPlaceInit, {
            4, 5
        }}, MaterialData{{}, {
            {"rotatedTextureMatrix", Matrix3::translation({1.0f, 2.0f})}
        }, {0, 0, 1}},
        "Trade::GltfSceneConverter::add(): custom material attribute boolCoordinatesTextureCoordinates in layer 2 (#EXT_invalid_textures) is Trade::MaterialAttributeType::Bool, not exporting any texture coordinate set\n"
        "Trade::GltfSceneConverter::add(): custom material attribute floatTexture in layer 2 (#EXT_invalid_textures) is Trade::MaterialAttributeType::Float, not writing a textureInfo object\n"
        "Trade::GltfSceneConverter::add(): custom material attribute intMatrixTextureMatrix in layer 2 (#EXT_invalid_textures) is Trade::MaterialAttributeType::Int, not exporting any texture transform\n"
        "Trade::GltfSceneConverter::add(): material attribute rotatedTextureMatrix in layer 2 (#EXT_invalid_textures) rotation was not used\n"
        "Trade::GltfSceneConverter::add(): custom material attribute stringLayerTextureLayer in layer 2 (#EXT_invalid_textures) is Trade::MaterialAttributeType::String, referencing layer 0 instead\n"
        "Trade::GltfSceneConverter::add(): material attribute TextureLayer in layer 3 (#EXT_oob_textures) value 2 out of range for 1 layers in texture 0, skipping\n"
        "Trade::GltfSceneConverter::add(): material attribute oobLayerInATextureLayer in layer 3 (#EXT_oob_textures) value 1 out of range for 1 layers in texture 0, skipping\n"
        "Trade::GltfSceneConverter::add(): custom material attribute oobTexture in layer 3 (#EXT_oob_textures) references texture 1 but only 1 textures were added so far, skipping\n"

        "Trade::GltfSceneConverter::add(): material attribute NotCustomAttribute was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute bufferAttribute was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute matrixAttribute was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute pointerAttribute was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute NotCustomAttributeEither in layer 1 (#EXT_invalid_attributes) was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute TextureMatrix in layer 1 (#EXT_invalid_attributes) was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute pointerAttributeAgain in layer 1 (#EXT_invalid_attributes) was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute unusedTextureLayer in layer 1 (#EXT_invalid_attributes) was not used\n"
        "Trade::GltfSceneConverter::add(): material layer 4 was not used\n"
        "Trade::GltfSceneConverter::add(): material layer 5 (notAnExtension) was not used\n"},
    {"skipped attributes, 3D textures", true, true, "material-custom-skipped-3d.gltf", {},
        MaterialData{{}, {
            {MaterialAttribute::TextureLayer, 8u},
            {MaterialAttribute::TextureMatrix, Matrix3::rotation(35.0_degf)},

            {MaterialAttribute::LayerName, "#EXT_invalid_textures"},
            {MaterialAttribute::TextureLayer, 7u},
            {"oobTexture", 3u},
            {"oobLayerInATexture", 1u},
            {"oobLayerInATextureLayer", 5u},
            {"oobLayerLocalLayerInATexture", 1u},

            {MaterialAttribute::LayerName, "#EXT_invalid_textures2"},
            {"oobGlobalLayerInATexture", 1u},
            {"noRotationTexture", 0u},
            {"noRotationTextureLayer", 0u}, /* implicit, ignored */
        }, {2, 8, 12}}, {InPlaceInit, {
            {0, "TextureLayer"},
            {0, "TextureMatrix"},
            {1, "TextureLayer"},
            {1, "oobLayerInATexture"},
            {1, "oobLayerInATextureLayer"},
            {1, "oobLayerLocalLayerInATexture"},
            {1, "oobTexture"},
            {2, "oobGlobalLayerInATexture"},
            {2, "noRotationTextureLayer"},
        }}, {}, {},
        "Trade::GltfSceneConverter::add(): material attribute oobLayerInATextureLayer in layer 1 (#EXT_invalid_textures) value 5 out of range for 5 layers in texture 1, skipping\n"
        "Trade::GltfSceneConverter::add(): material attribute TextureLayer in layer 1 (#EXT_invalid_textures) value 7 out of range for 5 layers in texture 1, skipping\n"
        "Trade::GltfSceneConverter::add(): custom material attribute oobTexture in layer 1 (#EXT_invalid_textures) references texture 3 but only 2 textures were added so far, skipping\n"
        "Trade::GltfSceneConverter::add(): material attribute TextureMatrix rotation was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute TextureLayer value 8 out of range for 5 layers in texture 1, skipping\n"},
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    MaterialData material;
    const char* message;
} AddMaterialInvalidData[]{
    {"texture out of bounds",
        MaterialData{{}, {
            {MaterialAttribute::OcclusionTexture, 1u},
        }}, "material attribute OcclusionTexture references texture 1 but only 1 were added so far"},
    {"texture in a layer out of bounds",
        MaterialData{{}, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::NormalTexture, 2u}
        }, {0, 2}}, "material attribute NormalTexture in layer ClearCoat references texture 2 but only 1 were added so far"},
    {"2D texture layer out of bounds",
        MaterialData{{}, {
            {MaterialAttribute::EmissiveTexture, 0u},
            {MaterialAttribute::EmissiveTextureLayer, 1u},
        }}, "material attribute EmissiveTextureLayer value 1 out of range for 1 layers in texture 0"},
    {"2D texture global layer out of bounds",
        MaterialData{{}, {
            {MaterialAttribute::EmissiveTexture, 0u},
            {MaterialAttribute::TextureLayer, 1u},
        }}, "material attribute TextureLayer value 1 out of range for 1 layers in texture 0"},
    {"2D texture layer in a layer out of bounds",
        MaterialData{{}, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactorTexture, 0u},
            {MaterialAttribute::LayerFactorTextureLayer, 1u},
        }, {0, 3}}, "material attribute LayerFactorTextureLayer in layer ClearCoat value 1 out of range for 1 layers in texture 0"},
    {"2D texture material-layer-local layer in a layer out of bounds",
        MaterialData{{}, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactorTexture, 0u},
            {MaterialAttribute::TextureLayer, 1u},
        }, {0, 3}}, "material attribute TextureLayer in layer ClearCoat value 1 out of range for 1 layers in texture 0"},
    {"2D texture global layer in a layer out of bounds",
        MaterialData{{}, {
            {MaterialAttribute::TextureLayer, 1u},
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactorTexture, 0u},
        }, {1, 3}}, "material attribute TextureLayer value 1 out of range for 1 layers in texture 0"},
    {"metallic/roughness, unsupported packing",
        MaterialData{{}, {
            {MaterialAttribute::MetalnessTexture, 0u},
            {MaterialAttribute::RoughnessTexture, 0u},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::B},
        }}, "unsupported R/B packing of a metallic/roughness texture"},
    {"metallic/roughness, no roughness texture",
        MaterialData{{}, {
            {MaterialAttribute::MetalnessTexture, 0u},
            {MaterialAttribute::MetalnessTextureSwizzle, MaterialTextureSwizzle::B},
        }}, "can only represent a combined metallic/roughness texture or neither of them"},
    {"metallic/roughness, no metalness texture",
        MaterialData{{}, {
            {MaterialAttribute::RoughnessTexture, 0u},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
        }}, "can only represent a combined metallic/roughness texture or neither of them"},
    {"unsupported normal texture packing",
        MaterialData{{}, {
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureSwizzle, MaterialTextureSwizzle::RG},
        }}, "unsupported RG packing of a normal texture"},
    {"unsupported occlusion texture packing",
        MaterialData{{}, {
            {MaterialAttribute::OcclusionTexture, 0u},
            {MaterialAttribute::OcclusionTextureSwizzle, MaterialTextureSwizzle::B},
        }}, "unsupported B packing of an occlusion texture"},
    {"unsupported clear coat layer factor texture packing",
        MaterialData{{}, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactorTexture, 0u},
            {MaterialAttribute::LayerFactorTextureSwizzle, MaterialTextureSwizzle::B},
        }, {0, 3}}, "unsupported B packing of a clear coat layer factor texture"},
    {"unsupported clear coat roughness texture packing",
        MaterialData{{}, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::RoughnessTexture, 0u},
            /* implicit swizzle, which is R */
        }, {0, 2}}, "unsupported R packing of a clear coat roughness texture"},
    {"unsupported clear coat normal texture packing",
        MaterialData{{}, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureSwizzle, MaterialTextureSwizzle::BA},
        }, {0, 3}}, "unsupported BA packing of a clear coat normal texture"},
};

/* Reusing the already-invented GltfImporter/Test/texcoord-flip.bin.in. The
   glb/bin file has the data Y-flipped, so the input has to be without. */
const Vector2 TextureCoordinateYFlipFloat[]{
    {1.0, 0.5},
    {0.5, 1.0},
    {0.0, 0.0}
};
const Vector2ub TextureCoordinateYFlipNormalizedUnsignedByte[]{
    {254, 127}, /* On Y flipped */
    {127, 0},
    {0, 254}
};
const Vector2us TextureCoordinateYFlipNormalizedUnsignedShort[]{
    {65534, 32767}, /* On Y flipped */
    {32767, 0},
    {0, 65534}
};
const Vector2b TextureCoordinateYFlipNormalizedByte[]{
    {-127, 0}, /* On X flipped */
    {0, 127},
    {127, -127},
};
const Vector2s TextureCoordinateYFlipShort[]{
    {200, 100}, /* On Y off-center */
    {100, 300},
    {0, -100}
};

/* Reusing the already-invented GltfImporter/Test/texcoord-flip.gltf. Again the
   input matrices have to be Y-flipped compared to what's in the gltf. */
const struct {
    const char* name;
    Containers::Optional<bool> textureCoordinateYFlipInMaterial;
    Containers::Optional<bool> keepMaterialDefaults;
    MeshData mesh;
    MaterialData material;
    const char* expected;
} TextureCoordinateYFlipData[]{
    {"floats", {}, {},
        MeshData{MeshPrimitive::Triangles, {}, TextureCoordinateYFlipFloat, {
            MeshAttributeData{MeshAttribute::TextureCoordinates, Containers::arrayView(TextureCoordinateYFlipFloat)}
        }},
        MaterialData{{}, {
            MaterialAttributeData{MaterialAttribute::BaseColorTexture, 0u},
        }},
        "texcoord-flip-floats.glb"},
    {"floats, flip in material", true, {},
        MeshData{MeshPrimitive::Triangles, {}, TextureCoordinateYFlipFloat, {
            MeshAttributeData{MeshAttribute::TextureCoordinates, Containers::arrayView(TextureCoordinateYFlipFloat)}
        }},
        MaterialData{{}, {
            MaterialAttributeData{MaterialAttribute::BaseColorTexture, 0u},
        }},
        "texcoord-flip-floats-material.glb"},
    {"floats, flip in material, custom material attribute", true, {},
        MeshData{MeshPrimitive::Triangles, {}, TextureCoordinateYFlipFloat, {
            MeshAttributeData{MeshAttribute::TextureCoordinates, Containers::arrayView(TextureCoordinateYFlipFloat)}
        }},
        MaterialData{{}, {
            {MaterialAttribute::LayerName, "#EXT_wonderful_extension"},
            {"wonderfulTexture", 0u},
        }, {0, 2}},
        "texcoord-flip-floats-material-custom-material-attribute.glb"},
    {"normalized unsigned byte", {}, {},
        MeshData{MeshPrimitive::Triangles, {}, TextureCoordinateYFlipNormalizedUnsignedByte, {
            MeshAttributeData{MeshAttribute::TextureCoordinates, VertexFormat::Vector2ubNormalized, Containers::arrayView(TextureCoordinateYFlipNormalizedUnsignedByte)}
        }},
        MaterialData{{}, {
            MaterialAttributeData{MaterialAttribute::BaseColorTexture, 0u},
            MaterialAttributeData{MaterialAttribute::BaseColorTextureMatrix,
                Matrix3::translation(Vector2::yAxis(1.0f))*
                Matrix3::scaling({1.00393f, -1.00393f})}
        }},
        "texcoord-flip-normalized-unsigned-byte.glb"},
    {"normalized unsigned byte, flip in material", true, {},
        MeshData{MeshPrimitive::Triangles, {}, TextureCoordinateYFlipNormalizedUnsignedByte, {
            MeshAttributeData{MeshAttribute::TextureCoordinates, VertexFormat::Vector2ubNormalized, Containers::arrayView(TextureCoordinateYFlipNormalizedUnsignedByte)}
        }},
        MaterialData{{}, {
            MaterialAttributeData{MaterialAttribute::BaseColorTexture, 0u},
            MaterialAttributeData{MaterialAttribute::BaseColorTextureMatrix,
                Matrix3::translation(Vector2::yAxis(1.0f))*
                Matrix3::scaling({1.00393f, -1.00393f})}
        }},
        "texcoord-flip-normalized-unsigned-byte-material.glb"},
    {"normalized unsigned short", {}, {},
        MeshData{MeshPrimitive::Triangles, {}, TextureCoordinateYFlipNormalizedUnsignedShort, {
            MeshAttributeData{MeshAttribute::TextureCoordinates, VertexFormat::Vector2usNormalized, Containers::arrayView(TextureCoordinateYFlipNormalizedUnsignedShort)}
        }},
        MaterialData{{}, {
            MaterialAttributeData{MaterialAttribute::BaseColorTexture, 0u},
            MaterialAttributeData{MaterialAttribute::BaseColorTextureMatrix,
                Matrix3::translation(Vector2::yAxis(1.0f))*
                Matrix3::scaling({1.000015259254738f, -1.000015259254738f})}
        }},
        "texcoord-flip-normalized-unsigned-short.glb"},
    /* The 1.0e-5 epsilon is too large to consider a scale by 1.000015259254738
       a non-identity, so explicitly force keeping defaults */
    /** @todo any better way to fix this or is this just a too rare corner
        case? */
    {"normalized unsigned short, flip in material", true, true,
        MeshData{MeshPrimitive::Triangles, {}, TextureCoordinateYFlipNormalizedUnsignedShort, {
            MeshAttributeData{MeshAttribute::TextureCoordinates, VertexFormat::Vector2usNormalized, Containers::arrayView(TextureCoordinateYFlipNormalizedUnsignedShort)}
        }},
        MaterialData{{}, {
            MaterialAttributeData{MaterialAttribute::BaseColorTexture, 0u},
            MaterialAttributeData{MaterialAttribute::BaseColorTextureMatrix,
                Matrix3::translation(Vector2::yAxis(1.0f))*
                Matrix3::scaling({1.000015259254738f, -1.000015259254738f})}
        }},
        "texcoord-flip-normalized-unsigned-short-material.glb"},
    {"normalized byte, flip in material", true, {},
        MeshData{MeshPrimitive::Triangles, {}, TextureCoordinateYFlipNormalizedByte, {
            MeshAttributeData{MeshAttribute::TextureCoordinates, VertexFormat::Vector2bNormalized, Containers::arrayView(TextureCoordinateYFlipNormalizedByte)}
        }},
        MaterialData{{}, {
            MaterialAttributeData{MaterialAttribute::BaseColorTexture, 0u},
            MaterialAttributeData{MaterialAttribute::BaseColorTextureMatrix,
                Matrix3::translation({0.5f, 0.5f})*
                Matrix3::scaling({-0.5f, 0.5f})}
        }},
        "texcoord-flip-normalized-byte-material.glb"},
    {"short, flip in material", true, {},
        MeshData{MeshPrimitive::Triangles, {}, TextureCoordinateYFlipShort, {
            MeshAttributeData{MeshAttribute::TextureCoordinates, Containers::arrayView(TextureCoordinateYFlipShort)}
        }},
        MaterialData{{}, {
            MaterialAttributeData{MaterialAttribute::BaseColorTexture, 0u},
            MaterialAttributeData{MaterialAttribute::BaseColorTextureMatrix,
                Matrix3::translation({0.0f, 0.25f})*
                Matrix3::scaling({0.005f, 0.0025f})}
        }},
        "texcoord-flip-short-material.glb"},
};

const struct {
    const char* name;
    Int defaultScene;
    const char* expected;
} AddSceneEmptyData[]{
    {"", -1, "scene-empty.gltf"},
    {"default scene", 0, "scene-empty-default.gltf"}
};

const struct {
    const char* name;
    SceneConverterFlags flags;
    Containers::StringView dataName;
    UnsignedShort offset;
    const char* expected;
    bool quiet;
} AddSceneData[]{
    {"", {}, {}, 0, "scene.gltf", false},
    {"quiet", SceneConverterFlag::Quiet, {}, 0, "scene.gltf", true},
    {"name", {}, "A simple sceen!", 0, "scene-name.gltf", false},
    {"object ID with an offset", {}, {}, 350, "scene.gltf", false}
};

const Containers::Pair<UnsignedInt, Int> SceneInvalidParentMappingOutOfBounds[]{
    {0, -1}, {15, 14}, {37, 36}, {1, -1}
};
const Containers::Pair<UnsignedInt, Int> SceneInvalidParentIndexOutOfBounds[]{
    {0, -1}, {36, 37}, {1, -1}
};
const Containers::Pair<UnsignedInt, UnsignedInt> SceneInvalidMappingOutOfBounds[]{
    {0, 0}, {36, 1}, {37, 1}, {1, 1}
};
const Containers::Pair<UnsignedInt, Int> SceneInvalidTwoParents[]{
    {0, -1}, {15, 14}, {36, 35}, {15, 17}, {1, -1}
};
const Containers::Pair<UnsignedInt, Int> SceneInvalidParentIsSelf[]{
    {0, -1}, {17, 17}, {1, -1}
};
const Containers::Pair<UnsignedInt, Int> SceneInvalidParentIsChild[]{
    {0, 3}, {3, 2}, {2, 0}
};
const Containers::Pair<UnsignedInt, UnsignedInt> SceneInvalidMeshOutOfBounds[]{
    {0, 0}, {17, 1}, {2, 2}, {1, 1}
};
const Containers::Triple<UnsignedInt, UnsignedInt, Int> SceneInvalidMaterialOutOfBounds[]{
    {0, 0, -1}, {17, 1, 2}, {2, 1, 1}
};

const struct {
    const char* name;
    SceneData scene;
    const char* message;
} AddSceneInvalidData[]{
    {"not 3D", SceneData{SceneMappingType::UnsignedInt, 1, nullptr, {}},
        "expected a 3D scene"},
    {"parent mapping out of bounds", SceneData{SceneMappingType::UnsignedInt, 37, {}, SceneInvalidParentMappingOutOfBounds, {
        /* To mark the scene as 3D */
        SceneFieldData{SceneField::Transformation,
            SceneMappingType::UnsignedInt, nullptr,
            SceneFieldType::Matrix4x4, nullptr},
        SceneFieldData{SceneField::Parent,
            Containers::stridedArrayView(SceneInvalidParentMappingOutOfBounds).slice(&Containers::Pair<UnsignedInt, Int>::first),
            Containers::stridedArrayView(SceneInvalidParentMappingOutOfBounds).slice(&Containers::Pair<UnsignedInt, Int>::second)},
    }}, "scene parent mapping 37 out of bounds for 37 objects"},
    {"parent index out of bounds", SceneData{SceneMappingType::UnsignedInt, 37,{}, SceneInvalidParentIndexOutOfBounds, {
        /* To mark the scene as 3D */
        SceneFieldData{SceneField::Transformation,
            SceneMappingType::UnsignedInt, nullptr,
            SceneFieldType::Matrix4x4, nullptr},
        SceneFieldData{SceneField::Parent,
            Containers::stridedArrayView(SceneInvalidParentIndexOutOfBounds).slice(&Containers::Pair<UnsignedInt, Int>::first),
            Containers::stridedArrayView(SceneInvalidParentIndexOutOfBounds).slice(&Containers::Pair<UnsignedInt, Int>::second)},
    }}, "scene parent reference 37 out of bounds for 37 objects"},
    {"two parents", SceneData{SceneMappingType::UnsignedInt, 37,{}, SceneInvalidTwoParents, {
        /* To mark the scene as 3D */
        SceneFieldData{SceneField::Transformation,
            SceneMappingType::UnsignedInt, nullptr,
            SceneFieldType::Matrix4x4, nullptr},
        SceneFieldData{SceneField::Parent,
            Containers::stridedArrayView(SceneInvalidTwoParents).slice(&Containers::Pair<UnsignedInt, Int>::first),
            Containers::stridedArrayView(SceneInvalidTwoParents).slice(&Containers::Pair<UnsignedInt, Int>::second)},
    }}, "object 15 has more than one parent"},
    {"parent is self", SceneData{SceneMappingType::UnsignedInt, 37,{}, SceneInvalidParentIsSelf, {
        /* To mark the scene as 3D */
        SceneFieldData{SceneField::Transformation,
            SceneMappingType::UnsignedInt, nullptr,
            SceneFieldType::Matrix4x4, nullptr},
        SceneFieldData{SceneField::Parent,
            Containers::stridedArrayView(SceneInvalidParentIsSelf).slice(&Containers::Pair<UnsignedInt, Int>::first),
            Containers::stridedArrayView(SceneInvalidParentIsSelf).slice(&Containers::Pair<UnsignedInt, Int>::second)},
    }}, "scene hierarchy contains a cycle starting at object 17"},
    {"parent is a child", SceneData{SceneMappingType::UnsignedInt, 37,{}, SceneInvalidParentIsChild, {
        /* To mark the scene as 3D */
        SceneFieldData{SceneField::Transformation,
            SceneMappingType::UnsignedInt, nullptr,
            SceneFieldType::Matrix4x4, nullptr},
        SceneFieldData{SceneField::Parent,
            Containers::stridedArrayView(SceneInvalidParentIsChild).slice(&Containers::Pair<UnsignedInt, Int>::first),
            Containers::stridedArrayView(SceneInvalidParentIsChild).slice(&Containers::Pair<UnsignedInt, Int>::second)},
    }}, "scene hierarchy contains a cycle starting at object 0"},
    /* Different code path from "parent mapping out of bounds" */
    {"mapping out of bounds", SceneData{SceneMappingType::UnsignedInt, 37,{}, SceneInvalidMappingOutOfBounds, {
        /* To mark the scene as 3D */
        SceneFieldData{SceneField::Transformation,
            SceneMappingType::UnsignedInt, nullptr,
            SceneFieldType::Matrix4x4, nullptr},
        SceneFieldData{SceneField::Light,
            Containers::stridedArrayView(SceneInvalidMappingOutOfBounds).slice(&Containers::Pair<UnsignedInt, UnsignedInt>::first),
            Containers::stridedArrayView(SceneInvalidMappingOutOfBounds).slice(&Containers::Pair<UnsignedInt, UnsignedInt>::second)},
    }}, "Trade::SceneField::Light mapping 37 out of bounds for 37 objects"},
    {"mesh out of bounds", SceneData{SceneMappingType::UnsignedInt, 37,{}, SceneInvalidMeshOutOfBounds, {
        /* To mark the scene as 3D */
        SceneFieldData{SceneField::Transformation,
            SceneMappingType::UnsignedInt, nullptr,
            SceneFieldType::Matrix4x4, nullptr},
        SceneFieldData{SceneField::Mesh,
            Containers::stridedArrayView(SceneInvalidMeshOutOfBounds).slice(&Containers::Pair<UnsignedInt, UnsignedInt>::first),
            Containers::stridedArrayView(SceneInvalidMeshOutOfBounds).slice(&Containers::Pair<UnsignedInt, UnsignedInt>::second)},
    }}, "scene references mesh 2 but only 2 were added so far"},
    {"material out of bounds", SceneData{SceneMappingType::UnsignedInt, 37,{}, SceneInvalidMaterialOutOfBounds, {
        /* To mark the scene as 3D */
        SceneFieldData{SceneField::Transformation,
            SceneMappingType::UnsignedInt, nullptr,
            SceneFieldType::Matrix4x4, nullptr},
        SceneFieldData{SceneField::Mesh,
            Containers::stridedArrayView(SceneInvalidMaterialOutOfBounds).slice(&Containers::Triple<UnsignedInt, UnsignedInt, Int>::first),
            Containers::stridedArrayView(SceneInvalidMaterialOutOfBounds).slice(&Containers::Triple<UnsignedInt, UnsignedInt, Int>::second)},
        SceneFieldData{SceneField::MeshMaterial,
            Containers::stridedArrayView(SceneInvalidMaterialOutOfBounds).slice(&Containers::Triple<UnsignedInt, UnsignedInt, Int>::first),
            Containers::stridedArrayView(SceneInvalidMaterialOutOfBounds).slice(&Containers::Triple<UnsignedInt, UnsignedInt, Int>::third)},
    }}, "scene references material 2 but only 2 were added so far"},
};

GltfSceneConverterTest::GltfSceneConverterTest() {
    addInstancedTests({&GltfSceneConverterTest::empty},
        Containers::arraySize(FileVariantData));

    addInstancedTests({&GltfSceneConverterTest::outputFormatDetectionToData},
        Containers::arraySize(OutputFormatDetectionToDataData));

    addInstancedTests({&GltfSceneConverterTest::outputFormatDetectionToFile},
        Containers::arraySize(OutputFormatDetectionToFileData));

    addTests({&GltfSceneConverterTest::metadata,
              &GltfSceneConverterTest::generatorVersion,
              &GltfSceneConverterTest::abort});

    addInstancedTests({&GltfSceneConverterTest::addMesh},
        Containers::arraySize(FileVariantWithNamesData));

    addTests({&GltfSceneConverterTest::addMeshBufferViewsNonInterleaved,
              &GltfSceneConverterTest::addMeshBufferViewsInterleavedPaddingBegin});

    addInstancedTests({&GltfSceneConverterTest::addMeshBufferViewsInterleavedPaddingBeginEnd},
        Containers::arraySize(VerboseData));

    addTests({&GltfSceneConverterTest::addMeshBufferViewsMixed});

    addInstancedTests({&GltfSceneConverterTest::addMeshNoAttributes},
        Containers::arraySize(QuietData));

    addTests({&GltfSceneConverterTest::addMeshNoIndices});

    addInstancedTests({&GltfSceneConverterTest::addMeshNoIndicesNoAttributes,
                       &GltfSceneConverterTest::addMeshNoIndicesNoVertices},
        Containers::arraySize(FileVariantStrictWarningData));

    addInstancedTests({&GltfSceneConverterTest::addMeshAttribute},
        Containers::arraySize(AddMeshAttributeData));

    addInstancedTests({&GltfSceneConverterTest::addMeshSkinningAttributes},
        Containers::arraySize(AddMeshSkinningAttributesData));

    addInstancedTests({&GltfSceneConverterTest::addMeshSkinningAttributesUnsignedInt},
        Containers::arraySize(QuietData));

    addTests({&GltfSceneConverterTest::addMeshDuplicateAttribute,
              &GltfSceneConverterTest::addMeshCustomAttributeResetName});

    addInstancedTests({&GltfSceneConverterTest::addMeshCustomAttributeNoName},
        Containers::arraySize(QuietData));

    addTests({&GltfSceneConverterTest::addMeshCustomObjectIdAttributeName,

              &GltfSceneConverterTest::addMeshMultiple,
              &GltfSceneConverterTest::addMeshBufferAlignment});

    addInstancedTests({&GltfSceneConverterTest::addMeshInvalid},
        Containers::arraySize(AddMeshInvalidData));

    addInstancedTests({&GltfSceneConverterTest::addImage2D},
        Containers::arraySize(AddImage2DData));

    addTests({&GltfSceneConverterTest::addImageCompressed2D});

    addInstancedTests({&GltfSceneConverterTest::addImage3D},
        Containers::arraySize(AddImage3DData));

    addTests({&GltfSceneConverterTest::addImageCompressed3D});

    addInstancedTests({&GltfSceneConverterTest::addImagePropagateFlags},
        Containers::arraySize(AddImagePropagateFlagsData));

    addTests({&GltfSceneConverterTest::addImagePropagateConfiguration});

    addInstancedTests({
        &GltfSceneConverterTest::addImagePropagateConfigurationUnknown,
        &GltfSceneConverterTest::addImagePropagateConfigurationGroup},
        Containers::arraySize(QuietData));

    addTests({&GltfSceneConverterTest::addImageMultiple,
              &GltfSceneConverterTest::addImageNoConverterManager,
              &GltfSceneConverterTest::addImageExternalToData});

    addInstancedTests({&GltfSceneConverterTest::addImageInvalid2D},
        Containers::arraySize(AddImageInvalid2DData));

    addInstancedTests({&GltfSceneConverterTest::addImageInvalid3D},
        Containers::arraySize(AddImageInvalid3DData));

    addInstancedTests({&GltfSceneConverterTest::addTexture},
        Containers::arraySize(AddTextureData));

    addTests({&GltfSceneConverterTest::addTextureMultiple,
              &GltfSceneConverterTest::addTextureDeduplicatedSamplers});

    addInstancedTests({&GltfSceneConverterTest::addTextureInvalid},
        Containers::arraySize(AddTextureInvalidData));

    addInstancedTests({&GltfSceneConverterTest::addMaterial},
        Containers::arraySize(AddMaterialData));

    addTests({&GltfSceneConverterTest::addMaterial2DArrayTextures});

    /* MSVC needs explicit type due to default template args */
    addInstancedTests<GltfSceneConverterTest>({
        &GltfSceneConverterTest::addMaterialUnusedAttributes,
        &GltfSceneConverterTest::addMaterialUnusedAttributes<SceneConverterFlag::Quiet>},
        Containers::arraySize(AddMaterialUnusedAttributesData));

    /* MSVC needs explicit type due to default template args */
    addInstancedTests<GltfSceneConverterTest>({
        &GltfSceneConverterTest::addMaterialCustom,
        &GltfSceneConverterTest::addMaterialCustom<SceneConverterFlag::Quiet>},
        Containers::arraySize(AddMaterialCustomData));

    addTests({&GltfSceneConverterTest::addMaterialMultiple});

    addInstancedTests({&GltfSceneConverterTest::addMaterialInvalid},
        Containers::arraySize(AddMaterialInvalidData));

    addTests({&GltfSceneConverterTest::addMaterial2DArrayTextureLayerOutOfBounds});

    addInstancedTests({&GltfSceneConverterTest::textureCoordinateYFlip},
        Containers::arraySize(TextureCoordinateYFlipData));

    addInstancedTests({&GltfSceneConverterTest::addSceneEmpty},
        Containers::arraySize(AddSceneEmptyData));

    addInstancedTests({&GltfSceneConverterTest::addScene},
        Containers::arraySize(AddSceneData));

    addInstancedTests({
        &GltfSceneConverterTest::addSceneMeshesMaterials,
        &GltfSceneConverterTest::addSceneCustomFields,
        &GltfSceneConverterTest::addSceneNoParentField},
        Containers::arraySize(QuietData));

    addTests({&GltfSceneConverterTest::addSceneMultiple});

    addInstancedTests({&GltfSceneConverterTest::addSceneInvalid},
        Containers::arraySize(AddSceneInvalidData));

    addTests({&GltfSceneConverterTest::usedRequiredExtensionsAddedAlready,

              &GltfSceneConverterTest::toDataButExternalBuffer});

    _converterManager.registerExternalManager(_imageConverterManager);

    /* Load the importer plugin directly from the build tree. Otherwise it's
       static and already loaded. It also pulls in the AnyImageImporter
       dependency. */
    #ifdef GLTFIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(GLTFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    _importerManager.setPluginDirectory("nonexistent");
    #endif

    /* Load the plugins directly from the build tree. Otherwise they're static
       and already loaded. */
    #ifdef BASISIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_imageConverterManager.load(BASISIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef BASISIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(BASISIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef GLTFSCENECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(GLTFSCENECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef KTXIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_imageConverterManager.load(KTXIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef KTXIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(KTXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef OPENEXRIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_imageConverterManager.load(OPENEXRIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBDXTIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_imageConverterManager.load(STBDXTIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_imageConverterManager.load(STBIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif

    /* Try to load Magnum's own TgaImageConverter plugin, if it exists. Do it
       after StbImageConverter so if TgaImageConverter is aliased to it, it
       doesn't cause an "StbImageConverter.so conflicts with currently loaded
       plugin of the same name" error. */
    if(_imageConverterManager.loadState("TgaImageConverter") != PluginManager::LoadState::NotFound)
        _imageConverterManager.load("TgaImageConverter");
    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    _imageConverterManager.setPluginDirectory("nonexistent");
    #endif

    /* By default don't write the generator name for smaller test files.
       Remember the original value however, for the generatorVersion() test
       case. */
    Utility::ConfigurationGroup& configuration = CORRADE_INTERNAL_ASSERT_EXPRESSION(_converterManager.metadata("GltfSceneConverter"))->configuration();
    _originalGeneratorName = configuration.value<Containers::StringView>("generator");
    configuration.setValue("generator", "");
    if(PluginManager::PluginMetadata* metadata = _imageConverterManager.metadata("KtxImageConverter"))
        metadata->configuration().setValue("generator", "");

    /* Create the output directory if it doesn't exist yet */
    CORRADE_INTERNAL_ASSERT_OUTPUT(Utility::Path::make(GLTFSCENECONVERTER_TEST_OUTPUT_DIR));
}

void GltfSceneConverterTest::empty() {
    auto&& data = FileVariantData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->configuration().setValue("binary", data.binary);

    CORRADE_VERIFY(converter->beginData());
    Containers::Optional<Containers::Array<char>> out = converter->endData();
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "empty" + data.suffix),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    /* The file should load without errors */
    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openData(*out));
}

void GltfSceneConverterTest::outputFormatDetectionToData() {
    auto&& data = OutputFormatDetectionToDataData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    if(data.binary)
        converter->configuration().setValue("binary", *data.binary);

    CORRADE_VERIFY(converter->beginData());
    Containers::Optional<Containers::Array<char>> out = converter->endData();
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::StringToFile);

    /* File contents verified in empty() already, this just verifies that a
       correct output format was chosen */
}

void GltfSceneConverterTest::outputFormatDetectionToFile() {
    auto&& data = OutputFormatDetectionToFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    if(data.binary)
        converter->configuration().setValue("binary", *data.binary);

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "file" + data.suffix);

    CORRADE_VERIFY(converter->beginFile(filename));
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);

    /* File contents verified in empty() already, this just verifies that a
       correct output format was chosen */
}

void GltfSceneConverterTest::metadata() {
    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->configuration().setValue("binary", false);

    converter->configuration().setValue("generator", "I have bugs, do I?");
    converter->configuration().setValue("copyright", "© always, Me Mememe ME");
    converter->configuration().addValue("extensionUsed", "MAGNUM_exported_this_file");
    converter->configuration().addValue("extensionUsed", "MAGNUM_can_write_json");
    converter->configuration().addValue("extensionRequired", "MAGNUM_is_amazing");
    converter->configuration().addValue("extensionRequired", "MAGNUM_exported_this_file");

    CORRADE_VERIFY(converter->beginData());
    Containers::Optional<Containers::Array<char>> out = converter->endData();
    CORRADE_VERIFY(out);

    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "metadata.gltf"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    /* The file should load if we ignore required extensions */
    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    {
        Error silenceError{nullptr};
        CORRADE_VERIFY(!importer->openData(*out));
    }
    importer->configuration().setValue("ignoreRequiredExtensions", true);
    CORRADE_VERIFY(importer->openData(*out));
    /** @todo once ImporterExtraAttribute is a thing, verify these are parsed */
}

void GltfSceneConverterTest::generatorVersion() {
    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    /* Restore the original generator name that was emptied in the constructor
       for smaller test files */
    converter->configuration().setValue("generator", _originalGeneratorName);
    /* Get a formatted text file out, not a binary that's default for to-data
       output */
    converter->configuration().setValue("binary", false);

    CORRADE_VERIFY(converter->beginData());
    Containers::Optional<Containers::Array<char>> out = converter->endData();
    CORRADE_VERIFY(out);

    Containers::StringView string = *out;

    /* The formatting is tested thoroughly in VersionTest */
    CORRADE_COMPARE_AS(string,
        "\"generator\": \"Magnum GltfSceneConverter v",
        TestSuite::Compare::StringContains);

    /* Get everything until the next ". Eh what a terrible API!? */
    Containers::StringView found = string.find("\"generator\": \"");
    found = string.slice(found.end(), string.end());
    CORRADE_INFO("Generator string found:" << found.prefix(found.find("\"").begin()));
}

void GltfSceneConverterTest::abort() {
    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->configuration().setValue("binary", false);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "file.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    const Vector3 positions[1]{};
    CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Triangles, {}, positions, {
        MeshAttributeData{MeshAttribute::Position, Containers::arrayView(positions)}
    }}));

    /* Starting a new file should clean up the previous state */
    CORRADE_VERIFY(converter->beginData());
    Containers::Optional<Containers::Array<char>> out = converter->endData();
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "empty.gltf"),
        TestSuite::Compare::StringToFile);
}

void GltfSceneConverterTest::addMesh() {
    auto&& data = FileVariantWithNamesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* A simple case focusing mainly on metadata preservation. Organizing into
       buffer views is tested thoroughly in addMeshBufferViews*() below. */

    const struct Vertex {
        Vector3 position;
        Vector3 normal;
    } vertices[]{
        {{1.0f, 2.0f, 3.0f},
         {7.0f, 8.0f, 9.0f}},
        {{4.0f, 5.0f, 6.0f},
         {10.0f, 11.0f, 12.0f}}
    };

    const UnsignedInt indices[] {
        0xffff, 0xeeee, 0, 2, 1, 2, 1, 2, 0xaaaa
    };

    MeshData mesh{MeshPrimitive::Points,
        {}, indices, MeshIndexData{Containers::arrayView(indices).slice(2, 2 + 6)},
        {}, vertices, {
            MeshAttributeData{MeshAttribute::Position, Containers::stridedArrayView(vertices).slice(&Vertex::position)},
            MeshAttributeData{MeshAttribute::Normal, Containers::stridedArrayView(vertices).slice(&Vertex::normal)}
        }
    };

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    if(data.accessorNames) converter->configuration().setValue("accessorNames", true);
    else CORRADE_VERIFY(!converter->configuration().value<bool>("accessorNames"));

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh" + data.suffix);
    CORRADE_VERIFY(converter->beginFile(filename));
    CORRADE_VERIFY(converter->add(mesh, data.dataName));
    CORRADE_VERIFY(converter->endFile());

    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh" + data.suffix),
        TestSuite::Compare::File);
    /* The binary is identical independent of the options set */
    if(!data.binary) CORRADE_COMPARE_AS(
        Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh.bin"),
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh.bin"),
        TestSuite::Compare::File);

    /* Verify various expectations that might be missed when just looking at
       the file */
    const Containers::Optional<Containers::String> gltf = Utility::Path::readString(filename);
    CORRADE_VERIFY(gltf);
    /* No extensions are needed for this simple case */
    CORRADE_VERIFY(!gltf->contains("extensionsUsed"));
    CORRADE_VERIFY(!gltf->contains("extensionsRequired"));
    /* If unnamed, there should be no name field */
    CORRADE_COMPARE(gltf->contains("name"), data.dataName ||data.accessorNames);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one mesh with what we have above */
    CORRADE_COMPARE(importer->meshCount(), 1);
    if(data.dataName) {
        CORRADE_COMPARE(importer->meshName(0), data.dataName);
        CORRADE_COMPARE(importer->meshForName(data.dataName), 0);
    }
    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->primitive(), MeshPrimitive::Points);

    CORRADE_COMPARE(imported->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE_AS(imported->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 2, 1, 2, 1, 2}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(imported->attributeCount(), 2);
    /* The attributes are sorted by name by the importer to handle duplicates */
    CORRADE_COMPARE(imported->attributeName(1), MeshAttribute::Position);
    CORRADE_COMPARE(imported->attributeName(0), MeshAttribute::Normal);
    CORRADE_COMPARE(imported->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE(imported->attributeFormat(MeshAttribute::Normal), VertexFormat::Vector3);
    CORRADE_COMPARE(imported->attributeStride(MeshAttribute::Position), sizeof(Vertex));
    CORRADE_COMPARE(imported->attributeStride(MeshAttribute::Normal), sizeof(Vertex));
    CORRADE_COMPARE_AS(imported->attribute<Vector3>(MeshAttribute::Position),
        Containers::stridedArrayView(vertices).slice(&Vertex::position),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->attribute<Vector3>(MeshAttribute::Normal),
        Containers::stridedArrayView(vertices).slice(&Vertex::normal),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addMeshBufferViewsNonInterleaved() {
    struct Vertices {
        Vector3 positions[2];
        Vector2ub textureCoordinates[2];
        UnsignedInt padding;
        Color4ub colors[2];
    } vertices[]{{
        {{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}},
        {{63, 127}, {191, 255}},
        0xffeeffeeu,
        {0x11223344_rgba, 0x55667788_rgba}
    }};
    MeshData mesh{MeshPrimitive::Lines, {}, vertices, {
        /* Even with mixed up order the buffer views should be written with the
           lowest offset first */
        MeshAttributeData{MeshAttribute::TextureCoordinates, VertexFormat::Vector2ubNormalized, Containers::arrayView(vertices->textureCoordinates)},
        MeshAttributeData{MeshAttribute::Position, Containers::arrayView(vertices->positions)},
        MeshAttributeData{MeshAttribute::Color, Containers::arrayView(vertices->colors)},
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-buffer-views-noninterleaved.gltf");

    CORRADE_VERIFY(converter->beginFile(filename));
    CORRADE_VERIFY(converter->add(mesh));
    CORRADE_VERIFY(converter->endFile());

    /* There should be three buffer views for three accessors and should have a
       4-byte gap between second and third buffer view */
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-buffer-views-noninterleaved.gltf"),
        TestSuite::Compare::File);
    CORRADE_COMPARE_AS(
        Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-buffer-views-noninterleaved.bin"),
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-buffer-views-noninterleaved.bin"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    CORRADE_COMPARE(importer->meshCount(), 1);

    /* The data should be exactly the same size, attributes with same offsets
       and strides */
    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->attributeCount(), 3);

    CORRADE_COMPARE(imported->attributeOffset(MeshAttribute::Position),
        offsetof(Vertices, positions));
    CORRADE_COMPARE(imported->attributeStride(MeshAttribute::Position),
        sizeof(Vector3));
    CORRADE_COMPARE_AS(imported->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView(vertices->positions),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(imported->attributeOffset(MeshAttribute::TextureCoordinates),
        offsetof(Vertices, textureCoordinates));
    CORRADE_COMPARE(imported->attributeStride(MeshAttribute::TextureCoordinates),
        sizeof(Vector2ub));
    CORRADE_COMPARE_AS(imported->attribute<Vector2ub>(MeshAttribute::TextureCoordinates),
        Containers::arrayView(vertices->textureCoordinates),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(imported->attributeOffset(MeshAttribute::Color),
        offsetof(Vertices, colors));
    CORRADE_COMPARE(imported->attributeStride(MeshAttribute::Color),
        sizeof(Color4ub));
    CORRADE_COMPARE_AS(imported->attribute<Color4ub>(MeshAttribute::Color),
        Containers::arrayView(vertices->colors),
        TestSuite::Compare::Container);

    /* And finally, this should match too */
    CORRADE_COMPARE_AS(imported->vertexData(),
        Containers::arrayCast<const char>(Containers::arrayView(vertices)),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addMeshBufferViewsInterleavedPaddingBegin() {
    struct Vertices {
        Color4ub colors[2];
        struct Interleaved {
            UnsignedInt padding;
            Vector3 positionNormal;
            Vector2 textureCoordinates;
        } interleaved[2];
    } vertices[]{{
        {0x11223344_rgba, 0x55667788_rgba},
        {{0xffeeffeeu, {1.0f, 2.0f, 3.0f}, {0.25f, 0.75f}},
         {0xddccddccu, {4.0f, 5.0f, 6.0f}, {0.5f, 1.0f}}},
    }};
    auto interleaved = Containers::stridedArrayView(vertices->interleaved);
    MeshData mesh{MeshPrimitive::LineStrip, {}, vertices, {
        /* Even with mixed up order the buffer views should be written with the
           lowest offset first */
        MeshAttributeData{MeshAttribute::TextureCoordinates, interleaved.slice(&Vertices::Interleaved::textureCoordinates)},
        /* Aliases the same data. Shouldn't cause any issues or randomness in
           the output due to std::sort() not being stable across STL
           implementations. */
        MeshAttributeData{MeshAttribute::Position, interleaved.slice(&Vertices::Interleaved::positionNormal)},
        MeshAttributeData{MeshAttribute::Normal, interleaved.slice(&Vertices::Interleaved::positionNormal)},
        /* A non-interleaved attribute at the beginning. The strided view
           should not get shifted to overlap it. */
        MeshAttributeData{MeshAttribute::Color, Containers::arrayView(vertices->colors)},
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-buffer-views-interleaved-padding-begin.gltf");

    CORRADE_VERIFY(converter->beginFile(filename));
    CORRADE_VERIFY(converter->add(mesh));
    CORRADE_VERIFY(converter->endFile());

    /* There should be two buffer views for four accessors, with positions and
       normals having a 4-byte offset */
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-buffer-views-interleaved-padding-begin.gltf"),
        TestSuite::Compare::File);
    CORRADE_COMPARE_AS(
        Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-buffer-views-interleaved-padding-begin.bin"),
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-buffer-views-interleaved-padding-begin.bin"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    CORRADE_COMPARE(importer->meshCount(), 1);

    /* The data should be exactly the same size, attributes with same offsets
       and strides */
    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->attributeCount(), 4);

    /* These two are the same */
    for(MeshAttribute attribute: {MeshAttribute::Position, MeshAttribute::Normal}) {
        CORRADE_ITERATION(attribute);
        CORRADE_COMPARE(imported->attributeOffset(attribute),
            offsetof(Vertices, interleaved) +
            offsetof(Vertices::Interleaved, positionNormal));
        CORRADE_COMPARE(imported->attributeStride(attribute),
            sizeof(Vertices::Interleaved));
        CORRADE_COMPARE_AS(imported->attribute<Vector3>(attribute),
            interleaved.slice(&Vertices::Interleaved::positionNormal),
            TestSuite::Compare::Container);
    }

    CORRADE_COMPARE(imported->attributeOffset(MeshAttribute::TextureCoordinates),
        offsetof(Vertices, interleaved) +
        offsetof(Vertices::Interleaved, textureCoordinates));
    CORRADE_COMPARE(imported->attributeStride(MeshAttribute::TextureCoordinates),
        sizeof(Vertices::Interleaved));
    CORRADE_COMPARE_AS(imported->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        interleaved.slice(&Vertices::Interleaved::textureCoordinates),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(imported->attributeOffset(MeshAttribute::Color),
        offsetof(Vertices, colors));
    CORRADE_COMPARE(imported->attributeStride(MeshAttribute::Color),
        sizeof(Color4ub));
    CORRADE_COMPARE_AS(imported->attribute<Color4ub>(MeshAttribute::Color),
        Containers::arrayView(vertices->colors),
        TestSuite::Compare::Container);

    /* And finally, this should match too */
    CORRADE_COMPARE_AS(imported->vertexData(),
        Containers::arrayCast<const char>(Containers::arrayView(vertices)),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addMeshBufferViewsInterleavedPaddingBeginEnd() {
    auto&& data = VerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    struct Vertex {
        UnsignedInt padding1;
        Vector3 position;
        Vector2 textureCoordinates;
        UnsignedInt padding2;
    } vertices[]{
        {0xffeeffeeu, {1.0f, 2.0f, 3.0f}, {0.25f, 0.75f}, 0xeeffeeffu},
        {0xddccddccu, {4.0f, 5.0f, 6.0f}, {0.5f, 1.0f}, 0xccddccddu},
    };
    auto view = Containers::stridedArrayView(vertices);
    /* MeshData doesn't require the end padding to be present, cut it away.
       The glTF exporter will then need to add it back. */
    MeshData mesh{MeshPrimitive::LineLoop, {}, Containers::arrayCast<char>(Containers::arrayView(vertices)).exceptSuffix(4), {
        /* Again arbitrary mixed up order */
        MeshAttributeData{MeshAttribute::TextureCoordinates, view.slice(&Vertex::textureCoordinates)},
        MeshAttributeData{MeshAttribute::Position, view.slice(&Vertex::position)},
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(data.flags);

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-buffer-views-interleaved-padding-begin-end.gltf");

    CORRADE_VERIFY(converter->beginFile(filename));
    std::ostringstream out;
    {
        Debug redirectOutput{&out};
        CORRADE_VERIFY(converter->add(mesh));
    }
    if(data.verbose)
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): vertex buffer was padded by 4 bytes to satisfy glTF buffer view requirements\n");
    else
        CORRADE_COMPARE(out.str(), "");
    CORRADE_VERIFY(converter->endFile());

    /* There should be one buffer view starting at offset 0, the position
       accessor starting at offset 4 */
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-buffer-views-interleaved-padding-begin-end.gltf"),
        TestSuite::Compare::File);
    CORRADE_COMPARE_AS(
        Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-buffer-views-interleaved-padding-begin-end.bin"),
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-buffer-views-interleaved-padding-begin-end.bin"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    CORRADE_COMPARE(importer->meshCount(), 1);

    /* The data should be exactly the same size, attributes with same offsets
       and strides */
    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->attributeCount(), 2);

    CORRADE_COMPARE(imported->attributeOffset(MeshAttribute::Position),
        offsetof(Vertex, position));
    CORRADE_COMPARE(imported->attributeStride(MeshAttribute::Position),
        sizeof(Vertex));
    CORRADE_COMPARE_AS(imported->attribute<Vector3>(MeshAttribute::Position),
        view.slice(&Vertex::position),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(imported->attributeOffset(MeshAttribute::TextureCoordinates),
        offsetof(Vertex, textureCoordinates));
    CORRADE_COMPARE(imported->attributeStride(MeshAttribute::TextureCoordinates),
        sizeof(Vertex));
    CORRADE_COMPARE_AS(imported->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        view.slice(&Vertex::textureCoordinates),
        TestSuite::Compare::Container);

    /* And finally, this should match too -- except for the padding, which is
       zero-filled */
    CORRADE_COMPARE(imported->vertexData().size(), mesh.vertexData().size() + 4);
    CORRADE_COMPARE_AS(imported->vertexData().exceptSuffix(4),
        Containers::arrayCast<const char>(Containers::arrayView(vertices)).exceptSuffix(4),
        TestSuite::Compare::Container);
    /** @todo use suffix() once it takes suffix length */
    CORRADE_COMPARE_AS(imported->vertexData().exceptPrefix(imported->vertexData().size() - 4),
        Containers::arrayView({'\0', '\0', '\0', '\0'}),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addMeshBufferViewsMixed() {
    /* A combination of interleaved and non-interleaved data */
    const struct Vertices {
        /* Buffer view 0, 1 accessor */
        Vector3 positions[2];
        /* Buffer view 1, 2 accessors */
        struct Interleaved1 {
            UnsignedByte jointIds[4];
            UnsignedShort weights[4];
        } interleaved1[2];
        /* Buffer view 2, 1 accessor */
        Vector2 textureCoordinates[2];
        /* Buffer view 3, 2 accessors */
        struct Interleaved2 {
            Color4ub color;
            /* Last three items here and the immediately following byte is
               secondary weights. Because they go over the stride, they should
               be put into a dedicated buffer view (number 4) */
            UnsignedShort secondaryJointIdsAndFirstThreeWeights[4];
        } interleaved2[2];
        UnsignedShort lastSecondaryWeight;
        /* Buffer view 5 */
        UnsignedShort objectIds[2];
    } vertices[]{{
        {{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}},
        {{{1, 2, 3, 4}, {100, 200, 300, 400}},
         {{5, 6, 7, 8}, {500, 600, 700, 800}}},
        {{0.1f, 0.2f}, {0.3f, 0.4f}},
        {{0xaabbccdd_rgba, {1000, 2000, 3000, 4000}},
         {0xeeff0011_rgba, {5000, 6000, 7000, 8000}}},
        9000,
        {123, 213}
    }};
    auto interleaved1 = Containers::stridedArrayView(vertices->interleaved1);
    auto interleaved2 = Containers::stridedArrayView(vertices->interleaved2);
    MeshData mesh{MeshPrimitive::Lines, {}, vertices, {
        MeshAttributeData{MeshAttribute::Position,
            Containers::arrayView(vertices->positions)},
        MeshAttributeData{MeshAttribute::JointIds,
            VertexFormat::UnsignedByte, interleaved1.slice(&Vertices::Interleaved1::jointIds), 4},
        MeshAttributeData{MeshAttribute::Weights,
            VertexFormat::UnsignedShortNormalized, interleaved1.slice(&Vertices::Interleaved1::weights), 4},
        MeshAttributeData{MeshAttribute::TextureCoordinates,
            Containers::arrayView(vertices->textureCoordinates)},
        MeshAttributeData{MeshAttribute::Color,
            VertexFormat::Vector4ubNormalized,
            interleaved2.slice(&Vertices::Interleaved2::color)},
        MeshAttributeData{MeshAttribute::JointIds,
            VertexFormat::UnsignedShort, interleaved2.slice(&Vertices::Interleaved2::secondaryJointIdsAndFirstThreeWeights), 4},
        /* Offset-only as it goes over the stride */
        MeshAttributeData{MeshAttribute::Weights,
            VertexFormat::UnsignedShortNormalized,
            offsetof(Vertices, interleaved2) +
            offsetof(Vertices::Interleaved2, secondaryJointIdsAndFirstThreeWeights) + sizeof(UnsignedShort),
            2, sizeof(Vertices::Interleaved2), 4},
        MeshAttributeData{MeshAttribute::ObjectId,
            Containers::arrayView(vertices->objectIds)},
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-buffer-views-mixed.gltf");

    CORRADE_VERIFY(converter->beginFile(filename));
    CORRADE_VERIFY(converter->add(mesh));
    CORRADE_VERIFY(converter->endFile());

    /* There should be 6 buffer views for 8 accessors */
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-buffer-views-mixed.gltf"),
        TestSuite::Compare::File);
    CORRADE_COMPARE_AS(
        Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-buffer-views-mixed.bin"),
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-buffer-views-mixed.bin"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    /** @todo drop once this is gone */
    importer->configuration().setValue("compatibilitySkinningAttributes", false);
    CORRADE_VERIFY(importer->openFile(filename));

    CORRADE_COMPARE(importer->meshCount(), 1);

    /* The data should be exactly the same size, attributes with same offsets
       and strides. Test just the data as those are sufficiently random, if
       there's something really wrong it would be caught by the tests above. */
    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->attributeCount(), 8);
    CORRADE_COMPARE_AS(imported->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView(vertices->positions),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4ub>(imported->attribute(MeshAttribute::JointIds, 0))),
        Containers::arrayCast<const Vector4ub>(interleaved1.slice(&Vertices::Interleaved1::jointIds)),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4us>(imported->attribute(MeshAttribute::Weights, 0))),
        Containers::arrayCast<const Vector4us>(interleaved1.slice(&Vertices::Interleaved1::weights)),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        Containers::arrayView(vertices->textureCoordinates),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->attribute<Color4ub>(MeshAttribute::Color),
        interleaved2.slice(&Vertices::Interleaved2::color),
        TestSuite::Compare::Container);

    /* Verifying these manually to be sure about what's happening -- there's
       overlap on three items */
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4us>(imported->attribute(MeshAttribute::JointIds, 1))), Containers::arrayView<Vector4us>({
        {1000, 2000, 3000, 4000},
        {5000, 6000, 7000, 8000},
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4us>(imported->attribute(MeshAttribute::Weights, 1))), Containers::arrayView<Vector4us>({
        {2000, 3000, 4000, 0xffee},
        {6000, 7000, 8000, 9000},
    }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(imported->attribute<UnsignedShort>(MeshAttribute::ObjectId),
        Containers::arrayView(vertices->objectIds),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addMeshNoAttributes() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    const UnsignedByte indices[] {
        0, 2, 1, 2
    };

    MeshData mesh{MeshPrimitive::LineStrip,
        {}, indices, MeshIndexData{Containers::arrayView(indices)}, 0
    };

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    /* Attribute-less meshes are not valid glTF, but we accept that under a
       flag */
    converter->configuration().setValue("strict", false);

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-no-attributes.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(mesh));
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): strict mode disabled, allowing an attribute-less mesh\n");
    }

    CORRADE_VERIFY(converter->endFile());

    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-no-attributes.gltf"),
        TestSuite::Compare::File);
    /* The bin file should be just the indices array from above */
    CORRADE_COMPARE_AS(
        Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-no-attributes.bin"),
        (Containers::StringView{reinterpret_cast<const char*>(indices), sizeof(indices)}),
        TestSuite::Compare::FileToString);

    /* Verify various expectations that might be missed when just looking at
       the file */
    const Containers::Optional<Containers::String> gltf = Utility::Path::readString(filename);
    CORRADE_VERIFY(gltf);
    /* No extensions are needed for this simple case */
    CORRADE_VERIFY(!gltf->contains("extensionsUsed"));
    CORRADE_VERIFY(!gltf->contains("extensionsRequired"));

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one mesh with what we have above */
    CORRADE_COMPARE(importer->meshCount(), 1);
    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->primitive(), MeshPrimitive::LineStrip);

    CORRADE_VERIFY(imported->isIndexed());
    CORRADE_COMPARE(imported->indexType(), MeshIndexType::UnsignedByte);
    CORRADE_COMPARE_AS(imported->indices<UnsignedByte>(),
        Containers::arrayView<UnsignedByte>({0, 2, 1, 2}),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addMeshNoIndices() {
    const Vector3 positions[] {
        {1.0f, 2.0f, 3.0f},
        {4.0f, 5.0f, 6.0f},
        {7.0f, 8.0f, 9.0f}
    };

    MeshData mesh{MeshPrimitive::Triangles,
        {}, positions, {MeshAttributeData{MeshAttribute::Position, Containers::arrayView(positions)}}
    };

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-no-indices.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));
    CORRADE_VERIFY(converter->add(mesh));
    CORRADE_VERIFY(converter->endFile());

    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-no-indices.gltf"),
        TestSuite::Compare::File);
    /* The bin file should be just the positions array from above */
    CORRADE_COMPARE_AS(
        Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-no-indices.bin"),
        (Containers::StringView{reinterpret_cast<const char*>(positions), sizeof(positions)}),
        TestSuite::Compare::FileToString);

    /* Verify various expectations that might be missed when just looking at
       the file */
    const Containers::Optional<Containers::String> gltf = Utility::Path::readString(filename);
    CORRADE_VERIFY(gltf);
    /* No extensions are needed for this simple case */
    CORRADE_VERIFY(!gltf->contains("extensionsUsed"));
    CORRADE_VERIFY(!gltf->contains("extensionsRequired"));

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one mesh with what we have above */
    CORRADE_COMPARE(importer->meshCount(), 1);
    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->primitive(), MeshPrimitive::Triangles);
    CORRADE_VERIFY(!imported->isIndexed());
    CORRADE_COMPARE(imported->attributeCount(), 1);
    CORRADE_COMPARE(imported->attributeName(0), MeshAttribute::Position);
    CORRADE_COMPARE(imported->attributeFormat(0), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(imported->attribute<Vector3>(0),
        Containers::arrayView(positions),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addMeshNoIndicesNoAttributes() {
    auto&& data = FileVariantStrictWarningData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(data.flags);

    /* Attribute-less meshes are not valid glTF, but we accept that under a
       flag */
    converter->configuration().setValue("strict", false);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-no-indices-no-attributes" + data.suffix);
    CORRADE_VERIFY(converter->beginFile(filename));

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::TriangleFan, 0}));
    }
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): strict mode disabled, allowing an attribute-less mesh\n");

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-no-indices-no-attributes" + data.suffix),
        TestSuite::Compare::File);
    /* There should be no (empty) bin file written */
    if(!data.binary)
        CORRADE_VERIFY(!Utility::Path::exists(Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-no-indices-no-attributes.bin")));

    /* Verify various expectations that might be missed when just looking at
       the file */
    const Containers::Optional<Containers::String> gltf = Utility::Path::readString(filename);
    CORRADE_VERIFY(gltf);
    /* No buffer, view or accessor should be referenced */
    CORRADE_VERIFY(!gltf->contains("buffers"));
    CORRADE_VERIFY(!gltf->contains("bufferViews"));
    CORRADE_VERIFY(!gltf->contains("accessors"));

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->primitive(), MeshPrimitive::TriangleFan);
    CORRADE_VERIFY(!imported->isIndexed());
    CORRADE_COMPARE(imported->vertexCount(), 0);
    CORRADE_COMPARE(imported->attributeCount(), 0);
}

void GltfSceneConverterTest::addMeshNoIndicesNoVertices() {
    auto&& data = FileVariantStrictWarningData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    MeshData mesh{MeshPrimitive::TriangleStrip, nullptr, {
        MeshAttributeData{MeshAttribute::Position, VertexFormat::Vector3, 0, 0, sizeof(Vector3)}
    }, 0};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(data.flags);

    /* Vertex-less meshes are not valid glTF, but we accept that under a
       flag */
    converter->configuration().setValue("strict", false);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-no-indices-no-vertices" + data.suffix);
    CORRADE_VERIFY(converter->beginFile(filename));

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(mesh));
    }
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): strict mode disabled, allowing a mesh with zero vertices\n");

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-no-indices-no-vertices" + data.suffix),
        TestSuite::Compare::File);
    /* There should be no (empty) bin file written */
    if(!data.binary)
        CORRADE_VERIFY(!Utility::Path::exists(Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-no-indices-no-vertices.bin")));

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->primitive(), MeshPrimitive::TriangleStrip);
    CORRADE_VERIFY(!imported->isIndexed());
    CORRADE_COMPARE(imported->vertexCount(), 0);
    CORRADE_COMPARE(imported->attributeCount(), 1);
    CORRADE_COMPARE(imported->attributeName(0), MeshAttribute::Position);
    CORRADE_COMPARE(imported->attributeFormat(0), VertexFormat::Vector3);
    CORRADE_COMPARE(imported->attributeStride(0), sizeof(Vector3));
}

void GltfSceneConverterTest::addMeshAttribute() {
    auto&& data = AddMeshAttributeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    const char vertices[32]{};
    MeshData mesh{MeshPrimitive::LineLoop, {}, vertices, {
        MeshAttributeData{data.attribute, data.format, 0, 1, 32}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->setFlags(data.flags);

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);
    CORRADE_VERIFY(converter->beginFile(filename));

    if(data.customName)
        converter->setMeshAttributeName(data.attribute, data.customName);
    if(data.strict)
        converter->configuration().setValue("strict", *data.strict);
    if(data.textureCoordinateYFlipInMaterial)
        converter->configuration().setValue("textureCoordinateYFlipInMaterial", *data.textureCoordinateYFlipInMaterial);

    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(mesh));
        if(data.expectedWarning)
            CORRADE_COMPARE(out.str(), Utility::format("Trade::GltfSceneConverter::add(): {}\n", data.expectedWarning));
        else CORRADE_COMPARE(out.str(), "");
    }

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);

    /* Verify various expectations that might be missed when just looking at
       the file */
    const Containers::Optional<Containers::String> gltf = Utility::Path::readString(filename);
    CORRADE_VERIFY(gltf);
    if(data.expectedKhrMeshQuantization) {
        CORRADE_VERIFY(gltf->contains("extensionsUsed"));
        CORRADE_VERIFY(gltf->contains("extensionsRequired"));
        CORRADE_VERIFY(gltf->contains("KHR_mesh_quantization"));
    } else {
        CORRADE_VERIFY(!gltf->contains("extensionsUsed"));
        CORRADE_VERIFY(!gltf->contains("extensionsRequired"));
    }

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one mesh with what we have above */
    CORRADE_COMPARE(importer->meshCount(), 1);
    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->primitive(), MeshPrimitive::LineLoop);

    CORRADE_COMPARE(imported->attributeCount(), 1);
    if(!data.expectCustomName)
        CORRADE_COMPARE(imported->attributeName(0), data.attribute);
    else
        CORRADE_COMPARE(importer->meshAttributeName(imported->attributeName(0)), data.expectCustomName);
    CORRADE_COMPARE(imported->attributeFormat(0), data.format);
}

void GltfSceneConverterTest::addMeshSkinningAttributes() {
    auto&& data = AddMeshSkinningAttributesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    const struct Vertex {
        /* The attributes are deliberately shuffled to avoid accidental order
           assumptions in the code */
        UnsignedShort jointIds[4];
        Vector3 position;
        UnsignedByte weights[4];
        UnsignedShort secondaryWeights[8];
        UnsignedByte secondaryJointIds[8];
        /* UnsignedInt + Float tested in addMeshSkinningAttributesUnsignedInt()
           below */
    } vertices[]{
        {{3, 5, 7, 9},
         {1.0f, 2.0f, 3.0f},
         {16, 32, 64, 128},
         {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000},
         {10, 20, 30, 40, 50, 60, 70, 80}},
        {{2, 4, 6, 8},
         {4.0f, 5.0f, 6.0f},
         {48, 96, 144, 192},
         {9000, 10000, 11000, 12000, 13000, 14000, 15000, 16000},
         {90, 100, 110, 120, 130, 140, 150, 160}}
    };
    auto view = Containers::stridedArrayView(vertices);

    Containers::Array<MeshAttributeData> attributes{InPlaceInit, {
        MeshAttributeData{MeshAttribute::JointIds,
            VertexFormat::UnsignedShort,
            view.slice(&Vertex::jointIds), 4},
        MeshAttributeData{MeshAttribute::Position,
            view.slice(&Vertex::position)},
        MeshAttributeData{MeshAttribute::Weights,
            VertexFormat::UnsignedByteNormalized,
            view.slice(&Vertex::weights), 4},
        MeshAttributeData{MeshAttribute::Weights,
            VertexFormat::UnsignedShortNormalized,
            view.slice(&Vertex::secondaryWeights), 8},
        MeshAttributeData{MeshAttribute::JointIds,
            VertexFormat::UnsignedByte,
            view.slice(&Vertex::secondaryJointIds), 8}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-skinning-attributes.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* These should get ignored during export */
    if(data.compatibilityAttributes) {
        arrayAppend(attributes, {
            MeshAttributeData{meshAttributeCustom(667),
                VertexFormat::Vector4ub,
                view.slice(&Vertex::secondaryJointIds)},
            MeshAttributeData{meshAttributeCustom(776),
                VertexFormat::Vector4usNormalized,
                view.slice(&Vertex::secondaryWeights)},
        });
        converter->setMeshAttributeName(meshAttributeCustom(667), "JOINTS");
        converter->setMeshAttributeName(meshAttributeCustom(776), "WEIGHTS");
    }

    CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Lines, {}, vertices, std::move(attributes)}));
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-skinning-attributes.gltf"),
        TestSuite::Compare::File);
    CORRADE_COMPARE_AS(
        Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-skinning-attributes.bin"),
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-skinning-attributes.bin"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    /** @todo drop once this is gone */
    importer->configuration().setValue("compatibilitySkinningAttributes", false);

    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->attributeCount(), 7);
    CORRADE_COMPARE(imported->attributeCount(MeshAttribute::JointIds), 3);
    CORRADE_COMPARE(imported->attributeCount(MeshAttribute::Weights), 3);

    /* Positions, just to ensure the others don't break them */
    CORRADE_VERIFY(imported->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE_AS(imported->attribute<Vector3>(MeshAttribute::Position),
        view.slice(&Vertex::position),
        TestSuite::Compare::Container);

    /* First set of skinning attributes should be added as a whole */
    CORRADE_COMPARE(imported->attributeFormat(MeshAttribute::JointIds, 0), VertexFormat::UnsignedShort);
    CORRADE_COMPARE(imported->attributeFormat(MeshAttribute::Weights, 0), VertexFormat::UnsignedByteNormalized);
    CORRADE_COMPARE(imported->attributeArraySize(MeshAttribute::JointIds, 0), 4);
    CORRADE_COMPARE(imported->attributeArraySize(MeshAttribute::Weights, 0), 4);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4us>(imported->attribute(MeshAttribute::JointIds, 0))),
        Containers::arrayCast<const Vector4us>(view.slice(&Vertex::jointIds)),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4ub>(imported->attribute(MeshAttribute::Weights, 0))),
        Containers::arrayCast<const Vector4ub>(view.slice(&Vertex::weights)),
        TestSuite::Compare::Container);

    /* Second set split into two */
    CORRADE_COMPARE(imported->attributeFormat(MeshAttribute::JointIds, 1), VertexFormat::UnsignedByte);
    CORRADE_COMPARE(imported->attributeFormat(MeshAttribute::JointIds, 2), VertexFormat::UnsignedByte);
    CORRADE_COMPARE(imported->attributeFormat(MeshAttribute::Weights, 1), VertexFormat::UnsignedShortNormalized);
    CORRADE_COMPARE(imported->attributeFormat(MeshAttribute::Weights, 2), VertexFormat::UnsignedShortNormalized);
    CORRADE_COMPARE(imported->attributeArraySize(MeshAttribute::JointIds, 1), 4);
    CORRADE_COMPARE(imported->attributeArraySize(MeshAttribute::JointIds, 2), 4);
    CORRADE_COMPARE(imported->attributeArraySize(MeshAttribute::Weights, 1), 4);
    CORRADE_COMPARE(imported->attributeArraySize(MeshAttribute::Weights, 2), 4);
    /** @todo ffs, add strided array view constructors from multidimensional
        arrays, this is horrific */
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4ub>(imported->attribute(MeshAttribute::JointIds, 1))),
        (Containers::arrayCast<1, const Vector4ub>(Containers::arrayCast<2, const UnsignedByte>(view.slice(&Vertex::secondaryJointIds)).exceptSuffix({0, 4}))),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4ub>(imported->attribute(MeshAttribute::JointIds, 2))),
        (Containers::arrayCast<1, const Vector4ub>(Containers::arrayCast<2, const UnsignedByte>(view.slice(&Vertex::secondaryJointIds)).exceptPrefix({0, 4}))),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4us>(imported->attribute(MeshAttribute::Weights, 1))),
        (Containers::arrayCast<1, const Vector4us>(Containers::arrayCast<2, const UnsignedShort>(view.slice(&Vertex::secondaryWeights)).exceptSuffix({0, 4}))),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4us>(imported->attribute(MeshAttribute::Weights, 2))),
        (Containers::arrayCast<1, const Vector4us>(Containers::arrayCast<2, const UnsignedShort>(view.slice(&Vertex::secondaryWeights)).exceptPrefix({0, 4}))),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addMeshSkinningAttributesUnsignedInt() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    const struct Vertex {
        Vector3 position;
        UnsignedInt jointIds[4];
        Float weights[4];
    } vertices[]{
        {{1.0f, 2.0f, 3.0f},
         {1, 2, 3, 4},
         {0.125f, 0.25f, 0.375f, 0.5f}},
        {{1.0f, 2.0f, 3.0f},
         {5, 6, 7, 8},
         {0.625f, 0.75f, 0.875f, 1.0f}}
    };
    auto view = Containers::stridedArrayView(vertices);

    MeshData mesh{MeshPrimitive::Lines, {}, vertices, {
        MeshAttributeData{MeshAttribute::Position,
            view.slice(&Vertex::position)},
        MeshAttributeData{MeshAttribute::JointIds,
            VertexFormat::UnsignedInt,
            view.slice(&Vertex::jointIds), 4},
        MeshAttributeData{MeshAttribute::Weights,
            VertexFormat::Float,
            view.slice(&Vertex::weights), 4},
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(data.flags);
    /* Behavior with strict=true tested in
       addMeshInvalid(32-bit skin joint IDs, strict) */
    converter->configuration().setValue("strict", false);

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-skinning-attributes-ui.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(mesh));
    }
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        /* It's not JOINTS_0 because the warning happens before the final
           attribute name is composed but that should be fine */
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): strict mode disabled, allowing a 32-bit integer attribute JOINTS\n");

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-skinning-attributes-ui.gltf"),
        TestSuite::Compare::File);
    /* There's not really anything special to test in the bin file, it's
       verified thoroughly enough for other formats in
       addMeshSkinningAttributes() above */

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    /** @todo drop once this is gone */
    importer->configuration().setValue("compatibilitySkinningAttributes", false);

    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->attributeCount(), 3);
    CORRADE_COMPARE(imported->attributeCount(MeshAttribute::JointIds), 1);
    CORRADE_COMPARE(imported->attributeCount(MeshAttribute::Weights), 1);

    CORRADE_VERIFY(imported->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE_AS(imported->attribute<Vector3>(MeshAttribute::Position),
        view.slice(&Vertex::position),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(imported->attributeFormat(MeshAttribute::JointIds), VertexFormat::UnsignedInt);
    CORRADE_COMPARE(imported->attributeFormat(MeshAttribute::Weights), VertexFormat::Float);
    CORRADE_COMPARE(imported->attributeArraySize(MeshAttribute::JointIds), 4);
    CORRADE_COMPARE(imported->attributeArraySize(MeshAttribute::Weights), 4);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4ui>(imported->attribute(MeshAttribute::JointIds))),
        Containers::arrayCast<const Vector4ui>(view.slice(&Vertex::jointIds)),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4>(imported->attribute(MeshAttribute::Weights))),
        Containers::arrayCast<const Vector4>(view.slice(&Vertex::weights)),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addMeshDuplicateAttribute() {
    const Vector4 vertices[3]{};
    const MeshAttribute customAttribute = meshAttributeCustom(0);

    MeshData mesh{MeshPrimitive::TriangleFan, {}, vertices, {
        /* Builtin non-numbered attribute, should have no number */
        MeshAttributeData{MeshAttribute::Position,
            VertexFormat::Vector3, Containers::stridedArrayView(vertices)},
        /* Custom non-numbered attribute, should have no number */
        MeshAttributeData{MeshAttribute::ObjectId,
            VertexFormat::UnsignedShort, Containers::stridedArrayView(vertices)},
        /* Builtin numbered attributes, should have a number*/
        MeshAttributeData{MeshAttribute::TextureCoordinates,
            VertexFormat::Vector2, Containers::stridedArrayView(vertices)},
        MeshAttributeData{MeshAttribute::Color,
            VertexFormat::Vector4, Containers::stridedArrayView(vertices)},
        /* Custom attribute, should have no number */
        MeshAttributeData{customAttribute,
            VertexFormat::Float, Containers::stridedArrayView(vertices)},

        /* All below should have numbers */

        /* Secondary builtin numbered attributes */
        MeshAttributeData{MeshAttribute::TextureCoordinates,
            VertexFormat::Vector2usNormalized, Containers::stridedArrayView(vertices)},
        MeshAttributeData{MeshAttribute::Color,
            VertexFormat::Vector3ubNormalized, Containers::stridedArrayView(vertices)},
        /* Tertiary builtin numbered attributes */
        MeshAttributeData{MeshAttribute::TextureCoordinates,
            VertexFormat::Vector2ubNormalized, Containers::stridedArrayView(vertices)},
        /* Secondary builtin non-numbered attribute */
        MeshAttributeData{MeshAttribute::Position, VertexFormat::Vector3,
            Containers::stridedArrayView(vertices)},
        /* Secondary custom non-numbered attribute */
        MeshAttributeData{MeshAttribute::ObjectId,
            VertexFormat::UnsignedByte, Containers::stridedArrayView(vertices)},
        /* Secondary custom attribute */
        MeshAttributeData{customAttribute,
            VertexFormat::ByteNormalized, Containers::stridedArrayView(vertices)},
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-duplicate-attribute.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));
    converter->setMeshAttributeName(customAttribute, "_YOLO");
    CORRADE_VERIFY(converter->add(mesh));
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-duplicate-attribute.gltf"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_COMPARE(importer->meshCount(), 1);

    const MeshAttribute importedSecondaryPositionAttribute = importer->meshAttributeForName("_POSITION_1");
    const MeshAttribute importedSecondaryObjectIdAttribute = importer->meshAttributeForName("_OBJECT_ID_1");
    const MeshAttribute importedCustomAttribute = importer->meshAttributeForName("_YOLO");
    const MeshAttribute importedSecondaryCustomAttribute = importer->meshAttributeForName("_YOLO_1");
    CORRADE_VERIFY(importedSecondaryPositionAttribute != MeshAttribute{});
    CORRADE_VERIFY(importedSecondaryObjectIdAttribute != MeshAttribute{});
    CORRADE_VERIFY(importedCustomAttribute != MeshAttribute{});
    CORRADE_VERIFY(importedSecondaryCustomAttribute != MeshAttribute{});

    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->primitive(), MeshPrimitive::TriangleFan);
    CORRADE_VERIFY(!imported->isIndexed());
    CORRADE_COMPARE(imported->attributeCount(), 11);

    /* GltfImporter (stable-)sorts the attributes first to figure out the
       numbering. Check that the numbers match by comparing types. */

    CORRADE_COMPARE(imported->attributeName(0), MeshAttribute::Color);
    CORRADE_COMPARE(imported->attributeFormat(0), VertexFormat::Vector4);
    CORRADE_COMPARE(imported->attributeName(1), MeshAttribute::Color);
    CORRADE_COMPARE(imported->attributeFormat(1), VertexFormat::Vector3ubNormalized);

    CORRADE_COMPARE(imported->attributeName(2), MeshAttribute::Position);
    CORRADE_COMPARE(imported->attributeFormat(2), VertexFormat::Vector3);

    CORRADE_COMPARE(imported->attributeName(3), MeshAttribute::TextureCoordinates);
    CORRADE_COMPARE(imported->attributeFormat(3), VertexFormat::Vector2);
    CORRADE_COMPARE(imported->attributeName(4), MeshAttribute::TextureCoordinates);
    CORRADE_COMPARE(imported->attributeFormat(4), VertexFormat::Vector2usNormalized);
    CORRADE_COMPARE(imported->attributeName(5), MeshAttribute::TextureCoordinates);
    CORRADE_COMPARE(imported->attributeFormat(5), VertexFormat::Vector2ubNormalized);

    CORRADE_COMPARE(imported->attributeName(6), MeshAttribute::ObjectId);
    CORRADE_COMPARE(imported->attributeFormat(6), VertexFormat::UnsignedShort);
    CORRADE_COMPARE(imported->attributeName(7), importedSecondaryObjectIdAttribute);
    CORRADE_COMPARE(imported->attributeFormat(7), VertexFormat::UnsignedByte);

    CORRADE_COMPARE(imported->attributeName(8), importedSecondaryPositionAttribute);
    /* There's no other allowed type without extra additions, so just trust
       it's the correct one */
    CORRADE_COMPARE(imported->attributeFormat(8), VertexFormat::Vector3);

    CORRADE_COMPARE(imported->attributeName(9), importedCustomAttribute);
    CORRADE_COMPARE(imported->attributeFormat(9), VertexFormat::Float);
    CORRADE_COMPARE(imported->attributeName(10), importedSecondaryCustomAttribute);
    CORRADE_COMPARE(imported->attributeFormat(10), VertexFormat::ByteNormalized);
}

void GltfSceneConverterTest::addMeshCustomAttributeResetName() {
    const char vertices[32]{};
    MeshData mesh{MeshPrimitive::LineLoop, {}, vertices, {
        MeshAttributeData{meshAttributeCustom(31434), VertexFormat::Matrix3x3sNormalizedAligned, 0, 1, 32}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    /* Reusing an existing test file to save on the combinations */
    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-attribute-matrix3x3s.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Set two names for something else (which shouldn't get used), overwrite
       the 31434 later (the first shouldn't get used) */
    converter->setMeshAttributeName(meshAttributeCustom(31434), "_BABA");
    converter->setMeshAttributeName(meshAttributeCustom(30560), "_YOLO");
    converter->setMeshAttributeName(meshAttributeCustom(31434), "_TBN");
    converter->setMeshAttributeName(meshAttributeCustom(31995), "_MEH");

    CORRADE_VERIFY(converter->add(mesh));
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-attribute-matrix3x3s.gltf"),
        TestSuite::Compare::File);
}

void GltfSceneConverterTest::addMeshCustomAttributeNoName() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    const char vertices[4]{};
    MeshData mesh{MeshPrimitive::LineLoop, {}, vertices, {
        MeshAttributeData{meshAttributeCustom(31434), VertexFormat::Float, 0, 1, 4}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(data.flags);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-custom-attribute-no-name.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Set two names for something else (which shouldn't get used) */
    converter->setMeshAttributeName(meshAttributeCustom(30560), "_YOLO");
    converter->setMeshAttributeName(meshAttributeCustom(31995), "_MEH");

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(mesh));
    }
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): no name set for Trade::MeshAttribute::Custom(31434), exporting as _31434\n");

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-custom-attribute-no-name.gltf"),
        TestSuite::Compare::File);
}

void GltfSceneConverterTest::addMeshCustomObjectIdAttributeName() {
    const char vertices[4]{};
    MeshData mesh{MeshPrimitive::LineLoop, {}, vertices, {
        MeshAttributeData{MeshAttribute::ObjectId, VertexFormat::UnsignedShort, 0, 1, 4},
        /* Test that the secondary attribute retains the name also */
        MeshAttributeData{MeshAttribute::ObjectId, VertexFormat::UnsignedByte, 0, 1, 4}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    /* Reusing an existing test file to save on the combinations */
    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-custom-objectid-name.gltf");
    converter->configuration().setValue("objectIdAttribute", "_SEMANTIC_INDEX");
    CORRADE_VERIFY(converter->beginFile(filename));
    CORRADE_VERIFY(converter->add(mesh));
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-custom-objectid-name.gltf"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");

    /* Set a custom object ID attribute name also in the importer */
    importer->configuration().setValue("objectIdAttribute", "_SEMANTIC_INDEX");
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_COMPARE(importer->meshCount(), 1);

    const MeshAttribute importedSecondaryObjectIdAttribute = importer->meshAttributeForName("_SEMANTIC_INDEX_1");
    CORRADE_VERIFY(importedSecondaryObjectIdAttribute != MeshAttribute{});

    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->primitive(), MeshPrimitive::LineLoop);
    CORRADE_VERIFY(!imported->isIndexed());
    CORRADE_COMPARE(imported->attributeCount(), 2);

    CORRADE_COMPARE(imported->attributeName(0), MeshAttribute::ObjectId);
    CORRADE_COMPARE(imported->attributeFormat(0), VertexFormat::UnsignedShort);
    /* It's not expected to have several of singular attributes, so the
       secondary attribute is treated as fully custom */
    CORRADE_COMPARE(imported->attributeName(1), importedSecondaryObjectIdAttribute);
    CORRADE_COMPARE(imported->attributeFormat(1), VertexFormat::UnsignedByte);
}

void GltfSceneConverterTest::addMeshMultiple() {
    /* Just to verify that mixing different primitives, indexed/nonindexed
       meshes etc. doesn't cause any issues */

    const Vector3 positions[]{{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}};
    const Color4us colors[]{{15, 36, 760, 26000}, {38, 26, 1616, 63555}};
    const UnsignedShort indices[]{0, 1, 0};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-multiple.gltf");

    CORRADE_VERIFY(converter->beginFile(filename));
    CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::TriangleFan,
        {}, indices, MeshIndexData{indices},
        {}, positions, {MeshAttributeData{MeshAttribute::Position, Containers::arrayView(positions)}}
    }));
    CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Lines,
        {}, colors, {MeshAttributeData{MeshAttribute::Color, Containers::arrayView(colors)}}
    }));
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-multiple.gltf"),
        TestSuite::Compare::File);
    CORRADE_COMPARE_AS(
        Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-multiple.bin"),
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-multiple.bin"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    CORRADE_COMPARE(importer->meshCount(), 2);

    Containers::Optional<MeshData> triangleFan = importer->mesh(0);
    CORRADE_VERIFY(triangleFan);
    CORRADE_VERIFY(triangleFan->isIndexed());
    CORRADE_COMPARE(triangleFan->attributeCount(), 1);
    CORRADE_COMPARE_AS(triangleFan->indices<UnsignedShort>(),
        Containers::arrayView(indices),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(triangleFan->attribute<Vector3>(0),
        Containers::arrayView(positions),
        TestSuite::Compare::Container);

    Containers::Optional<MeshData> lines = importer->mesh(1);
    CORRADE_VERIFY(lines);
    CORRADE_VERIFY(!lines->isIndexed());
    CORRADE_COMPARE(lines->attributeCount(), 1);
    CORRADE_COMPARE_AS(lines->attribute<Color4us>(0),
        Containers::arrayView(colors),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addMeshBufferAlignment() {
    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-buffer-alignment.gltf");
    converter->configuration().setValue("accessorNames", true);
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Mesh with 5 1-byte indices and 3 3-byte positions. The indices should
       start at offset 0, the positions should get padded by three bytes. */
    UnsignedByte indicesA[]{0, 1, 2, 0, 1};
    Vector3b positionsA[]{{1, 2, 3}, {40, 50, 60}, {7, 8, 9}};
    MeshData a{MeshPrimitive::LineLoop,
        {}, indicesA, MeshIndexData{indicesA},
        {}, positionsA, {
            MeshAttributeData{MeshAttribute::Position, Containers::arrayView(positionsA)}
        }};
    CORRADE_VERIFY(converter->add(a, "A"));

    /* Mesh with 3 2-byte indices and 2 6-byte positions. The indices should
       be padded by one byte (because they only need to be aligned to 2 bytes),
       the positions should then follow them tightly. */
    UnsignedShort indicesB[]{0, 1, 0};
    Vector3s positionsB[]{{100, 200, 300}, {4000, 5000, 6000}};
    MeshData b{MeshPrimitive::LineStrip,
        {}, indicesB, MeshIndexData{indicesB},
        {}, positionsB, {
            MeshAttributeData{MeshAttribute::Position, Containers::arrayView(positionsB)}
        }};
    CORRADE_VERIFY(converter->add(b, "B"));

    CORRADE_VERIFY(converter->endFile());

    const Containers::Optional<Containers::String> gltf = Utility::Path::readString(filename);
    CORRADE_VERIFY(gltf);
    CORRADE_COMPARE_AS(*gltf,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-buffer-alignment.gltf"),
        TestSuite::Compare::StringToFile);
    /* Not testing the bin file directly, it should be enough to just verify
       the import below */

    /* Verify the expected offsets that might be missed when just looking at
       the file: */
    CORRADE_COMPARE_AS(*gltf,
        "\"byteOffset\": 0", /* index buffer A */
        TestSuite::Compare::StringContains);
    CORRADE_COMPARE_AS(*gltf,
        "\"byteOffset\": 8", /* vertex buffer A */
        TestSuite::Compare::StringContains);
    CORRADE_COMPARE_AS(*gltf,
        "\"byteOffset\": 18", /* index buffer B */
        TestSuite::Compare::StringContains);
    CORRADE_COMPARE_AS(*gltf,
        "\"byteOffset\": 24", /* vertex buffer B */
        TestSuite::Compare::StringContains);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_COMPARE(importer->meshCount(), 2);

    /* Verify the extra paddings don't mess up with the data in any way */
    Containers::Optional<MeshData> importedA = importer->mesh(0);
    CORRADE_VERIFY(importedA);
    CORRADE_VERIFY(importedA->isIndexed());
    CORRADE_COMPARE(importedA->attributeCount(), 1);
    CORRADE_COMPARE_AS(importedA->indices<UnsignedByte>(),
        Containers::arrayView(indicesA),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(importedA->attribute<Vector3b>(0),
        Containers::arrayView(positionsA),
        TestSuite::Compare::Container);

    Containers::Optional<MeshData> importedB = importer->mesh(1);
    CORRADE_VERIFY(importedB);
    CORRADE_VERIFY(importedB->isIndexed());
    CORRADE_COMPARE(importedB->attributeCount(), 1);
    CORRADE_COMPARE_AS(importedB->indices<UnsignedShort>(),
        Containers::arrayView(indicesB),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(importedB->attribute<Vector3s>(0),
        Containers::arrayView(positionsB),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addMeshInvalid() {
    auto&& data = AddMeshInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    /* So we can easier verify corrupted files */
    converter->configuration().setValue("binary", false);

    /* Strict should be the default */
    if(!data.strict)
        converter->configuration().setValue("strict", false);
    else
        CORRADE_VERIFY(converter->configuration().value<bool>("strict"));

    CORRADE_VERIFY(converter->beginData());
    /* Some tested attributes are custom */
    converter->setMeshAttributeName(meshAttributeCustom(31434), "_YOLO");

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->add(data.mesh));
        CORRADE_COMPARE(out.str(), Utility::format("Trade::GltfSceneConverter::add(): {}\n", data.message));
    }

    /* The file should not get corrupted by this error */
    Containers::Optional<Containers::Array<char>> out = converter->endData();
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "empty.gltf"),
        TestSuite::Compare::StringToFile);
}

void GltfSceneConverterTest::addImage2D() {
    auto&& data = AddImage2DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_imageConverterManager.loadState(data.converterPlugin) == PluginManager::LoadState::NotFound)
        CORRADE_SKIP(data.converterPlugin << "plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(data.flags);

    converter->configuration().setValue("imageConverter", data.converterPlugin);
    converter->configuration().setValue("accessorNames", data.accessorNames);
    if(data.experimentalKhrTextureKtx)
        converter->configuration().setValue("experimentalKhrTextureKtx", *data.experimentalKhrTextureKtx);
    if(data.strict)
        converter->configuration().setValue("strict", *data.strict);
    if(data.bundle)
        converter->configuration().setValue("bundleImages", *data.bundle);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);

    /* Delete the other filename if it exists, to verify it's indeed written */
    Containers::String otherFilename;
    if(data.expectedOtherFile) {
        otherFilename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expectedOtherFile);
        if(Utility::Path::exists(otherFilename))
            CORRADE_VERIFY(Utility::Path::remove(otherFilename));
    }

    CORRADE_VERIFY(converter->beginFile(filename));

    {
        Color4ub imageData[]{0xff3366_rgb};

        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, imageData}, data.dataName));
        if(data.expectedWarning)
            CORRADE_COMPARE(out.str(), data.expectedWarning);
        else
            CORRADE_COMPARE(out.str(), "");
    }

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);
    if(otherFilename) CORRADE_COMPARE_AS(otherFilename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expectedOtherFile),
        TestSuite::Compare::File);

    /* There shouldn't be any *.bin written, unless the image is put into it */
    CORRADE_COMPARE(Utility::Path::exists(Utility::Path::splitExtension(Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected)).first() + ".bin"), Containers::StringView{data.expectedOtherFile}.hasSuffix(".bin"));

    /* Verify various expectations that might be missed when just looking at
       the file */
    const Containers::Optional<Containers::String> gltf = Utility::Path::readString(filename);
    CORRADE_VERIFY(gltf);

    /* For images alone, extensions should be recorded only as used -- they get
       recorded as required only once a texture references the image */
    CORRADE_COMPARE(gltf->contains("extensionsUsed"), data.expectedExtension);
    CORRADE_VERIFY(!gltf->contains("extensionsRequired"));

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");
    if(_importerManager.loadState(data.importerPlugin) == PluginManager::LoadState::NotFound)
        CORRADE_SKIP(data.importerPlugin << "plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one image */
    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<ImageData2D> imported = importer->image2D(0);
    CORRADE_VERIFY(imported);
    CORRADE_VERIFY(!imported->isCompressed());
    /* Not testing the format, as it gets changed to RGBA8 for Basis */
    CORRADE_COMPARE(imported->size(), Vector2i{1});
}

void GltfSceneConverterTest::addImageCompressed2D() {
    if(_imageConverterManager.loadState("KtxImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    converter->configuration().setValue("imageConverter", "KtxImageConverter");
    converter->configuration().setValue("experimentalKhrTextureKtx", true);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "image-ktx-compressed.glb");
    CORRADE_VERIFY(converter->beginFile(filename));

    char imageData[16]{};
    CORRADE_VERIFY(converter->add(CompressedImageView2D{CompressedPixelFormat::Bc1RGBAUnorm, {4, 4}, imageData}));

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "image-ktx-compressed.glb"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");
    if(_importerManager.loadState("KtxImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");

    /* experimentalKhrTextureKtx only needed for the texture in the importer */

    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one image */
    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<ImageData2D> imported = importer->image2D(0);
    CORRADE_VERIFY(imported);
    CORRADE_VERIFY(imported->isCompressed());
    CORRADE_COMPARE(imported->compressedFormat(), CompressedPixelFormat::Bc1RGBAUnorm);
    CORRADE_COMPARE(imported->size(), (Vector2i{4, 4}));
}

void GltfSceneConverterTest::addImage3D() {
    auto&& data = AddImage3DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_imageConverterManager.loadState("KtxImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    /* 3D image support should get advertised only with the option enabled */
    CORRADE_VERIFY(!(converter->features() & (SceneConverterFeature::AddImages3D|SceneConverterFeature::AddCompressedImages3D)));
    converter->configuration().setValue("experimentalKhrTextureKtx", true);
    CORRADE_VERIFY(converter->features() & (SceneConverterFeature::AddImages3D|SceneConverterFeature::AddCompressedImages3D));

    converter->configuration().setValue("imageConverter", "KtxImageConverter");
    if(data.bundle)
        converter->configuration().setValue("bundleImages", *data.bundle);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);

    /* Delete the other filename if it exists, to verify it's indeed written */
    Containers::String otherFilename;
    if(data.expectedOtherFile) {
        otherFilename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expectedOtherFile);
        if(Utility::Path::exists(otherFilename))
            CORRADE_VERIFY(Utility::Path::remove(otherFilename));
    }

    CORRADE_VERIFY(converter->beginFile(filename));

    /* Deliberately export a two-layer image to see that two textures are
       created for it */
    Color4ub imageData[]{0xff3366_rgb, 0xff3366_rgb};
    CORRADE_VERIFY(converter->add(ImageView3D{PixelFormat::RGB8Unorm, {1, 1, 2}, imageData, ImageFlag3D::Array}));

    /* There needs to be a 2D array texture referencing this image in order to
       detect it as 3D by the importer */
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2DArray,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        0}));

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);
    if(otherFilename) CORRADE_COMPARE_AS(otherFilename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expectedOtherFile),
        TestSuite::Compare::File);

    /* There shouldn't be any *.bin written, unless the image is put into it */
    CORRADE_COMPARE(Utility::Path::exists(Utility::Path::splitExtension(Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected)).first() + ".bin"), Containers::StringView{data.expectedOtherFile}.hasSuffix(".bin"));

    /* Verify various expectations that might be missed when just looking at
       the file */
    const Containers::Optional<Containers::String> gltf = Utility::Path::readString(filename);
    CORRADE_VERIFY(gltf);

    /* As there is a texture, the extension is also required now */
    CORRADE_VERIFY(gltf->contains("extensionsUsed"));
    CORRADE_VERIFY(gltf->contains("extensionsRequired"));

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");
    if(_importerManager.loadState("KtxImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");

    importer->configuration().setValue("experimentalKhrTextureKtx", true);

    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one 3D image */
    CORRADE_COMPARE(importer->image3DCount(), 1);
    Containers::Optional<ImageData3D> imported = importer->image3D(0);
    CORRADE_VERIFY(imported);
    CORRADE_VERIFY(!imported->isCompressed());
    CORRADE_COMPARE(imported->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE(imported->size(), (Vector3i{1, 1, 2}));
}

void GltfSceneConverterTest::addImageCompressed3D() {
    if(_imageConverterManager.loadState("KtxImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    converter->configuration().setValue("imageConverter", "KtxImageConverter");
    converter->configuration().setValue("experimentalKhrTextureKtx", true);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "image-3d-compressed.glb");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Deliberately export a two-layer image to see that two textures are
       created for it */
    char imageData[32]{};
    CORRADE_VERIFY(converter->add(CompressedImageView3D{CompressedPixelFormat::Bc1RGBAUnorm, {4, 4, 2}, imageData, ImageFlag3D::Array}));

    /* There needs to be a 2D array texture referencing this image in order to
       detect it as 3D by the importer */
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2DArray,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        0}));

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "image-3d-compressed.glb"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");
    if(_importerManager.loadState("KtxImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");

    importer->configuration().setValue("experimentalKhrTextureKtx", true);

    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one 3D image */
    CORRADE_COMPARE(importer->image3DCount(), 1);
    Containers::Optional<ImageData3D> imported = importer->image3D(0);
    CORRADE_VERIFY(imported);
    CORRADE_VERIFY(imported->isCompressed());
    CORRADE_COMPARE(imported->compressedFormat(), CompressedPixelFormat::Bc1RGBAUnorm);
    CORRADE_COMPARE(imported->size(), (Vector3i{4, 4, 2}));
}

void GltfSceneConverterTest::addImagePropagateFlags() {
    auto&& data = AddImagePropagateFlagsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_imageConverterManager.loadState("TgaImageConverter") == PluginManager::LoadState::NotFound ||
       /* TgaImageConverter is also provided by StbImageConverter, which
          doesn't make use of Flags::Verbose, so that one can't be used to test
          anything */
       _imageConverterManager.metadata("TgaImageConverter")->name() != "TgaImageConverter")
        CORRADE_SKIP("(Non-aliased) TgaImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(data.converterFlags);

    converter->configuration().setValue("imageConverter", "TgaImageConverter");
    /* So it allows using a TGA image */
    converter->configuration().setValue("strict", false);
    /* So it doesn't try to use RLE first and then falls back to uncompressed
       because RLE is larger, producing one extra verbose message */
    converter->configuration().group("imageConverter")->setValue("rle", false);

    CORRADE_VERIFY(converter->beginData());

    std::ostringstream out;
    {
        Debug redirectOutput{&out};
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey", data.imageFlags}));
    }
    CORRADE_COMPARE(out.str(), data.message);

    CORRADE_VERIFY(converter->endData());

    /* No need to test any roundtrip or file contents here, the verbose output
       doesn't affect anything in the output */
}

void GltfSceneConverterTest::addImagePropagateConfiguration() {
    if(_imageConverterManager.loadState("KtxImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    converter->configuration().setValue("imageConverter", "KtxImageConverter");
    converter->configuration().setValue("experimentalKhrTextureKtx", true);

    Utility::ConfigurationGroup* imageConverterConfiguration = converter->configuration().group("imageConverter");
    CORRADE_VERIFY(imageConverterConfiguration);
    imageConverterConfiguration->setValue("generator", "MAGNUM IS AWESOME");

    CORRADE_VERIFY(converter->beginData());

    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));

    Containers::Optional<Containers::Array<char>> data = converter->endData();
    CORRADE_VERIFY(data);

    /* No need to test any roundtrip or file contents apart from checking the
       configuration option got propagated */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        "KTXwriter\0MAGNUM IS AWESOME"_s,
        TestSuite::Compare::StringContains);
}

void GltfSceneConverterTest::addImagePropagateConfigurationUnknown() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->setFlags(data.flags);

    Utility::ConfigurationGroup* imageConverterConfiguration = converter->configuration().group("imageConverter");
    CORRADE_VERIFY(imageConverterConfiguration);
    imageConverterConfiguration->setValue("quality", 42);

    CORRADE_VERIFY(converter->beginData());

    std::ostringstream out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): option quality not recognized by PngImageConverter\n");

    /* No need to test anything apart from the message above */
    CORRADE_VERIFY(converter->endData());
}

void GltfSceneConverterTest::addImagePropagateConfigurationGroup() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(data.flags);

    Utility::ConfigurationGroup* imageConverterConfiguration = converter->configuration().group("imageConverter");
    CORRADE_VERIFY(imageConverterConfiguration);
    imageConverterConfiguration->addGroup("exif");

    CORRADE_VERIFY(converter->beginData());

    std::ostringstream out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): image converter configuration group propagation not implemented yet, ignoring\n");

    /* No need to test anything apart from the message above */
    CORRADE_VERIFY(converter->endData());
}

void GltfSceneConverterTest::addImageMultiple() {
    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");
    if(_imageConverterManager.loadState("JpegImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("JpegImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "image-multiple.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* First image bundled as JPEG */
    Color4ub imageData0[]{0xff3366_rgb};
    converter->configuration().setValue("bundleImages", true);
    converter->configuration().setValue("imageConverter", "JpegImageConverter");
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, imageData0}));

    /* Second image external as PNG; named */
    Color4ub imageData1[]{0x66ff3399_rgba};
    converter->configuration().setValue("bundleImages", false);
    converter->configuration().setValue("imageConverter", "PngImageConverter");
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, imageData1}));

    /* Third image again bundled as JPEG */
    Color4ub imageData2[]{0xff6633_rgb};
    converter->configuration().setValue("bundleImages", true);
    converter->configuration().setValue("imageConverter", "JpegImageConverter");
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, imageData2}));

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "image-multiple.gltf"),
        TestSuite::Compare::File);
    CORRADE_COMPARE_AS(Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "image-multiple.bin"),
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "image-multiple.bin"),
        TestSuite::Compare::File);
    CORRADE_COMPARE_AS(Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "image-multiple.1.png"),
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "image-multiple.1.png"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");
    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test a roundtrip");
    if(_importerManager.loadState("JpegImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("JpegImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_COMPARE(importer->image2DCount(), 3);

    Containers::Optional<ImageData2D> imported0 = importer->image2D(0);
    CORRADE_VERIFY(imported0);
    CORRADE_VERIFY(!imported0->isCompressed());
    CORRADE_COMPARE(imported0->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE(imported0->size(), (Vector2i{1}));
    CORRADE_COMPARE(imported0->pixels<Color3ub>()[0][0], 0xff3366_rgb);

    Containers::Optional<ImageData2D> imported1 = importer->image2D(1);
    CORRADE_VERIFY(imported1);
    CORRADE_VERIFY(!imported1->isCompressed());
    CORRADE_COMPARE(imported1->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE(imported1->size(), (Vector2i{1}));
    CORRADE_COMPARE(imported1->pixels<Color4ub>()[0][0], 0x66ff3399_rgba);

    Containers::Optional<ImageData2D> imported2 = importer->image2D(2);
    CORRADE_VERIFY(imported2);
    CORRADE_VERIFY(!imported2->isCompressed());
    CORRADE_COMPARE(imported2->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE(imported2->size(), (Vector2i{1}));
    /* Slight rounding error */
    CORRADE_COMPARE(imported2->pixels<Color3ub>()[0][0], 0xff6632_rgb);
}

void GltfSceneConverterTest::addImageNoConverterManager() {
    /* Create a new manager that doesn't have the image converter manager
       registered; load the plugin directly from the build tree. Otherwise it's
       static and already loaded. */
    PluginManager::Manager<AbstractSceneConverter> converterManager;
    #ifdef GLTFSCENECONVERTER_PLUGIN_FILENAME
    CORRADE_VERIFY(converterManager.load(GLTFSCENECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif

    Containers::Pointer<AbstractSceneConverter> converter =  converterManager.instantiate("GltfSceneConverter");

    /* So we can easier verify corrupted files; empty.gltf doesn't have the
       generator name written either */
    converter->configuration().setValue("binary", false);
    converter->configuration().setValue("generator", "");

    CORRADE_VERIFY(converter->beginData());

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->add(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, "yey"}));
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): the plugin must be instantiated with access to plugin manager that has a registered image converter manager in order to convert images\n");
    }

    /* The file should not get corrupted by this error */
    Containers::Optional<Containers::Array<char>> out = converter->endData();
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "empty.gltf"),
        TestSuite::Compare::StringToFile);
}

void GltfSceneConverterTest::addImageExternalToData() {
    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    /* So we can easier verify corrupted files */
    converter->configuration().setValue("binary", false);

    converter->configuration().setValue("bundleImages", false);
    CORRADE_VERIFY(converter->beginData());

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->add(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, "yey"}));
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): can only write a glTF with external images if converting to a file\n");
    }

    /* The file should not get corrupted by this error */
    Containers::Optional<Containers::Array<char>> out = converter->endData();
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "empty.gltf"),
        TestSuite::Compare::StringToFile);
}

void GltfSceneConverterTest::addImageInvalid2D() {
    auto&& data = AddImageInvalid2DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(data.plugin != "WhatImageConverter"_s &&  _imageConverterManager.loadState(data.plugin) == PluginManager::LoadState::NotFound)
        CORRADE_SKIP(data.plugin << "plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->configuration().setValue("imageConverter", data.plugin);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "empty" + data.suffix);
    CORRADE_VERIFY(converter->beginFile(filename));

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->add(data.image));
        /* If the message ends with a newline, it's the whole output, otherwise
           just the sentence without any placeholder */
        if(Containers::StringView{data.message}.hasSuffix('\n'))
            CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
        else
            CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfSceneConverter::add(): {}\n", data.message));
    }

    /* Try adding the same image again, to catch assertions due to potential
       internal state mismatches */
    {
        Error redirectError{nullptr};
        CORRADE_VERIFY(!converter->add(data.image));
    }

    /* The file should not get corrupted by this error */
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "empty" + data.suffix),
        TestSuite::Compare::File);
}

void GltfSceneConverterTest::addImageInvalid3D() {
    auto&& data = AddImageInvalid3DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(data.plugin != "WhatImageConverter"_s &&  _imageConverterManager.loadState(data.plugin) == PluginManager::LoadState::NotFound)
        CORRADE_SKIP(data.plugin << "plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->configuration().setValue("experimentalKhrTextureKtx", true);
    converter->configuration().setValue("imageConverter", data.plugin);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "empty" + data.suffix);
    CORRADE_VERIFY(converter->beginFile(filename));

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->add(data.image));
        /* If the message ends with a newline, it's the whole output, otherwise
           just the sentence without any placeholder */
        if(Containers::StringView{data.message}.hasSuffix('\n'))
            CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
        else
            CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfSceneConverter::add(): {}\n", data.message));
    }

    /* Try adding the same image again, to catch assertions due to potential
       internal state mismatches */
    {
        Error redirectError{nullptr};
        CORRADE_VERIFY(!converter->add(data.image));
    }

    /* The file should not get corrupted by this error */
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "empty" + data.suffix),
        TestSuite::Compare::File);
}

void GltfSceneConverterTest::addTexture() {
    auto&& data = AddTextureData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_imageConverterManager.loadState(data.converterPlugin) == PluginManager::LoadState::NotFound)
        CORRADE_SKIP(data.converterPlugin << "plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    converter->configuration().setValue("imageConverter", data.converterPlugin);
    if(data.experimentalKhrTextureKtx)
        converter->configuration().setValue("experimentalKhrTextureKtx", *data.experimentalKhrTextureKtx);
    if(data.strict)
        converter->configuration().setValue("strict", *data.strict);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Add an image to be referenced by a texture. Suppress warnings as we test
       those in addImage() already. */
    {
        Warning redirectWarning{nullptr};
        CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));
    }

    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        0}, data.dataName));

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");

    if(data.experimentalKhrTextureKtx)
        importer->configuration().setValue("experimentalKhrTextureKtx", *data.experimentalKhrTextureKtx);

    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one texture referencing the only image */
    CORRADE_COMPARE(importer->textureCount(), 1);
    Containers::Optional<TextureData> imported = importer->texture(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->minificationFilter(), SamplerFilter::Nearest);
    CORRADE_COMPARE(imported->magnificationFilter(), SamplerFilter::Nearest);
    CORRADE_COMPARE(imported->mipmapFilter(), SamplerMipmap::Base);
    CORRADE_COMPARE(imported->wrapping(), (Math::Vector3<SamplerWrapping>{SamplerWrapping::ClampToEdge, SamplerWrapping::ClampToEdge, SamplerWrapping::Repeat}));
    CORRADE_COMPARE(imported->image(), 0);
}

void GltfSceneConverterTest::addTextureMultiple() {
    if(_imageConverterManager.loadState("BasisImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImageConverter plugin not found, cannot test");
    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");
    if(_imageConverterManager.loadState("KtxImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->configuration().setValue("experimentalKhrTextureKtx", true);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "texture-multiple.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* First image PNG */
    converter->configuration().setValue("imageConverter", "PngImageConverter");
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));

    /* Second image Basis, unused. It will have a KHR_texture_basisu in
       extensionsUsed but not in extensionRequired. */
    converter->configuration().setValue("imageConverter", "BasisKtxImageConverter");
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}, "Basis-encoded, unused"));

    /* Third image KTX */
    converter->configuration().setValue("imageConverter", "KtxImageConverter");
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));

    /* Reference third and first image from two textures */
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Linear,
        SamplerFilter::Nearest,
        SamplerMipmap::Nearest,
        {SamplerWrapping::MirroredRepeat, SamplerWrapping::ClampToEdge, SamplerWrapping{}},
        2}));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Linear,
        SamplerMipmap::Linear,
        {SamplerWrapping::Repeat, SamplerWrapping::MirroredRepeat, SamplerWrapping{}},
        0}));

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "texture-multiple.gltf"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    importer->configuration().setValue("experimentalKhrTextureKtx", true);

    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be two textures referencing two out of the three images */
    CORRADE_COMPARE(importer->textureCount(), 2);
    Containers::Optional<TextureData> imported0 = importer->texture(0);
    CORRADE_VERIFY(imported0);
    CORRADE_COMPARE(imported0->minificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(imported0->magnificationFilter(), SamplerFilter::Nearest);
    CORRADE_COMPARE(imported0->mipmapFilter(), SamplerMipmap::Nearest);
    CORRADE_COMPARE(imported0->wrapping(), (Math::Vector3<SamplerWrapping>{SamplerWrapping::MirroredRepeat, SamplerWrapping::ClampToEdge, SamplerWrapping::Repeat}));
    CORRADE_COMPARE(imported0->image(), 2);

    Containers::Optional<TextureData> imported1 = importer->texture(1);
    CORRADE_VERIFY(imported1);
    CORRADE_COMPARE(imported1->minificationFilter(), SamplerFilter::Nearest);
    CORRADE_COMPARE(imported1->magnificationFilter(), SamplerFilter::Linear);
    CORRADE_COMPARE(imported1->mipmapFilter(), SamplerMipmap::Linear);
    CORRADE_COMPARE(imported1->wrapping(), (Math::Vector3<SamplerWrapping>{SamplerWrapping::Repeat, SamplerWrapping::MirroredRepeat, SamplerWrapping::Repeat}));
    CORRADE_COMPARE(imported1->image(), 0);
}

void GltfSceneConverterTest::addTextureDeduplicatedSamplers() {
    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "texture-deduplicated-samplers.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));

    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Nearest,
        {SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping{}},
        0}));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Linear,
        SamplerFilter::Nearest,
        SamplerMipmap::Nearest,
        {SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping{}},
        0}, "Different minification filter"));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Linear,
        SamplerMipmap::Nearest,
        {SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping{}},
        0}, "Different magnification filter"));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Linear,
        {SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping{}},
        0}, "Different mipmap filter"));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Nearest,
        {SamplerWrapping::ClampToEdge, SamplerWrapping::Repeat, SamplerWrapping{}},
        0}, "Different wrapping X"));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Nearest,
        {SamplerWrapping::Repeat, SamplerWrapping::ClampToEdge, SamplerWrapping{}},
        0}, "Different wrapping Y"));

    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Nearest,
        {SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping{}},
        0}, "Should reuse sampler 0"));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Linear,
        SamplerFilter::Nearest,
        SamplerMipmap::Nearest,
        {SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping{}},
        0}, "Should reuse sampler 1"));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Linear,
        SamplerMipmap::Nearest,
        {SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping{}},
        0}, "Should reuse sampler 2"));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Linear,
        {SamplerWrapping::Repeat, SamplerWrapping::Repeat, SamplerWrapping{}},
        0}, "Should reuse sampler 3"));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Nearest,
        {SamplerWrapping::ClampToEdge, SamplerWrapping::Repeat, SamplerWrapping{}},
        0}, "Should reuse sampler 4"));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Nearest,
        {SamplerWrapping::Repeat, SamplerWrapping::ClampToEdge, SamplerWrapping{}},
        0}, "Should reuse sampler 5"));

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "texture-deduplicated-samplers.gltf"),
        TestSuite::Compare::File);

    /* Not testing file roundtrip as sampler deduplication doesn't really
       make any difference there */
}

void GltfSceneConverterTest::addTextureInvalid() {
    auto&& data = AddTextureInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Add an image to be referenced by a texture */
    if(data.texture.type() == TextureType::Texture2D) {
        if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
            CORRADE_SKIP("PngImageConverter plugin not found, cannot test");
        CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));
    } else if(data.texture.type() == TextureType::Texture2DArray) {
        if(_imageConverterManager.loadState("KtxImageConverter") == PluginManager::LoadState::NotFound)
            CORRADE_SKIP("KtxImageConverter plugin not found, cannot test");
        converter->configuration().setValue("experimentalKhrTextureKtx", true);
        converter->configuration().setValue("imageConverter", "KtxImageConverter");
        CORRADE_VERIFY(converter->add(ImageView3D{PixelFormat::RGB8Unorm, {1, 1, 1}, "yey", ImageFlag3D::Array}));
    }

    if(data.experimentalKhrTextureKtx)
        converter->configuration().setValue("experimentalKhrTextureKtx", *data.experimentalKhrTextureKtx);

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->add(data.texture));
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfSceneConverter::add(): {}\n", data.message));
    }

    /* The file should not get corrupted by this error, thus the same as if
       just the (2D/3D/none) image was added */
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);
}

namespace {

MaterialData filterMaterialAttributes(const MaterialData& material, Containers::ArrayView<const Containers::Pair<UnsignedInt, MaterialAttribute>> remove, const Containers::Optional<MaterialData>& add) {
    Containers::BitArray attributesToKeep{DirectInit, material.attributeData().size(), true};
    for(const Containers::Pair<UnsignedInt, MaterialAttribute>& attribute: remove)
        attributesToKeep.reset(material.attributeDataOffset(attribute.first()) + material.attributeId(attribute.first(), attribute.second()));

    /* Remove all original MaterialTypes from the input, if any are to be added
       they're in `add` */
    MaterialData filtered = MaterialTools::filterAttributes(material, attributesToKeep, {});
    if(!add) return filtered;

    Containers::Optional<MaterialData> out = MaterialTools::merge(filtered, *add);
    CORRADE_VERIFY(out);
    return *std::move(out);
}

}

void GltfSceneConverterTest::addMaterial() {
    auto&& data = AddMaterialData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(data.needsTexture && _imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    if(data.keepDefaults)
        converter->configuration().setValue("keepMaterialDefaults", *data.keepDefaults);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);
    CORRADE_VERIFY(converter->beginFile(filename));

    if(data.needsTexture) {
        /* Add an image to be referenced by a texture */
        CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));

        /* Add a texture to be referenced by a material */
        CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
            SamplerFilter::Nearest,
            SamplerFilter::Nearest,
            SamplerMipmap::Base,
            SamplerWrapping::ClampToEdge,
            0}));
    }

    /* There should be no warning about unused attributes, actual warnings are
       tested in addMaterialUnusedAttributes() instead */
    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(data.material, data.dataName));
        CORRADE_COMPARE(out.str(), "");
    }

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* Disable Phong material fallback (enabled by default for compatibility),
       no use for that here */
    importer->configuration().setValue("phongMaterialFallback", false);

    /* There should be exactly one material, looking exactly the same as the
       filtered original */
    CORRADE_COMPARE(importer->materialCount(), 1);
    Containers::Optional<MaterialData> imported = importer->material(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE_AS(*imported,
        filterMaterialAttributes(data.material, data.expectedRemove, data.expectedAdd),
        DebugTools::CompareMaterial);
}

void GltfSceneConverterTest::addMaterial2DArrayTextures() {
    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");
    if(_imageConverterManager.loadState("KtxImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    converter->configuration().setValue("experimentalKhrTextureKtx", true);
    converter->configuration().setValue("imageConverter", "KtxImageConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "material-2d-array-textures.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Add a few 2D and 3D images to be referenced by a texture */
    CORRADE_VERIFY(converter->add(ImageView3D{PixelStorage{}.setAlignment(1), PixelFormat::R8Unorm, {1, 1, 4}, "yey", ImageFlag3D::Array}));
    CORRADE_VERIFY(converter->add(ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::R8Unorm, {1, 1}, "y"}, "2D KTX, not used"));
    CORRADE_VERIFY(converter->add(ImageView3D{PixelStorage{}.setAlignment(1), PixelFormat::R8Unorm, {1, 1, 7}, "yeyyey", ImageFlag3D::Array}));
    /* Also a plain PNG 2D image to test correct numbering in the non-extension
       code path */
    converter->configuration().setValue("imageConverter", "PngImageConverter");
    CORRADE_VERIFY(converter->add(ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::R8Unorm, {1, 1}, "y"}));

    /* Add corresponding textures, in a shuffled order to catch indexing bugs.
       Name one array texture but not the other to test that the name gets
       duplicated for each layer.  */
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2DArray,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        1}));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2DArray,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        0}, "2D array texture"));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        1}));

    MaterialData material{{}, {
        {MaterialAttribute::BaseColorTexture, 0u},
        {MaterialAttribute::BaseColorTextureLayer, 6u},
        {MaterialAttribute::EmissiveTexture, 2u},
        {MaterialAttribute::EmissiveTextureLayer, 0u}, /* Dropped on import */
        {MaterialAttribute::OcclusionTexture, 1u},
        {MaterialAttribute::OcclusionTextureLayer, 3u},
    }};
    CORRADE_VERIFY(converter->add(material));

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "material-2d-array-textures.gltf"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    importer->configuration().setValue("experimentalKhrTextureKtx", true);
    /* Disable Phong material fallback (enabled by default for compatibility),
       no use for that here */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be two 3D images and two 2D. Not verifying their contents,
       as that's sufficiently tested elsewhere. */
    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_COMPARE(importer->image3DCount(), 2);

    /* Three textures referencing two 3D images and one 2D. The 3D textures,
       stored as separate layers, should be deduplicated. */
    CORRADE_COMPARE(importer->textureCount(), 3);
    CORRADE_COMPARE(importer->textureForName("2D array texture"), 1);

    Containers::Optional<TextureData> importedTexture0 = importer->texture(0);
    CORRADE_VERIFY(importedTexture0);
    CORRADE_COMPARE(importedTexture0->type(), TextureType::Texture2DArray);
    CORRADE_COMPARE(importedTexture0->image(), 1);

    Containers::Optional<TextureData> importedTexture1 = importer->texture(1);
    CORRADE_VERIFY(importedTexture1);
    CORRADE_COMPARE(importedTexture1->type(), TextureType::Texture2DArray);
    CORRADE_COMPARE(importedTexture1->image(), 0);

    Containers::Optional<TextureData> importedTexture2 = importer->texture(2);
    CORRADE_VERIFY(importedTexture2);
    CORRADE_COMPARE(importedTexture2->type(), TextureType::Texture2D);
    CORRADE_COMPARE(importedTexture2->image(), 1);

    /* There should be exactly one material, looking exactly the same as the
       original */
    CORRADE_COMPARE(importer->materialCount(), 1);
    Containers::Optional<MaterialData> importedMaterial = importer->material(0);
    CORRADE_VERIFY(importedMaterial);
    CORRADE_COMPARE_AS(*importedMaterial, filterMaterialAttributes(material,
        /* Emissive layer is 0 and for a 2D image, which is same as not present
           at all */
        Containers::arrayView({Containers::pair(0u, MaterialAttribute::EmissiveTextureLayer)}),
        MaterialData{MaterialType::PbrMetallicRoughness, {}}),
        DebugTools::CompareMaterial);
}

template<SceneConverterFlag flag> void GltfSceneConverterTest::addMaterialUnusedAttributes() {
    auto&& data = AddMaterialUnusedAttributesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);
    if(flag == SceneConverterFlag::Quiet)
        setTestCaseTemplateName("SceneConverterFlag::Quiet");

    if(data.needsTexture && _imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(flag);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);
    CORRADE_VERIFY(converter->beginFile(filename));

    if(data.needsTexture) {
        /* Add an image to be referenced by a texture */
        CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));

        /* Add a texture to be referenced by a material */
        CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
            SamplerFilter::Nearest,
            SamplerFilter::Nearest,
            SamplerMipmap::Base,
            SamplerWrapping::ClampToEdge,
            0}));
    }

    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(data.material));
        if(flag == SceneConverterFlag::Quiet)
            CORRADE_COMPARE(out.str(), "");
        else
            CORRADE_COMPARE(out.str(), data.expectedWarning);
    }

    /* Testing the contents would be too time-consuming, the file itself has to
       suffice */
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);
}

template<SceneConverterFlag flag> void GltfSceneConverterTest::addMaterialCustom() {
    auto&& data = AddMaterialCustomData[testCaseInstanceId()];
    setTestCaseDescription(data.name);
    if(flag == SceneConverterFlag::Quiet)
        setTestCaseTemplateName("SceneConverterFlag::Quiet");

    if(data.needsTexture && _imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");
    if(data.needsTexture3D && _imageConverterManager.loadState("KtxImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(flag);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);
    CORRADE_VERIFY(converter->beginFile(filename));

    if(data.needsTexture) {
        /* Add an image to be referenced by a texture */
        CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));

        /* Add a texture to be referenced by a material */
        CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
            SamplerFilter::Nearest,
            SamplerFilter::Nearest,
            SamplerMipmap::Base,
            SamplerWrapping::ClampToEdge,
            0}));
    }

    if(data.needsTexture3D) {
        converter->configuration().setValue("experimentalKhrTextureKtx", true);
        converter->configuration().setValue("imageConverter", "KtxImageConverter");

        /* Add an image to be referenced by a texture */
        CORRADE_VERIFY(converter->add(ImageView3D{PixelFormat::RGB8Unorm, {1, 1, 5}, "yey0yey1yey2yey3yey", ImageFlag3D::Array}));

        /* Add a texture to be referenced by a material */
        CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2DArray,
            SamplerFilter::Nearest,
            SamplerFilter::Nearest,
            SamplerMipmap::Base,
            SamplerWrapping::ClampToEdge,
            0}));
    }

    for(const char* i: data.explicitUsedExtensions)
        converter->configuration().addValue("extensionUsed", i);

    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(data.material));
        if(flag == SceneConverterFlag::Quiet)
            CORRADE_COMPARE(out.str(), "");
        else
            CORRADE_COMPARE(out.str(), data.expectedWarning);
    }

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    if(data.needsTexture3D)
        importer->configuration().setValue("experimentalKhrTextureKtx", true);
    CORRADE_VERIFY(importer->openFile(filename));

    /* Disable Phong material fallback (enabled by default for compatibility),
       no use for that here */
    importer->configuration().setValue("phongMaterialFallback", false);

    /* Filter the expected-to-be-removed attributes and layers from the
       input */
    Containers::BitArray attributesToKeep{DirectInit, data.material.attributeData().size(), true};
    for(const Containers::Pair<UnsignedInt, const char*>& attribute: data.expectedRemoveAttributes)
        attributesToKeep.reset(data.material.attributeDataOffset(attribute.first()) + data.material.attributeId(attribute.first(), attribute.second()));
    Containers::BitArray layersToKeep{DirectInit, data.material.layerCount(), true};
    for(UnsignedInt layer: data.expectedRemoveLayers)
        layersToKeep.reset(layer);
    MaterialData filtered = MaterialTools::filterAttributesLayers(data.material, attributesToKeep, layersToKeep);
    if(data.expectedAdd) {
        Containers::Optional<MaterialData> out = MaterialTools::merge(filtered, *data.expectedAdd);
        CORRADE_VERIFY(out);
        filtered = *std::move(out);
    }

    /* There should be exactly one material, looking exactly the same as the
       filtered original */
    CORRADE_COMPARE(importer->materialCount(), 1);
    Containers::Optional<MaterialData> imported = importer->material(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE_AS(*imported,
        filtered,
        DebugTools::CompareMaterial);
}

void GltfSceneConverterTest::addMaterialMultiple() {
    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "material-multiple.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Add three textures referencing a single image  */
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        0}));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        0}));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        0}));

    /* A textureless material. Adding the type even though not use to make
       comparison with imported data easier. */
    MaterialData material0{MaterialType::PbrMetallicRoughness, {
        {MaterialAttribute::BaseColor, Color4{0.1f, 0.2f, 0.3f, 0.4f}},
        {MaterialAttribute::DoubleSided, true}
    }};
    CORRADE_VERIFY(converter->add(material0));

    /* A material referencing texture 0 and 2; texture 1 is unused. Since this
       one doesn't have any PBR properties, it's not marked as
       PbrMetallicRoughness on import and thus not here either. */
    MaterialData material1{{}, {
        {MaterialAttribute::NormalTexture, 2u},
        {MaterialAttribute::OcclusionTexture, 0u}
    }};
    CORRADE_VERIFY(converter->add(material1));

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "material-multiple.gltf"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(filename));

    /* Disable Phong material fallback (enabled by default for compatibility),
       no use for that here */
    importer->configuration().setValue("phongMaterialFallback", false);

    /* There should be two materials referencing two textures */
    CORRADE_COMPARE(importer->materialCount(), 2);
    Containers::Optional<MaterialData> imported0 = importer->material(0);
    CORRADE_VERIFY(imported0);
    CORRADE_COMPARE_AS(*imported0, material0, DebugTools::CompareMaterial);

    Containers::Optional<MaterialData> imported1 = importer->material(1);
    CORRADE_VERIFY(imported1);
    CORRADE_COMPARE_AS(*imported1, material1, DebugTools::CompareMaterial);
}

void GltfSceneConverterTest::addMaterialInvalid() {
    auto&& data = AddMaterialInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "texture.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Add an image to be referenced by a texture */
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));

    /* Add a texture to be referenced by a material */
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        0}));

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->add(data.material));
        CORRADE_COMPARE(out.str(), Utility::format("Trade::GltfSceneConverter::add(): {}\n", data.message));
    }

    /* The file should not get corrupted by this error, thus the same as if
       just the image & texture was added */
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "texture.gltf"),
        TestSuite::Compare::File);
}

void GltfSceneConverterTest::addMaterial2DArrayTextureLayerOutOfBounds() {
    /* Same as addMaterial2DArrayTextures() except for the error case at the
       end */

    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");
    if(_imageConverterManager.loadState("KtxImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    converter->configuration().setValue("experimentalKhrTextureKtx", true);
    converter->configuration().setValue("imageConverter", "KtxImageConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "material-2d-array-textures.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Add a few 2D and 3D images to be referenced by a texture */
    CORRADE_VERIFY(converter->add(ImageView3D{PixelStorage{}.setAlignment(1), PixelFormat::R8Unorm, {1, 1, 4}, "yey", ImageFlag3D::Array}));
    CORRADE_VERIFY(converter->add(ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::R8Unorm, {1, 1}, "y"}, "2D KTX, not used"));
    CORRADE_VERIFY(converter->add(ImageView3D{PixelStorage{}.setAlignment(1), PixelFormat::R8Unorm, {1, 1, 7}, "yeyyey", ImageFlag3D::Array}));
    /* Also a plain PNG 2D image to test correct numbering in the non-extension
       code path */
    converter->configuration().setValue("imageConverter", "PngImageConverter");
    CORRADE_VERIFY(converter->add(ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::R8Unorm, {1, 1}, "y"}));

    /* Add corresponding textures, in a shuffled order to catch indexing bugs.
       Name one array texture but not the other to test that the name gets
       duplicated for each layer.  */
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2DArray,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        1}));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2DArray,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        0}, "2D array texture"));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        1}));

    /* First material is fine, referencing the last layer of each image */
    CORRADE_VERIFY(converter->add(MaterialData{{}, {
        {MaterialAttribute::BaseColorTexture, 0u},
        {MaterialAttribute::BaseColorTextureLayer, 6u},
        {MaterialAttribute::EmissiveTexture, 2u},
        {MaterialAttribute::EmissiveTextureLayer, 0u},
        {MaterialAttribute::OcclusionTexture, 1u},
        {MaterialAttribute::OcclusionTextureLayer, 3u},
    }}));

    /* Second material has the second texture OOB */
    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->add(MaterialData{{}, {
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureLayer, 6u},
            {MaterialAttribute::OcclusionTexture, 1u},
            {MaterialAttribute::OcclusionTextureLayer, 4u},
        }}));
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): material attribute OcclusionTextureLayer value 4 out of range for 4 layers in texture 1\n");
    }

    /* The file should not get corrupted by this error, thus the same as if
       just the first material was added, which corresponds to
       addMaterial2DArrayTextures() */
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "material-2d-array-textures.gltf"),
        TestSuite::Compare::File);
}

void GltfSceneConverterTest::textureCoordinateYFlip() {
    auto&& data = TextureCoordinateYFlipData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    if(data.textureCoordinateYFlipInMaterial)
        converter->configuration().setValue("textureCoordinateYFlipInMaterial", *data.textureCoordinateYFlipInMaterial);
    if(data.keepMaterialDefaults)
        converter->configuration().setValue("keepMaterialDefaults", *data.keepMaterialDefaults);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Add an image to be referenced by a texture */
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));

    /* Add a texture to be referenced by a material */
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        0}));

    CORRADE_VERIFY(converter->add(data.mesh));
    CORRADE_VERIFY(converter->add(data.material));
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(filename));

    /* Disable Phong material fallback (enabled by default for compatibility),
       no use for that here */
    importer->configuration().setValue("phongMaterialFallback", false);

    /* There should be one mesh and one material */
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::TextureCoordinates));
    Containers::Array<Vector2> texCoords = mesh->textureCoordinates2DAsArray();

    /* Texture transform is added to materials that don't have it yet */
    Containers::Optional<MaterialData> material = importer->material(0);
    CORRADE_VERIFY(material);

    /* In case of custom material attributes, they're in custom layers, and
       then the first attribute in that layer is the layer name */
    UnsignedInt layer = material->layerCount() - 1;
    UnsignedInt firstAttributeId = layer ? 1 : 0;

    /* Assume the first material attribute in the last layer is the actual
       texture, derive the matrix attribute name from it */
    CORRADE_COMPARE_AS(material->attributeCount(layer),
        firstAttributeId,
        TestSuite::Compare::Greater);
    CORRADE_COMPARE_AS(material->attributeName(layer, firstAttributeId),
        "Texture",
        TestSuite::Compare::StringHasSuffix);
    const Containers::String matrixAttribute = material->attributeName(layer, firstAttributeId) + "Matrix"_s;

    CORRADE_COMPARE(material->hasAttribute(layer, matrixAttribute),
        (data.textureCoordinateYFlipInMaterial && *data.textureCoordinateYFlipInMaterial) ||
        data.material.hasAttribute(layer, matrixAttribute));

    /* Transformed texture coordinates should be the same regardless of the
       setting */
    if(Containers::Optional<Matrix3> matrix = material->findAttribute<Matrix3>(layer, matrixAttribute))
        MeshTools::transformPointsInPlace(*matrix, texCoords);
    CORRADE_COMPARE_AS(texCoords, Containers::arrayView<Vector2>({
        {1.0f, 0.5f},
        {0.5f, 1.0f},
        {0.0f, 0.0f}
    }), TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addSceneEmpty() {
    auto&& data = AddSceneEmptyData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);
    CORRADE_VERIFY(converter->beginFile(filename));

    CORRADE_VERIFY(converter->add(SceneData{SceneMappingType::UnsignedByte, 0, nullptr, {
        /* To mark the scene as 3D */
        SceneFieldData{SceneField::Transformation,
            SceneMappingType::UnsignedByte, nullptr,
            SceneFieldType::Matrix4x4, nullptr},
    }}));

    if(data.defaultScene != -1)
        converter->setDefaultScene(data.defaultScene);

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one scene, referencing all nodes */
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 0);
    Containers::Optional<SceneData> imported = importer->scene(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->mappingBound(), 0);
    /* There is ImporterState & Parent always, plus Transformation to indicate
       a 3D scene */
    CORRADE_COMPARE(imported->fieldCount(), 3);

    /* The scene should be set as default only if we called the function */
    CORRADE_COMPARE(importer->defaultScene(), data.defaultScene);
}

void GltfSceneConverterTest::addScene() {
    auto&& data = AddSceneData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(data.flags);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Deliberately using a 16-bit mapping to trigger accidentally hardcoded
       UnsignedInt inside add(SceneData). The optionally added offset *should
       not* change the output in any way. */
    struct Scene {
        Containers::Pair<UnsignedShort, Int> parents[5];
        Containers::Pair<UnsignedShort, Matrix4> transformations[6];
        struct Trs {
            UnsignedShort mapping;
            Vector3 translation;
            Quaternion rotation;
            Vector3 scaling;
        } trs[4];
    } sceneData[]{{
        /* Parents, unordered, including forward references, multiple children
           and deeper hierarchies. Object 4 is without a parent reference. */
        {{UnsignedShort(data.offset + 0), -1},
         {UnsignedShort(data.offset + 3), data.offset + 5},
         {UnsignedShort(data.offset + 2), -1},
         {UnsignedShort(data.offset + 1), data.offset + 5},
         {UnsignedShort(data.offset + 5), data.offset + 2}},

        /* One object should be without any transformation. One object has the
           transformation accidentally specified twice, which should be ignored
           with a warning. */
        {{UnsignedShort(data.offset + 2),
            Matrix4::translation({0.5f, 0.25f, 0.125f})*
            Matrix4::rotationZ(15.0_degf)*
            Matrix4::scaling({1.0f, 2.0f, 3.0f})},
         {UnsignedShort(data.offset + 4),
            Matrix4::rotationX(55.0_degf)},
         {UnsignedShort(data.offset + 0),
            Matrix4::translation({4.0f, 5.0f, 6.0f})},
         {UnsignedShort(data.offset + 1),
            Matrix4::rotationY(60.0_degf)},
         {UnsignedShort(data.offset + 5),
            Matrix4::rotationZ(15.0_degf)*
            Matrix4::translation({7.0f, 8.0f, 9.0f})},
         {UnsignedShort(data.offset + 5), /* duplicate */
            Matrix4::rotationZ(15.0_degf)*
            Matrix4::translation({7.0f, 8.0f, 9.0f})}},

        /* One object should be only with a matrix */
        {{UnsignedShort(data.offset + 1),
            {},
            Quaternion::rotation(60.0_degf, Vector3::yAxis()),
            Vector3{1.0f}},
         {UnsignedShort(data.offset + 4),
            {},
            Quaternion::rotation(15.0_degf, Vector3::xAxis()),
            Vector3{1.0f}},
         {UnsignedShort(data.offset + 2),
            {0.5f, 0.25f, 0.125f},
            Quaternion::rotation(15.0_degf, Vector3::zAxis()),
            {1.0f, 2.0f, 3.0f}},
         {UnsignedShort(data.offset + 0),
            {4.0f, 5.0f, 6.0f},
            {},
            Vector3{1.0f}}}
    }};

    if(data.dataName) {
        converter->setObjectName(data.offset + 3, "No transformation");
        converter->setObjectName(data.offset + 5, "This object has no parent and thus isn't exported");
        converter->setObjectName(data.offset + 5, "No TRS");
        converter->setObjectName(data.offset + 6, "This object doesn't exist");
    }

    SceneData scene{SceneMappingType::UnsignedShort, data.offset + 6u, {}, sceneData, {
        SceneFieldData{SceneField::Parent,
            Containers::stridedArrayView(sceneData->parents).slice(&Containers::Pair<UnsignedShort, Int>::first),
            Containers::stridedArrayView(sceneData->parents).slice(&Containers::Pair<UnsignedShort, Int>::second)},
        SceneFieldData{SceneField::Transformation,
            Containers::stridedArrayView(sceneData->transformations).slice(&Containers::Pair<UnsignedShort, Matrix4>::first),
            Containers::stridedArrayView(sceneData->transformations).slice(&Containers::Pair<UnsignedShort, Matrix4>::second)},
        SceneFieldData{SceneField::Translation,
            Containers::stridedArrayView(sceneData->trs).slice(&Scene::Trs::mapping),
            Containers::stridedArrayView(sceneData->trs).slice(&Scene::Trs::translation)},
        /* Ignored field, produces a warning */
        SceneFieldData{SceneField::Light,
            Containers::stridedArrayView(sceneData->parents).slice(&Containers::Pair<UnsignedShort, Int>::first),
            Containers::stridedArrayView(sceneData->parents).slice(&Containers::Pair<UnsignedShort, Int>::first)},
        SceneFieldData{SceneField::Rotation,
            Containers::stridedArrayView(sceneData->trs).slice(&Scene::Trs::mapping),
            Containers::stridedArrayView(sceneData->trs).slice(&Scene::Trs::rotation)},
        /* ImporterState field is ignored but without a warning */
        SceneFieldData{SceneField::ImporterState,
            SceneMappingType::UnsignedShort,
            Containers::stridedArrayView(sceneData->trs).slice(&Scene::Trs::mapping),
            SceneFieldType::Pointer,
            Containers::stridedArrayView(sceneData->trs).slice(&Scene::Trs::translation)},
        SceneFieldData{SceneField::Scaling,
            Containers::stridedArrayView(sceneData->trs).slice(&Scene::Trs::mapping),
            Containers::stridedArrayView(sceneData->trs).slice(&Scene::Trs::scaling)},
    }};

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(scene, data.dataName));
    }
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(), Utility::formatString(
            "Trade::GltfSceneConverter::add(): Trade::SceneField::Light was not used\n"
            "Trade::GltfSceneConverter::add(): parentless object {} was not used\n"
            "Trade::GltfSceneConverter::add(): ignoring duplicate field Trade::SceneField::Transformation for object {}\n",
            data.offset + 4,
            data.offset + 5));

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one scene */
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 5);
    Containers::Optional<SceneData> imported = importer->scene(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->mappingBound(), 5);
    CORRADE_COMPARE(imported->fieldCount(), 5 + 1 /*ImporterState*/);

    /* The fields are reordered in a breadth-first order */

    CORRADE_VERIFY(imported->hasField(SceneField::Parent));
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(SceneField::Parent),
        Containers::arrayView({0u, 2u, 4u, 3u, 1u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->field<Int>(SceneField::Parent),
        Containers::arrayView({-1, -1, 2, 4, 4}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(SceneField::Transformation));
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(SceneField::Transformation),
        Containers::arrayView({0u, 2u, 4u, 1u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->field<Matrix4>(SceneField::Transformation), Containers::arrayView({
        Matrix4::translation({4.0f, 5.0f, 6.0f}),
        Matrix4::translation({0.5f, 0.25f, 0.125f})*
            Matrix4::rotationZ(15.0_degf)*
            Matrix4::scaling({1.0f, 2.0f, 3.0f}),
        Matrix4::rotationZ(15.0_degf)*
            Matrix4::translation({7.0f, 8.0f, 9.0f}),
        Matrix4::rotationY(60.0_degf),
    }), TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(SceneField::Translation));
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(SceneField::Translation),
        Containers::arrayView({0u, 2u, 1u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->field<Vector3>(SceneField::Translation), Containers::arrayView({
        Vector3{4.0f, 5.0f, 6.0f},
        Vector3{0.5f, 0.25f, 0.125f},
        Vector3{}, /* MSVC (even 2022) needs an explicit type, lol */
    }), TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(SceneField::Rotation));
    /* Mapping is the same for all three TRS fields */
    CORRADE_COMPARE_AS(imported->field<Quaternion>(SceneField::Rotation), Containers::arrayView({
        Quaternion{}, /* MSVC (even 2022) needs an explicit type, lol */
        Quaternion::rotation(15.0_degf, Vector3::zAxis()),
        Quaternion::rotation(60.0_degf, Vector3::yAxis()),
    }), TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(SceneField::Scaling));
    /* Mapping is the same for all three TRS fields */
    CORRADE_COMPARE_AS(imported->field<Vector3>(SceneField::Scaling), Containers::arrayView({
        Vector3{1.0f},
        Vector3{1.0f, 2.0f, 3.0f},
        Vector3{1.0f},
    }), TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addSceneMeshesMaterials() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(data.flags);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "scene-meshes-materials.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Add four empty meshes to not have to bother with buffers. Not valid
       glTF but accepted with strict=false (which gets reset back after) */
    {
        Warning silenceWarning{nullptr};
        converter->configuration().setValue("strict", false);
        /* Naming them to see how they were reordered; use also a different
           primitive to spot unnecessary duplicates in the output */
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Points, 0}, "Mesh 0"));
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Lines, 0}, "Mesh 1"));
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::LineLoop, 0}, "Mesh 2"));
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::LineStrip, 0}, "Mesh 3"));
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Triangles, 0}, "Mesh 4"));
        /* These two are different but with the same name, thus their name
           should get preserved */
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::TriangleStrip, 0}, "Multimesh 5"));
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::TriangleFan, 0}, "Multimesh 5"));
        converter->configuration().setValue("strict", true);
    }

    /* Add two empty materials */
    {
        CORRADE_VERIFY(converter->add(MaterialData{{}, {}}, "Material 0"));
        CORRADE_VERIFY(converter->add(MaterialData{{}, {}}, "Material 1"));
        CORRADE_VERIFY(converter->add(MaterialData{{}, {}}, "Material 2"));
    }

    /* Deliberately using large & sparse object IDs to verify the warnings
       reference them and not the remapped ones. Preserve the IDs in object
       names for easier debugging tho. */
    converter->setObjectName(0, "Object 0");
    converter->setObjectName(10, "Object 10");
    converter->setObjectName(20, "Object 20");
    converter->setObjectName(30, "Object 30");
    converter->setObjectName(40, "Object 40");
    converter->setObjectName(50, "Object 50");
    converter->setObjectName(60, "Object 60");
    converter->setObjectName(70, "Object 70");
    converter->setObjectName(80, "Object 80");
    converter->setObjectName(90, "Object 90");
    converter->setObjectName(100, "Object 100");
    converter->setObjectName(110, "Object 110");
    converter->setObjectName(120, "Object 120");
    struct Scene {
        Containers::Pair<UnsignedInt, Int> parents[12];
        Containers::Triple<UnsignedInt, UnsignedInt, Int> meshesMaterials[18];
    } sceneData[]{{
        /* Object 30 is without a parent, thus ignored */
        {{0, -1},
         {40, -1},
         {20, -1},
         {10, -1},
         {50, -1},
         {60, -1},
         {70, -1},
         {80, -1},
         {90, -1},
         {100, -1},
         {110, -1},
         {120, -1}},

        /* - Object 10 is without any mesh
           - Mesh 4 is not referenced by any objects, so it gets added at the
             end

           Single-mesh assignments (first block):

           - Mesh 2 is referenced by objects 0 and 40 without a material so it
             should appear just once in the output
           - Mesh 3 is used by objects 60 and 80 both times with the same
             material so it should again appear just once
           - Mesh 1 is used by object 120 without a material, by object 70 with
             a material and by object 30 with a different material. Object 30
             doesn't have a parent and thus isn't included, so the mesh appears
             just twice in the output, not three times.

           Multi-mesh assignments (second block):

           - Object 50 has three mesh assignments, which should be preserved.
             They have different names so the name isn't preserved.
           - Object 90 has the same but in different order so it should
             reference the same
           - Object 100 references two meshes with the same name, thus the name
             gets preserved
           - Object 110 references the same two meshes as object 100, but with
             one material assignment different, so it gets a new mesh. Name is
             preserved again. */
        {{40, 2, -1},
         {120, 1, -1},
         {20, 0, -1},
         {0, 2, -1},
         {60, 3, 0},
         {30, 1, 2},
         {70, 1, 1},
         {80, 3, 0},

         {50, 0, 2},
         {90, 3, 0},
         {50, 3, 0},
         {90, 1, -1},
         {90, 0, 2},
         {100, 5, 2},
         {100, 6, -1},
         {50, 1, -1},
         {110, 5, 2},
         {110, 6, 0}},
    }};

    SceneData scene{SceneMappingType::UnsignedInt, 130, {}, sceneData, {
        /* To mark the scene as 3D */
        SceneFieldData{SceneField::Transformation,
            SceneMappingType::UnsignedInt, nullptr,
            SceneFieldType::Matrix4x4, nullptr},
        SceneFieldData{SceneField::Parent,
            Containers::stridedArrayView(sceneData->parents).slice(&Containers::Pair<UnsignedInt, Int>::first),
            Containers::stridedArrayView(sceneData->parents).slice(&Containers::Pair<UnsignedInt, Int>::second)},
        SceneFieldData{SceneField::Mesh,
            Containers::stridedArrayView(sceneData->meshesMaterials).slice(&Containers::Triple<UnsignedInt, UnsignedInt, Int>::first),
            Containers::stridedArrayView(sceneData->meshesMaterials).slice(&Containers::Triple<UnsignedInt, UnsignedInt, Int>::second)},
        SceneFieldData{SceneField::MeshMaterial,
            Containers::stridedArrayView(sceneData->meshesMaterials).slice(&Containers::Triple<UnsignedInt, UnsignedInt, Int>::first),
            Containers::stridedArrayView(sceneData->meshesMaterials).slice(&Containers::Triple<UnsignedInt, UnsignedInt, Int>::third)},
    }};

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(scene));
    }
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        /* Shouldn't warn about any duplicate fields */
        CORRADE_COMPARE(out.str(),
            "Trade::GltfSceneConverter::add(): parentless object 30 was not used\n");

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "scene-meshes-materials.gltf"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one scene */
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 12);
    CORRADE_COMPARE(importer->objectName(0), "Object 0");
    CORRADE_COMPARE(importer->objectName(1), "Object 10");
    CORRADE_COMPARE(importer->objectName(2), "Object 20");
    /* Object 30 didn't have a parent so it got excluded */
    CORRADE_COMPARE(importer->objectName(3), "Object 40");
    CORRADE_COMPARE(importer->objectName(4), "Object 50");
    CORRADE_COMPARE(importer->objectName(5), "Object 60");
    CORRADE_COMPARE(importer->objectName(6), "Object 70");
    CORRADE_COMPARE(importer->objectName(7), "Object 80");
    CORRADE_COMPARE(importer->objectName(8), "Object 90");
    CORRADE_COMPARE(importer->objectName(9), "Object 100");
    CORRADE_COMPARE(importer->objectName(10), "Object 110");
    CORRADE_COMPARE(importer->objectName(11), "Object 120");

    Containers::Optional<SceneData> imported = importer->scene(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->mappingBound(), 12);
    /* Not testing Parent, Transformation and ImporterState */
    CORRADE_COMPARE(imported->fieldCount(), 2 + 3);

    /* The mesh IDs are increasing even though they weren't in the original
       because we're picking unique mesh/material combinations as they
       appear */
    CORRADE_VERIFY(imported->hasField(SceneField::Mesh));
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        /* Object 50 and 90 have 3 meshes, object 100 and 110 have 2 */
         0,  3,  2,  4,  4,  4,  5,  6,  7,  8,  8,  8,  9,  9, 10, 10, 11
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->field<UnsignedInt>(SceneField::Mesh), Containers::arrayView<UnsignedInt>({
        /* Objects 0 and 40, 60 and 80 share the same mesh, objects 50 and 90
           share the same group of 3 meshes. No deduplication is done inside
           the multi-meshes at the moment, even though e.g. mesh 2 has the same
           data as mesh 1. */
         0,  0,  1,  2,  3,  4,  5,  6,  5,  2,  3,  4,  7,  8,  9, 10, 11
    }), TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(SceneField::MeshMaterial));
    /* Mapping same as Mesh */
    CORRADE_COMPARE_AS(imported->field<Int>(SceneField::MeshMaterial), Containers::arrayView({
        /* Meshes that have the same ID also have the same material
           assignment (Again, no deduplication done there) */
        -1, -1, -1,  2, -1,  0,  0,  1,  0,  2, -1,  0,  2, -1,  2,  0, -1
    }), TestSuite::Compare::Container);

    /* Meshes have their name preserved except for multi-meshes that are
       combined from meshes with different names */
    CORRADE_COMPARE(importer->meshCount(), 13);
    CORRADE_COMPARE(importer->meshName(0), "Mesh 2");
    CORRADE_COMPARE(importer->meshName(1), "Mesh 0");
    CORRADE_COMPARE(importer->meshName(2), "");
    CORRADE_COMPARE(importer->meshName(3), "");
    CORRADE_COMPARE(importer->meshName(4), "");
    CORRADE_COMPARE(importer->meshName(5), "Mesh 3");
    CORRADE_COMPARE(importer->meshName(6), "Mesh 1");
    CORRADE_COMPARE(importer->meshName(7), "Multimesh 5");
    CORRADE_COMPARE(importer->meshName(8), "Multimesh 5");
    CORRADE_COMPARE(importer->meshName(9), "Multimesh 5");
    CORRADE_COMPARE(importer->meshName(10), "Multimesh 5");
    CORRADE_COMPARE(importer->meshName(11), "Mesh 1");
    CORRADE_COMPARE(importer->meshName(12), "Mesh 4");

    /* For the multi-mesh the only way to check its relation to the input is to
       compare the primitive */
    Containers::Pair<UnsignedInt, MeshPrimitive> expectedPrimitives[]{
        {2, MeshPrimitive::Points},
        {3, MeshPrimitive::Lines},
        {4, MeshPrimitive::LineStrip}
    };
    for(Containers::Pair<UnsignedInt, MeshPrimitive> i: expectedPrimitives) {
        CORRADE_ITERATION(i.first());

        Containers::Optional<MeshData> importedMesh = importer->mesh(i.first());
        CORRADE_VERIFY(importedMesh);
        CORRADE_COMPARE(importedMesh->primitive(), i.second());
    }
}

void GltfSceneConverterTest::addSceneCustomFields() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(data.flags);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "scene-custom-fields.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    converter->setObjectName(0, "Custom field after builtin");
    converter->setObjectName(1, "To verify the 32-bit integer precision doesn't get lost along the way");
    converter->setObjectName(2, "Custom field between builtin");
    converter->setObjectName(3, "Custom field before builtin");
    converter->setObjectName(4, "Custom fields without a name, omitted");

    constexpr SceneField SceneFieldUnsignedInt = sceneFieldCustom(2322);
    constexpr SceneField SceneFieldInt = sceneFieldCustom(1766);
    /* Using huge IDs shouldn't cause any issues */
    constexpr SceneField SceneFieldFloat = sceneFieldCustom(0x7fffffff);
    constexpr SceneField SceneFieldBit = sceneFieldCustom(11);
    constexpr SceneField SceneFieldString = sceneFieldCustom(12);
    constexpr SceneField SceneFieldNameless = sceneFieldCustom(5318008);
    constexpr SceneField SceneFieldUnsupported = sceneFieldCustom(13);
    constexpr SceneField SceneFieldFloatArray = sceneFieldCustom(14);
    constexpr SceneField SceneFieldUnsignedArray = sceneFieldCustom(15);
    constexpr SceneField SceneFieldIntArray = sceneFieldCustom(16);
    constexpr SceneField SceneFieldBitArray = sceneFieldCustom(17);
    constexpr SceneField SceneFieldStringArray = sceneFieldCustom(18);

    converter->setSceneFieldName(SceneFieldUnsignedInt, "customUnsignedInt");
    converter->setSceneFieldName(SceneFieldInt, "customInt");
    converter->setSceneFieldName(SceneFieldFloat, "customFloat");
    converter->setSceneFieldName(SceneFieldBit, "customBit");
    converter->setSceneFieldName(SceneFieldString, "customString");
    /* CustomFieldNameless, ahem, doesn't have a name assigned */
    converter->setSceneFieldName(SceneFieldUnsupported, "customVector2");
    converter->setSceneFieldName(SceneFieldFloatArray, "customFloatArray");
    converter->setSceneFieldName(SceneFieldUnsignedArray, "customUnsignedArray");
    converter->setSceneFieldName(SceneFieldIntArray, "customIntArray");
    converter->setSceneFieldName(SceneFieldBitArray, "customBitArray");
    converter->setSceneFieldName(SceneFieldStringArray, "customStringArray");

    /* Adding also some builtin fields to verify the two can coexist */
    struct Scene {
        UnsignedInt parentMapping[5];
        Int parent[1];
        Containers::Pair<UnsignedInt, Vector3> translations[3];
        Containers::Pair<UnsignedInt, UnsignedInt> customUnsignedInt[2];
        Containers::Pair<UnsignedInt, Int> customInt[2];
        Containers::Pair<UnsignedInt, bool> customBit[3];
        char customStringData[11];
        Containers::Pair<UnsignedInt, UnsignedByte> customStringOffsets[3];
        Containers::Pair<UnsignedInt, UnsignedInt> customNameless[1];
        Containers::Pair<UnsignedInt, Vector2> customUnsupported[1];
        Vector3 scalings[3];
        Containers::Pair<UnsignedInt, Float> customFloat[3];
        Containers::Pair<UnsignedInt, Float> customFloatArray[6];
        Containers::Pair<UnsignedInt, UnsignedInt> customUnsignedArray[2];
        Containers::Pair<UnsignedInt, Int> customIntArray[3];
        Containers::Pair<UnsignedInt, bool> customBitArray[5];
        char customStringArrayData[18];
        Containers::Pair<UnsignedInt, UnsignedByte> customStringArrayOffsets[3];
    } sceneData[]{{
        {0, 1, 2, 3, 4},
        {-1},
        {{0, Vector3{1.0f, 2.0f, 3.0f}},
         {2, Vector3{4.0f, 5.0f, 6.0f}},
         {3, Vector3{}}}, /* Trivial, omitted */
        {{0, 176},
         {1, 4294967295}},
        {{1, Int(-2147483648)},
         {2, 25}},
        {{0, true},
         {3, false},
         {1, true}},
        "helloyesno",
        {{2, 5},
         {0, 8},
         {1, 10}},
        {{4, 666}},
        {{0, Vector2{1.0f, 2.0f}}},
        {/*0*/ Vector3{1.0f, 1.0f, 1.0f}, /* Trivial, omitted */
         /*2*/ Vector3{7.0f, 8.0f, 9.0f},
         /*3*/ Vector3{0.5f, 0.5f, 0.5f}},
        {{2, 17.5f},
         {0, 0.125f},
         {2, 25.5f}}, /* Duplicate, second ignored with a warning */
        {{3, 12.3f}, /* Mixed up order shouldn't matter for arrays */
         {1, 1.0f},
         {2, 0.25f},
         {3, 45.6f},
         {2, 0.125f},
         {3, 78.9f}},
        {{0, 1234},
         {0, 4294967295}},
        {{1, -15},
         {0, Int(-2147483648)},
         {1, 2147483647}},
        {{2, false},
         {0, false},
         {2, true},
         {0, false},
         {2, false}},
        "verynicebeautiful",
        {{3, 4},
         {0, 8},
         {3, 17}},
    }};

    SceneData scene{SceneMappingType::UnsignedInt, 5, {}, sceneData, {
        SceneFieldData{SceneField::Parent,
            Containers::stridedArrayView(sceneData->parentMapping),
            Containers::stridedArrayView(sceneData->parent).broadcasted<0>(5)},
        SceneFieldData{SceneField::Translation,
            Containers::stridedArrayView(sceneData->translations).slice(&Containers::Pair<UnsignedInt, Vector3>::first),
            Containers::stridedArrayView(sceneData->translations).slice(&Containers::Pair<UnsignedInt, Vector3>::second)},
        /* Deliberately specify custom fields among builtin ones to verify the
           order doesn't cause the output to be mixed up */
        SceneFieldData{SceneFieldUnsignedInt,
            Containers::stridedArrayView(sceneData->customUnsignedInt).slice(&Containers::Pair<UnsignedInt, UnsignedInt>::first),
            Containers::stridedArrayView(sceneData->customUnsignedInt).slice(&Containers::Pair<UnsignedInt, UnsignedInt>::second)},
        SceneFieldData{SceneFieldInt,
            Containers::stridedArrayView(sceneData->customInt).slice(&Containers::Pair<UnsignedInt, Int>::first),
            Containers::stridedArrayView(sceneData->customInt).slice(&Containers::Pair<UnsignedInt, Int>::second)},
        SceneFieldData{SceneFieldBit,
            Containers::stridedArrayView(sceneData->customBit).slice(&Containers::Pair<UnsignedInt, bool>::first),
            Containers::stridedArrayView(sceneData->customBit).slice(&Containers::Pair<UnsignedInt, bool>::second).sliceBit(0)},
        SceneFieldData{SceneFieldString,
            Containers::stridedArrayView(sceneData->customStringOffsets).slice(&Containers::Pair<UnsignedInt, UnsignedByte>::first),
            sceneData->customStringData, SceneFieldType::StringOffset8,
            Containers::stridedArrayView(sceneData->customStringOffsets).slice(&Containers::Pair<UnsignedInt, UnsignedByte>::second)},
        SceneFieldData{SceneFieldNameless,
            Containers::stridedArrayView(sceneData->customNameless).slice(&Containers::Pair<UnsignedInt, UnsignedInt>::first),
            Containers::stridedArrayView(sceneData->customNameless).slice(&Containers::Pair<UnsignedInt, UnsignedInt>::second)},
        SceneFieldData{SceneFieldUnsignedArray,
            Containers::stridedArrayView(sceneData->customUnsignedArray).slice(&Containers::Pair<UnsignedInt, UnsignedInt>::first),
            Containers::stridedArrayView(sceneData->customUnsignedArray).slice(&Containers::Pair<UnsignedInt, UnsignedInt>::second),
            SceneFieldFlag::MultiEntry},
        SceneFieldData{SceneFieldUnsupported,
            Containers::stridedArrayView(sceneData->customUnsupported).slice(&Containers::Pair<UnsignedInt, Vector2>::first),
            Containers::stridedArrayView(sceneData->customUnsupported).slice(&Containers::Pair<UnsignedInt, Vector2>::second)},
        SceneFieldData{SceneFieldIntArray,
            Containers::stridedArrayView(sceneData->customIntArray).slice(&Containers::Pair<UnsignedInt, Int>::first),
            Containers::stridedArrayView(sceneData->customIntArray).slice(&Containers::Pair<UnsignedInt, Int>::second),
            SceneFieldFlag::MultiEntry},
        SceneFieldData{SceneField::Scaling,
            Containers::stridedArrayView(sceneData->translations).slice(&Containers::Pair<UnsignedInt, Vector3>::first),
            Containers::stridedArrayView(sceneData->scalings)},
        SceneFieldData{SceneFieldFloat,
            Containers::stridedArrayView(sceneData->customFloat).slice(&Containers::Pair<UnsignedInt, Float>::first),
            Containers::stridedArrayView(sceneData->customFloat).slice(&Containers::Pair<UnsignedInt, Float>::second)},
        SceneFieldData{SceneFieldFloatArray,
            Containers::stridedArrayView(sceneData->customFloatArray).slice(&Containers::Pair<UnsignedInt, Float>::first),
            Containers::stridedArrayView(sceneData->customFloatArray).slice(&Containers::Pair<UnsignedInt, Float>::second),
            SceneFieldFlag::MultiEntry},
        SceneFieldData{SceneFieldBitArray,
            Containers::stridedArrayView(sceneData->customBitArray).slice(&Containers::Pair<UnsignedInt, bool>::first),
            Containers::stridedArrayView(sceneData->customBitArray).slice(&Containers::Pair<UnsignedInt, bool>::second).sliceBit(0),
            SceneFieldFlag::MultiEntry},
        SceneFieldData{SceneFieldStringArray,
            Containers::stridedArrayView(sceneData->customStringArrayOffsets).slice(&Containers::Pair<UnsignedInt, UnsignedByte>::first),
            sceneData->customStringArrayData, SceneFieldType::StringOffset8,
            Containers::stridedArrayView(sceneData->customStringArrayOffsets).slice(&Containers::Pair<UnsignedInt, UnsignedByte>::second),
            SceneFieldFlag::MultiEntry},
    }};

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(scene));
    }
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE_AS(out.str(),
            "Trade::GltfSceneConverter::add(): custom scene field 5318008 has no name assigned, skipping\n"
            "Trade::GltfSceneConverter::add(): custom scene field customVector2 has unsupported type Trade::SceneFieldType::Vector2, skipping\n"
            "Trade::GltfSceneConverter::add(): ignoring duplicate field customFloat for object 2\n",
            TestSuite::Compare::String);

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "scene-custom-fields.gltf"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a roundtrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    importer->configuration().group("customSceneFieldTypes")->addValue("customUnsignedInt", "UnsignedInt");
    importer->configuration().group("customSceneFieldTypes")->addValue("customInt", "Int");
    importer->configuration().group("customSceneFieldTypes")->addValue("customUnsignedArray", "UnsignedInt");
    importer->configuration().group("customSceneFieldTypes")->addValue("customIntArray", "Int");
    CORRADE_VERIFY(importer->openFile(filename));

    SceneField importedSceneFieldUnsignedInt = importer->sceneFieldForName("customUnsignedInt");
    SceneField importedSceneFieldInt = importer->sceneFieldForName("customInt");
    SceneField importedSceneFieldBit = importer->sceneFieldForName("customBit");
    SceneField importedSceneFieldString = importer->sceneFieldForName("customString");
    SceneField importedSceneFieldFloat = importer->sceneFieldForName("customFloat");
    SceneField importedSceneFieldFloatArray = importer->sceneFieldForName("customFloatArray");
    SceneField importedSceneFieldUnsignedArray = importer->sceneFieldForName("customUnsignedArray");
    SceneField importedSceneFieldIntArray = importer->sceneFieldForName("customIntArray");
    SceneField importedSceneFieldBitArray = importer->sceneFieldForName("customBitArray");
    SceneField importedSceneFieldStringArray = importer->sceneFieldForName("customStringArray");
    CORRADE_VERIFY(importedSceneFieldUnsignedInt != SceneField{});
    CORRADE_VERIFY(importedSceneFieldInt != SceneField{});
    CORRADE_VERIFY(importedSceneFieldBit != SceneField{});
    CORRADE_VERIFY(importedSceneFieldString != SceneField{});
    CORRADE_VERIFY(importedSceneFieldFloat != SceneField{});
    CORRADE_VERIFY(importedSceneFieldFloatArray != SceneField{});
    CORRADE_VERIFY(importedSceneFieldUnsignedArray != SceneField{});
    CORRADE_VERIFY(importedSceneFieldIntArray != SceneField{});
    CORRADE_VERIFY(importedSceneFieldBitArray != SceneField{});
    CORRADE_VERIFY(importedSceneFieldStringArray != SceneField{});

    /* There should be exactly one scene */
    CORRADE_COMPARE(importer->sceneCount(), 1);
    Containers::Optional<SceneData> imported = importer->scene(0);
    CORRADE_VERIFY(imported);
    /* Not testing Parent, Translation, Scaling and ImporterState */
    CORRADE_COMPARE(imported->fieldCount(), 10 + 4);

    CORRADE_VERIFY(imported->hasField(importedSceneFieldUnsignedInt));
    CORRADE_COMPARE(imported->fieldType(importedSceneFieldUnsignedInt), SceneFieldType::UnsignedInt);
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(importedSceneFieldUnsignedInt),
        Containers::arrayView({0u, 1u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->field<UnsignedInt>(importedSceneFieldUnsignedInt),
        Containers::arrayView({176u, 4294967295u}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(importedSceneFieldInt));
    CORRADE_COMPARE(imported->fieldType(importedSceneFieldInt), SceneFieldType::Int);
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(importedSceneFieldInt),
        Containers::arrayView({1u, 2u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->field<Int>(importedSceneFieldInt),
        Containers::arrayView({Int(-2147483648), 25}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(importedSceneFieldFloat));
    CORRADE_COMPARE(imported->fieldType(importedSceneFieldFloat), SceneFieldType::Float);
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(importedSceneFieldFloat),
        Containers::arrayView({0u, 2u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->field<Float>(importedSceneFieldFloat),
        Containers::arrayView({0.125f, 17.5f}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(importedSceneFieldBit));
    CORRADE_COMPARE(imported->fieldType(importedSceneFieldBit), SceneFieldType::Bit);
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(importedSceneFieldBit),
        Containers::arrayView({0u, 1u, 3u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->fieldBits(importedSceneFieldBit),
        Containers::stridedArrayView({true, true, false}).sliceBit(0),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(importedSceneFieldString));
    CORRADE_COMPARE(imported->fieldType(importedSceneFieldString), SceneFieldType::StringOffset32);
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(importedSceneFieldString),
        Containers::arrayView({0u, 1u, 2u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->fieldStrings(importedSceneFieldString),
        (Containers::StringIterable{"yes", "no", "hello"}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(importedSceneFieldFloatArray));
    CORRADE_COMPARE(imported->fieldType(importedSceneFieldFloatArray), SceneFieldType::Float);
    CORRADE_COMPARE(imported->fieldFlags(importedSceneFieldFloatArray), SceneFieldFlag::MultiEntry);
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(importedSceneFieldFloatArray),
        Containers::arrayView({1u, 2u, 2u, 3u, 3u, 3u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->field<Float>(importedSceneFieldFloatArray),
        Containers::arrayView({1.0f, 0.25f, 0.125f, 12.3f, 45.6f, 78.9f}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(importedSceneFieldUnsignedArray));
    CORRADE_COMPARE(imported->fieldType(importedSceneFieldUnsignedArray), SceneFieldType::UnsignedInt);
    CORRADE_COMPARE(imported->fieldFlags(importedSceneFieldUnsignedArray), SceneFieldFlag::MultiEntry);
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(importedSceneFieldUnsignedArray),
        Containers::arrayView({0u, 0u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->field<UnsignedInt>(importedSceneFieldUnsignedArray),
        Containers::arrayView({1234u, 4294967295u}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(importedSceneFieldIntArray));
    CORRADE_COMPARE(imported->fieldType(importedSceneFieldIntArray), SceneFieldType::Int);
    CORRADE_COMPARE(imported->fieldFlags(importedSceneFieldIntArray), SceneFieldFlag::MultiEntry);
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(importedSceneFieldIntArray),
        Containers::arrayView({0u, 1u, 1u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->field<Int>(importedSceneFieldIntArray),
        Containers::arrayView({Int(-2147483648), -15,2147483647}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(importedSceneFieldBitArray));
    CORRADE_COMPARE(imported->fieldType(importedSceneFieldBitArray), SceneFieldType::Bit);
    CORRADE_COMPARE(imported->fieldFlags(importedSceneFieldBitArray), SceneFieldFlag::MultiEntry);
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(importedSceneFieldBitArray),
        Containers::arrayView({0u, 0u, 2u, 2u, 2u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->fieldBits(importedSceneFieldBitArray),
        Containers::stridedArrayView({false, false, false, true, false}).sliceBit(0),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(importedSceneFieldStringArray));
    CORRADE_COMPARE(imported->fieldType(importedSceneFieldStringArray), SceneFieldType::StringOffset32);
    CORRADE_COMPARE(imported->fieldFlags(importedSceneFieldStringArray), SceneFieldFlag::MultiEntry);
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(importedSceneFieldStringArray),
        Containers::arrayView({0u, 3u, 3u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->fieldStrings(importedSceneFieldStringArray),
        (Containers::StringIterable{"nice", "very", "beautiful"}),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addSceneNoParentField() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(data.flags);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "scene-empty.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    Containers::Pair<UnsignedInt, Vector3> translations[3]{
        {0, {1.0f, 2.0f, 3.0f}},
        {1, {4.0f, 5.0f, 6.0f}}
    };

    SceneData scene{SceneMappingType::UnsignedInt, 2, {}, translations, {
        SceneFieldData{SceneField::Translation,
            Containers::stridedArrayView(translations).slice(&Containers::Pair<UnsignedInt, Vector3>::first),
            Containers::stridedArrayView(translations).slice(&Containers::Pair<UnsignedInt, Vector3>::second)}
    }};

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(scene));
    }
    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(),
            "Trade::GltfSceneConverter::add(): parentless object 0 was not used\n"
            "Trade::GltfSceneConverter::add(): parentless object 1 was not used\n");

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "scene-empty.gltf"),
        TestSuite::Compare::File);
}

void GltfSceneConverterTest::addSceneMultiple() {
    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "scene-empty.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    SceneData scene{SceneMappingType::UnsignedByte, 0, nullptr, {
        /* To mark the scene as 3D */
        SceneFieldData{SceneField::Transformation,
            SceneMappingType::UnsignedByte, nullptr,
            SceneFieldType::Matrix4x4, nullptr},
    }};
    CORRADE_VERIFY(converter->add(scene));

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->add(scene));
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): only one scene is supported at the moment\n");
    }

    /* The file should not get corrupted by this error, thus the same as if
       just one scene was added */
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "scene-empty.gltf"),
        TestSuite::Compare::File);
}

void GltfSceneConverterTest::addSceneInvalid() {
    auto&& data = AddSceneInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "scene-invalid.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Add two meshes to be referenced by a scene. Empty to not have to bother
       with buffers. Not valid glTF but accepted with strict=false (which gets
       reset back after) */
    if(data.scene.hasField(SceneField::Mesh)) {
        Warning silenceWarning{nullptr};
        converter->configuration().setValue("strict", false);
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Triangles, 0}));
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Triangles, 0}));
        converter->configuration().setValue("strict", true);
    }

    /* Add two materials to be referenced by a scene */
    if(data.scene.hasField(SceneField::MeshMaterial)) {
        CORRADE_VERIFY(converter->add(MaterialData{{}, {}}));
        CORRADE_VERIFY(converter->add(MaterialData{{}, {}}));
    }

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->add(data.scene));
        CORRADE_COMPARE(out.str(), Utility::format("Trade::GltfSceneConverter::add(): {}\n", data.message));
    }

    /* Add the data if not referenced to have a consistent output file */
    if(!data.scene.hasField(SceneField::Mesh)) {
        Warning silenceWarning{nullptr};
        converter->configuration().setValue("strict", false);
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Triangles, 0}));
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Triangles, 0}));
        converter->configuration().setValue("strict", true);
    }
    if(!data.scene.hasField(SceneField::MeshMaterial)) {
        CORRADE_VERIFY(converter->add(MaterialData{{}, {}}));
        CORRADE_VERIFY(converter->add(MaterialData{{}, {}}));
    }

    /* The file should not get corrupted by this error, thus the same as if
       just the data were added */
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "scene-invalid.gltf"),
        TestSuite::Compare::File);
}

void GltfSceneConverterTest::usedRequiredExtensionsAddedAlready() {
    const char vertices[4]{};
    MeshData mesh{MeshPrimitive::LineLoop, {}, vertices, {
        MeshAttributeData{MeshAttribute::Position, VertexFormat::Vector3b, 0, 1, 4}
    }};
    MaterialData material{MaterialType::Flat, {}};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "metadata-explicit-implicit-extensions.gltf");

    converter->configuration().addValue("extensionUsed", "KHR_mesh_quantization");
    converter->configuration().addValue("extensionUsed", "KHR_materials_unlit");
    converter->configuration().addValue("extensionUsed", "MAGNUM_is_amazing");
    converter->configuration().addValue("extensionRequired", "MAGNUM_can_write_json");
    converter->configuration().addValue("extensionRequired", "KHR_mesh_quantization");

    converter->beginFile(filename);
    /* This should not add KHR_mesh_quantization again to the file */
    CORRADE_VERIFY(converter->add(mesh));
    /* This should not add KHR_materials_unlit again to the file */
    CORRADE_VERIFY(converter->add(material));
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "metadata-explicit-implicit-extensions.gltf"),
        TestSuite::Compare::File);
}

void GltfSceneConverterTest::toDataButExternalBuffer() {
    const Vector3 positions[1]{};
    MeshData mesh{MeshPrimitive::LineLoop, {}, positions, {
        MeshAttributeData{MeshAttribute::Position, Containers::arrayView(positions)}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    /* Explicitly disable binary glTF (which is default for data output) to
       trigger a failure */
    converter->configuration().setValue("binary", false);

    CORRADE_VERIFY(converter->beginData());
    CORRADE_VERIFY(converter->add(mesh));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->endData());
    CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::endData(): can only write a glTF with external buffers if converting to a file\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::GltfSceneConverterTest)
