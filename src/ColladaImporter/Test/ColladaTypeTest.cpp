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

#include "ColladaTypeTest.h"

#include <QtTest/QTest>

#include "../ColladaType.h"

QTEST_APPLESS_MAIN(Magnum::Trade::ColladaImporter::Test::ColladaTypeTest)

namespace Magnum { namespace Trade { namespace ColladaImporter { namespace Test {

void ColladaTypeTest::gluint() {
    QVERIFY(ColladaType<GLuint>::fromString("123456") == 123456);
}

void ColladaTypeTest::glfloat() {
    QCOMPARE(ColladaType<GLfloat>::fromString("3.14"), 3.14f);
}

}}}}
