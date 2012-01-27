#ifndef Magnum_Trade_ColladaImporter_ColladaMeshData_h
#define Magnum_Trade_ColladaImporter_ColladaMeshData_h
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
 * @brief Class Magnum::Trade::ColladaImporter::ColladaMeshData
 */

#include "Trade/AbstractImporter.h"

namespace Magnum { namespace Trade { namespace ColladaImporter {

class ColladaMeshData: public AbstractImporter::MeshData {
    friend class ColladaImporter;

    public:
        std::vector<unsigned int>* const indices() { return &_indices; }
        std::vector<Vector3>* const vertices(size_t id) {
            if(id == 0) return &_vertices;
            return nullptr;
        }
        std::vector<Vector3>* const normals(size_t id) {
            if(id == 0) return &_normals;
            return nullptr;
        }

    private:
        std::vector<unsigned int> _indices;
        std::vector<Vector3> _vertices;
        std::vector<Vector3> _normals;
};

}}}

#endif
