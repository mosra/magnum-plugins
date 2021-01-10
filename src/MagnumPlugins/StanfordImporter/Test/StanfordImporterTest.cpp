/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/TestSuite/Compare/Numeric.h>
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
    void fileNotFound();
    void fileEmpty();
    void fileTooShort();

    void parse();
    void parsePerFace();
    void parsePerFaceToPerVertex();
    void empty();

    void customAttributes();
    void customAttributesPerFaceToPerVertex();
    void customAttributesDuplicate();
    void customAttributesNoFileOpened();

    void triangleFastPath();
    void triangleFastPathPerFaceToPerVertex();

    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

constexpr struct {
    const char* filename;
    const char* message;
    bool duringOpen;
} InvalidData[]{
    {"invalid-signature", "invalid file signature bla", true},

    {"format-invalid", "invalid format line format binary_big_endian 1.0 extradata", true},
    {"format-unsupported", "unsupported file format ascii 1.0", true},
    {"format-missing", "missing format line", true},
    {"format-too-late", "expected format line, got element face 1", true},

    {"unknown-line", "unknown line heh", true},
    {"unknown-element", "unknown element edge", true},

    {"unexpected-property", "unexpected property line", true},
    {"invalid-vertex-property", "invalid vertex property line property float x extradata", true},
    {"invalid-vertex-type", "invalid vertex component type float16", true},
    {"invalid-face-property", "invalid face property line property float x extradata", true},
    {"invalid-face-type", "invalid face component type float16", true},
    {"invalid-face-size-type", "invalid face size type float", true},
    {"invalid-face-index-type", "invalid face index type float", true},

    {"incomplete-vertex-specification", "incomplete vertex specification", true},
    {"incomplete-face-specification", "incomplete face specification", true},

    {"positions-not-same-type", "expecting all position components to have the same type but got Vector(VertexFormat::UnsignedShort, VertexFormat::UnsignedByte, VertexFormat::UnsignedShort)", true},
    {"positions-not-tightly-packed", "expecting position components to be tightly packed, but got offsets Vector(0, 4, 2) for a 2-byte type", true},
    {"positions-unsupported-type", "unsupported position component type VertexFormat::Double", true},

    {"normals-not-same-type", "expecting all normal components to have the same type but got Vector(VertexFormat::Short, VertexFormat::Float, VertexFormat::Short)", true},
    {"normals-not-tightly-packed", "expecting normal components to be tightly packed, but got offsets Vector(14, 13, 12) for a 1-byte type", true},
    {"normals-unsupported-type", "unsupported normal component type VertexFormat::UnsignedByte", true},

    {"texcoords-not-same-type", "expecting all texture coordinate components to have the same type but got Vector(VertexFormat::UnsignedShort, VertexFormat::Float)", true},
    {"texcoords-not-tightly-packed", "expecting texture coordinate components to be tightly packed, but got offsets Vector(13, 0) for a 1-byte type", true},
    {"texcoords-unsupported-type", "unsupported texture coordinate component type VertexFormat::Short", true},

    {"colors-not-same-type", "expecting all color components to have the same type but got Vector(VertexFormat::UnsignedByte, VertexFormat::Float, VertexFormat::UnsignedByte)", true},
    {"colors4-not-same-type", "expecting all color components to have the same type but got Vector(VertexFormat::UnsignedByte, VertexFormat::UnsignedByte, VertexFormat::UnsignedByte, VertexFormat::Float)", true},
    {"colors-not-tightly-packed", "expecting color components to be tightly packed, but got offsets Vector(12, 14, 13) for a 1-byte type", true},
    {"colors4-not-tightly-packed", "expecting color components to be tightly packed, but got offsets Vector(13, 14, 15, 12) for a 1-byte type", true},
    {"colors-unsupported-type", "unsupported color component type VertexFormat::Int", true},

    {"objectid-unsupported-type", "unsupported object ID type VertexFormat::Float", true},

    {"unsupported-face-size", "unsupported face size 5", false}
};

constexpr struct {
    std::size_t prefix;
    const char* message;
    bool duringOpen;
} ShortFileData[]{
    {0x103, "incomplete vertex data", true},
    {0x107, "incomplete index data", false},
    {0x117, "incomplete face data", false}
};

