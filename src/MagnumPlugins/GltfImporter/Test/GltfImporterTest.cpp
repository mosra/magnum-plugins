/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
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
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Containers/StringIterable.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h>
#include <Corrade/Utility/Json.h>
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/FileCallback.h>
#include <Magnum/Mesh.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareMaterial.h>
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

struct GltfImporterTest: TestSuite::Tester {
    explicit GltfImporterTest();

    void open();
    void openError();
    void openFileError();
    void openIgnoreUnknownChunk();
    void openExternalDataOrder();
    void openExternalDataNoPathNoCallback();
    void openExternalDataTooLong();
    void openExternalDataTooShort();
    void openExternalDataInvalidUri();

    void requiredExtensions();
    void requiredExtensionsUnsupported();
    void requiredExtensionsUnsupportedDisabled();

    void animation();
    void animationInvalid();
    void animationInvalidBufferNotFound();
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
    void cameraInvalid();

    void light();
    void lightInvalid();

    void scene();
    void sceneInvalidWholeFile();
    void sceneInvalid();
    void sceneDefaultNoDefault();
    void sceneDefaultOutOfBounds();
    void sceneTransformation();
    void sceneTransformationQuaternionNormalizationEnabled();
    void sceneTransformationQuaternionNormalizationDisabled();
    void sceneCustomFields();
    void sceneCustomFieldsInvalidConfiguration();

    void skin();
    void skinInvalid();
    void skinInvalidBufferNotFound();

    void mesh();
    void meshNoAttributes();
    void meshNoIndices();
    void meshNoIndicesNoAttributes();
    void meshNoIndicesNoVerticesNoBufferUri();
    void meshColors();
    void meshSkinAttributes();
    void meshCustomAttributes();
    void meshCustomAttributesNoFileOpened();
    void meshDuplicateAttributes();
    void meshUnorderedAttributes();
    void meshMultiplePrimitives();
    void meshUnsignedIntVertexFormats();
    void meshUnsupportedVertexFormats();
    void meshPrimitivesTypes();
    void meshSizeNotMultipleOfStride();
    void meshInvalidWholeFile();
    void meshInvalid();
    void meshInvalidBufferNotFound();

    void materialPbrMetallicRoughness();
    void materialPbrSpecularGlossiness();
    void materialCommon();
    void materialUnlit();
    void materialExtras();
    void materialClearCoat();
    void materialPhongFallback();
    void materialRaw();
    void materialRawIor();
    void materialRawSpecular();
    void materialRawTransmission();
    void materialRawVolume();
    void materialRawSheen();
    void materialRawOutOfBounds();

    void materialInvalid();

    void textureCoordinateYFlip();

    void texture();
    void textureExtensions();
    void textureInvalid();

    void imageEmbedded();
    void imageExternal();
    void imageExternalNoPathNoCallback();
    void imageBasis();
    void imageMipLevels();
    void imageInvalid();
    void imageInvalidNotFound();
    void imagePropagateImporterFlags();

    void experimentalKhrTextureKtx2D();
    void experimentalKhrTextureKtx2DArray();
    void experimentalKhrTextureKtxPhongFallback();
    void experimentalKhrTextureKtxNotEnabled();
    void experimentalKhrTextureKtxInvalidWholeFile();
    void experimentalKhrTextureKtxInvalidMaterial();
    void experimentalKhrTextureKtxInvalidImage();

    void fileCallbackBuffer();
    void fileCallbackBufferNotFound();
    void fileCallbackImage();
    void fileCallbackImageNotFound();

    void utf8filenames();
    void escapedStrings();
    void encodedUris();

    void versionSupported();
    void versionUnsupported();

