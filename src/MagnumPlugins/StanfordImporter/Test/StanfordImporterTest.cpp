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
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/FormatStl.h>
#include <Corrade/Utility/String.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct StanfordImporterTest: TestSuite::Tester {
    explicit StanfordImporterTest();

    void invalid();
    void fileEmpty();
    void fileTooShort();

    void parse();
    void empty();

    void triangleFastPath();

    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

constexpr struct {
    const char* filename;
    const char* message;
} InvalidData[]{
    {"invalid-signature", "invalid file signature bla"},

    {"format-invalid", "invalid format line format binary_big_endian 1.0 extradata"},
    {"format-unsupported", "unsupported file format ascii 1.0"},
    {"format-missing", "missing format line"},
    {"format-too-late", "expected format line, got element face 1"},

    {"unknown-line", "unknown line heh"},
    {"unknown-element", "unknown element edge"},

    {"unexpected-property", "unexpected property line"},
    {"invalid-vertex-property", "invalid vertex property line property float x extradata"},
    {"invalid-vertex-type", "invalid vertex component type float16"},
    {"invalid-face-property", "invalid face property line property float x extradata"},
    {"invalid-face-type", "invalid face component type float16"},
    {"invalid-face-size-type", "invalid face size type float"},
    {"invalid-face-index-type", "invalid face index type float"},

    {"incomplete-vertex-specification", "incomplete vertex specification"},
    {"incomplete-face-specification", "incomplete face specification"},

    {"positions-not-same-type", "expecting all position coordinates to have the same type but got Array(VertexFormat::UnsignedShort, VertexFormat::UnsignedByte, VertexFormat::UnsignedShort)"},
    {"positions-not-tightly-packed", "expecting position coordinates to be tightly packed, but got offsets Vector(0, 4, 2) for a 2-byte type"},
    {"positions-unsupported-type", "unsupported position component type VertexFormat::Double"},

    {"colors-not-same-type", "expecting all color channels to have the same type but got Array(VertexFormat::UnsignedByte, VertexFormat::Float, VertexFormat::UnsignedByte)"},
    {"colors-not-tightly-packed", "expecting color channels to be tightly packed, but got offsets Vector(12, 14, 13) for a 1-byte type"},
    {"colors-unsupported-type", "unsupported color channel type VertexFormat::Int"},

    {"unsupported-face-size", "unsupported face size 5"}
};

constexpr struct {
    std::size_t prefix;
    const char* message;
} ShortFileData[]{
    {0x103, "incomplete vertex data"},
    {0x107, "incomplete index data"},
    {0x117, "incomplete face data"}
};

constexpr struct {
    const char* filename;
    MeshIndexType indexType;
    VertexFormat positionFormat, colorFormat;
} ParseData[]{
    /* GCC 4.8 doesn't like just {}, needs VertexFormat{} */
    {"positions-float-indices-uint", MeshIndexType::UnsignedInt,
        VertexFormat::Vector3, VertexFormat{}},
    {"positions-colors-float-indices-int", MeshIndexType::UnsignedInt,
        VertexFormat::Vector3, VertexFormat::Vector3},
    /* Testing endian flip */
    {"positions-colors-float-indices-int-be", MeshIndexType::UnsignedInt,
        VertexFormat::Vector3, VertexFormat::Vector3},
    /* Testing endian flip of unaligned data */
    {"positions-colors-float-indices-int-be-unaligned", MeshIndexType::UnsignedInt,
        VertexFormat::Vector3, VertexFormat::Vector3},
    /* Testing various packing variants (hopefully exhausting all combinations) */
    {"positions-uchar-indices-ushort", MeshIndexType::UnsignedShort,
        VertexFormat::Vector3ub, VertexFormat{}},
    {"positions-char-colors-ushort-indices-short-be", MeshIndexType::UnsignedShort,
        VertexFormat::Vector3b, VertexFormat::Vector3usNormalized},
    {"positions-ushort-indices-uchar-be", MeshIndexType::UnsignedByte,
        VertexFormat::Vector3us, VertexFormat{}},
    {"positions-short-colors-uchar-indices-char", MeshIndexType::UnsignedByte,
        VertexFormat::Vector3s, VertexFormat::Vector3ubNormalized},
    /* CR/LF instead of LF */
    {"crlf", MeshIndexType::UnsignedByte, VertexFormat::Vector3us, VertexFormat{}},
    /* Ignoring extra components */
    {"ignored-face-components", MeshIndexType::UnsignedByte, VertexFormat::Vector3b, VertexFormat{}},
    {"ignored-vertex-components", MeshIndexType::UnsignedByte, VertexFormat::Vector3us, VertexFormat{}}
};