constexpr struct {
    const char* filename;
    MeshIndexType indexType;
    VertexFormat positionFormat, colorFormat, normalFormat, texcoordFormat,
        objectIdFormat;
    const char* objectIdAttribute;
    UnsignedInt attributeCount, faceAttributeCount;
} ParseData[]{
    /* GCC 4.8 doesn't like just {}, needs VertexFormat{} */
    {"positions-float-indices-uint", MeshIndexType::UnsignedInt,
        VertexFormat::Vector3, VertexFormat{},
        VertexFormat{}, VertexFormat{},
        VertexFormat{}, nullptr, 1, 0},
    /* All supported attributes, in the canonical type */
    {"positions-colors-normals-texcoords-float-objectid-uint-indices-int",
        MeshIndexType::UnsignedInt,
        VertexFormat::Vector3, VertexFormat::Vector3,
        VertexFormat::Vector3, VertexFormat::Vector2,
        VertexFormat::UnsignedInt, nullptr, 5, 0},
    /* Four-component colors */
    {"positions-colors4-float-indices-int", MeshIndexType::UnsignedInt,
        VertexFormat::Vector3, VertexFormat::Vector4,
        VertexFormat{}, VertexFormat{},
        VertexFormat{}, nullptr, 2, 0},
    /* Testing endian flip */
    {"positions-colors-normals-texcoords-float-objectid-uint-indices-int-be",
        MeshIndexType::UnsignedInt,
        VertexFormat::Vector3, VertexFormat::Vector3,
        VertexFormat::Vector3, VertexFormat::Vector2,
        VertexFormat::UnsignedInt, nullptr, 5, 0},
    /* Testing endian flip of unaligned data */
    {"positions-colors4-normals-texcoords-float-indices-int-be-unaligned",
        MeshIndexType::UnsignedInt,
        VertexFormat::Vector3, VertexFormat::Vector4,
        VertexFormat::Vector3, VertexFormat::Vector2,
        VertexFormat{}, nullptr, 8, 1},
    /* Testing various packing variants (hopefully exhausting all combinations) */
    {"positions-uchar-normals-char-objectid-short-indices-ushort",
        MeshIndexType::UnsignedShort,
        VertexFormat::Vector3ub, VertexFormat{},
        VertexFormat::Vector3bNormalized, VertexFormat{},
        VertexFormat::UnsignedShort, "OBJECTID", 3, 0},
    {"positions-char-colors4-ushort-texcoords-uchar-indices-short-be",
        MeshIndexType::UnsignedShort,
        VertexFormat::Vector3b, VertexFormat::Vector4usNormalized,
        VertexFormat{}, VertexFormat::Vector2ubNormalized,
        VertexFormat{}, nullptr, 3, 0},
    {"positions-ushort-normals-short-objectid-uchar-indices-uchar-be",
        MeshIndexType::UnsignedByte,
        VertexFormat::Vector3us, VertexFormat{},
        VertexFormat::Vector3sNormalized, VertexFormat{},
        VertexFormat::UnsignedByte, "semantic", 3, 0},
    {"positions-short-colors-uchar-texcoords-ushort-indices-char",
        MeshIndexType::UnsignedByte,
        VertexFormat::Vector3s, VertexFormat::Vector3ubNormalized,
        VertexFormat{}, VertexFormat::Vector2usNormalized,
        VertexFormat{}, nullptr, 3, 0},
    /* CR/LF instead of LF */
    {"crlf", MeshIndexType::UnsignedByte,
        VertexFormat::Vector3us, VertexFormat{},
        VertexFormat{}, VertexFormat{},
        VertexFormat{}, nullptr, 1, 0}
};

constexpr struct {
    const char* name;
    const char* filename;
    UnsignedInt faceAttributeCount;
    MeshIndexType indexType;
    VertexFormat colorFormat, normalFormat, objectIdFormat;
    ImporterFlags flags;
    const char* message;
} ParsePerFaceData[]{
    {"per-face nothing", "positions-float-indices-uint.ply", 0,
        MeshIndexType::UnsignedInt,
        VertexFormat{}, VertexFormat{}, VertexFormat{},
        {}, ""},
    {"per-face nothing, verbose", "positions-float-indices-uint.ply", 0,
        MeshIndexType::UnsignedInt,
        VertexFormat{}, VertexFormat{}, VertexFormat{},
        ImporterFlag::Verbose, ""},
    {"per-face colors", "per-face-colors-be.ply", 1,
        MeshIndexType::UnsignedShort,
        VertexFormat::Vector4, VertexFormat{}, VertexFormat{},
        {}, ""},
    {"per-face normals, object ids", "per-face-normals-objectid.ply", 2,
        MeshIndexType::UnsignedByte,
        VertexFormat{}, VertexFormat::Vector3, VertexFormat::UnsignedShort,
        {}, ""},
    {"per-face normals, object ids, verbose", "per-face-normals-objectid.ply", 2,
        MeshIndexType::UnsignedByte,
        VertexFormat{}, VertexFormat::Vector3, VertexFormat::UnsignedShort,
        ImporterFlag::Verbose, "Trade::StanfordImporter::mesh(): converting 2 per-face attributes to per-vertex\n"}
};

constexpr struct {
    const char* name;
    bool perFaceToPerVertex;
} EmptyData[]{
    {"", false},
    {"per-face to per-vertex", true}
};

