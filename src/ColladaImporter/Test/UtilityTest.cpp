/*
    Copyright © 2010, 2011 Vladimír Vondruš <mosra@centrum.cz>

    This file is part of Magnum.

    Magnum is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License version 3
    only, as published by the Free Software Foundation.

    Magnum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License version 3 for more details.
*/

#include "UtilityTest.h"

#include <QtTest/QTest>

#include "Magnum.h"
#include "../Utility.h"

QTEST_APPLESS_MAIN(Magnum::Plugins::ColladaImporter::Test::UtilityTest)

using namespace std;

namespace Magnum { namespace Plugins { namespace ColladaImporter { namespace Test {

void UtilityTest::parseVector() {
    /* Extremely spaceless */
    QCOMPARE((Utility::parseVector<Math::Vector<GLfloat, 1>>("3.14")[0]), 3.14f);

    /* Spaces */
    QVERIFY(Utility::parseVector<Vector2>("  \t  2.17\r\n3.28  ") == Vector2(2.17f, 3.28f));

    /* Nothing */
    QVERIFY(Utility::parseVector<Vector2>("") == Vector2());
    QVERIFY(Utility::parseVector<Vector2>("    ") == Vector2());

    /* Different size */
    QVERIFY(Utility::parseVector<Vector3>("2.17 3.28") == Vector3(2.17f, 3.28f, 0.0f));
    QVERIFY(Utility::parseVector<Vector3>("2.17 3.28 5.15", 2) == Vector3(2.17f, 3.28f, 0.0f));

    /* Moving from value */
    int from;
    Utility::parseVector<Vector2>("2.17 3.28", &(from = 0));
    QCOMPARE(from, 9);

    /* Different start */
    QVERIFY(Utility::parseVector<Vector2>("2.17 3.28", &(from = 5)) == Vector2(3.28f, 0.0f));
    QCOMPARE(from, 9);
}

void UtilityTest::parseArray() {
    /* Extremely spaceless */
    QVERIFY((Utility::parseArray<GLfloat>("3.14", 1) == vector<GLfloat>{3.14f}));

    /* Spaces */
    QVERIFY((Utility::parseArray<GLfloat>("  \t  2.17\r\n3.28  ", 2) == vector<GLfloat>{2.17f, 3.28f}));

    /* Different size */
    QVERIFY((Utility::parseArray<GLfloat>("2.17 3.28", 3) == vector<GLfloat>{2.17f, 3.28f, 0.0f}));
    QVERIFY((Utility::parseArray<GLfloat>("2.17 3.28 5.15", 2) == vector<GLfloat>{2.17f, 3.28f}));
}

}}}}
