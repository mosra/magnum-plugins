/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Iterable.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/File.h>
#include <Corrade/TestSuite/Compare/FileToString.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/FormatStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/AbstractSceneConverter.h>
#include <Magnum/Trade/MaterialData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>

#include "configure.h"
#include "../../GltfImporter/Test/compareMaterials.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct GltfSceneConverterTest: TestSuite::Tester {
    explicit GltfSceneConverterTest();

    void empty();
    void outputFormatDetectionToData();
    void outputFormatDetectionToFile();
    void metadata();

    void abort();

    void addMesh();
    void addMeshNonInterleaved();
    void addMeshNoAttributes();
    void addMeshNoIndices();
    void addMeshNoIndicesNoAttributes();
    void addMeshNoIndicesNoVertices();
    void addMeshAttribute();
    void addMeshDuplicateAttribute();
    void addMeshCustomAttributeResetName();
    void addMeshCustomAttributeNoName();
    void addMeshCustomObjectIdAttributeName();
    void addMeshMultiple();
    void addMeshInvalid();

    void addImage2D();
    void addImageCompressed2D();
    void addImagePropagateFlags2D();
    void addImagePropagateConfiguration2D();
    void addImagePropagateConfigurationGroup2D();
    void addImagePropagateConfigurationUnknown2D();
    void addImageMultiple();
    void addImageNoConverterManager2D();
    void addImageExternalToData2D();
    void addImageInvalid2D();

    void addTexture();
    void addTextureMultiple();
    void addTextureInvalid();

    void addMaterial();
    void addMaterialUnusedAttributes();
    void addMaterialMultiple();
    void addMaterialInvalid();

    void addSceneEmpty();
    void addScene();
    void addSceneMeshesMaterials();
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
    Containers::Optional<bool> strict;
    bool expectedKhrMeshQuantization;
    const char* expectCustomName;
    const char* expected;
    const char* expectedWarning;
} AddMeshAttributeData[]{
    {"positions, quantized", MeshAttribute::Position, VertexFormat::Vector3s,
        nullptr, {}, true, nullptr,
        "mesh-attribute-position-quantized.gltf", nullptr},
    {"normals, quantized", MeshAttribute::Normal, VertexFormat::Vector3bNormalized,
        nullptr, {}, true, nullptr,
        "mesh-attribute-normal-quantized.gltf", nullptr},
    {"tangents", MeshAttribute::Tangent, VertexFormat::Vector4,
        nullptr, {}, false, nullptr,
        "mesh-attribute-tangent.gltf", nullptr},
    {"tangents, quantized", MeshAttribute::Tangent, VertexFormat::Vector4sNormalized,
        nullptr, {}, true, nullptr,
        "mesh-attribute-tangent-quantized.gltf", nullptr},
    {"three-component tangents", MeshAttribute::Tangent, VertexFormat::Vector3,
        nullptr, {}, false, "_TANGENT3",
        "mesh-attribute-tangent3.gltf",
        "exporting three-component mesh tangents as a custom _TANGENT3 attribute"},
    {"bitangents", MeshAttribute::Bitangent, VertexFormat::Vector3,
        nullptr, {}, false, "_BITANGENT",
        "mesh-attribute-bitangent.gltf",
        "exporting separate mesh bitangents as a custom _BITANGENT attribute"},
    {"texture coordinates", MeshAttribute::TextureCoordinates, VertexFormat::Vector2,
        nullptr, {}, false, nullptr,
        "mesh-attribute-texture-coordinates.gltf", nullptr},
    {"texture coordinates, quantized", MeshAttribute::TextureCoordinates, VertexFormat::Vector2ub,
        nullptr, {}, true, nullptr,
        "mesh-attribute-texture-coordinates-quantized.gltf", nullptr},
    {"three-component colors", MeshAttribute::Color, VertexFormat::Vector3,
        nullptr, {}, false, nullptr,
        "mesh-attribute-color3.gltf", nullptr},
    {"four-component colors", MeshAttribute::Color, VertexFormat::Vector4,
        nullptr, {}, false, nullptr,
        "mesh-attribute-color4.gltf", nullptr},
    {"four-component colors, quantized", MeshAttribute::Color, VertexFormat::Vector4usNormalized,
        nullptr, {}, false, nullptr,
        "mesh-attribute-color4us.gltf", nullptr},
    {"8-bit object ID", MeshAttribute::ObjectId, VertexFormat::UnsignedByte,
        nullptr, {}, false, nullptr,
        "mesh-attribute-objectidub.gltf", nullptr},
    {"32-bit object ID", MeshAttribute::ObjectId, VertexFormat::UnsignedInt,
        nullptr, false, false, nullptr,
        "mesh-attribute-objectidui.gltf",
        "strict mode disabled, allowing a 32-bit integer attribute _OBJECT_ID"},
    {"2x2 matrix, quantized, aligned", meshAttributeCustom(2123), VertexFormat::Matrix2x2bNormalizedAligned,
        "_ROTATION2D", {}, false, "_ROTATION2D",
        "mesh-attribute-matrix2x2b.gltf", nullptr},
    {"3x3 matrix, quantized, aligned", meshAttributeCustom(4564), VertexFormat::Matrix3x3sNormalizedAligned,
        "_TBN", {}, false, "_TBN",
        "mesh-attribute-matrix3x3s.gltf", nullptr},
    {"4x4 matrix, quantized", meshAttributeCustom(0), VertexFormat::Matrix4x4bNormalized,
        "_TRANSFORMATION", {}, false, "_TRANSFORMATION",
        "mesh-attribute-matrix4x4b.gltf", nullptr}
};

const UnsignedInt AddMeshInvalidIndices[4]{};
const Vector4d AddMeshInvalidVertices[4]{};
const struct {
    const char* name;
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
            MeshAttributeData{MeshAttribute::Position, VertexFormat::Vector3, Containers::arrayView(AddMeshInvalidVertices).prefix(std::size_t{0})}
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
};