    void openMemory();
    void openTwice();
    void importTwice();

    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

/* The external-data.* files are packed in via a resource, filename mapping
   done in resources.conf */

using namespace Containers::Literals;
using namespace Math::Literals;

const struct {
    const char* name;
    Containers::StringView data;
    const char* message;
} OpenErrorData[]{
    {"binary header too short",
        "glTF\x02\x00\x00\x00\x13\x00\x00\x00\x00\x00\x00\x00JSO"_s,
        "binary glTF too small, expected at least 20 bytes but got only 19"},
    {"binary contents too short",
        "glTF\x02\x00\x00\x00\x16\x00\x00\x00\x01\x00\x00\x00JSON{"_s,
        "binary glTF size mismatch, expected 22 bytes but got 21"},
    {"binary contents too long",
        "glTF\x02\x00\x00\x00\x16\x00\x00\x00\x01\x00\x00\x00JSON{} "_s,
        "binary glTF size mismatch, expected 22 bytes but got 23"},
    {"binary JSON chunk contents too short",
        "glTF\x02\x00\x00\x00\x16\x00\x00\x00\x03\x00\x00\x00JSON{}"_s,
        "binary glTF size mismatch, expected 3 bytes for a JSON chunk but got only 2"},
    {"binary chunk header too short",
        "glTF\x02\x00\x00\x00\x1d\x00\x00\x00\x02\x00\x00\x00JSON{}\x02\x00\x00\0BIN"_s,
        "binary glTF chunk starting at 22 too small, expected at least 8 bytes but got only 7"},
    {"binary BIN chunk contents too short",
        "glTF\x02\x00\x00\x00\x1f\x00\x00\x00\x02\x00\x00\x00JSON{}\x02\x00\x00\0BIN\0\xff"_s,
        "binary glTF size mismatch, expected 2 bytes for a chunk starting at 22 but got only 1"},
    {"unknown binary glTF version",
        "glTF\x10\x00\x00\x00\x16\x00\x00\x00\x01\x00\x00\x00JSON{}"_s,
        "unsupported binary glTF version 16"},
    {"unknown binary JSON chunk",
        "glTF\x02\x00\x00\x00\x16\x00\x00\x00\x02\x00\x00\x00JSUN{}"_s,
        "expected a JSON chunk, got 0x4e55534a"},
    {"invalid JSON ascii",
        "{"_s,
        "Utility::Json: file too short, expected \" or } at <in>:1:2\n"
        "Trade::GltfImporter::openData(): invalid JSON\n"},
    {"invalid JSON binary",
        "glTF\x02\x00\x00\x00\x15\x00\x00\x00\x01\x00\x00\x00JSON{"_s,
        "Utility::Json: file too short, expected \" or } at <in>:1:22\n"
        "Trade::GltfImporter::openData(): invalid JSON\n"},
    {"no top-level JSON object",
        "[]",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:1\n"
        "Trade::GltfImporter::openData(): invalid JSON\n"},
    {"missing asset property",
        "{}",
        "missing or invalid asset property"},
    {"invalid asset property",
        R"({"asset": true})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Bool at <in>:1:11\n"
        "Trade::GltfImporter::openData(): missing or invalid asset property\n"},
    {"missing asset version property",
        R"({"asset": {}})",
        "missing or invalid asset version property"},
    {"invalid asset version property",
        R"({"asset": {"version": 2, "minVersion": 2}})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at <in>:1:23\n"
        "Trade::GltfImporter::openData(): missing or invalid asset version property\n"},
    {"invalid asset minVersion property",
        R"({"asset": {"version": "2.0", "minVersion": 2}})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at <in>:1:44\n"
        "Trade::GltfImporter::openData(): invalid asset minVersion property\n"},
    {"invalid extensionsUsed property",
        R"({"asset": {"version": "2.0"}, "extensionsUsed": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:49\n"
        "Trade::GltfImporter::openData(): invalid extensionsUsed property\n"},
    {"invalid extensionsUsed value",
        R"({"asset": {"version": "2.0"}, "extensionsUsed": ["KHR_lights_punctual", false]})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Bool at <in>:1:73\n"
        "Trade::GltfImporter::openData(): invalid used extension 1\n"},
    {"invalid extensionsRequired property",
        R"({"asset": {"version": "2.0"}, "extensionsRequired": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:53\n"
        "Trade::GltfImporter::openData(): invalid extensionsRequired property\n"},
    {"invalid extensionsRequired value",
        R"({"asset": {"version": "2.0"}, "extensionsRequired": ["KHR_mesh_quantization", false]})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Bool at <in>:1:79\n"
        "Trade::GltfImporter::openData(): invalid required extension 1\n"},
    {"invalid buffers property",
        R"({"asset": {"version": "2.0"}, "buffers": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:42\n"
        "Trade::GltfImporter::openData(): invalid buffers property\n"},
    {"invalid buffers value",
        R"({"asset": {"version": "2.0"}, "buffers": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:47\n"
        "Trade::GltfImporter::openData(): invalid buffer 1\n"},
    {"invalid bufferViews property",
        R"({"asset": {"version": "2.0"}, "bufferViews": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:46\n"
        "Trade::GltfImporter::openData(): invalid bufferViews property\n"},
    {"invalid bufferViews value",
        R"({"asset": {"version": "2.0"}, "bufferViews": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:51\n"
        "Trade::GltfImporter::openData(): invalid buffer view 1\n"},
    {"invalid accessors property",
        R"({"asset": {"version": "2.0"}, "accessors": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:44\n"
        "Trade::GltfImporter::openData(): invalid accessors property\n"},
    {"invalid accessors value",
        R"({"asset": {"version": "2.0"}, "accessors": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:49\n"
        "Trade::GltfImporter::openData(): invalid accessor 1\n"},
    {"invalid samplers property",
        R"({"asset": {"version": "2.0"}, "samplers": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:43\n"
        "Trade::GltfImporter::openData(): invalid samplers property\n"},
    {"invalid samplers value",
        R"({"asset": {"version": "2.0"}, "samplers": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:48\n"
        "Trade::GltfImporter::openData(): invalid sampler 1\n"},
    {"invalid nodes property",
        R"({"asset": {"version": "2.0"}, "nodes": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:40\n"
        "Trade::GltfImporter::openData(): invalid nodes property\n"},
    {"invalid nodes value",
        R"({"asset": {"version": "2.0"}, "nodes": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:45\n"
        "Trade::GltfImporter::openData(): invalid node 1\n"},
    {"invalid node name property",
        R"({"asset": {"version": "2.0"}, "nodes": [{}, {"name": 3}]})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at <in>:1:54\n"
        "Trade::GltfImporter::openData(): invalid node 1 name property\n"},
    {"invalid meshes property",
        R"({"asset": {"version": "2.0"}, "meshes": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:41\n"
        "Trade::GltfImporter::openData(): invalid meshes property\n"},
    {"invalid meshes value",
        R"({"asset": {"version": "2.0"}, "meshes": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:46\n"
        "Trade::GltfImporter::openData(): invalid mesh 1\n"},
    {"invalid mesh name property",
        R"({"asset": {"version": "2.0"}, "meshes": [{}, {"name": 3}]})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at <in>:1:55\n"
        "Trade::GltfImporter::openData(): invalid mesh 1 name property\n"},
    {"invalid cameras property",
        R"({"asset": {"version": "2.0"}, "cameras": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:42\n"
        "Trade::GltfImporter::openData(): invalid cameras property\n"},
    {"invalid cameras value",
        R"({"asset": {"version": "2.0"}, "cameras": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:47\n"
        "Trade::GltfImporter::openData(): invalid camera 1\n"},
    {"invalid camera name property",
        R"({"asset": {"version": "2.0"}, "cameras": [{}, {"name": 3}]})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at <in>:1:56\n"
        "Trade::GltfImporter::openData(): invalid camera 1 name property\n"},
    {"invalid animations property",
        R"({"asset": {"version": "2.0"}, "animations": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:45\n"
        "Trade::GltfImporter::openData(): invalid animations property\n"},
    {"invalid animations value",
        R"({"asset": {"version": "2.0"}, "animations": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:50\n"
        "Trade::GltfImporter::openData(): invalid animation 1\n"},
    {"invalid animations name property",
        R"({"asset": {"version": "2.0"}, "animations": [{}, {"name": 3}]})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at <in>:1:59\n"
        "Trade::GltfImporter::openData(): invalid animation 1 name property\n"},
    {"invalid skins property",
        R"({"asset": {"version": "2.0"}, "skins": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:40\n"
        "Trade::GltfImporter::openData(): invalid skins property\n"},
    {"invalid skin value",
        R"({"asset": {"version": "2.0"}, "skins": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:45\n"
        "Trade::GltfImporter::openData(): invalid skin 1\n"},
    {"invalid skin name property",
        R"({"asset": {"version": "2.0"}, "skins": [{}, {"name": 3}]})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at <in>:1:54\n"
        "Trade::GltfImporter::openData(): invalid skin 1 name property\n"},
    {"invalid images property",
        R"({"asset": {"version": "2.0"}, "images": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:41\n"
        "Trade::GltfImporter::openData(): invalid images property\n"},
    {"invalid image value",
        R"({"asset": {"version": "2.0"}, "images": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:46\n"
        "Trade::GltfImporter::openData(): invalid image 1\n"},
    {"invalid image name property",
        R"({"asset": {"version": "2.0"}, "images": [{}, {"name": 3}]})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at <in>:1:55\n"
        "Trade::GltfImporter::openData(): invalid image 1 name property\n"},
    {"invalid textures property",
        R"({"asset": {"version": "2.0"}, "textures": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:43\n"
        "Trade::GltfImporter::openData(): invalid textures property\n"},
    {"invalid textures value",
        R"({"asset": {"version": "2.0"}, "textures": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:48\n"
        "Trade::GltfImporter::openData(): invalid texture 1\n"},
    {"invalid textures name property",
        R"({"asset": {"version": "2.0"}, "textures": [{}, {"name": 3}]})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at <in>:1:57\n"
        "Trade::GltfImporter::openData(): invalid texture 1 name property\n"},
    {"invalid materials property",
        R"({"asset": {"version": "2.0"}, "materials": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:44\n"
        "Trade::GltfImporter::openData(): invalid materials property\n"},
    {"invalid materials value",
        R"({"asset": {"version": "2.0"}, "materials": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:49\n"
        "Trade::GltfImporter::openData(): invalid material 1\n"},
    {"invalid materials name property",
        R"({"asset": {"version": "2.0"}, "materials": [{}, {"name": 3}]})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at <in>:1:58\n"
        "Trade::GltfImporter::openData(): invalid material 1 name property\n"},
    {"invalid scenes property",
        R"({"asset": {"version": "2.0"}, "scenes": {}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:41\n"
        "Trade::GltfImporter::openData(): invalid scenes property\n"},
    {"invalid scene value",
        R"({"asset": {"version": "2.0"}, "scenes": [{}, []]})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:46\n"
        "Trade::GltfImporter::openData(): invalid scene 1\n"},
    {"invalid scene name property",
        R"({"asset": {"version": "2.0"}, "scenes": [{}, {"name": 3}]})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at <in>:1:55\n"
        "Trade::GltfImporter::openData(): invalid scene 1 name property\n"},
    {"invalid extensions property",
        R"({"asset": {"version": "2.0"}, "extensions": []})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:45\n"
        "Trade::GltfImporter::openData(): invalid extensions property\n"},
    {"invalid KHR_lights_punctual extension",
        R"({"asset": {"version": "2.0"}, "extensions": {"KHR_lights_punctual": []}})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:69\n"
        "Trade::GltfImporter::openData(): invalid KHR_lights_punctual extension\n"},
    {"invalid KHR_lights_punctual lights property",
        R"({"asset": {"version": "2.0"}, "extensions": {"KHR_lights_punctual": {"lights": {}}}})",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at <in>:1:80\n"
        "Trade::GltfImporter::openData(): invalid KHR_lights_punctual lights property\n"},
    {"invalid KHR_lights_punctual light value",
        R"({"asset": {"version": "2.0"}, "extensions": {"KHR_lights_punctual": {"lights": [{}, []]}}})",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at <in>:1:85\n"
        "Trade::GltfImporter::openData(): invalid KHR_lights_punctual light 1\n"},
    {"invalid KHR_lights_punctual light name property",
        R"({"asset": {"version": "2.0"}, "extensions": {"KHR_lights_punctual": {"lights": [{}, {"name": 3}]}}})",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at <in>:1:94\n"
        "Trade::GltfImporter::openData(): invalid KHR_lights_punctual light 1 name property\n"},
    {"invalid scene property",
        R"({"asset": {"version": "2.0"}, "scene": {}})",
        "Utility::Json::parseUnsignedInt(): expected a number, got Utility::JsonToken::Type::Object at <in>:1:40\n"
        "Trade::GltfImporter::openData(): invalid scene property\n"}
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
    {"invalid uri", "invalid URI escape sequence %%"},
    {"invalid base64", "invalid Base64 padding bytes b?"}
};

constexpr struct {
    const char* name;
    const char* message;
} AnimationInvalidData[]{
    {"unexpected time type",
        /** @todo might be good to eventually say the path instead of channel
            id, but only once KHR_animation_pointer is implemented */
        "channel 0 time track has unexpected type Vector4"},
    {"unexpected translation type",
        "translation track has unexpected type Vector4"},
    {"unexpected rotation type",
        "rotation track has unexpected type Float"},
    {"unexpected scaling type",
        "scaling track has unexpected type Vector4"},
    {"unsupported path",
        "unsupported track target color"},
    /* Full accessor checks are tested inside mesh-invalid.gltf, this only
       verifies the errors are propagated correctly */
    {"invalid input accessor",
        "accessor 3 needs 40 bytes but buffer view 0 has only 0"},
    {"invalid output accessor",
        "accessor 4 needs 120 bytes but buffer view 0 has only 0"},
    {"unsupported interpolation type",
        "unrecognized sampler 0 interpolation QUADRATIC"},
    {"sampler index out of bounds",
        "sampler index 1 in channel 0 out of range for 1 samplers"},
    {"node index out of bounds",
        "target node index 2 in channel 0 out of range for 2 nodes"},
    {"sampler input accessor index out of bounds",
        "accessor index 8 out of range for 8 accessors"},
    {"sampler output accessor index out of bounds",
        "accessor index 9 out of range for 8 accessors"},
    {"track size mismatch",
        "channel 0 target track size doesn't match time track size, expected 3 but got 2"},
    {"missing samplers",
        "missing or invalid samplers property"},
    {"invalid samplers",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at {}:264:19\n"
        "Trade::GltfImporter::animation(): missing or invalid samplers property\n"},
    {"invalid sampler",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Number at {}:270:9\n"
        "Trade::GltfImporter::animation(): invalid sampler 0\n"},
    {"missing sampler input",
        "missing or invalid sampler 0 input property"},
    {"invalid sampler input",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:287:20\n"
        "Trade::GltfImporter::animation(): missing or invalid sampler 0 input property\n"},
    {"missing sampler output",
        "missing or invalid sampler 0 output property"},
    {"invalid sampler output",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:307:21\n"
        "Trade::GltfImporter::animation(): missing or invalid sampler 0 output property\n"},
    {"invalid sampler interpolation",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Bool at {}:317:28\n"
        "Trade::GltfImporter::animation(): invalid sampler 0 interpolation property\n"},
    {"missing channels",
        "missing or invalid channels property"},
    {"invalid channels",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at {}:333:19\n"
        "Trade::GltfImporter::animation(): missing or invalid channels property\n"},
    {"invalid channel",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Number at {}:344:9\n"
        "Trade::GltfImporter::animation(): invalid channel 0\n"},
    {"missing channel target",
        "missing or invalid channel 1 target property"},
    {"invalid channel target",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::String at {}:379:21\n"
        "Trade::GltfImporter::animation(): missing or invalid channel 0 target property\n"},
    {"invalid channel target node",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:402:21\n"
        "Trade::GltfImporter::animation(): invalid channel 1 target node property\n"},
    {"missing channel target path",
        "missing or invalid channel 1 target path property"},
    {"invalid channel target path",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Null at {}:445:21\n"
        "Trade::GltfImporter::animation(): missing or invalid channel 0 target path property\n"},
    {"missing channel sampler",
        "missing or invalid channel 1 sampler property"},
    {"invalid channel sampler",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:484:22\n"
        "Trade::GltfImporter::animation(): missing or invalid channel 0 sampler property\n"}
};

constexpr struct {
    const char* name;
    const char* message;
} AnimationInvalidBufferNotFoundData[]{
    {"input buffer not found", "error opening /nonexistent1.bin"},
    {"output buffer not found", "error opening /nonexistent2.bin"}
};

const struct {
    const char* name;
    const char* message;
} CameraInvalidData[]{
    {"unrecognized type",
        "unrecognized type oblique"},
    {"missing type",
        "missing or invalid type property"},
    {"invalid type",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at {}:15:15\n"
        "Trade::GltfImporter::camera(): missing or invalid type property\n"},
    {"missing perspective property",
        "missing or invalid perspective property"},
    {"invalid perspective property",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Bool at {}:24:22\n"
        "Trade::GltfImporter::camera(): missing or invalid perspective property\n"},
    {"invalid perspective aspectRatio property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::Null at {}:30:24\n"
        "Trade::GltfImporter::camera(): invalid perspective aspectRatio property\n"},
    {"negative perspective aspectRatio",
        "expected positive perspective aspectRatio, got -3.5"},
    {"missing perspective yfov property",
        "missing or invalid perspective yfov property"},
    {"invalid perspective yfov property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::String at {}:55:17\n"
        "Trade::GltfImporter::camera(): missing or invalid perspective yfov property\n"},
    {"negative perspective yfov",
        "expected positive perspective yfov, got -1"},
    {"missing perspective znear property",
        "missing or invalid perspective znear property"},
    {"invalid perspective znear property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::String at {}:79:18\n"
        "Trade::GltfImporter::camera(): missing or invalid perspective znear property\n"},
    {"negative perspective znear",
        "expected positive perspective znear, got -0.01"},
    {"invalid perspective zfar property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::Null at {}:96:17\n"
        "Trade::GltfImporter::camera(): invalid perspective zfar property\n"},
    {"perspective zfar not larger than znear",
        "expected perspective zfar larger than znear of 0.125, got 0.125"},
    {"missing orthographic property",
        "missing or invalid orthographic property"},
    {"invalid orthographic property",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Bool at {}:115:23\n"
        "Trade::GltfImporter::camera(): missing or invalid orthographic property\n"},
    {"missing orthographic xmag property",
        "missing or invalid orthographic xmag property"},
    {"invalid orthographic xmag property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::String at {}:130:17\n"
        "Trade::GltfImporter::camera(): missing or invalid orthographic xmag property\n"},
    {"zero orthographic xmag",
        "expected non-zero orthographic xmag"},
    {"missing orthographic ymag property",
        "missing or invalid orthographic ymag property"},
    {"invalid orthographic ymag property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::String at {}:160:17\n"
        "Trade::GltfImporter::camera(): missing or invalid orthographic ymag property\n"},
    {"zero orthographic ymag",
        "expected non-zero orthographic ymag"},
    {"missing orthographic znear property",
        "missing or invalid orthographic znear property"},
    {"invalid orthographic znear property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::String at {}:190:18\n"
        "Trade::GltfImporter::camera(): missing or invalid orthographic znear property\n"},
    {"negative orthographic znear",
        "expected non-negative orthographic znear, got -1"},
    {"missing orthographic zfar property",
        "missing or invalid orthographic zfar property"},
    {"invalid orthographic zfar property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::String at {}:220:17\n"
        "Trade::GltfImporter::camera(): missing or invalid orthographic zfar property\n"},
    {"orthographic zfar not larger than znear",
        "expected orthographic zfar larger than znear of 0.5, got 0.5"},
};

constexpr struct {
    const char* name;
    const char* message;
} LightInvalidData[]{
    {"unknown type",
        "unrecognized type what"},
    {"directional with range",
        "range can't be defined for a directional light"},
    {"spot with too small inner angle",
        "spot inner and outer cone angle Deg(-0.572958) and Deg(45) out of allowed bounds"},
    /* These are kinda silly (not sure why inner can't be the same as outer),
       but let's follow the spec */
    {"spot with too large outer angle",
        "spot inner and outer cone angle Deg(0) and Deg(90.5273) out of allowed bounds"},
    {"spot with inner angle same as outer",
        "spot inner and outer cone angle Deg(14.3239) and Deg(14.3239) out of allowed bounds"},
    {"invalid color property",
        "Utility::Json::parseFloatArray(): expected an array, got Utility::JsonToken::Type::String at {}:42:20\n"
        "Trade::GltfImporter::light(): invalid color property\n"},
    {"invalid color array size",
        "Utility::Json::parseFloatArray(): expected a 3-element array, got 4 at {}:47:20\n"
        "Trade::GltfImporter::light(): invalid color property\n"},
    {"invalid intensity property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::String at {}:52:24\n"
        "Trade::GltfImporter::light(): invalid intensity property\n"},
    {"invalid range property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::String at {}:57:20\n"
        "Trade::GltfImporter::light(): invalid range property\n"},
    {"zero range",
        "expected positive range, got 0"},
    {"missing type property",
        "missing or invalid type property"},
    {"invalid type property",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at {}:69:19\n"
        "Trade::GltfImporter::light(): missing or invalid type property\n"},
    {"missing spot property",
        "missing or invalid spot property"},
    {"invalid spot property",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Number at {}:78:19\n"
        "Trade::GltfImporter::light(): missing or invalid spot property\n"},
    {"invalid spot innerConeAngle property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::Bool at {}:84:31\n"
        "Trade::GltfImporter::light(): invalid spot innerConeAngle property\n"},
    {"invalid spot outerConeAngle property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::Bool at {}:91:31\n"
        "Trade::GltfImporter::light(): invalid spot outerConeAngle property\n"}
};

constexpr struct {
    const char* name;
    const char* message;
} SkinInvalidData[]{
    {"no joints",
        "skin has no joints"},
    {"joint out of bounds",
        "joint index 2 out of range for 2 nodes"},
    {"accessor out of bounds",
        "accessor index 4 out of range for 4 accessors"},
    {"wrong accessor type",
        "inverse bind matrices have unexpected type Matrix3x3"},
    {"wrong accessor component type",
        "accessor 1 has an unsupported matrix component format UnsignedShort"},
    {"wrong accessor count",
        "invalid inverse bind matrix count, expected 2 but got 3"},
    /* Full accessor checks are tested inside mesh-invalid.gltf, this only
       verifies the errors are propagated correctly */
    {"invalid accessor",
        "accessor 3 needs 196 bytes but buffer view 0 has only 192"},
    {"missing joints property",
        "missing or invalid joints property"},
    {"invalid joints property",
        "Utility::Json::parseUnsignedIntArray(): expected an array, got Utility::JsonToken::Type::Object at {}:48:17\n"
        "Trade::GltfImporter::skin3D(): missing or invalid joints property\n"},
    {"invalid inverseBindMatrices property",
        "Utility::Json::parseUnsignedInt(): expected a number, got Utility::JsonToken::Type::Array at {}:52:30\n"
        "Trade::GltfImporter::skin3D(): invalid inverseBindMatrices property\n"},
};

const struct {
    const char* name;
    Containers::Optional<bool> strict;
    const char* message;
} MeshNoAttributesData[]{
    {"", {}, ""},
    {"strict", true, "Trade::GltfImporter::mesh(): strict mode enabled, disallowing a mesh with no attributes\n"}
};

const struct {
    const char* name;
    const char* suffix;
    Containers::Optional<bool> strict;
    const char* message;
} MeshNoVerticesData[]{
    {"ascii", ".gltf", {}, ""},
    {"binary", ".glb", {}, ""},
    {"strict, ascii", ".gltf", true, "Trade::GltfImporter::mesh(): strict mode enabled, disallowing a mesh with no vertices\n"},
    {"strict, binary", ".glb", true, "Trade::GltfImporter::mesh(): strict mode enabled, disallowing a mesh with no vertices\n"}
};

/** @todo remove once the compatibilitySkinningAttributes option is gone */
const struct {
    const char* name;
    #ifdef MAGNUM_BUILD_DEPRECATED
    Containers::Optional<bool> compatibilitySkinningAttributes;
    #endif
} MeshSkinAttributeData[]{
    {"",
        #ifdef MAGNUM_BUILD_DEPRECATED
        {}
        #endif
        },
    #ifdef MAGNUM_BUILD_DEPRECATED
    {"no compatibility attributes", false},
    #endif
};

const struct {
    const char* name;
    Containers::Optional<bool> strict;
    const char* message;
} MeshUnsignedIntVertexFormatsData[]{
    {"", {},
        ""},
    {"strict", true,
        "Trade::GltfImporter::mesh(): strict mode enabled, disallowing _OFFSET with a 32-bit integer vertex format Vector2ui\n"
        "Trade::GltfImporter::mesh(): strict mode enabled, disallowing _OBJECT_ID with a 32-bit integer vertex format UnsignedInt\n"},
};

const struct {
    const char* name;
    Containers::Optional<bool> strict;
    const char* message;
} MeshUnsupportedVertexFormatsData[]{
    {"", {},
        "Trade::GltfImporter::mesh(): found attribute COLOR_3 but expected COLOR_0\n"
        "Trade::GltfImporter::mesh(): unsupported COLOR_3 format Vector4us, importing as a custom attribute\n"
        "Trade::GltfImporter::mesh(): unsupported object ID attribute _OBJECT_ID format Short, importing as a custom attribute\n"
        "Trade::GltfImporter::mesh(): found attribute JOINTS_7 but expected JOINTS_0\n"
        "Trade::GltfImporter::mesh(): unsupported JOINTS_7 format Vector3ub, importing as a custom attribute\n"},
    {"strict", true,
        "Trade::GltfImporter::mesh(): found attribute COLOR_3 but expected COLOR_0\n"
        "Trade::GltfImporter::mesh(): unsupported COLOR_3 format Vector4us, set strict=false to import as a custom atttribute\n"
        "Trade::GltfImporter::mesh(): unsupported object ID attribute _OBJECT_ID format Short, set strict=false to import as a custom atttribute\n"
        "Trade::GltfImporter::mesh(): found attribute JOINTS_7 but expected JOINTS_0\n"
        "Trade::GltfImporter::mesh(): unsupported JOINTS_7 format Vector3ub, set strict=false to import as a custom atttribute\n"},
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
    const char* file;
    const char* message;
} MeshInvalidWholeFileData[]{
    {"missing primitives property",
        "mesh-invalid-missing-primitives-property.gltf",
        "missing or invalid primitives property in mesh 1"},
    {"invalid primitives property",
        "mesh-invalid-primitives-property.gltf",
        "Utility::Json::parseArray(): expected an array, got Utility::JsonToken::Type::Object at {}:12:21\n"
        "Trade::GltfImporter::openData(): missing or invalid primitives property in mesh 1\n"},
    {"empty primitives",
        "mesh-invalid-empty-primitives.gltf",
        "mesh 1 has no primitives"},
    {"invalid primitive",
        "mesh-invalid-primitive.gltf",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at {}:13:9\n"
        "Trade::GltfImporter::openData(): invalid mesh 1 primitive 0\n"},
    {"invalid primitive attributes property",
        "mesh-invalid-primitive-attributes-property.gltf",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at {}:14:25\n"
        "Trade::GltfImporter::openData(): invalid primitive attributes property in mesh 1\n"},
    {"texcoord flip invalid attribute",
        "mesh-invalid-texcoord-flip-attribute.gltf",
        "Utility::Json::parseUnsignedInt(): expected a number, got Utility::JsonToken::Type::String at {}:15:27\n"
        "Trade::GltfImporter::openData(): invalid attribute TEXCOORD_3 in mesh 1\n"},
    {"texcoord flip attribute out of bounds",
        "mesh-invalid-texcoord-flip-attribute-oob.gltf",
        "accessor index 2 out of range for 2 accessors"},
    {"texcoord flip attribute accessor missing componentType",
        "mesh-invalid-texcoord-flip-attribute-accessor-missing-component-type.gltf",
        "accessor 1 has missing or invalid componentType property"},
    {"texcoord flip attribute accessor invalid componentType",
        "mesh-invalid-texcoord-flip-attribute-accessor-invalid-component-type.gltf",
        "Utility::Json::parseUnsignedInt(): expected a number, got Utility::JsonToken::Type::String at {}:8:24\n"
        "Trade::GltfImporter::openData(): accessor 1 has missing or invalid componentType property\n"},
    {"texcoord flip attribute accessor invalid normalized",
        "mesh-invalid-texcoord-flip-attribute-accessor-invalid-normalized.gltf",
        "Utility::Json::parseBool(): expected a bool, got Utility::JsonToken::Type::Null at {}:9:21\n"
        "Trade::GltfImporter::openData(): accessor 1 has invalid normalized property\n"},
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    const char* message;
} MeshInvalidData[]{
    {"unrecognized primitive",
        "unrecognized primitive 666"},
    {"different vertex count for each accessor",
        "mismatched vertex count for attribute TEXCOORD_0, expected 3 but got 4"},
    {"different number of WEIGHTS and JOINTS attributes",
        "the mesh has 2 JOINTS_n attributes but 1 WEIGHTS_n attributes"},
    {"unexpected index type",
        "unsupported index type Vector2ui"},
    {"unsupported index component type",
        "unsupported index type Short"},
    {"normalized index type",
        "accessor 4 with component format UnsignedInt can't be normalized"},
    {"strided index view",
        "index buffer view is not contiguous"},
    {"accessor type size larger than buffer stride",
        "16-byte type defined by accessor 6 can't fit into buffer view 0 stride of 12"},
    {"normalized float",
        "accessor 7 with component format Float can't be normalized"},
    {"normalized int",
        "accessor 8 with component format UnsignedInt can't be normalized"},
    {"non-normalized byte matrix",
        "accessor 9 has an unsupported matrix component format Byte"},
    {"unknown type",
        "accessor 18 has invalid type EEE"},
    {"unknown component type",
        "accessor 19 has invalid componentType 9999"},
    {"sparse accessor",
        "accessor 10 is using sparse storage, which is unsupported"},
    {"multiple buffers",
        "meshes spanning multiple buffers are not supported"},
    {"invalid index accessor",
        "accessor 13 needs 40 bytes but buffer view 0 has only 36"},
    {"accessor range out of bounds",
        "accessor 14 needs 48 bytes but buffer view 0 has only 36"},
    {"buffer view range out of bounds",
        "buffer view 3 needs 60 bytes but buffer 1 has only 59"},
    {"buffer index out of bounds",
        "buffer index 7 out of range for 7 buffers"},
    {"buffer view index out of bounds",
        "buffer view index 16 out of range for 16 buffer views"},
    {"accessor index out of bounds",
        "accessor index 42 out of range for 42 accessors"},
    {"mesh index accessor out of bounds",
        "accessor index 42 out of range for 42 accessors"},
    {"buffer with missing uri property",
        "buffer 2 has missing uri property"},
    {"buffer with invalid uri property",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Array at {}:875:14\n"
        "Trade::GltfImporter::mesh(): buffer 3 has invalid uri property\n"},
    {"buffer with invalid uri",
        "invalid URI escape sequence %%"},
    {"buffer with missing byteLength property",
        "buffer 5 has missing or invalid byteLength property"},
    {"buffer with invalid byteLength property",
        "Utility::Json::parseSize(): too large integer literal -3 at {}:889:21\n"
        "Trade::GltfImporter::mesh(): buffer 6 has missing or invalid byteLength property\n"},
    {"buffer view with missing buffer property",
        "buffer view 9 has missing or invalid buffer property"},
    {"buffer view with invalid buffer property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:825:17\n"
        "Trade::GltfImporter::mesh(): buffer view 10 has missing or invalid buffer property\n"},
    {"buffer view with invalid byteOffset property",
        "Utility::Json::parseSize(): too large integer literal -1 at {}:831:21\n"
        "Trade::GltfImporter::mesh(): buffer view 11 has invalid byteOffset property\n"},
    {"buffer view with missing byteLength property",
        "buffer view 12 has missing or invalid byteLength property"},
    {"buffer view with invalid byteLength property",
        "Utility::Json::parseSize(): too large integer literal -12 at {}:841:21\n"
        "Trade::GltfImporter::mesh(): buffer view 13 has missing or invalid byteLength property\n"},
    {"buffer view with invalid byteStride property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -4 at {}:847:21\n"
        "Trade::GltfImporter::mesh(): buffer view 14 has invalid byteStride property\n"},
    {"accessor with missing bufferView property",
        "accessor 11 has missing or invalid bufferView property"},
    {"accessor with invalid bufferView property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:687:21\n"
        "Trade::GltfImporter::mesh(): accessor 30 has missing or invalid bufferView property\n"},
    {"accessor with invalid byteOffset property",
        "Utility::Json::parseSize(): too large integer literal -1 at {}:695:21\n"
        "Trade::GltfImporter::mesh(): accessor 31 has invalid byteOffset property\n"},
    {"accessor with missing componentType property",
        "accessor 32 has missing or invalid componentType property"},
    {"accessor with invalid componentType property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:709:24\n"
        "Trade::GltfImporter::mesh(): accessor 33 has missing or invalid componentType property\n"},
    {"accessor with missing count property",
        "accessor 34 has missing or invalid count property"},
    {"accessor with invalid count property",
        "Utility::Json::parseSize(): too large integer literal -1 at {}:723:16\n"
        "Trade::GltfImporter::mesh(): accessor 35 has missing or invalid count property\n"},
    {"accessor with missing type property",
        "accessor 36 has missing or invalid type property"},
    {"accessor with invalid type property",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at {}:737:15\n"
        "Trade::GltfImporter::mesh(): accessor 37 has missing or invalid type property\n"},
    {"accessor with invalid normalized property",
        "Utility::Json::parseBool(): expected a bool, got Utility::JsonToken::Type::Null at {}:745:21\n"
        "Trade::GltfImporter::mesh(): accessor 38 has invalid normalized property\n"},
    {"invalid primitive property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:436:19\n"
        "Trade::GltfImporter::mesh(): invalid primitive mode property\n"},
    {"invalid attribute property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:446:26\n"
        "Trade::GltfImporter::mesh(): invalid attribute _WEIRD_EH\n"},
    {"invalid indices property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:456:22\n"
        "Trade::GltfImporter::mesh(): invalid indices property\n"},
};

constexpr struct {
    const char* name;
    const char* message;
} MeshInvalidBufferNotFoundData[]{
    {"buffer not found", "error opening /nonexistent1.bin"},
    {"indices buffer not found", "error opening /nonexistent2.bin"}
};

constexpr struct {
    const char* name;
    const char* message;
} MaterialInvalidData[]{
    {"invalid alphaMode property",
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at {}:8:20\n"
        "Trade::GltfImporter::material(): invalid alphaMode property\n"},
    {"unrecognized alpha mode",
        "unrecognized alphaMode WAT"},
    {"invalid alphaCutoff property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::String at {}:17:22\n"
        "Trade::GltfImporter::material(): invalid alphaCutoff property\n"},
    {"invalid doubleSided property",
        "Utility::Json::parseBool(): expected a bool, got Utility::JsonToken::Type::Null at {}:21:22\n"
        "Trade::GltfImporter::material(): invalid doubleSided property\n"},
    {"invalid pbrMetallicRoughness property",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at {}:25:31\n"
        "Trade::GltfImporter::material(): invalid pbrMetallicRoughness property\n"},
    {"invalid pbrMetallicRoughness baseColorFactor property",
        "Utility::Json::parseFloatArray(): expected an array, got Utility::JsonToken::Type::String at {}:30:28\n"
        "Trade::GltfImporter::material(): invalid pbrMetallicRoughness baseColorFactor property\n"},
    {"invalid pbrMetallicRoughness baseColorFactor array size",
        "Utility::Json::parseFloatArray(): expected a 4-element array, got 3 at {}:36:28\n"
        "Trade::GltfImporter::material(): invalid pbrMetallicRoughness baseColorFactor property\n"},
    {"invalid pbrMetallicRoughness baseColorTexture",
        "baseColorTexture index 2 out of range for 2 textures"},
    {"invalid pbrMetallicRoughness metallicFactor property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::Bool at {}:50:27\n"
        "Trade::GltfImporter::material(): invalid pbrMetallicRoughness metallicFactor property\n"},
    {"invalid pbrMetallicRoughness roughnessFactor property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::Bool at {}:56:28\n"
        "Trade::GltfImporter::material(): invalid pbrMetallicRoughness roughnessFactor property\n"},
    {"invalid pbrMetallicRoughness metallicRoughnessTexture",
        "metallicRoughnessTexture index 2 out of range for 2 textures"},
    {"invalid extensions property",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at {}:69:21\n"
        "Trade::GltfImporter::material(): invalid extensions property\n"},
    {"invalid extension",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Null at {}:74:32\n"
        "Trade::GltfImporter::material(): invalid KHR_materials_unlit extension property\n"},
    {"invalid KHR_materials_pbrSpecularGlossiness diffuseFactor property",
        "Utility::Json::parseFloatArray(): expected an array, got Utility::JsonToken::Type::String at {}:81:28\n"
        "Trade::GltfImporter::material(): invalid KHR_materials_pbrSpecularGlossiness diffuseFactor property\n"},
    {"invalid KHR_materials_pbrSpecularGlossiness diffuseFactor array size",
        "Utility::Json::parseFloatArray(): expected a 4-element array, got 3 at {}:89:28\n"
        "Trade::GltfImporter::material(): invalid KHR_materials_pbrSpecularGlossiness diffuseFactor property\n"},
    {"invalid KHR_materials_pbrSpecularGlossiness diffuseTexture",
        "diffuseTexture index 2 out of range for 2 textures"},
    {"invalid KHR_materials_pbrSpecularGlossiness specularFactor property",
        "Utility::Json::parseFloatArray(): expected an array, got Utility::JsonToken::Type::String at {}:107:29\n"
        "Trade::GltfImporter::material(): invalid KHR_materials_pbrSpecularGlossiness specularFactor property\n"},
    {"invalid KHR_materials_pbrSpecularGlossiness specularFactor array size",
        "Utility::Json::parseFloatArray(): expected a 3-element array, got 4 at {}:115:29\n"
        "Trade::GltfImporter::material(): invalid KHR_materials_pbrSpecularGlossiness specularFactor property\n"},
    {"invalid KHR_materials_pbrSpecularGlossiness glossinessFactor property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::Bool at {}:123:31\n"
        "Trade::GltfImporter::material(): invalid KHR_materials_pbrSpecularGlossiness glossinessFactor property\n"},
    {"invalid KHR_materials_pbrSpecularGlossiness specularGlossinessTexture",
        "specularGlossinessTexture index 2 out of range for 2 textures"},
    {"invalid normalTexture",
        "normalTexture index 2 out of range for 2 textures"},
    {"invalid normalTexture scale property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::Bool at {}:147:18\n"
        "Trade::GltfImporter::material(): invalid normalTexture scale property\n"},
    {"invalid occlusionTexture",
        "occlusionTexture index 2 out of range for 2 textures"},
    {"invalid occlusionTexture strength property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::String at {}:160:21\n"
        "Trade::GltfImporter::material(): invalid occlusionTexture strength property\n"},
    {"invalid emissiveFactor property",
        "Utility::Json::parseFloatArray(): expected an array, got Utility::JsonToken::Type::Number at {}:165:25\n"
        "Trade::GltfImporter::material(): invalid emissiveFactor property\n"},
    {"invalid emissiveFactor array size",
        "Utility::Json::parseFloatArray(): expected a 3-element array, got 4 at {}:169:25\n"
        "Trade::GltfImporter::material(): invalid emissiveFactor property\n"},
    {"invalid emissiveTexture",
        "emissiveTexture index 2 out of range for 2 textures"},
    {"invalid KHR_materials_clearcoat clearcoatFactor property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::Array at {}:181:30\n"
        "Trade::GltfImporter::material(): invalid KHR_materials_clearcoat clearcoatFactor property\n"},
    {"invalid KHR_materials_clearcoat clearcoatTexture",
        "clearcoatTexture index 2 out of range for 2 textures"},
    {"invalid KHR_materials_clearcoat clearcoatRoughnessFactor property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::Bool at {}:199:39\n"
        "Trade::GltfImporter::material(): invalid KHR_materials_clearcoat roughnessFactor property\n"},
    {"invalid KHR_materials_clearcoat clearcoatRoughnessTexture",
        "clearcoatRoughnessTexture index 2 out of range for 2 textures"},
    {"invalid KHR_materials_clearcoat clearcoatNormalTexture",
        "clearcoatNormalTexture index 2 out of range for 2 textures"},
    {"invalid KHR_materials_clearcoat clearcoatNormalTexture scale property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::Bool at {}:229:22\n"
        "Trade::GltfImporter::material(): invalid KHR_materials_clearcoat normalTexture scale property\n"},
    /* Invalid texture object cases are tested thoroughly only once on the
       baseColorTexture object, as the helper code path is shared. General
       error propagation was tested above alaready. */
    {"invalid texture object",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Number at {}:237:29\n"
        "Trade::GltfImporter::material(): invalid baseColorTexture property\n"},
    {"missing texture object index property",
        "missing or invalid baseColorTexture index property"},
    {"invalid texture object index property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -2 at {}:250:20\n"
        "Trade::GltfImporter::material(): missing or invalid baseColorTexture index property\n"},
    {"invalid texture object texCoord property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:259:23\n"
        "Trade::GltfImporter::material(): invalid baseColorTexture texcoord property\n"},
    {"invalid texture object extensions property",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at {}:268:25\n"
        "Trade::GltfImporter::material(): invalid baseColorTexture extensions property\n"},
    {"invalid texture object KHR_texture_transform extension",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Null at {}:278:38\n"
        "Trade::GltfImporter::material(): invalid baseColorTexture KHR_texture_transform extension\n"},
    {"invalid texture object KHR_texture_transform texCoord property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:290:27\n"
        "Trade::GltfImporter::material(): invalid baseColorTexture KHR_texture_transform texcoord property\n"},
    {"invalid texture object KHR_texture_transform scale property",
        "Utility::Json::parseFloatArray(): expected an array, got Utility::JsonToken::Type::Number at {}:303:24\n"
        "Trade::GltfImporter::material(): invalid baseColorTexture KHR_texture_transform scale property\n"},
    {"invalid texture object KHR_texture_transform scale array size",
        "Utility::Json::parseFloatArray(): expected a 2-element array, got 1 at {}:316:24\n"
        "Trade::GltfImporter::material(): invalid baseColorTexture KHR_texture_transform scale property\n"},
    {"invalid texture object KHR_texture_transform rotation property",
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::Array at {}:329:27\n"
        "Trade::GltfImporter::material(): invalid baseColorTexture KHR_texture_transform rotation property\n"},
    {"invalid texture object KHR_texture_transform offset property",
        "Utility::Json::parseFloatArray(): expected an array, got Utility::JsonToken::Type::Number at {}:342:25\n"
        "Trade::GltfImporter::material(): invalid baseColorTexture KHR_texture_transform offset property\n"},
    {"invalid texture object KHR_texture_transform offset array size",
        "Utility::Json::parseFloatArray(): expected a 2-element array, got 1 at {}:355:25\n"
        "Trade::GltfImporter::material(): invalid baseColorTexture KHR_texture_transform offset property\n"},
};

constexpr struct {
    const char* name;
    const char* file;
    const char* message;
} SceneInvalidWholeFileData[]{
    {"scene node has parent",
        "scene-invalid-child-not-root.gltf",
        "node 1 is both a root node and a child of node 0"},
    {"node has multiple parents",
        "scene-invalid-multiple-parents.gltf",
        "node 2 is a child of both node 0 and node 1"},
    {"child is self",
        "scene-invalid-cycle.gltf",
        "node tree contains cycle starting at node 0"},
    {"great-grandchild is self",
        "scene-invalid-cycle-deep.gltf",
        "node tree contains cycle starting at node 0"},
    {"child out of bounds",
        "scene-invalid-child-oob.gltf",
        "child index 7 in node 4 out of range for 7 nodes"},
    {"node out of bounds",
        "scene-invalid-node-oob.gltf",
        "node index 7 in scene 0 out of range for 7 nodes"},
    {"invalid nodes property",
        "scene-invalid-nodes-property.gltf",
        "Utility::Json::parseUnsignedIntArray(): expected an array, got Utility::JsonToken::Type::Object at {}:8:16\n"
        "Trade::GltfImporter::openData(): invalid nodes property of scene 1\n"},
    {"invalid children property",
        "scene-invalid-children-property.gltf",
        "Utility::Json::parseUnsignedIntArray(): expected an array, got Utility::JsonToken::Type::Object at {}:8:19\n"
        "Trade::GltfImporter::openData(): invalid children property of node 1\n"},
    {"invalid extras property",
        "scene-invalid-extras-property.gltf",
        "Utility::Json::parseObject(): invalid unicode escape sequence \\uhh at {}:9:10\n"
        "Trade::GltfImporter::openData(): invalid node 1 extras property\n"}
};

constexpr struct {
    const char* name;
    const char* message;
} SceneInvalidData[]{
    {"camera out of bounds",
        "camera index 1 in node 3 out of range for 1 cameras"},
    {"light out of bounds",
        "light index 2 in node 4 out of range for 2 lights"},
    {"material out of bounds",
        "material index 4 in mesh 0 primitive 0 out of range for 4 materials"},
    {"material in a multi-primitive mesh out of bounds",
        "material index 5 in mesh 1 primitive 1 out of range for 4 materials"},
    {"mesh out of bounds",
        "mesh index 5 in node 7 out of range for 5 meshes"},
    {"skin out of bounds",
        "skin index 3 in node 8 out of range for 3 skins"},
    {"skin for a multi-primitive mesh out of bounds",
        "skin index 3 in node 9 out of range for 3 skins"},
    {"invalid mesh property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:132:15\n"
        "Trade::GltfImporter::scene(): invalid mesh property of node 10\n"},
    {"invalid mesh material property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:70:23\n"
        "Trade::GltfImporter::scene(): invalid material property of mesh 4 primitive 1\n"},
    {"invalid camera property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:140:17\n"
        "Trade::GltfImporter::scene(): invalid camera property of node 12\n"},
    {"invalid skin property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:144:15\n"
        "Trade::GltfImporter::scene(): invalid skin property of node 13\n"},
    {"invalid extensions property",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at {}:148:21\n"
        "Trade::GltfImporter::scene(): invalid node 14 extensions property\n"},
    {"invalid KHR_lights_punctual extension",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Number at {}:153:32\n"
        "Trade::GltfImporter::scene(): invalid node 15 KHR_lights_punctual extension\n"},
    {"invalid KHR_lights_punctual light property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:160:20\n"
        "Trade::GltfImporter::scene(): missing or invalid KHR_lights_punctual light property of node 16\n"},
    {"invalid translation property",
        "Utility::Json::parseFloatArray(): expected an array, got Utility::JsonToken::Type::Number at {}:166:22\n"
        "Trade::GltfImporter::scene(): invalid translation property of node 17\n"},
    {"invalid translation array size",
        "Utility::Json::parseFloatArray(): expected a 3-element array, got 2 at {}:170:22\n"
        "Trade::GltfImporter::scene(): invalid translation property of node 18\n"},
    {"invalid rotation property",
        "Utility::Json::parseFloatArray(): expected an array, got Utility::JsonToken::Type::Number at {}:174:19\n"
        "Trade::GltfImporter::scene(): invalid rotation property of node 19\n"},
    {"invalid rotation array size",
        "Utility::Json::parseFloatArray(): expected a 4-element array, got 3 at {}:178:19\n"
        "Trade::GltfImporter::scene(): invalid rotation property of node 20\n"},
    {"invalid scale property",
        "Utility::Json::parseFloatArray(): expected an array, got Utility::JsonToken::Type::Number at {}:182:16\n"
        "Trade::GltfImporter::scene(): invalid scale property of node 21\n"},
    {"invalid scale array size",
        "Utility::Json::parseFloatArray(): expected a 3-element array, got 2 at {}:186:16\n"
        "Trade::GltfImporter::scene(): invalid scale property of node 22\n"},
    {"invalid matrix property",
        "Utility::Json::parseFloatArray(): expected an array, got Utility::JsonToken::Type::Number at {}:190:17\n"
        "Trade::GltfImporter::scene(): invalid matrix property of node 23\n"},
    {"invalid matrix array size",
        "Utility::Json::parseFloatArray(): expected a 16-element array, got 4 at {}:194:17\n"
        "Trade::GltfImporter::scene(): invalid matrix property of node 24\n"}
};

constexpr struct {
    const char* materialName;
    const char* fileName;
    const char* meshName;
    bool flipInMaterial;
    bool hasTextureTransformation;
} TextureCoordinateYFlipData[]{
    {"no transform",
        "texcoord-flip.gltf", "float", false, false},
    {"no transform",
        "texcoord-flip.gltf", "float", true, false},
    {"identity transform",
        "texcoord-flip.gltf", "float", false, true},
    {"identity transform",
        "texcoord-flip.gltf", "float", true, true},
    {"transform from normalized unsigned byte",
        "texcoord-flip.gltf",
        "normalized unsigned byte", false, true},
    {"transform from normalized unsigned byte",
        "texcoord-flip.gltf",
        "normalized unsigned byte", true, true},
    {"transform from normalized unsigned short",
        "texcoord-flip.gltf",
        "normalized unsigned short", false, true},
    {"transform from normalized unsigned short",
        "texcoord-flip.gltf",
        "normalized unsigned short", true, true},
    /* The following are in a separate file because otherwise
       textureCoordinateYFlipInMaterial would get implicitly enabled for all,
       making flips in meshes impossible to test */
    {"transform from normalized signed byte",
        "texcoord-flip-unnormalized.gltf",
        "normalized signed byte", false, true},
    {"transform from normalized signed byte",
        "texcoord-flip-unnormalized.gltf",
        "normalized signed byte", true, true},
    {"transform from signed short",
        "texcoord-flip-unnormalized.gltf",
        "signed short", false, true},
    {"transform from signed short",
        "texcoord-flip-unnormalized.gltf",
        "signed short", true, true},
};

const struct {
    const char* name;
    UnsignedInt id;
    const char* xfail;
    UnsignedInt xfailId;
} TextureExtensionsData[]{
    {"GOOGLE_texture_basis", 1,
        "Magnum's JSON parser currently takes the first duplicate key instead of last.",
        3},
    {"KHR_texture_basisu", 2, nullptr, 0},
    {"MSFT_texture_dds", 3, nullptr, 0},
    {"EXT_texture_webp", 5, nullptr, 0},
    /* declaration order decides preference */
    {"MSFT_texture_dds and GOOGLE_texture_basis", 3, nullptr, 0},
    {"GOOGLE_texture_basis and KHR_texture_basisu", 1, nullptr, 0},
    {"unknown extension", 0, nullptr, 0},
    {"GOOGLE_texture_basis and unknown", 1, nullptr, 0}
};

constexpr struct {
    const char* name;
    const char* message;
} TextureInvalidData[]{
    {"invalid sampler minFilter",
        "unrecognized minFilter 1"},
    {"invalid sampler magFilter",
        "unrecognized magFilter 2"},
    {"invalid sampler wrapS",
        "unrecognized wrapS 3"},
    {"invalid sampler wrapT",
        "unrecognized wrapT 4"},
    {"sampler out of bounds",
        "index 9 out of range for 9 samplers"},
    {"source out of bounds",
        "index 1 out of range for 1 images"},
    {"out of bounds GOOGLE_texture_basis",
        "index 3 out of range for 1 images"},
    {"out of bounds KHR_texture_basisu",
        "index 4 out of range for 1 images"},
    {"unknown extension, no fallback",
        "missing or invalid source property"},
    {"missing source property",
        "missing or invalid source property"},
    {"invalid source property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:106:17\n"
        "Trade::GltfImporter::texture(): missing or invalid source property\n"},
    {"invalid extensions property",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at {}:110:21\n"
        "Trade::GltfImporter::texture(): invalid extensions property\n"},
    {"invalid extension",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Null at {}:115:31\n"
        "Trade::GltfImporter::texture(): invalid KHR_texture_basisu extension\n"},
    {"missing extension source property",
        "missing or invalid KHR_texture_basisu source property"},
    {"invalid extension source property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:128:21\n"
        "Trade::GltfImporter::texture(): missing or invalid KHR_texture_basisu source property\n"},
    {"invalid sampler property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:135:18\n"
        "Trade::GltfImporter::texture(): invalid sampler property\n"},
    {"invalid sampler magFilter property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:32:20\n"
        "Trade::GltfImporter::texture(): invalid magFilter property\n"},
    {"invalid sampler minFilter property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:36:20\n"
        "Trade::GltfImporter::texture(): invalid minFilter property\n"},
    {"invalid sampler wrapS property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:40:16\n"
        "Trade::GltfImporter::texture(): invalid wrapS property\n"},
    {"invalid sampler wrapT property",
        "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {}:44:16\n"
        "Trade::GltfImporter::texture(): invalid wrapT property\n"},
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

const struct {
    const char* name;
    const char* requiresPlugin;
    const char* message;
} ImageInvalidData[]{
    {"both uri and buffer view", nullptr,
        "expected exactly one of uri or bufferView properties defined"},
    {"invalid buffer view", nullptr,
        "buffer view 2 needs 151 bytes but buffer 1 has only 150"},
    {"missing uri property", nullptr,
        "expected exactly one of uri or bufferView properties defined"},
    {"invalid uri property", nullptr,
        "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Bool at {}:21:14\n"
        "Trade::GltfImporter::image2D(): invalid uri property\n"},
    {"invalid bufferView property", nullptr,
        "Utility::Json::parseUnsignedInt(): too large integer literal -2 at {}:25:21\n"
        "Trade::GltfImporter::image2D(): invalid bufferView property\n"},
    {"strided buffer view", nullptr,
        "buffer view 3 is strided"},
    {"data uri magic not recognizable", nullptr,
        "Trade::AnyImageImporter::openData(): cannot determine the format from signature 0x53454b52\n"},
    {"not a 2D image", "DdsImporter",
        "expected exactly one 2D image in an image file but got 0"},
};

const struct {
    const char* name;
    const char* message;
} ImageInvalidNotFoundData[]{
    {"uri not found", "Trade::AbstractImporter::openFile(): cannot open file /nonexistent.png"},
    {"buffer not found", "Trade::GltfImporter::image2D(): error opening /nonexistent.bin"}
};

const struct {
    const char* name;
    const char* file;
    const char* message;
} ExperimentalTextureKtxInvalidWholeFileData[]{
    {"invalid extensions property",
        "texture-invalid-ktx-extensions-property.gltf",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at {}:13:21\n"
        "Trade::GltfImporter::openData(): invalid extensions property in texture 1\n"},
    {"invalid extension",
        "texture-invalid-ktx-extension.gltf",
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at {}:23:28\n"
        "Trade::GltfImporter::openData(): invalid KHR_texture_ktx extension in texture 2\n"},
    {"missing extension source property",
        "texture-invalid-ktx-missing-extension-source-property.gltf",
        "missing or invalid MSFT_texture_dds source property in texture 1"},
    {"invalid extension source property",
        "texture-invalid-ktx-extension-source-property.gltf",
        "Utility::Json::parseUnsignedInt(): too large integer literal -2 at {}:25:21\n"
        "Trade::GltfImporter::openData(): missing or invalid MSFT_texture_dds source property in texture 2\n"},
    {"extension source out of bounds",
        "texture-invalid-ktx-extension-source-oob.gltf",
        "index 2 in texture 0 out of range for 2 images"},
    {"missing source property",
        "texture-invalid-ktx-missing-source-property.gltf",
        "missing or invalid source property in texture 1"},
    {"invalid source property",
        "texture-invalid-ktx-source-property.gltf",
        "Utility::Json::parseUnsignedInt(): too large integer literal -2 at {}:22:17\n"
        "Trade::GltfImporter::openData(): missing or invalid source property in texture 2\n"},
    {"source out of bounds",
        "texture-invalid-ktx-source-oob.gltf",
        "index 2 in texture 0 out of range for 2 images"},
    {"referenced with a layer, then with no extension",
        "texture-invalid-ktx-layer-then-no-extension.gltf",
        "texture 3 references image 1 as 2D but an earlier texture referenced it as a 2D array layer"},
    {"referenced with a layer, then with no layer",
        "texture-invalid-ktx-layer-then-no-layer.gltf",
        "texture 3 references image 2 as 2D but an earlier texture referenced it as a 2D array layer"},
    {"referenced with no extension, then with a layer",
        "texture-invalid-ktx-no-extension-then-layer.gltf",
        "texture 3 references image 1 as a 2D array layer but an earlier texture referenced it as 2D"},
    {"referenced with no layer, then with a layer",
        "texture-invalid-ktx-no-layer-then-layer.gltf",
        "texture 3 references image 2 as a 2D array layer but an earlier texture referenced it as 2D"},
};

const struct {
    const char* name;
    const char* message;
} ExperimentalTextureKtxInvalidMaterialData[]{
    {"invalid layer",
        "Utility::Json::parseUnsignedInt(): too large integer literal -3 at {}:19:20\n"
        "Trade::GltfImporter::material(): invalid KHR_texture_ktx layer property\n"}
};

const struct {
    const char* name;
    Int dimensions;
    const char* message;
} ExperimentalTextureKtxInvalidImageData[]{
    {"not a 3D image", 3,
        "expected exactly one 3D image in an image file but got 0"},
    {"not a 2D image", 2,
        "expected exactly one 2D image in an image file but got 0"}
};

constexpr struct {
    const char* name;
    const char* file;
    const char* message;
} UnsupportedVersionData[]{
    {"version 1.0", "version-legacy.gltf", "unsupported version 1.0, expected 2.x"},
    {"version 3.0", "version-unsupported.gltf", "unsupported version 3.0, expected 2.x"},
    {"minVersion 2.1", "version-unsupported-min.gltf", "unsupported minVersion 2.1, expected 2.0"}
};

/* Shared among all plugins that implement data copying optimizations */
const struct {
    const char* name;
    bool(*open)(AbstractImporter&, Containers::ArrayView<const void>);
} OpenMemoryData[]{
    {"data", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        /* Copy to ensure the original memory isn't referenced */
        Containers::Array<char> copy{NoInit, data.size()};
        Utility::copy(Containers::arrayCast<const char>(data), copy);
        return importer.openData(copy);
    }},
    {"memory", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        return importer.openMemory(data);
    }},
};

