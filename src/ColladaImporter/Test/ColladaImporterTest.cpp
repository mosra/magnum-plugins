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

QTEST_APPLESS_MAIN(Magnum::Plugins::ColladaImporter::Test::ColladaImporterTest)

namespace Magnum { namespace Plugins { namespace ColladaImporter { namespace Test {

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

}}}}
