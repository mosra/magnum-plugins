/*
    Copyright © 2010, 2011, 2012 Vladimír Vondruš <mosra@centrum.cz>

    This file is part of Magnum.

    Magnum is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License version 3
    only, as published by the Free Software Foundation.

    Magnum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License version 3 for more details.
*/

#include "ColladaImporterTest.h"

#include <QtTest/QTest>

#include "Utility/Directory.h"
#include "../ColladaImporter.h"

#include "ColladaImporterTestConfigure.h"

using namespace std;
using namespace Corrade::Utility;

QTEST_APPLESS_MAIN(Magnum::Trade::ColladaImporter::Test::ColladaImporterTest)

namespace Magnum { namespace Trade { namespace ColladaImporter { namespace Test {

void ColladaImporterTest::openWrongNamespace() {
    ColladaImporter importer;
    stringstream debug;
    Error::setOutput(&debug);
    QVERIFY(!importer.open(Directory::join(COLLADAIMPORTER_TEST_DIR, "openWrongNamespace.dae")));
    QVERIFY(debug.str() == "ColladaImporter: unsupported namespace \"http://www.collada.org/2006/11/COLLADASchema\"\n");
}

void ColladaImporterTest::openWrongVersion() {
    ColladaImporter importer;
    stringstream debug;
    Error::setOutput(&debug);
    QVERIFY(!importer.open(Directory::join(COLLADAIMPORTER_TEST_DIR, "openWrongVersion.dae")));
    QVERIFY(debug.str() == "ColladaImporter: unsupported version \"1.4.0\"\n");
}

void ColladaImporterTest::count() {
    ColladaImporter importer;
    QVERIFY(importer.open(Directory::join(COLLADAIMPORTER_TEST_DIR, "count.dae")));

    QVERIFY(importer.materialCount() == 4);
    QVERIFY(importer.objectCount() == 5);
    QVERIFY(importer.meshCount() == 5); /* Meshes are the same as objects */

    QEXPECT_FAIL(0, "Cameras are not implemented", Continue);
    QVERIFY(importer.cameraCount() == 3);
    QEXPECT_FAIL(0, "Lights are not implemented", Continue);
    QVERIFY(importer.lightCount() == 2);
    QEXPECT_FAIL(0, "Scenes are not implemented", Continue);
    QVERIFY(importer.sceneCount() == 1);
}

void ColladaImporterTest::parseSource() {
    ColladaImporter importer;
    QVERIFY(importer.open(Directory::join(COLLADAIMPORTER_TEST_DIR, "parseSource.dae")));

    stringstream debug;
    Error::setOutput(&debug);
    QVERIFY(importer.parseSource<Vector3>("WrongTotalCount") == vector<Vector3>());
    QVERIFY(debug.str() == "ColladaImporter: wrong total count in source \"WrongTotalCount\"\n");

    QEXPECT_FAIL(0, "Swapped coordinates in source are not implemented", Continue);
    QVERIFY(importer.parseSource<Vector3>("SwappedCoords") == vector<Vector3>{Vector3(0, 1, 2)});

    QVERIFY((importer.parseSource<Vector4>("MoreElements") == vector<Vector4>{
        Vector4(0, 1, 2),
        Vector4(3, 4, 5)
    }));
}

void ColladaImporterTest::material() {
    ColladaImporter importer;
    QVERIFY(importer.open(Directory::join(COLLADAIMPORTER_TEST_DIR, "material.dae")));

    stringstream debug;
    Error::setOutput(&debug);
    QVERIFY(!importer.material(0));
    QVERIFY(debug.str() == "ColladaImporter: \"profile_GLSL\" effect profile not supported\n");

    debug.str("");
    QVERIFY(!importer.material(1));
    QVERIFY(debug.str() == "ColladaImporter: \"lambert\" shader not supported\n");

    /** @todo Wrong way. */
//     QVERIFY(!!importer.material(2));
//     Material* material = static_cast<Material*>(importer.material(2).get());
//
//     QVERIFY(material->ambientColor == Vector3(1, 0, 0));
//     QVERIFY(material->diffuseColor == Vector3(0, 1, 0));
//     QVERIFY(material->specularColor == Vector3(0, 0, 1));
//     QCOMPARE(material->shininess, 50.0f);
}

}}}}