GltfImporterTest::GltfImporterTest() {
    addInstancedTests({&GltfImporterTest::open},
                      Containers::arraySize(SingleFileData));

    addInstancedTests({&GltfImporterTest::openError},
                      Containers::arraySize(OpenErrorData));

    addTests({&GltfImporterTest::openFileError,
              &GltfImporterTest::openIgnoreUnknownChunk});

    addInstancedTests({&GltfImporterTest::openExternalDataOrder},
        Containers::arraySize(SingleFileData));

    addTests({&GltfImporterTest::openExternalDataNoPathNoCallback});

    addInstancedTests({&GltfImporterTest::openExternalDataTooLong},
        Containers::arraySize(SingleFileData));

    addInstancedTests({&GltfImporterTest::openExternalDataTooShort},
        Containers::arraySize(MultiFileData));

    addInstancedTests({&GltfImporterTest::openExternalDataInvalidUri},
                      Containers::arraySize(InvalidUriData));

    addTests({&GltfImporterTest::requiredExtensions,
              &GltfImporterTest::requiredExtensionsUnsupported,
              &GltfImporterTest::requiredExtensionsUnsupportedDisabled});

    addInstancedTests({&GltfImporterTest::animation},
                      Containers::arraySize(MultiFileData));

    addInstancedTests({&GltfImporterTest::animationInvalid},
        Containers::arraySize(AnimationInvalidData));

    addInstancedTests({&GltfImporterTest::animationInvalidBufferNotFound},
        Containers::arraySize(AnimationInvalidBufferNotFoundData));

    addTests({&GltfImporterTest::animationMissingTargetNode});

    addInstancedTests({&GltfImporterTest::animationSpline},
                      Containers::arraySize(MultiFileData));

    addTests({&GltfImporterTest::animationSplineSharedWithSameTimeTrack,
              &GltfImporterTest::animationSplineSharedWithDifferentTimeTrack,

              &GltfImporterTest::animationShortestPathOptimizationEnabled,
              &GltfImporterTest::animationShortestPathOptimizationDisabled,
              &GltfImporterTest::animationQuaternionNormalizationEnabled,
              &GltfImporterTest::animationQuaternionNormalizationDisabled,
              &GltfImporterTest::animationMergeEmpty,
              &GltfImporterTest::animationMerge});

    addTests({&GltfImporterTest::camera});

    addInstancedTests({&GltfImporterTest::cameraInvalid},
        Containers::arraySize(CameraInvalidData));

    addTests({&GltfImporterTest::light});

    addInstancedTests({&GltfImporterTest::lightInvalid},
        Containers::arraySize(LightInvalidData));

    addTests({&GltfImporterTest::scene});

    addInstancedTests({&GltfImporterTest::sceneInvalidWholeFile},
        Containers::arraySize(SceneInvalidWholeFileData));

    addInstancedTests({&GltfImporterTest::sceneInvalid},
        Containers::arraySize(SceneInvalidData));

    addTests({&GltfImporterTest::sceneDefaultNoDefault,
              &GltfImporterTest::sceneDefaultOutOfBounds,
              &GltfImporterTest::sceneTransformation,
              &GltfImporterTest::sceneTransformationQuaternionNormalizationEnabled,
              &GltfImporterTest::sceneTransformationQuaternionNormalizationDisabled,
              &GltfImporterTest::sceneCustomFields,
              &GltfImporterTest::sceneCustomFieldsInvalidConfiguration});

    addInstancedTests({&GltfImporterTest::skin},
        Containers::arraySize(MultiFileData));

    addInstancedTests({&GltfImporterTest::skinInvalid},
        Containers::arraySize(SkinInvalidData));

    addTests({&GltfImporterTest::skinInvalidBufferNotFound});

    addInstancedTests({&GltfImporterTest::mesh},
                      Containers::arraySize(MultiFileData));

    addInstancedTests({&GltfImporterTest::meshNoAttributes},
        Containers::arraySize(MeshNoAttributesData));

    addTests({&GltfImporterTest::meshNoIndices});

    addInstancedTests({&GltfImporterTest::meshNoIndicesNoAttributes},
        Containers::arraySize(MeshNoAttributesData));

    addInstancedTests({&GltfImporterTest::meshNoIndicesNoVerticesNoBufferUri},
        Containers::arraySize(MeshNoVerticesData));

    addTests({&GltfImporterTest::meshColors});

    addInstancedTests({&GltfImporterTest::meshSkinAttributes},
        Containers::arraySize(MeshSkinAttributeData));

    addTests({&GltfImporterTest::meshCustomAttributes,
              &GltfImporterTest::meshCustomAttributesNoFileOpened,
              &GltfImporterTest::meshDuplicateAttributes,
              &GltfImporterTest::meshUnorderedAttributes,
              &GltfImporterTest::meshMultiplePrimitives});

    addInstancedTests({&GltfImporterTest::meshUnsignedIntVertexFormats},
        Containers::arraySize(MeshUnsignedIntVertexFormatsData));

    addInstancedTests({&GltfImporterTest::meshUnsupportedVertexFormats},
        Containers::arraySize(MeshUnsupportedVertexFormatsData));

    addInstancedTests({&GltfImporterTest::meshPrimitivesTypes},
        Containers::arraySize(MeshPrimitivesTypesData));

    addTests({&GltfImporterTest::meshSizeNotMultipleOfStride});

    addInstancedTests({&GltfImporterTest::meshInvalidWholeFile},
        Containers::arraySize(MeshInvalidWholeFileData));

    addInstancedTests({&GltfImporterTest::meshInvalid},
        Containers::arraySize(MeshInvalidData));

    addInstancedTests({&GltfImporterTest::meshInvalidBufferNotFound},
        Containers::arraySize(MeshInvalidBufferNotFoundData));

    addTests({&GltfImporterTest::materialPbrMetallicRoughness,
              &GltfImporterTest::materialPbrSpecularGlossiness,
              &GltfImporterTest::materialCommon,
              &GltfImporterTest::materialUnlit,
              &GltfImporterTest::materialExtras,
              &GltfImporterTest::materialClearCoat,
              &GltfImporterTest::materialPhongFallback,
              &GltfImporterTest::materialRaw,
              &GltfImporterTest::materialRawIor,
              &GltfImporterTest::materialRawSpecular,
              &GltfImporterTest::materialRawTransmission,
              &GltfImporterTest::materialRawVolume,
              &GltfImporterTest::materialRawSheen,
              &GltfImporterTest::materialRawOutOfBounds});

    addInstancedTests({&GltfImporterTest::materialInvalid},
        Containers::arraySize(MaterialInvalidData));

    addInstancedTests({&GltfImporterTest::textureCoordinateYFlip},
        Containers::arraySize(TextureCoordinateYFlipData));

    addTests({&GltfImporterTest::texture});

    addInstancedTests({&GltfImporterTest::textureExtensions},
                      Containers::arraySize(TextureExtensionsData));

    addInstancedTests({&GltfImporterTest::textureInvalid},
                      Containers::arraySize(TextureInvalidData));

    addInstancedTests({&GltfImporterTest::imageEmbedded},
                      Containers::arraySize(ImageEmbeddedData));

    addInstancedTests({&GltfImporterTest::imageExternal},
                      Containers::arraySize(ImageExternalData));

    addTests({&GltfImporterTest::imageExternalNoPathNoCallback});

    addInstancedTests({&GltfImporterTest::imageBasis},
                      Containers::arraySize(ImageBasisData));

    addTests({&GltfImporterTest::imageMipLevels});

    addInstancedTests({&GltfImporterTest::imageInvalid},
                      Containers::arraySize(ImageInvalidData));

    addInstancedTests({&GltfImporterTest::imageInvalidNotFound},
        Containers::arraySize(ImageInvalidNotFoundData));

    addTests({&GltfImporterTest::imagePropagateImporterFlags});

    addTests({&GltfImporterTest::experimentalKhrTextureKtx2D,
              &GltfImporterTest::experimentalKhrTextureKtx2DArray,
              &GltfImporterTest::experimentalKhrTextureKtxPhongFallback,
              &GltfImporterTest::experimentalKhrTextureKtxNotEnabled});

    addInstancedTests({&GltfImporterTest::experimentalKhrTextureKtxInvalidWholeFile},
        Containers::arraySize(ExperimentalTextureKtxInvalidWholeFileData));

    addInstancedTests({&GltfImporterTest::experimentalKhrTextureKtxInvalidMaterial},
        Containers::arraySize(ExperimentalTextureKtxInvalidMaterialData));

    addInstancedTests({&GltfImporterTest::experimentalKhrTextureKtxInvalidImage},
        Containers::arraySize(ExperimentalTextureKtxInvalidImageData));

    addInstancedTests({&GltfImporterTest::fileCallbackBuffer,
                       &GltfImporterTest::fileCallbackBufferNotFound,
                       &GltfImporterTest::fileCallbackImage,
                       &GltfImporterTest::fileCallbackImageNotFound},
                      Containers::arraySize(SingleFileData));

    addTests({&GltfImporterTest::utf8filenames,
              &GltfImporterTest::escapedStrings,
              &GltfImporterTest::encodedUris,

              &GltfImporterTest::versionSupported});

    addInstancedTests({&GltfImporterTest::versionUnsupported},
                      Containers::arraySize(UnsupportedVersionData));

    addInstancedTests({&GltfImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&GltfImporterTest::openTwice,
              &GltfImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. */
    #ifdef GLTFIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(GLTFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
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
    #ifdef DDSIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(DDSIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef KTXIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(KTXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void GltfImporterTest::open() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "empty"_s + data.suffix);
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_VERIFY(importer->isOpened());

    /* Importer state should give the JSON instance */
    const auto* state = static_cast<const Utility::Json*>(importer->importerState());
    CORRADE_VERIFY(state);
    CORRADE_COMPARE(state->root()["asset"]["version"].asString(), "2.0");

    Containers::Optional<Containers::Array<char>> file = Utility::Path::read(filename);
    CORRADE_VERIFY(file);
    CORRADE_VERIFY(importer->openData(*file));
    CORRADE_VERIFY(importer->isOpened());

    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void GltfImporterTest::openError() {
    auto&& data = OpenErrorData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(data.data));
    /* If the message ends with a newline, it's the whole output, otherwise
       just the sentence */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), data.message);
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::openData(): {}\n", data.message));
}

