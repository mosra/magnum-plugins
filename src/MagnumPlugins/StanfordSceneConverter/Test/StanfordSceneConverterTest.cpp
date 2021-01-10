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
#include <Corrade/Containers/Optional.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/AbstractSceneConverter.h>
#include <Magnum/Trade/MeshData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct StanfordSceneConverterTest: TestSuite::Tester {
    explicit StanfordSceneConverterTest();

    void nonIndexedAllAttributes();
    template<class T> void indexed();

    void threeComponentColors();
    void triangleFan();
    void indexedTriangleStrip();
    void empty();

    void lines();
    void twoComponentPositions();
    void invalidEndianness();

    void ignoredAttributes();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractSceneConverter> _converterMnager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

struct {
    const char* name;
    const char* endianness;
    const char* objectIdAttribute;
    const char* file;
} NonIndexedAllAttributesData[] {
    {"native endian", nullptr,
        #ifdef CORRADE_TARGET_BIG_ENDIAN
        "SEMANTIC",
        "nonindexed-all-attributes-be.ply"
        #else
        nullptr,
        "nonindexed-all-attributes-le.ply"
        #endif
        },
    {"little endian", "little", nullptr, "nonindexed-all-attributes-le.ply"},
    {"big endian", "big", "SEMANTIC", "nonindexed-all-attributes-be.ply"}
};

struct {
    const char* name;
    const char* endianness;
    const char* fileSuffix;
} IndexedData[] {
    {"little endian", "little", "le"},
    {"big endian", "big", "be"}
};

struct {
    const char* name;
    MeshAttribute attribute;
    VertexFormat format;
    const char* message;
} IgnoredAttributesData[] {
    {"unsupported attribute", meshAttributeCustom(3), VertexFormat::UnsignedShort,
        "skipping unsupported attribute Trade::MeshAttribute::Custom(3)"},
    {"implementation-specific format", MeshAttribute::ObjectId, vertexFormatWrap(3),
        "skipping attribute Trade::MeshAttribute::ObjectId with VertexFormat::ImplementationSpecific(0x3)"},
    {"unsupported format", MeshAttribute::Position, VertexFormat::Vector3h,
        "skipping attribute Trade::MeshAttribute::Position with unsupported format VertexFormat::Vector3h"}
};

