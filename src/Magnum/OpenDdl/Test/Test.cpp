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
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/DebugStl.h>

#include "Magnum/OpenDdl/Document.h"
#include "Magnum/OpenDdl/Property.h"
#include "Magnum/OpenDdl/Structure.h"
#include "Magnum/OpenDdl/Validation.h"

namespace Magnum { namespace OpenDdl { namespace Test { namespace {

struct Test: TestSuite::Tester {
    explicit Test();

    void primitive();
    void primitiveEmpty();
    void primitiveName();
    void primitiveExpectedListStart();
    void primitiveExpectedListEnd();
    void primitiveExpectedSeparator();
    void primitiveExpectedNext();

    void primitiveSubArray();
    void primitiveSubArrayEmpty();
    void primitiveSubArrayName();
    void primitiveSubArrayInvalidSize();
    void primitiveSubArrayExpectedArraySizeEnd();
    void primitiveSubArrayExpectedSubSeparator();
    void primitiveSubArrayExpectedSubNext();
    void primitiveSubArrayExpectedNext();
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

    void documentChildren();
    void structureChildren();
    void structureProperties();

    void structureEquality();

    void validate();

    void validateUnexpectedPrimitiveInRoot();
    void validateTooManyPrimitives();
    void validateTooLittlePrimitives();
    void validateUnexpectedPrimitiveArraySize();
    void validateWrongPrimitiveType();

    void validateUnexpectedStructure();
    void validateTooManyStructures();
    void validateTooLittleStructures();
    void validateUnknownStructure();

    void validateExpectedProperty();
    void validateUnexpectedProperty();
    void validateWrongPropertyType();
    void validateUnknownProperty();

    void reference();
    void referenceInProperty();
    void referenceNull();
    void referenceChain();
    void referenceInvalid();
};

Test::Test() {
    addTests({&Test::primitive,
              &Test::primitiveEmpty,
              &Test::primitiveName,
              &Test::primitiveExpectedListStart,
              &Test::primitiveExpectedListEnd,
              &Test::primitiveExpectedSeparator,
              &Test::primitiveExpectedNext,

              &Test::primitiveSubArray,
              &Test::primitiveSubArrayEmpty,
              &Test::primitiveSubArrayName,
              &Test::primitiveSubArrayInvalidSize,
              &Test::primitiveSubArrayExpectedArraySizeEnd,
              &Test::primitiveSubArrayExpectedSubSeparator,
              &Test::primitiveSubArrayExpectedSubNext,
              &Test::primitiveSubArrayExpectedNext,
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
              &Test::customPropertyUnknown,
              &Test::customPropertyExpectedValueAssignment,
              &Test::customPropertyExpectedSeparator,
              &Test::customPropertyExpectedListEnd,
              &Test::customPropertyInvalidIdentifier,
              &Test::customPropertyInvalidValue,

              &Test::hierarchy,

              &Test::documentChildren,
              &Test::structureChildren,
              &Test::structureProperties,

              &Test::structureEquality,

              &Test::validate,

              &Test::validateUnexpectedPrimitiveInRoot,
              &Test::validateTooManyPrimitives,
              &Test::validateTooLittlePrimitives,
              &Test::validateUnexpectedPrimitiveArraySize,
              &Test::validateWrongPrimitiveType,

              &Test::validateUnexpectedStructure,
              &Test::validateTooManyStructures,
              &Test::validateTooLittleStructures,
              &Test::validateUnknownStructure,

              &Test::validateExpectedProperty,
              &Test::validateUnexpectedProperty,
              &Test::validateWrongPropertyType,
              &Test::validateUnknownProperty,

              &Test::reference,
              &Test::referenceInProperty,
              &Test::referenceNull,
              &Test::referenceChain,
              &Test::referenceInvalid});
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
        (Containers::Array<Short>{Containers::InPlaceInit, {35, -0x0c, 45}}),
        TestSuite::Compare::Container);
}

void Test::primitiveEmpty() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"float {}"}, {}, {}));
    CORRADE_VERIFY(!d.isEmpty());

    Structure s = d.firstChild();
    CORRADE_VERIFY(!s.isCustom());
    CORRADE_COMPARE(s.type(), Type::Float);
    CORRADE_VERIFY(!s.hasName());
    CORRADE_COMPARE(s.name(), "");
    CORRADE_COMPARE(s.arraySize(), 0);
}