void GltfImporterTest::openFileError() {
    /* To verify the filename gets correctly propagated into the error message */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "error.gltf");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(filename));
    CORRADE_COMPARE(out.str(), Utility::formatString(
        "Utility::Json::parseObject(): expected an object, got Utility::JsonToken::Type::Array at {}:2:12\n"
        "Trade::GltfImporter::openData(): missing or invalid asset property\n", filename));
}

void GltfImporterTest::openIgnoreUnknownChunk() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    std::ostringstream out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(importer->openData(
        "glTF\x02\x00\x00\x00\x5d\x00\x00\x00"
        "\x1b\x00\x00\x00JSON{\"asset\":{\"version\":\"2.0\"}}"
        "\x04\x00\x00\0BIB\0\xff\xff\xff\xff"
        "\x02\x00\x00\0BIN\0\xab\xcd" /* this one gets picked, other ignored */
        "\x03\x00\x00\0BIG\0\xef\xff\xff"
        "\x05\x00\x00\0BIN\0\x01\x23\x45\x67\x89"_s)); /* duplicate BIN ignored */
    CORRADE_COMPARE(out.str(),
        "Trade::GltfImporter::openData(): ignoring chunk 0x424942 at 47\n"
        "Trade::GltfImporter::openData(): ignoring chunk 0x474942 at 69\n"
        "Trade::GltfImporter::openData(): ignoring chunk 0x4e4942 at 80\n");
}

void GltfImporterTest::openExternalDataOrder() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

            return callbackData.rs.getRaw(Utility::Path::join("some/path", filename));
        }, callbackData);

    /* Prevent the file callback being used for the main glTF content */
    Containers::Optional<Containers::Array<char>> content = Utility::Path::read(Utility::Path::join(GLTFIMPORTER_TEST_DIR,
        "external-data-order"_s + data.suffix));
    CORRADE_VERIFY(content);
    CORRADE_VERIFY(importer->openData(*content));

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

void GltfImporterTest::openExternalDataNoPathNoCallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    Containers::Optional<Containers::Array<char>> file = Utility::Path::read(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "buffer-long-size.gltf"));
    CORRADE_VERIFY(file);
    CORRADE_VERIFY(importer->openData(*file));
    CORRADE_COMPARE(importer->meshCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::mesh(): external buffers can be imported only when opening files from the filesystem or if a file callback is present\n");
}

void GltfImporterTest::openExternalDataTooLong() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "buffer-long-size"_s + data.suffix)));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_VERIFY(importer->mesh(0));
}

void GltfImporterTest::openExternalDataTooShort() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "buffer-invalid-short-size"_s + data.suffix)));
    CORRADE_COMPARE(importer->meshCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::mesh(): buffer 0 is too short, expected 24 bytes but got 12\n");
}

void GltfImporterTest::openExternalDataInvalidUri() {
    auto&& data = InvalidUriData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "invalid-uri.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->image2DCount(), Containers::arraySize(InvalidUriData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(data.name));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::image2D(): {}\n", data.message));
}

void GltfImporterTest::requiredExtensions() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "required-extensions.gltf")));
}

void GltfImporterTest::requiredExtensionsUnsupported() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    /* Disabled by default */
    CORRADE_VERIFY(!importer->configuration().value<bool>("ignoreRequiredExtensions"));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "required-extensions-unsupported.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::openData(): required extension EXT_lights_image_based not supported, enable ignoreRequiredExtensions to ignore\n");
}

void GltfImporterTest::requiredExtensionsUnsupportedDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->configuration().setValue("ignoreRequiredExtensions", true));

    std::ostringstream out;
    Warning redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "required-extensions-unsupported.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::openData(): required extension EXT_lights_image_based not supported, ignoring\n");
}

void GltfImporterTest::animation() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

        /* Importer state should give the glTF animation object */
        const auto* state = static_cast<const Utility::JsonToken*>(animation->importerState());
        CORRADE_VERIFY(state);
        CORRADE_COMPARE((*state)["name"].asString(), "empty");

    /* Empty translation/rotation/scaling animation */
    } {
        Containers::Optional<Trade::AnimationData> animation = importer->animation("empty TRS animation");
        CORRADE_VERIFY(animation);
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

void GltfImporterTest::animationInvalid() {
    auto&& data = AnimationInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation-invalid.gltf");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->animationCount(), Containers::arraySize(AnimationInvalidData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(data.name));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::animation(): {}\n", data.message));
}

void GltfImporterTest::animationInvalidBufferNotFound() {
    auto&& data = AnimationInvalidBufferNotFoundData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation-invalid-buffer-notfound.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->animationCount(), Containers::arraySize(AnimationInvalidBufferNotFoundData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation(data.name));
    /* There's an error from Path::read() before */
    CORRADE_COMPARE_AS(out.str(),
        Utility::format("\nTrade::GltfImporter::animation(): {}\n", data.message),
        TestSuite::Compare::StringHasSuffix);
}

void GltfImporterTest::animationMissingTargetNode() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation-missing-target-node.gltf")));
    CORRADE_COMPARE(importer->animationCount(), 1);

    /* The importer skips channels that don't have a target node */

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

void GltfImporterTest::animationSpline() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation"_s + data.suffix)));

    Containers::Optional<Trade::AnimationData> animation = importer->animation("TRS animation, splines");
    CORRADE_VERIFY(animation);
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

void GltfImporterTest::animationSplineSharedWithSameTimeTrack() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation-splines-sharing.gltf")));

    Containers::Optional<Trade::AnimationData> animation = importer->animation("TRS animation, splines, sharing data with the same time track");
    CORRADE_VERIFY(animation);
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

void GltfImporterTest::animationSplineSharedWithDifferentTimeTrack() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation-splines-sharing.gltf")));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->animation("TRS animation, splines, sharing data with different time track"));
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::animation(): spline track is shared with different time tracks, we don't support that, sorry\n");
}

void GltfImporterTest::animationShortestPathOptimizationEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

void GltfImporterTest::animationShortestPathOptimizationDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

void GltfImporterTest::animationQuaternionNormalizationEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::animation(): quaternions in some rotation tracks were renormalized\n");
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

void GltfImporterTest::animationQuaternionNormalizationDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

void GltfImporterTest::animationMergeEmpty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    /* Enable animation merging */
    importer->configuration().setValue("mergeAnimationClips", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "empty.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 0);
    CORRADE_COMPARE(importer->animationForName(""), -1);
}

void GltfImporterTest::animationMerge() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    /* Enable animation merging */
    importer->configuration().setValue("mergeAnimationClips", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "animation.gltf")));

    CORRADE_COMPARE(importer->animationCount(), 1);
    CORRADE_COMPARE(importer->animationName(0), "");
    CORRADE_COMPARE(importer->animationForName(""), -1);

    Containers::Optional<Trade::AnimationData> animation = importer->animation(0);
    CORRADE_VERIFY(animation);
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

    /* No importer state should be present in this case */
    CORRADE_VERIFY(!animation->importerState());
}

void GltfImporterTest::camera() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

        /* Importer state should give the glTF camera object */
        const auto* state = static_cast<const Utility::JsonToken*>(cam->importerState());
        CORRADE_VERIFY(state);
        CORRADE_COMPARE((*state)["name"].asString(), "Orthographic 4:3");
    } {
        Containers::Optional<Trade::CameraData> cam = importer->camera("Perspective 1:1 75° hFoV");
        CORRADE_VERIFY(cam);
        CORRADE_COMPARE(cam->type(), CameraType::Perspective3D);
        CORRADE_COMPARE(cam->fov(), 75.0_degf);
        CORRADE_COMPARE(cam->aspectRatio(), 1.0f);
        CORRADE_COMPARE(cam->near(), 0.1f);
        CORRADE_COMPARE(cam->far(), 150.0f);

        /* Importer state should give the glTF camera object (orthographic and
           perspective cameras are handled separately) */
        const auto* state = static_cast<const Utility::JsonToken*>(cam->importerState());
        CORRADE_VERIFY(state);
        CORRADE_COMPARE((*state)["name"].asString(), "Perspective 1:1 75° hFoV");
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

void GltfImporterTest::cameraInvalid() {
    auto&& data = CameraInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "camera-invalid.gltf");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(Containers::arraySize(CameraInvalidData), importer->cameraCount());

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->camera(data.name));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::camera(): {}\n", data.message));
}

void GltfImporterTest::light() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

        /* Importer state should give the glTF light object */
        const auto* state = static_cast<const Utility::JsonToken*>(light->importerState());
        CORRADE_VERIFY(state);
        CORRADE_COMPARE((*state)["name"].asString(), "Point with everything implicit");

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

void GltfImporterTest::lightInvalid() {
    auto&& data = LightInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "light-invalid.gltf");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->lightCount(), Containers::arraySize(LightInvalidData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->light(data.name));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::light(): {}\n", data.message));
}

void GltfImporterTest::scene() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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
       distinguishes between 2D and 3D), empty parent (which is there always to
       tell which objects belong to the scene) and empty importer state */
    {
        Containers::Optional<SceneData> scene = importer->scene(0);
        CORRADE_VERIFY(scene);
        CORRADE_VERIFY(scene->is3D());
        CORRADE_COMPARE(scene->mappingBound(), 0);
        CORRADE_COMPARE(scene->fieldCount(), 3);
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_COMPARE(scene->fieldType(SceneField::Parent), SceneFieldType::Int);
        CORRADE_COMPARE(scene->fieldSize(SceneField::Parent), 0);
        CORRADE_VERIFY(scene->hasField(SceneField::Transformation));
        CORRADE_COMPARE(scene->fieldType(SceneField::Transformation), SceneFieldType::Matrix4x4);
        CORRADE_COMPARE(scene->fieldSize(SceneField::Transformation), 0);
        CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
        CORRADE_COMPARE(scene->fieldType(SceneField::ImporterState), SceneFieldType::Pointer);
        CORRADE_COMPARE(scene->fieldSize(SceneField::ImporterState), 0);

    /* Testing mainly the hierarchy and light / camera / ... references here.
       Transformations tested in sceneTransformation() and others. */
    } {
        Containers::Optional<SceneData> scene = importer->scene(1);
        CORRADE_VERIFY(scene);
        CORRADE_COMPARE(scene->mappingType(), SceneMappingType::UnsignedInt);
        /* There's object 7 but only in scene 2, so this scene should have
           object count only as a max of all referenced objects  */
        CORRADE_COMPARE(scene->mappingBound(), 7);
        CORRADE_COMPARE(scene->fieldCount(), 7 + 1 /* ImporterState */);

        /* Importer state should give the glTF scene object */
        const auto* state = static_cast<const Utility::JsonToken*>(scene->importerState());
        CORRADE_VERIFY(state);
        CORRADE_COMPARE((*state)["name"].asString(), "Scene");

        /* Parents */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Parent), Containers::arrayView<UnsignedInt>({
            2, 4, 5, 6, /* root */
            3, 1, /* children of node 5 */
            0 /* child of node 1 */
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Int>(SceneField::Parent), Containers::arrayView<Int>({
            -1, -1, -1, -1,
            5, 5,
            1
        }), TestSuite::Compare::Container);

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

        /* Importer states should give the glTF node objects, mapping shared
           with the parent field */
        CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::ImporterState),
            scene->mapping<UnsignedInt>(SceneField::Parent),
            TestSuite::Compare::Container);
        Containers::Optional<const void*> objectState = scene->importerStateFor(4);
        CORRADE_VERIFY(objectState && *objectState);
        CORRADE_COMPARE((*static_cast<const Utility::JsonToken*>(*objectState))["name"].asString(), "Light");

    /* Another scene, with no material assignments, so there should be no
       material field. It also references an object that's not in scene 1,
       so the objectCount should account for it. */
    } {
        Containers::Optional<SceneData> scene = importer->scene(2);
        CORRADE_VERIFY(scene);
        CORRADE_COMPARE(scene->mappingType(), SceneMappingType::UnsignedInt);
        CORRADE_COMPARE(scene->mappingBound(), 8);
        CORRADE_COMPARE(scene->fieldCount(), 3 + 1 /* ImporterState */);

        /* Parents, importer state, transformation. Assume it behaves like
           above, no need to test again. */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
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

void GltfImporterTest::sceneInvalidWholeFile() {
    auto&& data = SceneInvalidWholeFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, data.file);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(filename));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::openData(): {}\n", data.message));
}

void GltfImporterTest::sceneInvalid() {
    auto&& data = SceneInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-invalid.gltf");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(Containers::arraySize(SceneInvalidData), importer->sceneCount());

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->scene(data.name));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::scene(): {}\n", data.message));
}

void GltfImporterTest::sceneDefaultNoDefault() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-default-none.gltf")));

    /* There is at least one scene, but it's not made default as some use cases
       may rely on the (lack of the) "scene" property. */
    CORRADE_COMPARE(importer->defaultScene(), -1);
    CORRADE_COMPARE(importer->sceneCount(), 1);
}

void GltfImporterTest::sceneDefaultOutOfBounds() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-default-oob.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::openData(): scene index 0 out of range for 0 scenes\n");
}

void GltfImporterTest::sceneTransformation() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-transformation.gltf")));

    CORRADE_COMPARE(importer->sceneCount(), 7);

    /* Scene with all four transformation fields */
    {
        Containers::Optional<SceneData> scene = importer->scene("Everything");
        CORRADE_VERIFY(scene);
        CORRADE_COMPARE(scene->mappingBound(), 7);
        CORRADE_COMPARE(scene->fieldCount(), 5 + 1 /* ImporterState */);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));

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
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Translation), Containers::arrayView<UnsignedInt>({
            0, 2, 3, 4, 5
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Vector3>(SceneField::Translation), Containers::arrayView<Vector3>({
            {1.5f, -2.5f, 0.3f},
            {1.5f, -2.5f, 0.3f},
            {1.5f, -2.5f, 0.3f},
            {},
            {}
        }), TestSuite::Compare::Container);
        CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
        CORRADE_COMPARE_AS(scene->field<Quaternion>(SceneField::Rotation), Containers::arrayView<Quaternion>({
            Quaternion::rotation(45.0_degf, Vector3::yAxis()),
            Quaternion::rotation(45.0_degf, Vector3::yAxis()),
            {},
            Quaternion::rotation(45.0_degf, Vector3::yAxis()),
            {}
        }), TestSuite::Compare::Container);
        CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
        CORRADE_COMPARE_AS(scene->field<Vector3>(SceneField::Scaling), Containers::arrayView<Vector3>({
            {0.9f, 0.5f, 2.3f},
            {0.9f, 0.5f, 2.3f},
            Vector3{1.0f},
            Vector3{1.0f},
            {0.9f, 0.5f, 2.3f},
        }), TestSuite::Compare::Container);

    /* Both matrices and TRS (and the implicit transformation) */
    } {
        Containers::Optional<SceneData> scene = importer->scene("Matrix + TRS");
        CORRADE_VERIFY(scene);
        CORRADE_COMPARE(scene->fieldCount(), 4 + 1 /* ImporterState */);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));

        /* Assuming both matrices and TRS represent the same, the matrix is
           considered redundant and so only TRS is present in the output. */
        CORRADE_VERIFY(!scene->hasField(SceneField::Transformation));

        /* The implicit transformation object is not contained in these */
        CORRADE_VERIFY(scene->hasField(SceneField::Translation));
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(SceneField::Translation), Containers::arrayView<UnsignedInt>({
            0
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Vector3>(SceneField::Translation), Containers::arrayView<Vector3>({
            {1.5f, -2.5f, 0.3f}
        }), TestSuite::Compare::Container);
        CORRADE_VERIFY(scene->hasField(SceneField::Rotation));
        CORRADE_COMPARE_AS(scene->field<Quaternion>(SceneField::Rotation), Containers::arrayView<Quaternion>({
            Quaternion::rotation(45.0_degf, Vector3::yAxis())
        }), TestSuite::Compare::Container);
        CORRADE_VERIFY(scene->hasField(SceneField::Scaling));
        CORRADE_COMPARE_AS(scene->field<Vector3>(SceneField::Scaling), Containers::arrayView<Vector3>({
            {0.9f, 0.5f, 2.3f}
        }), TestSuite::Compare::Container);

    /* Just matrices (and the implicit transformation) */
    } {
        Containers::Optional<SceneData> scene = importer->scene("Just matrices");
        CORRADE_VERIFY(scene);
        CORRADE_COMPARE(scene->fieldCount(), 2 + 1 /* ImporterState */);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));

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
        CORRADE_COMPARE(scene->fieldCount(), 4 + 1 /* ImporterState */);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));

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
        CORRADE_COMPARE(scene->fieldCount(), 2 + 1 /* ImporterState */);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));

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
        CORRADE_COMPARE(scene->fieldCount(), 2 + 1 /* ImporterState */);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));

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
        CORRADE_COMPARE(scene->fieldCount(), 2 + 1 /* ImporterState */);

        /* Fields we're not interested in */
        CORRADE_VERIFY(scene->hasField(SceneField::Parent));

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

void GltfImporterTest::sceneTransformationQuaternionNormalizationEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    /* Enabled by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("normalizeQuaternions"));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-transformation-patching.gltf")));
    CORRADE_COMPARE(importer->sceneCount(), 1);

    Containers::Optional<SceneData> scene;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        scene = importer->scene(0);
    }
    CORRADE_VERIFY(scene);
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::scene(): rotation quaternion of node 3 was renormalized\n");

    Containers::Optional<Containers::Triple<Vector3, Quaternion, Vector3>> trs = scene->translationRotationScaling3DFor(3);
    CORRADE_VERIFY(trs);
    CORRADE_COMPARE(trs->second(), Quaternion::rotation(45.0_degf, Vector3::yAxis()));
}

void GltfImporterTest::sceneTransformationQuaternionNormalizationDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    /* Explicity disable */
    importer->configuration().setValue("normalizeQuaternions", false);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-transformation-patching.gltf")));
    CORRADE_COMPARE(importer->sceneCount(), 1);

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

