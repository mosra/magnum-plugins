#ifndef Magnum_Trade_Implementation_Utility_h
#define Magnum_Trade_Implementation_Utility_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
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

#include <vector>
#include <QtCore/QString>
#include <QtXmlPatterns/QXmlQuery>

#include "ColladaType.h"

namespace Magnum { namespace Trade { namespace Implementation {

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
        template<class Vector> static Vector parseVector(const QString& data, int* from, std::size_t size = Vector::Size);

        /**
         * @brief Parse vector of numbers
         *
         * Convenience alternative to parseVector(const QString&, int*, std::size_t).
         */
        template<class Vector> static Vector parseVector(const QString& data, std::size_t size = Vector::Size) {
            int from = 0;
            return parseVector<Vector>(data, &from, size);
        }

        /**
         * @brief Parse array of numbers
         * @tparam Single   Single-value data type, parsable by ColladaType.
         * @param data      Data array
         * @param count     Count of numbers
         */
        template<class Single> static std::vector<Single> parseArray(const QString& data, std::size_t count);

        /* Parse the <source> element */
        template<class T> static std::vector<T> parseSource(QXmlQuery& query, const QString& namespaceDeclaration, const QString& id);
};

template<class Vector> Vector Utility::parseVector(const QString& data, int* from, std::size_t size) {
    Vector output;
    int to;
    for(std::size_t j = 0; j != size; ++j) {
        to = data.indexOf(' ', *from);
        while(to == *from)
            to = data.indexOf(' ', ++*from);
        output[j] = ColladaType<typename Vector::Type>::fromString(data.mid(*from, to-*from));
        *from = (to == -1 ? data.size() : to+1);
    }

    return output;
}

template<class Single> std::vector<Single> Utility::parseArray(const QString& data, std::size_t count) {
    std::vector<Single> output;
    output.reserve(count);

    int from = 0;
    int to;
    for(std::size_t i = 0; i != count; ++i) {
        to = data.indexOf(' ', from);
        while(to == from)
            to = data.indexOf(' ', ++from);
        output.push_back(ColladaType<Single>::fromString(data.mid(from, to-from)));
        from = (to == -1 ? data.size() : to+1);
    }

    return output;
}

template<class T> std::vector<T> Utility::parseSource(QXmlQuery& query, const QString& namespaceDeclaration, const QString& id) {
    std::vector<T> output;
    QString tmp;

    /* Count of items */
    query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@count/string()").arg(id));
    query.evaluateTo(&tmp);
    UnsignedInt count = ColladaType<UnsignedInt>::fromString(tmp);

    /* Size of each item */
    query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@stride/string()").arg(id));
    query.evaluateTo(&tmp);
    UnsignedInt size = ColladaType<UnsignedInt>::fromString(tmp);

    /* Data source */
    query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@source/string()").arg(id));
    query.evaluateTo(&tmp);
    QString source = tmp.mid(1).trimmed();

    /* Verify total count */
    query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source/float_array[@id='%0']/@count/string()").arg(source));
    query.evaluateTo(&tmp);
    if(ColladaType<UnsignedInt>::fromString(tmp) != count*size) {
        Error() << "Trade::ColladaImporter::mesh3D(): wrong total count in source" << ('"'+id+'"').toStdString();
        return output;
    }

    /** @todo Assert right order of coordinates and type */

    /* Items */
    query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source/float_array[@id='%0']/string()").arg(source));
    query.evaluateTo(&tmp);

    output.reserve(count);
    Int from = 0;
    for(std::size_t i = 0; i != count; ++i)
        output.push_back(Implementation::Utility::parseVector<T>(tmp, &from, size));

    return output;
}

}}}

#endif