constexpr struct {
    const char* filename;
} CustomAttributeData[]{
    {"custom-components"},
    {"custom-components-be"}
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

    addTests({&StanfordImporterTest::fileNotFound,
              &StanfordImporterTest::fileEmpty});

    addInstancedTests({&StanfordImporterTest::fileTooShort},
        Containers::arraySize(ShortFileData));

    addInstancedTests({&StanfordImporterTest::parse},
        Containers::arraySize(ParseData));

    addInstancedTests({&StanfordImporterTest::parsePerFace,
                       &StanfordImporterTest::parsePerFaceToPerVertex},
        Containers::arraySize(ParsePerFaceData));

    addInstancedTests({&StanfordImporterTest::empty},
        Containers::arraySize(EmptyData));

    addInstancedTests({&StanfordImporterTest::customAttributes,
                       &StanfordImporterTest::customAttributesPerFaceToPerVertex},
        Containers::arraySize(CustomAttributeData));

    addTests({&StanfordImporterTest::customAttributesDuplicate,
              &StanfordImporterTest::customAttributesNoFileOpened});

    addInstancedTests({&StanfordImporterTest::triangleFastPath,
                       &StanfordImporterTest::triangleFastPathPerFaceToPerVertex},
        Containers::arraySize(FastTrianglePathData));

    addTests({&StanfordImporterTest::openTwice,
              &StanfordImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STANFORDIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STANFORDIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void StanfordImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(Utility::String::replaceAll(data.filename, "-", " "));

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    const bool failed = !importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, Utility::formatString("{}.ply", data.filename)));
    CORRADE_VERIFY(failed == data.duringOpen);
    if(!failed) {
        CORRADE_VERIFY(!importer->mesh(0));
    }
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::StanfordImporter::{}(): {}\n",
        data.duringOpen ? "openData" : "mesh",
        data.message));
}

void StanfordImporterTest::fileNotFound() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile("nonexistent.ply"));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::StanfordImporter::openFile(): cannot open file nonexistent.ply\n"));
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
    const bool failed = !importer->openData(file.prefix(data.prefix));
    CORRADE_VERIFY(failed == data.duringOpen);
    if(!failed) {
        CORRADE_VERIFY(!importer->mesh(0));
    }
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::StanfordImporter::{}(): {}\n",
        data.duringOpen ? "openData" : "mesh",
        data.message));
}

/*
    First face is quad, second is triangle. For testing per-face data converted
    to per-vertex, the first two faces have the same attributes, the third has
    different, thus the data get split like this:

    0--3--4         0--3 4--6
    |\ | /          |\ | | /
    | \|/           | \| |/
    1--2            1--2 5
*/
constexpr UnsignedInt Indices[]{0, 1, 2, 0, 2, 3, 3, 2, 4};
constexpr UnsignedInt IndicesPerFaceToPerVertex[]{0, 1, 2, 0, 2, 3, 4, 5, 6};
constexpr Vector3 Positions[]{
    {1.0f, 3.0f, 2.0f},
    {1.0f, 1.0f, 2.0f},
    {3.0f, 3.0f, 2.0f},
    {3.0f, 1.0f, 2.0f},
    {5.0f, 3.0f, 9.0f}
};
constexpr Vector3 PositionsPerFaceToPerVertex[]{
    {1.0f, 3.0f, 2.0f},
    {1.0f, 1.0f, 2.0f},
    {3.0f, 3.0f, 2.0f},
    {3.0f, 1.0f, 2.0f},

    {3.0f, 1.0f, 2.0f},
    {3.0f, 3.0f, 2.0f},
    {5.0f, 3.0f, 9.0f}
};

constexpr Color3 Colors[]{
    {0.8f, 0.2f, 0.4f},
    {0.6f, 0.666667f, 1.0f},
    {0.0f, 0.0666667f, 0.9333333f},
    {0.733333f, 0.8666666f, 0.133333f},
    {0.266667f, 0.3333333f, 0.466667f}
};
constexpr Color4 Colors4[]{
    {0.8f, 0.2f, 0.4f, 0.266667f},
    {0.6f, 0.666667f, 1.0f, 0.8666667f},
    {0.0f, 0.0666667f, 0.9333333f, 0.466667f},
    {0.733333f, 0.8666667f, 0.133333f, 0.666667f},
    {0.266667f, 0.3333333f, 0.466666f, 0.0666667f}
};
constexpr Vector3 Normals[]{
    {-0.333333f, -0.6666667f, -0.933333f},
    {-0.0f, -0.133333f, -1.0f},
    {-0.6f, -0.8f, -0.2f},
    {-0.4f, -0.733333f, -0.933333f},
    {-0.133333f, -0.733333f, -0.4f}
};
constexpr Vector2 TextureCoordinates[]{
    {0.933333f, 0.333333f},
    {0.133333f, 0.933333f},
    {0.666667f, 0.266667f},
    {0.466666f, 0.333333f},
    {0.866666f, 0.066666f}
};
constexpr UnsignedInt ObjectIds[]{
    215, 71, 133, 5, 196
};

