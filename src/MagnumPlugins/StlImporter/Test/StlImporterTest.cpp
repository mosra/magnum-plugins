/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
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

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct StlImporterTest: TestSuite::Tester {
    explicit StlImporterTest();

    void invalid();
    void almostAsciiButNotActually();
    void emptyBinary();
    void binary();

    void asciiDelegateAssimp();
    void asciiDelegateAssimpNoPlugin();
    void asciiDelegateAssimpFailed();

    void openMemory();
    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
    /* A dedicated manager that has AssimpImporter loaded as well in order to
       test both with and without AssimpImporter present. It needs to load
       AnyImageImporter from a system-wide location. */
    PluginManager::Manager<AbstractImporter> _managerWithAssimpImporter;
};

using namespace Containers::Literals;

constexpr const char Data[]{
    /* 80-byte header */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    3, 0, 0, 0, /* Triangle count */

    /* 12*4 + 2 bytes per triangle */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    /* Extra (invalid) data */
    0
};

const struct {
    const char* name;
    Containers::ArrayView<const char> data;
    const char* message;
} InvalidData[] {
    {"four bytes of an ASCII file", Containers::arrayView("soli").exceptSuffix(1),
        "file too short, got only 4 bytes"},
    {"83 bytes", Containers::arrayView(Data).prefix(83),
        "file too short, expected at least 84 bytes but got 83"},
    {"too short", Containers::arrayView(Data).exceptSuffix(2),
        "file size doesn't match triangle count, expected 234 but got 233 for 3 triangles"},
    {"too long", Containers::arrayView(Data),
        "file size doesn't match triangle count, expected 234 but got 235 for 3 triangles"}
};

const struct {
    const char* name;
    bool perFaceToPerVertex;
    UnsignedInt level;
    UnsignedInt levelCount;
    MeshPrimitive primitive;
    UnsignedInt vertexCount;
    UnsignedInt attributeCount;
    bool positions, normals;
} BinaryData[] {
    {"", true, 0, 1, MeshPrimitive::Triangles, 6, 2, true, true},
    {"per-face normals, level 0",
        false, 0, 2, MeshPrimitive::Triangles, 6, 1, true, false},
    {"per-face normals, level 1",
        false, 1, 2, MeshPrimitive::Faces, 2, 1, false, false}
};

const struct {
    const char* name;
    ImporterFlags flags;
} AsciiDelegateAssimpData[]{
    {"", {}},
    {"verbose", ImporterFlag::Verbose},
};

/* Shared among all plugins that implement data copying optimizations */
const struct {
    const char* name;
    bool(*open)(AbstractImporter&, Containers::ArrayView<const void>);
} OpenMemoryData[]{
    {"data", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        /* Copy to ensure the original memory isn't referenced */
        Containers::Array<char> copy{InPlaceInit, Containers::arrayCast<const char>(data)};
        return importer.openData(copy);
    }},
    {"memory", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        return importer.openMemory(data);
    }},
};

StlImporterTest::StlImporterTest() {
    addInstancedTests({&StlImporterTest::invalid},
        Containers::arraySize(InvalidData));

    addTests({&StlImporterTest::almostAsciiButNotActually,
              &StlImporterTest::emptyBinary});

    addInstancedTests({&StlImporterTest::binary},
        Containers::arraySize(BinaryData));

    addInstancedTests({&StlImporterTest::asciiDelegateAssimp},
        Containers::arraySize(AsciiDelegateAssimpData));

    addTests({&StlImporterTest::asciiDelegateAssimpNoPlugin,
              &StlImporterTest::asciiDelegateAssimpFailed});

    addInstancedTests({&StlImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&StlImporterTest::openTwice,
              &StlImporterTest::importTwice});

    /* Load AssimpImporter and reset the plugin dir after so it doesn't load
       anything else from the filesystem. Do this also in case of static
       plugins (no _FILENAME defined) so it doesn't attempt to load dynamic
       system-wide plugins. */
    #ifdef ASSIMPIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_managerWithAssimpImporter.load(ASSIMPIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    _managerWithAssimpImporter.setPluginDirectory({});
    #endif

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STLIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STLIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    CORRADE_INTERNAL_ASSERT_OUTPUT(_managerWithAssimpImporter.load(STLIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void StlImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StlImporter");

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(data.data));
    CORRADE_COMPARE(out,
        Utility::format("Trade::StlImporter::openData(): {}\n", data.message));
}

void StlImporterTest::almostAsciiButNotActually() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StlImporter");

    constexpr const char data[]{
        /* 80-byte header, starting like an ascii file but not fully. The
           importer should not fail on that. */
        's', 'o', 'l', 'i', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

        0, 0, 0, 0, /* No triangles */
    };

    CORRADE_VERIFY(importer->openData(data));

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_COMPARE(mesh->vertexCount(), 0);
    CORRADE_COMPARE(mesh->attributeCount(), 2);
}

