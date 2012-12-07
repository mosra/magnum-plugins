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

#include "UtilityTest.h"

#include <Math/Vector3.h>
#include "../Utility.h"

CORRADE_TEST_MAIN(Magnum::Trade::ColladaImporter::Test::UtilityTest)

namespace Magnum { namespace Trade { namespace ColladaImporter { namespace Test {

UtilityTest::UtilityTest() {
    addTests(&UtilityTest::parseVector,
             &UtilityTest::parseArray);
}

void UtilityTest::parseVector() {
    /* Extremely spaceless */
    CORRADE_COMPARE((Utility::parseVector<Math::Vector<1, GLfloat>>("3.14")[0]), 3.14f);

    /* Spaces */
    CORRADE_COMPARE(Utility::parseVector<Vector2>("     2.17  3.28  "), Vector2(2.17f, 3.28f));

    /* Nothing */
    CORRADE_COMPARE(Utility::parseVector<Vector2>(""), Vector2());
    CORRADE_COMPARE(Utility::parseVector<Vector2>("    "), Vector2());

    /* Different size */
    CORRADE_COMPARE(Utility::parseVector<Vector3>("2.17 3.28"), Vector3(2.17f, 3.28f, 0.0f));
    CORRADE_COMPARE(Utility::parseVector<Vector3>("2.17 3.28 5.15", 2), Vector3(2.17f, 3.28f, 0.0f));

    /* Moving from value */
    int from;
    Utility::parseVector<Vector2>("2.17 3.28", &(from = 0));
    CORRADE_COMPARE(from, 9);

    /* Different start */
    CORRADE_COMPARE(Utility::parseVector<Vector2>("2.17 3.28", &(from = 5)), Vector2(3.28f, 0.0f));
    CORRADE_COMPARE(from, 9);
}

void UtilityTest::parseArray() {
    /* Extremely spaceless */
    CORRADE_COMPARE(Utility::parseArray<float>("3.14", 1), (std::vector<float>{3.14f}));

    /* Spaces */
    CORRADE_COMPARE(Utility::parseArray<float>("     2.17  3.28  ", 2), (std::vector<float>{2.17f, 3.28f}));

    /* Different size */
    CORRADE_COMPARE(Utility::parseArray<float>("2.17 3.28", 3), (std::vector<float>{2.17f, 3.28f, 0.0f}));
    CORRADE_COMPARE(Utility::parseArray<float>("2.17 3.28 5.15", 2), (std::vector<float>{2.17f, 3.28f}));
}

}}}}