void StanfordImporterTest::parse() {
    auto&& data = ParseData[testCaseInstanceId()];
    setTestCaseDescription(Utility::String::replaceAll(data.filename, "-", " "));

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    /* Disable per-face to per-vertex conversion, that's tested thoroughly in
       parsePerFace() */
    importer->configuration().setValue("perFaceToPerVertex", false);
    if(data.objectIdAttribute)
        importer->configuration().setValue("objectIdAttribute", data.objectIdAttribute);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, Utility::formatString("{}.ply", data.filename))));
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->meshLevelCount(0), 2);

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(), data.attributeCount);

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

    /* Colors */
    if(data.colorFormat == VertexFormat{}) {
        CORRADE_VERIFY(!mesh->hasAttribute(MeshAttribute::Color));
    } else if(vertexFormatComponentCount(data.colorFormat) == 3) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Color));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Color), data.colorFormat);
        CORRADE_COMPARE_AS(Containers::arrayCast<Color3>(Containers::stridedArrayView(mesh->colorsAsArray())),
            Containers::stridedArrayView(Colors),
            TestSuite::Compare::Container);
    } else if(vertexFormatComponentCount(data.colorFormat) == 4) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Color));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Color), data.colorFormat);
        CORRADE_COMPARE_AS(Containers::stridedArrayView(mesh->colorsAsArray()),
            Containers::stridedArrayView(Colors4),
            TestSuite::Compare::Container);
    }

    /* Normals */
    if(data.normalFormat != VertexFormat{}) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Normal));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Normal), data.normalFormat);

        /* Because the signed packed formats are extremely imprecise, we
           increase the fuzziness a bit */
        auto normals = mesh->normalsAsArray();
        const Float precision = Math::pow(10.0f, -1.5f*vertexFormatSize(vertexFormatComponentFormat(data.normalFormat)));
        CORRADE_COMPARE_AS(precision, 5.0e-2f, TestSuite::Compare::Less);
        CORRADE_COMPARE_AS(precision, 1.0e-6f, TestSuite::Compare::GreaterOrEqual);
        CORRADE_COMPARE(normals.size(), Containers::arraySize(Normals));
        CORRADE_ITERATION("precision" << precision);
        for(std::size_t i = 0; i != normals.size(); ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE_WITH(normals[i], Normals[i],
                TestSuite::Compare::around(Vector3{precision}));
        }
    } else CORRADE_VERIFY(!mesh->hasAttribute(MeshAttribute::Normal));

    /* Texture coordinates */
    if(data.texcoordFormat != VertexFormat{}) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::TextureCoordinates));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::TextureCoordinates), data.texcoordFormat);
        CORRADE_COMPARE_AS(mesh->textureCoordinates2DAsArray(),
            Containers::stridedArrayView(TextureCoordinates),
            TestSuite::Compare::Container);
    } else CORRADE_VERIFY(!mesh->hasAttribute(MeshAttribute::TextureCoordinates));

    /* Object ID */
    if(data.objectIdFormat != VertexFormat{}) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::ObjectId));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::ObjectId), data.objectIdFormat);
        CORRADE_COMPARE_AS(mesh->objectIdsAsArray(),
            Containers::stridedArrayView(ObjectIds),
            TestSuite::Compare::Container);
    } else CORRADE_VERIFY(!mesh->hasAttribute(MeshAttribute::ObjectId));
}

