#ifndef Magnum_OpenDdl_Implementation_parsers_h
#define Magnum_OpenDdl_Implementation_parsers_h
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

#include <string>
#include <tuple>
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/Magnum.h>

#include "Magnum/OpenDdl/OpenDdl.h"

namespace Magnum { namespace OpenDdl { namespace Implementation {

enum class InternalPropertyType: UnsignedByte;

enum class ParseErrorType: UnsignedInt {
    NoError,
    InvalidEscapeSequence,
    InvalidIdentifier,
    InvalidName,
    InvalidCharacterLiteral,
    InvalidLiteral,
    InvalidPropertyValue,
    InvalidSubArraySize,
    LiteralOutOfRange,
    ExpectedIdentifier,
    ExpectedName,
    ExpectedLiteral,
    ExpectedSeparator,
    ExpectedListStart,
    ExpectedListEnd,
    ExpectedArraySizeEnd,
    ExpectedPropertyValue,
    ExpectedPropertyAssignment,
    ExpectedPropertyListEnd
};

Debug& operator<<(Debug& debug, ParseErrorType value);

struct ParseError {
    /*implicit*/ constexpr ParseError(): error{ParseErrorType::NoError}, type{}, position{} {}

    /*implicit*/ constexpr ParseError(ParseErrorType error, Type type, const char* position = nullptr): error{error}, type{type}, position{position} {}

    /*implicit*/ constexpr ParseError(ParseErrorType error, const char* position = nullptr): error{error}, type{}, position{position} {}

    ParseErrorType error;
    Type type;
    const char* position;
};

bool equals(Containers::ArrayView<const char> a, Containers::ArrayView<const char> b);

template<std::size_t size> const char* findLastOf(Containers::ArrayView<const char> data, const char(&characters)[size]) {
    for(const char* c = data.end(); c != data.begin(); --c)
        for(std::size_t i = 0; i != size - 1; ++i)
            if(*(c - 1) == characters[i]) return c - 1;

    return data.begin();
}

const char* whitespace(Containers::ArrayView<const char> data);
std::pair<const char*, char> escapedChar(Containers::ArrayView<const char> data, ParseError& error);
const char* escapedUnicode(Containers::ArrayView<const char> data, std::string& out, ParseError& error);
const char* identifier(Containers::ArrayView<const char> data, ParseError& error);

std::pair<const char*, bool> boolLiteral(Containers::ArrayView<const char> data, ParseError& error);
std::pair<const char*, char> characterLiteral(Containers::ArrayView<const char> data, ParseError& error);
template<class T> std::tuple<const char*, T, Int> integralLiteral(Containers::ArrayView<const char> data, std::string& buffer, ParseError& error);
template<class T> std::pair<const char*, T> floatingPointLiteral(Containers::ArrayView<const char> data, std::string& buffer, ParseError& error);
std::pair<const char*, std::string> stringLiteral(Containers::ArrayView<const char> data, ParseError& error);
std::pair<const char*, std::string> nameLiteral(Containers::ArrayView<const char> data, ParseError& error);
std::pair<const char*, Containers::ArrayView<const char>> referenceLiteral(Containers::ArrayView<const char> data, ParseError& error);
std::pair<const char*, Type> possiblyTypeLiteral(Containers::ArrayView<const char> data);
std::pair<const char*, Type> typeLiteral(Containers::ArrayView<const char> data, ParseError& error);

std::pair<const char*, InternalPropertyType> propertyValue(Containers::ArrayView<const char> data, bool& boolValue, Int& integerValue, Float& floatingPointValue, std::string& stringValue, Containers::ArrayView<const char>& referenceValue, Type& typeValue, std::string& buffer, ParseError& error);

}}}

#endif
