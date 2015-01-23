#ifndef Magnum_OpenDdl_Document_h
#define Magnum_OpenDdl_Document_h
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

/** @file
 * @brief Class @ref Magnum::OpenDdl::CharacterLiteral, @ref Magnum::OpenDdl::Document
 */

#include <string>
#include <vector>
#include <Corrade/Containers/Array.h>

#include "MagnumExternal/Optional/optional.hpp"
#include "MagnumPlugins/OpenGexImporter/OpenDdl/OpenDdl.h"
#include "MagnumPlugins/OpenGexImporter/OpenDdl/Type.h"

namespace Magnum { namespace OpenDdl {

/**
@brief Character literal

Just a wrapper around @ref Corrade::Containers::ArrayReference which ensures
that character literals have proper size.
*/
struct CharacterLiteral: Containers::ArrayReference<const char> {
    /** @brief Constructor */
    template<std::size_t size> constexpr CharacterLiteral(const char(&string)[size]): Containers::ArrayReference<const char>{string, size - 1} {}
};

namespace Implementation {
    template<Type> struct ExtractDataListItem;
    template<class> struct ExtractIntegralDataListItem;
    template<class> struct ExtractFloatingPointDataListItem;
    enum class ParseErrorType: UnsignedInt;
    struct ParseError;
    class StructureList;
    class StructureOfList;
}

/**
@brief OpenDDL document

@attention All @ref Structure and @ref Property instances are just references
    to internal data of originating document, thus you must ensure that the
    document is available for whole lifetime of these instances.

@requires_gl On OpenGL ES the `double` type is not recognized. Additionally,
    due to JavaScript limitations, on @ref MAGNUM_TARGET_WEBGL "WebGL" the
    `unsigned_int64` and `int64` types are not recognized.
*/
class Document {
    friend Property;
    friend Structure;
    template<Type> friend struct Implementation::ExtractDataListItem;
    template<class> friend struct Implementation::ExtractIntegralDataListItem;
    template<class> friend struct Implementation::ExtractFloatingPointDataListItem;

    public:
        /**
         * @brief Constructor
         *
         * Creates empty document.
         */
        explicit Document();

        /** @brief Copying is disabled */
        Document(const Document&) = delete;

        /** @brief Moving is disabled */
        Document(Document&&) = delete;

        ~Document();

        /** @brief Copying is disabled */
        Document& operator=(const Document&) = delete;

        /** @brief Moving is disabled */
        Document& operator=(Document&&) = delete;

        /**
         * @brief Parse data
         * @param data                      Document data
         * @param structureIdentifiers      Structure identifiers
         * @param propertyIdentifiers       Property identifiers
         * @return Whether the parsing succeeded
         *
         * The data are appended to already parsed data. Each identifier from
         * the lists is converted to ID corresponding to its position in the
         * list. If the parsing results in error, detailed info is printed on
         * error output and the document has undefined contents.
         *
         * After parsing, all references to structure data are valid until next
         * parse call.
         */
        bool parse(Containers::ArrayReference<const char> data, std::initializer_list<CharacterLiteral> structureIdentifiers, std::initializer_list<CharacterLiteral> propertyIdentifiers);

        /** @brief Whether the document is empty */
        bool isEmpty() { return _structures.empty(); }

        /**
         * @brief Find first top-level structure in the document
         *
         * Returns `std::nullopt` if the document is empty.
         * @see @ref isEmpty(), @ref findFirstChildOf(), @ref firstChild(),
         *      @ref Structure::findFirstChild(), @ref Structure::findNext()
         */
        std::optional<Structure> findFirstChild() const;

        /**
         * @brief First top-level structure in the document
         *
         * The document must not be empty.
         * @see @ref isEmpty(), @ref firstChildOf(), @ref findFirstChild(),
         *      @ref Structure::firstChild(), @ref Structure::findNext()
         */
        Structure firstChild() const;

        /**
         * @brief Top-level structures
         *
         * The returned list can be traversed using common range-based for:
         * @code
         * for(Structure s: document.children()) {
         *     // ...
         * }
         * @endcode
         * @see @ref childrenOf(), @ref Structure::children()
         */
        Implementation::StructureList children() const;

        /**
         * @brief Find first custom top-level structure of given type
         *
         * Returns `std::nullopt` if there is no such structure.
         * @see @ref findFirstChild(), @ref firstChildOf(),
         *      @ref Structure::findFirstChildOf(), @ref Structure::findNextOf()
         */
        std::optional<Structure> findFirstChildOf(Type type) const;

        /**
         * @brief Find first custom top-level structure of given identifier
         *
         * Returns `std::nullopt` if there is no such structure.
         * @see @ref findFirstChild(), @ref firstChildOf(),
         *      @ref Structure::findFirstChildOf(), @ref Structure::findNextOf()
         */
        std::optional<Structure> findFirstChildOf(Int identifier) const;

        /**
         * @brief First custom top-level structure of given type
         *
         * Expects that there is such structure.
         * @see @ref firstChild(), @ref findFirstChildOf(), @ref validate(),
         *      @ref Structure::firstChildOf()
         */
        Structure firstChildOf(Type type) const;

        /**
         * @brief First custom top-level structure of given identifier
         *
         * Expects that there is such structure.
         * @see @ref firstChild(), @ref findFirstChildOf(),
         *      @ref Structure::firstChildOf()
         */
        Structure firstChildOf(Int identifier) const;