void StlImporterTest::emptyBinary() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StlImporter");

    constexpr const char data[]{
        /* 80-byte header */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

        0, 0, 0, 0, /* No triangles */
    };

    CORRADE_VERIFY(importer->openData(data));

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_COMPARE(mesh->vertexCount(), 0);
    CORRADE_COMPARE(mesh->attributeCount(), 2);
}

void StlImporterTest::binary() {
    auto&& data = BinaryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StlImporter");

    /* Set only if disabled, to test the default value as well */
    if(!data.perFaceToPerVertex)
        importer->configuration().setValue("perFaceToPerVertex", data.perFaceToPerVertex);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(STLIMPORTER_TEST_DIR, "binary.stl")));

    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->meshLevelCount(0), data.levelCount);

    Containers::Optional<MeshData> mesh = importer->mesh(0, data.level);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->primitive(), data.primitive);
    CORRADE_COMPARE(mesh->vertexCount(), data.vertexCount);
    CORRADE_COMPARE(mesh->attributeCount(), data.attributeCount);

    if(data.level == 0) {
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
            Containers::arrayView<Vector3>({
                {1.0f, 2.0f, 3.0f},
                {4.0f, 5.0f, 6.0f},
                {7.0f, 8.0f, 9.0f},

                {1.1f, 2.1f, 3.1f},
                {4.1f, 5.1f, 6.1f},
                {7.1f, 8.1f, 9.1f}
            }), TestSuite::Compare::Container);

        if(data.normals) {
            CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Normal), VertexFormat::Vector3);
            CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Normal),
                Containers::arrayView<Vector3>({
                    {0.1f, 0.2f, 0.3f},
                    {0.1f, 0.2f, 0.3f},
                    {0.1f, 0.2f, 0.3f},

                    {0.4f, 0.5f, 0.6f},
                    {0.4f, 0.5f, 0.6f},
                    {0.4f, 0.5f, 0.6f}
                }), TestSuite::Compare::Container);
        }
    } else if(data.level == 1) {
        CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Normal), VertexFormat::Vector3);
        CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Normal),
            Containers::arrayView<Vector3>({
                {0.1f, 0.2f, 0.3f},
                {0.4f, 0.5f, 0.6f}
            }), TestSuite::Compare::Container);
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

void StlImporterTest::asciiDelegateAssimp() {
    auto&& data = AsciiDelegateAssimpData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_managerWithAssimpImporter.loadState("AssimpImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AssimpImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _managerWithAssimpImporter.instantiate("StlImporter");

    /* Set flags to see if they get propagated */
    importer->setFlags(data.flags);

    Containers::String out;
    {
        Debug redirectDebug{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(STLIMPORTER_TEST_DIR, "ascii.stl")));
    }
    if(data.flags & ImporterFlag::Verbose)
        CORRADE_COMPARE_AS(out,
            "Trade::StlImporter::openData(): forwarding an ASCII file to AssimpImporter\n"
            "Trade::AssimpImporter: Info,  ",
            TestSuite::Compare::StringHasPrefix);
    else
        CORRADE_COMPARE(out, "");

    /* All of these should forward to the correct instance and not crash */
    CORRADE_COMPARE(importer->meshCount(), 1);
    CORRADE_COMPARE(importer->meshLevelCount(0), 1);

    /* The ASCII file has extra custom attributes but Assimp doesn't support
       those and ignores them, so it gives back nothing */
    CORRADE_COMPARE(importer->meshAttributeName(meshAttributeCustom(2)), "");
    CORRADE_COMPARE(importer->meshAttributeForName("id"), MeshAttribute{});

    Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->attributeCount(), 2);

    /* Assimp imports the mesh as indexed always */
    CORRADE_VERIFY(mesh->isIndexed());
    CORRADE_COMPARE(mesh->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE_AS(mesh->indices<UnsignedInt>(), Containers::arrayView({
        0u, 1u, 2u
    }), TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Position));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Position), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position), Containers::arrayView({
        Vector3{-0.5f, -0.5f, 0.0f},
        Vector3{+0.5f, -0.5f, 0.0f},
        Vector3{ 0.0f, +0.5f, 0.0f}
    }), TestSuite::Compare::Container);

    CORRADE_VERIFY(mesh->hasAttribute(MeshAttribute::Normal));
    CORRADE_COMPARE(mesh->attributeFormat(MeshAttribute::Normal), VertexFormat::Vector3);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Normal), Containers::arrayView({
        Vector3{0.0f, 0.0f, 1.0f},
        Vector3{0.0f, 0.0f, 1.0f},
        Vector3{0.0f, 0.0f, 1.0f}
    }), TestSuite::Compare::Container);

    /* Verify that closing works as intended as well */
    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void StlImporterTest::asciiDelegateAssimpNoPlugin() {
    /* Happens on builds with static plugins. Can't really do much there. */
    if(_manager.loadState("AssimpImporter") != PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AssimpImporter plugin loaded, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StlImporter");

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(STLIMPORTER_TEST_DIR, "ascii.stl")));
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    CORRADE_COMPARE(out,
        "PluginManager::Manager::load(): plugin AssimpImporter is not static and was not found in nonexistent\n"
        "Trade::StlImporter::openData(): can't forward an ASCII file to AssimpImporter\n");
    #else
    CORRADE_COMPARE(out,
        "PluginManager::Manager::load(): plugin AssimpImporter was not found\n"
        "Trade::StlImporter::openData(): can't forward an ASCII file to AssimpImporter\n");
    #endif
}