void StanfordImporterTest::parsePerFace() {
    auto&& data = ParsePerFaceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");
    /* Verbose flag won't do anything in this case */
    importer->setFlags(data.flags);

    importer->configuration().setValue("perFaceToPerVertex", false);
    importer->configuration().setValue("objectIdAttribute", "objectid");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, data.filename)));
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->meshLevelCount(0), 2);

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), data.indexType);
    CORRADE_COMPARE_AS(mesh->indicesAsArray(),
        Containers::arrayView(Indices),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(mesh->positions3DAsArray(),
        Containers::arrayView(Positions),
        TestSuite::Compare::Container);

    auto faceMesh = importer->mesh(0, 1);
    CORRADE_VERIFY(faceMesh);
    CORRADE_COMPARE(faceMesh->primitive(), MeshPrimitive::Faces);
    CORRADE_VERIFY(!faceMesh->isIndexed());
    CORRADE_COMPARE(faceMesh->attributeCount(), data.faceAttributeCount);
    /* Two faces, one a quad, the other a triangle */
    CORRADE_COMPARE(faceMesh->vertexCount(), 3);

    if(data.colorFormat != VertexFormat{}) {
        CORRADE_VERIFY(faceMesh->hasAttribute(MeshAttribute::Color));
        CORRADE_COMPARE(faceMesh->attributeFormat(MeshAttribute::Color), data.colorFormat);
        CORRADE_COMPARE_AS(faceMesh->colorsAsArray(),
            Containers::arrayView<Color4>({
                {0.8f, 0.2f, 0.4f, 0.266667f},
                {0.8f, 0.2f, 0.4f, 0.266667f},
                {0.6f, 0.666667f, 1.0f, 0.866667f}
            }), TestSuite::Compare::Container);
    }

    if(data.normalFormat != VertexFormat{}) {
        CORRADE_VERIFY(faceMesh->hasAttribute(MeshAttribute::Normal));
        CORRADE_COMPARE(faceMesh->attributeFormat(MeshAttribute::Normal), data.normalFormat);
        CORRADE_COMPARE_AS(faceMesh->normalsAsArray(),
            Containers::arrayView<Vector3>({
                {-0.333333f, -0.666667f, -0.933333f},
                {-0.333333f, -0.666667f, -0.933333f},
                {-0.0f, -0.133333f, -1.0f}
            }), TestSuite::Compare::Container);
    }

    if(data.objectIdFormat != VertexFormat{}) {
        CORRADE_VERIFY(faceMesh->hasAttribute(MeshAttribute::ObjectId));
        CORRADE_COMPARE(faceMesh->attributeFormat(MeshAttribute::ObjectId), data.objectIdFormat);
        CORRADE_COMPARE_AS(faceMesh->objectIdsAsArray(),
            Containers::arrayView<UnsignedInt>({117, 117, 56}),
            TestSuite::Compare::Container);
    }
}

void StanfordImporterTest::parsePerFaceToPerVertex() {
    auto&& data = ParsePerFaceData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");
    importer->setFlags(data.flags);

    /* Done by default */
    //importer->configuration().setValue("perFaceToPerVertex", true);
    importer->configuration().setValue("objectIdAttribute", "objectid");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, data.filename)));
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->meshLevelCount(0), 1);

    std::ostringstream out;
    Containers::Optional<MeshData> mesh;
    {
        Debug redirectOutput{&out};
        mesh = importer->mesh(0);
    }
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(), 1 + data.faceAttributeCount);

    CORRADE_VERIFY(mesh->isIndexed());

    /* If there are no per-face attributes, the data is kept as-is. Otherwise
       it's duplicated to account for split vertices */
    if(!data.faceAttributeCount) {
        CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedInt);
        CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
            Containers::arrayView(Indices), TestSuite::Compare::Container);

        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
        CORRADE_COMPARE_AS(mesh->positions3DAsArray(),
            Containers::arrayView(Positions),
            TestSuite::Compare::Container);
    } else {
        /* Index type always inflated to 32bit */
        CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedInt);
        CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
            Containers::arrayView(IndicesPerFaceToPerVertex),
            TestSuite::Compare::Container);

        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
        CORRADE_COMPARE_AS(mesh->positions3DAsArray(),
            Containers::arrayView(PositionsPerFaceToPerVertex),
            TestSuite::Compare::Container);
    }

    if(data.colorFormat != VertexFormat{}) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Color));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Color), data.colorFormat);
        CORRADE_COMPARE_AS(mesh->colorsAsArray(),
            Containers::arrayView<Color4>({
                {0.8f, 0.2f, 0.4f, 0.266667f},
                {0.8f, 0.2f, 0.4f, 0.266667f},
                {0.8f, 0.2f, 0.4f, 0.266667f},
                {0.8f, 0.2f, 0.4f, 0.266667f},

                {0.6f, 0.666667f, 1.0f, 0.866667f},
                {0.6f, 0.666667f, 1.0f, 0.866667f},
                {0.6f, 0.666667f, 1.0f, 0.866667f}
            }), TestSuite::Compare::Container);
    }

    if(data.normalFormat != VertexFormat{}) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Normal));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Normal), data.normalFormat);
        CORRADE_COMPARE_AS(mesh->normalsAsArray(),
            Containers::arrayView<Vector3>({
                {-0.333333f, -0.666667f, -0.933333f},
                {-0.333333f, -0.666667f, -0.933333f},
                {-0.333333f, -0.666667f, -0.933333f},
                {-0.333333f, -0.666667f, -0.933333f},

                {-0.0f, -0.133333f, -1.0f},
                {-0.0f, -0.133333f, -1.0f},
                {-0.0f, -0.133333f, -1.0f}
            }), TestSuite::Compare::Container);
    }

    if(data.objectIdFormat != VertexFormat{}) {
        CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::ObjectId));
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::ObjectId), data.objectIdFormat);
        CORRADE_COMPARE_AS(mesh->objectIdsAsArray(),
            Containers::arrayView<UnsignedInt>({
                117, 117, 117, 117,
                56, 56, 56}),
            TestSuite::Compare::Container);
    }

    /* Verbose message, if any */
    CORRADE_COMPARE(out.str(), data.message);
}

