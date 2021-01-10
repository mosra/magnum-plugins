#ifndef Magnum_OpenDdl_Structure_h
#define Magnum_OpenDdl_Structure_h
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
 * @brief Class @ref Magnum::OpenDdl::Structure, enum @ref Magnum::OpenDdl::Type
 */

#ifdef _MSC_VER
#include <algorithm> /* For std::min() */
#endif
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/Assert.h>

#include "Magnum/OpenDdl/Document.h"

namespace Magnum { namespace OpenDdl {

namespace Implementation {
    class StructureIterator;
    class StructureOfIterator;
    class PropertyList;
}

/**
@brief OpenDDL structure

See @ref Document for more information.

@attention The class consists just of reference to internal data in originating
    @ref Document instance, thus you must ensure that the document is available
    for whole instance lifetime. On the other hand you can copy the instance
    however you like without worrying about performance.
*/
class MAGNUM_OPENDDL_EXPORT Structure {
    public:
        /**
         * @brief Equality operator
         *
         * Returns @cpp true @ce if the two instances refer to the same
         * structure in the same document.
         */
        bool operator==(const Structure& other) const {
            return &_document.get() == &other._document.get() &&
                   &_data.get() == &other._data.get();
        }

        /** @brief Non-equality operator */
        bool operator!=(const Structure& other) const { return !operator==(other); }

        /**
         * @brief Whether the structure is custom
         *
         * @see @ref type(), @ref identifier()
         */
        bool isCustom() const { return type() == Type::Custom; }

        /**
         * @brief Structure type
         *
         * @see @ref isCustom(), @ref identifier()
         */
        Type type() const { return std::min(Type::Custom, _data.get().primitive.type); }

        /**
         * @brief Custom structure identifier
         *
         * The structure must be custom.
         * @see @ref isCustom(), @ref UnknownIdentifier
         */
        Int identifier() const;

        /** @brief Whether the structure has a name */
        bool hasName() const { return _data.get().name != 0; }

        /**
         * @brief Structure name
         *
         * @see @ref hasName()
         */
        const std::string& name() const { return _document.get()._strings[_data.get().name]; }

        /**
         * @brief Array size
         *
         * The structure must not be custom.
         * @see @ref isCustom()
         */
        std::size_t arraySize() const;

        /**
         * @brief Subarray size
         *
         * The structure must not be custom. If the array has no subarrays,
         * @cpp 0 @ce is returned.
         * @see @ref isCustom()
         */
        std::size_t subArraySize() const;

        /**
         * @brief Structure data
         *
         * The structure must not be custom, must be of corresponding type and
         * the array must have exactly one item.
         * @see @ref isCustom(), @ref type(), @ref arraySize(), @ref asArray(),
         *      @ref asReference()
         */
        template<class T>
        #ifndef DOXYGEN_GENERATING_OUTPUT
        typename Implementation::ReturnTypeFor<T>::Type
        #else
        const T&
        #endif
        as() const;

        /**
         * @brief Reference structure data
         *
         * The structure must not be custom, must be of @ref Type::Reference
         * and the array must have exactly one item. Returns referenced
         * structure or @ref Corrade::Containers::NullOpt if the reference is
         * `null`.
         * @see @ref isCustom(), @ref type(), @ref arraySize()
         */
        Containers::Optional<Structure> asReference() const;

        /**
         * @brief Structure data array
         *
         * The structure must not be custom and must be of corresponding type.
         * @see @ref isCustom(), @ref type(), @ref subArraySize(), @ref as()
         */
        template<class T> Containers::ArrayView<const T> asArray() const;

        /**
         * @brief Reference structure data array
         *
         * The structure must not be custom and must be of @ref Type::Reference.
         * For each item returns referenced structure or
         * @ref Corrade::Containers::NullOpt if the reference is `null`.
         * @see @ref isCustom(), @ref type(), @ref arraySize()
         */
        Containers::Array<Containers::Optional<Structure>> asReferenceArray() const;

        /**
         * @brief Parent structure
         *
         * Returns @ref Corrade::Containers::NullOpt if the structure is
         * top-level.
         */
        Containers::Optional<Structure> parent() const;

        /**
         * @brief Find next sibling structure
         *
         * Returns @ref Corrade::Containers::NullOpt if the structure is last
         * in given level.
         * @see @ref findNextOf(), @ref firstChild()
         */
        Containers::Optional<Structure> findNext() const {
            return _data.get().next ? Containers::optional(Structure{_document, _document.get()._structures[_data.get().next]}) : Containers::NullOpt;
        }

        /**
         * @brief Find next custom sibling structure of given identifier
         *
         * Returns @ref Corrade::Containers::NullOpt if there is no such
         * structure.
         * @see @ref findNext(), @ref findNextSame(), @ref findFirstChildOf()
         */
        Containers::Optional<Structure> findNextOf(Int identifier) const;

        /** @overload */
        Containers::Optional<Structure> findNextOf(std::initializer_list<Int> identifiers) const {
            return findNextOf({identifiers.begin(), identifiers.size()});
        }
        Containers::Optional<Structure> findNextOf(Containers::ArrayView<const Int> identifiers) const; /**< @overload */