void Test::primitiveName() {
    Document d;
    CORRADE_VERIFY(d.parse(CharacterLiteral{"float %name {}"}, {}, {}));
    CORRADE_VERIFY(!d.isEmpty());
    CORRADE_VERIFY(d.firstChild().hasName());
    CORRADE_COMPARE(d.firstChild().name(), "%name");
}

void Test::primitiveExpectedListStart() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"float 35"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected { character on line 1\n");
}

void Test::primitiveExpectedListEnd() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"float { 35"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected } character on line 1\n");
}

void Test::primitiveExpectedSeparator() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"float { 35 45"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected , character on line 1\n");
}

void Test::primitiveExpectedNext() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"float { 35,"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected float literal on line 1\n");
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
        (Containers::Array<UnsignedByte>{Containers::InPlaceInit, {0xca, 0xfe, 0xba, 0xbe}}),
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

void Test::primitiveSubArrayInvalidSize() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"unsigned_int8[0] {}"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): invalid subarray size on line 1\n");
}

void Test::primitiveSubArrayExpectedArraySizeEnd() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"unsigned_int8[2 {"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected ] character on line 1\n");
}

void Test::primitiveSubArrayExpectedSubSeparator() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"unsigned_int8[2] { {0xca, 0xfe} {0xba"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected , character on line 1\n");
}

void Test::primitiveSubArrayExpectedSubNext() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"unsigned_int8[3] { {0xca, 0xfe,"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected unsigned_int8 literal on line 1\n");
}

void Test::primitiveSubArrayExpectedNext() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"unsigned_int8[2] { {0xca, 0xfe},"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected { character on line 1\n");
}

void Test::primitiveSubArrayExpectedSubListEnd() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"int32[2] { {0xca, 0xfe, 0xba"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected } character on line 1\n");
}

void Test::primitiveSubArrayExpectedSeparator() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    #ifndef MAGNUM_TARGET_GLES
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"double[2] { {35 45"}, {}, {}));
    #else
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"float[2] { {35 45"}, {}, {}));
    #endif
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected , character on line 1\n");
}

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
    CORRADE_VERIFY(!s.hasName());
    CORRADE_COMPARE(s.name(), "");
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
    CORRADE_VERIFY(d.firstChild().hasName());
    CORRADE_COMPARE(d.firstChild().name(), "%some_name");
}

void Test::customInvalidIdentifier() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"%name { string"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): invalid identifier on line 1\n");
}

void Test::customExpectedListStart() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root string"}, {}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected { character on line 1\n");
}

void Test::customExpectedListEnd() {
    std::ostringstream out;
    Error redirectError{&out};

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

    Containers::Optional<Property> p1 = s.propertyOf(BooleanProperty);
    CORRADE_VERIFY(p1);
    CORRADE_VERIFY(p1->isTypeCompatibleWith(PropertyType::Bool));
    CORRADE_COMPARE(p1->identifier(), BooleanProperty);
    CORRADE_COMPARE(p1->as<bool>(), true);

    Containers::Optional<Property> p2 = s.propertyOf(SomeProperty);
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
    CORRADE_VERIFY(d.parse(CharacterLiteral{"Root (unspecified = \"hello\") {}"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_VERIFY(!d.isEmpty());

    Structure s = d.firstChild();
    CORRADE_VERIFY(s.isCustom());
    CORRADE_COMPARE(s.propertyCount(), 1);

    Containers::Optional<Property> p1 = s.findPropertyOf(UnknownIdentifier);
    CORRADE_VERIFY(p1);
    CORRADE_VERIFY(p1->isTypeCompatibleWith(PropertyType::String));
    CORRADE_COMPARE(p1->identifier(), UnknownIdentifier);
    CORRADE_COMPARE(p1->as<std::string>(), "hello");
}

void Test::customPropertyExpectedSeparator() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root (some = 15.3 boolean"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected , character on line 1\n");
}

void Test::customPropertyExpectedValueAssignment() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root (some 15.3"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected = character on line 1\n");
}

void Test::customPropertyExpectedListEnd() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root (some = 15.3 "}, structureIdentifiers, propertyIdentifiers));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): expected ) character on line 1\n");
}

