/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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

#include <Corrade/TestSuite/Tester.h>
#include <Magnum/Math/Vector3.h>

#include "MagnumPlugins/ColladaImporter/Utility.h"

namespace Magnum { namespace Trade { namespace Test {

struct UtilityTest: TestSuite::Tester {
    explicit UtilityTest();

    void parseVector();
    void parseArray();
};

UtilityTest::UtilityTest() {
    addTests({&UtilityTest::parseVector,
              &UtilityTest::parseArray});
}

void UtilityTest::parseVector() {
    /* Extremely spaceless */
    CORRADE_COMPARE((Implementation::Utility::parseVector<Math::Vector<1, Float>>("3.14")[0]), 3.14f);

    /* Spaces */
    CORRADE_COMPARE(Implementation::Utility::parseVector<Vector2>("     2.17  3.28  "), Vector2(2.17f, 3.28f));

    /* Nothing */
    CORRADE_COMPARE(Implementation::Utility::parseVector<Vector2>({}), Vector2());
    CORRADE_COMPARE(Implementation::Utility::parseVector<Vector2>("    "), Vector2());

    /* Different size */
    CORRADE_COMPARE(Implementation::Utility::parseVector<Vector3>("2.17 3.28"), Vector3(2.17f, 3.28f, 0.0f));
    CORRADE_COMPARE(Implementation::Utility::parseVector<Vector3>("2.17 3.28 5.15", 2), Vector3(2.17f, 3.28f, 0.0f));

    /* Moving from value */
    int from;
    Implementation::Utility::parseVector<Vector2>("2.17 3.28", &(from = 0));
    CORRADE_COMPARE(from, 9);

    /* Different start */
    CORRADE_COMPARE(Implementation::Utility::parseVector<Vector2>("2.17 3.28", &(from = 5)), Vector2(3.28f, 0.0f));
    CORRADE_COMPARE(from, 9);
}

void UtilityTest::parseArray() {
    /* Extremely spaceless */
    CORRADE_COMPARE(Implementation::Utility::parseArray<Float>("3.14", 1), (std::vector<Float>{3.14f}));

    /* Spaces */
    CORRADE_COMPARE(Implementation::Utility::parseArray<Float>("     2.17  3.28  ", 2), (std::vector<Float>{2.17f, 3.28f}));

    /* Different size */
    CORRADE_COMPARE(Implementation::Utility::parseArray<Float>("2.17 3.28", 3), (std::vector<Float>{2.17f, 3.28f, 0.0f}));
    CORRADE_COMPARE(Implementation::Utility::parseArray<Float>("2.17 3.28 5.15", 2), (std::vector<Float>{2.17f, 3.28f}));
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::UtilityTest)
