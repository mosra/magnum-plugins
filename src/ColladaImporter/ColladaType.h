#ifndef Magnum_Trade_ColladaImporter_ColladaType_h
#define Magnum_Trade_ColladaImporter_ColladaType_h
/*
    Copyright © 2010, 2011, 2012 Vladimír Vondruš <mosra@centrum.cz>

    This file is part of Magnum.

    Magnum is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License version 3
    only, as published by the Free Software Foundation.

    Magnum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License version 3 for more details.
*/

/** @file
 * @brief Class Magnum::Trade::ColladaImporter::ColladaType
 */

#include "Magnum.h"

#include <QtCore/QString>

namespace Magnum { namespace Trade { namespace ColladaImporter {

/**
@brief Class for dealing with Collada types
*/
template<class T> struct ColladaType {
    #ifdef DOXYGEN_GENERATING_OUTPUT
    /**
     * @brief Convert type from string
     *
     * Not implemented for unknown types.
     */
    static T fromString(const QString& str);
    #endif
};

#ifndef DOXYGEN_GENERATING_OUTPUT
template<> struct ColladaType<GLuint> {
    inline static GLuint fromString(const QString& str) {
        return str.toUInt();
    }
};

template<> struct ColladaType<GLfloat> {
    inline static GLfloat fromString(const QString& str) {
        return str.toFloat();
    }
};
#endif

}}}

#endif
