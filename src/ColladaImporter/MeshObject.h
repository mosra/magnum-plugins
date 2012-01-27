#ifndef Magnum_Trade_ColladaImporter_MeshObject_h
#define Magnum_Trade_ColladaImporter_MeshObject_h
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
 * @brief Class Magnum::Trade::ColladaImporter::MeshObject
 */

#include <memory>

#include "Object.h"
#include "Mesh.h"
#include "Trade/AbstractMaterial.h"

namespace Magnum { namespace Trade { namespace ColladaImporter {

/** @brief Object with mesh */
class MeshObject: public Object {
    public:
        /**
         * @brief Constructor
         * @param mesh      Mesh
         * @param material  Material
         * @param parent    Parent object
         */
        MeshObject(const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<AbstractMaterial>& material, Object* parent = nullptr): Object(parent), mesh(mesh), material(material) {}

        inline void draw(const Matrix4& transformationMatrix, const Matrix4& projectionMatrix) {
            material->use(transformationMatrix, projectionMatrix);
            mesh->draw();
        }

    private:
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<AbstractMaterial> material;
};

}}}

#endif