const struct {
    const char* name;
    const char* converterPlugin;
    const char* importerPlugin;
    bool accessorNames;
    Containers::StringView dataName;
    Containers::Optional<bool> experimentalKhrTextureKtx;
    Containers::Optional<bool> strict;
    Containers::Optional<bool> bundle;
    const char* expected;
    const char* expectedOtherFile;
    const char* expectedWarning;
} AddImage2DData[]{
    {"*.gltf", "PngImageConverter", "PngImporter",
        false, {}, {}, {}, {},
        "image.gltf", "image.0.png", nullptr},
    /* The image (or the buffer) is the same as image.0.png in these three
       variants, not testing its contents */
    {"*.gltf, name", "PngImageConverter", "PngImporter",
        false, "A very pingy image", {}, {}, {},
        "image-name.gltf", nullptr, nullptr},
    {"*.gltf, bundled, accessor names", "PngImageConverter", "PngImporter",
        true, {}, {}, {}, true,
        "image-accessor-names.gltf", nullptr, nullptr},
    {"*.gltf, bundled, name, accessor names", "PngImageConverter", "PngImporter",
        true, "A rather pingy image", {}, {}, true,
        "image-name-accessor-names.gltf", nullptr, nullptr},
    {"*.glb", "PngImageConverter", "PngImporter",
        false, {}, {}, {}, {},
        "image.glb", nullptr, nullptr},
    {"*.gltf, bundled", "PngImageConverter", "PngImporter",
        false, {}, {}, {}, true,
        "image-bundled.gltf", "image-bundled.bin", nullptr},
    {"*.glb, not bundled", "PngImageConverter", "PngImporter",
        false, {}, {}, {}, false,
        "image-not-bundled.glb", "image-not-bundled.0.png", nullptr},
    {"JPEG", "JpegImageConverter", "JpegImporter",
        false, {}, {}, {}, {},
        "image-jpeg.glb", nullptr, nullptr},
    {"KTX2+Basis", "BasisKtxImageConverter", "BasisImporter",
        false, {}, {}, {}, {},
        "image-basis.glb", nullptr, nullptr},
    {"KTX2 with extension", "KtxImageConverter", "KtxImporter",
        false, {}, true, {}, {},
        "image-ktx.glb", nullptr, nullptr},
    {"KTX2 without extension", "KtxImageConverter", "KtxImporter",
        false, {}, {}, false, {},
        /* The file is exactly the same, the only difference would be with
           a texture reference */
        "image-ktx.glb", nullptr,
        "Trade::GltfSceneConverter::add(): KTX2 images can be saved using the KHR_texture_ktx extension, enable experimentalKhrTextureKtx to use it\n"
        "Trade::GltfSceneConverter::add(): strict mode disabled, allowing image/ktx2 MIME type for an image\n"},
    /* Explicitly using TGA converter from stb_image to avoid minor differences
       if Magnum's own TgaImageConverter is present as well */
    {"TGA", "StbTgaImageConverter", "TgaImporter",
        false, {}, {}, false, {},
        "image-tga.glb", nullptr,
        "Trade::GltfSceneConverter::add(): strict mode disabled, allowing image/x-tga MIME type for an image\n"},
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
    {"plugin without compressed file conversion", "PngImageConverter", ".gltf",
        ImageData2D{CompressedPixelFormat::Astc4x4RGBAUnorm, {1, 1}, DataFlags{}, "abc"},
        "PngImageConverter doesn't support Trade::ImageConverterFeature::ConvertCompressed2DToFile"},
    /** @todo plugin without data conversion once we have something that calls
        into a 3rd party binary (pngcrush?) or something that produces more
        than one file and thus can't save to data */
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
    TextureData texture;
    const char* message;
} AddTextureInvalidData[]{
    {"image out of range",
        TextureData{TextureType::Texture2D,
            SamplerFilter::Nearest,
            SamplerFilter::Nearest,
            SamplerMipmap::Base,
            SamplerWrapping::ClampToEdge,
            1},
        "texture references image 1 but only 1 were added so far"},
    {"invalid type",
        TextureData{TextureType::Texture1DArray,
            SamplerFilter::Nearest,
            SamplerFilter::Nearest,
            SamplerMipmap::Base,
            SamplerWrapping::ClampToEdge,
            0},
        "expected a 2D texture, got Trade::TextureType::Texture1DArray"},
};

