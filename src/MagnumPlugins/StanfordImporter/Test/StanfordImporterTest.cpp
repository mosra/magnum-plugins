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

    void invalidFormat();
    void unsupportedFormat();
    void missingFormat();

    void unknownLine();
    void unknownElement();

    void unexpectedProperty();
    void invalidVertexProperty();
    void invalidVertexType();
    void unknownFaceProperty();
    void invalidFaceSizeType();
    void invalidFaceIndexType();

    void incompleteVertex();
    void incompleteFace();

    void invalidFaceSize();
    void shortFile();

    void empty();
    void common();
    void bigEndian();
    void crlf();
    void ignoredVertexComponents();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

StanfordImporterTest::StanfordImporterTest() {
    addTests({&StanfordImporterTest::invalidSignature,

              &StanfordImporterTest::invalidFormat,
              &StanfordImporterTest::unsupportedFormat,
              &StanfordImporterTest::missingFormat,

              &StanfordImporterTest::unknownLine,
              &StanfordImporterTest::unknownElement,

              &StanfordImporterTest::unexpectedProperty,
              &StanfordImporterTest::invalidVertexProperty,
              &StanfordImporterTest::invalidVertexType,
              &StanfordImporterTest::unknownFaceProperty,
              &StanfordImporterTest::invalidFaceSizeType,
              &StanfordImporterTest::invalidFaceIndexType,

              &StanfordImporterTest::incompleteVertex,
              &StanfordImporterTest::incompleteFace,

              &StanfordImporterTest::invalidFaceSize,
              &StanfordImporterTest::shortFile,

              &StanfordImporterTest::empty,
              &StanfordImporterTest::common,
              &StanfordImporterTest::bigEndian,
              &StanfordImporterTest::crlf,
              &StanfordImporterTest::ignoredVertexComponents});

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

void StanfordImporterTest::invalidFormat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "invalid-format.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): invalid format line format binary_big_endian 1.0 extradata\n");
}

void StanfordImporterTest::unsupportedFormat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "unsupported-format.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): unsupported file format ascii 1.0\n");
}

void StanfordImporterTest::missingFormat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "missing-format.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): missing format line\n");
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

void StanfordImporterTest::unknownFaceProperty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "unknown-face-property.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): unknown face property line property float x\n");
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

void StanfordImporterTest::invalidFaceSize() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "invalid-face-size.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): unsupported face size 5\n");
}

void StanfordImporterTest::shortFile() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "short-file.ply")));
    CORRADE_VERIFY(!importer->mesh3D(0));
    CORRADE_COMPARE(out.str(), "Trade::StanfordImporter::mesh3D(): file is too short\n");
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

void StanfordImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "empty.ply")));

    auto mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_VERIFY(!mesh->isIndexed());
    CORRADE_VERIFY(mesh->positions(0).empty());
}

void StanfordImporterTest::common() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StanfordImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STANFORDIMPORTER_TEST_DIR, "common.ply")));

    auto mesh = importer->mesh3D(0);
    CORRADE_VERIFY(mesh);
    CORRADE_COMPARE(mesh->indices(), indices);
    CORRADE_COMPARE(mesh->positions(0), positions);
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

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StanfordImporterTest)
