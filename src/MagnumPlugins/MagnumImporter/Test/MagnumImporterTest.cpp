/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct MagnumImporterTest: TestSuite::Tester {
    explicit MagnumImporterTest();

    template<std::size_t i> void openInvalid();
    void openIgnoredChunk();
    void openTooLargeFor32bit();

    void mesh();
    void meshEndianSwapUnsignedIntIndices();
    void meshEndianSwapUnsignedByteIndices();
    template<std::size_t i> void meshInvalid();
    void meshEndianSwapImplementationSpecificFormat();

    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

const struct {
    const char* name;
    const char* suffix;
    bool indexed;
} OpenData[] {
    {"32-bit LE", "le32", true},
    {"32-bit LE, non-indexed", "le32", false},
    {"64-bit LE", "le64", true},
    {"64-bit LE, non-indexed", "le64", false},
    {"32-bit BE", "be32", true},
    {"32-bit BE, non-indexed", "be32", false},
    {"64-bit BE", "be64", true},
    {"64-bit BE, non-indexed", "be64", false}
};

constexpr char DataLittle32[]{
    '\x80', '\x0a', '\x0d', '\x0a', 'B', 'l', 'O', 'B', 0, 0,
    42, 0, 'M', 'e', 's', 'h', 20 + 5, 0, 0, 0,

    'h', 'e', 'l', 'l', 'o'
};
constexpr char DataBig32[]{
    '\x80', '\x0a', '\x0d', '\x0a', 'B', 'O', 'l', 'B', 0, 0,
    0, 42, 'M', 'e', 's', 'h', 0, 0, 0, 20 + 5,

    'h', 'e', 'l', 'l', 'o'
};
constexpr char DataLittle64[]{
    '\x80', '\x0a', '\x0d', '\x0a', 'B', 'L', 'O', 'B', 0, 0,
    42, 0, 'M', 'e', 's', 'h', 24 + 5, 0, 0, 0, 0, 0, 0, 0,

    'h', 'e', 'l', 'l', 'o'
};
constexpr char DataBig64[]{
    '\x80', '\x0a', '\x0d', '\x0a', 'B', 'O', 'L', 'B', 0, 0,
    0, 42, 'M', 'e', 's', 'h', 0, 0, 0, 0, 0, 0, 0, 24 + 5,

    'h', 'e', 'l', 'l', 'o'
};

const struct {
    const char* name;
    /* 32, then 64 */
    std::size_t size[2];
    std::size_t offset;
    Containers::Array<char> replace;
    const char* message[2];
} OpenInvalidData[] {
    {"too short header",
        {19, 23}, 0, {},
        {"file too short, expected at least 20 bytes for a header but got 19",
         "64-bit file too short, expected at least 24 bytes for a header but got 23"}},
    {"too short chunk",
        {24, 28}, 0, {},
        {"file too short, expected at least 25 bytes but got 24",
         "file too short, expected at least 29 bytes but got 28"}},
    {"wrong version",
        {}, 0, Containers::array<char>({'\x7f'}),
        {"expected version 128 but got 127",
         "expected version 128 but got 127"}},
    {"invalid signature",
        {}, 4, Containers::array<char>({'B', 'L', 'A', 'B'}),
        {"invalid signature Trade::DataChunkSignature('B', 'L', 'A', 'B')",
         "invalid signature Trade::DataChunkSignature('B', 'L', 'A', 'B')"}},
    {"invalid,  check bytes",
        {}, 8, Containers::array<char>({1, 0}),
        {"invalid header check bytes",
         "invalid header check bytes"}}
};

const struct {
    const char* name;
    /* 32, then 64 */
    std::size_t size[2];
    std::size_t offset[2];
    /* little 32, little 64, big 32, big 64 */
    Containers::Array<char> replace[4];
    const char* message[2];
} MeshInvalidData[] {
    {"chunk too short to contain a meshdata header",
        {}, {16, 16}, /* not cutting the file, only adapting header */
        {Containers::array<char>({0x2f, 0, 0, 0}),
         Containers::array<char>({0x3f, 0, 0, 0, 0, 0, 0, 0}),
         Containers::array<char>({0, 0, 0, 0x2f}),
         Containers::array<char>({0, 0, 0, 0, 0, 0, 0, 0x3f})},
        {"expected at least a 48-byte chunk for a header but got 47",
         "expected at least a 64-byte chunk for a header but got 63"}},
    {"chunk too short to contain all data",
        {}, {16, 16}, /* not cutting the file, only adapting header */
        {Containers::array<char>({'\xd3', 0, 0, 0}),
         Containers::array<char>({'\xf3', 0, 0, 0, 0, 0, 0, 0}),
         Containers::array<char>({0, 0, 0, '\xd3'}),
         Containers::array<char>({0, 0, 0, 0, 0, 0, 0, '\xf3'})},
        {"expected a 212-byte chunk but got 211",
         "expected a 244-byte chunk but got 243"}},
    {"invalid type version",
        {}, {10, 10},
        {Containers::array<char>({1, 0}),
         Containers::array<char>({1, 0}),
         Containers::array<char>({0, 1}),
         Containers::array<char>({0, 1})},
        {"invalid chunk type version, expected 0 but got 1",
         "invalid chunk type version, expected 0 but got 1"}},
    {"index array out of bounds",
        {}, {36, 40},
        {Containers::array<char>({5, 0, 0, 0}),
         Containers::array<char>({5, 0, 0, 0, 0, 0, 0, 0}),
         Containers::array<char>({0, 0, 0, 5}),
         Containers::array<char>({0, 0, 0, 0, 0, 0, 0, 5})},
        {"indices [5:13] out of range for 12 bytes of index data",
         "indices [5:13] out of range for 12 bytes of index data"}},
    {"attribute out of bounds",
        {}, {48 + 20 + 16, 64 + 24 + 16},
        {Containers::array<char>({23, 0, 0, 0}),
         Containers::array<char>({23, 0, 0, 0, 0, 0, 0, 0}),
         Containers::array<char>({0, 0, 0, 23}),
         Containers::array<char>({0, 0, 0, 0, 0, 0, 0, 23})},
        {"attribute 1 [23:73] out of range for 72 bytes of vertex data",
         "attribute 1 [23:73] out of range for 72 bytes of vertex data"}},
};

MagnumImporterTest::MagnumImporterTest() {
    addInstancedTests<MagnumImporterTest>({
        &MagnumImporterTest::openInvalid<0>,
        &MagnumImporterTest::openInvalid<1>,
        &MagnumImporterTest::openInvalid<2>,
        &MagnumImporterTest::openInvalid<3>
    }, Containers::arraySize(OpenInvalidData));

    addTests({&MagnumImporterTest::openTooLargeFor32bit,
              &MagnumImporterTest::openIgnoredChunk});

    addInstancedTests({&MagnumImporterTest::mesh},
        Containers::arraySize(OpenData));

    addTests({&MagnumImporterTest::meshEndianSwapUnsignedIntIndices,
              &MagnumImporterTest::meshEndianSwapUnsignedByteIndices});

    addInstancedTests<MagnumImporterTest>({
        &MagnumImporterTest::meshInvalid<0>,
        &MagnumImporterTest::meshInvalid<1>,
        &MagnumImporterTest::meshInvalid<2>,
        &MagnumImporterTest::meshInvalid<3>
    }, Containers::arraySize(MeshInvalidData));

    addTests({&MagnumImporterTest::meshEndianSwapImplementationSpecificFormat,

              &MagnumImporterTest::openTwice,
              &MagnumImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef MAGNUMIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(MAGNUMIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

const struct {
    const char* name;
    Containers::ArrayView<const char> data;
    const char* suffix;
} Names[] {
    {"Little32", Containers::arrayView(DataLittle32), "le32"},
    {"Little64", Containers::arrayView(DataLittle64), "le64"},
    {"Big32", Containers::arrayView(DataBig32), "be32"},
    {"Big64", Containers::arrayView(DataBig64), "be64"}
};

template<std::size_t i> void MagnumImporterTest::openInvalid() {
    auto&& data = OpenInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);
    setTestCaseTemplateName(Names[i].name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("MagnumImporter");

    Containers::Array<char> blob{Containers::NoInit, Names[i].data.size()};
    Utility::copy(Names[i].data, blob);

    Containers::ArrayView<char> view = blob;
    if(data.size[i%2]) view = view.prefix(data.size[i%2]);
    if(data.replace) Utility::copy(data.replace, view.slice(data.offset, data.offset + data.replace.size()));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(view));
    CORRADE_COMPARE(out.str(),
        Utility::formatString("Trade::MagnumImporter::openData(): {}\n", data.message[i%2]));
}

void MagnumImporterTest::openIgnoredChunk() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("MagnumImporter");

    constexpr char data[]{
        '\x80', '\x0a', '\x0d', '\x0a', 'B', 'l', 'O', 'B', 0, 0,
        42, 0, 'W', 'a', 'v', 'e', 20 + 5, 0, 0, 0,

        'h', 'e', 'l', 'l', 'o'
    };

    std::ostringstream out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(importer->openData(data));
    CORRADE_COMPARE(importer->meshCount(), 0);
    CORRADE_COMPARE(out.str(), "Trade::MagnumImporter::openData(): ignoring unknown chunk Trade::DataChunkType('W', 'a', 'v', 'e')\n");
}

void MagnumImporterTest::openTooLargeFor32bit() {
    if(sizeof(void*) == 8)
        CORRADE_SKIP("Can't test on a 64-bit platform.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("MagnumImporter");

    constexpr char data[]{
        '\x80', '\x0a', '\x0d', '\x0a', 'B', 'L', 'O', 'B', 0, 0,
        42, 0, 'W', 'a', 'v', 'e', 0, 0, 0, 0, 1, 0, 0, 0,

        'h', 'e', 'l', 'l', 'o'
    };

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(data));
    CORRADE_COMPARE(out.str(), "Trade::MagnumImporter::openData(): chunk size 4294967296 too large to process on a 32-bit platform\n");
}

void MagnumImporterTest::mesh() {
    auto&& data = OpenData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("MagnumImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR,
        Utility::formatString("mesh-{}{}.blob",
            data.indexed ? "" : "nonindexed-",
            data.suffix))));

    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<MeshData> meshData = importer->mesh(0);
    CORRADE_VERIFY(meshData);
    CORRADE_COMPARE(meshData->attributeCount(), 4);
    CORRADE_COMPARE(meshData->vertexCount(), 3);
    /* The importer produces a full copy, owned */
    CORRADE_COMPARE(meshData->indexDataFlags(), DataFlag::Owned|DataFlag::Mutable);
    CORRADE_COMPARE(meshData->vertexDataFlags(), DataFlag::Owned|DataFlag::Mutable);

    CORRADE_COMPARE(meshData->attributeName(0), MeshAttribute::Position);
    CORRADE_COMPARE(meshData->attributeFormat(0), VertexFormat::Vector2);
    CORRADE_COMPARE(meshData->attributeOffset(0), 0);
    CORRADE_COMPARE(meshData->attributeStride(0), 24);
    CORRADE_COMPARE(meshData->attributeArraySize(0), 0);
    CORRADE_COMPARE_AS(meshData->attribute<Vector2>(0),
        Containers::arrayView<Vector2>({
            {1.0f, 0.5f}, {2.0f, 1.5f}, {3.0f, 2.5f}
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE(meshData->attributeName(1), MeshAttribute::TextureCoordinates);
    CORRADE_COMPARE(meshData->attributeFormat(1), VertexFormat::Vector2ub);
    CORRADE_COMPARE(meshData->attributeOffset(1), 8);
    CORRADE_COMPARE(meshData->attributeStride(1), 24);
    CORRADE_COMPARE(meshData->attributeArraySize(1), 0);
    CORRADE_COMPARE_AS(meshData->attribute<Vector2ub>(1),
        Containers::arrayView<Vector2ub>({
            {23, 15}, {232, 144}, {17, 242}
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE(meshData->attributeName(2), meshAttributeCustom(23));
    CORRADE_COMPARE(meshData->attributeFormat(2), VertexFormat::UnsignedShort);
    CORRADE_COMPARE(meshData->attributeOffset(2), 10);
    CORRADE_COMPARE(meshData->attributeStride(2), 24);
    CORRADE_COMPARE(meshData->attributeArraySize(2), 2);
    CORRADE_COMPARE_AS((meshData->attribute<UnsignedShort[]>(2).transposed<0, 1>()[0]),
        Containers::arrayView<UnsignedShort>({3247, 6243, 15}), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS((meshData->attribute<UnsignedShort[]>(2).transposed<0, 1>()[1]),
        Containers::arrayView<UnsignedShort>({1256, 1241, 2323}), TestSuite::Compare::Container);

    CORRADE_COMPARE(meshData->attributeName(3), meshAttributeCustom(14));
    CORRADE_COMPARE(meshData->attributeFormat(3), VertexFormat::Double);
    CORRADE_COMPARE(meshData->attributeOffset(3), 16);
    CORRADE_COMPARE(meshData->attributeStride(3), 24);
    CORRADE_COMPARE(meshData->attributeArraySize(3), 0);
    CORRADE_COMPARE_AS(meshData->attribute<Double>(3),
        Containers::arrayView<Double>({
            1.1, 1.2, 1.3
        }), TestSuite::Compare::Container);

    if(data.indexed) {
        CORRADE_VERIFY(meshData->isIndexed());
        CORRADE_COMPARE(meshData->indexCount(), 4);
        CORRADE_COMPARE(meshData->indexType(), MeshIndexType::UnsignedShort);
        CORRADE_COMPARE(meshData->indexOffset(), 4);
        CORRADE_COMPARE_AS(meshData->indices<UnsignedShort>(),
            Containers::arrayView<UnsignedShort>({1, 0, 1, 0}),
            TestSuite::Compare::Container);
    } else CORRADE_VERIFY(!meshData->isIndexed());
}

void MagnumImporterTest::meshEndianSwapUnsignedIntIndices() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("MagnumImporter");
    #ifndef CORRADE_TARGET_BIG_ENDIAN
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR, "mesh-uint-indices-be32.blob")));
    #else
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR, "mesh-uint-indices-le32.blob")));
    #endif

    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<MeshData> meshData = importer->mesh(0);
    CORRADE_VERIFY(meshData);
    CORRADE_VERIFY(meshData->isIndexed());
    CORRADE_COMPARE(meshData->indexCount(), 2);
    CORRADE_COMPARE(meshData->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE_AS(meshData->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({256415, 213247}),
        TestSuite::Compare::Container);
}

void MagnumImporterTest::meshEndianSwapUnsignedByteIndices() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("MagnumImporter");
    #ifndef CORRADE_TARGET_BIG_ENDIAN
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR, "mesh-ubyte-indices-be32.blob")));
    #else
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR, "mesh-ubyte-indices-le32.blob")));
    #endif

    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<MeshData> meshData = importer->mesh(0);
    CORRADE_VERIFY(meshData);
    CORRADE_VERIFY(meshData->isIndexed());
    CORRADE_COMPARE(meshData->indexCount(), 2);
    CORRADE_COMPARE(meshData->indexType(), MeshIndexType::UnsignedByte);
    CORRADE_COMPARE_AS(meshData->indices<UnsignedByte>(),
        Containers::arrayView<UnsignedByte>({254, 213}),
        TestSuite::Compare::Container);
}

