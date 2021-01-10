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

#include <tuple>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/DebugStl.h>

#include "Magnum/OpenDdl/Document.h"
#include "Magnum/OpenDdl/Property.h"
#include "Magnum/OpenDdl/Implementation/Parsers.h"

namespace Magnum { namespace OpenDdl { namespace Test { namespace {

struct ParsersTest: TestSuite::Tester {
    explicit ParsersTest();

    void equals();
    void findLastOf();
    void whitespace();
    void onelineComment();
    void multilineComment();

    void escapedCharInvalid();
    void escapedChar();
    void escapedCharHex();

    void escapedUnicodeInvalid();
    void escapedUnicode4();
    void escapedUnicode6();

    void identifierInvalid();
    void identifier();

    void boolLiteralInvalid();
    void boolLiteral();

    void characterLiteralInvalid();
    void characterLiteral();
    void characterLiteralEscaped();

    void integerLiteralInvalid();
    void integerLiteral();
    void integerLiteralChar();
    void integerLiteralBinary();

    void floatLiteralInvalid();
    void floatLiteral();
    void floatLiteralBinary();

    void stringLiteralInvalid();
    void stringLiteralEmpty();
    void stringLiteral();
    void stringLiteralEscaped();
    void stringLiteralConcatenated();

    void nameLiteralInvalid();
    void nameLiteral();

    void referenceLiteralInvalid();
    void referenceLiteralNull();
    void referenceLiteral();

    void typeLiteralInvalid();
    void typeLiteral();

