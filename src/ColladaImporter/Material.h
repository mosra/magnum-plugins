#ifndef Magnum_Plugins_ColladaImporter_Material_h
#define Magnum_Plugins_ColladaImporter_Material_h
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
 * @brief Class Magnum::Plugins::ColladaImporter::Material
 */

#include <memory>

#include "AbstractMaterial.h"

namespace Magnum { namespace Plugins { namespace ColladaImporter {

class PointLight;
class PhongShader;

/** @brief Base class for Collada materials */
class Material: public AbstractMaterial {
    public:
        /**
         * @brief Constructor
         * @param shader        Phong shader instance
         * @param ambientColor  Object ambient color
         * @param diffuseColor  Object diffuse color
         * @param specularColor Object specular color
         * @param shininess     Object shininess
         * @param light         Light object
         */
        Material(const std::shared_ptr<PhongShader>& shader, const Vector3& ambientColor, const Vector3& diffuseColor, const Vector3& specularColor, GLfloat shininess, const std::shared_ptr<PointLight>& light);

        bool use(const Matrix4& transformationMatrix, const Matrix4& projectionMatrix);

    private:
        std::shared_ptr<PhongShader> shader;
        Vector3 ambientColor,
            diffuseColor,
            specularColor;
        GLfloat shininess;
        std::shared_ptr<PointLight> light;
};

}}}

#endif