void StanfordImporterTest::empty() {
    auto&& data = EmptyData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    importer->configuration().setValue("perFaceToPerVertex", data.perFaceToPerVertex);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "empty.ply")));
    CORRADE_COMPARE(importer->meshCount(), 1);

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

    /* There's no faces or per-face attributes either */
    if(!data.perFaceToPerVertex) {
        CORRADE_COMPARE(importer->meshLevelCount(0), 2);

        auto faceMesh = importer->mesh(0, 1);
        CORRADE_VERIFY(faceMesh);
        CORRADE_COMPARE(faceMesh->primitive(), MeshPrimitive::Faces);
        CORRADE_VERIFY(!faceMesh->isIndexed());
        CORRADE_COMPARE(faceMesh->attributeCount(), 0);
        CORRADE_COMPARE(faceMesh->vertexCount(), 0);
    }
}

void StanfordImporterTest::customAttributes() {
    auto&& data = CustomAttributeData[testCaseInstanceId()];
    setTestCaseDescription(Utility::String::replaceAll(data.filename, "-", " "));

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    importer->configuration().setValue("perFaceToPerVertex", false);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, Utility::formatString("{}.ply", data.filename))));

    /* The mapping should be available even before the mesh is imported */
    const MeshAttribute indexAttribute = importer->meshAttributeForName("index");
    CORRADE_COMPARE(indexAttribute, meshAttributeCustom(0));
    CORRADE_COMPARE(importer->meshAttributeName(indexAttribute), "index");

    const MeshAttribute weightAttribute = importer->meshAttributeForName("weight");
    CORRADE_COMPARE(weightAttribute, meshAttributeCustom(1));
    CORRADE_COMPARE(importer->meshAttributeName(weightAttribute), "weight");

    const MeshAttribute maskAttribute = importer->meshAttributeForName("mask");
    CORRADE_COMPARE(maskAttribute, meshAttributeCustom(2));
    CORRADE_COMPARE(importer->meshAttributeName(maskAttribute), "mask");

    const MeshAttribute idAttribute = importer->meshAttributeForName("id");
    CORRADE_COMPARE(idAttribute, meshAttributeCustom(3));
    CORRADE_COMPARE(importer->meshAttributeName(idAttribute), "id");

    /* Asking for attribute name that's out of bounds should be handled
       gracefully */
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(1000)), "");

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(), 3);
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedByte);
    CORRADE_COMPARE_AS(mesh->indicesAsArray(),
        Containers::arrayView(Indices),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3us);
    CORRADE_COMPARE_AS(mesh->positions3DAsArray(),
        Containers::arrayView(Positions),
        TestSuite::Compare::Container);

    /* Custom attributes */
    CORRADE_VERIFY(mesh->hasAttribute(indexAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(indexAttribute), VertexFormat::UnsignedByte);
    CORRADE_COMPARE_AS(mesh->attribute<UnsignedByte>(indexAttribute),
        Containers::arrayView<UnsignedByte>({
            0xaa, 0xab, 0xac, 0xad, 0xae
        }), TestSuite::Compare::Container);
    CORRADE_VERIFY(mesh->hasAttribute(weightAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(weightAttribute), VertexFormat::Double);
    CORRADE_COMPARE_AS(mesh->attribute<Double>(weightAttribute),
        Containers::arrayView<Double>({
            1.23456, 12.3456, 123.456, 1234.56, 12345.6
        }), TestSuite::Compare::Container);

    /* Custom face attributes */
    auto faceMesh = importer->mesh(0, 1);
    CORRADE_VERIFY(faceMesh);
    CORRADE_COMPARE(faceMesh->primitive(), MeshPrimitive::Faces);
    CORRADE_VERIFY(!faceMesh->isIndexed());
    CORRADE_VERIFY(faceMesh->hasAttribute(maskAttribute));
    CORRADE_COMPARE(faceMesh->attributeFormat(maskAttribute), VertexFormat::UnsignedShort);
    CORRADE_COMPARE_AS(faceMesh->attribute<UnsignedShort>(maskAttribute),
        Containers::arrayView<UnsignedShort>({
            0xf0f0, 0xf0f0, 0xf1f1
        }), TestSuite::Compare::Container);
    CORRADE_VERIFY(faceMesh->hasAttribute(idAttribute));
    CORRADE_COMPARE(faceMesh->attributeFormat(idAttribute), VertexFormat::Int);
    CORRADE_COMPARE_AS(faceMesh->attribute<Int>(idAttribute),
        Containers::arrayView<Int>({
            15688464, 15688464, -24512
        }), TestSuite::Compare::Container);
}

void StanfordImporterTest::customAttributesPerFaceToPerVertex() {
    auto&& data = CustomAttributeData[testCaseInstanceId()];
    setTestCaseDescription(Utility::String::replaceAll(data.filename, "-", " "));

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    /* Done by default */
    //importer->configuration().setValue("perFaceToPerVertex", true);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, Utility::formatString("{}.ply", data.filename))));

    const MeshAttribute indexAttribute = importer->meshAttributeForName("index");
    const MeshAttribute weightAttribute = importer->meshAttributeForName("weight");
    const MeshAttribute maskAttribute = importer->meshAttributeForName("mask");
    const MeshAttribute idAttribute = importer->meshAttributeForName("id");

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(), 5);
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE_AS(mesh->indicesAsArray(),
        Containers::arrayView(IndicesPerFaceToPerVertex),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3us);
    CORRADE_COMPARE_AS(mesh->positions3DAsArray(),
        Containers::arrayView(PositionsPerFaceToPerVertex),
        TestSuite::Compare::Container);

    /* Custom attributes */
    CORRADE_VERIFY(mesh->hasAttribute(indexAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(indexAttribute), VertexFormat::UnsignedByte);
    CORRADE_COMPARE_AS(mesh->attribute<UnsignedByte>(indexAttribute),
        Containers::arrayView<UnsignedByte>({
            0xaa, 0xab, 0xac, 0xad,
            0xad, 0xac, 0xae
        }), TestSuite::Compare::Container);
    CORRADE_VERIFY(mesh->hasAttribute(weightAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(weightAttribute), VertexFormat::Double);
    CORRADE_COMPARE_AS(mesh->attribute<Double>(weightAttribute),
        Containers::arrayView<Double>({
            1.23456, 12.3456, 123.456, 1234.56,
            1234.56, 123.456, 12345.6
        }), TestSuite::Compare::Container);

    /* Custom face attributes, converted into per-vertex */
    CORRADE_VERIFY(mesh->hasAttribute(maskAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(maskAttribute), VertexFormat::UnsignedShort);
    CORRADE_COMPARE_AS(mesh->attribute<UnsignedShort>(maskAttribute),
        Containers::arrayView<UnsignedShort>({
            0xf0f0, 0xf0f0, 0xf0f0, 0xf0f0,
            0xf1f1, 0xf1f1, 0xf1f1
        }), TestSuite::Compare::Container);
    CORRADE_VERIFY(mesh->hasAttribute(idAttribute));
    CORRADE_COMPARE(mesh->attributeFormat(idAttribute), VertexFormat::Int);
    CORRADE_COMPARE_AS(mesh->attribute<Int>(idAttribute),
        Containers::arrayView<Int>({
            15688464, 15688464, 15688464, 15688464,
            -24512, -24512, -24512
        }), TestSuite::Compare::Container);
}

void StanfordImporterTest::customAttributesDuplicate() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "custom-components-duplicate.ply")));

    /* Disabling this to check that names can be shared across
       per-face/per-vertex attributes as well */
    importer->configuration().setValue("perFaceToPerVertex", false);

    const MeshAttribute weightAttribute = importer->meshAttributeForName("weight");
    CORRADE_COMPARE(weightAttribute, meshAttributeCustom(0));
    CORRADE_COMPARE(importer->meshAttributeName(weightAttribute), "weight");

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(), 4);
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedByte);
    CORRADE_COMPARE_AS(mesh->indicesAsArray(),
        Containers::arrayView(Indices),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3us);
    CORRADE_COMPARE_AS(mesh->positions3DAsArray(),
        Containers::arrayView(Positions),
        TestSuite::Compare::Container);

    /* Custom attributes. The weight is there three times, it should always be
       the same ID, but can be a different format. */
    CORRADE_COMPARE(mesh->attributeCount(weightAttribute), 3);
    CORRADE_COMPARE(mesh->attributeFormat(weightAttribute, 0), VertexFormat::Float);
    CORRADE_COMPARE_AS(mesh->attribute<Float>(weightAttribute, 0),
        Containers::arrayView<Float>({
            0.1f, 0.2f, 0.3f, 0.4f, 0.5f
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeFormat(weightAttribute, 1), VertexFormat::Float);
    CORRADE_COMPARE_AS(mesh->attribute<Float>(weightAttribute, 1),
        Containers::arrayView<Float>({
            0.7f, 0.1f, 0.3f, 0.6f, 0.4f
        }), TestSuite::Compare::Container);
    CORRADE_COMPARE(mesh->attributeFormat(weightAttribute, 2), VertexFormat::Double);
    CORRADE_COMPARE_AS(mesh->attribute<Double>(weightAttribute, 2),
        Containers::arrayView<Double>({
            0.2, 0.7, 0.4, 0.0, 0.1
        }), TestSuite::Compare::Container);

    /* And in the faces as well */
    auto faceMesh = importer->mesh(0, 1);
    CORRADE_VERIFY(faceMesh);
    CORRADE_COMPARE(faceMesh->primitive(), MeshPrimitive::Faces);
    CORRADE_VERIFY(!faceMesh->isIndexed());
    CORRADE_VERIFY(faceMesh->hasAttribute(weightAttribute));
    CORRADE_COMPARE(faceMesh->attributeFormat(weightAttribute), VertexFormat::UnsignedByte);
    CORRADE_COMPARE_AS(faceMesh->attribute<UnsignedByte>(weightAttribute),
        Containers::arrayView<UnsignedByte>({
            127, 127, 155
        }), TestSuite::Compare::Container);
}

void StanfordImporterTest::customAttributesNoFileOpened() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    /* These should return nothing (and not crash) */
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(564)), "");
    CORRADE_COMPARE(importer->meshAttributeForName("thing"), MeshAttribute{});
}

