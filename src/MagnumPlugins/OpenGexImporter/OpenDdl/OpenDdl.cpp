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

#include "Document.h"
#include "Property.h"
#include "Structure.h"
#include "Validation.h"
#include "parsers.h"

#include <algorithm>
#include <tuple>

namespace Magnum { namespace OpenDdl {

Debug operator<<(Debug debug, const Type value) {
    switch(value) {
        #define _c(value) case Type::value: return debug << "OpenDdl::Type::" #value;
        _c(Bool)
        _c(UnsignedByte)
        _c(Byte)
        _c(UnsignedShort)
        _c(Short)
        _c(UnsignedInt)
        _c(Int)
        #ifndef MAGNUM_TARGET_WEBGL
        _c(UnsignedLong)
        _c(Long)
        #endif
        /** @todo Half */
        _c(Float)
        #ifndef MAGNUM_TARGET_GLES
        _c(Double)
        #endif
        _c(String)
        _c(Reference)
        _c(Type)
        _c(Custom)
        #undef _c
    }

    return debug << "OpenDdl::Type::(invalid)";
}

Debug operator<<(Debug debug, const PropertyType value) {
    switch(value) {
        #define _c(value) case PropertyType::value: return debug << "OpenDdl::PropertyType::" #value;
        _c(Bool)
        _c(UnsignedByte)
        _c(Byte)
        _c(UnsignedShort)
        _c(Short)
        _c(UnsignedInt)
        _c(Int)
        #ifndef MAGNUM_TARGET_WEBGL
        _c(UnsignedLong)
        _c(Long)
        #endif
        /** @todo Half */
        _c(Float)
        #ifndef MAGNUM_TARGET_GLES
        _c(Double)
        #endif
        _c(String)
        _c(Reference)
        _c(Type)
        #undef _c
    }

    return debug << "OpenDdl::PropertyType::(invalid)";
}

namespace Implementation {
    Debug operator<<(Debug debug, const InternalPropertyType value) {
        switch(value) {
            #define _c(value) case InternalPropertyType::value: return debug << "OpenDdl::Implementation::InternalPropertyType::" #value;
            _c(Bool)
            _c(Integral)
            _c(Float)
            _c(String)
            _c(Reference)
            _c(Type)
            _c(Binary)
            _c(Character)
            #undef _c
        }

        return debug << "OpenDdl::Implementation::InternalPropertyType::(invalid)";
    }
}

Document::Document() {
    /* First string is reserved for empty names */
    _strings.push_back(std::string{});
}

Document::~Document() = default;

bool Document::parse(Containers::ArrayReference<const char> data, const std::initializer_list<CharacterLiteral> structureIdentifiers, const std::initializer_list<CharacterLiteral> propertyIdentifiers) {
    _structureIdentifiers = {structureIdentifiers.begin(), structureIdentifiers.size()};
    _propertyIdentifiers = {propertyIdentifiers.begin(), propertyIdentifiers.size()};

    Implementation::ParseError error;
    std::string buffer;

    const char* i = Implementation::whitespace(data);
    i = parseStructureList(data.suffix(i), buffer, error);

    if(!i) {
        /* Calculate line number */
        std::size_t line = 1;
        for(char c: data.prefix(error.position)) if(c == '\n') ++line;

        Error e;
        e << "OpenDdl::Document::parse():";

        switch(error.error) {
            case Implementation::ParseErrorType::InvalidEscapeSequence:
                e << "invalid escape sequence";
                break;
            case Implementation::ParseErrorType::InvalidIdentifier:
                e << "invalid identifier";
                break;
            case Implementation::ParseErrorType::InvalidName:
                e << "invalid name";
                break;
            case Implementation::ParseErrorType::InvalidCharacterLiteral:
                e << "invalid character literal";
                break;
            case Implementation::ParseErrorType::InvalidPropertyValue:
                e << "invalid property value";
                break;
            case Implementation::ParseErrorType::LiteralOutOfRange:
                e << (error.type == Type::String ? "unterminated string literal" : "numeric literal out of range");
                break;
            case Implementation::ParseErrorType::ExpectedIdentifier:
                e << "expected identifier";
                break;
            case Implementation::ParseErrorType::ExpectedName:
                e << "expected name";
                break;
            case Implementation::ParseErrorType::ExpectedSeparator:
                e << "expected , character";
                break;
            case Implementation::ParseErrorType::ExpectedListStart:
                e << "expected { character";
                break;
            case Implementation::ParseErrorType::ExpectedListEnd:
                e << "expected } character";
                break;
            case Implementation::ParseErrorType::ExpectedArraySizeEnd:
                e << "expected ] character";
                break;
            case Implementation::ParseErrorType::ExpectedPropertyValue:
                e << "expected property value";
                break;
            case Implementation::ParseErrorType::ExpectedPropertyAssignment:
                e << "expected = character";
                break;
            case Implementation::ParseErrorType::ExpectedPropertyListEnd:
                e << "expected ) character";
                break;

            case Implementation::ParseErrorType::InvalidLiteral:
            case Implementation::ParseErrorType::ExpectedLiteral: {
                e << (error.error == Implementation::ParseErrorType::InvalidLiteral ? "invalid" : "expected");

                switch(error.type) {
                    #define _c(type, identifier) \
                        case Type::type: e << #identifier; break;
                    _c(Bool, bool)
                    _c(Byte, int8)
                    _c(UnsignedByte, unsigned_int8)
                    _c(Short, int16)
                    _c(UnsignedShort, unsigned_int16)
                    _c(Int, int32)
                    _c(UnsignedInt, unsigned_int32)
                    #ifndef MAGNUM_TARGET_WEBGL
                    _c(Long, int64)
                    _c(UnsignedLong, unsigned_int64)
                    #endif
                    /** @todo Half */
                    _c(Float, float)
                    #ifndef MAGNUM_TARGET_GLES
                    _c(Double, double)
                    #endif
                    _c(String, string)
                    _c(Reference, ref)
                    _c(Type, type)
                    #undef _c
                    case Type::Custom: CORRADE_ASSERT_UNREACHABLE();
                }

                e << "literal";

                break;
            }

            case Implementation::ParseErrorType::NoError:
                CORRADE_ASSERT_UNREACHABLE();
        }

        e << "on line" << line;

        return false;
    }

    return true;
}

const char* Document::parseProperty(const Containers::ArrayReference<const char> data, std::string& buffer, const Int identifier, Implementation::ParseError& error) {
    bool boolValue;
    Int integerValue;
    Float floatValue;
    std::string stringValue;
    Type typeValue;

    const char* i;
    Implementation::InternalPropertyType type;
    std::tie(i, type) = Implementation::propertyValue(data, boolValue, integerValue, floatValue, stringValue, typeValue, buffer, error);

    if(!i) return nullptr;

    std::size_t position = 0;
    switch(type) {
        case Implementation::InternalPropertyType::Bool:
            position = _bools.size();
            _bools.push_back(boolValue);
            break;
        case Implementation::InternalPropertyType::Binary:
        case Implementation::InternalPropertyType::Character:
        case Implementation::InternalPropertyType::Integral:
            position = _ints.size();
            _ints.push_back(integerValue);
            break;
        case Implementation::InternalPropertyType::Float:
            position = _floats.size();
            _floats.push_back(floatValue);
            break;
        case Implementation::InternalPropertyType::String:
        case Implementation::InternalPropertyType::Reference:
            position = _strings.size();
            _strings.push_back(stringValue);
            break;
        case Implementation::InternalPropertyType::Type:
            position = _types.size();
            _types.push_back(typeValue);
            break;
    }

    _properties.emplace_back(identifier, type, position);
    return i;
}

namespace Implementation {

template<> struct ExtractDataListItem<Type::Bool> {
    static const char* extract(const Containers::ArrayReference<const char> data, Document& document, std::string&, Implementation::ParseError& error) {
        const char* i;
        bool value;
        std::tie(i, value) = Implementation::boolLiteral(data, error);
        document.data<bool>().push_back(value);
        return i;
    }
};

template<class T> struct ExtractIntegralDataListItem {
    static const char* extract(const Containers::ArrayReference<const char> data, Document& document, std::string& buffer, Implementation::ParseError& error) {
        const char* i;
        T value;
        std::tie(i, value, std::ignore) = Implementation::integralLiteral<T>(data, buffer, error);
        document.data<T>().push_back(value);
        return i;
    }
};
#define _c(T) \
    template<> struct ExtractDataListItem<Type::T>: ExtractIntegralDataListItem<T> {};
_c(UnsignedByte)
_c(Byte)
_c(UnsignedShort)
_c(Short)
_c(UnsignedInt)
_c(Int)
#ifndef MAGNUM_TARGET_WEBGL
_c(UnsignedLong)
_c(Long)
#endif
#undef _c

template<class T> struct ExtractFloatingPointDataListItem {
    static const char* extract(const Containers::ArrayReference<const char> data, Document& document, std::string& buffer, Implementation::ParseError& error) {
        const char* i;
        T value;
        std::tie(i, value) = Implementation::floatingPointLiteral<T>(data, buffer, error);
        document.data<T>().push_back(value);
        return i;
    }
};
#define _c(T) \
    template<> struct ExtractDataListItem<Type::T>: ExtractFloatingPointDataListItem<T> {};
/** @todo Half */
_c(Float)
#ifndef MAGNUM_TARGET_GLES
_c(Double)
#endif
#undef _c

template<> struct ExtractDataListItem<Type::String> {
    static const char* extract(const Containers::ArrayReference<const char> data, Document& document, std::string&, Implementation::ParseError& error) {
        const char* i;
        std::string value;
        std::tie(i, value) = Implementation::stringLiteral(data, error);
        document.data<std::string>().push_back(std::move(value));
        return i;
    }
};

template<> struct ExtractDataListItem<Type::Reference> {
    static const char* extract(const Containers::ArrayReference<const char> data, Document& document, std::string&, Implementation::ParseError& error) {
        const char* i;
        std::string value;
        std::tie(i, value) = Implementation::referenceLiteral(data, error);
        document.data<std::string>().push_back(std::move(value));
        return i;
    }
};

template<> struct ExtractDataListItem<Type::Type> {
    static const char* extract(const Containers::ArrayReference<const char> data, Document& document, std::string&, Implementation::ParseError& error) {
        const char* i;
        Type value;
        std::tie(i, value) = Implementation::typeLiteral(data, error);
        document.data<Type>().push_back(value);
        return i;
    }
};

}

namespace {

template<Type type> std::pair<const char*, std::size_t> dataList(const Containers::ArrayReference<const char> data, Document& document, std::string& buffer, Implementation::ParseError& error) {
    const char* i = data;
    std::size_t j = 0;
    for(; i && i != data.end() && *i != '}'; ) {
        if(j) {
            if(*i != ',') {
                error = {Implementation::ParseErrorType::ExpectedSeparator, i};
                return {};
            }

            i = Implementation::whitespace(data.suffix(i + 1));
        }

        i = Implementation::ExtractDataListItem<type>::extract(data.suffix(i), document, buffer, error);

        i = Implementation::whitespace(data.suffix(i));

        ++j;
    }

    return {i, j};
}

template<Type type> std::pair<const char*, std::size_t> dataArrayList(const Containers::ArrayReference<const char> data, Document& document, std::string& buffer, const std::size_t subArraySize, Implementation::ParseError& error) {
    if(!subArraySize) return dataList<type>(data, document, buffer, error);

    const char* i = data;
    std::size_t j = 0;
    for(; i && i != data.end() && *i != '}'; ) {
        if(j) {
            if(*i != ',') {
                error = {Implementation::ParseErrorType::ExpectedSeparator, i};
                return {};
            }

            i = Implementation::whitespace(data.suffix(i + 1));
        }

        if(i == data.end() || *i != '{') {
            error = {Implementation::ParseErrorType::ExpectedListStart, i};
            return {};
        }

        i = Implementation::whitespace(data.suffix(i + 1));

        for(std::size_t k = 0; k != subArraySize; ++k) {
            if(k) {
                if(i && (i == data.end() || *i != ',')) {
                    error = {Implementation::ParseErrorType::ExpectedSeparator, i};
                    return {};
                }

                i = Implementation::whitespace(data.suffix(i + 1));
            }

            i = Implementation::ExtractDataListItem<type>::extract(data.suffix(i), document, buffer, error);

            i = Implementation::whitespace(data.suffix(i));
        }

        if(i == data.end() || *i != '}') {
            error = {Implementation::ParseErrorType::ExpectedListEnd, i};
            return {};
        }

        i = Implementation::whitespace(data.suffix(i + 1));

        ++j;
    }

    return {i, j*subArraySize};
}

Int identifierId(const Containers::ArrayReference<const char> data, Containers::ArrayReference<const CharacterLiteral> identifiers) {
    Int i = 0;
    for(const Containers::ArrayReference<const char> identifier: identifiers) {
        if(Implementation::equals(data, identifier)) return i;
        ++i;
    }

    return UnknownIdentifier;
}

}

std::pair<const char*, std::size_t> Document::parseStructure(const Containers::ArrayReference<const char> data, std::string& buffer, Implementation::ParseError& error) {
    /* Identifier */
    const char* const structureIdentifier = Implementation::identifier(data, error);
    if(!structureIdentifier) return {};

    /* Decide whether primitive or custom */
    const char* i;
    Type type;
    std::tie(i, type) = Implementation::possiblyTypeLiteral(data.prefix(structureIdentifier));

    /* Primitive */
    if(i) {
        i = Implementation::whitespace(data.suffix(structureIdentifier));

        /* Array */
        Int subArraySize = 0;
        if(i != data.end() && *i == '[') {
            i = Implementation::whitespace(data.suffix(i + 1));

            std::tie(i, subArraySize, std::ignore) = Implementation::integralLiteral<Int>(data.suffix(i), buffer, error);

            if(!i) return {};

            i = Implementation::whitespace(data.suffix(i));

            if(i == data.end() || *i != ']') {
                error = {Implementation::ParseErrorType::ExpectedArraySizeEnd, i};
                return {};
            }

            i = Implementation::whitespace(data.suffix(i + 1));
        }

        /* Name */
        std::size_t name = 0;
        if(i && i != data.end() && (*i == '%' || *i == '$')) {
            std::string s;
            std::tie(i, s) = Implementation::nameLiteral(data.suffix(i), error);
            name = _strings.size();
            _strings.push_back(std::move(s));

            i = Implementation::whitespace(data.suffix(i));
        }

        /* Propagate errors */
        if(!i) return {};

        /* Data list */
        if(i == data.end() || *i != '{') {
            error = {Implementation::ParseErrorType::ExpectedListStart, i};
            return {};
        }

        i = Implementation::whitespace(data.suffix(i + 1));

        std::size_t dataBegin = 0, dataSize = 0;
        switch(type) {
            #define _c(type) \
            case Type::type: \
                dataBegin = dataPosition<Type::type>(); \
                std::tie(i, dataSize) = dataArrayList<Type::type>(data.suffix(i), *this, buffer, subArraySize, error); break;
            _c(Bool)
            _c(UnsignedByte)
            _c(Byte)
            _c(UnsignedShort)
            _c(Short)
            _c(UnsignedInt)
            _c(Int)
            #ifndef MAGNUM_TARGET_WEBGL
            _c(UnsignedLong)
            _c(Long)
            #endif
            /** @todo Half */
            _c(Float)
            #ifndef MAGNUM_TARGET_GLES
            _c(Double)
            #endif
            _c(String)
            _c(Reference)
            _c(Type)
            #undef _c
            case Type::Custom:
                CORRADE_ASSERT_UNREACHABLE();
        }

        /* Propagate errors */
        if(!i) return {};

        i = Implementation::whitespace(data.suffix(i));

        if(i == data.end() || *i != '}') {
            error = {Implementation::ParseErrorType::ExpectedListEnd, i};
            return {};
        }

        _structures.emplace_back(type, name, subArraySize, dataBegin, dataSize, _structures.size() + 1);
        return {i + 1, _structures.size() - 1};

    /* Custom structure */
    } else {
        const Int structureIdentifierId = identifierId(data.prefix(structureIdentifier), _structureIdentifiers);

        i = Implementation::whitespace(data.suffix(structureIdentifier));

        /* Name */
        std::size_t name = 0;
        if(i && i != data.end() && (*i == '%' || *i == '$')) {
            std::string s;
            std::tie(i, s) = Implementation::nameLiteral(data.suffix(i), error);
            name = _strings.size();
            _strings.push_back(std::move(s));

            i = Implementation::whitespace(data.suffix(i));
        }

        const std::size_t propertyBegin = _properties.size();
        std::size_t propertySize = 0;

        /* Property list */
        if(i && i != data.end() && *i == '(') {
            i = Implementation::whitespace(data.suffix(i + 1));

            while(i && i != data.end() && *i != ')') {
                if(propertySize) {
                    if(*i != ',') {
                        error = {Implementation::ParseErrorType::ExpectedSeparator, i};
                        return {};
                    }

                    i = Implementation::whitespace(data.suffix(i + 1));
                }

                const char* const propertyIdentifier = Implementation::identifier(data.suffix(i), error);
                if(!propertyIdentifier) return {};

                const Int propertyIdentifierId = identifierId(data.slice(i, propertyIdentifier), _propertyIdentifiers);

                /* Propagate errors */
                if(!i) return {};

                i = Implementation::whitespace(data.suffix(propertyIdentifier));

                if(i == data.end() || *i != '=') {
                    error = {Implementation::ParseErrorType::ExpectedPropertyAssignment, i};
                    return {};
                }

                i = Implementation::whitespace(data.suffix(i + 1));

                i = parseProperty(data.suffix(i), buffer, propertyIdentifierId, error);

                i = Implementation::whitespace(data.suffix(i));

                ++propertySize;
            }

            /* Propagate errors */
            if(!i) return {};

            if(i == data.end() || *i != ')') {
                error = {Implementation::ParseErrorType::ExpectedPropertyListEnd, i};
                return {};
            }

            i = Implementation::whitespace(data.suffix(i + 1));
        }

        /* Structure start */
        if(i == data.end() || *i != '{') {
            error = {Implementation::ParseErrorType::ExpectedListStart, i};
            return {};
        }

        i = Implementation::whitespace(data.suffix(i + 1));

        const std::size_t position = _structures.size();
        _structures.emplace_back();

        /* Substructure */
        i = parseStructureList(data.suffix(i), buffer, error);

        /* Propagate errors */
        if(!i) return {};

        i = Implementation::whitespace(data.suffix(i));

        /* Structure end */
        if(i == data.end() || *i != '}') {
            error = {Implementation::ParseErrorType::ExpectedListEnd, i};
            return {};
        }

        /* First child is implicitly the next one, if no substructures were
           parsed, the "child" index is set to 0. */
        const std::size_t firstChild = position + 1 == _structures.size() ? 0 : position + 1;

        /* Next sibling is implicitly the one after all children. If this is
           the last structure in given list, the "next" index is set to 0 in
           parseStructureList() below. */
        _structures[position] = StructureData{structureIdentifierId, name, propertyBegin, propertySize, firstChild, _structures.size()};

        return {i + 1, position};
    }
}

const char* Document::parseStructureList(const Containers::ArrayReference<const char> data, std::string& buffer, Implementation::ParseError& error) {
    const std::size_t listStart = _structures.size();

    /* Parse all structures in the list */
    const char* i = data;
    std::size_t last;
    while(i && i != data.end() && *i != '}') {
        std::tie(i, last) = parseStructure(data.suffix(i), buffer, error);
        i = Implementation::whitespace(data.suffix(i));
    }

    /* Last structure in the list has no next */
    if(listStart != _structures.size()) _structures[last].next = 0;

    return i;
}

bool Document::validate(const Validation::Structures allowedRootStructures, const std::initializer_list<Validation::Structure> structures) const {
    std::vector<Int> countsBuffer;
    countsBuffer.reserve(structures.size());

    /* Check that there are no primitive structures in root */
    for(const Structure s: children()) if(!s.isCustom()) {
        Error() << "OpenDdl::Document::validate(): unexpected primitive structure in root";
        return false;
    }

    /* Check custom structures */
    return validateLevel(findFirstChild(), allowedRootStructures, structures, countsBuffer);
}

bool Document::validateLevel(const std::optional<Structure> first, const std::initializer_list<std::pair<Int, std::pair<Int, Int>>> allowedStructures, const std::initializer_list<Validation::Structure> structures, std::vector<Int>& counts) const {
    counts.assign(allowedStructures.size(), 0);

    /* Count number of custom structures in this level */
    for(std::optional<Structure> it = first; it; it = it->findNext()) {
        const Structure s = *it;

        if(!s.isCustom() || s.identifier() == UnknownIdentifier) continue;

        /* Verify that the structure is allowed (ignoring unknown ones) */
        auto found = std::find_if(allowedStructures.begin(), allowedStructures.end(), [s](const std::pair<Int, std::pair<Int, Int>>& v){ return v.first == s.identifier(); });
        if(found == allowedStructures.end()) {
            Error() << "OpenDdl::Document::validate(): unexpected structure" << structureName(s.identifier());
            return false;
        }

        /* Verify that we don't exceed allowed count */
        const std::size_t i = found - allowedStructures.begin();
        if(++counts[i] > found->second.second && found->second.second) {
            Error() << "OpenDdl::Document::validate(): too many" << structureName(s.identifier()) << "structures, got" << counts[i] << "but expected max" << found->second.second;
            return false;
        }
    }

    /* Verify that all required structures are there */
    {
        std::size_t i = 0;
        for(const std::pair<Int, std::pair<Int, Int>> allowedStructure: allowedStructures) {
            CORRADE_INTERNAL_ASSERT(allowedStructure.second.first >= 0 && (allowedStructure.second.second == 0 || allowedStructure.second.second >= allowedStructure.second.first));

            if(allowedStructure.second.first > counts[i]) {
                Error() << "OpenDdl::Document::validate(): too little" << structureName(allowedStructure.first) << "structures, got" << counts[i] << "but expected min" << allowedStructure.second.first;
                return false;
            }

            ++i;
        }
    }

    /* Descend into substructures (breadth-first) */
    for(std::optional<Structure> it = first; it; it = it->findNext()) {
        const Structure s = *it;

        if(!s.isCustom() || s.identifier() == UnknownIdentifier) continue;

        auto found = std::find_if(structures.begin(), structures.end(), [s](const Validation::Structure& v){ return v.identifier() == s.identifier(); });
        CORRADE_ASSERT(found != structures.end(), "OpenDdl::Document::validate(): missing specification for structure" << structureName(s.identifier()), false);
        if(!validateStructure(s, *found, structures, counts))
            return false;
    }

    return true;
}

bool Document::validateStructure(const Structure structure, const Validation::Structure& validation, const std::initializer_list<Validation::Structure> structures, std::vector<Int>& counts) const {
    counts.assign(validation.properties().size(), 0);

    /* Verify that there is no unexpected property (ignoring unknown ones) */
    for(const Property p: structure.properties()) {
        if(p.identifier() == UnknownIdentifier) continue;

        auto found = std::find_if(validation.properties().begin(), validation.properties().end(), [p](const Validation::Property& v){ return v.identifier() == p.identifier(); });
        if(found == validation.properties().end()) {
            Error() << "OpenDdl::Document::validate(): unexpected property" << propertyName(p.identifier()) << "in structure" << structureName(structure.identifier());
            return false;
        }

        if(!p.isTypeCompatibleWith(found->type())) {
            Error() << "OpenDdl::Document::validate(): unexpected type of property" << propertyName(p.identifier()) << ", expected" << found->type();
            return false;
        }

        counts[found - validation.properties().begin()] = 1;
    }

    /* Verify that all required properties are there */
    {
        std::size_t i = 0;
        for(const Validation::Property& p: validation.properties()) if(counts[i++] == 0 && p.isRequired()) {
            Error() << "OpenDdl::Document::validate(): expected property" << propertyName(p.identifier()) << "in structure" << structureName(structure.identifier());
            return false;
        }
    }

    /* Check that there are only primitive sub-structures with required type
       and size and in required amount */
    Int remainingPrimitiveCountAllowed = validation.primitiveCount();
    for(const Structure s: structure.children()) {
        if(s.isCustom()) continue;

        /* Error if there are no primitive sub-structures allowed or there is requirement on
           primitive count and we exceeded that requirement */
        if(!validation.primitives().size() || (validation.primitiveCount() && --remainingPrimitiveCountAllowed < 0)) {
            Error() << "OpenDdl::Document::validate(): expected exactly" << validation.primitiveCount() << "primitive sub-structures in structure" << structureName(structure.identifier());
            return false;
        }

        /* Verify that the primitive sub-structure has one of allowed types */
        auto found = std::find(validation.primitives().begin(), validation.primitives().end(), s.type());
        if(found == validation.primitives().end()) {
            Error() << "OpenDdl::Document::validate(): unexpected sub-structure of type" << s.type() << "in structure" << structureName(structure.identifier());
            return false;
        }

        /* Verify that the primitive sub-structure has required size */
        if(validation.primitiveArraySize() != 0 && s.arraySize() != validation.primitiveArraySize()) {
            Error() << "OpenDdl::Document::validate(): expected exactly" << validation.primitiveArraySize() << "values in" << structureName(structure.identifier()) << "sub-structure";
            return false;
        }
    }

    /* Error if there was requirement on primitive structures count and we had
       less primitive structures */
    if(validation.primitiveCount() && remainingPrimitiveCountAllowed > 0) {
        Error() << "OpenDdl::Document::validate(): expected exactly" << validation.primitiveCount() << "primitive sub-structures in structure" << structureName(structure.identifier());
        return false;
    }

    /* Check also custom substructures */
    return validateLevel(structure.findFirstChild(), validation.structures(), structures, counts);
}

const char* Document::structureName(const Int identifier) const {
    if(identifier == UnknownIdentifier) return "(unknown)";
    CORRADE_INTERNAL_ASSERT(std::size_t(identifier) < _structureIdentifiers.size());
    return *(_structureIdentifiers.begin() + identifier);
}

const char* Document::propertyName(const Int identifier) const {
    if(identifier == UnknownIdentifier) return "(unknown)";
    CORRADE_INTERNAL_ASSERT(std::size_t(identifier) < _propertyIdentifiers.size());
    return *(_propertyIdentifiers.begin() + identifier);
}

#ifndef DOXYGEN_GENERATING_OUTPUT
Document::StructureData::StructureData(const Type type, const std::size_t name, const std::size_t subArraySize, const std::size_t dataBegin, const std::size_t dataSize, const std::size_t next) noexcept: name{name}, primitive{type, subArraySize, dataBegin, dataSize}, next{next} {
    CORRADE_INTERNAL_ASSERT(type < Type::Custom);
}

Document::StructureData::StructureData(const Int type, const std::size_t name, const std::size_t propertyBegin, const std::size_t propertySize, const std::size_t firstChild, const std::size_t next) noexcept: name{name}, custom{type, propertyBegin, propertySize, firstChild}, next{next} {
    CORRADE_INTERNAL_ASSERT(type >= 0);
}
#endif

std::optional<Structure> Document::findFirstChild() const {
    return _structures.empty() ? std::nullopt : std::make_optional(Structure{*this, _structures.front()});
}

Structure Document::firstChild() const {
    const std::optional<Structure> s = findFirstChild();
    CORRADE_ASSERT(s, "OpenDdl::Document::firstChild(): the document is empty", *s);
    return *s;
}

Implementation::StructureList Document::children() const {
    return Implementation::StructureList{findFirstChild()};
}

std::optional<Structure> Document::findFirstChildOf(const Type type) const {
    for(std::optional<Structure> s = findFirstChild(); s; s = s->findNext())
        if(!s->isCustom() && s->type() == type) return s;
    return std::nullopt;
}

std::optional<Structure> Document::findFirstChildOf(const Int identifier) const {
    for(std::optional<Structure> s = findFirstChild(); s; s = s->findNext())
        if(s->isCustom() && s->identifier() == identifier) return s;
    return std::nullopt;
}

Structure Document::firstChildOf(const Type type) const {
    const std::optional<Structure> s = findFirstChildOf(type);
    CORRADE_ASSERT(s, "OpenDdl::Document::firstChildOf(): no such child", *s);
    return *s;
}

Structure Document::firstChildOf(const Int identifier) const {
    const std::optional<Structure> s = findFirstChildOf(identifier);
    CORRADE_ASSERT(s, "OpenDdl::Document::firstChildOf(): no such child", *s);
    return *s;
}

Implementation::StructureOfList Document::childrenOf(const Int identifier) const {
    return Implementation::StructureOfList{findFirstChildOf(identifier)};
}

Int Structure::identifier() const {
    CORRADE_ASSERT(isCustom(), "OpenDdl::Structure::identifier(): not a custom structure", {});
    return Int(_data.get().primitive.type) - Int(Type::Custom);
}

std::size_t Structure::arraySize() const {
    CORRADE_ASSERT(!isCustom(), "OpenDdl::Structure::arraySize(): not a primitive structure", {});
    return _data.get().primitive.size;
}

std::size_t Structure::subArraySize() const {
    CORRADE_ASSERT(!isCustom(), "OpenDdl::Structure::subArraySize(): not a primitive structure", {});
    return _data.get().primitive.subArraySize;
}

std::optional<Structure> Structure::findNextOf(const Int identifier) const {
    std::optional<Structure> s = *this;
    while((s = s->findNext()))
        if(s->isCustom() && s->identifier() == identifier) return s;
    return std::nullopt;
}

Int Structure::propertyCount() const {
    CORRADE_ASSERT(isCustom(), "OpenDdl::Structure::propertyCount(): not a custom structure", {});
    return _data.get().custom.propertiesSize;
}

Implementation::PropertyList Structure::properties() const {
    CORRADE_ASSERT(isCustom(), "OpenDdl::Structure::properties(): not a custom structure",
        Implementation::PropertyList(_document, 0, 0));
    return Implementation::PropertyList{_document, _data.get().custom.propertiesBegin, _data.get().custom.propertiesSize};
}

std::optional<Property> Structure::findPropertyOf(const Int identifier) const {
    std::optional<Property> found;
    CORRADE_ASSERT(isCustom(), "OpenDdl::Structure::findPropertyOf(): not a custom structure", {});
    for(std::size_t i = 0; i != _data.get().custom.propertiesSize; ++i) {
        const std::size_t j = _data.get().custom.propertiesBegin + i;
        if(_document.get()._properties[j].identifier == identifier) found = Property{_document, j};
    }
    return found;
}

Property Structure::propertyOf(const Int identifier) const {
    const std::optional<Property> p = findPropertyOf(identifier);
    CORRADE_ASSERT(p, "OpenDdl::Structure::propertyOf(): no such property", *p);
    return *p;
}

bool Structure::hasChildren() const {
    CORRADE_ASSERT(isCustom(), "OpenDdl::Structure::hasChildren(): not a custom structure", {});
    return _data.get().custom.firstChild;
}

std::optional<Structure> Structure::findFirstChild() const {
    CORRADE_ASSERT(isCustom(), "OpenDdl::Structure::firstChild(): not a custom structure", {});
    return hasChildren() ? std::make_optional(Structure{_document, _document.get()._structures[_data.get().custom.firstChild]}) : std::nullopt;
}

Structure Structure::firstChild() const {
    const std::optional<Structure> s = findFirstChild();
    CORRADE_ASSERT(s, "OpenDdl::Structure::firstChild(): no children", *s);
    return *s;
}

Implementation::StructureList Structure::children() const {
    CORRADE_ASSERT(isCustom(), "OpenDdl::Structure::children(): not a custom structure",
        Implementation::StructureList(findFirstChild()));
    return Implementation::StructureList{findFirstChild()};
}

std::optional<Structure> Structure::findFirstChildOf(const Type type) const  {
    for(std::optional<Structure> s = findFirstChild(); s; s = s->findNext())
        if(!s->isCustom() && s->type() == type) return s;
    return std::nullopt;
}

std::optional<Structure> Structure::findFirstChildOf(const Int identifier) const  {
    for(std::optional<Structure> s = findFirstChild(); s; s = s->findNext())
        if(s->isCustom() && s->identifier() == identifier) return s;
    return std::nullopt;
}

Structure Structure::firstChildOf(const Type type) const {
    std::optional<Structure> const s = findFirstChildOf(type);
    CORRADE_ASSERT(s, "OpenDdl::Structure::firstChildOf(): no such child", *s);
    return *s;
}

Structure Structure::firstChildOf(const Int identifier) const {
    std::optional<Structure> const s = findFirstChildOf(identifier);
    CORRADE_ASSERT(s, "OpenDdl::Structure::firstChildOf(): no such child", *s);
    return *s;
}

Implementation::StructureOfList Structure::childrenOf(const Int identifier) const {
    CORRADE_ASSERT(isCustom(), "OpenDdl::Structure::childrenOf(): not a custom structure",
        Implementation::StructureOfList(findFirstChildOf(identifier)));
    return Implementation::StructureOfList{findFirstChildOf(identifier)};
}

bool Property::isTypeCompatibleWith(PropertyType type) const {
    switch(type) {
        case PropertyType::UnsignedByte:
        case PropertyType::Byte:
        case PropertyType::UnsignedShort:
        case PropertyType::Short:
        case PropertyType::UnsignedInt:
        case PropertyType::Int:
        #ifndef MAGNUM_TARGET_WEBGL
        case PropertyType::UnsignedLong:
        case PropertyType::Long:
        #endif
            return _data.get().type == Implementation::InternalPropertyType::Integral ||
                   _data.get().type == Implementation::InternalPropertyType::Binary ||
                   _data.get().type == Implementation::InternalPropertyType::Character;

        /** @todo Half */
        case PropertyType::Float:
        #ifndef MAGNUM_TARGET_GLES
        case PropertyType::Double:
        #endif
            /** @todo Implement extracting float properties from binary */
            return _data.get().type == Implementation::InternalPropertyType::Float;

        case PropertyType::Bool:
        case PropertyType::String:
        case PropertyType::Reference:
        case PropertyType::Type:
            return _data.get().type == Implementation::InternalPropertyType(UnsignedByte(type));
    }

    CORRADE_ASSERT_UNREACHABLE();
}

}}