StanfordSceneConverterTest::StanfordSceneConverterTest() {
    addInstancedTests({&StanfordSceneConverterTest::nonIndexedAllAttributes},
        Containers::arraySize(NonIndexedAllAttributesData));

    addInstancedTests<StanfordSceneConverterTest>({
        &StanfordSceneConverterTest::indexed<UnsignedByte>,
        &StanfordSceneConverterTest::indexed<UnsignedShort>,
        &StanfordSceneConverterTest::indexed<UnsignedInt>},
        Containers::arraySize(IndexedData));

    addTests({&StanfordSceneConverterTest::threeComponentColors,
              &StanfordSceneConverterTest::triangleFan,
              &StanfordSceneConverterTest::indexedTriangleStrip,
              &StanfordSceneConverterTest::empty,

              &StanfordSceneConverterTest::lines,
              &StanfordSceneConverterTest::twoComponentPositions,
              &StanfordSceneConverterTest::invalidEndianness});

    addInstancedTests({&StanfordSceneConverterTest::ignoredAttributes},
        Containers::arraySize(IgnoredAttributesData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STANFORDSCENECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterMnager.load(STANFORDSCENECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STANFORDIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(STANFORDIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

/* Has to be defined out of class as MSVC 2015 doesn't understand the bitfields
   otherwise */
struct Vertex {
    Vector2s textureCoordinates;
    Vector3 position;
    Int:32;
    Color4ub color;
    UnsignedInt objectId;
    Int:8;
    Vector3b normal;
};

void StanfordSceneConverterTest::nonIndexedAllAttributes() {
    auto&& data = NonIndexedAllAttributesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    using namespace Math::Literals;

    /* Type includes paddings to verify that those are gone when saving the
       file. Four triangles in total. */
    const Vertex vertices[] {
        {{15, 33}, {1.5f, 0.4f, 9.2f}, 0xdeadbeef_rgba, 163247, {15, -100, 0}},
        {{2762, 90}, {0.3f, -1.1f, 0.1f}, 0xbadcafe_rgba, 13543154, {12, 52, -44}},
        {{}, {}, {}, 0, {}},
        {{}, {}, {}, 0, {}},
        {{}, {}, {}, 0, {}},
        {{15, 34}, {0.4f, 2.2f, 0.1f}, 0x33005577_rgba, 10, {14, 42, 34}},
        {{}, {}, {}, 0, {}},
        {{18, 98}, {1.0f, 2.0f, 3.0f}, 0x77777777_rgba, 168, {0, 78, 24}},
        {{}, {}, {}, 0, {}},
        {{}, {}, {}, 0, {}},
        {{}, {}, {}, 0, {}},
        {{}, {}, {}, 0, {}}
    };
    MeshData mesh{MeshPrimitive::Triangles, {}, vertices, {
        MeshAttributeData{MeshAttribute::TextureCoordinates,
            VertexFormat::Vector2usNormalized,
            offsetof(Vertex, textureCoordinates), 12, sizeof(Vertex)},
        MeshAttributeData{MeshAttribute::Position,
            VertexFormat::Vector3,
            offsetof(Vertex, position), 12, sizeof(Vertex)},
        MeshAttributeData{MeshAttribute::Color,
            VertexFormat::Vector4ubNormalized,
            offsetof(Vertex, color), 12, sizeof(Vertex)},
        MeshAttributeData{MeshAttribute::ObjectId,
            VertexFormat::UnsignedInt,
            offsetof(Vertex, objectId), 12, sizeof(Vertex)},
        MeshAttributeData{MeshAttribute::Normal,
            VertexFormat::Vector3bNormalized,
            offsetof(Vertex, normal), 12, sizeof(Vertex)}
    }};

    Containers::Pointer<AbstractSceneConverter> converter = _converterMnager.instantiate("StanfordSceneConverter");
    if(data.endianness)
        converter->configuration().setValue("endianness", data.endianness);
    if(data.objectIdAttribute)
        converter->configuration().setValue("objectIdAttribute", data.objectIdAttribute);

    Containers::Array<char> out = converter->convertToData(mesh);
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS((std::string{out.data(), out.size()}),
        Utility::Directory::join(STANFORDSCENECONVERTER_TEST_DIR, data.file),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("StanfordImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StanfordImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StanfordImporter");
    if(data.objectIdAttribute)
        importer->configuration().setValue("objectIdAttribute", data.objectIdAttribute);
    CORRADE_VERIFY(importer->openData(out));

    Containers::Optional<MeshData> importedMesh = importer->mesh(0);
    CORRADE_VERIFY(importedMesh);

    CORRADE_VERIFY(importedMesh->isIndexed());
    CORRADE_COMPARE(importedMesh->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE_AS(importedMesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(importedMesh->attributeCount(), 5);
    for(UnsignedInt i = 0; i != importedMesh->attributeCount(); ++i) {
        CORRADE_ITERATION(i);
        CORRADE_COMPARE(importedMesh->attributeStride(i), 27);
    }

    CORRADE_VERIFY(importedMesh->hasAttribute(MeshAttribute::TextureCoordinates));
    CORRADE_COMPARE(importedMesh->attributeFormat(MeshAttribute::TextureCoordinates), VertexFormat::Vector2usNormalized);
    CORRADE_COMPARE(importedMesh->attributeOffset(MeshAttribute::TextureCoordinates), 0);
    CORRADE_COMPARE_AS(importedMesh->attribute<Vector2us>(MeshAttribute::TextureCoordinates),
        mesh.attribute<Vector2us>(MeshAttribute::TextureCoordinates),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(importedMesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(importedMesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE(importedMesh->attributeOffset(MeshAttribute::Position), 4);
    CORRADE_COMPARE_AS(importedMesh->attribute<Vector3>(MeshAttribute::Position),
        mesh.attribute<Vector3>(MeshAttribute::Position),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(importedMesh->hasAttribute(MeshAttribute::Color));
    CORRADE_COMPARE(importedMesh->attributeFormat(MeshAttribute::Color), VertexFormat::Vector4ubNormalized);
    CORRADE_COMPARE(importedMesh->attributeOffset(MeshAttribute::Color), 16);
    CORRADE_COMPARE_AS(importedMesh->attribute<Color4ub>(MeshAttribute::Color),
        mesh.attribute<Color4ub>(MeshAttribute::Color),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(importedMesh->hasAttribute(MeshAttribute::ObjectId));
    CORRADE_COMPARE(importedMesh->attributeFormat(MeshAttribute::ObjectId), VertexFormat::UnsignedInt);
    CORRADE_COMPARE(importedMesh->attributeOffset(MeshAttribute::ObjectId), 20);
    CORRADE_COMPARE_AS(importedMesh->attribute<UnsignedInt>(MeshAttribute::ObjectId),
        mesh.attribute<UnsignedInt>(MeshAttribute::ObjectId),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(importedMesh->hasAttribute(MeshAttribute::Normal));
    CORRADE_COMPARE(importedMesh->attributeFormat(MeshAttribute::Normal), VertexFormat::Vector3bNormalized);
    CORRADE_COMPARE(importedMesh->attributeOffset(MeshAttribute::Normal), 24);
    CORRADE_COMPARE_AS(importedMesh->attribute<Vector3b>(MeshAttribute::Normal),
        mesh.attribute<Vector3b>(MeshAttribute::Normal),
        TestSuite::Compare::Container);
}

template<class> struct IndexTypeData;
template<> struct IndexTypeData<UnsignedInt> {
    static const char* file() { return "indexed-uint-{}.ply"; }
    static MeshIndexType type() { return MeshIndexType::UnsignedInt; }
};
template<> struct IndexTypeData<UnsignedShort> {
    static const char* file() { return "indexed-ushort-{}.ply"; }
    static MeshIndexType type() { return MeshIndexType::UnsignedShort; }
};
template<> struct IndexTypeData<UnsignedByte> {
    static const char* file() { return "indexed-uchar-{}.ply"; }
    static MeshIndexType type() { return MeshIndexType::UnsignedByte; }
};

template<class T> void StanfordSceneConverterTest::indexed() {
    auto&& data = IndexedData[testCaseInstanceId()];
    setTestCaseTemplateName(Math::TypeTraits<T>::name());
    setTestCaseDescription(data.name);

    const Vector3 positions[] {
        {-1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        { 1.0f,  1.0f, 0.0f},
        {-1.0f,  1.0f, 0.0f}
    };
    const T indices[] { 0, 1, 2, 0, 2, 3 };
    MeshData mesh{MeshPrimitive::Triangles,
        {}, indices, MeshIndexData{indices},
        {}, positions, {
            MeshAttributeData{MeshAttribute::Position,
            Containers::arrayView(positions)}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterMnager.instantiate("StanfordSceneConverter");
    converter->configuration().setValue("endianness", data.endianness);

    Containers::Array<char> out = converter->convertToData(mesh);
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS((std::string{out.data(), out.size()}),
        Utility::Directory::join(STANFORDSCENECONVERTER_TEST_DIR,
            Utility::formatString(IndexTypeData<T>::file(), data.fileSuffix)),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("StanfordImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StanfordImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StanfordImporter");
    CORRADE_VERIFY(importer->openData(out));

    Containers::Optional<MeshData> importedMesh = importer->mesh(0);
    CORRADE_VERIFY(importedMesh);

    CORRADE_VERIFY(importedMesh->isIndexed());
    CORRADE_COMPARE(importedMesh->indexType(), IndexTypeData<T>::type());
    CORRADE_COMPARE_AS(importedMesh->indices<T>(),
        Containers::arrayView(indices),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(importedMesh->attributeCount(), 1);
    CORRADE_VERIFY(importedMesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(importedMesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE(importedMesh->attributeOffset(MeshAttribute::Position), 0);
    CORRADE_COMPARE_AS(importedMesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView(positions),
        TestSuite::Compare::Container);
}

void StanfordSceneConverterTest::threeComponentColors() {
    const struct Vertex {
        Vector3s position;
        Color3us color;
    } vertices[] {
        {{15, 3233, -6}, {257, 15, 1566}},
        {{687, -357, 10}, {687, 5, 0}},
        {{1, 2, 3}, {0, 2, 0}}
    };
    MeshData mesh{MeshPrimitive::Triangles, {}, vertices, {
        MeshAttributeData{MeshAttribute::Position,
            VertexFormat::Vector3s,
            offsetof(Vertex, position), 3, sizeof(Vertex)},
        MeshAttributeData{MeshAttribute::Color,
            VertexFormat::Vector3usNormalized,
            offsetof(Vertex, color), 3, sizeof(Vertex)}
    }};

    Containers::Pointer<AbstractSceneConverter> converter = _converterMnager.instantiate("StanfordSceneConverter");
    converter->configuration().setValue("endianness", "little");

    Containers::Array<char> out = converter->convertToData(mesh);
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS((std::string{out.data(), out.size()}),
        Utility::Directory::join(STANFORDSCENECONVERTER_TEST_DIR, "three-component-color-le.ply"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("StanfordImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StanfordImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StanfordImporter");
    CORRADE_VERIFY(importer->openData(out));

    Containers::Optional<MeshData> importedMesh = importer->mesh(0);
    CORRADE_VERIFY(importedMesh);

    CORRADE_VERIFY(importedMesh->isIndexed());
    CORRADE_COMPARE(importedMesh->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE_AS(importedMesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1, 2}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(importedMesh->attributeCount(), 2);

    CORRADE_VERIFY(importedMesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(importedMesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3s);
    CORRADE_COMPARE(importedMesh->attributeOffset(MeshAttribute::Position), 0);
    CORRADE_COMPARE(importedMesh->attributeStride(MeshAttribute::Color), 12);
    CORRADE_COMPARE_AS(importedMesh->attribute<Vector3s>(MeshAttribute::Position),
        mesh.attribute<Vector3s>(MeshAttribute::Position),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(importedMesh->hasAttribute(MeshAttribute::Color));
    CORRADE_COMPARE(importedMesh->attributeFormat(MeshAttribute::Color), VertexFormat::Vector3usNormalized);
    CORRADE_COMPARE(importedMesh->attributeOffset(MeshAttribute::Color), 6);
    CORRADE_COMPARE(importedMesh->attributeStride(MeshAttribute::Color), 12);
    CORRADE_COMPARE_AS(importedMesh->attribute<Color3us>(MeshAttribute::Color),
        mesh.attribute<Color3us>(MeshAttribute::Color),
        TestSuite::Compare::Container);
}

void StanfordSceneConverterTest::triangleFan() {
    const Vector3 positions[] {
        { 0.0f,  0.0f, 0.0f},
        {-1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        { 1.0f,  1.0f, 0.0f}
    };
    MeshData mesh{MeshPrimitive::TriangleFan,
        {}, positions, {
            MeshAttributeData{MeshAttribute::Position,
            Containers::arrayView(positions)}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterMnager.instantiate("StanfordSceneConverter");
    converter->configuration().setValue("endianness", "little");

    Containers::Array<char> out = converter->convertToData(mesh);
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS((std::string{out.data(), out.size()}),
        Utility::Directory::join(STANFORDSCENECONVERTER_TEST_DIR, "triangle-fan-le.ply"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("StanfordImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StanfordImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StanfordImporter");
    CORRADE_VERIFY(importer->openData(out));

    Containers::Optional<MeshData> importedMesh = importer->mesh(0);
    CORRADE_VERIFY(importedMesh);

    CORRADE_VERIFY(importedMesh->isIndexed());
    CORRADE_COMPARE(importedMesh->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE_AS(importedMesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1, 2, 0, 2, 3}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(importedMesh->attributeCount(), 1);
    CORRADE_VERIFY(importedMesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(importedMesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE(importedMesh->attributeOffset(MeshAttribute::Position), 0);
    CORRADE_COMPARE(importedMesh->attributeStride(MeshAttribute::Position), 12);
    CORRADE_COMPARE_AS(importedMesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView(positions),
        TestSuite::Compare::Container);
}

void StanfordSceneConverterTest::indexedTriangleStrip() {
    const UnsignedShort indices[] { 1, 2, 0, 3 };
    const Vector3 positions[] {
        { 0.0f,  0.0f, 0.0f},
        {-1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        { 1.0f,  1.0f, 0.0f}
    };
    MeshData mesh{MeshPrimitive::TriangleStrip,
        {}, indices, MeshIndexData{indices},
        {}, positions, {
            MeshAttributeData{MeshAttribute::Position,
            Containers::arrayView(positions)}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterMnager.instantiate("StanfordSceneConverter");
    converter->configuration().setValue("endianness", "little");

    Containers::Array<char> out = converter->convertToData(mesh);
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS((std::string{out.data(), out.size()}),
        Utility::Directory::join(STANFORDSCENECONVERTER_TEST_DIR, "indexed-triangle-strip-le.ply"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("StanfordImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StanfordImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StanfordImporter");
    CORRADE_VERIFY(importer->openData(out));

    Containers::Optional<MeshData> importedMesh = importer->mesh(0);
    CORRADE_VERIFY(importedMesh);

    CORRADE_VERIFY(importedMesh->isIndexed());
    CORRADE_COMPARE(importedMesh->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE_AS(importedMesh->indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({0, 1, 2, 2, 1, 3}),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(importedMesh->attributeCount(), 1);
    CORRADE_VERIFY(importedMesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(importedMesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE(importedMesh->attributeOffset(MeshAttribute::Position), 0);
    CORRADE_COMPARE(importedMesh->attributeStride(MeshAttribute::Position), 12);
    CORRADE_COMPARE_AS(importedMesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {-1.0f, -1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f},
            { 0.0f,  0.0f, 0.0f},
            { 1.0f,  1.0f, 0.0f}
        }), TestSuite::Compare::Container);
}

void StanfordSceneConverterTest::empty() {
    Containers::Pointer<AbstractSceneConverter> converter =  _converterMnager.instantiate("StanfordSceneConverter");
    converter->configuration().setValue("endianness", "little");

    Containers::Array<char> out = converter->convertToData(MeshData{MeshPrimitive::Triangles, 0});
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS((std::string{out.data(), out.size()}),
        Utility::Directory::join(STANFORDSCENECONVERTER_TEST_DIR, "empty-le.ply"),
        TestSuite::Compare::StringToFile);
}

void StanfordSceneConverterTest::lines() {
    Containers::Pointer<AbstractSceneConverter> converter =  _converterMnager.instantiate("StanfordSceneConverter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(MeshData{MeshPrimitive::Lines, 0}));
    CORRADE_COMPARE(out.str(),
        "Trade::StanfordSceneConverter::convertToData(): expected a triangle mesh, got MeshPrimitive::Lines\n");
}

void StanfordSceneConverterTest::twoComponentPositions() {
    const Vector2 positions[1]{};
    MeshData mesh{MeshPrimitive::Triangles,
        {}, positions, {
            MeshAttributeData{MeshAttribute::Position,
            Containers::arrayView(positions)}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterMnager.instantiate("StanfordSceneConverter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(mesh));
    CORRADE_COMPARE(out.str(),
        "Trade::StanfordSceneConverter::convertToData(): two-component positions are not supported\n");
}

void StanfordSceneConverterTest::invalidEndianness() {
    Containers::Pointer<AbstractSceneConverter> converter =  _converterMnager.instantiate("StanfordSceneConverter");
    converter->configuration().setValue("endianness", "wrong");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(MeshData{MeshPrimitive::Triangles, 0}));
    CORRADE_COMPARE(out.str(),
        "Trade::StanfordSceneConverter::convertToData(): invalid option endianness=wrong\n");
}

void StanfordSceneConverterTest::ignoredAttributes() {
    auto&& data = IgnoredAttributesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    const struct Vertex {
        Vector3 position;
        UnsignedLong thing;
    } vertices[] {
        {{-1.0f, -1.0f, 0.0f}, 0xabce},
        {{ 1.0f, -1.0f, 0.0f}, 0x5d4e},
        {{ 1.0f,  1.0f, 0.0f}, 0xed5e},
        {{-1.0f,  1.0f, 0.0f}, 0xaabe}
    };
    const UnsignedShort indices[] { 0, 1, 2, 0, 2, 3 };
    MeshData mesh{MeshPrimitive::Triangles,
        {}, indices, MeshIndexData{indices},
        {}, vertices, {
            MeshAttributeData{MeshAttribute::Position,
                VertexFormat::Vector3,
                offsetof(Vertex, position), 4,
                sizeof(Vertex)},
            MeshAttributeData{data.attribute,
                data.format,
                offsetof(Vertex, thing), 4,
                sizeof(Vertex)}
    }};

    Containers::Pointer<AbstractSceneConverter> converter =  _converterMnager.instantiate("StanfordSceneConverter");
    converter->configuration().setValue("endianness", "little");

    Containers::Array<char> out;
    std::ostringstream wout;
    {
        Warning redirectWarning{&wout};
        out = converter->convertToData(mesh);
    }
    CORRADE_COMPARE(wout.str(),
        Utility::formatString("Trade::StanfordSceneConverter::convertToData(): {}\n", data.message));
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS((std::string{out.data(), out.size()}),
        Utility::Directory::join(STANFORDSCENECONVERTER_TEST_DIR, "indexed-ushort-le.ply"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("StanfordImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StanfordImporter plugin not found, cannot test a rountrip");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StanfordImporter");
    CORRADE_VERIFY(importer->openData(out));

    Containers::Optional<MeshData> importedMesh = importer->mesh(0);
    CORRADE_VERIFY(importedMesh);

    CORRADE_VERIFY(importedMesh->isIndexed());
    CORRADE_COMPARE(importedMesh->indexType(), MeshIndexType::UnsignedShort);
    CORRADE_COMPARE_AS(importedMesh->indices<UnsignedShort>(),
        Containers::arrayView(indices),
        TestSuite::Compare::Container);

    CORRADE_COMPARE(importedMesh->attributeCount(), 1);
    CORRADE_VERIFY(importedMesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(importedMesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE(importedMesh->attributeOffset(MeshAttribute::Position), 0);
    CORRADE_COMPARE(importedMesh->attributeStride(MeshAttribute::Position), 12);
    CORRADE_COMPARE_AS(importedMesh->attribute<Vector3>(MeshAttribute::Position),
        mesh.attribute<Vector3>(MeshAttribute::Position),
        TestSuite::Compare::Container);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StanfordSceneConverterTest)