void StanfordImporterTest::triangleFastPath() {
    auto&& data = FastTrianglePathData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");
    importer->configuration().setValue("triangleFastPath", data.enabled);
    importer->configuration().setValue("perFaceToPerVertex", false);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "triangle-fast-path-be.ply")));

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedShort);
    /* The file is BE to verify the endian flip is done here as well */
    CORRADE_COMPARE_AS(mesh->indices<UnsignedShort>(),
        Containers::arrayView<UnsignedShort>({
            0, 1, 2, 0, 2, 3, 3, 2, 4
        }), TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 1);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position),
        VertexFormat::Vector3b);
    CORRADE_COMPARE(mesh->vertexCount(), 5);

    const MeshAttribute somethingBefore = importer->meshAttributeForName("something_before");
    const MeshAttribute somethingAfter = importer->meshAttributeForName("something_after");

    auto faceMesh = importer->mesh(0, 1);
    CORRADE_VERIFY(faceMesh);
    CORRADE_COMPARE(faceMesh->primitive(), MeshPrimitive::Faces);
    CORRADE_VERIFY(!faceMesh->isIndexed());
    CORRADE_VERIFY(faceMesh->hasAttribute(somethingBefore));
    CORRADE_COMPARE(faceMesh->attributeFormat(somethingBefore), VertexFormat::UnsignedInt);
    CORRADE_COMPARE_AS(faceMesh->attribute<UnsignedInt>(somethingBefore),
        Containers::arrayView<UnsignedInt>({
            0xfaffffff, 0xfaffffff, 0xffffffaf
        }), TestSuite::Compare::Container);
    CORRADE_VERIFY(faceMesh->hasAttribute(somethingAfter));
    CORRADE_COMPARE(faceMesh->attributeFormat(somethingAfter), VertexFormat::UnsignedShort);
    CORRADE_COMPARE_AS(faceMesh->attribute<UnsignedShort>(somethingAfter),
        Containers::arrayView<UnsignedShort>({
            0xabaa, 0xabaa, 0xbbab
        }), TestSuite::Compare::Container);
}