const struct {
    const char* name;
    bool needsTexture;
    Containers::Optional<bool> keepDefaults;
    const char* expected;
    Containers::StringView dataName;
    MaterialData material;
    MaterialTypes expectedTypes;
    Containers::Array<MaterialAttribute> expectedRemove;
    Containers::Array<MaterialAttributeData> expectedAdd;
} AddMaterialData[]{
    {"empty", false, {}, "material-empty.gltf", {},
        MaterialData{{}, {}}, {}, {}, {}},
    {"name", false, {}, "material-name.gltf", "A nicely useless material",
        MaterialData{{}, {}}, {}, {}, {}},
    {"common", true, {}, "material-common.gltf", {},
        MaterialData{{}, {
            /* More than one texture tested in addMaterialMultiple() */
            {MaterialAttribute::AlphaMask, 0.75f}, /* unused */
            {MaterialAttribute::AlphaBlend, true},
            {MaterialAttribute::DoubleSided, true},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 0.375f},
            {MaterialAttribute::NormalTextureCoordinates, 7u},
            {MaterialAttribute::OcclusionTexture, 0u},
            {MaterialAttribute::OcclusionTextureStrength, 1.5f},
            {MaterialAttribute::OcclusionTextureCoordinates, 8u},
            {MaterialAttribute::EmissiveColor, Color3{0.5f, 0.6f, 0.7f}},
            {MaterialAttribute::EmissiveTexture, 0u},
            {MaterialAttribute::EmissiveTextureCoordinates, 9u},
        }}, {}, Containers::array({
            MaterialAttribute::AlphaMask
        }), {}},
    {"alpha mask", false, {}, "material-alpha-mask.gltf", {},
        MaterialData{{}, {
            {MaterialAttribute::AlphaMask, 0.75f},
        }}, {}, {}, {}},
    {"metallic/roughness", true, {}, "material-metallicroughness.gltf", {},
        MaterialData{{}, {
            {MaterialAttribute::BaseColor, Color4{0.1f, 0.2f, 0.3f, 0.4f}},
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::BaseColorTextureCoordinates, 10u},
            /* The Swizzle and Coordinates have to be set like this to make
               this a packed texture like glTF wants */
            {MaterialAttribute::Metalness, 0.25f},
            {MaterialAttribute::Roughness, 0.75f},
            {MaterialAttribute::MetalnessTexture, 0u},
            {MaterialAttribute::MetalnessTextureSwizzle, MaterialTextureSwizzle::B},
            {MaterialAttribute::MetalnessTextureCoordinates, 11u},
            {MaterialAttribute::RoughnessTexture, 0u},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
            {MaterialAttribute::RoughnessTextureCoordinates, 11u},
        }}, MaterialType::PbrMetallicRoughness, Containers::array({
            MaterialAttribute::MetalnessTexture,
            MaterialAttribute::MetalnessTextureSwizzle,
            MaterialAttribute::RoughnessTextureSwizzle,
            MaterialAttribute::RoughnessTexture
        }), Containers::array({
            MaterialAttributeData{MaterialAttribute::NoneRoughnessMetallicTexture, 0u}
        })},
    {"metallic/roughness, packed texture attribute", true, {}, "material-metallicroughness.gltf", {},
        MaterialData{{}, {
            {MaterialAttribute::BaseColor, Color4{0.1f, 0.2f, 0.3f, 0.4f}},
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::BaseColorTextureCoordinates, 10u},
            {MaterialAttribute::Metalness, 0.25f},
            {MaterialAttribute::Roughness, 0.75f},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 0u},
            {MaterialAttribute::MetalnessTextureCoordinates, 11u},
            {MaterialAttribute::RoughnessTextureCoordinates, 11u},
        }}, MaterialType::PbrMetallicRoughness, {}, {}},
    {"metallic/roughness, global texture attributes", true, {}, "material-metallicroughness.gltf", {},
        MaterialData{{}, {
            {MaterialAttribute::BaseColor, Color4{0.1f, 0.2f, 0.3f, 0.4f}},
            {MaterialAttribute::BaseColorTexture, 0u},
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
            {MaterialAttribute::TextureCoordinates, 11u}
        }}, MaterialType::PbrMetallicRoughness, Containers::array({
            MaterialAttribute::MetalnessTexture,
            MaterialAttribute::MetalnessTextureSwizzle,
            MaterialAttribute::RoughnessTextureSwizzle,
            MaterialAttribute::RoughnessTexture,
            MaterialAttribute::TextureCoordinates
        }), Containers::array({
            MaterialAttributeData{MaterialAttribute::NoneRoughnessMetallicTexture, 0u},
            MaterialAttributeData{MaterialAttribute::MetalnessTextureCoordinates, 11u},
            MaterialAttributeData{MaterialAttribute::RoughnessTextureCoordinates, 11u},
        })},
    {"explicit default texture swizzle", true, {}, "material-default-texture-swizzle.gltf", {},
        MaterialData{{}, {
            /* The swizzles are just checked but not written anywhere, so this
               is the same as specifying just the textures alone */
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureSwizzle, MaterialTextureSwizzle::RGB},
            {MaterialAttribute::OcclusionTexture, 0u},
            {MaterialAttribute::OcclusionTextureSwizzle, MaterialTextureSwizzle::R},
            /* No EmissiveTextureSwizzle or BaseColorTextureSwizzle attributes,
               Metallic and Roughness textures won't work with defaults */
        }}, {}, Containers::array({
            MaterialAttribute::NormalTextureSwizzle,
            MaterialAttribute::OcclusionTextureSwizzle
        }), {}},
    {"default values kept", true, true, "material-defaults-kept.gltf", {},
        MaterialData{{}, {
            /* Textures have to be present, otherwise the texture-related
               properties are not saved */
            {MaterialAttribute::AlphaBlend, false},
            {MaterialAttribute::DoubleSided, false},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 1.0f},
            {MaterialAttribute::NormalTextureCoordinates, 0u},
            {MaterialAttribute::OcclusionTexture, 0u},
            {MaterialAttribute::OcclusionTextureStrength, 1.0f},
            {MaterialAttribute::OcclusionTextureCoordinates, 0u},
            {MaterialAttribute::EmissiveColor, 0x000000_rgbf},
            {MaterialAttribute::EmissiveTexture, 0u},
            {MaterialAttribute::EmissiveTextureCoordinates, 0u},
            {MaterialAttribute::BaseColor, 0xffffffff_rgbaf},
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::BaseColorTextureCoordinates, 0u},
            {MaterialAttribute::Metalness, 1.0f},
            {MaterialAttribute::Roughness, 1.0f},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 0u},
            {MaterialAttribute::MetalnessTextureCoordinates, 0u},
            {MaterialAttribute::RoughnessTextureCoordinates, 0u},
        }}, MaterialType::PbrMetallicRoughness, {}, {}},
    {"default values omitted", true, {}, "material-defaults-omitted.gltf", {},
        MaterialData{{}, {
            /* Same as above */
            {MaterialAttribute::AlphaBlend, false},
            {MaterialAttribute::DoubleSided, false},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 1.0f},
            {MaterialAttribute::NormalTextureCoordinates, 0u},
            {MaterialAttribute::OcclusionTexture, 0u},
            {MaterialAttribute::OcclusionTextureStrength, 1.0f},
            {MaterialAttribute::OcclusionTextureCoordinates, 0u},
            {MaterialAttribute::EmissiveColor, 0x000000_rgbf},
            {MaterialAttribute::EmissiveTexture, 0u},
            {MaterialAttribute::EmissiveTextureCoordinates, 0u},
            {MaterialAttribute::BaseColor, 0xffffffff_rgbaf},
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::BaseColorTextureCoordinates, 0u},
            {MaterialAttribute::Metalness, 1.0f},
            {MaterialAttribute::Roughness, 1.0f},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 0u},
            {MaterialAttribute::MetalnessTextureCoordinates, 0u},
            {MaterialAttribute::RoughnessTextureCoordinates, 0u},
        }}, MaterialType::PbrMetallicRoughness, Containers::array({
            MaterialAttribute::AlphaBlend,
            MaterialAttribute::DoubleSided,
            MaterialAttribute::NormalTextureScale,
            MaterialAttribute::NormalTextureCoordinates,
            MaterialAttribute::OcclusionTextureStrength,
            MaterialAttribute::OcclusionTextureCoordinates,
            MaterialAttribute::EmissiveColor,
            MaterialAttribute::EmissiveTextureCoordinates,
            MaterialAttribute::BaseColor,
            MaterialAttribute::BaseColorTextureCoordinates,
            MaterialAttribute::Metalness,
            MaterialAttribute::Roughness,
            MaterialAttribute::MetalnessTextureCoordinates,
            MaterialAttribute::RoughnessTextureCoordinates,
        }), {}},
    {"alpha mask default values kept", false, true, "material-alpha-mask-defaults-kept.gltf", {},
        MaterialData{{}, {
            {MaterialAttribute::AlphaMask, 0.5f},
        }}, {}, {}, {}},
    {"alpha mask default values omitted", false, {}, "material-alpha-mask-defaults-omitted.gltf", {},
        MaterialData{{}, {
            /* Same as above */
            {MaterialAttribute::AlphaMask, 0.5f},
        }}, {}, {}, {}},
    {"unlit", false, {}, "material-unlit.gltf", {},
        /* PbrMetallicRoughness should not get added on import, only Flat */
        MaterialData{MaterialType::Flat, {
            {MaterialAttribute::BaseColor, Color4{0.1f, 0.2f, 0.3f, 0.4f}},
            /* To avoid data loss, non-flat properties are still written, even
               though they make no sense for a flat-shaded material */
            {MaterialAttribute::Roughness, 0.57f}
        }}, MaterialType::Flat, {}, {}}
};

