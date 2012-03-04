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

#include <unordered_map>
#include <QtCore/QFile>
#include <QtCore/QStringList>

#include "Utility/Directory.h"
#include "SizeTraits.h"
#include "Trade/PhongMaterialData.h"
#include "Trade/MeshData.h"
#include "TgaImporter/TgaImporter.h"

using namespace std;
using namespace Corrade::Utility;
using namespace Corrade::PluginManager;

PLUGIN_REGISTER(ColladaImporter, Magnum::Trade::ColladaImporter::ColladaImporter,
                "cz.mosra.magnum.Trade.AbstractImporter/0.1")

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
    query.setQuery(namespaceDeclaration + "count(/COLLADA/library_geometries/geometry)");
    query.evaluateTo(&tmp);
    GLuint objectCount = ColladaType<GLuint>::fromString(tmp);

    /* Materials */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_materials/material/@id/string()");
    query.evaluateTo(&listTmp);

    /* Images */
    query.setQuery(namespaceDeclaration + "count(//library_images/image)");
    query.evaluateTo(&tmp);
    GLuint image2DCount = ColladaType<GLuint>::fromString(tmp);

    d = new Document(objectCount, listTmp.size(), image2DCount);
    d->filename = filename;
    d->query = query;

    /* Add all materials to material map */
    for(size_t i = 0; i != static_cast<size_t>(listTmp.size()); ++i)
        d->materialMap[listTmp[i].toStdString()] = i;

    /** @todo Also image map */

    Debug() << QString("ColladaImporter: file contains\n"
                       "    %0 objects/meshes\n"
                       "    %1 materials\n"
                       "    %2 images")
        .arg(objectCount).arg(listTmp.size()).arg(image2DCount).toStdString();

    return true;
}

void ColladaImporter::close() {
    if(!d) return;

    delete d;
    d = 0;
}

MeshData* ColladaImporter::mesh(size_t id) {
    if(!d || id >= d->meshes.size()) return nullptr;
    if(d->meshes[id]) return d->meshes[id];

    QString tmp;

    /** @todo More polylists in one mesh */

    /* Get polygon count */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/@count/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    GLuint polygonCount = ColladaType<GLuint>::fromString(tmp);

    /* Get vertex count per polygon */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/vcount/string()").arg(id+1));
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
    d->query.setQuery((namespaceDeclaration + "count(/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input)").arg(id+1));
    d->query.evaluateTo(&tmp);
    GLuint stride = ColladaType<GLuint>::fromString(tmp);

    /* Get mesh indices */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/p/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    vector<GLuint> originalIndices = Utility::parseArray<GLuint>(tmp, vertexCount*stride);

    /** @todo assert size()%stride == 0 */

    /* Get unique combinations of vertices, build resulting index array. Key
       is position of unique index combination from original vertex array,
       value is resulting index. */
    unordered_map<unsigned int, unsigned int, IndexHash, IndexEqual> indexCombinations(originalIndices.size()/stride, IndexHash(originalIndices, stride), IndexEqual(originalIndices, stride));
    vector<unsigned int>* indices = new vector<unsigned int>;
    for(size_t i = 0, end = originalIndices.size()/stride; i != end; ++i)
        indices->push_back(indexCombinations.insert(make_pair(i, indexCombinations.size())).first->second);

    /* Get mesh vertices */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input[@semantic='VERTEX']/@source/string()")
        .arg(id+1));
    d->query.evaluateTo(&tmp);
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/vertices[@id='%0']/input[@semantic='POSITION']/@source/string()")
        .arg(tmp.mid(1).trimmed()));
    d->query.evaluateTo(&tmp);
    vector<Vector3> originalVertices = parseSource<Vector3>(tmp.mid(1).trimmed());

    /* Build vertex array */
    GLuint vertexOffset = attributeOffset(id, "VERTEX");
    vector<Vector4>* vertices = new vector<Vector4>(indexCombinations.size());
    for(auto i: indexCombinations)
        (*vertices)[i.second] = originalVertices[originalIndices[i.first*stride+vertexOffset]];

    QStringList tmpList;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input/@semantic/string()").arg(id+1));
    d->query.evaluateTo(&tmpList);
    vector<vector<Vector3>*> normals;
    vector<vector<Vector2>*> textureCoords2D;
    for(QString attribute: tmpList) {
        /* Vertices - already built */
        if(attribute == "VERTEX") continue;

        /* Normals */
        else if(attribute == "NORMAL")
            normals.push_back(buildAttributeArray<Vector3>(id, "NORMAL", normals.size(), originalIndices, stride, indexCombinations));

        /* 2D texture coords */
        else if(attribute == "TEXCOORD")
            textureCoords2D.push_back(buildAttributeArray<Vector2>(id, "TEXCOORD", textureCoords2D.size(), originalIndices, stride, indexCombinations));

        /* Something other */
        else Warning() << "ColladaImporter:" << '"' + attribute.toStdString() + '"' << "input semantic not supported";
    }

    Debug() << QString("ColladaImporter: mesh contains %0 faces and %1 unique vertices")
        .arg(indices->size()/3).arg(vertices->size()).toStdString();

    d->meshes[id] = new MeshData(Mesh::Primitive::Triangles, indices, {vertices}, normals, textureCoords2D);
    return d->meshes[id];
}