    void propertyValueInvalid();
    void propertyValueBool();
    void propertyValueIntegral();
    void propertyValueCharacter();
    void propertyValueBinary();
    void propertyValueFloatingPoint();
    void propertyValueString();
    void propertyValueReference();
    void propertyValueReferenceNull();
    void propertyValueType();
};

ParsersTest::ParsersTest() {
    addTests({&ParsersTest::equals,
              &ParsersTest::findLastOf,
              &ParsersTest::whitespace,
              &ParsersTest::onelineComment,
              &ParsersTest::multilineComment,

              &ParsersTest::escapedCharInvalid,
              &ParsersTest::escapedChar,
              &ParsersTest::escapedCharHex,

              &ParsersTest::escapedUnicodeInvalid,
              &ParsersTest::escapedUnicode4,
              &ParsersTest::escapedUnicode6,

              &ParsersTest::identifierInvalid,
              &ParsersTest::identifier,

              &ParsersTest::boolLiteralInvalid,
              &ParsersTest::boolLiteral,

              &ParsersTest::characterLiteralInvalid,
              &ParsersTest::characterLiteral,
              &ParsersTest::characterLiteralEscaped,

              &ParsersTest::integerLiteralInvalid,
              &ParsersTest::integerLiteral,
              &ParsersTest::integerLiteralChar,
              &ParsersTest::integerLiteralBinary,

              &ParsersTest::floatLiteralInvalid,
              &ParsersTest::floatLiteral,
              &ParsersTest::floatLiteralBinary,

              &ParsersTest::stringLiteralInvalid,
              &ParsersTest::stringLiteralEmpty,
              &ParsersTest::stringLiteral,
              &ParsersTest::stringLiteralEscaped,
              &ParsersTest::stringLiteralConcatenated,

              &ParsersTest::nameLiteralInvalid,
              &ParsersTest::nameLiteral,

              &ParsersTest::referenceLiteralInvalid,
              &ParsersTest::referenceLiteralNull,
              &ParsersTest::referenceLiteral,

              &ParsersTest::typeLiteralInvalid,
              &ParsersTest::typeLiteral,

              &ParsersTest::propertyValueInvalid,
              &ParsersTest::propertyValueBool,
              &ParsersTest::propertyValueIntegral,
              &ParsersTest::propertyValueCharacter,
              &ParsersTest::propertyValueBinary,
              &ParsersTest::propertyValueFloatingPoint,
              &ParsersTest::propertyValueString,
              &ParsersTest::propertyValueReference,
              &ParsersTest::propertyValueReferenceNull,
              &ParsersTest::propertyValueType});
}

#define VERIFY_PARSED(e, data, i, parsed) \
    CORRADE_COMPARE(e.error, Implementation::ParseErrorType::NoError); \
    CORRADE_VERIFY(i >= data.begin()); \
    CORRADE_COMPARE((std::string{data.begin(), std::size_t(i - data.begin())}), parsed);

void ParsersTest::equals() {
    CharacterLiteral a{"HelloWorld"};
    CharacterLiteral b{"Hello"};

    CORRADE_VERIFY(Implementation::equals(a, a));
    CORRADE_VERIFY(!Implementation::equals(a, b));
    CORRADE_VERIFY(!Implementation::equals(b, a));
}

void ParsersTest::findLastOf() {
    /* I'm too lazy to create another VERIFY_SUFFIX_PARSED(), so I'll test to
       prefix instead */
    CharacterLiteral a{"$hello%world"};
    auto ai = Implementation::findLastOf(a, "$%");
    VERIFY_PARSED(Implementation::ParseError{}, a, ai, "$hello");

    CharacterLiteral b{"%hello$world"};
    auto bi = Implementation::findLastOf(b, "$%");
    VERIFY_PARSED(Implementation::ParseError{}, b, bi, "%hello");

    Containers::ArrayView<const char> c{"%", 0};
    auto ci = Implementation::findLastOf({c, 0}, "$%");
    VERIFY_PARSED(Implementation::ParseError{}, c, ci, "");
}

void ParsersTest::whitespace() {
    CharacterLiteral a{""};
    auto ai = Implementation::whitespace(a);
    VERIFY_PARSED(Implementation::ParseError{}, a, ai, "");

    /* Just whitespace */
    CharacterLiteral b{"\n  "};
    auto bi = Implementation::whitespace(b);
    VERIFY_PARSED(Implementation::ParseError{}, b, bi, "\n  ");

    /* Whitespace and something after */
    CharacterLiteral c{" \b \t \n  X"};
    auto ci = Implementation::whitespace(c);
    VERIFY_PARSED(Implementation::ParseError{}, c, ci, " \b \t \n  ");
}

void ParsersTest::onelineComment() {
    CharacterLiteral a{" \b \t // comment \nX"};
    auto ai = Implementation::whitespace(a);
    VERIFY_PARSED(Implementation::ParseError{}, a, ai, " \b \t // comment \n");

    CharacterLiteral b{" \b \t // comment /* other comment \n*/ \nX"};
    auto bi = Implementation::whitespace(b);
    VERIFY_PARSED(Implementation::ParseError{}, b, bi, " \b \t // comment /* other comment \n");
}

void ParsersTest::multilineComment() {
    CharacterLiteral a{" \b \t /* comment \n bla \n comment */X"};
    auto ai = Implementation::whitespace(a);
    VERIFY_PARSED(Implementation::ParseError{}, a, ai, " \b \t /* comment \n bla \n comment */");

    CharacterLiteral b{" \b \t /* comment \n // bla \n comment */X"};
    auto bi = Implementation::whitespace(b);
    VERIFY_PARSED(Implementation::ParseError{}, b, bi, " \b \t /* comment \n // bla \n comment */");
}

void ParsersTest::escapedCharInvalid() {
    Implementation::ParseError error;

    CORRADE_VERIFY(!Implementation::escapedChar(CharacterLiteral{"\\"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidEscapeSequence);

    CORRADE_VERIFY(!Implementation::escapedChar(CharacterLiteral{"\\h"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidEscapeSequence);

    CORRADE_VERIFY(!Implementation::escapedChar(CharacterLiteral{"\\x1"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidEscapeSequence);
}

void ParsersTest::escapedChar() {
    CharacterLiteral a{"\\nX"};

    Implementation::ParseError error;
    const char* ai;
    char c;
    std::tie(ai, c) = Implementation::escapedChar(a, error);
    VERIFY_PARSED(error, a, ai, "\\n");
    CORRADE_COMPARE(c, '\n');
}

void ParsersTest::escapedCharHex() {
    CharacterLiteral a{"\\x0AX"};

    Implementation::ParseError error;
    const char* ai;
    char c;
    std::tie(ai, c) = Implementation::escapedChar(a, error);
    VERIFY_PARSED(error, a, ai, "\\x0A");
    CORRADE_COMPARE(c, '\n');
}

void ParsersTest::escapedUnicodeInvalid() {
    Implementation::ParseError error;
    std::string s;

    CORRADE_VERIFY(!Implementation::escapedUnicode(CharacterLiteral{"\\"}, s, error));
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidEscapeSequence);

    CORRADE_VERIFY(!Implementation::escapedUnicode(CharacterLiteral{"\\u123"}, s, error));
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidEscapeSequence);

    CORRADE_VERIFY(!Implementation::escapedUnicode(CharacterLiteral{"\\U12345"}, s, error));
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidEscapeSequence);
}

void ParsersTest::escapedUnicode4() {
    CharacterLiteral a{"\\u006dX"};
    Implementation::ParseError error;
    std::string s;
    const char* ai = Implementation::escapedUnicode(a, s, error);
    VERIFY_PARSED(error, a, ai, "\\u006d");
    CORRADE_EXPECT_FAIL("Not yet implemented.");
    CORRADE_COMPARE(s, "m");
}

void ParsersTest::escapedUnicode6() {
    CharacterLiteral a{"\\U00006DX"};
    Implementation::ParseError error;
    std::string s;
    const char* ai = Implementation::escapedUnicode(a, s, error);
    VERIFY_PARSED(error, a, ai, "\\U00006D");
    CORRADE_EXPECT_FAIL("Not yet implemented.");
    CORRADE_COMPARE(s, "m");
}

void ParsersTest::identifierInvalid() {
    Implementation::ParseError error;

    CORRADE_VERIFY(!Implementation::identifier(CharacterLiteral{""}, error));
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::ExpectedIdentifier);

    CORRADE_VERIFY(!Implementation::identifier(CharacterLiteral{"0"}, error));
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidIdentifier);
}

void ParsersTest::identifier() {
    CharacterLiteral a{"my_mesh,"};
    Implementation::ParseError error;
    const char* ai = Implementation::identifier(a, error);
    VERIFY_PARSED(error, a, ai, "my_mesh");
}

void ParsersTest::boolLiteralInvalid() {
    Implementation::ParseError error;

    CORRADE_VERIFY(!Implementation::boolLiteral(CharacterLiteral{""}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);

    CORRADE_VERIFY(!Implementation::boolLiteral(CharacterLiteral{"TRUE"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);
}

void ParsersTest::boolLiteral() {
    CharacterLiteral a{"trueX"};

    Implementation::ParseError error;
    const char* ai;
    bool value;
    std::tie(ai, value) = Implementation::boolLiteral(a, error);
    VERIFY_PARSED(error, a, ai, "true");
    CORRADE_COMPARE(value, true);
}

void ParsersTest::characterLiteralInvalid() {
    Implementation::ParseError error;

    CORRADE_VERIFY(!Implementation::characterLiteral(CharacterLiteral{""}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidCharacterLiteral);

    CORRADE_VERIFY(!Implementation::characterLiteral(CharacterLiteral{"'"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidCharacterLiteral);

    CORRADE_VERIFY(!Implementation::characterLiteral(CharacterLiteral{"'a"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidCharacterLiteral);

    CORRADE_VERIFY(!Implementation::characterLiteral(CharacterLiteral{"'\n"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidCharacterLiteral);
}

void ParsersTest::characterLiteral() {
    CharacterLiteral a{"'a'X"};

    Implementation::ParseError error;
    const char* ai;
    char value;
    std::tie(ai, value) = Implementation::characterLiteral(a, error);
    VERIFY_PARSED(error, a, ai, "'a'");
    CORRADE_COMPARE(value, 'a');
}

void ParsersTest::characterLiteralEscaped() {
    CharacterLiteral a{"'\\n'X"};

    Implementation::ParseError error;
    const char* ai;
    char value;
    std::tie(ai, value) = Implementation::characterLiteral(a, error);
    VERIFY_PARSED(error, a, ai, "'\\n'");
    CORRADE_COMPARE(value, '\n');
}

void ParsersTest::integerLiteralInvalid() {
    Implementation::ParseError error;
    std::string buffer;

    CORRADE_VERIFY(!std::get<0>(Implementation::integralLiteral<Short>(CharacterLiteral{""}, buffer, error)));
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::ExpectedLiteral);

    CORRADE_VERIFY(!std::get<0>(Implementation::integralLiteral<Short>(CharacterLiteral{"+"}, buffer, error)));
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);

    CORRADE_VERIFY(!std::get<0>(Implementation::integralLiteral<Short>(CharacterLiteral{"A"}, buffer, error)));
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);

    CORRADE_VERIFY(!std::get<0>(Implementation::integralLiteral<Short>(CharacterLiteral{"_1"}, buffer, error)));
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);

    CORRADE_VERIFY(!std::get<0>(Implementation::integralLiteral<Short>(CharacterLiteral{"0b_1"}, buffer, error)));
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);

    CORRADE_VERIFY(!std::get<0>(Implementation::integralLiteral<Short>(CharacterLiteral{"32768"}, buffer, error)));
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::LiteralOutOfRange);

    CORRADE_VERIFY(!std::get<0>(Implementation::integralLiteral<UnsignedShort>(CharacterLiteral{"-1"}, buffer, error)));
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::LiteralOutOfRange);
}

void ParsersTest::integerLiteral() {
    CharacterLiteral a{"-3_7X"};

    Implementation::ParseError error;
    std::string buffer;
    const char* ai;
    Short value;
    Int base;
    std::tie(ai, value, base) = Implementation::integralLiteral<Short>(a, buffer, error);
    VERIFY_PARSED(error, a, ai, "-3_7");
    CORRADE_COMPARE(value, -37);
    CORRADE_COMPARE(base, 10);
}

void ParsersTest::integerLiteralChar() {
    CharacterLiteral a{"+'a'X"};

    Implementation::ParseError error;
    std::string buffer;
    const char* ai;
    Short value;
    Int base;
    std::tie(ai, value, base) = Implementation::integralLiteral<Short>(a, buffer, error);
    VERIFY_PARSED(error, a, ai, "+'a'");
    CORRADE_COMPARE(value, 'a');
    CORRADE_COMPARE(base, 256);
}

void ParsersTest::integerLiteralBinary() {
    CharacterLiteral a{"-0o7_5"};

    Implementation::ParseError error;
    std::string buffer;
    const char* ai;
    Short value;
    Int base;
    std::tie(ai, value, base) = Implementation::integralLiteral<Short>(a, buffer, error);
    VERIFY_PARSED(error, a, ai, "-0o7_5");
    CORRADE_COMPARE(value, -075);
    CORRADE_COMPARE(base, 8);
}

void ParsersTest::floatLiteralInvalid() {
    Implementation::ParseError error;
    std::string buffer;

    CORRADE_VERIFY(!Implementation::floatingPointLiteral<Float>(CharacterLiteral{""}, buffer, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::ExpectedLiteral);

    CORRADE_VERIFY(!Implementation::floatingPointLiteral<Float>(CharacterLiteral{"+"}, buffer, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);

    CORRADE_VERIFY(!Implementation::floatingPointLiteral<Float>(CharacterLiteral{"A"}, buffer, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);

    CORRADE_VERIFY(!Implementation::floatingPointLiteral<Float>(CharacterLiteral{"_1"}, buffer, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);

    CORRADE_VERIFY(!Implementation::floatingPointLiteral<Float>(CharacterLiteral{"."}, buffer, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);

    CORRADE_VERIFY(!Implementation::floatingPointLiteral<Float>(CharacterLiteral{"0.e-"}, buffer, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);
}

void ParsersTest::floatLiteral() {
    CharacterLiteral a{"-1_.0_0e+5X"};

    Implementation::ParseError error;
    std::string buffer;
    const char* ai;
    Float value;
    std::tie(ai, value) = Implementation::floatingPointLiteral<Float>(a, buffer, error);
    VERIFY_PARSED(error, a, ai, "-1_.0_0e+5");
    CORRADE_COMPARE(value, -1.0e+5);
}

void ParsersTest::floatLiteralBinary() {
    CharacterLiteral a{"-0xbad_cafe_X"};

    Implementation::ParseError error;
    std::string buffer;
    const char* ai;
    Float value;
    std::tie(ai, value) = Implementation::floatingPointLiteral<Float>(a, buffer, error);
    VERIFY_PARSED(error, a, ai, "-0xbad_cafe_");
    UnsignedInt v = 0xbadcafe;
    CORRADE_COMPARE(value, -reinterpret_cast<Float&>(v));
}

void ParsersTest::stringLiteralInvalid() {
    Implementation::ParseError error;

    CORRADE_VERIFY(!Implementation::stringLiteral(CharacterLiteral{""}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::ExpectedLiteral);

    CORRADE_VERIFY(!Implementation::stringLiteral(CharacterLiteral{"\""}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::LiteralOutOfRange);

    CORRADE_VERIFY(!Implementation::stringLiteral(CharacterLiteral{"\"\n\""}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);
}

void ParsersTest::stringLiteralEmpty() {
    CharacterLiteral a{"\"\"X"};

    Implementation::ParseError error;
    const char* ai;
    std::string value;
    std::tie(ai, value) = Implementation::stringLiteral(a, error);
    VERIFY_PARSED(error, a, ai, "\"\"");
    CORRADE_COMPARE(value, "");
}

void ParsersTest::stringLiteral() {
    CharacterLiteral a{"\"abc ěšč\"X"};

    Implementation::ParseError error;
    const char* ai;
    std::string value;
    std::tie(ai, value) = Implementation::stringLiteral(a, error);
    VERIFY_PARSED(error, a, ai, "\"abc ěšč\"");
    CORRADE_COMPARE(value, "abc ěšč");
}

void ParsersTest::stringLiteralEscaped() {
    CharacterLiteral a{"\"abc \\n0\\\" heh\"X"};

    Implementation::ParseError error;
    const char* ai;
    std::string value;
    std::tie(ai, value) = Implementation::stringLiteral(a, error);
    VERIFY_PARSED(error, a, ai, "\"abc \\n0\\\" heh\"");
    CORRADE_COMPARE(value, "abc \n0\" heh");
}

void ParsersTest::stringLiteralConcatenated() {
    CharacterLiteral a{"\"abc\" /* comment */ \" ěšč\"X"};

    Implementation::ParseError error;
    const char* ai;
    std::string value;
    std::tie(ai, value) = Implementation::stringLiteral(a, error);
    VERIFY_PARSED(error, a, ai, "\"abc\" /* comment */ \" ěšč\"");
    CORRADE_COMPARE(value, "abc ěšč");
}

void ParsersTest::nameLiteralInvalid() {
    Implementation::ParseError error;

    CORRADE_VERIFY(!Implementation::nameLiteral(CharacterLiteral{""}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::ExpectedName);

    CORRADE_VERIFY(!Implementation::nameLiteral(CharacterLiteral{"a"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidName);

    CORRADE_VERIFY(!Implementation::nameLiteral(CharacterLiteral{"$"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::ExpectedIdentifier);
}

void ParsersTest::nameLiteral() {
    CharacterLiteral a{"%my_array,"};

    Implementation::ParseError error;
    const char* ai;
    std::string value;
    std::tie(ai, value) = Implementation::nameLiteral(a, error);
    VERIFY_PARSED(error, a, ai, "%my_array");
    CORRADE_COMPARE(value, "%my_array");
}

void ParsersTest::referenceLiteralInvalid() {
    Implementation::ParseError error;

    CORRADE_VERIFY(!Implementation::referenceLiteral(CharacterLiteral{""}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::ExpectedLiteral);

    CORRADE_VERIFY(!Implementation::referenceLiteral(CharacterLiteral{"a"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);

    CORRADE_VERIFY(!Implementation::referenceLiteral(CharacterLiteral{"%"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::ExpectedIdentifier);

    CORRADE_VERIFY(!Implementation::referenceLiteral(CharacterLiteral{"$%a"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidIdentifier);
}

void ParsersTest::referenceLiteralNull() {
    CharacterLiteral a{"null,"};

    Implementation::ParseError error;
    const char* ai;
    Containers::ArrayView<const char> value;
    std::tie(ai, value) = Implementation::referenceLiteral(a, error);
    VERIFY_PARSED(error, a, ai, "null");
    CORRADE_VERIFY(value.empty());
}

void ParsersTest::referenceLiteral() {
    CharacterLiteral a{"$my_mesh%my_array,"};

    Implementation::ParseError error;
    const char* ai;
    Containers::ArrayView<const char> value;
    std::tie(ai, value) = Implementation::referenceLiteral(a, error);
    VERIFY_PARSED(error, a, ai, "$my_mesh%my_array");
    CORRADE_COMPARE((std::string{value, value.size()}), "$my_mesh%my_array");
}

void ParsersTest::typeLiteralInvalid() {
    Implementation::ParseError error;

    CORRADE_VERIFY(!Implementation::typeLiteral(CharacterLiteral{""}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::ExpectedLiteral);

    CORRADE_VERIFY(!Implementation::typeLiteral(CharacterLiteral{"boo"}, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidLiteral);
}

void ParsersTest::typeLiteral() {
    CharacterLiteral a{"unsigned_int16,"};

    Implementation::ParseError error;
    const char* ai;
    Type value;
    std::tie(ai, value) = Implementation::typeLiteral(a, error);
    VERIFY_PARSED(error, a, ai, "unsigned_int16");
    CORRADE_COMPARE(value, Type::UnsignedShort);
}

void ParsersTest::propertyValueInvalid() {
    Implementation::ParseError error;
    std::string buffer;

    bool boolValue = {};
    Int integerValue = {};
    Float floatingPointValue = {};
    std::string stringValue;
    Containers::ArrayView<const char> referenceValue;
    Type typeValue = {};

    CORRADE_VERIFY(!Implementation::propertyValue(CharacterLiteral{""}, boolValue, integerValue, floatingPointValue, stringValue, referenceValue, typeValue, buffer, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::ExpectedPropertyValue);

    CORRADE_VERIFY(!Implementation::propertyValue(CharacterLiteral{"bleh"}, boolValue, integerValue, floatingPointValue, stringValue, referenceValue, typeValue, buffer, error).first);
    CORRADE_COMPARE(error.error, Implementation::ParseErrorType::InvalidPropertyValue);
}

void ParsersTest::propertyValueBool() {
    CharacterLiteral a{"true,"};

    Implementation::ParseError error;
    std::string buffer;
    bool boolValue = {};
    Int integerValue = {};
    Float floatingPointValue = {};
    std::string stringValue;
    Containers::ArrayView<const char> referenceValue;
    Type typeValue = {};
    const char* ai;
    Implementation::InternalPropertyType type;
    std::tie(ai, type) = Implementation::propertyValue(a, boolValue, integerValue, floatingPointValue, stringValue, referenceValue, typeValue, buffer, error);
    VERIFY_PARSED(error, a, ai, "true");
    CORRADE_COMPARE(type, Implementation::InternalPropertyType::Bool);
    CORRADE_COMPARE(boolValue, true);
}

void ParsersTest::propertyValueIntegral() {
    CharacterLiteral a{"17, 0.0"};

    Implementation::ParseError error;
    std::string buffer;
    bool boolValue = {};
    Int integerValue = {};
    Float floatingPointValue = {};
    std::string stringValue;
    Containers::ArrayView<const char> referenceValue;
    Type typeValue = {};
    const char* ai;
    Implementation::InternalPropertyType type;
    std::tie(ai, type) = Implementation::propertyValue(a, boolValue, integerValue, floatingPointValue, stringValue, referenceValue, typeValue, buffer, error);
    VERIFY_PARSED(error, a, ai, "17");
    CORRADE_COMPARE(type, Implementation::InternalPropertyType::Integral);
    CORRADE_COMPARE(integerValue, 17);
}

void ParsersTest::propertyValueCharacter() {
    CharacterLiteral a{"'a', 0.0"};

    Implementation::ParseError error;
    std::string buffer;
    bool boolValue = {};
    Int integerValue = {};
    Float floatingPointValue = {};
    std::string stringValue;
    Containers::ArrayView<const char> referenceValue;
    Type typeValue = {};
    const char* ai;
    Implementation::InternalPropertyType type;
    std::tie(ai, type) = Implementation::propertyValue(a, boolValue, integerValue, floatingPointValue, stringValue, referenceValue, typeValue, buffer, error);
    VERIFY_PARSED(error, a, ai, "'a'");
    CORRADE_COMPARE(type, Implementation::InternalPropertyType::Character);
    CORRADE_COMPARE(integerValue, 'a');
}

void ParsersTest::propertyValueBinary() {
    CharacterLiteral a{"0xff, 0.0"};

    Implementation::ParseError error;
    std::string buffer;
    bool boolValue = {};
    Int integerValue = {};
    Float floatingPointValue = {};
    std::string stringValue;
    Containers::ArrayView<const char> referenceValue;
    Type typeValue = {};
    const char* ai;
    Implementation::InternalPropertyType type;
    std::tie(ai, type) = Implementation::propertyValue(a, boolValue, integerValue, floatingPointValue, stringValue, referenceValue, typeValue, buffer, error);
    VERIFY_PARSED(error, a, ai, "0xff");
    CORRADE_COMPARE(type, Implementation::InternalPropertyType::Binary);
    CORRADE_COMPARE(integerValue, 0xff);
}

void ParsersTest::propertyValueFloatingPoint() {
    CharacterLiteral a{"15.0_0,"};

    Implementation::ParseError error;
    std::string buffer;
    bool boolValue = {};
    Int integerValue = {};
    Float floatingPointValue = {};
    std::string stringValue;
    Containers::ArrayView<const char> referenceValue;
    Type typeValue = {};
    const char* ai;
    Implementation::InternalPropertyType type;
    std::tie(ai, type) = Implementation::propertyValue(a, boolValue, integerValue, floatingPointValue, stringValue, referenceValue, typeValue, buffer, error);
    VERIFY_PARSED(error, a, ai, "15.0_0");
    CORRADE_COMPARE(type, Implementation::InternalPropertyType::Float);
    CORRADE_COMPARE(floatingPointValue, 15.0f);
}

void ParsersTest::propertyValueString() {
    CharacterLiteral a{"\"hello\","};

    Implementation::ParseError error;
    std::string buffer;
    bool boolValue = {};
    Int integerValue = {};
    Float floatingPointValue = {};
    std::string stringValue;
    Containers::ArrayView<const char> referenceValue;
    Type typeValue = {};
    const char* ai;
    Implementation::InternalPropertyType type;
    std::tie(ai, type) = Implementation::propertyValue(a, boolValue, integerValue, floatingPointValue, stringValue, referenceValue, typeValue, buffer, error);
    VERIFY_PARSED(error, a, ai, "\"hello\"");
    CORRADE_COMPARE(type, Implementation::InternalPropertyType::String);
    CORRADE_COMPARE(stringValue, "hello");
}

void ParsersTest::propertyValueReference() {
    CharacterLiteral a{"%my_array2,"};

    Implementation::ParseError error;
    std::string buffer;
    bool boolValue = {};
    Int integerValue = {};
    Float floatingPointValue = {};
    std::string stringValue;
    Containers::ArrayView<const char> referenceValue;
    Type typeValue = {};
    const char* ai;
    Implementation::InternalPropertyType type;
    std::tie(ai, type) = Implementation::propertyValue(a, boolValue, integerValue, floatingPointValue, stringValue, referenceValue, typeValue, buffer, error);
    VERIFY_PARSED(error, a, ai, "%my_array2");
    CORRADE_COMPARE(type, Implementation::InternalPropertyType::Reference);
    CORRADE_COMPARE((std::string{referenceValue, referenceValue.size()}), "%my_array2");
}

void ParsersTest::propertyValueReferenceNull() {
    CharacterLiteral a{"null,"};

    Implementation::ParseError error;
    std::string buffer;
    bool boolValue = {};
    Int integerValue = {};
    Float floatingPointValue = {};
    std::string stringValue;
    Containers::ArrayView<const char> referenceValue;
    Type typeValue = {};
    const char* ai;
    Implementation::InternalPropertyType type;
    std::tie(ai, type) = Implementation::propertyValue(a, boolValue, integerValue, floatingPointValue, stringValue, referenceValue, typeValue, buffer, error);
    VERIFY_PARSED(error, a, ai, "null");
    CORRADE_COMPARE(type, Implementation::InternalPropertyType::Reference);
    CORRADE_VERIFY(referenceValue.empty());
}

void ParsersTest::propertyValueType() {
    CharacterLiteral a{"float,"};

    Implementation::ParseError error;
    std::string buffer;
    bool boolValue = {};
    Int integerValue = {};
    Float floatingPointValue = {};
    std::string stringValue;
    Containers::ArrayView<const char> referenceValue;
    Type typeValue = {};
    const char* ai;
    Implementation::InternalPropertyType type;
    std::tie(ai, type) = Implementation::propertyValue(a, boolValue, integerValue, floatingPointValue, stringValue, referenceValue, typeValue, buffer, error);
    VERIFY_PARSED(error, a, ai, "float");
    CORRADE_COMPARE(type, Implementation::InternalPropertyType::Type);
    CORRADE_COMPARE(typeValue, Type::Float);
}

}}}}

CORRADE_TEST_MAIN(Magnum::OpenDdl::Test::ParsersTest)
