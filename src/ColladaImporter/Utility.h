#ifndef Magnum_Trade_ColladaImporter_Utility_h
#define Magnum_Trade_ColladaImporter_Utility_h
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
 * @brief Class Magnum::Trade::ColladaImporter::Utility
 */

#include <vector>
#include <QtCore/QString>

#include "ColladaType.h"

namespace Magnum { namespace Trade { namespace ColladaImporter {

/**
@brief Utility functions for parsing
*/
class Utility {
    public:
        /**
         * @brief Parse vector of numbers from array
         * @tparam Vector   Vector type (based on Math::Vector)
         * @param data      Data array
         * @param from      Starting position
         * @param size      Size of the vector (only @c size items will be
         *      saved to the vector)
         *
         * Returns parsed vector and moves @c from to position of next vector.
         */
        template<class Vector> static Vector parseVector(const QString& data, int* from, size_t size = Vector::Size) {
            Vector output;
            int to;
            for(size_t j = 0; j != size; ++j) {
                to = data.indexOf(' ', *from);
                while(to == *from)
                    to = data.indexOf(' ', ++*from);
                output.set(j, ColladaType<typename Vector::Type>::fromString(data.mid(*from, to-*from)));
                *from = (to == -1 ? data.size() : to+1);
            }

            return output;
        }

        /**
         * @brief Parse vector of numbers
         *
         * Convenience alternative to parseVector(const QString&, int*, size_t).
         */
        template<class Vector> inline static Vector parseVector(const QString& data, size_t size = Vector::Size) {
            int from = 0;
            return parseVector<Vector>(data, &from, size);
        }

        /**
         * @brief Parse array of numbers
         * @tparam Single   Single-value data type, parsable by ColladaType.
         * @param data      Data array
         * @param count     Count of numbers
         */
        template<class Single> static std::vector<Single> parseArray(const QString& data, size_t count) {
            std::vector<Single> output;
            output.reserve(count);

            int from = 0;
            int to;
            for(size_t i = 0; i != count; ++i) {
                to = data.indexOf(' ', from);
                while(to == from)
                    to = data.indexOf(' ', ++from);
                output.push_back(ColladaType<Single>::fromString(data.mid(from, to-from)));
                from = (to == -1 ? data.size() : to+1);
            }

            return output;
        }
};

}}}

#endif