const struct {
    const char* name;
    bool needsTexture;
    const char* expected;
    MaterialData material;
    const char* expectedWarning;
} AddMaterialUnusedAttributesData[]{
    {"texture properties but no textures", false, "material-empty.gltf",
        MaterialData{{}, {
            {MaterialAttribute::BaseColorTextureCoordinates, 5u},
            {MaterialAttribute::EmissiveTextureCoordinates, 10u},
            {MaterialAttribute::MetalnessTextureCoordinates, 6u},
            {MaterialAttribute::NormalTextureCoordinates, 8u},
            {MaterialAttribute::NormalTextureScale, 1.5f},
            {MaterialAttribute::OcclusionTextureCoordinates, 9u},
            {MaterialAttribute::OcclusionTextureStrength, 0.3f},
            {MaterialAttribute::RoughnessTextureCoordinates, 7u},
        }},
        "Trade::GltfSceneConverter::add(): material attribute BaseColorTextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute EmissiveTextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute MetalnessTextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute NormalTextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute NormalTextureScale was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute OcclusionTextureCoordinates was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute OcclusionTextureStrength was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute RoughnessTextureCoordinates was not used\n"},
    {"unused attributes and layers", false, "material-empty.gltf",
        MaterialData{{}, {
            {MaterialAttribute::Shininess, 15.0f},
            {MaterialAttribute::SpecularTexture, 0u},
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.5f},
        }, {2, 3, 4}},
        "Trade::GltfSceneConverter::add(): material attribute Shininess was not used\n"
        "Trade::GltfSceneConverter::add(): material attribute SpecularTexture was not used\n"
        /* It especially shouldn't warn about unused attribute LayerName */
        "Trade::GltfSceneConverter::add(): material layer 1 (ClearCoat) was not used\n"
        "Trade::GltfSceneConverter::add(): material layer 2 was not used\n"},
};

const struct {
    const char* name;
    MaterialData material;
    const char* message;
} AddMaterialInvalidData[]{
    {"texture out of bounds",
        MaterialData{{}, {
            {MaterialAttribute::MetalnessTexture, 1u},
        }}, "material attribute MetalnessTexture value 1 out of range for 1 textures"},
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
        }}, "unsupported B packing of an occlusion texture"}
};