void Test::customPropertyInvalidIdentifier() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root (%some = 15.3"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): invalid identifier on line 1\n");
}

void Test::customPropertyInvalidValue() {
    std::ostringstream out;
    Error redirectError{&out};

    Document d;
    CORRADE_VERIFY(!d.parse(CharacterLiteral{"Root (some = Fail"}, structureIdentifiers, propertyIdentifiers));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::parse(): invalid property value on line 1\n");
}

void Test::hierarchy() {
    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
// This should finally work.

Root (some /*duplicates are ignored*/ = 15.0, some = 0.5) { string { "hello", "world" } }

Hierarchic %node819 (boolean = false, id = 819) {
    Hierarchic %node820 (boolean = true, id = 820) {
        Some { int32[2] { {3, 4}, {5, 6} } }
    }

    Some { int16[2] { {0, 1}, {2, 3} } }
}

Hierarchic %node821 {}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));
    CORRADE_VERIFY(!d.isEmpty());

    Containers::Optional<Structure> root = d.findFirstChildOf(RootStructure);
    CORRADE_VERIFY(root);
    CORRADE_VERIFY(!root->parent());
    CORRADE_VERIFY(root->isCustom());
    Containers::Optional<Property> rootSomeProperty = root->findPropertyOf(SomeProperty);
    CORRADE_VERIFY(rootSomeProperty);
    CORRADE_COMPARE(rootSomeProperty->identifier(), SomeProperty);
    CORRADE_VERIFY(rootSomeProperty->isTypeCompatibleWith(PropertyType::Float));
    CORRADE_COMPARE(rootSomeProperty->as<Float>(), 0.5f);

    CORRADE_VERIFY(root->hasChildren());
    Structure string = root->firstChild();
    CORRADE_VERIFY(string.parent());
    CORRADE_VERIFY(string.parent() == root);
    CORRADE_VERIFY(!string.findNext());
    CORRADE_COMPARE(string.type(), Type::String);
    CORRADE_COMPARE_AS(root->firstChildOf(Type::String).asArray<std::string>(),
        (Containers::Array<std::string>{Containers::InPlaceInit, {"hello", "world"}}),
        TestSuite::Compare::Container);

    CORRADE_VERIFY(!root->findNextOf(RootStructure));
    CORRADE_VERIFY(!root->findPropertyOf(BooleanProperty));

    Containers::Optional<Structure> hierarchicA = d.findFirstChildOf(HierarchicStructure);
    CORRADE_VERIFY(hierarchicA);
    CORRADE_VERIFY(!hierarchicA->parent());
    CORRADE_VERIFY(hierarchicA->isCustom());
    CORRADE_COMPARE(hierarchicA->identifier(), HierarchicStructure);
    CORRADE_COMPARE(hierarchicA->name(), "%node819");
    Containers::Optional<Structure> hASome = hierarchicA->findFirstChildOf(SomeStructure);
    CORRADE_VERIFY(hASome);
    CORRADE_VERIFY(hASome->parent() == hierarchicA);
    CORRADE_VERIFY(hASome->isCustom());
    CORRADE_VERIFY(!hASome->findNext());
    Containers::Optional<Structure> hASomeData = hASome->findFirstChild();
    CORRADE_VERIFY(hASomeData);
    CORRADE_VERIFY(hASomeData->parent() && *hASomeData->parent() == *hASome);
    CORRADE_COMPARE(hASomeData->type(), Type::Short);
    CORRADE_COMPARE(hASomeData->subArraySize(), 2);
    CORRADE_COMPARE_AS(hASomeData->asArray<Short>(),
        (Containers::Array<Short>{Containers::InPlaceInit, {0, 1, 2, 3}}),
        TestSuite::Compare::Container);

    Containers::Optional<Structure> hierarchicB = hierarchicA->findFirstChildOf(HierarchicStructure);
    CORRADE_VERIFY(hierarchicB);
    CORRADE_VERIFY(hierarchicB->parent() == hierarchicA);
    CORRADE_VERIFY(hierarchicB->isCustom());
    CORRADE_COMPARE(hierarchicB->name(), "%node820");
    Containers::Optional<Property> hBbooleanProperty = hierarchicB->findPropertyOf(BooleanProperty);
    CORRADE_VERIFY(hBbooleanProperty);
    CORRADE_VERIFY(hBbooleanProperty->isTypeCompatibleWith(PropertyType::Bool));
    CORRADE_COMPARE(hBbooleanProperty->as<bool>(), true);
    Containers::Optional<Structure> hBSome = hierarchicB->findFirstChildOf(SomeStructure);
    CORRADE_VERIFY(hBSome);
    CORRADE_VERIFY(hBSome->parent() == hierarchicB);
    CORRADE_VERIFY(hBSome->isCustom());
    CORRADE_VERIFY(!hBSome->findNext());
    Containers::Optional<Structure> hBSomeData = hBSome->findFirstChild();
    CORRADE_VERIFY(hBSomeData);
    CORRADE_VERIFY(hBSomeData->parent() == hBSome);
    CORRADE_COMPARE(hBSomeData->type(), Type::Int);
    CORRADE_COMPARE(hBSomeData->subArraySize(), 2);
    CORRADE_COMPARE_AS(hBSomeData->asArray<Int>(),
        (Containers::Array<Int>{Containers::InPlaceInit, {3, 4, 5, 6}}),
        TestSuite::Compare::Container);

    Containers::Optional<Structure> hierarchicC = hierarchicA->findNextOf(HierarchicStructure);
    CORRADE_VERIFY(hierarchicC);
    CORRADE_VERIFY(!hierarchicC->parent());
    CORRADE_VERIFY(hierarchicC->isCustom());
    CORRADE_COMPARE(hierarchicC->name(), "%node821");

    CORRADE_VERIFY(!hierarchicC->findNextOf(HierarchicStructure));
}

