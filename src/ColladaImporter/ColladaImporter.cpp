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

#include "ColladaImporter.h"

#include <QtCore/QVector>
#include <QtCore/QFile>
#include <QtCore/QStringList>

#include "SizeTraits.h"
#include "Trade/PhongMaterialData.h"
#include "Trade/MeshData.h"

using namespace std;
using namespace Corrade::Utility;
using namespace Corrade::PluginManager;

PLUGIN_REGISTER(Magnum::Trade::ColladaImporter::ColladaImporter, "cz.mosra.magnum.Trade.AbstractImporter/0.1")

namespace Magnum { namespace Trade { namespace ColladaImporter {

const QString ColladaImporter::namespaceDeclaration =
    "declare default element namespace \"http://www.collada.org/2005/11/COLLADASchema\";\n";

ColladaImporter::ColladaImporter(AbstractPluginManager* manager, const string& plugin): AbstractImporter(manager, plugin), d(0), zero(0), app(qApp ? 0 : new QCoreApplication(zero, 0)) {}

ColladaImporter::~ColladaImporter() {
    close();
    delete app;
}

ColladaImporter::Document::~Document() {
    for(auto i: meshes) delete i;
    for(auto i: materials) delete i;
}

bool ColladaImporter::open(const string& filename) {
    /* Close previous document */
    if(d) close();

    QXmlQuery query;

    /* Open the file and load it into XQuery */
    QFile file(QString::fromStdString(filename));
    if(!file.open(QIODevice::ReadOnly)) {
        Error() << "ColladaImporter: cannot open file" << filename;
        return false;
    }
    if(!query.setFocus(&file)) {
        Error() << "ColladaImporter: cannot load XML";
        return false;
    }

    QString tmp;
    QStringList listTmp;

    /* Check namespace */
    query.setQuery("namespace-uri(/*:COLLADA)");
    query.evaluateTo(&tmp);
    tmp = tmp.trimmed();
    if(tmp != "http://www.collada.org/2005/11/COLLADASchema") {
        Error() << "ColladaImporter: unsupported namespace" << ('"'+tmp+'"').toStdString();
        return false;
    }

    /* Check version */
    query.setQuery(namespaceDeclaration + "/COLLADA/@version/string()");
    query.evaluateTo(&tmp);
    tmp = tmp.trimmed();
    if(tmp != "1.4.1") {
        Error() << "ColladaImporter: unsupported version" << ('"'+tmp+'"').toStdString();
        return false;
    }

    /* Geometry count */
    query.setQuery(namespaceDeclaration + "count(//library_geometries/geometry)");
    query.evaluateTo(&tmp);
    GLuint objectCount = ColladaType<GLuint>::fromString(tmp);

    /* Materials */
    query.setQuery(namespaceDeclaration + "//library_materials/material/@id/string()");
    query.evaluateTo(&listTmp);

    d = new Document(objectCount, listTmp.size());
    d->query = query;

    /* Add all materials to material map */
    for(size_t i = 0; i != static_cast<size_t>(listTmp.size()); ++i)
        d->materialMap[listTmp[i].toStdString()] = i;

    Debug() << QString("ColladaImporter: file contains\n"
                       "    %0 objects/meshes\n"
                       "    %1 materials")
        .arg(objectCount).arg(listTmp.size()).toStdString();

    return true;
}

void ColladaImporter::close() {
    if(!d) return;

    delete d;
    d = 0;
}

MeshData* ColladaImporter::mesh(size_t meshId) {
    if(!d || meshId >= d->meshes.size()) return nullptr;
    if(d->meshes[meshId]) return d->meshes[meshId];

    QString tmp;

    /** @todo More polylists in one mesh */

    /* Get polygon count */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/@count/string()").arg(meshId+1));
    d->query.evaluateTo(&tmp);
    GLuint polygonCount = ColladaType<GLuint>::fromString(tmp);

    /* Get vertex count per polygon */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/vcount/string()").arg(meshId+1));
    d->query.evaluateTo(&tmp);
    vector<GLuint> vertexCountPerFace = Utility::parseArray<GLuint>(tmp, polygonCount);

    GLuint vertexCount = 0;
    for(GLuint count: vertexCountPerFace) {
        vertexCount += count;
        if(count == 3) continue;

        /** @todo Support also quads */
        Error() << "ColladaImporter:" << count << "vertices per face not supported";
        return nullptr;
    }

    /* Get input count per vertex */
    d->query.setQuery((namespaceDeclaration + "count(//geometry[%0]/mesh/polylist/input)").arg(meshId+1));
    d->query.evaluateTo(&tmp);
    GLuint stride = ColladaType<GLuint>::fromString(tmp);

    /* Get mesh indices */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/p/string()").arg(meshId+1));
    d->query.evaluateTo(&tmp);
    vector<GLuint> indices = Utility::parseArray<GLuint>(tmp, vertexCount*stride);

    /* Get mesh vertices offset in indices */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/input[@semantic='VERTEX']/@offset/string()")
        .arg(meshId+1));
    d->query.evaluateTo(&tmp);
    GLuint vertexOffset = ColladaType<GLuint>::fromString(tmp);