const struct {
    const char* name;
    Containers::StringView dataName;
    UnsignedShort offset;
    const char* expected;
} AddSceneData[]{
    {"", {}, 0, "scene.gltf"},
    {"name", "A simple sceen!", 0, "scene-name.gltf"},
    {"object ID with an offset", {}, 350, "scene.gltf"}
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
              &GltfSceneConverterTest::abort});

    addInstancedTests({&GltfSceneConverterTest::addMesh},
        Containers::arraySize(FileVariantWithNamesData));

    addTests({&GltfSceneConverterTest::addMeshNonInterleaved,
              &GltfSceneConverterTest::addMeshNoAttributes,
              &GltfSceneConverterTest::addMeshNoIndices});

    addInstancedTests({&GltfSceneConverterTest::addMeshNoIndicesNoAttributes,
                       &GltfSceneConverterTest::addMeshNoIndicesNoVertices},
        Containers::arraySize(FileVariantData));

    addInstancedTests({&GltfSceneConverterTest::addMeshAttribute},
        Containers::arraySize(AddMeshAttributeData));

    addTests({&GltfSceneConverterTest::addMeshDuplicateAttribute,
              &GltfSceneConverterTest::addMeshCustomAttributeResetName,
              &GltfSceneConverterTest::addMeshCustomAttributeNoName,
              &GltfSceneConverterTest::addMeshCustomObjectIdAttributeName,

              &GltfSceneConverterTest::addMeshMultiple});

    addInstancedTests({&GltfSceneConverterTest::addMeshInvalid},
        Containers::arraySize(AddMeshInvalidData));

    addInstancedTests({&GltfSceneConverterTest::addImage2D},
        Containers::arraySize(AddImage2DData));

    addTests({&GltfSceneConverterTest::addImageCompressed2D,
              &GltfSceneConverterTest::addImagePropagateFlags2D,
              &GltfSceneConverterTest::addImagePropagateConfiguration2D,
              &GltfSceneConverterTest::addImagePropagateConfigurationGroup2D,
              &GltfSceneConverterTest::addImagePropagateConfigurationUnknown2D,
              &GltfSceneConverterTest::addImageMultiple,
              &GltfSceneConverterTest::addImageNoConverterManager2D,
              &GltfSceneConverterTest::addImageExternalToData2D});

    addInstancedTests({&GltfSceneConverterTest::addImageInvalid2D},
        Containers::arraySize(AddImageInvalid2DData));

    addInstancedTests({&GltfSceneConverterTest::addTexture},
        Containers::arraySize(AddTextureData));

    addTests({&GltfSceneConverterTest::addTextureMultiple});

    addInstancedTests({&GltfSceneConverterTest::addTextureInvalid},
        Containers::arraySize(AddTextureInvalidData));

    addInstancedTests({&GltfSceneConverterTest::addMaterial},
        Containers::arraySize(AddMaterialData));

    addInstancedTests({&GltfSceneConverterTest::addMaterialUnusedAttributes},
        Containers::arraySize(AddMaterialUnusedAttributesData));

    addTests({&GltfSceneConverterTest::addMaterialMultiple});

    addInstancedTests({&GltfSceneConverterTest::addMaterialInvalid},
        Containers::arraySize(AddMaterialInvalidData));

    addTests({&GltfSceneConverterTest::addSceneEmpty});

    addInstancedTests({&GltfSceneConverterTest::addScene},
        Containers::arraySize(AddSceneData));

    addTests({&GltfSceneConverterTest::addSceneMeshesMaterials,
              &GltfSceneConverterTest::addSceneNoParentField,
              &GltfSceneConverterTest::addSceneMultiple});

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

    /* By default don't write the generator name for smaller test files */
    CORRADE_INTERNAL_ASSERT_EXPRESSION(_converterManager.metadata("GltfSceneConverter"))->configuration().setValue("generator", "");
    if(PluginManager::PluginMetadata* metadata = _imageConverterManager.metadata("KtxImageConverter"))
        metadata->configuration().setValue("writerName", "");

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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

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

    const struct Vertex {
        UnsignedInt padding0;
        Vector3 position;
        UnsignedInt padding1[2];
        Vector3 normal;
    } vertices[]{
        {0xaaaaaaaau,
         {1.0f, 2.0f, 3.0f},
         {0xffffffffu, 0xeeeeeeeeu},
         {7.0f, 8.0f, 9.0f}},
        {0xddddddddu,
         {4.0f, 5.0f, 6.0f},
         {0xccccccccu, 0xbbbbbbbbu},
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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

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

void GltfSceneConverterTest::addMeshNonInterleaved() {
    const struct {
        UnsignedInt padding0[1];
        Vector3 positions[2];
        UnsignedInt padding1[5];
        Vector3 normals[2];
        UnsignedInt padding2[2];
    } vertices[]{{
        {0xaaaaaaaau},
        {{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}},
        {0xffffffffu, 0xeeeeeeeeu, 0xddddddddu, 0xccccccccu, 0xbbbbbbbbu},
        {{7.0f, 8.0f, 9.0f}, {10.0f, 11.0f, 12.0f}},
        {0x99999999u, 0x88888888u}
    }};

    const UnsignedShort indices[] {
        0, 2, 1, 2, 1, 2
    };

    MeshData mesh{MeshPrimitive::Lines,
        {}, indices, MeshIndexData{indices},
        {}, vertices, {
            MeshAttributeData{MeshAttribute::Position, Containers::arrayView(vertices->positions)},
            MeshAttributeData{MeshAttribute::Normal, Containers::arrayView(vertices->normals)}
        }
    };

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-noninterleaved.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));
    CORRADE_VERIFY(converter->add(mesh));
    CORRADE_VERIFY(converter->endFile());

    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-noninterleaved.gltf"),
        TestSuite::Compare::File);
    /* Not testing the .bin file as it won't get any special treatment compared
       to addMesh() above */

    /* Verify various expectations that might be missed when just looking at
       the file */
    const Containers::Optional<Containers::String> gltf = Utility::Path::readString(filename);
    CORRADE_VERIFY(gltf);
    /* There should be no byteStride as the attributes are all tightly packed */
    CORRADE_VERIFY(!gltf->contains("byteStride"));
    /* No extensions are needed for this simple case */
    CORRADE_VERIFY(!gltf->contains("extensionsUsed"));
    CORRADE_VERIFY(!gltf->contains("extensionsRequired"));

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->primitive(), MeshPrimitive::Lines);

    CORRADE_COMPARE(imported->indexType(), MeshIndexType::UnsignedShort);
    CORRADE_COMPARE_AS(imported->indices<UnsignedShort>(),
        Containers::arrayView<UnsignedShort>({0, 2, 1, 2, 1, 2}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(imported->attributeCount(), 2);
    /* The attributes are sorted by name by the importer to handle duplicates */
    CORRADE_COMPARE(imported->attributeName(1), MeshAttribute::Position);
    CORRADE_COMPARE(imported->attributeName(0), MeshAttribute::Normal);
    CORRADE_COMPARE(imported->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE(imported->attributeFormat(MeshAttribute::Normal), VertexFormat::Vector3);
    CORRADE_COMPARE(imported->attributeStride(MeshAttribute::Position), sizeof(Vector3));
    CORRADE_COMPARE(imported->attributeStride(MeshAttribute::Normal), sizeof(Vector3));
    CORRADE_COMPARE_AS(imported->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView(vertices->positions),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->attribute<Vector3>(MeshAttribute::Normal),
        Containers::arrayView(vertices->normals),
        TestSuite::Compare::Container);
}

void GltfSceneConverterTest::addMeshNoAttributes() {
    const UnsignedByte indices[] {
        0, 2, 1, 2, 1, 2
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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

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
        Containers::arrayView<UnsignedByte>({0, 2, 1, 2, 1, 2}),
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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

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
    auto&& data = FileVariantData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    /* Attribute-less meshes are not valid glTF, but we accept that under a
       flag */
    converter->configuration().setValue("strict", false);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-no-indices-no-attributes" + data.suffix);
    CORRADE_VERIFY(converter->beginFile(filename));

    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::TriangleFan, 0}));
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): strict mode disabled, allowing an attribute-less mesh\n");
    }

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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

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
    auto&& data = FileVariantData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    MeshData mesh{MeshPrimitive::TriangleStrip, nullptr, {
        MeshAttributeData{MeshAttribute::Position, VertexFormat::Vector3, 0, 0, sizeof(Vector3)}
    }, 0};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    /* Vertex-less meshes are not valid glTF, but we accept that under a
       flag */
    converter->configuration().setValue("strict", false);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-no-indices-no-vertices" + data.suffix);
    CORRADE_VERIFY(converter->beginFile(filename));

    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(mesh));
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): strict mode disabled, allowing a mesh with zero vertices\n");
    }

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-no-indices-no-vertices" + data.suffix),
        TestSuite::Compare::File);
    /* There should be no (empty) bin file written */
    if(!data.binary)
        CORRADE_VERIFY(!Utility::Path::exists(Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-no-indices-no-vertices.bin")));

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

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

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);
    CORRADE_VERIFY(converter->beginFile(filename));

    if(data.customName)
        converter->setMeshAttributeName(data.attribute, data.customName);
    if(data.strict)
        converter->configuration().setValue("strict", *data.strict);

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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

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

void GltfSceneConverterTest::addMeshDuplicateAttribute() {
    const Vector4 vertices[3]{};
    const MeshAttribute jointsAttribute = meshAttributeCustom(0);
    const MeshAttribute weightsAttribute = meshAttributeCustom(1);
    const MeshAttribute customAttribute = meshAttributeCustom(2);

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
        /* Magnum custom but glTF builtin numbered attributes, should have a
           number */
        MeshAttributeData{jointsAttribute,
            VertexFormat::Vector4ub, Containers::stridedArrayView(vertices)},
        MeshAttributeData{weightsAttribute,
            VertexFormat::Vector4ubNormalized, Containers::stridedArrayView(vertices)},
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
        /* Secondary custom but glTF builtin numbered attributes */
        MeshAttributeData{jointsAttribute,
            VertexFormat::Vector4us, Containers::stridedArrayView(vertices)},
        MeshAttributeData{weightsAttribute,
            VertexFormat::Vector4, Containers::stridedArrayView(vertices)},
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
    /* Magnum doesn't have a builtin enum for these two yet, but the plugin
       will recognize them */
    converter->setMeshAttributeName(jointsAttribute, "JOINTS");
    converter->setMeshAttributeName(weightsAttribute, "WEIGHTS");
    converter->setMeshAttributeName(customAttribute, "_YOLO");
    CORRADE_VERIFY(converter->add(mesh));
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "mesh-duplicate-attribute.gltf"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_COMPARE(importer->meshCount(), 1);

    const MeshAttribute importedJointsAttribute = importer->meshAttributeForName("JOINTS");
    const MeshAttribute importedWeightsAttribute = importer->meshAttributeForName("WEIGHTS");
    const MeshAttribute importedSecondaryPositionAttribute = importer->meshAttributeForName("_POSITION_1");
    const MeshAttribute importedSecondaryObjectIdAttribute = importer->meshAttributeForName("_OBJECT_ID_1");
    const MeshAttribute importedCustomAttribute = importer->meshAttributeForName("_YOLO");
    const MeshAttribute importedSecondaryCustomAttribute = importer->meshAttributeForName("_YOLO_1");
    CORRADE_VERIFY(importedJointsAttribute != MeshAttribute{});
    CORRADE_VERIFY(importedWeightsAttribute != MeshAttribute{});
    CORRADE_VERIFY(importedSecondaryPositionAttribute != MeshAttribute{});
    CORRADE_VERIFY(importedSecondaryObjectIdAttribute != MeshAttribute{});
    CORRADE_VERIFY(importedCustomAttribute != MeshAttribute{});
    CORRADE_VERIFY(importedSecondaryCustomAttribute != MeshAttribute{});

    Containers::Optional<MeshData> imported = importer->mesh(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->primitive(), MeshPrimitive::TriangleFan);
    CORRADE_VERIFY(!imported->isIndexed());
    CORRADE_COMPARE(imported->attributeCount(), 15);

    /* CgltfImporter (stable-)sorts the attributes first to figure out the
       numbering. Check that the numbers match by comparing types. */

    CORRADE_COMPARE(imported->attributeName(0), MeshAttribute::Color);
    CORRADE_COMPARE(imported->attributeFormat(0), VertexFormat::Vector4);
    CORRADE_COMPARE(imported->attributeName(1), MeshAttribute::Color);
    CORRADE_COMPARE(imported->attributeFormat(1), VertexFormat::Vector3ubNormalized);

    CORRADE_COMPARE(imported->attributeName(2), importedJointsAttribute);
    CORRADE_COMPARE(imported->attributeFormat(2), VertexFormat::Vector4ub);
    CORRADE_COMPARE(imported->attributeName(3), importedJointsAttribute);
    CORRADE_COMPARE(imported->attributeFormat(3), VertexFormat::Vector4us);

    CORRADE_COMPARE(imported->attributeName(4), MeshAttribute::Position);
    CORRADE_COMPARE(imported->attributeFormat(4), VertexFormat::Vector3);

    CORRADE_COMPARE(imported->attributeName(5), MeshAttribute::TextureCoordinates);
    CORRADE_COMPARE(imported->attributeFormat(5), VertexFormat::Vector2);
    CORRADE_COMPARE(imported->attributeName(6), MeshAttribute::TextureCoordinates);
    CORRADE_COMPARE(imported->attributeFormat(6), VertexFormat::Vector2usNormalized);
    CORRADE_COMPARE(imported->attributeName(7), MeshAttribute::TextureCoordinates);
    CORRADE_COMPARE(imported->attributeFormat(7), VertexFormat::Vector2ubNormalized);

    CORRADE_COMPARE(imported->attributeName(8), importedWeightsAttribute);
    CORRADE_COMPARE(imported->attributeFormat(8), VertexFormat::Vector4ubNormalized);
    CORRADE_COMPARE(imported->attributeName(9), importedWeightsAttribute);
    CORRADE_COMPARE(imported->attributeFormat(9), VertexFormat::Vector4);

    CORRADE_COMPARE(imported->attributeName(10), MeshAttribute::ObjectId);
    CORRADE_COMPARE(imported->attributeFormat(10), VertexFormat::UnsignedShort);
    CORRADE_COMPARE(imported->attributeName(11), importedSecondaryObjectIdAttribute);
    CORRADE_COMPARE(imported->attributeFormat(11), VertexFormat::UnsignedByte);

    CORRADE_COMPARE(imported->attributeName(12), importedSecondaryPositionAttribute);
    /* There's no other allowed type without extra additions, so just trust
       it's the correct one */
    CORRADE_COMPARE(imported->attributeFormat(12), VertexFormat::Vector3);

    CORRADE_COMPARE(imported->attributeName(13), importedCustomAttribute);
    CORRADE_COMPARE(imported->attributeFormat(13), VertexFormat::Float);
    CORRADE_COMPARE(imported->attributeName(14), importedSecondaryCustomAttribute);
    CORRADE_COMPARE(imported->attributeFormat(14), VertexFormat::ByteNormalized);
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
    const char vertices[4]{};
    MeshData mesh{MeshPrimitive::LineLoop, {}, vertices, {
        MeshAttributeData{meshAttributeCustom(31434), VertexFormat::Float, 0, 1, 4}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "mesh-custom-attribute-no-name.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Set two names for something else (which shouldn't get used) */
    converter->setMeshAttributeName(meshAttributeCustom(30560), "_YOLO");
    converter->setMeshAttributeName(meshAttributeCustom(31995), "_MEH");

    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(mesh));
        CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): no name set for Trade::MeshAttribute::Custom(31434), exporting as _31434\n");
    }

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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

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
    const UnsignedInt indices[]{0, 1, 0};

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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    CORRADE_COMPARE(importer->meshCount(), 2);

    Containers::Optional<MeshData> triangleFan = importer->mesh(0);
    CORRADE_VERIFY(triangleFan);
    CORRADE_VERIFY(triangleFan->isIndexed());
    CORRADE_COMPARE(triangleFan->attributeCount(), 1);
    CORRADE_COMPARE_AS(triangleFan->indices<UnsignedInt>(),
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

    /* For images alone, no extensions should be recorded -- they only get so
       if a texture references the image */
    CORRADE_VERIFY(!gltf->contains("extensionsUsed"));
    CORRADE_VERIFY(!gltf->contains("extensionsRequired"));

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");
    if(_importerManager.loadState(data.importerPlugin) == PluginManager::LoadState::NotFound)
        CORRADE_SKIP(data.importerPlugin << "plugin not found, cannot test a rountrip");

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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");
    if(_importerManager.loadState("KtxImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImporter plugin not found, cannot test a rountrip");

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

void GltfSceneConverterTest::addImagePropagateFlags2D() {
    if(_imageConverterManager.loadState("TgaImageConverter") == PluginManager::LoadState::NotFound ||
       /* TgaImageConverter is also provided by StbImageConverter, which
          doesn't make use of Flags::Verbose, so that one can't be used to test
          anything */
       _imageConverterManager.metadata("TgaImageConverter")->name() != "TgaImageConverter")
        CORRADE_SKIP("(Non-aliased) TgaImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->addFlags(SceneConverterFlag::Verbose);

    converter->configuration().setValue("imageConverter", "TgaImageConverter");
    /* So it allows using a TGA image */
    converter->configuration().setValue("strict", false);

    CORRADE_VERIFY(converter->beginData());

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));
    CORRADE_COMPARE(out.str(), "Trade::TgaImageConverter::convertToData(): converting from RGB to BGR\n");

    CORRADE_VERIFY(converter->endData());

    /* No need to test any roundtrip or file contents here, the verbose output
       doesn't affect anything in the output */
}

void GltfSceneConverterTest::addImagePropagateConfiguration2D() {
    if(_imageConverterManager.loadState("KtxImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    converter->configuration().setValue("imageConverter", "KtxImageConverter");
    converter->configuration().setValue("experimentalKhrTextureKtx", true);

    Utility::ConfigurationGroup* imageConverterConfiguration = converter->configuration().group("imageConverter");
    CORRADE_VERIFY(imageConverterConfiguration);
    imageConverterConfiguration->setValue("writerName", "MAGNUM IS AWESOME");

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

void GltfSceneConverterTest::addImagePropagateConfigurationUnknown2D() {
    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Utility::ConfigurationGroup* imageConverterConfiguration = converter->configuration().group("imageConverter");
    CORRADE_VERIFY(imageConverterConfiguration);
    imageConverterConfiguration->setValue("quality", 42);

    CORRADE_VERIFY(converter->beginData());

    std::ostringstream out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));
    CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::add(): option quality not recognized by PngImageConverter\n");

    /* No need to test anything apart from the message above */
    CORRADE_VERIFY(converter->endData());
}

void GltfSceneConverterTest::addImagePropagateConfigurationGroup2D() {
    if(_imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Utility::ConfigurationGroup* imageConverterConfiguration = converter->configuration().group("imageConverter");
    CORRADE_VERIFY(imageConverterConfiguration);
    imageConverterConfiguration->addGroup("exif");

    CORRADE_VERIFY(converter->beginData());

    std::ostringstream out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));
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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");
    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test a rountrip");
    if(_importerManager.loadState("JpegImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("JpegImporter plugin not found, cannot test a rountrip");

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

void GltfSceneConverterTest::addImageNoConverterManager2D() {
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

void GltfSceneConverterTest::addImageExternalToData2D() {
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

    /* Second image Basis, unused */
    converter->configuration().setValue("imageConverter", "BasisKtxImageConverter");
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}, "Basis-encoded, unused"));

    /* Third image KTX */
    converter->configuration().setValue("imageConverter", "KtxImageConverter");
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));

    /* Reference third and first image from two textures */
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        2}));
    CORRADE_VERIFY(converter->add(TextureData{TextureType::Texture2D,
        SamplerFilter::Nearest,
        SamplerFilter::Nearest,
        SamplerMipmap::Base,
        SamplerWrapping::ClampToEdge,
        0}));

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "texture-multiple.gltf"),
        TestSuite::Compare::File);

    /* Verify various expectations that might be missed when just looking at
       the file */
    const Containers::Optional<Containers::String> gltf = Utility::Path::readString(filename);
    CORRADE_VERIFY(gltf);
    /* The Basis image is not referenced and thus there should be no extension
       reference */
    CORRADE_VERIFY(!gltf->contains("KHR_texture_basisu"));

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    importer->configuration().setValue("experimentalKhrTextureKtx", true);

    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be two textures referencing two out of the three images */
    CORRADE_COMPARE(importer->textureCount(), 2);
    Containers::Optional<TextureData> imported0 = importer->texture(0);
    CORRADE_VERIFY(imported0);
    CORRADE_COMPARE(imported0->image(), 2);

    Containers::Optional<TextureData> imported1 = importer->texture(1);
    CORRADE_VERIFY(imported1);
    CORRADE_COMPARE(imported1->image(), 0);
}

