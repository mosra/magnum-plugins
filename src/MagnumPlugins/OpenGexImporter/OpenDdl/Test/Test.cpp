/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015
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
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>

#include "MagnumPlugins/OpenGexImporter/OpenDdl/Document.h"
#include "MagnumPlugins/OpenGexImporter/OpenDdl/Property.h"
#include "MagnumPlugins/OpenGexImporter/OpenDdl/Structure.h"

namespace Magnum { namespace OpenDdl { namespace Test {

struct Test: TestSuite::Tester {
    explicit Test();

    void primitive();
    void primitiveEmpty();
    void primitiveName();
    void primitiveExpectedListStart();
    void primitiveExpectedListEnd();
    void primitiveExpectedSeparator();

    void primitiveSubArray();
    void primitiveSubArrayEmpty();
    void primitiveSubArrayName();
    void primitiveSubArrayExpectedArraySizeEnd();
    void primitiveSubArrayExpectedSubSeparator();
    void primitiveSubArrayExpectedSubListEnd();
    void primitiveSubArrayExpectedSeparator();

    void custom();
    void customEmpty();
    void customUnknown();
    void customName();
    void customInvalidIdentifier();
    void customExpectedListStart();
    void customExpectedListEnd();

    void customProperty();
    void customPropertyEmpty();
    void customPropertyUnknown();
    void customPropertyExpectedValueAssignment();
    void customPropertyExpectedSeparator();
    void customPropertyExpectedListEnd();
    void customPropertyInvalidIdentifier();
    void customPropertyInvalidValue();

    void hierarchy();
};

Test::Test() {
    addTests({&Test::primitive,
              &Test::primitiveEmpty,
              &Test::primitiveName,
              &Test::primitiveExpectedListStart,
              &Test::primitiveExpectedListEnd,
              &Test::primitiveExpectedSeparator,

              &Test::primitiveSubArray,
              &Test::primitiveSubArrayEmpty,
              &Test::primitiveSubArrayName,
              &Test::primitiveSubArrayExpectedArraySizeEnd,
              &Test::primitiveSubArrayExpectedSubSeparator,
              &Test::primitiveSubArrayExpectedSubListEnd,
              &Test::primitiveSubArrayExpectedSeparator,

              &Test::custom,
              &Test::customEmpty,
              &Test::customUnknown,
              &Test::customName,
              &Test::customInvalidIdentifier,
              &Test::customExpectedListStart,
              &Test::customExpectedListEnd,

              &Test::customProperty,
              &Test::customPropertyEmpty,
              &Test::customPropertyExpectedValueAssignment,
              &Test::customPropertyExpectedSeparator,
              &Test::customPropertyExpectedListEnd,
              &Test::customPropertyInvalidValue,

              &Test::hierarchy});
}

void Test::primitive() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"int16 { 35, -'\\x0c', 45 }"}, {}, {}));
    CORRADE_VERIFY(!d.isEmpty());

    Structure s = d.firstChild();
    CORRADE_VERIFY(!s.isCustom());
    CORRADE_COMPARE(s.type(), Type::Short);
    CORRADE_COMPARE(s.arraySize(), 3);
    CORRADE_COMPARE(s.subArraySize(), 0);
    CORRADE_COMPARE_AS(s.asArray<Short>(),
        Containers::Array<Short>::from(35, -0x0c, 45),
        TestSuite::Compare::Container);
}

void Test::primitiveEmpty() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"float {}"}, {}, {}));
    CORRADE_VERIFY(!d.isEmpty());

    Structure s = d.firstChild();
    CORRADE_VERIFY(!s.isCustom());
    CORRADE_COMPARE(s.type(), Type::Float);
    CORRADE_COMPARE(s.name(), "");
    CORRADE_COMPARE(s.arraySize(), 0);
}

void Test::primitiveName() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"float %name {}"}, {}, {}));
    CORRADE_VERIFY(!d.isEmpty());
    CORRADE_COMPARE(d.firstChild().name(), "%name");
}