    /* Get mesh vertices */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/input[@semantic='VERTEX']/@source/string()")
        .arg(meshId+1));
    d->query.evaluateTo(&tmp);
    d->query.setQuery((namespaceDeclaration + "//vertices[@id='%0']/input[@semantic='POSITION']/@source/string()")
        .arg(tmp.mid(1).trimmed()));
    d->query.evaluateTo(&tmp);
    vector<Vector3> vertices = parseSource<Vector3>(tmp.mid(1).trimmed());

    /* Get mesh normals offset in indices */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/input[@semantic='NORMAL']/@offset/string()")
        .arg(meshId+1));
    d->query.evaluateTo(&tmp);
    GLuint normalOffset = ColladaType<GLuint>::fromString(tmp);

    /* Get mesh normals */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/input[@semantic='NORMAL']/@source/string()")
        .arg(meshId+1));
    d->query.evaluateTo(&tmp);
    vector<Vector3> normals = parseSource<Vector3>(tmp.mid(1).trimmed());

    /* Unique vertices and normals */
    QMap<unsigned long long, unsigned int> uniqueMap;
    vector<unsigned int>* uniqueIndices = new vector<unsigned int>();
    vector<Vector4>* uniqueVertices = new vector<Vector4>();
    vector<Vector3>* uniqueNormals = new vector<Vector3>();
    for(unsigned int i = 0; i < indices.size(); i += stride) {
        unsigned long long uniqueHash = static_cast<unsigned long long>(indices[i]) << 32 | indices[i+1];
        if(uniqueMap.contains(uniqueHash)) {
            uniqueIndices->push_back(uniqueMap[uniqueHash]);
            continue;
        }

        uniqueVertices->push_back(vertices[indices[i+vertexOffset]]);
        uniqueNormals->push_back(normals[indices[i+normalOffset]]);

        uniqueMap[uniqueHash] = uniqueVertices->size()-1;
        uniqueIndices->push_back(uniqueVertices->size()-1);
    }

    Debug() << QString("ColladaImporter: mesh contains %0 faces and %1 unique vertices")
        .arg(uniqueIndices->size()/3).arg(uniqueVertices->size()).toStdString();

    d->meshes[meshId] = new MeshData(Mesh::Primitive::Triangles, uniqueIndices, {uniqueVertices}, {uniqueNormals}, {});
    return d->meshes[meshId];
}

AbstractMaterialData* ColladaImporter::ColladaImporter::material(size_t id) {
    /* Return nullptr if no such material exists, or return existing, if already parsed */
    if(!d || id >= d->materials.size()) return nullptr;
    if(d->materials[id]) return d->materials[id];

    QString tmp;

    /* Get effect ID */
    QString effect;
    d->query.setQuery((namespaceDeclaration + "//material[%0]/instance_effect/@url/string()").arg(id+1));
    d->query.evaluateTo(&effect);
    effect = effect.mid(1).trimmed();

    /* Find out which profile it is */
    d->query.setQuery((namespaceDeclaration + "//effect[@id='%0']/*[substring(name(), 1, 8) = 'profile_']/name()").arg(effect));
    d->query.evaluateTo(&tmp);

    /** @todo Support other profiles */

    if(tmp.trimmed() != "profile_COMMON") {
        Error() << "ColladaImporter:" << ('"'+tmp.trimmed()+'"').toStdString() << "effect profile not supported";
        return nullptr;
    }

    /* Get shader type */
    d->query.setQuery((namespaceDeclaration + "//effect[@id='%0']/profile_COMMON/technique/*/name()").arg(effect));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    /** @todo Other (blinn, goraund) profiles */
    if(tmp != "phong") {
        Error() << "ColladaImporter:" << ('"'+tmp+'"').toStdString() << "shader not supported";
        return nullptr;
    }

    /* Ambient color */
    d->query.setQuery((namespaceDeclaration + "//effect[@id='%0']/profile_COMMON/technique/phong/ambient/color/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    Vector3 ambientColor = Utility::parseVector<Vector3>(tmp);

    /* Diffuse color */
    d->query.setQuery((namespaceDeclaration + "//effect[@id='%0']/profile_COMMON/technique/phong/diffuse/color/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    Vector3 diffuseColor = Utility::parseVector<Vector3>(tmp);

    /* Specular color */
    d->query.setQuery((namespaceDeclaration + "//effect[@id='%0']/profile_COMMON/technique/phong/specular/color/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    Vector3 specularColor = Utility::parseVector<Vector3>(tmp);

    /* Shininess */
    d->query.setQuery((namespaceDeclaration + "//effect[@id='%0']/profile_COMMON/technique/phong/shininess/float/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    GLfloat shininess = ColladaType<GLfloat>::fromString(tmp);

    /** @todo Emission, IOR */

    d->materials[id] = new PhongMaterialData(ambientColor, diffuseColor, specularColor, shininess);
    return d->materials[id];
}

}}}
