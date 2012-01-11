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

#include "Material.h"

#include "PhongShader.h"
#include "PointLight.h"

using namespace std;

namespace Magnum { namespace Plugins { namespace ColladaImporter {

Material::Material(const shared_ptr<PhongShader>& shader, const Magnum::Vector3& ambientColor, const Magnum::Vector3& diffuseColor, const Magnum::Vector3& specularColor, GLfloat shininess, const shared_ptr<PointLight>& light): shader(shader), ambientColor(ambientColor), diffuseColor(diffuseColor), specularColor(specularColor), shininess(shininess), light(light) {}

bool Material::use(const Magnum::Matrix4& transformationMatrix, const Magnum::Matrix4& projectionMatrix) {
    if(!shader->use()) return false;

    /* Object properties */
    shader->setAmbientColorUniform(ambientColor);
    shader->setDiffuseColorUniform(diffuseColor);
    shader->setSpecularColorUniform(specularColor);
    shader->setShininessUniform(128.0);

    /* Light properties */
    shader->setLightUniform(light->position());
    shader->setLightAmbientColorUniform(light->ambientColor());
    shader->setLightDiffuseColorUniform(light->diffuseColor());
    shader->setLightSpecularColorUniform(light->specularColor());

    /* Tranformation */
    shader->setTransformationMatrixUniform(transformationMatrix);
    shader->setProjectionMatrixUniform(projectionMatrix);

    return true;
}

}}}