void Test::documentChildren() {
    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
Root %root1 {}
Hierarchic %hierarchic1 {
    Root %root2 {}
    Hierarchic %hierarchic2 {}
}
Hierarchic %hierarchic3 {}
Unknown %unknown {}
Root %root3 {}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    {
        std::vector<std::string> names;
        for(Structure s: d.children()) names.push_back(s.name());
        CORRADE_COMPARE(names, (std::vector<std::string>{
            "%root1",
            "%hierarchic1",
            "%hierarchic3",
            "%unknown",
            "%root3"}));
    } {
        std::vector<std::string> names;
        for(Structure s: d.childrenOf(HierarchicStructure)) names.push_back(s.name());
        CORRADE_COMPARE(names, (std::vector<std::string>{
            "%hierarchic1",
            "%hierarchic3"}));
    } {
        std::vector<std::string> names;
        for(Structure s: d.childrenOf(HierarchicStructure, RootStructure)) names.push_back(s.name());
        CORRADE_COMPARE(names, (std::vector<std::string>{
            "%root1",
            "%hierarchic1",
            "%hierarchic3",
            "%root3"}));
    } {
        std::vector<std::string> names;
        for(Structure s: d.childrenOf(SomeStructure))
            names.push_back(s.name());
        CORRADE_VERIFY(names.empty());
    }
}

void Test::structureChildren() {
    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
Root %root1 {}
Hierarchic %hierarchic1 {
    Root %root2 {}
    Unknown %unknown {}
    Hierarchic %hierarchic2 {
        Root %root3 {}
    }
    Root %root4 {}
}
Hierarchic %hierarchic3 {}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    {
        std::vector<std::string> names;
        for(Structure s: d.firstChildOf(HierarchicStructure).children())
            names.push_back(s.name());
        CORRADE_COMPARE(names, (std::vector<std::string>{
            "%root2",
            "%unknown",
            "%hierarchic2",
            "%root4"}));
    } {
        std::vector<std::string> names;
        for(Structure s: d.firstChildOf(HierarchicStructure).childrenOf(RootStructure))
            names.push_back(s.name());
        CORRADE_COMPARE(names, (std::vector<std::string>{
            "%root2",
            "%root4"}));
    } {
        std::vector<std::string> names;
        for(Structure s: d.firstChildOf(HierarchicStructure).childrenOf(RootStructure, HierarchicStructure))
            names.push_back(s.name());
        CORRADE_COMPARE(names, (std::vector<std::string>{
            "%root2",
            "%hierarchic2",
            "%root4"}));
    } {
        std::vector<std::string> names;
        for(Structure s: d.firstChildOf(RootStructure).children())
            names.push_back(s.name());
        CORRADE_VERIFY(names.empty());
    }
}