        /**
         * @brief Find next custom sibling structure of the same identifier
         *
         * The structure must be custom. Equivalent to calling
         * @cpp structure.findNextOf(structure.identifier()) @ce. Returns
         * @ref Corrade::Containers::NullOpt if there is no such structure.
         * @see @ref isCustom(), @ref findNext(), @ref findNextOf()
         */
        Containers::Optional<Structure> findNextSame() const { return findNextOf(identifier()); }

        /**
         * @brief Whether the structure has properties
         *
         * The structure must be custom.
         * @see @ref isCustom()
         */
        bool hasProperties() const { return propertyCount(); }

        /**
         * @brief Property count
         *
         * The structure must be custom.
         * @see @ref isCustom()
         */
        Int propertyCount() const;

        /**
         * @brief Custom structure properties
         *
         * The structure must be custom. The returned list can be traversed
         * using common range-based for:
         *
         * @code{.cpp}
         * for(Property p: structure.properties()) {
         *     // ...
         * }
         * @endcode
         *
         * @see @ref isCustom(), @ref children()
         */
        Implementation::PropertyList properties() const;

        /**
         * @brief Find custom structure property of given identifier
         *
         * The structure must be custom. Returns
         * @ref Corrade::Containers::NullOpt if the structure doesn't contain
         * any property of given identifier.
         * @see @ref isCustom(), @ref propertyOf()
         */
        Containers::Optional<Property> findPropertyOf(Int identifier) const;

        /**
         * @brief Custom structure property of given identifier
         *
         * The structure must be custom and there must be such property.
         * @see @ref isCustom(), @ref Document::validate(),
         *      @ref findPropertyOf()
         */
        Property propertyOf(Int identifier) const;

        /**
         * @brief Whether the structure has children
         *
         * The structure must be custom.
         * @see @ref isCustom()
         */
        bool hasChildren() const;

        /**
         * @brief Find first child structure
         *
         * The structure must be custom. Returns
         * @ref Corrade::Containers::NullOpt if the structure has no children.
         * @see @ref isCustom(), @ref firstChild(), @ref findNext(),
         *      @ref findFirstChildOf(), @ref parent()
         */
        Containers::Optional<Structure> findFirstChild() const;

        /**
         * @brief First child structure
         *
         * The structure must be custom and must have at least one child.
         * @see @ref isCustom(), @ref hasChildren(), @ref findFirstChild(),
         *      @ref Document::validate(), @ref firstChildOf(), @ref parent()
         */
        Structure firstChild() const;

        /**
         * @brief Structure children
         *
         * The structure must be custom. The returned list can be traversed
         * using common range-based for:
         *
         * @code{.cpp}
         * for(Structure p: structure.children()) {
         *     // ...
         * }
         * @endcode
         *
         * @see @ref isCustom(), @ref childrenOf(), @ref Document::children()
         */
        Implementation::StructureList children() const;

        /**
         * @brief Find first custom child structure of given type
         *
         * The structure must be custom. Returns
         * @ref Corrade::Containers::NullOpt if there is no such structure.
         * @see @ref isCustom(), @ref firstChildOf()
         */
        Containers::Optional<Structure> findFirstChildOf(Type type) const;

        /**
         * @brief Find first custom child structure of given identifier
         *
         * The structure must be custom. Returns
         * @ref Corrade::Containers::NullOpt if there is no such structure.
         * @see @ref isCustom(), @ref firstChildOf(), @ref findNextOf()
         */
        Containers::Optional<Structure> findFirstChildOf(Int identifier) const;

        /** @overload */
        Containers::Optional<Structure> findFirstChildOf(std::initializer_list<Int> identifiers) const {
            return findFirstChildOf({identifiers.begin(), identifiers.size()});
        }
        Containers::Optional<Structure> findFirstChildOf(Containers::ArrayView<const Int> identifiers) const; /**< @overload */

        /**
         * @brief First custom child structure of given type
         *
         * The structure must be custom and there must be such child structure.
         * @see @ref isCustom(), @ref Document::validate(),
         *      @ref findFirstChildOf()
         */
        Structure firstChildOf(Type type) const;

        /**
         * @brief First custom child structure of given identifier
         *
         * The structure must be custom and there must be such child structure.
         * @see @ref isCustom(), @ref Document::validate(),
         *      @ref findFirstChildOf()
         */
        Structure firstChildOf(Int identifier) const;

        /**
         * @brief Structure children of given identifier
         *
         * The structure must be custom. The returned list can be traversed
         * using common range-based for:
         *
         * @code{.cpp}
         * for(Structure p: structure.childrenOf(...)) {
         *     // ...
         * }
         * @endcode
         *
         * @see @ref isCustom(), @ref children(), @ref Document::childrenOf()
         */
        Implementation::StructureOfList<1> childrenOf(Int identifier) const;
        template<class ...T> Implementation::StructureOfList<sizeof...(T)+1> childrenOf(Int identifier, T... identifiers) const; /**< @overload */