void GltfImporterTest::sceneCustomFields() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* Types can be overriden only before opening a file */
    importer->configuration().group("customSceneFieldTypes")->addValue("offset", "Int");
    importer->configuration().group("customSceneFieldTypes")->addValue("flags", "UnsignedInt");
    /* Configuration for a field that's not in the file should do nothing
       even if invalid */
    importer->configuration().group("customSceneFieldTypes")->addValue("bounds", "Range3D[6]");
    /* The radius field stays at the default, which is Float */

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-custom-fields.gltf");
    CORRADE_VERIFY(importer->openFile(filename));

    /* Test mapping in both directions */
    SceneField sceneFieldRadius = importer->sceneFieldForName("radius");
    SceneField sceneFieldOffset = importer->sceneFieldForName("offset");
    SceneField sceneFieldCategory = importer->sceneFieldForName("category");
    SceneField sceneFieldFlags = importer->sceneFieldForName("flags");
    CORRADE_COMPARE(sceneFieldRadius, sceneFieldCustom(1));
    CORRADE_COMPARE(sceneFieldOffset, sceneFieldCustom(2));
    CORRADE_COMPARE(sceneFieldCategory, sceneFieldCustom(4));
    CORRADE_COMPARE(sceneFieldFlags, sceneFieldCustom(5));
    CORRADE_COMPARE(importer->sceneFieldName(sceneFieldCustom(1)), "radius");
    CORRADE_COMPARE(importer->sceneFieldName(sceneFieldCustom(2)), "offset");
    CORRADE_COMPARE(importer->sceneFieldName(sceneFieldCustom(4)), "category");
    CORRADE_COMPARE(importer->sceneFieldName(sceneFieldCustom(5)), "flags");

    /* Unlike in materials, case of custom names is not normalized */
    CORRADE_COMPARE(importer->sceneFieldForName("UppercaseName"), sceneFieldCustom(3));
    CORRADE_COMPARE(importer->sceneFieldName(sceneFieldCustom(3)), "UppercaseName");

    /* Names of custom fields should get gathered right after import,
       independently of whether they are in any scene. Only extras of known
       types are considered, though. */
    CORRADE_COMPARE(importer->sceneFieldForName("invalidBoolField"), SceneField{});
    CORRADE_COMPARE(importer->sceneFieldForName("invalidNullField"), SceneField{});
    CORRADE_COMPARE(importer->sceneFieldForName("invalidArrayField"), SceneField{});
    CORRADE_COMPARE(importer->sceneFieldForName("invalidObjectField"), SceneField{});
    CORRADE_COMPARE(importer->sceneFieldForName("registeredButNotInAnyScene"), sceneFieldCustom(0));

    /* Unknown fields */
    CORRADE_COMPARE(importer->sceneFieldName(sceneFieldCustom(6)), "");
    CORRADE_COMPARE(importer->sceneFieldForName("nonexistent"), SceneField{});

    /* Two scenes, each having a different subset of custom fields */
    CORRADE_COMPARE(importer->sceneCount(), 2);

    {
        Containers::Optional<Trade::SceneData> scene;
        std::ostringstream out;
        {
            Warning redirectWarning{&out};
            Error redirectError{&out};
            scene = importer->scene(0);
        }
        CORRADE_VERIFY(scene);
        CORRADE_COMPARE(out.str(), Utility::formatString(
            "Trade::GltfImporter::scene(): node 2 extras property is Utility::JsonToken::Type::Array, skipping\n"
            "Trade::GltfImporter::scene(): node 3 extras invalidBoolField property is Utility::JsonToken::Type::Bool, skipping\n"
            "Trade::GltfImporter::scene(): node 3 extras invalidNullField property is Utility::JsonToken::Type::Null, skipping\n"
            "Trade::GltfImporter::scene(): node 3 extras invalidArrayField property is Utility::JsonToken::Type::Array, skipping\n"
            "Trade::GltfImporter::scene(): node 3 extras invalidObjectField property is Utility::JsonToken::Type::Object, skipping\n"
            "Utility::Json::parseFloat(): invalid floating-point literal 56.0f at {0}:49:19\n"
            "Trade::GltfImporter::scene(): invalid node 6 extras radius property, skipping\n"
            /* These fail only because the fields have the type overriden */
            "Utility::Json::parseInt(): invalid integer literal 23.5 at {0}:50:19\n"
            "Trade::GltfImporter::scene(): invalid node 6 extras offset property, skipping\n"
            "Utility::Json::parseString(): expected a string, got Utility::JsonToken::Type::Number at {0}:51:21\n"
            "Trade::GltfImporter::scene(): invalid node 6 extras category property, skipping\n"
            "Utility::Json::parseUnsignedInt(): too large integer literal -1 at {0}:52:18\n"
            "Trade::GltfImporter::scene(): invalid node 6 extras flags property, skipping\n"
            "Utility::Json::parseInt(): expected a number, got Utility::JsonToken::Type::String at {0}:53:19\n"
            "Trade::GltfImporter::scene(): invalid node 6 extras offset property, skipping\n", filename));

        /* Parent, ImporterState and Transformation (for marking the scene as
           3D) is there always, plus `radius`, `offset`, `UppercaseName` and
           `category` fields used in nodes of the first scene */
        CORRADE_COMPARE(scene->fieldCount(), 3 + 4);

        CORRADE_VERIFY(scene->hasField(SceneField::Parent));
        CORRADE_VERIFY(scene->hasField(SceneField::ImporterState));

        CORRADE_VERIFY(scene->hasField(sceneFieldRadius));
        CORRADE_COMPARE(scene->fieldType(sceneFieldRadius), SceneFieldType::Float);
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(sceneFieldRadius),
            Containers::arrayView({5u, 5u, 8u}),
            TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Float>(sceneFieldRadius),
            Containers::arrayView({5.25f, 3.5f, 0.5f}),
            TestSuite::Compare::Container);

        CORRADE_VERIFY(scene->hasField(sceneFieldOffset));
        CORRADE_COMPARE(scene->fieldType(sceneFieldOffset), SceneFieldType::Int);
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(sceneFieldOffset),
            Containers::arrayView({5u, 5u, 7u}),
            TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Int>(sceneFieldOffset),
            /* W.T.F., C, why do I need a cast for this?! */
            Containers::arrayView({17, -22, Int(-2147483648)}),
            TestSuite::Compare::Container);

        CORRADE_VERIFY(scene->hasField(sceneFieldCategory));
        CORRADE_COMPARE(scene->fieldType(sceneFieldCategory), SceneFieldType::StringOffset32);
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(sceneFieldCategory),
            Containers::arrayView({5u, 5u, 8u}),
            TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->fieldStrings(sceneFieldCategory), Containers::arrayView({
            "duplicated"_s, "very"_s, "noteless"_s
        }), TestSuite::Compare::Container);
    } {
        Containers::Optional<Trade::SceneData> scene;
        std::ostringstream out;
        {
            Warning redirectWarning{&out};
            scene = importer->scene(1);
        }
        CORRADE_VERIFY(scene);
        /* No warnings should be for the second scene, as the warning nodes are
           not part of it */
        CORRADE_COMPARE(out.str(), "");

        /* Parent, ImporterState and Transformation (for marking the scene as
           3D) is there always, plus `radius`, `flags` and `category` fields
           used in nodes of the second scene */
        CORRADE_COMPARE(scene->fieldCount(), 3 + 3);

        CORRADE_VERIFY(scene->hasField(sceneFieldRadius));
        CORRADE_COMPARE(scene->fieldType(sceneFieldRadius), SceneFieldType::Float);
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(sceneFieldRadius),
            Containers::arrayView({9u, 8u}),
            TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<Float>(sceneFieldRadius),
            Containers::arrayView({5.5f, 0.5f}),
            TestSuite::Compare::Container);

        CORRADE_VERIFY(scene->hasField(sceneFieldFlags));
        CORRADE_COMPARE(scene->fieldType(sceneFieldFlags), SceneFieldType::UnsignedInt);
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(sceneFieldFlags),
            Containers::arrayView({9u}),
            TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->field<UnsignedInt>(sceneFieldFlags),
            Containers::arrayView({4294967295u}),
            TestSuite::Compare::Container);

        CORRADE_VERIFY(scene->hasField(sceneFieldCategory));
        CORRADE_COMPARE(scene->fieldType(sceneFieldCategory), SceneFieldType::StringOffset32);
        CORRADE_COMPARE_AS(scene->mapping<UnsignedInt>(sceneFieldCategory),
            Containers::arrayView({9u, 8u}),
            TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(scene->fieldStrings(sceneFieldCategory), Containers::arrayView({
            "positive"_s, "noteless"_s
        }), TestSuite::Compare::Container);
    }
}

void GltfImporterTest::sceneCustomFieldsInvalidConfiguration() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    importer->configuration().group("customSceneFieldTypes")->addValue("offset", "Vector2ui");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "scene-custom-fields.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::openData(): invalid type Vector2ui specified for custom scene field offset\n");
}

void GltfImporterTest::skin() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "skin"_s + data.suffix)));

    CORRADE_COMPARE(importer->skin3DCount(), 2);
    CORRADE_COMPARE(importer->skin3DName(1), "explicit inverse bind matrices");
    CORRADE_COMPARE(importer->skin3DForName("explicit inverse bind matrices"), 1);
    CORRADE_COMPARE(importer->skin3DForName("nonexistent"), -1);

    {
        Containers::Optional<Trade::SkinData3D> skin = importer->skin3D("implicit inverse bind matrices");
        CORRADE_VERIFY(skin);
        CORRADE_COMPARE_AS(skin->joints(),
            Containers::arrayView<UnsignedInt>({1, 2}),
            TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(skin->inverseBindMatrices(),
            Containers::arrayView({Matrix4{}, Matrix4{}}),
            TestSuite::Compare::Container);

        /* Importer state should give the glTF skin object */
        const auto* state = static_cast<const Utility::JsonToken*>(skin->importerState());
        CORRADE_VERIFY(state);
        CORRADE_COMPARE((*state)["name"].asString(), "implicit inverse bind matrices");

    } {
        Containers::Optional<Trade::SkinData3D> skin = importer->skin3D("explicit inverse bind matrices");
        CORRADE_VERIFY(skin);
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

void GltfImporterTest::skinInvalid() {
    auto&& data = SkinInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "skin-invalid.gltf");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(Containers::arraySize(SkinInvalidData), importer->skin3DCount());

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->skin3D(data.name));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::skin3D(): {}\n", data.message));
}

void GltfImporterTest::skinInvalidBufferNotFound() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "skin-invalid-buffer-notfound.gltf")));

    CORRADE_COMPARE(importer->skin3DCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->skin3D("buffer not found"));
    /* There's an error from Path::read() before */
    CORRADE_COMPARE_AS(out.str(),
        "\nTrade::GltfImporter::skin3D(): error opening /nonexistent.bin\n",
        TestSuite::Compare::StringHasSuffix);
}

void GltfImporterTest::mesh() {
    auto&& data = MultiFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh"_s + data.suffix)));

    CORRADE_COMPARE(importer->meshName(0), "Indexed mesh");
    CORRADE_COMPARE(importer->meshForName("Indexed mesh"), 0);
    CORRADE_COMPARE(importer->meshForName("Nonexistent"), -1);

    /* These are present always on a deprecated build; see the
       meshSkinAttributes() test case for details */
    #ifdef MAGNUM_BUILD_DEPRECATED
    const UnsignedInt customAttributeOffset = 2;
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(0)), "JOINTS");
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(1)), "WEIGHTS");
    #else
    const UnsignedInt customAttributeOffset = 0;
    #endif

    /* All attributes including the builtin ones are exposed via
       meshAttributeForName() to prepare for fallback cases where they get
       may get imported as custom. See the meshUnsupportedVertexFormats() test
       case for more information. */
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(customAttributeOffset + 0)), "NORMAL");
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(customAttributeOffset + 1)), "POSITION");
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(customAttributeOffset + 2)), "TANGENT");
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(customAttributeOffset + 3)), "_OBJECT_ID");
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(customAttributeOffset + 4)), "TEXCOORD_0");
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(customAttributeOffset + 5)), "");

    /* Check inverse mapping as well */
    CORRADE_COMPARE(importer->meshAttributeForName("POSITION"), meshAttributeCustom(customAttributeOffset + 1));
    CORRADE_COMPARE(importer->meshAttributeForName("TEXCOORD_0"), meshAttributeCustom(customAttributeOffset + 4));
    CORRADE_COMPARE(importer->meshAttributeForName("_OBJECT_ID"), meshAttributeCustom(customAttributeOffset + 3));

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
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
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::ObjectId), VertexFormat::UnsignedShort);
    CORRADE_COMPARE_AS(mesh->attribute<UnsignedShort>(MeshAttribute::ObjectId),
        Containers::arrayView<UnsignedShort>({
            215, 71, 133
        }), TestSuite::Compare::Container);

    /* Importer state should give the glTF mesh primitive object (i.e., not
       the enclosing mesh). Parent is the primitive array, its parent is the
       "primitives" key, and its parent is the mesh object. */
    const auto* state = static_cast<const Utility::JsonToken*>(mesh->importerState());
    CORRADE_VERIFY(state);
    CORRADE_COMPARE((*state->parent()->parent()->parent())["name"].asString(), "Indexed mesh");
}

void GltfImporterTest::meshNoAttributes() {
    auto&& data = MeshNoAttributesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    if(data.strict)
        importer->configuration().setValue("strict", *data.strict);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh.gltf")));

    Containers::Optional<Trade::MeshData> mesh;
    std::ostringstream out;
    {
        Error redirectError{&out};
        mesh = importer->mesh("Attribute-less indexed mesh");
    }
    CORRADE_COMPARE(!!mesh, !data.strict || !*data.strict); CORRADE_COMPARE(out.str(), data.message);

    if(mesh) {
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
        CORRADE_VERIFY(mesh->isIndexed());
        CORRADE_COMPARE_AS(mesh->indicesAsArray(),
            Containers::arrayView<UnsignedInt>({0, 1, 2}),
            TestSuite::Compare::Container);
        CORRADE_COMPARE(mesh->vertexCount(), 0);
        CORRADE_COMPARE(mesh->attributeCount(), 0);
    }
}

void GltfImporterTest::meshNoIndices() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh.gltf")));

    Containers::Optional<Trade::MeshData> mesh = importer->mesh("Non-indexed mesh");
    CORRADE_VERIFY(mesh);
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

void GltfImporterTest::meshNoIndicesNoAttributes() {
    auto&& data = MeshNoAttributesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    if(data.strict)
        importer->configuration().setValue("strict", *data.strict);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh.gltf")));

    Containers::Optional<Trade::MeshData> mesh;
    std::ostringstream out;
    {
        Error redirectError{&out};
        mesh = importer->mesh("Attribute-less mesh");
    }

    CORRADE_COMPARE(!!mesh, !data.strict || !*data.strict); CORRADE_COMPARE(out.str(), data.message);

    if(mesh) {
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
        CORRADE_VERIFY(!mesh->isIndexed());
        CORRADE_COMPARE(mesh->vertexCount(), 0);
        CORRADE_COMPARE(mesh->attributeCount(), 0);
    }
}

void GltfImporterTest::meshNoIndicesNoVerticesNoBufferUri() {
    auto&& data = MeshNoVerticesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    if(data.strict)
        importer->configuration().setValue("strict", *data.strict);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-no-indices-no-vertices-no-buffer-uri"_s + data.suffix)));

    Containers::Optional<Trade::MeshData> mesh;
    std::ostringstream out;
    {
        Error redirectError{&out};
        mesh = importer->mesh(0);
    }

    CORRADE_COMPARE(!!mesh, !data.strict || !*data.strict); CORRADE_COMPARE(out.str(), data.message);

    if(mesh) {
        CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
        CORRADE_VERIFY(!mesh->isIndexed());
        CORRADE_COMPARE(mesh->vertexCount(), 0);
        CORRADE_COMPARE(mesh->attributeCount(), 1);
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    }
}

void GltfImporterTest::meshColors() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-colors.gltf")));

    CORRADE_COMPARE(importer->meshCount(), 1);

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

void GltfImporterTest::meshSkinAttributes() {
    auto&& data = MeshSkinAttributeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    #ifdef MAGNUM_BUILD_DEPRECATED
    if(data.compatibilitySkinningAttributes)
        importer->configuration().setValue("compatibilitySkinningAttributes", *data.compatibilitySkinningAttributes);
    #endif

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-skin-attributes.gltf")));

    /* The backwards compatibility mapping should be available even before the
       mesh is imported, and for robustness always regardless of whether
       compatibilitySkinningAttributes is set. Otherwise it could happen that
       it won't get added during file opening, but then
       compatibilitySkinningAttributes gets flipped back on and mesh import
       asserts because there's no entry for JOINTS / WEIGHTS in the map. */
    const MeshAttribute jointsAttribute = importer->meshAttributeForName("JOINTS");
    const MeshAttribute weightsAttribute = importer->meshAttributeForName("WEIGHTS");
    #ifdef MAGNUM_BUILD_DEPRECATED
    const UnsignedInt customAttributeOffset = 2;
    CORRADE_COMPARE(jointsAttribute, meshAttributeCustom(0));
    CORRADE_COMPARE(weightsAttribute, meshAttributeCustom(1));
    #else
    const UnsignedInt customAttributeOffset = 0;
    CORRADE_COMPARE(jointsAttribute, MeshAttribute{});
    CORRADE_COMPARE(weightsAttribute, MeshAttribute{});
    #endif

    /* However, the actual numbered names are exposed as well -- which is for
       the case when they would have an invalid vertex format, and thus would
       get imported as custom. See the meshUnsupportedVertexFormats() test case
       for more information. */
    CORRADE_COMPARE(importer->meshAttributeForName("JOINTS_0"), meshAttributeCustom(customAttributeOffset + 0));
    CORRADE_COMPARE(importer->meshAttributeForName("JOINTS_1"), meshAttributeCustom(customAttributeOffset + 1));
    CORRADE_COMPARE(importer->meshAttributeForName("WEIGHTS_0"), meshAttributeCustom(customAttributeOffset + 2));
    CORRADE_COMPARE(importer->meshAttributeForName("WEIGHTS_1"), meshAttributeCustom(customAttributeOffset + 3));

    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->isIndexed());

    /* Position + two pairs of joints & weights */
    #ifdef MAGNUM_BUILD_DEPRECATED
    if(!data.compatibilitySkinningAttributes || *data.compatibilitySkinningAttributes) {
        CORRADE_COMPARE(mesh->attributeCount(), 5 + 4);
    } else
    #endif
    {
        CORRADE_COMPARE(mesh->attributeCount(), 5);
    }
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {1.5f, -1.0f, -0.5f},
            {-0.5f, 2.5f, 0.75f},
            {-2.0f, 1.0f, 0.3f}
        }), TestSuite::Compare::Container);

    /* Attributes. All of them are currently four-component so casting to a
       Vector4<T> for easier comparison */
    /** @todo implement multi-dimensional support in Compare::Container instead
        and drop the workaround */
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::JointIds), 2);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::JointIds, 0), VertexFormat::UnsignedByte);
    CORRADE_COMPARE(mesh->attributeArraySize(MeshAttribute::JointIds, 0), 4);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4ub>(mesh->attribute<UnsignedByte[]>(MeshAttribute::JointIds, 0))),
        Containers::arrayView<Vector4ub>({
            {1,  2,  3,  4},
            {5,  6,  7,  8},
            {9, 10, 11, 12}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::JointIds, 1), VertexFormat::UnsignedShort);
    CORRADE_COMPARE(mesh->attributeArraySize(MeshAttribute::JointIds, 1), 4);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4us>(mesh->attribute<UnsignedShort[]>(MeshAttribute::JointIds, 1))),
        Containers::arrayView<Vector4us>({
            {13, 14, 15, 16},
            {17, 18, 19, 20},
            {21, 22, 23, 24}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeCount(MeshAttribute::Weights), 2);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Weights, 0), VertexFormat::Float);
    CORRADE_COMPARE(mesh->attributeArraySize(MeshAttribute::Weights, 0), 4);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4>(mesh->attribute<Float[]>(MeshAttribute::Weights, 0))),
        Containers::arrayView<Vector4>({
            {0.125f, 0.25f, 0.375f, 0.0f},
            {0.1f,   0.05f, 0.05f,  0.05f},
            {0.2f,   0.0f,  0.3f,   0.0f}
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Weights, 1), VertexFormat::UnsignedShortNormalized);
    CORRADE_COMPARE(mesh->attributeArraySize(MeshAttribute::Weights, 1), 4);
    CORRADE_COMPARE_AS((Containers::arrayCast<1, const Vector4us>(mesh->attribute<UnsignedShort[]>(MeshAttribute::Weights, 1))),
        Containers::arrayView<Vector4us>({
            {       0, 0xffff/8,         0, 0xffff/8},
            {0xffff/2, 0xffff/8, 0xffff/16, 0xffff/16},
            {       0, 0xffff/4, 0xffff/4,  0}
        }), TestSuite::Compare::Container);

    /* Backwards compatibility custom attributes */
    #ifdef MAGNUM_BUILD_DEPRECATED
    if(!data.compatibilitySkinningAttributes || *data.compatibilitySkinningAttributes) {
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

        /* The compat attributes should alias the builtin ones, not have the
           data duplicated */
        CORRADE_COMPARE(mesh->attributeOffset(jointsAttribute), mesh->attributeOffset(MeshAttribute::JointIds));
        CORRADE_COMPARE(mesh->attributeOffset(weightsAttribute), mesh->attributeOffset(MeshAttribute::Weights));
    }
    #endif
}

void GltfImporterTest::meshCustomAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    {
        std::ostringstream out;
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-custom-attributes.gltf")));
        CORRADE_COMPARE(importer->meshCount(), 2);

        CORRADE_COMPARE(out.str(),
            "Trade::GltfImporter::openData(): unknown attribute OBJECT_ID3, importing as custom attribute\n"
            "Trade::GltfImporter::openData(): unknown attribute NOT_AN_IDENTITY, importing as custom attribute\n");
    }

    /* On a deprecated build the first two attributes are hardcoded JOINTS and
       WEIGHTS */
    const UnsignedInt customAttributeOffset =
        #ifdef MAGNUM_BUILD_DEPRECATED
        2
        #else
        0
        #endif
        ;

    /* The mapping should be available even before the mesh is imported.
       Attributes are sorted in declaration order. */
    const MeshAttribute tbnAttribute = importer->meshAttributeForName("_TBN");
    CORRADE_COMPARE(tbnAttribute, meshAttributeCustom(customAttributeOffset + 0));
    CORRADE_COMPARE(importer->meshAttributeName(tbnAttribute), "_TBN");
    CORRADE_COMPARE(importer->meshAttributeForName("Nonexistent"), MeshAttribute{});

    const MeshAttribute uvRotation = importer->meshAttributeForName("_UV_ROTATION");
    CORRADE_COMPARE(uvRotation, meshAttributeCustom(customAttributeOffset + 1));
    CORRADE_COMPARE(importer->meshAttributeName(uvRotation), "_UV_ROTATION");

    const MeshAttribute tbnPreciserAttribute = importer->meshAttributeForName("_TBN_PRECISER");
    const MeshAttribute objectIdAttribute = importer->meshAttributeForName("OBJECT_ID3");

    const MeshAttribute doubleShotAttribute = importer->meshAttributeForName("_DOUBLE_SHOT");
    CORRADE_COMPARE(doubleShotAttribute, meshAttributeCustom(customAttributeOffset + 6));
    const MeshAttribute negativePaddingAttribute = importer->meshAttributeForName("_NEGATIVE_PADDING");
    CORRADE_COMPARE(negativePaddingAttribute, meshAttributeCustom(customAttributeOffset + 4));
    const MeshAttribute notAnIdentityAttribute = importer->meshAttributeForName("NOT_AN_IDENTITY");
    CORRADE_VERIFY(notAnIdentityAttribute != MeshAttribute{});

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

    /* Not testing import failure of non-core glTF attribute types, that's
       already tested in meshInvalid() */
}

void GltfImporterTest::meshCustomAttributesNoFileOpened() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* These should return nothing (and not crash) */
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(564)), "");
    CORRADE_COMPARE(importer->meshAttributeForName("thing"), MeshAttribute{});
}

void GltfImporterTest::meshDuplicateAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

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

