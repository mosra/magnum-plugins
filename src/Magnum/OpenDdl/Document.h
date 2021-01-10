#ifndef Magnum_OpenDdl_Document_h
#define Magnum_OpenDdl_Document_h
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

/** @file
 * @brief Class @ref Magnum::OpenDdl::CharacterLiteral, @ref Magnum::OpenDdl::Document
 */

#include <string>
#include <vector>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>

#include "Magnum/OpenDdl/visibility.h"
#include "Magnum/OpenDdl/OpenDdl.h"
#include "Magnum/OpenDdl/Type.h"

namespace Magnum { namespace OpenDdl {

/**
@brief Character literal

Just a wrapper around @ref Corrade::Containers::ArrayView which ensures
that character literals have proper size.
*/
struct CharacterLiteral: Containers::ArrayView<const char> {
    /** @brief Constructor */
    /* MSVC 2015 can't handle {} here */
    template<std::size_t size> constexpr CharacterLiteral(const char(&string)[size]): Containers::ArrayView<const char>(string, size - 1) {}
};

namespace Implementation {
    template<Type> struct ExtractDataListItem;
    template<class> struct ExtractIntegralDataListItem;
    template<class> struct ExtractFloatingPointDataListItem;
    enum class ParseErrorType: UnsignedInt;
    struct ParseError;
    class StructureList;
    template<std::size_t size> class StructureOfList;
}

namespace Validation {
    typedef std::initializer_list<std::pair<Int, std::pair<Int, Int>>> Structures;
    class Structure;
}

/**
@brief OpenDDL document

Parser for the [OpenDDL](http://www.openddl.org) file format.

The parser loads the file into an in-memory structure, which is just a set of
flat arrays. When traversing the parsed document, all @ref Structure and
@ref Property objects are just tiny wrappers around references to the internal
data of the originating document, thus you must ensure that the document is
available for whole lifetime of these instances. On the other hand this allows
you to copy and store these instances without worrying about performance.

@section Magnum-OpenDdl-Document-usage Usage

To avoid needless allocations and string comparisons when using the parsed
document, all structure and property names are represented as integer IDs. To
parse a file, you first need to build a list of string names with their
corresponding IDs for both structure and property names. The following example
is a subset of the [OpenGEX](http://www.opengex.org) file format:

@code{.cpp}
namespace OpenGex {

enum: Int {
    GeometryObject,
    IndexArray,
    Mesh,
    VertexArray
};

const std::initializer_list<OpenDdl::CharacterLiteral> structures{
    "GeometryObject",
    "IndexArray",
    "Mesh",
    "VertexArray"
}

enum: Int {
    attrib,
    key,
    motion_blur,
    primitive,
    shadow,
    two_sided,
    visible
};

const std::initializer_list<OpenDdl::CharacterLiteral> properties{
    "attrib",
    "key",
    "motion_blur",
    "primitive",
    "shadow",
    "two_sided",
    "visible"
};

}
@endcode

Each enum value has corresponding string representation and the string
identifiers are then passed to @ref parse():

@code{.cpp}
OpenDdl::Document d;
bool parsed = d.parse(data, OpenGex::structures, OpenGex::properties);
@endcode

If the file contains structures or properties which are not included in the
identifer lists, these are parsed with @ref UnknownIdentifier ID. If the
document has syntax errors, the function returns @cpp false @ce and prints
detailed diagnostics on @ref Corrade::Utility::Error output. After parsing you
can traverse the document using IDs from the enums:

@code{.cpp}
for(OpenDdl::Structure geometryObject: d.childrenOf(OpenGex::GeometryObject)) {
    // Decide about primitive
    if(Containers::Optional<OpenDdl::Property> primitive = geometryObject.findPropertyOf(OpenGex::primitive)) {
        if(!primitive->isTypeCompatibleWith(OpenDdl::Type::String)) {
            // error ...
        }

        std::string str = primitive->as<std::string>();
        if(str == "triangles") {
            // ...
        } else if(str == "lines") {
            // ...
        } // ...
    } else {
        // ...
    }

    // Parse vertex array
    if(Containers::Optional<OpenDdl::Structure> vertexArray = geometryObject.findFirstChildOf(OpenGex::VertexArray)) {
        if(!vertexArray->hasChildren() || vertexArray->firstChild().type() != OpenDdl::Type::Float) {
            // error ...
        }

        Containers::ArrayView<const Float> vertexArray = vertexArray->firstChild().asArray<Float>();
        // ...

    } else {
        // error ...
    }
}
@endcode

As you can see, the error checking can get pretty tiresome after a while.
That's when document validation proves to be useful. The validation is just
rough and checks only proper document hierarchy, allowed structure and property
types, structure count and presence of required properties, but that's often
enough to avoid most of the redundant checks. You define which structures can
appear at document level and then for each structure what properties and which
substructures it can have. Again a (very stripped down) subset of OpenGEX
specification:

@code{.cpp}
namespace OpenGex {

using namespace OpenDdl::Validation;

// GeometryObject and Metric can be root structures
const Structures rootStructures{
    {GeometryObject, {}},
    {Metric, {}}
};

// Info about particular structures
const std::initializer_list<Structure> structureInfo{
    // Metric structure has required key string property and contains exactly
    // one float or string primitive substructure with exactly one value
    {Metric,            Properties{{key, PropertyType::String, RequiredProperty}},
                        Primitives{Type::Float,
                                   Type::String}, 1, 1},

    // GeometryObject structure has optional visible and shadow boolean
    // properties and one or more Mesh substructures
    {GeometryObject,    Properties{{visible, OpenDdl::PropertyType::Bool, OptionalProperty},
                                   {shadow, OpenDdl::PropertyType::Bool, OptionalProperty}},
                        Structures{{Mesh, {1, 0}}}},

    // Mesh structure has optional lod and primitive properties, at least one
    // VertexArray substructure and zero or more IndexArray substructures
    {Mesh,              Properties{{lod, OpenDdl::PropertyType::UnsignedInt, OptionalProperty},
                                   {primitive, OpenDdl::PropertyType::String, OptionalProperty}},
                        Structures{{VertexArray, {1, 0}},
                                   {IndexArray, {}}}},

    // IndexArray structure has exactly one unsigned primitive substructure
    // with any number of values
    {IndexArray,        Primitives{OpenDdl::Type::UnsignedByte,
                                   OpenDdl::Type::UnsignedShort,
                                   OpenDdl::Type::UnsignedInt}, 1, 0},

    // VertexArray structure has required attrib property and exactly one float
    // substructure with any number of values
    {VertexArray,       Properties{{attrib, OpenDdl::PropertyType::String, RequiredProperty}},
                        Primitives{OpenDdl::Type::Float}, 1, 0}
};

}
@endcode

You then pass it to @ref validate() and check the return value. As with
@ref parse(), structures with @ref UnknownIdentifier ID are ignored and if the
validation fails, detailed diagnostics is printed on @ref Corrade::Utility::Error
output:

@code{.cpp}
bool valid = d.validate(OpenGex::rootStructures, OpenGex::structureInfo);
@endcode

If the document is valid, you can access child structures and properties
directly with e.g. @ref Structure::firstChildOf(), @ref Structure::propertyOf()
etc. instead of using @ref Structure::findFirstChildOf(),
@ref Structure::findPropertyOf() etc. and checking return value all the time:

@code{.cpp}
// Decide about primitive
if(Containers::Optional<OpenDdl::Property> primitive = geometryObject.findPropertyOf(OpenGex::primitive)) {
    auto&& str = primitive->as<std::string>();
    if(str == "triangles") {
        // ...
    } else if(str == "lines") {
        // ...
    } // ...
} else {
    // ...
}

// Parse vertex array
OpenDdl::Structure vertexArray = geometryObject.firstChildOf(OpenGex::VertexArray);
auto&& attrib = vertexArray.propertyOf(OpenGex::attrib).as<std::string>();
if(attrib == "position") {
    // ...
} else if(attrib == "normal") {
    // ...
}

// Parse vertex array data
Containers::ArrayView<const Float> data = vertexArray.firstChild().asArray<Float>();
// ...
@endcode

@requires_gles Due to JavaScript limitations, on WebGL the `unsigned_int64`
    and `int64` types are not recognized.
*/
class MAGNUM_OPENDDL_EXPORT Document {
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
        /** @todo some sane way to ensure that the initializer lists are valid for whole Document lifetime */
        bool parse(Containers::ArrayView<const char> data, std::initializer_list<CharacterLiteral> structureIdentifiers, std::initializer_list<CharacterLiteral> propertyIdentifiers);