        /**
         * @brief Top-level structures of given identifier
         *
         * The returned list can be traversed using common range-based for:
         * @code
         * for(Structure s: document.childrenOf(...)) {
         *     // ...
         * }
         * @endcode
         * @see @ref children(), @ref Structure::childrenOf()
         */
        Implementation::StructureOfList childrenOf(Int identifier) const;

    private:
        struct PropertyData;
        struct StructureData;

        const char* parseProperty(Containers::ArrayReference<const char> data, std::string& buffer, Int position, Implementation::ParseError& error);
        std::pair<const char*, std::size_t> parseStructure(Containers::ArrayReference<const char> data, std::string& buffer, Implementation::ParseError& error);
        const char* parseStructureList(Containers::ArrayReference<const char> data, std::string& buffer, Implementation::ParseError& error);

        template<class T> std::vector<T>& data();
        template<class T> const std::vector<T>& data() const;
        template<Type> std::size_t dataPosition() const;

        std::vector<bool> _bools;
        std::vector<Byte> _bytes;
        std::vector<UnsignedByte> _unsignedBytes;
        std::vector<Short> _shorts;
        std::vector<UnsignedShort> _unsignedShorts;
        std::vector<Int> _ints;
        std::vector<UnsignedInt> _unsignedInts;
        #ifndef MAGNUM_TARGET_WEBGL
        std::vector<Long> _longs;
        std::vector<UnsignedLong> _unsignedLongs;
        #endif
        /** @todo Half */
        std::vector<Float> _floats;
        #ifndef MAGNUM_TARGET_GLES
        std::vector<Double> _doubles;
        #endif
        std::vector<std::string> _strings;
        std::vector<Type> _types;

        std::vector<PropertyData> _properties;
        std::vector<StructureData> _structures;

        Containers::ArrayReference<const CharacterLiteral> _structureIdentifiers;
        Containers::ArrayReference<const CharacterLiteral> _propertyIdentifiers;
};

#ifndef DOXYGEN_GENERATING_OUTPUT
#define _c(T, member) \
    template<> inline std::vector<T>& Document::data() { return member; } \
    template<> inline const std::vector<T>& Document::data() const { return member; }
_c(bool, _bools)
_c(UnsignedByte, _unsignedBytes)
_c(Byte, _bytes)
_c(UnsignedShort, _unsignedShorts)
_c(Short, _shorts)
_c(UnsignedInt, _unsignedInts)
_c(Int, _ints)
#ifndef MAGNUM_TARGET_WEBGL
_c(UnsignedLong, _unsignedLongs)
_c(Long, _longs)
#endif
/** @todo Half */
_c(Float, _floats)
#ifndef MAGNUM_TARGET_GLES
_c(Double, _doubles)
#endif
_c(std::string, _strings)
_c(Type, _types)
#undef _c

#define _c(type, T) \
    template<> inline std::size_t Document::dataPosition<Type::type>() const { \
        return data<T>().size(); \
    }
_c(Bool, bool)
_c(UnsignedByte, UnsignedByte)
_c(Byte, Byte)
_c(UnsignedShort, UnsignedShort)
_c(Short, Short)
_c(UnsignedInt, UnsignedInt)
_c(Int, Int)
#ifndef MAGNUM_TARGET_WEBGL
_c(UnsignedLong, UnsignedLong)
_c(Long, Long)
#endif
/** @todo Half */
_c(Float, Float)
#ifndef MAGNUM_TARGET_GLES
_c(Double, Double)
#endif
_c(String, std::string)
_c(Reference, std::string)
_c(Type, Type)
#undef _c
#endif

#ifndef DOXYGEN_GENERATING_OUTPUT
struct Document::PropertyData {
    constexpr explicit PropertyData(Int identifier, Implementation::InternalPropertyType type, std::size_t position) noexcept: identifier{identifier}, type{type}, position{position} {}

    Int identifier;
    Implementation::InternalPropertyType type;
    std::size_t position;
};

struct Document::StructureData {
    constexpr explicit StructureData() noexcept: name{}, custom{UnknownIdentifier, 0, 0, 0}, next{0} {}

    explicit StructureData(Type type, std::size_t name, std::size_t subArraySize, std::size_t dataBegin, std::size_t dataSize, std::size_t next) noexcept;

    explicit StructureData(Int type, std::size_t name, std::size_t propertyBegin, std::size_t propertySize, std::size_t firstChild, std::size_t next) noexcept;

    std::size_t name;

    struct Primitive {
        constexpr explicit Primitive(Type type, std::size_t subArraySize, std::size_t begin, std::size_t size) noexcept: type{type}, subArraySize{subArraySize}, begin{begin}, size{size} {}

        Type type;
        std::size_t subArraySize;

        std::size_t begin;
        std::size_t size;
    };

    struct Custom {
        constexpr explicit Custom(Int identifier, std::size_t propertiesBegin, std::size_t propertiesSize, std::size_t firstChild) noexcept: identifier{Int(Type::Custom) + identifier}, propertiesBegin{propertiesBegin}, propertiesSize{propertiesSize}, firstChild{firstChild} {}

        Int identifier;

        std::size_t propertiesBegin;
        std::size_t propertiesSize;

        std::size_t firstChild;
    };

    union {
         Primitive primitive;
         Custom custom;
    };

    std::size_t next;
};
#endif

namespace Implementation {
    template<class T> struct ReturnTypeFor { typedef T Type; };
    template<> struct ReturnTypeFor<std::string> { typedef const std::string& Type; };
}

}}

#endif