void Test::structureProperties() {
    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
Root (some = "string to ignore", boolean = "hello", unknown = "hey", some = "string") {}
Hierarchic () {}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    {
        std::vector<std::string> strings;
        for(Property p: d.firstChildOf(RootStructure).properties())
            strings.push_back(p.as<std::string>());
        CORRADE_COMPARE(strings, (std::vector<std::string>{
            "string to ignore",
            "hello",
            "hey",
            "string"}));
    } {
        std::vector<std::string> strings;
        for(Property p: d.firstChildOf(HierarchicStructure).properties())
            strings.push_back(p.as<std::string>());
        CORRADE_VERIFY(strings.empty());
    }
}

void Test::structureEquality() {
    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
Root {}
Some {}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    Structure a = d.firstChildOf(RootStructure);
    Structure b = d.firstChildOf(SomeStructure);
    CORRADE_VERIFY(a == a && b == b);
    CORRADE_VERIFY(a != b && b != a);
}

void Test::validate() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{
R"oddl(
Root (some = 15.0, some = 0.5) { string { "hello", "world" } }

Hierarchic (boolean = false, id = 819) {
    ref { null }

    Hierarchic (boolean = true, id = 820) {
        Some { int32[2] { {3, 4}, {5, 6} } }
    }

    Some { int16[2] { {0, 1}, {2, 3} } }
}

Hierarchic (boolean = false) {}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    CORRADE_VERIFY(d.validate(
        Structures{{RootStructure, {1, 1}},
                   {HierarchicStructure, {1, 0}}},
        {
            {RootStructure,
                 Properties{{SomeProperty, PropertyType::Float, RequiredProperty},
                            {BooleanProperty, PropertyType::Bool, OptionalProperty}},
                 Primitives{Type::String}, 1, 0},
            {HierarchicStructure,
                 Properties{{BooleanProperty, PropertyType::Bool, RequiredProperty}},
                 Primitives{Type::Reference}, 0, 1,
                 Structures{{SomeStructure, {0, 1}},
                            {HierarchicStructure, {}}}},
            {SomeStructure,
                 Primitives{Type::Int, Type::Short}, 1, 4}
        }));
}

void Test::validateUnexpectedPrimitiveInRoot() {
    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
string { "hello" }
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!d.validate({}, {}));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): unexpected primitive structure in root\n");
}

void Test::validateTooManyPrimitives() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
Root {
    Hierarchic { }
    string { "world" }
    string { "world" }
}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!d.validate(
        Structures{{RootStructure, {1, 1}}},
        {
            {RootStructure,
                Primitives{Type::String}, 1, 1,
                Structures{{HierarchicStructure, {1, 1}}}},
        }));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): expected exactly 1 primitive sub-structures in structure Root\n");
}

void Test::validateTooLittlePrimitives() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
Root {
    Hierarchic { }
    string { "world" }
}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!d.validate(
        Structures{{RootStructure, {1, 1}}},
        {
            {RootStructure,
                Primitives{Type::String}, 2, 1,
                Structures{{HierarchicStructure, {1, 1}}}},
        }));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): expected exactly 2 primitive sub-structures in structure Root\n");
}

void Test::validateUnexpectedPrimitiveArraySize() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
Root {
    string { "hello", "world", "how is it going" }
}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!d.validate(
        Structures{{RootStructure, {1, 1}}},
        {
            {RootStructure, Primitives{Type::String}, 1, 2},
        }));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): expected exactly 2 values in Root sub-structure\n");
}

