#ifndef Magnum_OpenDdl_Type_h
#define Magnum_OpenDdl_Type_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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
 * @brief Enum @ref Magnum::OpenDdl::Type, @ref Magnum::OpenDdl::PropertyType, constant @ref Magnum::OpenDdl::UnknownIdentifier
 */

#include <Magnum/Magnum.h>

namespace Magnum { namespace OpenDdl {

/**
@brief OpenDDL type

@see @ref Structure::type()
*/
enum class Type: UnsignedInt {
    /** Boolean. Stored in `bool` type. */
    Bool,

    /**
     * Unsigned byte (8 bit). Stored in @ref Magnum::UnsignedByte "UnsignedByte"
     * type.
     */
    UnsignedByte,

    /** Signed byte (8 bit). Stored in @ref Magnum::Byte "Byte" type. */
    Byte,

    /**
     * Unsigned short (16 bit). Stored in @ref Magnum::UnsignedShort "UnsignedShort"
     * type.
     */
    UnsignedShort,

    /** Signed short (16 bit). Stored in @ref Magnum::Short "Short" type. */
    Short,

    /**
     * Unsigned int (32 bit). Stored in @ref Magnum::UnsignedInt "UnsignedInt"
     * type.
     */
    UnsignedInt,

    /** Signed int (32 bit). Stored in @ref Magnum::Int "Int" type. */
    Int,

    #ifndef MAGNUM_TARGET_WEBGL
    /**
     * Unsigned long (64 bit). Stored in @ref Magnum::UnsignedLong "UnsignedLong"
     * type.
     * @requires_gles 64-bit integers are not available in WebGL.
     */
    UnsignedLong,

    /**
     * Long (64 bit). Stored in @ref Magnum::Long "Long" type.
     * @requires_gles 64-bit integers are not available in WebGL.
     */
    Long,
    #endif

    /** @todo Half */

    /** Float (32 bit). Stored in @ref Magnum::Float "Float" type. */
    Float,

    /** Double (64 bit). Stored in @ref Magnum::Double "Double" type. */
    Double,

    /** UTF-8 string. Stored in `std::string` type. */
    String,

    /** ASCII reference string. Stored in `std::string` type. */
    Reference,

    /** Type enumeration. Stored in @ref Type type. */
    Type,

    /** Custom structure type */
    Custom
};

/** @debugoperatorenum{Type} */
Debug& operator<<(Debug& debug, Type value);

/**
@brief Property type

Because of parsing ambiguity, the properties are internally stored only in a
subset of types. The remaining types are just for use in
@ref Property::isTypeCompatibleWith(). See documentation of particular values
for more information.
*/
enum class PropertyType: UnsignedByte {
    /** Boolean. Stored in `bool` type. */
    Bool,

    /** Unsigned byte (8 bit). Stored as if it is @ref PropertyType::Int. */
    UnsignedByte,

    /** Signed byte (8 bit). Stored as if it is @ref PropertyType::Int. */
    Byte,

    /** Unsigned short (16 bit). Stored as if it is @ref PropertyType::Int. */
    UnsignedShort,

    /** Signed short (16 bit). Stored as if it is @ref PropertyType::Int. */
    Short,

    /** Unsigned int (32 bit). Stored as if it is @ref PropertyType::Int. */
    UnsignedInt,

    /** Signed int (32 bit). Stored in @ref Magnum::Int "Int" type. */
    Int,

    #ifndef MAGNUM_TARGET_WEBGL
    /**
     * Unsigned long (64 bit). Stored as if it is @ref PropertyType::Int.
     * @requires_gles 64-bit integers are not available in WebGL.
     */
    UnsignedLong,

    /**
     * Long (64 bit). Stored as if it is @ref PropertyType::Int.
     * @requires_gles 64-bit integers are not available in WebGL.
     */
    Long,
    #endif

    /** @todo Half */

    /** Float (32 bit). Stored in @ref Magnum::Float "Float" type. */
    Float,

    /** Double (64 bit). Stored as if it is @ref PropertyType::Float. */
    Double,

    /** UTF-8 string. Stored in `std::string` type. */
    String,

    /** ASCII reference string. Stored in `std::string` type. */
    Reference,

    /** Type enumeration. Stored in @ref Type type. */
    Type
};

/** @debugoperatorenum{PropertyType} */
Debug& operator<<(Debug& debug, PropertyType value);

enum: Int {
    /**
     * @brief Unknown identifier
     *
     * Identifier which was not in identifier list passed to
     * @ref Document::parse().
     * @see @ref Structure::identifier(), @ref Property::identifier()
     */
    UnknownIdentifier =
        #ifndef CORRADE_TARGET_ANDROID
        INT16_MAX
        #else
        0x7fff /* Not defined on Android for some weird reason */
        #endif
        - Int(Type::Custom)
};

namespace Implementation {
    enum class InternalPropertyType: UnsignedByte {
        Bool = UnsignedByte(PropertyType::Bool),
        Integral = UnsignedByte(PropertyType::Int),
        Float = UnsignedByte(PropertyType::Float),
        String = UnsignedByte(PropertyType::String),
        Reference = UnsignedByte(PropertyType::Reference),
        Type = UnsignedByte(PropertyType::Type),
        Character = 254,
        Binary = 255
    };
    Debug& operator<<(Debug& debug, InternalPropertyType value);
}

}}

#endif