AbstractMaterialData* ColladaImporter::material(size_t id) {
    if(!d || id >= d->materials.size()) return nullptr;
    if(d->materials[id]) return d->materials[id];

    QString tmp;

    /* Get effect ID */
    QString effect;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_materials/material[%0]/instance_effect/@url/string()").arg(id+1));
    d->query.evaluateTo(&effect);
    effect = effect.mid(1).trimmed();

    /* Find out which profile it is */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/*[substring(name(), 1, 8) = 'profile_']/name()").arg(effect));
    d->query.evaluateTo(&tmp);

    /** @todo Support other profiles */

    if(tmp.trimmed() != "profile_COMMON") {
        Error() << "ColladaImporter:" << ('"'+tmp.trimmed()+'"').toStdString() << "effect profile not supported";
        return nullptr;
    }

    /* Get shader type */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/*/name()").arg(effect));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    /** @todo Other (blinn, goraund) profiles */
    if(tmp != "phong") {
        Error() << "ColladaImporter:" << ('"'+tmp+'"').toStdString() << "shader not supported";
        return nullptr;
    }

    /* Ambient color */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/ambient/color/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    Vector3 ambientColor = Utility::parseVector<Vector3>(tmp);

    /* Diffuse color */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/diffuse/color/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    Vector3 diffuseColor = Utility::parseVector<Vector3>(tmp);

    /* Specular color */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/specular/color/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    Vector3 specularColor = Utility::parseVector<Vector3>(tmp);

    /* Shininess */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/shininess/float/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    GLfloat shininess = ColladaType<GLfloat>::fromString(tmp);

    /** @todo Emission, IOR */

    d->materials[id] = new PhongMaterialData(ambientColor, diffuseColor, specularColor, shininess);
    return d->materials[id];
}

ImageData2D* ColladaImporter::image2D(size_t id) {
    if(!d || id >= d->images2D.size()) return nullptr;
    if(d->images2D[id]) return d->images2D[id];

    QString tmp;

    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_images/image[%0]/init_from/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    if(tmp.right(3) != "tga") {
        Error() << "ColladaImporter:" << '"' + tmp.toStdString() + '"' << "has unsupported format";
        return nullptr;
    }

    TgaImporter::TgaImporter tgaImporter;
    ImageData2D* image;
    if(!tgaImporter.open(Directory::join(Directory::path(d->filename), tmp.toStdString())) || !(image = tgaImporter.image2D(0)))
        return nullptr;

    d->images2D[id] = image;
    return d->images2D[id];
}

GLuint ColladaImporter::attributeOffset(size_t meshId, const QString& attribute, unsigned int id) {
    QString tmp;

    /* Get attribute offset in indices */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input[@semantic='%1'][%2]/@offset/string()")
        .arg(meshId+1).arg(attribute).arg(id+1));
    d->query.evaluateTo(&tmp);
    return ColladaType<GLuint>::fromString(tmp);
}

}}}