template<std::size_t i> void MagnumImporterTest::meshInvalid() {
    auto&& data = MeshInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);
    setTestCaseTemplateName(Names[i].name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("MagnumImporter");

    Containers::Array<char> blob = Utility::Directory::read(Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR,
        Utility::formatString("mesh-{}.blob", Names[i].suffix)));
    CORRADE_VERIFY(blob);

    Containers::ArrayView<char> view = blob;
    if(data.size[i%2]) view = view.prefix(data.size[i%2]);
    if(data.replace[i]) Utility::copy(data.replace[i], view.slice(data.offset[i%2], data.offset[i%2] + data.replace[i].size()));

    CORRADE_VERIFY(importer->openData(view));
    CORRADE_COMPARE(importer->meshCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(),
        Utility::formatString("Trade::MagnumImporter::mesh(): {}\n", data.message[i%2]));
}

void MagnumImporterTest::meshEndianSwapImplementationSpecificFormat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("MagnumImporter");

    /* Take a file of the opposite endianness and patch it to contain an
       implementation-specific vertex format */
    Containers::Array<char> blob = Utility::Directory::read(Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR,
        #ifndef CORRADE_TARGET_BIG_ENDIAN
        "mesh-be32.blob"
        #else
        "mesh-le32.blob"
        #endif
    ));
    CORRADE_VERIFY(blob);
    Utility::copy(
        #ifndef CORRADE_TARGET_BIG_ENDIAN
        Containers::array<char>({'\x80', 0, 0, 1}),
        #else
        Containers::array<char>({1, 0, 0, '\x80'}),
        #endif
        blob.slice(48 + 2*20, 48 + 2*20 + 4));

    CORRADE_VERIFY(importer->openData(blob));
    CORRADE_COMPARE(importer->meshCount(), 1);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(),
        "Trade::MagnumImporter::mesh(): cannot perform endian swap on VertexFormat::ImplementationSpecific(0x1)\n");
}

void MagnumImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("MagnumImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR, "mesh-le32.blob")));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR, "mesh-le32.blob")));

    /* Shouldn't crash, leak or anything */
}

void MagnumImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("MagnumImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR, "mesh-le32.blob")));

    /* Verify that everything is working the same way on second use */
    {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->vertexCount(), 3);
    } {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->vertexCount(), 3);
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::MagnumImporterTest)