void StlImporterTest::asciiDelegateAssimpFailed() {
    if(_managerWithAssimpImporter.loadState("AssimpImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AssimpImporter plugin not found, cannot test");

    /* FFS, there's still no common way to detect whether a sanitizer is
       enabled?! Former is Clang only, latter is GCC-only. */
    #ifdef __has_feature
    #if __has_feature(address_sanitizer)
    #define _MAGNUM_ASAN_ENABLED
    #endif
    #elif defined(__SANITIZE_ADDRESS__)
    #define _MAGNUM_ASAN_ENABLED
    #endif
    #ifdef _MAGNUM_ASAN_ENABLED
    CORRADE_SKIP("Assimp leaks memory on STL import failure, not running this test with AddressSanitizer enabled.");
    #endif

    Containers::Pointer<AbstractImporter> importer = _managerWithAssimpImporter.instantiate("StlImporter");

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(
        /* A zoo of Assimp bugs:
           - If this is "solid broken\nfacet normal"_s, Assimp including 5.3.1
             leaks.
           - If this is "solid broken\nfacet! normal"_s, Assimp 5.3.1 no longer
             leaks, but Assimp 5.0.1 asserts and 4.1.0 still leaks. Same is for
             "solid \xff\n"_s, which was an attempt to convince Assimp that the
             file is neither binary nor ASCII because it contains non-ASCII
             characters.
           Fucking hell. Why is it so easy to run into nasty bugs in that
           library. To make this test work on as many versions as possible, I'm
           staying with the former but skipping this test when sanitizers are
           enabled. */
        "solid broken\nfacet normal"_s));
    CORRADE_COMPARE_AS(out,
        "Trade::AssimpImporter::openData(): loading failed: ",
        TestSuite::Compare::StringHasPrefix);
}

void StlImporterTest::openMemory() {
    /* Same as (a subset of) binary() except that it uses openData() &
       openMemory() instead of openFile() to test data copying on import */

    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StlImporter");
    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(STLIMPORTER_TEST_DIR, "binary.stl"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*importer, *memory));
    CORRADE_COMPARE(importer->meshCount(), 1);

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE_AS(mesh->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {1.0f, 2.0f, 3.0f},
            {4.0f, 5.0f, 6.0f},
            {7.0f, 8.0f, 9.0f},

            {1.1f, 2.1f, 3.1f},
            {4.1f, 5.1f, 6.1f},
            {7.1f, 8.1f, 9.1f}
        }), TestSuite::Compare::Container);
}

void StlImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StlImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(STLIMPORTER_TEST_DIR, "binary.stl")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(STLIMPORTER_TEST_DIR, "binary.stl")));

    /* Shouldn't crash, leak or anything */
}

void StlImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StlImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(STLIMPORTER_TEST_DIR, "binary.stl")));

    /* Verify that everything is working the same way on second use */
    {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->vertexCount(), 6);
    } {
        Containers::Optional<Trade::MeshData> mesh = importer->mesh(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->vertexCount(), 6);
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StlImporterTest)