    private:
        #ifndef DOXYGEN_GENERATING_OUTPUT /* https://bugzilla.gnome.org/show_bug.cgi?id=776986 */
        friend Document;
        friend Property;
        friend Implementation::StructureIterator;
        friend Implementation::StructureOfIterator;
        #endif

        explicit Structure(const Document& document, const Document::StructureData& data) noexcept: _document{document}, _data{data} {}

        Containers::Reference<const Document> _document;
        Containers::Reference<const Document::StructureData> _data;
};

namespace Implementation {
    template<class> bool isStructureType(Type);
    template<> inline bool isStructureType<bool>(Type type) { return type == Type::Bool; }
    template<> inline bool isStructureType<std::string>(Type type) {
        return type == Type::String;
    }
    #ifndef DOXYGEN_GENERATING_OUTPUT
    #define _c(T) \
        template<> inline bool isStructureType<T>(Type type) { return type == Type::T; }
    _c(UnsignedByte)
    _c(Byte)
    _c(UnsignedShort)
    _c(Short)
    _c(UnsignedInt)
    _c(Int)
    #ifndef CORRADE_TARGET_EMSCRIPTEN
    _c(UnsignedLong)
    _c(Long)
    #endif
    /** @todo Half */
    _c(Float)
    _c(Double)
    #undef _c
    #endif
}

template<class T>
#ifndef DOXYGEN_GENERATING_OUTPUT
typename Implementation::ReturnTypeFor<T>::Type
#else
const T&
#endif
Structure::as() const {
    CORRADE_ASSERT(arraySize() == 1,
        "OpenDdl::Structure::as(): not a single value", _document.get().data<T>().front());
    CORRADE_ASSERT(Implementation::isStructureType<T>(type()),
        "OpenDdl::Structure::as(): not of given type", _document.get().data<T>().front());
    return _document.get().data<T>()[_data.get().primitive.begin];
}

template<class T> Containers::ArrayView<const T> Structure::asArray() const {
    CORRADE_ASSERT(Implementation::isStructureType<T>(type()),
        "OpenDdl::Structure::asArray(): not of given type", nullptr);
    return {_document.get().data<T>().data() + _data.get().primitive.begin, _data.get().primitive.size};
}

namespace Implementation {

class StructureIterator {
    public:
        explicit StructureIterator(Containers::Optional<Structure> item) noexcept: _item{item} {}

        Structure operator*() const { return *_item; }
        bool operator!=(const StructureIterator& other) const {
            return !_item != !other._item || ((_item && other._item) && &_item->_data != &other._item->_data);
        }

        StructureIterator& operator++() {
            _item = _item->findNext();
            return *this;
        }

    private:
        Containers::Optional<Structure> _item;
};

class StructureList {
    public:
        explicit StructureList(Containers::Optional<Structure> first) noexcept: _first{first} {}

        StructureIterator begin() const { return StructureIterator{_first}; }
        StructureIterator cbegin() const { return begin(); }
        StructureIterator end() const { return StructureIterator{Containers::NullOpt}; }
        StructureIterator cend() const { return end(); }

    private:
        Containers::Optional<Structure> _first;
};

class StructureOfIterator {
    public:
        explicit StructureOfIterator(Containers::Optional<Structure> item, Containers::ArrayView<const Int> identifiers) noexcept: _item{item}, _identifiers(identifiers) {}

        Structure operator*() const { return *_item; }
        bool operator!=(const StructureOfIterator& other) const {
            return _item != other._item;
        }
        StructureOfIterator& operator++() {
            _item = _item->findNextOf(_identifiers);
            return *this;
        }

    private:
        Containers::Optional<Structure> _item;
        Containers::ArrayView<const Int> _identifiers;
};

template<std::size_t size> class StructureOfList {
    public:
        template<class ...T> explicit StructureOfList(Containers::Optional<Structure> first, T... identifiers) noexcept: _first{first}, _identifiers{identifiers...} {
            static_assert(sizeof...(T) == size, "Invalid identifier count");
        }

        StructureOfIterator begin() const { return StructureOfIterator{_first, _identifiers}; }
        StructureOfIterator cbegin() const { return begin(); }
        StructureOfIterator end() const { return StructureOfIterator{Containers::NullOpt, _identifiers}; }
        StructureOfIterator cend() const { return end(); }

    private:
        Containers::Optional<Structure> _first;
        Int _identifiers[size];
};

}

template<class ...T> inline Implementation::StructureOfList<sizeof...(T)+1> Structure::childrenOf(Int identifier, T... identifiers) const {
    CORRADE_ASSERT(isCustom(), "OpenDdl::Structure::childrenOf(): not a custom structure",
        (Implementation::StructureOfList<sizeof...(T)+1>{findFirstChildOf({identifier, identifiers...}), identifier, identifiers...}));
    return Implementation::StructureOfList<sizeof...(T)+1>{findFirstChildOf({identifier, identifiers...}), identifier, identifiers...};
}

}}

#endif