void StanfordImporterTest::triangleFastPathPerFaceToPerVertex() {
    auto&& data = FastTrianglePathData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");
    importer->configuration().setValue("triangleFastPath", data.enabled);

    /* Done by default */
    //importer->configuration().setValue("perFaceToPerVertex", true);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "triangle-fast-path-be.ply")));

    auto mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);

    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(),
        Containers::arrayView(IndicesPerFaceToPerVertex),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(mesh->attributeCount(), 3);
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position),
        VertexFormat::Vector3b);
    CORRADE_COMPARE(mesh->vertexCount(), 7);

    const MeshAttribute somethingBefore = importer->meshAttributeForName("something_before");
    const MeshAttribute somethingAfter = importer->meshAttributeForName("something_after");

    CORRADE_VERIFY(mesh->hasAttribute(somethingBefore));
    CORRADE_COMPARE(mesh->attributeFormat(somethingBefore), VertexFormat::UnsignedInt);
    CORRADE_COMPARE_AS(mesh->attribute<UnsignedInt>(somethingBefore),
        Containers::arrayView<UnsignedInt>({
            0xfaffffff, 0xfaffffff, 0xfaffffff, 0xfaffffff,
            0xffffffaf, 0xffffffaf, 0xffffffaf
        }), TestSuite::Compare::Container);
    CORRADE_VERIFY(mesh->hasAttribute(somethingAfter));
    CORRADE_COMPARE(mesh->attributeFormat(somethingAfter), VertexFormat::UnsignedShort);
    CORRADE_COMPARE_AS(mesh->attribute<UnsignedShort>(somethingAfter),
        Containers::arrayView<UnsignedShort>({
            0xabaa, 0xabaa, 0xabaa, 0xabaa,
            0xbbab, 0xbbab, 0xbbab
        }), TestSuite::Compare::Container);
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