void Test::primitiveExpectedListStart() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"float 35"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected { character on line 1\n");
}

void Test::primitiveExpectedListEnd() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"float { 35"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected } character on line 1\n");
}

void Test::primitiveExpectedSeparator() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"float { 35 45"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected , character on line 1\n");
}

void Test::primitiveSubArray() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"unsigned_int8[2] { {0xca, 0xfe}, {0xba, 0xbe} }"}, {}, {}));
    CORRADE_VERIFY(!d.isEmpty());

    Structure s = d.firstChild();
    CORRADE_VERIFY(!s.isCustom());
    CORRADE_COMPARE(s.type(), Type::UnsignedByte);
    CORRADE_COMPARE(s.name(), "");
    CORRADE_COMPARE(s.arraySize(), 4);
    CORRADE_COMPARE(s.subArraySize(), 2);
    CORRADE_COMPARE_AS(s.asArray<UnsignedByte>(),
        Containers::Array<UnsignedByte>::from(0xca, 0xfe, 0xba, 0xbe),
        TestSuite::Compare::Container);
}

void Test::primitiveSubArrayEmpty() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"unsigned_int8[2] {}"}, {}, {}));
    CORRADE_VERIFY(!d.isEmpty());

    Structure s = d.firstChild();
    CORRADE_VERIFY(!s.isCustom());
    CORRADE_COMPARE(s.type(), Type::UnsignedByte);
    CORRADE_COMPARE(s.name(), "");
    CORRADE_COMPARE(s.arraySize(), 0);
    CORRADE_COMPARE(s.subArraySize(), 2);
}

void Test::primitiveSubArrayName() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"unsigned_int8[2] $name {}"}, {}, {}));
    CORRADE_VERIFY(!d.isEmpty());
    CORRADE_COMPARE(d.firstChild().name(), "$name");
}

void Test::primitiveSubArrayExpectedArraySizeEnd() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"unsigned_int8[2 {"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected ] character on line 1\n");
}

void Test::primitiveSubArrayExpectedSubSeparator() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"unsigned_int8[2] { {0xca, 0xfe} {0xba"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected , character on line 1\n");
}

void Test::primitiveSubArrayExpectedSubListEnd() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"int32[2] { {0xca, 0xfe, 0xba"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected } character on line 1\n");
}

void Test::primitiveSubArrayExpectedSeparator() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"double[2] { {35 45"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected , character on line 1\n");
}

namespace {

enum: Int {
    SomeStructure,
    RootStructure,
    HierarchicStructure
};

const std::initializer_list<CharacterLiteral> structureIdentifiers {
    "Some",
    "Root",
    "Hierarchic"
};

enum: Int {
    SomeProperty,
    BooleanProperty,
    ReferenceProperty
};

const std::initializer_list<CharacterLiteral> propertyIdentifiers {
    "some",
    "boolean",
    "reference"
};

}

void Test::custom() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"Root { string {\"hello\"} }"}, structureIdentifiers, {}));
    CORRADE_VERIFY(!d.isEmpty());

    Structure s = d.firstChild();
    CORRADE_VERIFY(s.isCustom());
    CORRADE_COMPARE(s.identifier(), RootStructure);
    CORRADE_COMPARE(s.name(), "");
    CORRADE_VERIFY(s.hasChildren());
    Structure c = s.firstChild();
    CORRADE_VERIFY(!c.isCustom());
    CORRADE_COMPARE(c.type(), Type::String);
    CORRADE_COMPARE(c.as<std::string>(), "hello");
}

void Test::customEmpty() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"Some {}"}, structureIdentifiers, {}));
    CORRADE_VERIFY(!d.isEmpty());

    Structure s = d.firstChild();
    CORRADE_VERIFY(s.isCustom());
    CORRADE_COMPARE(s.identifier(), SomeStructure);
    CORRADE_VERIFY(!s.hasChildren());
}