        /** @brief Whether the document is empty */
        bool isEmpty() { return _structures.empty(); }

        /**
         * @brief Find first top-level structure in the document
         *
         * Returns @ref Corrade::Containers::NullOpt if the document is empty.
         * @see @ref isEmpty(), @ref findFirstChildOf(), @ref firstChild(),
         *      @ref Structure::findFirstChild(), @ref Structure::findNext()
         */
        Containers::Optional<Structure> findFirstChild() const;

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
         *
         * @code{.cpp}
         * for(Structure s: document.children()) {
         *     // ...
         * }
         * @endcode
         *
         * @see @ref childrenOf(), @ref Structure::children()
         */
        Implementation::StructureList children() const;

        /**
         * @brief Find first custom top-level structure of given type
         *
         * Returns @ref Corrade::Containers::NullOpt if there is no such
         * structure.
         * @see @ref findFirstChild(), @ref firstChildOf(),
         *      @ref Structure::findFirstChildOf(), @ref Structure::findNextOf()
         */
        Containers::Optional<Structure> findFirstChildOf(Type type) const;

        /**
         * @brief Find first custom top-level structure of given identifier
         *
         * Returns @ref Corrade::Containers::NullOpt if there is no such
         * structure.
         * @see @ref findFirstChild(), @ref firstChildOf(),
         *      @ref Structure::findFirstChildOf(), @ref Structure::findNextOf()
         */
        Containers::Optional<Structure> findFirstChildOf(Int identifier) const;