void GltfImporterTest::meshUnorderedAttributes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-unordered-attributes.gltf")));
    CORRADE_COMPARE(importer->meshCount(), 1);

    const MeshAttribute customAttribute4 = importer->meshAttributeForName("_CUSTOM_4");
    CORRADE_VERIFY(customAttribute4 != MeshAttribute{});
    const MeshAttribute customAttribute1 = importer->meshAttributeForName("_CUSTOM_1");
    CORRADE_VERIFY(customAttribute1 != MeshAttribute{});

    /* Custom attributes are sorted in declaration order */
    CORRADE_VERIFY(customAttribute4 < customAttribute1);

    std::ostringstream out;
    Warning redirectWarning{&out};

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(), 7);

    /* No warning about _CUSTOM_4 and _CUSTOM_1 */
    CORRADE_COMPARE(out.str(),
        "Trade::GltfImporter::mesh(): found attribute COLOR_3 but expected COLOR_0\n"
        "Trade::GltfImporter::mesh(): found attribute COLOR_9 but expected COLOR_4\n"
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

void GltfImporterTest::meshMultiplePrimitives() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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
    CORRADE_COMPARE(importer->sceneCount(), 1);
    Containers::Optional<SceneData> scene = importer->scene(0);
    CORRADE_COMPARE(scene->mappingBound(), 5);
    CORRADE_COMPARE(scene->fieldCount(), 4 + 1 /* ImporterState */);
    CORRADE_VERIFY(scene->hasField(SceneField::Parent));
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

void GltfImporterTest::meshUnsignedIntVertexFormats() {
    auto&& data = MeshUnsignedIntVertexFormatsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    if(data.strict)
        importer->configuration().setValue("strict", *data.strict);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-unsigned-int-vertex-formats.gltf")));
    CORRADE_COMPARE(importer->meshCount(), 2);

    Containers::Optional<Trade::MeshData> mesh0;
    Containers::Optional<Trade::MeshData> mesh1;
    std::ostringstream out;
    {
        Error redirectError{&out};
        mesh0 = importer->mesh(0);
        mesh1 = importer->mesh(1);
    }

    CORRADE_COMPARE(!!mesh0, !data.strict || !*data.strict);
    CORRADE_COMPARE(!!mesh1, !data.strict || !*data.strict);
    CORRADE_COMPARE(out.str(), data.message);

    if(mesh0) {
        CORRADE_COMPARE(mesh0->attributeCount(), 1);
        CORRADE_VERIFY(isMeshAttributeCustom(mesh0->attributeName(0)));
        CORRADE_COMPARE(importer->meshAttributeName(mesh0->attributeName(0)), "_OFFSET");
        CORRADE_COMPARE(mesh0->attributeFormat(0), VertexFormat::Vector2ui);
        CORRADE_COMPARE_AS(mesh0->attribute<Vector2ui>(0), Containers::arrayView({
            Vector2ui{0xaaaaaaaau, 0xbbbbbbbbu}
        }), TestSuite::Compare::Container);
    }

    if(mesh1) {
        CORRADE_COMPARE(mesh1->attributeCount(), 1);
        CORRADE_COMPARE(mesh1->attributeName(0), MeshAttribute::ObjectId);
        CORRADE_COMPARE(mesh1->attributeFormat(0), VertexFormat::UnsignedInt);
        CORRADE_COMPARE_AS(mesh1->attribute<UnsignedInt>(0), Containers::arrayView({
            0xccccccccu
        }), TestSuite::Compare::Container);
    }
}

void GltfImporterTest::meshUnsupportedVertexFormats() {
    auto&& data = MeshUnsupportedVertexFormatsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    if(data.strict)
        importer->configuration().setValue("strict", *data.strict);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-unsupported-vertex-formats.gltf")));
    CORRADE_COMPARE(importer->meshCount(), 3);

    /* The data have to be split across three meshes because it always bails
       on the first error and so the subsequent errors wouldn't be caught if
       it was all a single mesh */
    Containers::Optional<Trade::MeshData> mesh0;
    Containers::Optional<Trade::MeshData> mesh1;
    Containers::Optional<Trade::MeshData> mesh2;
    std::ostringstream out;
    {
        Error redirectError{&out};
        Warning redirectWarning{&out};
        mesh0 = importer->mesh(0);
        mesh1 = importer->mesh(1);
        mesh2 = importer->mesh(2);
    }
    CORRADE_COMPARE(!!mesh0, !data.strict || !*data.strict);
    CORRADE_COMPARE(!!mesh1, !data.strict || !*data.strict);
    CORRADE_COMPARE(!!mesh2, !data.strict || !*data.strict);
    CORRADE_COMPARE(out.str(), data.message);

    if(mesh0) {
        CORRADE_COMPARE(mesh0->attributeCount(), 2);
        CORRADE_VERIFY(isMeshAttributeCustom(mesh0->attributeName(0)));
        /* The numeric suffix should stay to fully preserve the input and make
           it possible to roundtrip the file with GltfSceneConverter */
        CORRADE_COMPARE(importer->meshAttributeName(mesh0->attributeName(0)), "COLOR_3");
        /* It should be Vector3usNormalized to qualify for a proper color
           attribute */
        CORRADE_COMPARE(mesh0->attributeFormat(0), VertexFormat::Vector4us);
        CORRADE_COMPARE_AS(mesh0->attribute<Vector4us>(0), Containers::arrayView({
            Vector4us{0x1122, 0x3344, 0x5566, 0x7788}
        }), TestSuite::Compare::Container);
    }

    if(mesh1) {
        CORRADE_COMPARE(mesh1->attributeCount(), 2);
        CORRADE_VERIFY(isMeshAttributeCustom(mesh1->attributeName(1)));
        CORRADE_COMPARE(importer->meshAttributeName(mesh1->attributeName(1)), "_OBJECT_ID");
        CORRADE_COMPARE(mesh1->attributeFormat(1), VertexFormat::Short);
        CORRADE_COMPARE_AS(mesh1->attribute<Short>(1), Containers::arrayView({
            Short{-5602}
        }), TestSuite::Compare::Container);
    }

    if(mesh2) {
        CORRADE_COMPARE(mesh2->attributeCount(), 2);
        CORRADE_VERIFY(isMeshAttributeCustom(mesh2->attributeName(0)));
        /* Here the numeric suffix should stay as well, even though internally
           there's extra work done to expose it as a custom JOINTS attribute */
        CORRADE_COMPARE(importer->meshAttributeName(mesh2->attributeName(0)), "JOINTS_7");
        CORRADE_COMPARE(mesh2->attributeFormat(0), VertexFormat::Vector3ub);
        CORRADE_COMPARE_AS(mesh2->attribute<Vector3ub>(0), Containers::arrayView({
            Vector3ub{3, 16, 166}
        }), TestSuite::Compare::Container);
    }

    /* All three meshes should have the same position attribute which didn't
       cause an error */
    if(mesh0) for(auto* mesh: {&*mesh0, &*mesh1, &*mesh2}) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position), Containers::arrayView({
            Vector3{1.0f, 2.0f, 3.0f}
        }), TestSuite::Compare::Container);
    }
}

void GltfImporterTest::meshPrimitivesTypes() {
    auto&& data = MeshPrimitivesTypesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Disable Y-flipping to have consistent results. Tested separately for all
       types in materialTexCoordFlip(). */
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

void GltfImporterTest::meshSizeNotMultipleOfStride() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-size-not-multiple-of-stride.gltf")));
    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {1.0f, 2.0f, 3.0f},
            {4.0f, 5.0f, 6.0f}
        }), TestSuite::Compare::Container);
}

void GltfImporterTest::meshInvalidWholeFile() {
    auto&& data = MeshInvalidWholeFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, data.file);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(filename));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::openData(): {}\n", data.message));
}

void GltfImporterTest::meshInvalid() {
    auto&& data = MeshInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-invalid.gltf");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(Containers::arraySize(MeshInvalidData), importer->meshCount());

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(data.name));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::mesh(): {}\n", data.message));
}

void GltfImporterTest::meshInvalidBufferNotFound() {
    auto&& data = MeshInvalidBufferNotFoundData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "mesh-invalid-buffer-notfound.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->meshCount(), Containers::arraySize(MeshInvalidBufferNotFoundData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(data.name));
    /* There's an error from Path::read() before */
    CORRADE_COMPARE_AS(out.str(),
        Utility::format("\nTrade::GltfImporter::mesh(): {}\n", data.message),
        TestSuite::Compare::StringHasSuffix);
}

void GltfImporterTest::materialPbrMetallicRoughness() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

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

        /* Importer state should give the glTF material object */
        const auto* state = static_cast<const Utility::JsonToken*>(material->importerState());
        CORRADE_VERIFY(state);
        CORRADE_COMPARE((*state)["name"].asString(), "defaults");
    }

    const Containers::Pair<Containers::StringView, MaterialData> materials[]{
        {"color", MaterialData{MaterialType::PbrMetallicRoughness, {
            {MaterialAttribute::BaseColor, Color4{0.3f, 0.4f, 0.5f, 0.8f}},
            {MaterialAttribute::Metalness, 0.56f},
            {MaterialAttribute::Roughness, 0.89f}
        }}},
        {"textures", MaterialData{MaterialType::PbrMetallicRoughness, {
            {MaterialAttribute::BaseColor, Color4{0.7f, 0.8f, 0.9f, 1.1f}},
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::Metalness, 0.6f},
            {MaterialAttribute::Roughness, 0.9f},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 1u},
        }}},
        {"identity texture transform + sets", MaterialData{MaterialType::PbrMetallicRoughness, {
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::BaseColorTextureMatrix, Matrix3{}},
            {MaterialAttribute::BaseColorTextureCoordinates, 0u},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 1u},
            {MaterialAttribute::MetalnessTextureMatrix, Matrix3{}},
            {MaterialAttribute::RoughnessTextureMatrix, Matrix3{}},
            {MaterialAttribute::MetalnessTextureCoordinates, 0u},
            {MaterialAttribute::RoughnessTextureCoordinates, 0u},
        }}},
        {"texture transform", MaterialData{MaterialType::PbrMetallicRoughness, {
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::BaseColorTextureMatrix, Matrix3{
                {0.164968f, 0.472002f, 0.0f}, /* All */
                {-0.472002f, 0.164968f, 0.0f},
                {0.472002f, -0.164968f, 1.0f}
            }},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 1u},
            {MaterialAttribute::MetalnessTextureMatrix, Matrix3{
                {0.5f, 0.0f, 0.0f}, /* Offset + scale */
                {0.0f, 0.5f, 0.0f},
                {0.0f, -0.5f, 1.0f}
            }},
            {MaterialAttribute::RoughnessTextureMatrix, Matrix3{
                {0.5f, 0.0f, 0.0f}, /* Offset + scale */
                {0.0f, 0.5f, 0.0f},
                {0.0f, -0.5f, 1.0f}
            }},
        }}},
        {"texture coordinate sets", MaterialData{MaterialType::PbrMetallicRoughness, {
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::BaseColorTextureCoordinates, 7u},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 1u},
            {MaterialAttribute::MetalnessTextureCoordinates, 5u},
            {MaterialAttribute::RoughnessTextureCoordinates, 5u},
        }}},
        {"empty texture transform with overriden coordinate set", MaterialData{MaterialType::PbrMetallicRoughness, {
            {MaterialAttribute::BaseColorTexture, 1u},
            {MaterialAttribute::BaseColorTextureMatrix, Matrix3{}},
            {MaterialAttribute::NoneRoughnessMetallicTexture, 0u},
            {MaterialAttribute::MetalnessTextureMatrix, Matrix3{}},
            {MaterialAttribute::RoughnessTextureMatrix, Matrix3{}},
            {MaterialAttribute::MetalnessTextureCoordinates, 2u}, /* not 5 */
            {MaterialAttribute::RoughnessTextureCoordinates, 2u},
        }}},
    };

    /* The "defaults" material handled explicitly above */
    CORRADE_COMPARE(importer->materialCount(), Containers::arraySize(materials) + 1);

    for(const auto& expected: materials) {
        Containers::Optional<Trade::MaterialData> material = importer->material(expected.first());
        CORRADE_ITERATION(expected.first());
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material,
            expected.second(),
            DebugTools::CompareMaterial);

        /* Verify the attributes aren't accidentally mismatched and it always
           classifies as packed metallic/roughness texture if one of them is
           present */
        const auto& pbr = material->as<PbrMetallicRoughnessMaterialData>();
        if(pbr.hasMetalnessTexture() || pbr.hasRoughnessTexture())
            CORRADE_VERIFY(pbr.hasNoneRoughnessMetallicTexture());
    }
}

void GltfImporterTest::materialPbrSpecularGlossiness() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

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
    }

    const Containers::Pair<Containers::StringView, MaterialData> materials[]{
        {"color", MaterialData{MaterialType::PbrSpecularGlossiness, {
            {MaterialAttribute::DiffuseColor, Color4{0.3f, 0.4f, 0.5f, 0.8f}},
            {MaterialAttribute::SpecularColor, Color4{0.1f, 0.2f, 0.6f, 0.0f}},
            {MaterialAttribute::Glossiness, 0.89f}
        }}},
        {"textures", MaterialData{MaterialType::PbrSpecularGlossiness, {
            {MaterialAttribute::DiffuseColor, Color4{0.7f, 0.8f, 0.9f, 1.1f}},
            {MaterialAttribute::DiffuseTexture, 0u},
            {MaterialAttribute::SpecularColor, Color4{0.4f, 0.5f, 0.6f, 0.0f}},
            {MaterialAttribute::SpecularGlossinessTexture, 1u},
            {MaterialAttribute::Glossiness, 0.9f}
        }}},
        {"identity texture transform + sets", MaterialData{MaterialType::PbrSpecularGlossiness, {
            {MaterialAttribute::DiffuseTexture, 0u},
            {MaterialAttribute::DiffuseTextureMatrix, Matrix3{}},
            {MaterialAttribute::DiffuseTextureCoordinates, 0u},
            {MaterialAttribute::SpecularGlossinessTexture, 1u},
            {MaterialAttribute::SpecularTextureMatrix, Matrix3{}},
            {MaterialAttribute::GlossinessTextureMatrix, Matrix3{}},
            {MaterialAttribute::SpecularTextureCoordinates, 0u},
            {MaterialAttribute::GlossinessTextureCoordinates, 0u},
        }}},
        {"texture transform", MaterialData{MaterialType::PbrSpecularGlossiness, {
            {MaterialAttribute::DiffuseTexture, 0u},
            {MaterialAttribute::DiffuseTextureMatrix, Matrix3{
                {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.0f, -1.0f, 1.0f}
            }},
            {MaterialAttribute::SpecularGlossinessTexture, 1u},
            {MaterialAttribute::SpecularTextureMatrix, Matrix3{
                {0.5f, 0.0f, 0.0f},
                {0.0f, 0.5f, 0.0f},
                {0.0f, 0.5f, 1.0f}
            }},
            {MaterialAttribute::GlossinessTextureMatrix, Matrix3{
                {0.5f, 0.0f, 0.0f},
                {0.0f, 0.5f, 0.0f},
                {0.0f, 0.5f, 1.0f}
            }},
        }}},
        {"texture coordinate sets", MaterialData{MaterialType::PbrSpecularGlossiness, {
            {MaterialAttribute::DiffuseTexture, 0u},
            {MaterialAttribute::DiffuseTextureCoordinates, 7u},
            {MaterialAttribute::SpecularGlossinessTexture, 1u},
            {MaterialAttribute::SpecularTextureCoordinates, 5u},
            {MaterialAttribute::GlossinessTextureCoordinates, 5u},
        }}},
        {"both metallic/roughness and specular/glossiness", MaterialData{MaterialType::PbrMetallicRoughness|MaterialType::PbrSpecularGlossiness, {
            {MaterialAttribute::BaseColor, Color4{0.3f, 0.4f, 0.5f, 0.8f}},
            {MaterialAttribute::Metalness, 0.56f},
            {MaterialAttribute::Roughness, 0.89f},
            {MaterialAttribute::DiffuseColor, Color4{0.3f, 0.4f, 0.5f, 0.8f}},
            {MaterialAttribute::SpecularColor, Color4{0.1f, 0.2f, 0.6f, 0.0f}},
            {MaterialAttribute::Glossiness, 0.89f}
        }}}
    };

    /* The "defaults" material handled explicitly above */
    CORRADE_COMPARE(importer->materialCount(), Containers::arraySize(materials) + 1);

    for(const auto& expected: materials) {
        Containers::Optional<Trade::MaterialData> material = importer->material(expected.first());
        CORRADE_ITERATION(expected.first());
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material,
            expected.second(),
            DebugTools::CompareMaterial);

        /* Verify the attributes aren't accidentally mismatched and it always
           classifies as packed specular/glossiness texture if one of them is
           present */
        const auto& pbr = material->as<PbrSpecularGlossinessMaterialData>();
        if(pbr.hasSpecularTexture() || pbr.hasGlossinessTexture())
            CORRADE_VERIFY(pbr.hasSpecularGlossinessTexture());
    }
}

void GltfImporterTest::materialCommon() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-common.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 7);

    {
        Containers::Optional<Trade::MaterialData> material = importer->material("defaults");
        CORRADE_COMPARE(material->types(), MaterialTypes{});
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 0);

        /* These are glTF defaults, just verify those are consistent with
           MaterialData API defaults (if they wouldn't be, we'd need to add
           explicit attributes to override those) */
        CORRADE_COMPARE(material->alphaMode(), MaterialAlphaMode::Opaque);
        CORRADE_COMPARE(material->alphaMask(), 0.5f);
    }

    const Containers::Pair<Containers::StringView, MaterialData> materials[]{
        {"alpha mask", MaterialData{{}, {
            {MaterialAttribute::AlphaMask, 0.369f},
        }}},
        {"double-sided alpha blend", MaterialData{{}, {
            {MaterialAttribute::AlphaBlend, true},
            {MaterialAttribute::DoubleSided, true},
        }}},
        {"opaque", MaterialData{{}, {
            {MaterialAttribute::DoubleSided, false}, /* explicit default, kept */
            {MaterialAttribute::AlphaBlend, false},
        }}},
        {"normal, occlusion, emissive texture", MaterialData{{}, {
            {MaterialAttribute::NormalTexture, 1u},
            {MaterialAttribute::NormalTextureScale, 0.56f},
            {MaterialAttribute::OcclusionTexture, 2u},
            {MaterialAttribute::OcclusionTextureStrength, 0.21f},
            {MaterialAttribute::EmissiveColor, Color3{0.1f, 0.2f, 0.3f}},
            {MaterialAttribute::EmissiveTexture, 0u},
        }}},
        {"normal, occlusion, emissive texture identity transform + sets", MaterialData{{}, {
            {MaterialAttribute::NormalTexture, 1u},
            {MaterialAttribute::NormalTextureMatrix, Matrix3{}},
            {MaterialAttribute::NormalTextureCoordinates, 0u},
            {MaterialAttribute::OcclusionTexture, 2u},
            {MaterialAttribute::OcclusionTextureMatrix, Matrix3{}},
            {MaterialAttribute::OcclusionTextureCoordinates, 0u},
            {MaterialAttribute::EmissiveTexture, 0u},
            {MaterialAttribute::EmissiveTextureMatrix, Matrix3{}},
        }}},
        {"normal, occlusion, emissive texture transform + sets", MaterialData{{}, {
            {MaterialAttribute::NormalTexture, 1u},
            {MaterialAttribute::NormalTextureMatrix, Matrix3{
                {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.0f, -1.0f, 1.0f}
            }},
            {MaterialAttribute::NormalTextureCoordinates, 2u},
            {MaterialAttribute::OcclusionTexture, 2u},
            {MaterialAttribute::OcclusionTextureMatrix, Matrix3{
                {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.5f, -1.0f, 1.0f}
            }},
            {MaterialAttribute::OcclusionTextureCoordinates, 3u},
            {MaterialAttribute::EmissiveTexture, 0u},
            {MaterialAttribute::EmissiveTextureMatrix, Matrix3{
                {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.5f, 0.0f, 1.0f}
            }},
            {MaterialAttribute::EmissiveTextureCoordinates, 1u},
        }}}
    };

    /* The "defaults" material handled explicitly above */
    CORRADE_COMPARE(importer->materialCount(), Containers::arraySize(materials) + 1);

    for(const auto& expected: materials) {
        Containers::Optional<Trade::MaterialData> material = importer->material(expected.first());
        CORRADE_ITERATION(expected.first());
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material,
            expected.second(),
            DebugTools::CompareMaterial);
    }
}

void GltfImporterTest::materialUnlit() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-unlit.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 1);

    Containers::Optional<Trade::MaterialData> material = importer->material(0);
    CORRADE_VERIFY(material);
    /* Metallic/roughness is removed from types */
    CORRADE_COMPARE_AS(*material, (MaterialData{MaterialType::Flat, {
        {MaterialAttribute::BaseColor, Color4{0.7f, 0.8f, 0.9f, 1.1f}},
        {MaterialAttribute::BaseColorTexture, 1u}
    }}), DebugTools::CompareMaterial);
}

void GltfImporterTest::materialExtras() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-extras.gltf")));

    {
        for(const char* name: {"primitive", "string", "array"}) {
            CORRADE_ITERATION(name);
            Containers::Optional<MaterialData> material;
            std::ostringstream out;
            {
                Warning redirectWarning{&out};
                material = importer->material(name);
            }
            CORRADE_VERIFY(material);
            CORRADE_COMPARE(material->layerCount(), 1);
            CORRADE_COMPARE(material->attributeCount(), 0);

            CORRADE_COMPARE(out.str(), "Trade::GltfImporter::material(): extras property is not an object, skipping\n");
        }
    } {
        const char* name = "empty";
        CORRADE_ITERATION(name);
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 0);
    } {
        const char* name = "invalid";
        CORRADE_ITERATION(name);
        Containers::Optional<MaterialData> material;
        std::ostringstream out;
        {
            Warning redirectWarning{&out};
            material = importer->material(name);
        }
        CORRADE_VERIFY(material);
        /* All attributes are invalid and ignored */
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 0);

        /** @todo maybe reduce the variants since there's a catch-all error for
            most of them now? */
        CORRADE_COMPARE(out.str(),
            "Trade::GltfImporter::material(): property with an empty name, skipping\n"
            "Trade::GltfImporter::material(): property aValueThatWontFit is too large with 84 bytes, skipping\n"
            "Trade::GltfImporter::material(): property anIncrediblyLongNameThatSadlyWontFitPaddingPaddingPadding!! is too large with 63 bytes, skipping\n"
            "Trade::GltfImporter::material(): property boolArray is not a numeric array, skipping\n"
            "Trade::GltfImporter::material(): property emptyArray is an invalid or unrepresentable numeric vector, skipping\n"
            "Trade::GltfImporter::material(): property mixedBoolArray is not a numeric array, skipping\n"
            "Trade::GltfImporter::material(): property mixedObjectArray is not a numeric array, skipping\n"
            "Trade::GltfImporter::material(): property mixedStringArray is not a numeric array, skipping\n"
            "Trade::GltfImporter::material(): property nestedObject is an object, skipping\n"
            "Trade::GltfImporter::material(): property nestedObjectTexture is an object, skipping\n"
            "Trade::GltfImporter::material(): property null is a null, skipping\n"
            "Trade::GltfImporter::material(): property oversizedArray is an invalid or unrepresentable numeric vector, skipping\n"
            "Trade::GltfImporter::material(): property stringArray is not a numeric array, skipping\n");
    } {
        const char* name = "extras";
        CORRADE_ITERATION(name);
        Containers::Optional<MaterialData> material;
        std::ostringstream out;
        {
            Warning redirectWarning{&out};
            material = importer->material(name);
        }

        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material, (MaterialData{MaterialType::PbrMetallicRoughness|MaterialType::PbrClearCoat, {
            {MaterialAttribute::BaseColor, Color4{0.8f, 0.2f, 0.4f, 0.3f}},
            {MaterialAttribute::BaseColorTexture, 0u},
            {MaterialAttribute::DoubleSided, true},

            /* Extras are in the base layer */
            {"boolTrue"_s, true},
            {"boolFalse"_s, false},
            {"int"_s, -7992835.0f},
            {"unsignedInt"_s, 109835761.0f},
            {"float"_s, 4.321f},
            {"string"_s, "Ribbit -- ribbit"_s},
            {"encodedString"_s, "마이크 체크"_s},
            {"emptyString"_s, ""_s},
            {"doubleSided"_s, false},
            {"vec1"_s, 91.2f},
            {"vec2"_s, Vector2{9.0f, 8.0f}},
            {"vec3"_s, Vector3{9.0f, 0.08f, 7.3141f}},
            {"vec4"_s, Vector4{-9.0f, 8.0f, 7.0f, -6.0f}},
            {"duplicate"_s, true},

            {Trade::MaterialAttribute::LayerName, "ClearCoat"_s},
            {MaterialAttribute::LayerFactor, 0.5f},
            {MaterialAttribute::Roughness, 0.0f}
        }, {17, 20}}), DebugTools::CompareMaterial);

        CORRADE_COMPARE(out.str(), "Trade::GltfImporter::material(): property invalid is not a numeric array, skipping\n");
    }
}