void Test::customUnknown() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"UnspecifiedStructure {}"}, structureIdentifiers, {}));
    CORRADE_VERIFY(!d.isEmpty());

    Structure s = d.firstChild();
    CORRADE_VERIFY(s.isCustom());
    CORRADE_COMPARE(s.identifier(), UnknownIdentifier);
    CORRADE_VERIFY(!s.hasChildren());
}

void Test::customName() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"Some %some_name {}"}, structureIdentifiers, {}));
    CORRADE_VERIFY(!d.isEmpty());
    CORRADE_COMPARE(d.firstChild().name(), "%some_name");
}

void Test::customInvalidIdentifier() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"%name { string"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): invalid identifier on line 1\n");
}

void Test::customExpectedListStart() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root string"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected { character on line 1\n");
}

void Test::customExpectedListEnd() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root { "}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected } character on line 1\n");
}

void Test::customProperty() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"Root %some_name (boolean = true, some = 15.3) {}"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_VERIFY(!d.isEmpty());

    Structure s = d.firstChild();
    CORRADE_VERIFY(s.isCustom());
    CORRADE_COMPARE(s.identifier(), RootStructure);
    CORRADE_COMPARE(s.name(), "%some_name");
    CORRADE_COMPARE(s.propertyCount(), 2);

    std::optional<Property> p1 = s.propertyOf(BooleanProperty);
    CORRADE_VERIFY(p1);
    CORRADE_VERIFY(p1->isTypeCompatibleWith(PropertyType::Bool));
    CORRADE_COMPARE(p1->identifier(), BooleanProperty);
    CORRADE_COMPARE(p1->as<bool>(), true);

    std::optional<Property> p2 = s.propertyOf(SomeProperty);
    CORRADE_VERIFY(p2);
    CORRADE_VERIFY(p2->isTypeCompatibleWith(PropertyType::Float));
    CORRADE_COMPARE(p2->identifier(), SomeProperty);
    CORRADE_COMPARE(p2->as<Float>(), 15.3f);
}

void Test::customPropertyEmpty() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"Root () {}"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_VERIFY(!d.isEmpty());

    Structure s = d.firstChild();
    CORRADE_VERIFY(s.isCustom());
    CORRADE_VERIFY(!s.hasProperties());
}

void Test::customPropertyUnknown() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"Root (unspecified = %hello) {}"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_VERIFY(!d.isEmpty());

    Structure s = d.firstChild();
    CORRADE_VERIFY(s.isCustom());
    CORRADE_COMPARE(s.propertyCount(), 1);

    std::optional<Property> p1 = s.findPropertyOf(UnknownIdentifier);
    CORRADE_VERIFY(p1);
    CORRADE_VERIFY(p1->isTypeCompatibleWith(PropertyType::Reference));
    CORRADE_COMPARE(p1->identifier(), UnknownIdentifier);
    CORRADE_COMPARE(p1->as<std::string>(), "%hello");
}

void Test::customPropertyExpectedSeparator() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root (some = 15.3 boolean"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected , character on line 1\n");
}

void Test::customPropertyExpectedValueAssignment() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root (some 15.3"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected = character on line 1\n");
}

void Test::customPropertyExpectedListEnd() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root (some = 15.3 "}, structureIdentifiers, propertyIdentifiers));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected ) character on line 1\n");
}

void Test::customPropertyInvalidIdentifier() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root (%some = 15.3"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): invalid identifier on line 1\n");
}

void Test::customPropertyInvalidValue() {
    std::ostringstream out;
    Error::setOutput(&out);

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root (some = Fail"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): invalid property value on line 1\n");
}