        /** @overload */
        Containers::Optional<Structure> findFirstChildOf(std::initializer_list<Int> identifiers) const;
        Containers::Optional<Structure> findFirstChildOf(Containers::ArrayView<const Int> identifiers) const; /**< @overload */

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
         * @see @ref firstChild(), @ref findFirstChildOf(), @ref validate(),
         *      @ref Structure::firstChildOf()
         */
        Structure firstChildOf(Int identifier) const;

        /**
         * @brief Top-level structures of given identifier
         *
         * The returned list can be traversed using common range-based for:
         *
         * @code{.cpp}
         * for(Structure s: document.childrenOf(...)) {
         *     // ...
         * }
         * @endcode
         *
         * @see @ref children(), @ref Structure::childrenOf()
         * @todo This is ugly because it does not use { } like all the others
         */
        template<class ...T> Implementation::StructureOfList<sizeof...(T)+1> childrenOf(Int identifier, T... identifiers) const;

        /**
         * @brief Validate document
         *
         * Validates the document according to passed specification. Structures
         * and properties that have unknown names are ignored.
         *
         * Note that sub-array sizes, reference validity and some other things
         * are not checked, this is just to ensure that the document has
         * expected structure so you can use @ref firstChildOf(),
         * @ref Structure::firstChildOf(), @ref Structure::propertyOf() etc.
         * without additional validation.
         */
        bool validate(Validation::Structures allowedRootStructures, std::initializer_list<Validation::Structure> structures) const;

    private:
        struct PropertyData;
        struct StructureData;

        MAGNUM_OPENDDL_LOCAL const char* parseProperty(Containers::ArrayView<const char> data, std::vector<std::pair<std::size_t, Containers::ArrayView<const char>>>& references, std::string& buffer, Int position, Implementation::ParseError& error);
        MAGNUM_OPENDDL_LOCAL std::pair<const char*, std::size_t> parseStructure(std::size_t parent, Containers::ArrayView<const char> data, std::vector<std::pair<std::size_t, Containers::ArrayView<const char>>>& references, std::string& buffer, Implementation::ParseError& error);
        MAGNUM_OPENDDL_LOCAL const char* parseStructureList(std::size_t parent, Containers::ArrayView<const char> data, std::vector<std::pair<std::size_t, Containers::ArrayView<const char>>>& references, std::string& buffer, Implementation::ParseError& error);

