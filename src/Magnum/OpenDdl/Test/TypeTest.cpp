/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/DebugStl.h>

#include "Magnum/OpenDdl/Type.h"

namespace Magnum { namespace OpenDdl { namespace Test { namespace {

struct TypeTest: TestSuite::Tester {
    explicit TypeTest();

    void debugType();
    void debugPropertyType();
    void debugInternalPropertyType();
};

TypeTest::TypeTest() {
    addTests({&TypeTest::debugType,
              &TypeTest::debugPropertyType,
              &TypeTest::debugInternalPropertyType});
}

void TypeTest::debugType() {
    std::ostringstream out;
    Debug{&out} << Type::Bool << Type(0xdeadbabe);
    CORRADE_COMPARE(out.str(), "OpenDdl::Type::Bool OpenDdl::Type(0xdeadbabe)\n");
}

void TypeTest::debugPropertyType() {
    std::ostringstream out;
    Debug{&out} << PropertyType::Bool << PropertyType(0xbe);
    CORRADE_COMPARE(out.str(), "OpenDdl::PropertyType::Bool OpenDdl::PropertyType(0xbe)\n");
}

void TypeTest::debugInternalPropertyType() {
    std::ostringstream out;
    Debug{&out} << Implementation::InternalPropertyType::Bool << Implementation::InternalPropertyType(0xbe);
    CORRADE_COMPARE(out.str(), "OpenDdl::Implementation::InternalPropertyType::Bool OpenDdl::Implementation::InternalPropertyType(0xbe)\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::OpenDdl::Test::TypeTest)