constexpr struct {
    const char* name;
    bool enabled;
} FastTrianglePathData[]{
    {"", true},
    {"disabled", false}
};

StanfordImporterTest::StanfordImporterTest() {
    addInstancedTests({&StanfordImporterTest::invalid},
        Containers::arraySize(InvalidData));

    addTests({&StanfordImporterTest::fileEmpty});

    addInstancedTests({&StanfordImporterTest::fileTooShort},
        Containers::arraySize(ShortFileData));

    addInstancedTests({&StanfordImporterTest::parse},
        Containers::arraySize(ParseData));

    addTests({&StanfordImporterTest::empty});

    addInstancedTests({&StanfordImporterTest::triangleFastPath},
        Containers::arraySize(FastTrianglePathData));

    addTests({&StanfordImporterTest::openTwice,
              &StanfordImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STANFORDIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(STANFORDIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void StanfordImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(Utility::String::replaceAll(data.filename, "-", " "));

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, Utility::formatString("{}.ply", data.filename))));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::StanfordImporter::mesh(): {}\n", data.message));
}

void StanfordImporterTest::fileEmpty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(nullptr));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::StanfordImporter::openData(): the file is empty\n"));
}

void StanfordImporterTest::fileTooShort() {
    auto&& data = ShortFileData[testCaseInstanceId()];
    setTestCaseDescription(data.message);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    Containers::Array<char> file = Utility::Directory::read(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "positions-float-indices-uint.ply"));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openData(file.prefix(data.prefix)));
    CORRADE_VERIFY(!importer->mesh(0));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::StanfordImporter::mesh(): {}\n", data.message));
}

/*
    First face is quad, second is triangle.

    0--3--4
    |\ | /
    | \|/
    1--2
*/
constexpr UnsignedInt Indices[]{0, 1, 2, 0, 2, 3, 3, 2, 4};
constexpr Vector3 Positions[]{
    {1.0f, 3.0f, 2.0f},
    {1.0f, 1.0f, 2.0f},
    {3.0f, 3.0f, 2.0f},
    {3.0f, 1.0f, 2.0f},
    {5.0f, 3.0f, 9.0f}
};
constexpr Color3 Colors[]{
    {0.8f, 0.2f, 0.4f},
    {0.6f, 0.666667f, 1.0f},
    {0.0f, 0.0666667f, 0.9333333f},
    {0.733333f, 0.8666666f, 0.133333f},
    {0.266667f, 0.3333333f, 0.466666f}
};

void StanfordImporterTest::parse() {
    auto&& data = ParseData[testCaseInstanceId()];
    setTestCaseDescription(Utility::String::replaceAll(data.filename, "-", " "));

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, Utility::formatString("{}.ply", data.filename))));

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), data.indexType);
    CORRADE_COMPARE_AS(mesh->indicesAsArray(),
        Containers::arrayView(Indices),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), data.positionFormat);
    CORRADE_COMPARE_AS(mesh->positions3DAsArray(),
        Containers::arrayView(Positions),
        TestSuite::Compare::Container);

    if(data.colorFormat != VertexFormat{}) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Color));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Color), data.colorFormat);
        CORRADE_COMPARE_AS(Containers::arrayCast<Color3>(Containers::stridedArrayView(mesh->colorsAsArray())),
            Containers::stridedArrayView(Colors),
            TestSuite::Compare::Container);
    }
}

void StanfordImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "empty.ply")));

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    /* Metadata parsed, but the actual count is zero */
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE(mesh->indexCount(), 0);

    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position),
        VertexFormat::Vector3);
    CORRADE_COMPARE(mesh->vertexCount(), 0);
}

void StanfordImporterTest::triangleFastPath() {
    auto&& data = FastTrianglePathData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");
    importer->configuration().setValue("triangleFastPath", data.enabled);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "triangle-fast-path-be.ply")));

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedShort);
    /* The file is BE to verify the endian flip is done here as well */
    CORRADE_COMPARE_AS(mesh->indices<UnsignedShort>(),
        Containers::arrayView<UnsignedShort>({
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position),
        VertexFormat::Vector3b);
    CORRADE_COMPARE(mesh->vertexCount(), 0);
}

void StanfordImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "positions-float-indices-uint.ply")));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "positions-float-indices-uint.ply")));

    /* Shouldn't crash, leak or anything */
}

void StanfordImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "positions-float-indices-uint.ply")));

    /* Verify that everything is working the same way on second use */
    {
        Containers::Optional<MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
            Containers::arrayView(Positions),
            TestSuite::Compare::Container);
    } {
        Containers::Optional<MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
            Containers::arrayView(Positions),
            TestSuite::Compare::Container);
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StanfordImporterTest)
