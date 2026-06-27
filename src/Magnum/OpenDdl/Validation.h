#ifndef Magnum_OpenDdl_Validation_h
#define Magnum_OpenDdl_Validation_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
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

#ifdef MAGNUM_BUILD_DEPRECATED
/** @file
 * @brief Class @ref Magnum::OpenDdl::Validation::Property, @ref Magnum::OpenDdl::Validation::Structure, typedef @ref Magnum::OpenDdl::Validation::Primitives, @ref Magnum::OpenDdl::Validation::Properties, @ref Magnum::OpenDdl::Validation::Structures, tag @ref Magnum::OpenDdl::Validation::RequiredPropertyType, constant @ref Magnum::OpenDdl::Validation::OptionalProperty, @ref Magnum::OpenDdl::Validation::RequiredProperty
 * @m_deprecated_since_latest Use the @ref Magnum::Trade::AssimpImporter plugin
 *      for parsing OpenDDL / OpenGEX files instead.
 */
#endif

#include <Magnum/configure.h>

#ifdef MAGNUM_BUILD_DEPRECATED
#include <Corrade/Containers/Array.h>

#include "Magnum/OpenDdl/visibility.h"
#include "Magnum/OpenDdl/Type.h"

namespace Magnum { namespace OpenDdl {

namespace CORRADE_DEPRECATED_NAMESPACE("use use the AssimpImporter plugin for parsing OpenDDL / OpenGEX files instead") Validation {

/**
@brief Tag type for required and optional properties
@m_deprecated_since_latest Use the @ref Trade::AssimpImporter plugin for
    parsing OpenDDL / OpenGEX files instead.

@see @ref Property, @ref RequiredProperty, @ref OptionalProperty
*/
struct CORRADE_DEPRECATED("use use the AssimpImporter plugin for parsing OpenDDL / OpenGEX files instead") RequiredPropertyType {
    #ifndef DOXYGEN_GENERATING_OUTPUT
    bool required;
    #endif
};

/**
@brief Required property
@m_deprecated_since_latest Use the @ref Trade::AssimpImporter plugin for
    parsing OpenDDL / OpenGEX files instead.

@see @ref Property
*/
CORRADE_IGNORE_DEPRECATED_PUSH /* GCC 4.8 warns here */
CORRADE_DEPRECATED("use use the AssimpImporter plugin for parsing OpenDDL / OpenGEX files instead") constexpr RequiredPropertyType RequiredProperty{true};
CORRADE_IGNORE_DEPRECATED_POP

/**
@brief Optional property
@m_deprecated_since_latest Use the @ref Trade::AssimpImporter plugin for
    parsing OpenDDL / OpenGEX files instead.

@see @ref Property
*/
CORRADE_IGNORE_DEPRECATED_PUSH /* GCC 4.8 warns here */
CORRADE_DEPRECATED("use use the AssimpImporter plugin for parsing OpenDDL / OpenGEX files instead") constexpr RequiredPropertyType OptionalProperty{false};
CORRADE_IGNORE_DEPRECATED_POP

/**
@brief Property specification
@m_deprecated_since_latest Use the @ref Trade::AssimpImporter plugin for
    parsing OpenDDL / OpenGEX files instead.

Example usage (excerpt from OpenGEX specification of `Animation` structure):

@code{.cpp}
Properties{{clip, PropertyType::UnsignedInt, OptionalProperty},
           {begin, PropertyType::Float, OptionalProperty},
           {end, PropertyType::Float, OptionalProperty}}
@endcode

@see @ref Properties
*/
class CORRADE_DEPRECATED("use use the AssimpImporter plugin for parsing OpenDDL / OpenGEX files instead") Property {
    public:
        /**
         * @brief Constructor
         * @param identifier    Property identifier
         * @param type          Expected property type
         * @param required      Whether the property is required
         */
        CORRADE_IGNORE_DEPRECATED_PUSH
        constexpr /*implicit*/ Property(Int identifier, PropertyType type, RequiredPropertyType required): _identifier{identifier}, _type{type}, _required{required.required} {}
        CORRADE_IGNORE_DEPRECATED_POP

        #ifndef DOXYGEN_GENERATING_OUTPUT
        constexpr Int identifier() const { return _identifier; }
        CORRADE_IGNORE_DEPRECATED_PUSH
        constexpr PropertyType type() const { return _type; }
        CORRADE_IGNORE_DEPRECATED_POP
        constexpr bool isRequired() const { return _required; }
        #endif