void Test::validateWrongPrimitiveType() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
Root { int32 {} }
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!d.validate(
        Structures{{RootStructure, {1, 1}}},
        {
            {RootStructure, Primitives{Type::String}, 1, 0},
        }));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): unexpected sub-structure of type OpenDdl::Type::Int in structure Root\n");
}

void Test::validateUnexpectedStructure() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
Root { }
Hierarchic {  }
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!d.validate(
        Structures{{RootStructure, {1, 2}}},
        {
            {RootStructure},
            {HierarchicStructure},
        }));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): unexpected structure Hierarchic\n");
}

void Test::validateTooManyStructures() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
Root { }
Root { }
Root { }
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!d.validate(
        Structures{{RootStructure, {1, 2}}},
        {
            {RootStructure, Structures{}},
        }));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): too many Root structures, got 3 but expected max 2\n");
}

void Test::validateTooLittleStructures() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
Root { }
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!d.validate(
        Structures{{RootStructure, {2, 3}}},
        {
            {RootStructure, Structures{}},
        }));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): too little Root structures, got 1 but expected min 2\n");
}

void Test::validateUnknownStructure() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{R"oddl(
Root { string { "hello" } }

Unknown { Root { int32 {} } }
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    /* Unknown structure should be ignored even if its contents don't
       validate */
    CORRADE_VERIFY(d.validate(
        Structures{{RootStructure, {1, 1}}},
        {
            {RootStructure, Primitives{Type::String}, 1, 1},
        }));
}

void Test::validateExpectedProperty() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{
R"oddl(
Root () {}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!d.validate(
        Structures{{RootStructure, {1, 1}}},
        {
            {RootStructure,
                 Properties{{SomeProperty, PropertyType::Float, RequiredProperty},
                            {BooleanProperty, PropertyType::Bool, OptionalProperty}}}
        }));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): expected property some in structure Root\n");
}

void Test::validateUnexpectedProperty() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{
R"oddl(
Root (some = 15.0, boolean = true) {}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!d.validate(
        Structures{{RootStructure, {1, 1}}},
        {
            {RootStructure, Properties{{SomeProperty, PropertyType::Float, RequiredProperty}}}
        }));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): unexpected property boolean in structure Root\n");
}

void Test::validateWrongPropertyType() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{
R"oddl(
Root (some = false) {}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!d.validate(
        Structures{{RootStructure, {1, 1}}},
        {
            {RootStructure, Properties{{SomeProperty, PropertyType::Float, RequiredProperty}}}
        }));
    CORRADE_COMPARE(out.str(), "OpenDdl::Document::validate(): unexpected type of property some , expected OpenDdl::PropertyType::Float\n");
}