void Test::hierarchy() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{R"oddl(
// This should finally work.

Root (some /*duplicates are ignored*/ = 15.0, some = 0.5) { string { "hello", "world" } }

Hierarchic %node819 (boolean = false, id = 819) {
    Hierarchic %node820 (boolean = true, id = 820) {
        Some { int32[2] { {3, 4}, {5, 6} } }
    }

    Some { int16[2] { {0, 1}, {2, 3} } }
}

Hierarchic %node821 {}
    )oddl"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_VERIFY(!d.isEmpty());

    std::optional<Structure> root = d.findFirstChildOf(RootStructure);
    CORRADE_VERIFY(root);
    CORRADE_VERIFY(root->isCustom());
    std::optional<Property> rootSomeProperty = root->findPropertyOf(SomeProperty);
    CORRADE_VERIFY(rootSomeProperty);
    CORRADE_COMPARE(rootSomeProperty->identifier(), SomeProperty);
    CORRADE_VERIFY(rootSomeProperty->isTypeCompatibleWith(PropertyType::Float));
    CORRADE_COMPARE(rootSomeProperty->as<Float>(), 0.5f);
    CORRADE_VERIFY(root->hasChildren());
    CORRADE_VERIFY(!root->findFirstChild()->findNext());
    CORRADE_COMPARE(root->firstChild().type(), Type::String);
    CORRADE_COMPARE_AS(root->firstChildOf(Type::String).asArray<std::string>(),
        Containers::Array<std::string>::from("hello", "world"),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(!root->findNextOf(RootStructure));
    CORRADE_VERIFY(!root->findPropertyOf(BooleanProperty));

    std::optional<Structure> hierarchicA = d.findFirstChildOf(HierarchicStructure);
    CORRADE_VERIFY(hierarchicA);
    CORRADE_VERIFY(hierarchicA->isCustom());
    CORRADE_COMPARE(hierarchicA->identifier(), HierarchicStructure);
    CORRADE_COMPARE(hierarchicA->name(), "%node819");
    std::optional<Structure> hASome = hierarchicA->findFirstChildOf(SomeStructure);
    CORRADE_VERIFY(hASome);
    CORRADE_VERIFY(hASome->isCustom());
    CORRADE_VERIFY(!hASome->findNext());
    std::optional<Structure> hASomeData = hASome->findFirstChild();
    CORRADE_VERIFY(hASomeData);
    CORRADE_COMPARE(hASomeData->type(), Type::Short);
    CORRADE_COMPARE(hASomeData->subArraySize(), 2);
    CORRADE_COMPARE_AS(hASomeData->asArray<Short>(),
        Containers::Array<Short>::from(0, 1, 2, 3),
        TestSuite::Compare::Container);

    std::optional<Structure> hierarchicB = hierarchicA->findFirstChildOf(HierarchicStructure);
    CORRADE_VERIFY(hierarchicB);
    CORRADE_VERIFY(hierarchicB->isCustom());
    CORRADE_COMPARE(hierarchicB->name(), "%node820");
    std::optional<Property> hBbooleanProperty = hierarchicB->findPropertyOf(BooleanProperty);
    CORRADE_VERIFY(hBbooleanProperty);
    CORRADE_VERIFY(hBbooleanProperty->isTypeCompatibleWith(PropertyType::Bool));
    CORRADE_COMPARE(hBbooleanProperty->as<bool>(), true);
    std::optional<Structure> hBSome = hierarchicB->findFirstChildOf(SomeStructure);
    CORRADE_VERIFY(hBSome);
    CORRADE_VERIFY(hBSome->isCustom());
    CORRADE_VERIFY(!hBSome->findNext());
    std::optional<Structure> hBSomeData = hBSome->findFirstChild();
    CORRADE_VERIFY(hBSomeData);
    CORRADE_COMPARE(hBSomeData->type(), Type::Int);
    CORRADE_COMPARE(hBSomeData->subArraySize(), 2);
    CORRADE_COMPARE_AS(hBSomeData->asArray<Int>(),
        Containers::Array<Int>::from(3, 4, 5, 6),
        TestSuite::Compare::Container);

    std::optional<Structure> hierarchicC = hierarchicA->findNextOf(HierarchicStructure);
    CORRADE_VERIFY(hierarchicC);
    CORRADE_VERIFY(hierarchicC->isCustom());
    CORRADE_COMPARE(hierarchicC->name(), "%node821");

    CORRADE_VERIFY(!hierarchicC->findNextOf(HierarchicStructure));
}

}}}

CORRADE_TEST_MAIN(Magnum::OpenDdl::Test::Test)