        MAGNUM_OPENDDL_LOCAL std::size_t dereference(std::size_t originatingStructure, Containers::ArrayView<const char> reference) const;

        MAGNUM_OPENDDL_LOCAL bool validateLevel(const Containers::Optional<Structure>& first, Containers::ArrayView<const std::pair<Int, std::pair<Int, Int>>> allowedStructures, Containers::ArrayView<const Validation::Structure> structures, std::vector<Int>& counts) const;
        MAGNUM_OPENDDL_LOCAL bool validateStructure(Structure structure, const Validation::Structure& validation, Containers::ArrayView<const Validation::Structure> structures, std::vector<Int>& counts) const;

        MAGNUM_OPENDDL_LOCAL const char* structureName(Int identifier) const;
        MAGNUM_OPENDDL_LOCAL const char* propertyName(Int identifier) const;

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
        #ifndef CORRADE_TARGET_EMSCRIPTEN
        std::vector<Long> _longs;
        std::vector<UnsignedLong> _unsignedLongs;
        #endif
        /** @todo Half */
        std::vector<Float> _floats;
        std::vector<Double> _doubles;
        std::vector<std::string> _strings;
        std::vector<std::size_t> _references;
        std::vector<Type> _types;

        std::vector<PropertyData> _properties;
        std::vector<StructureData> _structures;

        Containers::ArrayView<const CharacterLiteral> _structureIdentifiers;
        Containers::ArrayView<const CharacterLiteral> _propertyIdentifiers;
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
#ifndef CORRADE_TARGET_EMSCRIPTEN
_c(UnsignedLong, _unsignedLongs)
_c(Long, _longs)
#endif
/** @todo Half */
_c(Float, _floats)
_c(Double, _doubles)
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
#ifndef CORRADE_TARGET_EMSCRIPTEN
_c(UnsignedLong, UnsignedLong)
_c(Long, Long)
#endif
/** @todo Half */
_c(Float, Float)
_c(Double, Double)
_c(String, std::string)
_c(Type, Type)
#undef _c
#endif

#ifndef DOXYGEN_GENERATING_OUTPUT
struct MAGNUM_OPENDDL_LOCAL Document::PropertyData {
    constexpr explicit PropertyData(Int identifier, Implementation::InternalPropertyType type, std::size_t position) noexcept: identifier{identifier}, type{type}, position{position} {}

    Int identifier;
    Implementation::InternalPropertyType type;
    std::size_t position;
};

struct MAGNUM_OPENDDL_LOCAL Document::StructureData {
    /* Needed for "placeholder" object in parseStructure() which is later
       replaced with real one */
    explicit StructureData() noexcept: name{}, custom{UnknownIdentifier, 0, 0, 0}, parent{0}, next{0} {}

    explicit StructureData(Type type, std::size_t name, std::size_t subArraySize, std::size_t dataBegin, std::size_t dataSize, std::size_t parent, std::size_t next) noexcept;

    explicit StructureData(Int type, std::size_t name, std::size_t propertyBegin, std::size_t propertySize, std::size_t firstChild, std::size_t parent, std::size_t next) noexcept;

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

    std::size_t parent;
    std::size_t next;
};
#endif

namespace Implementation {
    template<class T> struct ReturnTypeFor { typedef T Type; };
    template<> struct ReturnTypeFor<std::string> { typedef const std::string& Type; };
}

template<class ...T> inline Implementation::StructureOfList<sizeof...(T)+1> Document::childrenOf(Int identifier, T... identifiers) const {
    return Implementation::StructureOfList<sizeof...(T)+1>{findFirstChildOf({identifier, identifiers...}), identifier, identifiers...};
}

}}

#endif