void GltfImporterTest::materialClearCoat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

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
        CORRADE_COMPARE(material->types(), MaterialType::PbrClearCoat);
        CORRADE_COMPARE(material->layerCount(), 2);
        CORRADE_VERIFY(material->hasLayer(MaterialLayer::ClearCoat));

        const auto& pbr = material->as<PbrClearCoatMaterialData>();
        CORRADE_COMPARE(pbr.attributeCount(), 6); /* + layer name */
        /* These are glTF defaults, which are *not* consistent with ours */
        CORRADE_COMPARE(pbr.layerFactor(), 0.0f);
        CORRADE_COMPARE(pbr.roughness(), 0.0f);
        /* Same here, while the layer factor swizzle is ours default, the
           roughness swizzle not */
        CORRADE_COMPARE(pbr.layerFactorTexture(), 0);
        CORRADE_COMPARE(pbr.roughnessTexture(), 0);
        CORRADE_COMPARE(pbr.layerFactorTextureSwizzle(), MaterialTextureSwizzle::R);
        CORRADE_COMPARE(pbr.roughnessTextureSwizzle(), MaterialTextureSwizzle::G);
    }

    const Containers::Pair<Containers::StringView, MaterialData> materials[]{
        {"factors", MaterialData{MaterialType::PbrClearCoat, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.67f},
            {MaterialAttribute::Roughness, 0.34f},
        }, {0, 3}}},
        {"textures", MaterialData{MaterialType::PbrClearCoat, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.7f},
            {MaterialAttribute::LayerFactorTexture, 2u},
            {MaterialAttribute::Roughness, 0.4f},
            {MaterialAttribute::RoughnessTexture, 1u},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureScale, 0.35f},
        }, {0, 8}}},
        {"packed textures", MaterialData{MaterialType::PbrClearCoat, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.0f}, /* silly defaults */
            {MaterialAttribute::LayerFactorTexture, 1u},
            {MaterialAttribute::Roughness, 0.0f}, /* silly defaults */
            {MaterialAttribute::RoughnessTexture, 1u},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
        }, {0, 3 + 3}}},
        {"texture identity transform + sets", MaterialData{MaterialType::PbrClearCoat, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.0f}, /* silly defaults */
            {MaterialAttribute::LayerFactorTexture, 2u},
            {MaterialAttribute::LayerFactorTextureMatrix, Matrix3{}},
            {MaterialAttribute::LayerFactorTextureCoordinates, 0u},
            {MaterialAttribute::Roughness, 0.0f}, /* silly defaults */
            {MaterialAttribute::RoughnessTexture, 1u},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
            {MaterialAttribute::RoughnessTextureMatrix, Matrix3{}},
            {MaterialAttribute::RoughnessTextureCoordinates, 0u},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureMatrix, Matrix3{}},
        }, {0, 9 + 3}}},
        {"texture transform + coordinate set", MaterialData{MaterialType::PbrClearCoat, {
            {MaterialLayer::ClearCoat},
            {MaterialAttribute::LayerFactor, 0.0f}, /* silly defaults */
            {MaterialAttribute::LayerFactorTexture, 2u},
            {MaterialAttribute::LayerFactorTextureMatrix, Matrix3{
                {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.0f, -1.0f, 1.0f}
            }},
            {MaterialAttribute::LayerFactorTextureCoordinates, 5u},
            {MaterialAttribute::Roughness, 0.0f}, /* silly defaults */
            {MaterialAttribute::RoughnessTexture, 1u},
            {MaterialAttribute::RoughnessTextureSwizzle, MaterialTextureSwizzle::G},
            {MaterialAttribute::RoughnessTextureMatrix, Matrix3{
                {0.5f, 0.0f, 0.0f},
                {0.0f, 0.5f, 0.0f},
                {0.0f, 0.5f, 1.0f}
            }},
            {MaterialAttribute::RoughnessTextureCoordinates, 1u},
            {MaterialAttribute::NormalTexture, 0u},
            {MaterialAttribute::NormalTextureMatrix, Matrix3{
                {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.5f, 0.0f, 1.0f}
            }},
            {MaterialAttribute::NormalTextureCoordinates, 7u},
        }, {0, 10 + 3}}}
    };

    /* The "defaults" material handled explicitly above */
    CORRADE_COMPARE(importer->materialCount(), Containers::arraySize(materials) + 1);

    for(const auto& expected: materials) {
        Containers::Optional<Trade::MaterialData> material = importer->material(expected.first());
        CORRADE_ITERATION(expected.first());
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material,
            expected.second(),
            DebugTools::CompareMaterial);

        /* Verify the attributes aren't accidentally mismatched and it always
           classifies as packed factor/roughness texture if both point to
           the same texture */
        const auto& pbr = material->as<PbrClearCoatMaterialData>();
        if(pbr.hasAttribute(MaterialAttribute::LayerFactorTexture) &&
           pbr.hasAttribute(MaterialAttribute::RoughnessTexture) &&
           pbr.attribute<UnsignedInt>(MaterialAttribute::LayerFactorTexture) == pbr.attribute<UnsignedInt>(MaterialAttribute::RoughnessTexture))
            CORRADE_VERIFY(pbr.hasLayerFactorRoughnessTexture());
    }
}

void GltfImporterTest::materialPhongFallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* phongMaterialFallback should be on by default */
    CORRADE_VERIFY(importer->configuration().value<bool>("phongMaterialFallback"));

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-phong-fallback.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 4);

    {
        const char* name = "none";
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE(material->types(), MaterialType::Phong);
        CORRADE_COMPARE(material->layerCount(), 1);
        CORRADE_COMPARE(material->attributeCount(), 0);

        /* These are glTF defaults, just verify those are consistent with
           MaterialData API defaults (if they wouldn't be, we'd need to add
           explicit attributes to override those) */
        const auto& phong = material->as<PhongMaterialData>();
        CORRADE_COMPARE(phong.diffuseColor(), (Color4{1.0f}));
        CORRADE_COMPARE(phong.specularColor(), (Color4{1.0f, 0.0f}));
    }

    const Containers::Pair<Containers::StringView, MaterialData> materials[]{
        {"metallic/roughness", MaterialData{MaterialType::PbrMetallicRoughness|MaterialType::Phong, {
            /* Original properties should stay */
            {MaterialAttribute::BaseColor, Color4{0.7f, 0.8f, 0.9f, 1.1f}},
            {MaterialAttribute::BaseColorTexture, 1u},
            {MaterialAttribute::BaseColorTextureMatrix, Matrix3{
                {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.5f, -1.0f, 1.0f}
            }},
            {MaterialAttribute::BaseColorTextureCoordinates, 3u},
            /* ... and should be copied into phong properties as well */
            {MaterialAttribute::DiffuseColor, Color4{0.7f, 0.8f, 0.9f, 1.1f}},
            {MaterialAttribute::DiffuseTexture, 1u},
            {MaterialAttribute::DiffuseTextureMatrix, Matrix3{
                {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.5f, -1.0f, 1.0f}
            }},
            {MaterialAttribute::DiffuseTextureCoordinates, 3u},
        }}},
        {"specular/glossiness", MaterialData{MaterialType::PbrSpecularGlossiness|MaterialType::Phong, {
            /* Original properties should stay, Phong recognizes them directly */
            {MaterialAttribute::DiffuseColor, Color4{0.7f, 0.8f, 0.9f, 1.1f}},
            {MaterialAttribute::DiffuseTexture, 1u},
            {MaterialAttribute::DiffuseTextureMatrix, Matrix3{
                {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {0.5f, -1.0f, 1.0f}
            }},
            {MaterialAttribute::DiffuseTextureCoordinates, 3u},
            {MaterialAttribute::SpecularColor, Color4{0.1f, 0.2f, 0.6f, 0.0f}},
            {MaterialAttribute::SpecularGlossinessTexture, 0u},
            {MaterialAttribute::SpecularTextureMatrix, Matrix3{
                {0.5f, 0.0f, 0.0f},
                {0.0f, 0.5f, 0.0f},
                {0.0f, 0.5f, 1.0f}
            }},
            {MaterialAttribute::GlossinessTextureMatrix, Matrix3{
                {0.5f, 0.0f, 0.0f},
                {0.0f, 0.5f, 0.0f},
                {0.0f, 0.5f, 1.0f}
            }},
            {MaterialAttribute::SpecularTextureCoordinates, 2u},
            {MaterialAttribute::GlossinessTextureCoordinates, 2u},
        }}},
        /* Phong type is added even for unlit materials, since that's how it
           behaved before */
        {"unlit", MaterialData{MaterialType::Flat|MaterialType::Phong, {
            /* Nothing */
        }}}
    };

    /* The "none" material handled explicitly above */
    CORRADE_COMPARE(importer->materialCount(), Containers::arraySize(materials) + 1);

    for(const auto& expected: materials) {
        Containers::Optional<Trade::MaterialData> material = importer->material(expected.first());
        CORRADE_ITERATION(expected.first());
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material,
            expected.second(),
            DebugTools::CompareMaterial);
    }
}

void GltfImporterTest::materialRaw() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    importer->configuration().setValue("phongMaterialFallback", false);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-raw.gltf");

    CORRADE_VERIFY(importer->openFile(filename));

    Containers::Optional<MaterialData> material;
    std::ostringstream outWarning, outError;
    {
        Warning redirectWarning{&outWarning};
        Error redirectError{&outError};
        material = importer->material("raw");
    }

    CORRADE_VERIFY(material);
    CORRADE_COMPARE_AS(*material, (MaterialData{MaterialType::PbrMetallicRoughness|MaterialType::PbrClearCoat, {
        /* Standard layer import still works */
        {MaterialAttribute::BaseColor, Color4{0.8f, 0.2f, 0.4f, 0.3f}},
        {MaterialAttribute::BaseColorTexture, 0u},
        {MaterialAttribute::DoubleSided, true},

        /* Known extension layer import still works */
        {Trade::MaterialAttribute::LayerName, "ClearCoat"_s},
        {MaterialAttribute::LayerFactor, 0.5f},
        {MaterialAttribute::Roughness, 0.0f},

        /* All attributes in this extension have invalid types or are too
           large, and hence are skipped */
        {Trade::MaterialAttribute::LayerName, "#MAGNUM_material_forbidden_types"_s},

        /* Unknown extension with a textureInfo object */
        {Trade::MaterialAttribute::LayerName, "#MAGNUM_material_snake"_s},
        {"snakeFactor"_s, 6.6f},
        {"snakeTexture"_s, 1u},
        {"snakeTextureMatrix"_s, Matrix3{
            {0.33f, 0.0f,  0.0f},
            {0.0f,  0.44f, 0.0f},
            {0.5f,  1.06f, 1.0f}
        }},
        {"snakeTextureCoordinates"_s, 3u},
        {"snakeTextureScale"_s, 0.2f},
        {"scaleIsAStringTexture"_s, 1u},

        /* Unknown extension with all other supported types */
        {Trade::MaterialAttribute::LayerName, "#MAGNUM_material_type_zoo"_s},
        {"boolTrue"_s, true},
        {"boolFalse"_s, false},
        {"int"_s, -7992835.0f},
        {"unsignedInt"_s, 109835761.0f},
        {"float"_s, 4.321f},
        {"string"_s, "Ribbit -- ribbit"_s},
        {"encodedString"_s, "마이크 체크"_s},
        {"emptyString"_s, ""_s},
        {"uppercaseName"_s, true},
        {"vec1"_s, 91.2f},
        {"vec2"_s, Vector2{9.0f, 8.0f}},
        {"vec3"_s, Vector3{9.0f, 0.08f, 7.3141f}},
        {"vec4"_s, Vector4{-9.0f, 8.0f, 7.0f, -6.0f}},
        {"duplicate"_s, true},

        /* Empty extensions are preserved -- this is mainly for use cases like
           KHR_materials_unlit, where just the presence of the extension alone
           affects material properties */
        {Trade::MaterialAttribute::LayerName, "#VENDOR_empty_extension_object"_s},
    }, {3, 6, 7, 14, 29, 30}}), DebugTools::CompareMaterial);

    /** @todo maybe reduce the variants since there's a catch-all error for
        most of them now? */
    CORRADE_COMPARE(outWarning.str(),
        /* MAGNUM_material_forbidden_types. Attributes are sorted by name. */
        "Trade::GltfImporter::material(): extension with an empty name, skipping\n"
        "Trade::GltfImporter::material(): property with an empty name, skipping\n"
        "Trade::GltfImporter::material(): property Texture has a non-texture object type, skipping\n"
        "Trade::GltfImporter::material(): property aValueThatWontFit is too large with 84 bytes, skipping\n"
        /* These are not sorted because they're not JSON attributes, and added
           in this order by materialTexture() */
        "Trade::GltfImporter::material(): property alsoTestingThisWithAnOverlyElongatedNameButThisTimeForATextureMatrix is too large with 104 bytes, skipping\n"
        "Trade::GltfImporter::material(): property alsoTestingThisWithAnOverlyElongatedNameButThisTimeForATextureCoordinates is too large with 77 bytes, skipping\n"
        "Trade::GltfImporter::material(): property alsoTestingThisWithAnOverlyElongatedNameButThisTimeForATexture is too large with 66 bytes, skipping\n"
        "Trade::GltfImporter::material(): property alsoTestingThisWithAnOverlyElongatedNameButThisTimeForATextureScale is too large with 71 bytes, skipping\n"
        "Trade::GltfImporter::material(): property anIncrediblyLongNameThatSadlyWontFitPaddingPaddingPadding!! is too large with 63 bytes, skipping\n"
        "Trade::GltfImporter::material(): property boolArray is not a numeric array, skipping\n"
        "Trade::GltfImporter::material(): property emptyArray is an invalid or unrepresentable numeric vector, skipping\n"
        /* Error from Utility::Json precedes this */
        "Trade::GltfImporter::material(): property invalidBool is invalid, skipping\n"
        /* Error from Utility::Json precedes this */
        "Trade::GltfImporter::material(): property invalidFloat is invalid, skipping\n"
        /* Error from Utility::Json precedes this */
        "Trade::GltfImporter::material(): property invalidString is invalid, skipping\n"
        /* Error about missing or invalid index precedes this */
        "Trade::GltfImporter::material(): property invalidTexture has an invalid texture object, skipping\n"
        "Trade::GltfImporter::material(): property mixedBoolArray is not a numeric array, skipping\n"
        "Trade::GltfImporter::material(): property mixedObjectArray is not a numeric array, skipping\n"
        "Trade::GltfImporter::material(): property mixedStringArray is not a numeric array, skipping\n"
        "Trade::GltfImporter::material(): property nonTextureObject has a non-texture object type, skipping\n"
        "Trade::GltfImporter::material(): property null is a null, skipping\n"
        "Trade::GltfImporter::material(): property oversizedArray is an invalid or unrepresentable numeric vector, skipping\n"
        "Trade::GltfImporter::material(): property stringArray is not a numeric array, skipping\n"
        /* Error about expecting a number but getting a string precedes this */
        "Trade::GltfImporter::material(): invalid MAGNUM_material_snake scaleIsAStringTexture scale property, skipping\n"
        /* MAGNUM_material_type_zoo */
        "Trade::GltfImporter::material(): property invalid is not a numeric array, skipping\n"
        "Trade::GltfImporter::material(): extension name VENDOR_material_thisnameiswaytoolongforalayername! is too long with 50 characters, skipping\n");
    CORRADE_COMPARE(outError.str(), Utility::formatString(
        "Utility::Json::parseBool(): invalid bool literal fail at {0}:119:36\n"
        "Utility::Json::parseFloat(): invalid floating-point literal 0f at {0}:120:37\n"
        "Utility::Json::parseString(): invalid unicode escape sequence \\uhhhh at {0}:121:39\n"
        "Trade::GltfImporter::material(): missing or invalid invalidTexture index property\n"
        "Utility::Json::parseFloat(): expected a number, got Utility::JsonToken::Type::String at {0}:60:34\n", filename));
}

void GltfImporterTest::materialRawIor() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-ior.gltf")));

    constexpr Containers::StringView layer = "#KHR_materials_ior"_s;

    /** @todo remove the defaults since we have no special-casing anymore */
    const Containers::Pair<Containers::StringView, MaterialData> materials[]{
        {"defaults"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
            }, {0, 1}}},
        {"factors"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"ior"_s, 1.25f}
            }, {0, 2}}}
    };

    CORRADE_COMPARE(importer->materialCount(), Containers::arraySize(materials));

    for(const auto& expected: materials) {
        Containers::Optional<Trade::MaterialData> material = importer->material(expected.first());
        CORRADE_ITERATION(expected.first());
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material,
            expected.second(),
            DebugTools::CompareMaterial);
    }
}

void GltfImporterTest::materialRawSpecular() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-specular.gltf")));

    constexpr Containers::StringView layer = "#KHR_materials_specular"_s;

    /** @todo remove the defaults since we have no special-casing anymore */
    const Containers::Pair<Containers::StringView, MaterialData> materials[]{
        {"defaults"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer}
            }, {0, 1}}},
        {"factors"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"specularFactor"_s, 0.67f},
                {"specularColorFactor"_s, Vector3{0.2f, 0.4f, 0.6f}},
            }, {0, 3}}},
        {"textures"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"specularFactor"_s, 0.7f},
                {"specularColorFactor"_s, Vector3{0.3f, 0.4f, 0.5f}},
                {"specularTexture"_s, 2u},
                {"specularColorTexture"_s, 1u}
            }, {0, 5}}},
        {"texture identity transform"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"specularTexture"_s, 1u},
                {"specularTextureMatrix"_s, Matrix3{}},
                {"specularColorTexture"_s, 0u},
                {"specularColorTextureMatrix"_s, Matrix3{}}
            }, {0, 5}}},
        {"texture transform + coordinate set"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"specularTexture"_s, 2u},
                {"specularTextureCoordinates"_s, 4u},
                {"specularTextureMatrix"_s, Matrix3{
                    {1.0f,  0.0f, 0.0f},
                    {0.0f,  1.0f, 0.0f},
                    {0.0f, -1.0f, 1.0f}
                }},
                {"specularColorTexture"_s, 1u},
                {"specularColorTextureCoordinates"_s, 1u},
                {"specularColorTextureMatrix"_s, Matrix3{
                    {0.5f, 0.0f, 0.0f},
                    {0.0f, 0.5f, 0.0f},
                    {0.0f, 0.5f, 1.0f}
                }}
            }, {0, 7}}}
    };

    CORRADE_COMPARE(importer->materialCount(), Containers::arraySize(materials));

    for(const auto& expected: materials) {
        Containers::Optional<Trade::MaterialData> material = importer->material(expected.first());
        CORRADE_ITERATION(expected.first());
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material,
            expected.second(),
            DebugTools::CompareMaterial);
    }
}

void GltfImporterTest::materialRawTransmission() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-transmission.gltf")));

    constexpr Containers::StringView layer = "#KHR_materials_transmission"_s;

    /** @todo remove the defaults since we have no special-casing anymore */
    const Containers::Pair<Containers::StringView, MaterialData> materials[]{
        {"defaults"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer}
            }, {0, 1}}},
        {"factors"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"transmissionFactor"_s, 0.67f}
            }, {0, 2}}},
        {"textures"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"transmissionFactor"_s, 0.7f},
                {"transmissionTexture"_s, 1u}
            }, {0, 3}}},
        {"texture identity transform"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"transmissionTexture"_s, 0u},
                {"transmissionTextureMatrix"_s, Matrix3{}}
            }, {0, 3}}},
        {"texture transform + coordinate set"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"transmissionTexture"_s, 1u},
                {"transmissionTextureCoordinates"_s, 3u},
                {"transmissionTextureMatrix"_s, Matrix3{
                    {1.0f,  0.0f, 0.0f},
                    {0.0f,  1.0f, 0.0f},
                    {0.0f, -1.0f, 1.0f}
                }}
            }, {0, 4}}}
    };

    CORRADE_COMPARE(importer->materialCount(), Containers::arraySize(materials));

    for(const auto& expected: materials) {
        Containers::Optional<Trade::MaterialData> material = importer->material(expected.first());
        CORRADE_ITERATION(expected.first());
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material,
            expected.second(),
            DebugTools::CompareMaterial);
    }
}

void GltfImporterTest::materialRawVolume() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-volume.gltf")));

    constexpr Containers::StringView layer = "#KHR_materials_volume"_s;

    /** @todo remove the defaults since we have no special-casing anymore */
    const Containers::Pair<Containers::StringView, MaterialData> materials[]{
        {"defaults"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer}
            }, {0, 1}}},
        {"factors"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"thicknessFactor"_s, 0.67f},
                {"attenuationDistance"_s, 15.3f},
                {"attenuationColor"_s, Vector3{0.7f, 0.1f, 1.0f}}
            }, {0, 4}}},
        {"textures"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"thicknessFactor"_s, 0.7f},
                {"attenuationDistance"_s, 1.12f},
                {"attenuationColor"_s, Vector3{0.1f, 0.05f, 0.0f}},
                {"thicknessTexture"_s, 1u},
            }, {0, 5}}},
        {"texture identity transform"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"thicknessTexture"_s, 0u},
                {"thicknessTextureMatrix"_s, Matrix3{}},
            }, {0, 3}}},
        {"texture transform + coordinate set"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"thicknessTexture"_s, 1u},
                {"thicknessTextureCoordinates"_s, 3u},
                {"thicknessTextureMatrix"_s, Matrix3{
                    {1.0f,  0.0f, 0.0f},
                    {0.0f,  1.0f, 0.0f},
                    {0.0f, -1.0f, 1.0f}
                }},
            }, {0, 4}}}
    };

    CORRADE_COMPARE(importer->materialCount(), Containers::arraySize(materials));

    for(const auto& expected: materials) {
        Containers::Optional<Trade::MaterialData> material = importer->material(expected.first());
        CORRADE_ITERATION(expected.first());
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material,
            expected.second(),
            DebugTools::CompareMaterial);
    }
}

void GltfImporterTest::materialRawSheen() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-sheen.gltf")));

    constexpr Containers::StringView layer = "#KHR_materials_sheen"_s;

    /** @todo remove the defaults since we have no special-casing anymore */
    const Containers::Pair<Containers::StringView, MaterialData> materials[]{
        {"defaults"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer}
            }, {0, 1}}},
        {"factors"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"sheenColorFactor"_s, Vector3{0.2f, 0.4f, 0.6f}},
                {"sheenRoughnessFactor"_s, 0.67f}
            }, {0, 3}}},
        {"textures"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"sheenColorFactor"_s, Vector3{0.3f, 0.4f, 0.5f}},
                {"sheenRoughnessFactor"_s, 0.7f},
                {"sheenColorTexture"_s, 1u},
                {"sheenRoughnessTexture"_s, 2u},
            }, {0, 5}}},
        {"texture identity transform"_s, MaterialData{MaterialType{}, {
                {Trade::MaterialAttribute::LayerName, layer},
                {"sheenColorTexture"_s, 1u},
                {"sheenColorTextureMatrix"_s, Matrix3x3{}},
                {"sheenRoughnessTexture"_s, 0u},
                /* sheenRoughnessTextureMatrix is too large and skipped */
            }, {0, 4}}},
        {"texture transform + coordinate set"_s, MaterialData{MaterialType{}, {
            {Trade::MaterialAttribute::LayerName, layer},
            {"sheenColorTexture"_s, 2u},
            {"sheenColorTextureCoordinates"_s, 4u},
            {"sheenColorTextureMatrix"_s, Matrix3{
                {1.0f,  0.0f, 0.0f},
                {0.0f,  1.0f, 0.0f},
                {0.0f, -1.0f, 1.0f}
            }},
            {"sheenRoughnessTexture"_s, 1u},
            {"sheenRoughnessTextureCoordinates"_s, 1u},
            /* sheenRoughnessTextureMatrix is too large and skipped */
        }, {0, 6}}}
    };

    CORRADE_COMPARE(importer->materialCount(), Containers::arraySize(materials));

    std::ostringstream out;
    Warning redirectWarning{&out};

    for(const auto& expected: materials) {
        Containers::Optional<Trade::MaterialData> material = importer->material(expected.first());
        CORRADE_ITERATION(expected.first());
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material,
            expected.second(),
            DebugTools::CompareMaterial);
    }

    CORRADE_COMPARE(out.str(),
        "Trade::GltfImporter::material(): property sheenRoughnessTextureMatrix is too large with 63 bytes, skipping\n"
        "Trade::GltfImporter::material(): property sheenRoughnessTextureMatrix is too large with 63 bytes, skipping\n");
}

void GltfImporterTest::materialRawOutOfBounds() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in materialPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-raw.gltf")));

    std::ostringstream out;
    Containers::Optional<MaterialData> material;
    {
        Error redirectError{&out};
        Warning redirectWarning{&out};
        material = importer->material("raw out-of-bounds");
    }

    CORRADE_VERIFY(material);
    CORRADE_COMPARE_AS(*material, (MaterialData{{}, {
        /* Texture object is ignored because it has an invalid index, the rest
           is kept */
        {Trade::MaterialAttribute::LayerName, "#MAGNUM_material_snake"_s},
        {"snakeFactor"_s, 6.6f}
    }, {0, 2}}), DebugTools::CompareMaterial);

    /** @todo merge with materialRaw()? since the same error is if the texture
        has no index property */
    CORRADE_COMPARE(out.str(),
        "Trade::GltfImporter::material(): snakeTexture index 2 out of range for 2 textures\n"
        "Trade::GltfImporter::material(): property snakeTexture has an invalid texture object, skipping\n");
}