void GltfSceneConverterTest::addTextureInvalid() {
    auto&& data = AddTextureInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "image.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Add an image to be referenced by a texture */
    CORRADE_VERIFY(converter->add(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, "yey"}));

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->add(data.texture));
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfSceneConverter::add(): {}\n", data.message));
    }

    /* The file should not get corrupted by this error, thus the same as if
       just the image was added */
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "image.gltf"),
        TestSuite::Compare::File);
}

namespace {

MaterialData filterMaterialAttributes(const MaterialData& material, MaterialTypes types, Containers::ArrayView<const MaterialAttribute> remove, Containers::ArrayView<const MaterialAttributeData> add) {
    /* Currently only the base layer */
    CORRADE_INTERNAL_ASSERT(material.layerCount() == 1);

    Containers::Array<MaterialAttributeData> out;

    /* O(n^2), yes, sorry. Need to be fixed if made into a public API. */
    for(UnsignedInt i = 0; i != material.attributeCount(); ++i) {
        bool excluded = false;
        for(std::size_t j = 0; j != remove.size(); ++j) {
            if(material.attributeName(i) == materialAttributeName(remove[j])) {
                excluded = true;
                break;
            }
        }

        if(!excluded) arrayAppend(out, material.attributeData()[i]);
    }

    arrayAppend(out, add);

    return MaterialData{types, std::move(out)};
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

    /* There should be no warning about unused attributes */
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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* Disable Phong material fallback (enabled by default for compatibility),
       no use for that here */
    importer->configuration().setValue("phongMaterialFallback", false);

    /* There should be exactly one material, looking exactly the same as the
       original */
    CORRADE_COMPARE(importer->materialCount(), 1);
    Containers::Optional<MaterialData> imported = importer->material(0);
    CORRADE_VERIFY(imported);
    compareMaterials(*imported, filterMaterialAttributes(data.material, data.expectedTypes, data.expectedRemove, data.expectedAdd));
}

void GltfSceneConverterTest::addMaterialUnusedAttributes() {
    auto&& data = AddMaterialUnusedAttributesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(data.needsTexture && _imageConverterManager.loadState("PngImageConverter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImageConverter plugin not found, cannot test");

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

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
        CORRADE_COMPARE(out.str(), data.expectedWarning);
    }

    /* Testing the contents would be too time-consuming, the file itself has to
       suffice */
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);
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
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(filename));

    /* Disable Phong material fallback (enabled by default for compatibility),
       no use for that here */
    importer->configuration().setValue("phongMaterialFallback", false);

    /* There should be two materials referencing two textures */
    CORRADE_COMPARE(importer->materialCount(), 2);
    Containers::Optional<MaterialData> imported0 = importer->material(0);
    CORRADE_VERIFY(imported0);
    compareMaterials(*imported0, material0);

    Containers::Optional<MaterialData> imported1 = importer->material(1);
    CORRADE_VERIFY(imported1);
    compareMaterials(*imported1, material1);
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

