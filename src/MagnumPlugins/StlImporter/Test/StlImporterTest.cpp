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
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct StlImporterTest: TestSuite::Tester {
    explicit StlImporterTest();

    void invalid();
    void ascii();
    void almostAsciiButNotActually();
    void emptyBinary();
    void binary();

    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

constexpr const char data[]{
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
    {"four bytes of an ASCII file", Containers::arrayView("soli").except(1),
        "file too short, got only 4 bytes"},
    {"83 bytes", Containers::arrayView(data).prefix(83),
        "file too short, expected at least 84 bytes but got 83"},
    {"too short", Containers::arrayView(data).except(2),
        "file size doesn't match triangle count, expected 234 but got 233 for 3 triangles"},
    {"too long", Containers::arrayView(data),
        "file size doesn't match triangle count, expected 234 but got 235 for 3 triangles"}
};

StlImporterTest::StlImporterTest() {
    addInstancedTests({&StlImporterTest::invalid},
        Containers::arraySize(InvalidData));

    addTests({&StlImporterTest::ascii,
              &StlImporterTest::almostAsciiButNotActually,
              &StlImporterTest::emptyBinary,
              &StlImporterTest::binary});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STLIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(STLIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void StlImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StlImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(data.data));
    CORRADE_COMPARE(out.str(),
        Utility::formatString("Trade::StlImporter::openData(): {}\n", data.message));
}

void StlImporterTest::ascii() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StlImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(STLIMPORTER_TEST_DIR, "ascii.stl")));
    CORRADE_COMPARE(out.str(), "Trade::StlImporter::openData(): ASCII STL files are not supported, sorry\n");
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
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StlImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STLIMPORTER_TEST_DIR, "binary.stl")));

    Containers::Optional<MeshData> mesh = importer->mesh(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_COMPARE(mesh->primitive(), MeshPrimitive::Triangles);
    CORRADE_COMPARE(mesh->vertexCount(), 6);
    CORRADE_COMPARE(mesh->attributeCount(), 2);

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

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StlImporterTest)