    private:
        Int _identifier;
        CORRADE_IGNORE_DEPRECATED_PUSH
        PropertyType _type;
        CORRADE_IGNORE_DEPRECATED_POP
        bool _required;
};

/**
@brief List of allowed properties for validation
@m_deprecated_since_latest Use the @ref Trade::AssimpImporter plugin for
    parsing OpenDDL / OpenGEX files instead.

See @ref Validation::Property for example usage.
*/
CORRADE_IGNORE_DEPRECATED_PUSH
typedef CORRADE_DEPRECATED("use use the AssimpImporter plugin for parsing OpenDDL / OpenGEX files instead") std::initializer_list<Property> Properties;
CORRADE_IGNORE_DEPRECATED_POP

/**
@brief List of allowed structures for validation
@m_deprecated_since_latest Use the @ref Trade::AssimpImporter plugin for
    parsing OpenDDL / OpenGEX files instead.

First value is structure identifier, the pair specifies minimal and maximal
allowed count of structures with given identifier. Maximal count set to
@cpp 0 @ce means that there is no upper limit.

See @ref Validation::Structure for example usage.
*/
CORRADE_IGNORE_DEPRECATED_PUSH
typedef CORRADE_DEPRECATED("use use the AssimpImporter plugin for parsing OpenDDL / OpenGEX files instead") std::initializer_list<std::pair<Int, std::pair<Int, Int>>> Structures;
CORRADE_IGNORE_DEPRECATED_POP

/**
@brief List of allowed primitive types for validation
@m_deprecated_since_latest Use the @ref Trade::AssimpImporter plugin for
    parsing OpenDDL / OpenGEX files instead.

See @ref Validation::Structure for example usage.
*/
CORRADE_IGNORE_DEPRECATED_PUSH
typedef CORRADE_DEPRECATED("use use the AssimpImporter plugin for parsing OpenDDL / OpenGEX files instead") std::initializer_list<Type> Primitives;
CORRADE_IGNORE_DEPRECATED_POP

/**
@brief Structure spec for validation
@m_deprecated_since_latest Use the @ref Trade::AssimpImporter plugin for
    parsing OpenDDL / OpenGEX files instead.

Example usage (excerpt from OpenGEX specification of `Texture` structure):

@code{.cpp}
{Texture,
    // Requiring string attrib property, optional integer texcoord property
    Properties{{attrib, PropertyType::String, RequiredProperty},
               {texcoord, PropertyType::UnsignedInt, OptionalProperty}},

    // Requiring exactly one primitive substructure with exactly one string
    // value for filename
    Primitives{Type::String}, 1, 1,

    // There can be any number of Transform, Translation, Rotation, Scale and
    // Animation substructures
    Structures{{Transform, {}},
               {Translation, {}},
               {Rotation, {}},
               {Scale, {}},
               {Animation, {}}}}
@endcode
*/
class CORRADE_DEPRECATED("use use the AssimpImporter plugin for parsing OpenDDL / OpenGEX files instead") MAGNUM_OPENDDL_EXPORT Structure {
    public:
        /**
         * @brief Constructor
         * @param identifier            Structure identifier
         * @param properties            List of allowed properties
         * @param primitives            List of allowed primitive types
         * @param structures            List of allowed custom sub-structures
         * @param primitiveCount        Expected primitive sub-structure count
         * @param primitiveArraySize    Expected primitive array size
         *
         * Setting @p primitiveCount to @cpp 0 @ce means that there is no
         * requirement on primitive array count. Setting @p primitiveArraySize
         * to @cpp 0 @ce means that there is no requirement on primitive array
         * size.
         */
        CORRADE_IGNORE_DEPRECATED_PUSH
        /*implicit*/ Structure(Int identifier, Properties properties, Primitives primitives, std::size_t primitiveCount, std::size_t primitiveArraySize, Structures structures = {});
        CORRADE_IGNORE_DEPRECATED_POP

        /** @overload */
        CORRADE_IGNORE_DEPRECATED_PUSH
        /*implicit*/ Structure(Int identifier, Primitives primitives, std::size_t primitiveCount, std::size_t primitiveArraySize, Structures structures = {}): Structure{identifier, {}, primitives, primitiveCount, primitiveArraySize, structures} {}
        CORRADE_IGNORE_DEPRECATED_POP

        /** @overload */
        CORRADE_IGNORE_DEPRECATED_PUSH
        /*implicit*/ Structure(Int identifier, Properties properties, Structures structures = {}): Structure{identifier, properties, {}, {}, {}, structures} {}
        CORRADE_IGNORE_DEPRECATED_POP

        /** @overload */
        CORRADE_IGNORE_DEPRECATED_PUSH
        /*implicit*/ Structure(Int identifier, Structures structures = {}): Structure{identifier, {}, {}, {}, {}, structures} {}
        CORRADE_IGNORE_DEPRECATED_POP

        #ifndef DOXYGEN_GENERATING_OUTPUT
        Int identifier() const { return _identifier; }
        CORRADE_IGNORE_DEPRECATED_PUSH
        Containers::ArrayView<const Property> properties() const { return _properties; }
        Containers::ArrayView<const Type> primitives() const { return _primitives; }
        CORRADE_IGNORE_DEPRECATED_POP
        Containers::ArrayView<const std::pair<Int, std::pair<Int, Int>>> structures() const { return _structures; }
        std::size_t primitiveCount() const { return _primitiveCount; }
        std::size_t primitiveArraySize() const { return _primitiveArraySize; }
        #endif

    private:
        Int _identifier;

        CORRADE_IGNORE_DEPRECATED_PUSH
        Containers::Array<Property> _properties;
        Containers::Array<Type> _primitives;
        CORRADE_IGNORE_DEPRECATED_POP
        Containers::Array<std::pair<Int, std::pair<Int, Int>>> _structures;
        std::size_t _primitiveCount;
        std::size_t _primitiveArraySize;
};

}}}
#else
#error use the AssimpImporter plugin for parsing OpenDDL / OpenGEX files instead
#endif

#endif
