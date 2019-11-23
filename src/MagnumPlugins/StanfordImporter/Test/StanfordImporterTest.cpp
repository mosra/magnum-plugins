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
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData3D.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct StanfordImporterTest: TestSuite::Tester {
    explicit StanfordImporterTest();

    void invalidSignature();

    void formatInvalid();
    void formatUnsupported();
    void formatMissing();
    void formatTooLate();

    void unknownLine();
    void unknownElement();

    void unexpectedProperty();
    void invalidVertexProperty();
    void invalidVertexType();
    void invalidFaceProperty();
    void invalidFaceType();
    void invalidFaceSizeType();
    void invalidFaceIndexType();

    void incompleteVertex();
    void incompleteFace();

    void fileDoesNotExist();
    void fileTooShort();
    void invalidFaceSize();

    void openFile();
    void openData();
    void empty();
    void bigEndian();
    void crlf();
    void ignoredVertexComponents();
    void ignoredFaceComponents();

    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

StanfordImporterTest::StanfordImporterTest() {
    addTests({&StanfordImporterTest::invalidSignature,

              &StanfordImporterTest::formatInvalid,
              &StanfordImporterTest::formatUnsupported,
              &StanfordImporterTest::formatMissing,
              &StanfordImporterTest::formatTooLate,

              &StanfordImporterTest::unknownLine,
              &StanfordImporterTest::unknownElement,

              &StanfordImporterTest::unexpectedProperty,
              &StanfordImporterTest::invalidVertexProperty,
              &StanfordImporterTest::invalidVertexType,
              &StanfordImporterTest::invalidFaceProperty,
              &StanfordImporterTest::invalidFaceType,
              &StanfordImporterTest::invalidFaceSizeType,
              &StanfordImporterTest::invalidFaceIndexType,

              &StanfordImporterTest::incompleteVertex,
              &StanfordImporterTest::incompleteFace,

              &StanfordImporterTest::fileDoesNotExist,
              &StanfordImporterTest::fileTooShort,
              &StanfordImporterTest::invalidFaceSize,

              &StanfordImporterTest::openFile,
              &StanfordImporterTest::openData,
              &StanfordImporterTest::empty,
              &StanfordImporterTest::bigEndian,
              &StanfordImporterTest::crlf,
              &StanfordImporterTest::ignoredVertexComponents,
              &StanfordImporterTest::ignoredFaceComponents,

              &StanfordImporterTest::openTwice,
              &StanfordImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STANFORDIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(STANFORDIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void StanfordImporterTest::invalidSignature() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "invalid-signature.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): invalid file signature bla\n");
}

void StanfordImporterTest::formatInvalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "format-invalid.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): invalid format line format binary_big_endian 1.0 extradata\n");
}

void StanfordImporterTest::formatUnsupported() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "format-unsupported.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): unsupported file format ascii 1.0\n");
}

void StanfordImporterTest::formatMissing() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "format-missing.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): missing format line\n");
}

void StanfordImporterTest::formatTooLate() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "format-too-late.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): expected format line, got element face 1\n");
}

void StanfordImporterTest::unknownLine() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "unknown-line.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): unknown line heh\n");
}

void StanfordImporterTest::unknownElement() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "unknown-element.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): unknown element edge\n");
}

void StanfordImporterTest::unexpectedProperty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "unexpected-property.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): unexpected property line\n");
}

void StanfordImporterTest::invalidVertexProperty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "invalid-vertex-property.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): invalid vertex property line property float x extradata\n");
}

void StanfordImporterTest::invalidVertexType() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "invalid-vertex-type.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): invalid vertex component type float16\n");
}

void StanfordImporterTest::invalidFaceProperty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "invalid-face-property.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): invalid face property line property float x extradata\n");
}

void StanfordImporterTest::invalidFaceType() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "invalid-face-type.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): invalid face component type float16\n");
}

void StanfordImporterTest::invalidFaceSizeType() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "invalid-face-size-type.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): invalid face size type int128\n");
}

void StanfordImporterTest::invalidFaceIndexType() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "invalid-face-index-type.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): invalid face index type int128\n");
}

void StanfordImporterTest::incompleteVertex() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "incomplete-vertex.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): incomplete vertex specification\n");
}

void StanfordImporterTest::incompleteFace() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "incomplete-face.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): incomplete face specification\n");
}

void StanfordImporterTest::fileDoesNotExist() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile("nonexistent.ply"));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::openFile(): cannot open file nonexistent.ply\n");
}

void StanfordImporterTest::fileTooShort() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "short-file.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): file is too short\n");
}

void StanfordImporterTest::invalidFaceSize() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "invalid-face-size.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): unsupported face size 5\n");
}

/*
    First face is quad, second is triangle.

    0--3--4
    |\ | /
    | \|/
    1--2
*/
const std::vector<UnsignedInt> indices{0, 1, 2, 0, 2, 3, 3, 2, 4};
const std::vector<Vector3> positions{
    {1.0f, 3.0f, 2.0f},
    {1.0f, 1.0f, 2.0f},
    {3.0f, 3.0f, 2.0f},
    {3.0f, 1.0f, 2.0f},
    {5.0f, 3.0f, 9.0f}
};

void StanfordImporterTest::openFile() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "common.ply")));

    auto mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->indices(), indices);
    CORRADE_COMPARE(mesh->positions(0), positions);
}

void StanfordImporterTest::openData() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "common.ply"))));

    auto mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->indices(), indices);
    CORRADE_COMPARE(mesh->positions(0), positions);
}

void StanfordImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "empty.ply")));

    auto mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_VERIFY(mesh->positions(0).empty());
}

void StanfordImporterTest::bigEndian() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "big-endian.ply")));

    auto mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->indices(), indices);
    CORRADE_COMPARE(mesh->positions(0), positions);
}

void StanfordImporterTest::crlf() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "crlf.ply")));

    auto mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->indices(), indices);
    CORRADE_COMPARE(mesh->positions(0), positions);
}

void StanfordImporterTest::ignoredVertexComponents() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "ignored-vertex-components.ply")));

    auto mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->indices(), indices);
    CORRADE_COMPARE(mesh->positions(0), positions);
}

void StanfordImporterTest::ignoredFaceComponents() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "ignored-face-components.ply")));

    auto mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->indices(), indices);
    CORRADE_COMPARE(mesh->positions(0), positions);
}

void StanfordImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "common.ply")));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "common.ply")));

    /* Shouldn't crash, leak or anything */
}

void StanfordImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "common.ply")));

    /* Verify that everything is working the same way on second use */
    {
        Containers::Optional<Trade::MeshData3D> mesh = importer->mesh3D(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->positions(0), positions);
    } {
        Containers::Optional<Trade::MeshData3D> mesh = importer->mesh3D(0);
        CORRADE_VERIFY(mesh);
        CORRADE_COMPARE(mesh->positions(0), positions);
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StanfordImporterTest)