void Test::validateUnknownProperty() {
    using namespace Validation;

    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{
R"oddl(
Root (some = 15.0, id = null) {}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    /* Unknown property should be ignored */
    CORRADE_VERIFY(d.validate(
        Structures{{RootStructure, {1, 1}}},
        {
            {RootStructure, Properties{{SomeProperty, PropertyType::Float, RequiredProperty}}}
        }));
}

void Test::reference() {
    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{
R"oddl(
Root { ref { %b0 } }
Hierarchic { ref { $b1 } }
Root {
    Root %b0 {}
    Root {
        Root $b1 {}
    }
}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    Containers::Optional<Structure> b0 = d.firstChildOf(RootStructure).firstChild().asReference();
    CORRADE_VERIFY(b0);
    CORRADE_COMPARE(b0->name(), "%b0");

    Containers::Optional<Structure> b1 = d.firstChildOf(HierarchicStructure).firstChild().asReference();
    CORRADE_VERIFY(b1);
    CORRADE_COMPARE(b1->name(), "$b1");
}

void Test::referenceInProperty() {
    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{
R"oddl(
Root (reference = %b0) {}
Hierarchic (reference = $b1) {}
Root {
    Root %b0 {}
    Root {
        Root $b1 {}
    }
}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    Containers::Optional<Structure> b0 = d.firstChildOf(RootStructure).propertyOf(ReferenceProperty).asReference();
    CORRADE_VERIFY(b0);
    CORRADE_COMPARE(b0->name(), "%b0");

    Containers::Optional<Structure> b1 = d.firstChildOf(HierarchicStructure).propertyOf(ReferenceProperty).asReference();
    CORRADE_VERIFY(b1);
    CORRADE_COMPARE(b1->name(), "$b1");
}

void Test::referenceNull() {
    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{
R"oddl(
Root (reference = null) {}
Hierarchic { ref { null } }
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    CORRADE_VERIFY(!d.firstChildOf(RootStructure).propertyOf(ReferenceProperty).asReference());
    CORRADE_VERIFY(!d.firstChildOf(HierarchicStructure).firstChild().asReference());
}

void Test::referenceChain() {
    Document d;
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s = CharacterLiteral{
R"oddl(
ref {
    /* These two are different structures */
    %local1, %root%local1,

    /* Both of these should be found and not result in an error */
    $global1%local2,  $global2
}

Root %root {
    Root %local1 {
        int16 %local3 {}
    }

    ref {
        /* Single name, takes the sibling */
        %local1,

        /* Single name but sibling not found, takes the global one */
        %local4,

        /* Multiple names, takes the global one */
        %local1%local3
    }

    Root $global1 {
        int8 $global2 {}
        float %local2 {}
    }
}
Root %local1 {
    int32 %local3 {}
}
bool %local4 {}
    )oddl"};
    CORRADE_VERIFY(d.parse(s, structureIdentifiers, propertyIdentifiers));

    Containers::Array<Containers::Optional<Structure>> topLevel =
        d.firstChildOf(Type::Reference).asReferenceArray();
    CORRADE_COMPARE(topLevel.size(), 4);

    CORRADE_VERIFY(topLevel[0]);
    CORRADE_COMPARE(topLevel[0]->name(), "%local1");
    CORRADE_VERIFY(!topLevel[0]->parent());

    CORRADE_VERIFY(topLevel[1]);
    CORRADE_VERIFY(topLevel[1] != topLevel[0]);
    CORRADE_COMPARE(topLevel[1]->name(), "%local1");
    CORRADE_VERIFY(topLevel[1]->parent());

    CORRADE_VERIFY(topLevel[2]);
    CORRADE_COMPARE(topLevel[2]->name(), "%local2");
    CORRADE_COMPARE(topLevel[2]->type(), Type::Float);

    CORRADE_VERIFY(topLevel[3]);
    CORRADE_COMPARE(topLevel[3]->name(), "$global2");
    CORRADE_COMPARE(topLevel[3]->type(), Type::Byte);


    Containers::Array<Containers::Optional<Structure>> local =
        d.firstChildOf(RootStructure).firstChildOf(Type::Reference).asReferenceArray();
    CORRADE_COMPARE(local.size(), 3);

    CORRADE_VERIFY(local[0]);
    CORRADE_COMPARE(local[0]->name(), "%local1");
    CORRADE_VERIFY(local[0]->parent());

    CORRADE_VERIFY(local[1]);
    CORRADE_COMPARE(local[1]->name(), "%local4");
    CORRADE_COMPARE(local[1]->type(), Type::Bool);

    CORRADE_VERIFY(local[2]);
    CORRADE_COMPARE(local[2]->name(), "%local3");
    CORRADE_COMPARE(local[2]->type(), Type::Int);
}

void Test::referenceInvalid() {
    Document d;
    std::ostringstream out;
    Error redirectError{&out};

    /* Single name not found */
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s1 = CharacterLiteral{
R"oddl(
Hierarchic (reference = %local1) {}
    )oddl"};
    CORRADE_VERIFY(!d.parse(s1, structureIdentifiers, propertyIdentifiers));

    /* Incomplete chain (even though that could be found as sibling) */
    /* GCC < 4.9 cannot handle multiline raw string literals inside macros */
    auto s2 = CharacterLiteral{
R"oddl(
Root %root {
    Hierarchic (reference = %local1%local2) {}

    Root %local1 {
        int16 %local2 {}
    }
}
    )oddl"};
    CORRADE_VERIFY(!d.parse(s2, structureIdentifiers, propertyIdentifiers));

    CORRADE_COMPARE(out.str(),
        "OpenDdl::Document::parse(): reference %local1 was not found\n"
        "OpenDdl::Document::parse(): reference %local1%local2 was not found\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::OpenDdl::Test::Test)
