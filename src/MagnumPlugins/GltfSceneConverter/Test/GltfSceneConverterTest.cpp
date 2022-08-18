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
#include <Corrade/Containers/Optional.h>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/File.h>
#include <Corrade/TestSuite/Compare/FileToString.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/FormatStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/AbstractSceneConverter.h>
#include <Magnum/Trade/MeshData.h>

#include "configure.h"

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

    void requiredExtensionsAddedAlready();

    void toDataButExternalBuffer();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractSceneConverter> _converterManager{"nonexistent"};
    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _importerManager;
};

using namespace Containers::Literals;

const struct {
    const char* name;
    bool binary;
    Containers::StringView suffix;
} FileVariantData[] {
    {"text", false, ".gltf"},
    {"binary", true, ".glb"}
};

const struct {
    const char* name;
    bool binary, accessorNames;
    Containers::StringView dataName;
    Containers::StringView suffix;
} FileVariantWithNamesData[] {
    {"text", false, false, {}, ".gltf"},
    {"text, name", false, false, "This very cool piece of data", "-name.gltf"},
    {"text, accessor names", false, true, {}, "-accessor-names.gltf"},
    {"text, name, accessor names", false, true, "A mesh", "-name-accessor-names.gltf"},
    {"binary", true, false, {}, ".glb"}
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

const struct {
    const char* name;
    MeshAttribute attribute;
    VertexFormat format;
    const char* expected;
} AddMeshDuplicateAttributeData[]{
    {}
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

    addInstancedTests({&GltfSceneConverterTest::addMeshDuplicateAttribute},
        Containers::arraySize(AddMeshDuplicateAttributeData));

    addTests({&GltfSceneConverterTest::addMeshCustomAttributeResetName,
              &GltfSceneConverterTest::addMeshCustomAttributeNoName,
              &GltfSceneConverterTest::addMeshCustomObjectIdAttributeName,

              &GltfSceneConverterTest::addMeshMultiple});

    addInstancedTests({&GltfSceneConverterTest::addMeshInvalid},
        Containers::arraySize(AddMeshInvalidData));

    addTests({&GltfSceneConverterTest::requiredExtensionsAddedAlready,

              &GltfSceneConverterTest::toDataButExternalBuffer});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef GLTFSCENECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(GLTFSCENECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
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
    _importerManager.setPluginDirectory({});
    #endif

    /* By default don't write the generator name for smaller test files */
    CORRADE_INTERNAL_ASSERT_EXPRESSION(_converterManager.metadata("GltfSceneConverter"))->configuration().setValue("generator", "");

    /* Create the output directory if it doesn't exist yet */
    CORRADE_INTERNAL_ASSERT_OUTPUT(Utility::Path::make(GLTFSCENECONVERTER_TEST_OUTPUT_DIR));
}

void GltfSceneConverterTest::empty() {
    auto&& data = FileVariantData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->configuration().setValue("binary", data.binary);

    converter->beginData();
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

    converter->beginData();
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

    converter->beginData();
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
    converter->beginData();
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

    /* Strict should be the default */
    if(!data.strict)
        converter->configuration().setValue("strict", false);
    else
        CORRADE_VERIFY(converter->configuration().value<bool>("strict"));

    converter->beginData();
    /* Some tested attributes are custom */
    converter->setMeshAttributeName(meshAttributeCustom(31434), "_YOLO");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->add(data.mesh));
    CORRADE_COMPARE(out.str(), Utility::format("Trade::GltfSceneConverter::add(): {}\n", data.message));
}

void GltfSceneConverterTest::requiredExtensionsAddedAlready() {
    const char vertices[4]{};
    MeshData mesh{MeshPrimitive::LineLoop, {}, vertices, {
        MeshAttributeData{MeshAttribute::TextureCoordinates, VertexFormat::Vector2b, 0, 1, 4}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "metadata-explicit-implicit-extensions.gltf");

    converter->configuration().addValue("extensionUsed", "KHR_mesh_quantization");
    converter->configuration().addValue("extensionUsed", "MAGNUM_is_amazing");
    converter->configuration().addValue("extensionRequired", "MAGNUM_can_write_json");
    converter->configuration().addValue("extensionRequired", "KHR_mesh_quantization");

    converter->beginFile(filename);
    /* This should not add KHR_mesh_quantization again to the file */
    CORRADE_VERIFY(converter->add(mesh));
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

    converter->beginData();
    CORRADE_VERIFY(converter->add(mesh));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->endData());
    CORRADE_COMPARE(out.str(), "Trade::GltfSceneConverter::endData(): can only write a glTF with external buffers if converting to a file\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::GltfSceneConverterTest)
