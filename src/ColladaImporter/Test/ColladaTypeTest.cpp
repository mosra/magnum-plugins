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

#include <TestSuite/Tester.h>

#include "../ColladaType.h"

namespace Magnum { namespace Trade { namespace ColladaImporter { namespace Test {

class ColladaTypeTest: public Corrade::TestSuite::Tester {
    public:
        ColladaTypeTest();

        void gluint();
        void glfloat();
};

ColladaTypeTest::ColladaTypeTest() {
    addTests(&ColladaTypeTest::gluint,
             &ColladaTypeTest::glfloat);
}

void ColladaTypeTest::gluint() {
    CORRADE_COMPARE(ColladaType<UnsignedInt>::fromString("123456"), 123456);
}

void ColladaTypeTest::glfloat() {
    CORRADE_COMPARE(ColladaType<Float>::fromString("3.14"), 3.14f);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::ColladaImporter::Test::ColladaTypeTest)