void GltfSceneConverterTest::addSceneEmpty() {
    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "scene-empty.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    CORRADE_VERIFY(converter->add(SceneData{SceneMappingType::UnsignedByte, 0, nullptr, {
        /* To mark the scene as 3D */
        SceneFieldData{SceneField::Transformation,
            SceneMappingType::UnsignedByte, nullptr,
            SceneFieldType::Matrix4x4, nullptr},
    }}));

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "scene-empty.gltf"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

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
}

void GltfSceneConverterTest::addScene() {
    auto&& data = AddSceneData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, data.expected);
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Deliberately using a 16-bit mapping to trigger accidentally hardcoded
       UnsignedInt inside add(SceneData). The optionally added offset *should
       not* change the output in any way. */
    struct Scene {
        Containers::Pair<UnsignedShort, Int> parents[5];
        Containers::Pair<UnsignedShort, Matrix4> transformations[5];
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

        /* One object should be without any transformation */
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
        /* Ignored custom field, produces another warning */
        SceneFieldData{sceneFieldCustom(5318008),
            Containers::stridedArrayView(sceneData->trs).slice(&Scene::Trs::mapping),
            Containers::stridedArrayView(sceneData->trs).slice(&Scene::Trs::translation)},
        SceneFieldData{SceneField::Scaling,
            Containers::stridedArrayView(sceneData->trs).slice(&Scene::Trs::mapping),
            Containers::stridedArrayView(sceneData->trs).slice(&Scene::Trs::scaling)},
    }};

    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(scene, data.dataName));
        CORRADE_COMPARE(out.str(), Utility::formatString(
            "Trade::GltfSceneConverter::add(): Trade::SceneField::Light was not used\n"
            "Trade::GltfSceneConverter::add(): Trade::SceneField::Custom(5318008) was not used\n"
            "Trade::GltfSceneConverter::add(): parentless object {} was not used\n", data.offset + 4));
    }

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

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
    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "scene-meshes-materials.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Add four empty meshes to not have to bother with buffers. Not valid
       glTF but accepted with strict=false (which gets reset back after) */
    {
        Warning silenceWarning{nullptr};
        converter->configuration().setValue("strict", false);
        /* Naming them to see how they were reordered */
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Triangles, 0}, "Mesh 0"));
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Triangles, 0}, "Mesh 1"));
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Triangles, 0}, "Mesh 2"));
        CORRADE_VERIFY(converter->add(MeshData{MeshPrimitive::Triangles, 0}, "Mesh 3"));
        converter->configuration().setValue("strict", true);
    }

    /* Add two empty materials */
    {
        CORRADE_VERIFY(converter->add(MaterialData{{}, {}}, "Material 0"));
        CORRADE_VERIFY(converter->add(MaterialData{{}, {}}, "Material 1"));
    }

    /* Deliberately using large & sparse object IDs to verify the warnings
       reference them and not the remapped ones */
    struct Scene {
        Containers::Pair<UnsignedInt, Int> parents[8];
        Containers::Triple<UnsignedInt, UnsignedInt, Int> meshesMaterials[9];
    } sceneData[]{{
        /* Object 30 is without a parent, thus ignored */
        {{0, -1},
         {40, -1},
         {20, -1},
         {10, -1},
         {50, -1},
         {60, -1},
         {70, -1},
         {80, -1}},

        /* Object 10 is without any mesh, mesh 2 is referenced by two objects;
           object 50 referencing two meshes (ignored with a warning).

           Then, mesh 1 is used again with a material; mesh 3 is used twice
           and both times with the same material. */
        {{40, 2, -1},
         {50, 1, -1},
         {30, 1, -1},
         {20, 0, -1},
         {50, 0, -1},
         {0, 2, -1},

         {60, 3, 0},
         {70, 1, 1},
         {80, 3, 0}},
    }};

    SceneData scene{SceneMappingType::UnsignedInt, 90, {}, sceneData, {
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

    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(scene));
        CORRADE_COMPARE(out.str(),
            "Trade::GltfSceneConverter::add(): parentless object 30 was not used\n"
            "Trade::GltfSceneConverter::add(): ignoring duplicate field Trade::SceneField::Mesh for object 50\n");
    }

    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "scene-meshes-materials.gltf"),
        TestSuite::Compare::File);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* There should be exactly one scene */
    CORRADE_COMPARE(importer->sceneCount(), 1);
    CORRADE_COMPARE(importer->objectCount(), 8);
    Containers::Optional<SceneData> imported = importer->scene(0);
    CORRADE_VERIFY(imported);
    CORRADE_COMPARE(imported->mappingBound(), 8);
    /* Not testing Parent, Transformation and ImporterState */
    CORRADE_COMPARE(imported->fieldCount(), 2 + 3);

    /* The mesh IDs are increasing even though they weren't in the original
       because we're picking unique mesh/material combinations as they
       appear */
    CORRADE_VERIFY(imported->hasField(SceneField::Mesh));
    CORRADE_COMPARE_AS(imported->mapping<UnsignedInt>(SceneField::Mesh),
        Containers::arrayView({0u, 3u, 2u, 4u, 5u, 6u, 7u}),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(imported->field<UnsignedInt>(SceneField::Mesh),
        Containers::arrayView({0u, 0u, 1u, 2u, 3u, 4u, 3u}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(imported->hasField(SceneField::MeshMaterial));
    /* Mapping same as Mesh */
    CORRADE_COMPARE_AS(imported->field<Int>(SceneField::MeshMaterial),
        Containers::arrayView({-1, -1, -1, -1, 0, 1, 0}),
        TestSuite::Compare::Container);

    /* The meshes, however, will be reordered and duplicated if assigned to
       different materials */
    CORRADE_COMPARE(importer->meshCount(), 5);
    CORRADE_COMPARE(importer->meshName(0), "Mesh 2");
    CORRADE_COMPARE(importer->meshName(1), "Mesh 0");
    CORRADE_COMPARE(importer->meshName(2), "Mesh 1");
    CORRADE_COMPARE(importer->meshName(3), "Mesh 3");
    CORRADE_COMPARE(importer->meshName(4), "Mesh 1");
}

void GltfSceneConverterTest::addSceneNoParentField() {
    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

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

    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(converter->add(scene));
        CORRADE_COMPARE(out.str(),
            "Trade::GltfSceneConverter::add(): parentless object 0 was not used\n"
            "Trade::GltfSceneConverter::add(): parentless object 1 was not used\n");
    }

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
        MeshAttributeData{MeshAttribute::TextureCoordinates, VertexFormat::Vector2b, 0, 1, 4}
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