void GltfImporterTest::materialInvalid() {
    auto&& data = MaterialInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-invalid.gltf");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(Containers::arraySize(MaterialInvalidData), importer->materialCount());

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->material(data.name));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::material(): {}\n", data.message));
}

void GltfImporterTest::textureCoordinateYFlip() {
    auto&& data = TextureCoordinateYFlipData[testCaseInstanceId()];
    setTestCaseDescription(Utility::formatString("{}{}", data.materialName, data.flipInMaterial ? ", textureCoordinateYFlipInMaterial" : ""));

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

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
    Containers::Optional<Trade::MaterialData> material = importer->material(data.materialName);
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

void GltfImporterTest::texture() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "texture.gltf")));

    CORRADE_COMPARE(importer->textureCount(), 5);
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

        /* Importer state should give the glTF texture object */
        const auto* state = static_cast<const Utility::JsonToken*>(texture->importerState());
        CORRADE_VERIFY(state);
        CORRADE_COMPARE((*state)["name"].asString(), "another variant");
    } {
        Containers::Optional<Trade::TextureData> texture = importer->texture("shared sampler");
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), 2);
        CORRADE_COMPARE(texture->type(), TextureType::Texture2D);

        /* Same sampler as texture 0, should reuse the cached parsed data */
        CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Nearest);
        CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Nearest);
        CORRADE_COMPARE(texture->wrapping(), Math::Vector3<SamplerWrapping>(SamplerWrapping::MirroredRepeat, SamplerWrapping::ClampToEdge, SamplerWrapping::Repeat));
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

void GltfImporterTest::textureExtensions() {
    auto&& data = TextureExtensionsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "texture-extensions.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->textureCount(), Containers::arraySize(TextureExtensionsData));

    Containers::Optional<Trade::TextureData> texture = importer->texture(data.name);
    CORRADE_VERIFY(texture);
    {
        CORRADE_EXPECT_FAIL_IF(data.xfail, Containers::StringView{data.xfail});
        CORRADE_COMPARE(texture->image(), data.id);
    }
    /* If the original ID check is expected to fail, verify that the ID is
       correctly incorrect */
    if(data.xfail) CORRADE_COMPARE(texture->image(), data.xfailId);
}

void GltfImporterTest::textureInvalid() {
    auto&& data = TextureInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "texture-invalid.gltf");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->textureCount(), Containers::arraySize(TextureInvalidData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->texture(data.name));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::texture(): {}\n", data.message));
}

constexpr char ExpectedImageData[] =
    "\xa8\xa7\xac\xff\x9d\x9e\xa0\xff\xad\xad\xac\xff\xbb\xbb\xba\xff\xb3\xb4\xb6\xff"
    "\xb0\xb1\xb6\xff\xa0\xa0\xa1\xff\x9f\x9f\xa0\xff\xbc\xbc\xba\xff\xcc\xcc\xcc\xff"
    "\xb2\xb4\xb9\xff\xb8\xb9\xbb\xff\xc1\xc3\xc2\xff\xbc\xbd\xbf\xff\xb8\xb8\xbc\xff";

void GltfImporterTest::imageEmbedded() {
    auto&& data = ImageEmbeddedData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(ExpectedImageData).prefix(60), TestSuite::Compare::Container);

    /* Importer state should give the glTF image object */
    const auto* state = static_cast<const Utility::JsonToken*>(image->importerState());
    CORRADE_VERIFY(state);
    CORRADE_COMPARE((*state)["name"].asString(), "Image");
}

void GltfImporterTest::imageExternal() {
    auto&& data = ImageExternalData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "image"_s + data.suffix)));

    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_COMPARE(importer->image2DName(1), "Image");
    CORRADE_COMPARE(importer->image2DForName("Image"), 1);
    CORRADE_COMPARE(importer->image2DForName("Nonexistent"), -1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(ExpectedImageData).prefix(60), TestSuite::Compare::Container);
}

void GltfImporterTest::imageExternalNoPathNoCallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    Containers::Optional<Containers::Array<char>> file = Utility::Path::read(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "image.gltf"));
    CORRADE_VERIFY(file);
    CORRADE_VERIFY(importer->openData(*file));
    CORRADE_COMPARE(importer->image2DCount(), 2);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::image2D(): external images can be imported only when opening files from the filesystem or if a file callback is present\n");
}

void GltfImporterTest::imageBasis() {
    auto&& data = ImageBasisData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    /* Import as ASTC */
    _manager.metadata("BasisImporter")->configuration().setValue("format", "Astc4x4RGBA");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "image-basis"_s + data.suffix)));

    CORRADE_COMPARE(importer->textureCount(), 1);
    CORRADE_COMPARE(importer->image2DCount(), 2);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(5, 3));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc4x4RGBAUnorm);

    /* The texture refers to the image indirectly via an extension, test the
       mapping */
    Containers::Optional<Trade::TextureData> texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->image(), 1);
}

void GltfImporterTest::imageMipLevels() {
    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    /* Import as RGBA so we can verify the pixels */
    _manager.metadata("BasisImporter")->configuration().setValue("format", "RGBA8");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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
    CORRADE_VERIFY(!image11->isCompressed());
    CORRADE_COMPARE(image11->size(), (Vector2i{2, 1}));
    CORRADE_COMPARE(image11->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(Containers::arrayCast<const UnsignedByte>(image11->data()),
        Containers::arrayView<UnsignedByte>({
            172, 172, 181, 255, 184, 184, 193, 255
        }), TestSuite::Compare::Container);
}

void GltfImporterTest::imageInvalid() {
    auto&& data = ImageInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(data.requiresPlugin && _manager.loadState(data.requiresPlugin) == PluginManager::LoadState::NotFound)
        CORRADE_SKIP(data.requiresPlugin << "plugin not found, cannot test");

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "image-invalid.gltf");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(filename));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->image2DCount(), Containers::arraySize(ImageInvalidData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(data.name));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::image2D(): {}\n", data.message));
}

void GltfImporterTest::imageInvalidNotFound() {
    auto&& data = ImageInvalidNotFoundData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "image-invalid-notfound.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->image2DCount(), Containers::arraySize(ImageInvalidNotFoundData));

    Int id = importer->image2DForName(data.name);
    CORRADE_VERIFY(id != -1);

    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer->image2D(id));
        /* image2DLevelCount() can't fail, but should not crash either */
        CORRADE_COMPARE(importer->image2DLevelCount(id), 1);
        /* There's an error from Path::read() before */
        CORRADE_COMPARE_AS(out.str(),
            Utility::formatString("\n{}\n", data.message),
            TestSuite::Compare::StringHasSuffix);

    /* The importer should get cached even in case of failure, so the message
       should get printed just once */
    } {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer->image2D(data.name));
        CORRADE_COMPARE(out.str(), "");
    }
}

void GltfImporterTest::imagePropagateImporterFlags() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    importer->setFlags(ImporterFlag::Verbose);

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "image.gltf")));
    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_VERIFY(importer->image2D(0));
    /* If this starts to fail (possibly due to verbose output from openFile()),
       add \n at the front and change to Compare::StringHasSuffix */
    CORRADE_COMPARE(out.str(),
        "Trade::AnyImageImporter::openFile(): using PngImporter (provided by StbImageImporter)\n");
}

void GltfImporterTest::experimentalKhrTextureKtx2D() {
    if(_manager.loadState("KtxImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in experimentalKhrTextureKtx2DArrayPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    importer->configuration().setValue("experimentalKhrTextureKtx", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "texture-ktx.gltf")));

    /* There are two 3D images as well */
    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_COMPARE(importer->image2DName(1), "PNG");
    CORRADE_COMPARE(importer->image2DForName("PNG"), 1);
    /* 3D images shouldn't leak into 2D mapping */
    CORRADE_COMPARE(importer->image2DForName("2D array KTX"), -1);

    /* The PNG has no mips, the other has 2. In case of the 3D images, there's
       3 mip levels, to avoid mixing them up by accident. */
    Int imageMipsId = importer->image2DForName("2D KTX with mips");
    CORRADE_VERIFY(imageMipsId != -1);
    CORRADE_COMPARE(importer->image2DLevelCount(1), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(imageMipsId), 2);

    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(imageMipsId, 0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(imageMipsId, 1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), (Vector2i{2, 1}));
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D("PNG");
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
        CORRADE_COMPARE(image->size(), (Vector2i{5, 3}));
    }

    /* Three more layer textures got deduplicated */
    CORRADE_COMPARE(importer->textureCount(), 5);
    CORRADE_COMPARE(importer->textureName(4), "PNG"); /* originally texture 5 */
    CORRADE_COMPARE(importer->textureForName("PNG"), 4);

    /* Three textures out of 5 reference the 2D images */
    {
        Containers::Optional<Trade::TextureData> texture = importer->texture("2D");
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), importer->image2DForName("2D KTX with mips"));
        CORRADE_COMPARE(texture->type(), TextureType::Texture2D);
    } {
        Containers::Optional<Trade::TextureData> texture = importer->texture("2D without extension");
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), importer->image2DForName("2D KTX with mips"));
        CORRADE_COMPARE(texture->type(), TextureType::Texture2D);
    } {
        Containers::Optional<Trade::TextureData> texture = importer->texture("PNG");
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), importer->image2DForName("PNG"));
        CORRADE_COMPARE(texture->type(), TextureType::Texture2D);
    }

    /* No changes to materials take place, so no need to test name mapping
       either */
    CORRADE_COMPARE(importer->materialCount(), 7);

    /* Three materials out of 7 reference textures that, in turn, reference the
       2D images. They should not have the layer properties. */
    {
        Containers::Optional<Trade::MaterialData> material = importer->material("2D");
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::EmissiveTexture));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(MaterialAttribute::EmissiveTexture), importer->textureForName("2D"));
        CORRADE_VERIFY(!material->hasAttribute(MaterialAttribute::EmissiveTextureLayer));
    } {
        Containers::Optional<Trade::MaterialData> material = importer->material("2D without extension");
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::NormalTexture));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(MaterialAttribute::NormalTexture), importer->textureForName("2D without extension"));
        CORRADE_VERIFY(!material->hasAttribute(MaterialAttribute::NormalTextureLayer));
    } {
        Containers::Optional<Trade::MaterialData> material = importer->material("PNG");
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::BaseColorTexture));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(MaterialAttribute::BaseColorTexture), importer->textureForName("PNG"));
        CORRADE_VERIFY(!material->hasAttribute(MaterialAttribute::BaseColorTextureLayer));
    }
}

void GltfImporterTest::experimentalKhrTextureKtx2DArray() {
    if(_manager.loadState("KtxImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* Disable Phong material fallback (enabled by default for compatibility),
       testing that separately in experimentalKhrTextureKtx2DArrayPhongFallback() */
    importer->configuration().setValue("phongMaterialFallback", false);

    importer->configuration().setValue("experimentalKhrTextureKtx", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "texture-ktx.gltf")));

    /* There are two 2D images as well */
    CORRADE_COMPARE(importer->image3DCount(), 2);
    CORRADE_COMPARE(importer->image3DName(1), "2D array KTX");
    CORRADE_COMPARE(importer->image3DForName("2D array KTX"), 1);
    /* 2D images shouldn't leak into 3D mapping */
    CORRADE_COMPARE(importer->image3DForName("2D KTX"), -1);

    /* One image has no mips, the other has 3. In case of the 2D images,
       there's 2 mip levels, to avoid mixing them up by accident. */
    Int imageMipsId = importer->image3DForName("2D array KTX with mips");
    CORRADE_VERIFY(imageMipsId != -1);
    CORRADE_COMPARE(importer->image3DLevelCount(1), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(imageMipsId), 3);

    {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D("2D array KTX");
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), (Vector3i{4, 3, 3}));
    } {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(imageMipsId, 0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), (Vector3i{4, 3, 3}));
    } {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(imageMipsId, 1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), (Vector3i{2, 1, 3}));
    } {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(imageMipsId, 2);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), (Vector3i{1, 1, 3}));
    }

    /* Two ... well, three more layer textures got deduplicated */
    {
        CORRADE_EXPECT_FAIL("KHR_texture_ktx deduplication doesn't take differing samplers into account at the moment.");
        CORRADE_COMPARE(importer->textureCount(), 6);
    }
    CORRADE_COMPARE(importer->textureCount(), 5);
    CORRADE_COMPARE(importer->textureName(3), "Second 2D array"); /* originally texture 4 */
    CORRADE_COMPARE(importer->textureForName("Second 2D array"), 3);
    /* Deduplicated textures shouldn't leak into the mapping */
    CORRADE_COMPARE(importer->textureForName("First 2D array again"), -1);
    CORRADE_COMPARE(importer->textureForName("First 2D array again, different sampler"), -1);
    CORRADE_COMPARE(importer->textureForName("Second 2D array again"), -1);

    /* Two textures out of 5 reference the 3D images, the three more textures
       got ignored */
    {
        Containers::Optional<Trade::TextureData> texture = importer->texture("First 2D array");
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), importer->image3DForName("2D array KTX"));
        CORRADE_COMPARE(texture->type(), TextureType::Texture2DArray);
        CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Nearest);
        CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Nearest);
    } {
        Containers::Optional<Trade::TextureData> texture = importer->texture("Second 2D array");
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->image(), importer->image3DForName("2D array KTX with mips"));
        CORRADE_COMPARE(texture->type(), TextureType::Texture2DArray);
        CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Nearest);
        CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Linear);
    }

    /* No changes to materials take place, so no need to test name mapping
       either */
    CORRADE_COMPARE(importer->materialCount(), 7);

    /* Four materials out of 7 reference textures that, in turn, reference the
       2D array image layers. */
    {
        Containers::Optional<Trade::MaterialData> material = importer->material("First 2D array layer 0");
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::OcclusionTexture));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(MaterialAttribute::OcclusionTexture), importer->textureForName("First 2D array"));
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::OcclusionTextureLayer));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(MaterialAttribute::OcclusionTextureLayer), 0);
    } {
        Containers::Optional<Trade::MaterialData> material = importer->material("First 2D array layer 1");
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::NoneRoughnessMetallicTexture));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(MaterialAttribute::NoneRoughnessMetallicTexture), importer->textureForName("First 2D array"));
        /* For packed textures the layer is duplicated for each */
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::RoughnessTextureLayer));
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::MetalnessTextureLayer));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(MaterialAttribute::RoughnessTextureLayer), 1);
        CORRADE_COMPARE(material->attribute<UnsignedInt>(MaterialAttribute::MetalnessTextureLayer), 1);
    } {
        Containers::Optional<Trade::MaterialData> material = importer->material("Second 2D array layer 3");
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::BaseColorTexture));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(MaterialAttribute::BaseColorTexture), importer->textureForName("Second 2D array"));
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::BaseColorTextureLayer));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(MaterialAttribute::BaseColorTextureLayer), 3);
    } {
        Containers::Optional<Trade::MaterialData> material = importer->material("Second 2D array layer 5");
        CORRADE_VERIFY(material);
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::BaseColorTexture));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(MaterialAttribute::BaseColorTexture), importer->textureForName("Second 2D array"));
        CORRADE_VERIFY(material->hasAttribute(MaterialAttribute::BaseColorTextureLayer));
        CORRADE_COMPARE(material->attribute<UnsignedInt>(MaterialAttribute::BaseColorTextureLayer), 5);
    }
}

void GltfImporterTest::experimentalKhrTextureKtxPhongFallback() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    /* Tests only the interaction of experimentalKhrTextureKtx with
       phongMaterialFallback, everything else is tested in
       materialPhongFallback() */

    importer->configuration().setValue("experimentalKhrTextureKtx", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-phong-fallback-ktx.gltf")));
    CORRADE_COMPARE(importer->materialCount(), 3);

    /* Both DiffuseTexture and DiffuseTextureLayer get added */
    {
        const char* name = "metallic/roughness base color array";
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material, (MaterialData{MaterialType::PbrMetallicRoughness|MaterialType::Phong, {
            {MaterialAttribute::BaseColorTexture, 1u},
            {MaterialAttribute::BaseColorTextureLayer, 55u},
            {MaterialAttribute::DiffuseTexture, 1u},
            {MaterialAttribute::DiffuseTextureLayer, 55u},
        }}), DebugTools::CompareMaterial);

    /* Nothing gets added */
    } {
        const char* name = "specular/glossiness diffuse array";
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material, (MaterialData{MaterialType::PbrSpecularGlossiness|MaterialType::Phong, {
            {MaterialAttribute::DiffuseTexture, 1u},
            {MaterialAttribute::DiffuseTextureLayer, 55u},
            {MaterialAttribute::SpecularGlossinessTexture, 0u},
        }}), DebugTools::CompareMaterial);

    /* Nothing gets added here either */
    } {
        const char* name = "specular/glossiness specular array";
        Containers::Optional<Trade::MaterialData> material = importer->material(name);
        CORRADE_ITERATION(name);
        CORRADE_VERIFY(material);
        CORRADE_COMPARE_AS(*material, (MaterialData{MaterialType::PbrSpecularGlossiness|MaterialType::Phong, {
            {MaterialAttribute::DiffuseTexture, 0u},
            {MaterialAttribute::SpecularGlossinessTexture, 1u},
            {MaterialAttribute::SpecularTextureLayer, 55u},
            {MaterialAttribute::GlossinessTextureLayer, 55u},
        }}), DebugTools::CompareMaterial);
    }
}

void GltfImporterTest::experimentalKhrTextureKtxNotEnabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_COMPARE(importer->configuration().value("experimentalKhrTextureKtx"), "false");

    std::ostringstream out;
    Warning redirectWarning{&out};
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "texture-ktx.gltf")));
    CORRADE_COMPARE(out.str(), Utility::formatString(
        /* First is a warning, second is an error */
        "Trade::GltfImporter::openData(): used extension KHR_texture_ktx is experimental, enable experimentalKhrTextureKtx to use it\n"
        "Trade::GltfImporter::openData(): required extension KHR_texture_ktx not supported, enable ignoreRequiredExtensions to ignore\n"));
}

void GltfImporterTest::experimentalKhrTextureKtxInvalidWholeFile() {
    auto&& data = ExperimentalTextureKtxInvalidWholeFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, data.file);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    importer->configuration().setValue("experimentalKhrTextureKtx", true);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(filename));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::openData(): {}\n", data.message));
}

void GltfImporterTest::experimentalKhrTextureKtxInvalidMaterial() {
    auto&& data = ExperimentalTextureKtxInvalidMaterialData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::String filename = Utility::Path::join(GLTFIMPORTER_TEST_DIR, "material-invalid-ktx.gltf");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    importer->configuration().setValue("experimentalKhrTextureKtx", true);

    CORRADE_VERIFY(importer->openFile(filename));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->materialCount(), Containers::arraySize(ExperimentalTextureKtxInvalidMaterialData));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->material(data.name));
    /* If the message ends with a newline, it's the whole output including a
       potential placeholder for the filename, otherwise just the sentence
       without any placeholder */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), Utility::formatString(data.message, filename));
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::material(): {}\n", data.message));
}

void GltfImporterTest::experimentalKhrTextureKtxInvalidImage() {
    auto&& data = ExperimentalTextureKtxInvalidImageData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("KtxImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    importer->configuration().setValue("experimentalKhrTextureKtx", true);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "image-invalid-ktx.gltf")));

    /* Check we didn't forget to test anything */
    CORRADE_COMPARE(importer->image1DCount() + importer->image2DCount() + importer->image3DCount(), Containers::arraySize(ExperimentalTextureKtxInvalidImageData));

    std::ostringstream out;
    Error redirectError{&out};
    if(data.dimensions == 2)
        CORRADE_VERIFY(!importer->image2D(data.name));
    else if(data.dimensions == 3)
        CORRADE_VERIFY(!importer->image3D(data.name));
    else CORRADE_INTERNAL_ASSERT_UNREACHABLE();
    CORRADE_COMPARE(out.str(), Utility::formatString(
        "Trade::GltfImporter::image{}D(): {}\n", data.dimensions, data.message));
}

void GltfImporterTest::fileCallbackBuffer() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

void GltfImporterTest::fileCallbackBufferNotFound() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->features() & ImporterFeature::FileCallback);

    importer->setFileCallback([](const std::string&, InputFileCallbackPolicy, void*)
        -> Containers::Optional<Containers::ArrayView<const char>> { return {}; });

    Utility::Resource rs{"data"};
    CORRADE_VERIFY(importer->openData(rs.getRaw("some/path/data" + std::string{data.suffix})));
    CORRADE_COMPARE(importer->meshCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::mesh(): error opening data.bin through a file callback\n");
}

void GltfImporterTest::fileCallbackImage() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

void GltfImporterTest::fileCallbackImageNotFound() {
    auto&& data = SingleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

void GltfImporterTest::utf8filenames() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

void GltfImporterTest::escapedStrings() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "escaped-strings.gltf")));

    CORRADE_COMPARE(importer->objectCount(), 6);
    CORRADE_COMPARE(importer->objectName(0), "");
    CORRADE_COMPARE(importer->objectName(1), "UTF-8: Лорем ипсум долор сит амет");
    CORRADE_COMPARE(importer->objectName(2), "UTF-8 escaped: Лорем ипсум долор сит амет");
    CORRADE_COMPARE(importer->objectName(3), "Special: \"/\\\b\f\r\n\t");
    CORRADE_COMPARE(importer->objectName(4), "Everything: říční člun \t\t\n حليب اللوز");
    /* Old versions of the spec used to forbid non-ASCII keys or enums:
       https://github.com/KhronosGroup/glTF/tree/fd3ab461a1114fb0250bd76099153d2af50a7a1d/specification/2.0#json-encoding
       Newer spec versions changed this to "ASCII characters [...] SHOULD be
       written without JSON escaping". Nevertheless, our JSON parser handles
       that properly. */
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

void GltfImporterTest::encodedUris() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

void GltfImporterTest::versionSupported() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "version-supported.gltf")));
}

void GltfImporterTest::versionUnsupported() {
    auto&& data = UnsupportedVersionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, data.file)));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::GltfImporter::openData(): {}\n", data.message));
}

void GltfImporterTest::openMemory() {
    /* Same as (a subset of) camera() except that it uses openData() &
       openMemory() instead of openFile() to test data copying on import */

    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "camera.gltf"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*importer, *memory));
    CORRADE_COMPARE(importer->cameraCount(), 4);

    Containers::Optional<Trade::CameraData> cam = importer->camera(0);
    CORRADE_VERIFY(cam);
    CORRADE_COMPARE(cam->type(), CameraType::Orthographic3D);
    CORRADE_COMPARE(cam->size(), (Vector2{4.0f, 3.0f}));
    CORRADE_COMPARE(cam->aspectRatio(), 1.333333f);
    CORRADE_COMPARE(cam->near(), 0.01f);
    CORRADE_COMPARE(cam->far(), 100.0f);
}

void GltfImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "camera.gltf")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(GLTFIMPORTER_TEST_DIR, "camera.gltf")));

    /* Shouldn't crash, leak or anything */
}

void GltfImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
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

CORRADE_TEST_MAIN(Magnum::Trade::Test::GltfImporterTest)
