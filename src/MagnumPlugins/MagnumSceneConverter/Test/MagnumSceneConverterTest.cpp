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
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/Trade/AbstractSceneConverter.h>
#include <Magnum/Trade/MeshData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct MagnumSceneConverterTest: TestSuite::Tester {
    explicit MagnumSceneConverterTest();

    void convert();
    void convertEndianSwapUnsignedIntIndices();
    /* UnsignedShort indices tested inside convert() */
    void convertEndianSwapUnsignedByteIndices();
    void convertTooLargeFor32bit();
    void convertInvalid();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractSceneConverter> _manager{"nonexistent"};
};

const struct {
    const char* name;
    const char* suffix;
    const char* plugin;
    bool indexed;
} ConvertData[] {
    {"32-bit LE", "le32", "Little32", true},
    {"32-bit LE, non-indexed", "le32", "Little32", false},
    {"64-bit LE", "le64", "Little64", true},
    {"64-bit LE, non-indexed", "le64", "Little64", false},
    {"32-bit BE", "be32", "Big32", true},
    {"32-bit BE, non-indexed", "be32", "Big32", false},
    {"64-bit BE", "be64", "Big64", true},
    {"64-bit BE, non-indexed", "be64", "Big64", false},
    {"current",
        #ifndef CORRADE_TARGET_BIG_ENDIAN
        sizeof(void*) == 4 ? "le32" : "le64",
        #else
        sizeof(void*) == 4 ? "be32" : "be64",
        #endif
        "", true},
    {"current, non-indexed",
        #ifndef CORRADE_TARGET_BIG_ENDIAN
        sizeof(void*) == 4 ? "le32" : "le64",
        #else
        sizeof(void*) == 4 ? "be32" : "be64",
        #endif
        "", false}
};

const struct {
    const char* name;
    const char* plugin;
    std::size_t offset;
    Containers::Array<char> replace;
    const char* message;
} ConvertInvalidData[] {
    {"endian-swap of an implementation-specific format",
        #ifndef CORRADE_TARGET_BIG_ENDIAN
        "MagnumBig32SceneConverter",
        #else
        "MagnumLittle32SceneConverter",
        #endif
        sizeof(void*) == 4 ? 48 + 2*20 : 64 + 2*24,
        #ifndef CORRADE_TARGET_BIG_ENDIAN
        Containers::array<char>({1, 0, 0, '\x80'}),
        #else
        Containers::array<char>({'\x80', 0, 0, 1}),
        #endif
        "cannot perform endian swap on VertexFormat::ImplementationSpecific(0x1)"}
};

MagnumSceneConverterTest::MagnumSceneConverterTest() {
    addInstancedTests({&MagnumSceneConverterTest::convert},
        Containers::arraySize(ConvertData));

    addTests({&MagnumSceneConverterTest::convertEndianSwapUnsignedIntIndices,
              &MagnumSceneConverterTest::convertEndianSwapUnsignedByteIndices,
              &MagnumSceneConverterTest::convertTooLargeFor32bit});

    addInstancedTests({&MagnumSceneConverterTest::convertInvalid},
        Containers::arraySize(ConvertInvalidData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef MAGNUMSCENECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(MAGNUMSCENECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void MagnumSceneConverterTest::convert() {
    auto&& data = ConvertData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("Magnum" + std::string{data.plugin} + "SceneConverter");

    Containers::Array<char> in = Utility::Directory::read(Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR,
        Utility::formatString("mesh-{}{}.blob",
            data.indexed ? "" : "nonindexed-",
            #ifndef CORRADE_TARGET_BIG_ENDIAN
            sizeof(void*) == 4 ? "le32" : "le64"
            #else
            sizeof(void*) == 4 ? "be32" : "be64"
            #endif
        )));
    Containers::Optional<MeshData> mesh = MeshData::deserialize(in);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->isIndexed(), data.indexed);

    Containers::Array<char> out = converter->convertToData(*mesh);
    CORRADE_COMPARE_AS((std::string{out.data(), out.size()}),
        Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR,
            Utility::formatString("mesh-{}{}.blob",
                data.indexed ? "" : "nonindexed-", data.suffix)),
        TestSuite::Compare::StringToFile);
}

void MagnumSceneConverterTest::convertEndianSwapUnsignedIntIndices() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate(
        #ifndef CORRADE_TARGET_BIG_ENDIAN
        "MagnumBig32SceneConverter"
        #else
        "MagnumLittle32SceneConverter"
        #endif
    );

    constexpr UnsignedInt indices[] { 256415, 213247 };
    MeshData mesh{MeshPrimitive::Points,
        {}, indices, MeshIndexData{indices}, 1};

    Containers::Array<char> out = converter->convertToData(mesh);
    #ifndef CORRADE_TARGET_BIG_ENDIAN
    CORRADE_COMPARE_AS((std::string{out.data(), out.size()}),
        Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR, "mesh-uint-indices-be32.blob"),
        TestSuite::Compare::StringToFile);
    #else
    CORRADE_COMPARE_AS((std::string{out.data(), out.size()}),
        Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR, "mesh-uint-indices-le32.blob"),
        TestSuite::Compare::StringToFile);
    #endif
}

void MagnumSceneConverterTest::convertEndianSwapUnsignedByteIndices() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate(
        #ifndef CORRADE_TARGET_BIG_ENDIAN
        "MagnumBig32SceneConverter"
        #else
        "MagnumLittle32SceneConverter"
        #endif
    );

    constexpr UnsignedByte indices[] { 254, 213 };
    MeshData mesh{MeshPrimitive::Points,
        {}, indices, MeshIndexData{indices}, 1};

    Containers::Array<char> out = converter->convertToData(mesh);
    #ifndef CORRADE_TARGET_BIG_ENDIAN
    CORRADE_COMPARE_AS((std::string{out.data(), out.size()}),
        Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR, "mesh-ubyte-indices-be32.blob"),
        TestSuite::Compare::StringToFile);
    #else
    CORRADE_COMPARE_AS((std::string{out.data(), out.size()}),
        Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR, "mesh-ubyte-indices-le32.blob"),
        TestSuite::Compare::StringToFile);
    #endif
}

void MagnumSceneConverterTest::convertTooLargeFor32bit() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MagnumLittle32SceneConverter");

    Containers::ArrayView<UnsignedByte> indices{nullptr, 0xffffffffu - 47u};
    MeshData mesh{MeshPrimitive::Points,
        {}, indices, MeshIndexData{indices}, 1};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(mesh));
    CORRADE_COMPARE(out.str(), "Trade::MagnumSceneConverter::convertToData(): data size 4294967296 too large for a 32-bit output platform\n");
}

void MagnumSceneConverterTest::convertInvalid() {
    auto&& data = ConvertInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate(data.plugin);

    Containers::Array<char> blob = Utility::Directory::read(Utility::Directory::join(MAGNUMIMPORTER_TEST_DIR,
        Utility::formatString("mesh-{}.blob",
            #ifndef CORRADE_TARGET_BIG_ENDIAN
            sizeof(void*) == 4 ? "le32" : "le64"
            #else
            sizeof(void*) == 4 ? "be32" : "be64"
            #endif
        )));
    CORRADE_VERIFY(blob);

    if(data.replace) Utility::copy(data.replace, blob.slice(data.offset, data.offset + data.replace.size()));

    Containers::Optional<MeshData> mesh = MeshData::deserialize(blob);
    CORRADE_VERIFY(mesh);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(*mesh));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::MagnumSceneConverter::convertToData(): {}\n", data.message));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::MagnumSceneConverterTest)
