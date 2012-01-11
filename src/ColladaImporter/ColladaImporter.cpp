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

#include "IndexedMesh.h"

#include "MeshObject.h"
#include "PhongShader.h"
#include "Material.h"
#include "PointLight.h"
#include "ColladaType.h"
#include "Utility.h"
#include "SizeTraits.h"

using namespace std;
using namespace Corrade::Utility;
using namespace Corrade::PluginManager;

PLUGIN_REGISTER(Magnum::Plugins::ColladaImporter::ColladaImporter, "cz.mosra.magnum.AbstractImporter/0.1")

namespace Magnum { namespace Plugins { namespace ColladaImporter {

const QString ColladaImporter::namespaceDeclaration =
    "declare default element namespace \"http://www.collada.org/2005/11/COLLADASchema\";\n";

ColladaImporter::ColladaImporter(AbstractPluginManager* manager, const string& plugin): AbstractImporter(manager, plugin), d(0), zero(0), app(zero, 0) {
}

bool ColladaImporter::open(istream& in) {
    /* Close previous document */
    if(d) close();

    QXmlQuery query;

    /* Open the file and load it into XQuery */
    {
        in.seekg(0, ios::end);
        in.getloc();
        size_t size = in.tellg();
        in.seekg(0, ios::beg);
        char* buffer = new char[size];
        in.read(buffer, size);
        QString data = QString::fromUtf8(buffer, size);
        delete[] buffer;
        if(!query.setFocus(data)) {
            Error() << "ColladaImporter: cannot load XML";
            return false;
        }
    }

    QString tmp;
    QStringList listTmp;

    /* Geometry count */
    query.setQuery(namespaceDeclaration + "count(//library_geometries/geometry)");
    query.evaluateTo(&tmp);
    GLuint geometryCount = ColladaType<GLuint>::fromString(tmp);

    /* Materials */
    query.setQuery(namespaceDeclaration + "//library_materials/material/@id/string()");
    query.evaluateTo(&listTmp);

    d = new Document(geometryCount, listTmp.size());
    d->query = query;

    /* Add all materials to material map */
    for(size_t i = 0; i != static_cast<size_t>(listTmp.size()); ++i)
        d->materialMap[listTmp[i].toStdString()] = i;

    Debug() << QString("ColladaImporter: file contains\n"
                       "    %0 geometries\n"
                       "    %1 materials")
        .arg(geometryCount).arg(listTmp.size()).toStdString();

    return true;
}

void ColladaImporter::close() {
    if(!d) return;

    delete d;
    d = 0;
}

shared_ptr<Object> ColladaImporter::object(size_t id) {
    /* Return nullptr if no such object exists, or return existing, if already parsed */
    if(!d || id >= d->geometries.size()) return nullptr;
    if(d->geometries[id]) return d->geometries[id];

    QString tmp;

    /** @todo More polylists in one mesh */

    /* Get polygon count */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/@count/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    GLuint polygonCount = ColladaType<GLuint>::fromString(tmp);

    /* Get vertex count per polygon */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/vcount/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    vector<GLuint> vertexCountPerFace = Utility::parseArray<GLuint>(tmp, polygonCount);

    GLuint vertexCount = 0;
    for(GLuint count: vertexCountPerFace) {
        vertexCount += count;
        if(count == 3) continue;

        /** @todo Support also quads */
        Error() << "Collada import:" << count << "vertices per face not supported";
        return nullptr;
    }

    /* Get input count per vertex */
    d->query.setQuery((namespaceDeclaration + "count(//geometry[%0]/mesh/polylist/input)").arg(id+1));
    d->query.evaluateTo(&tmp);
    GLuint stride = ColladaType<GLuint>::fromString(tmp);

    /* Get mesh indices */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/p/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    vector<GLuint> indices = Utility::parseArray<GLuint>(tmp, vertexCount*stride);

    /* Get mesh vertices offset in indices */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/input[@semantic='VERTEX']/@offset/string()")
        .arg(id+1));
    d->query.evaluateTo(&tmp);
    GLuint vertexOffset = ColladaType<GLuint>::fromString(tmp);

    /* Get mesh vertices */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/input[@semantic='VERTEX']/@source/string()")
        .arg(id+1));
    d->query.evaluateTo(&tmp);
    d->query.setQuery((namespaceDeclaration + "//vertices[@id='%0']/input[@semantic='POSITION']/@source/string()")
        .arg(tmp.mid(1).trimmed()));
    d->query.evaluateTo(&tmp);
    vector<Vector4> vertices = parseSource<Vector4>(tmp.mid(1).trimmed());

    /* Get mesh normals offset in indices */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/input[@semantic='NORMAL']/@offset/string()")
        .arg(id+1));
    d->query.evaluateTo(&tmp);
    GLuint normalOffset = ColladaType<GLuint>::fromString(tmp);

    /* Get mesh normals */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/input[@semantic='NORMAL']/@source/string()")
        .arg(id+1));
    d->query.evaluateTo(&tmp);
    vector<Vector3> normals = parseSource<Vector3>(tmp.mid(1).trimmed());

    /* Unique vertices and normals */
    vector<VertexData> uniqueData;
    vector<GLuint> uniqueIndices;
    QMap<unsigned long long, size_t> uniqueMap;
    for(size_t i = 0; i < static_cast<size_t>(indices.size()); i += stride) {
        unsigned long long uniqueHash = static_cast<unsigned long long>(indices[i]) << 32 | indices[i+1];
        if(uniqueMap.contains(uniqueHash)) {
            uniqueIndices.push_back(uniqueMap[uniqueHash]);
            continue;
        }

        uniqueData.push_back({vertices[indices[i+vertexOffset]], normals[indices[i+normalOffset]]});

        uniqueMap[uniqueHash] = uniqueData.size()-1;
        uniqueIndices.push_back(uniqueData.size()-1);
    }

    Debug() << QString("ColladaImporter: mesh contains %0 faces and %1 unique vertices")
        .arg(uniqueIndices.size()/3).arg(uniqueData.size()/2).toStdString();

    /* Create output mesh */
    IndexedMesh* mesh = new IndexedMesh(Mesh::Triangles, uniqueData.size()/2, 0, 0);

    Buffer* buffer = mesh->addBuffer(true);
    buffer->setData(uniqueData.size()*sizeof(VertexData), uniqueData.data(), Buffer::DrawStatic);
    mesh->bindAttribute<Vector4>(buffer, PhongShader::Vertex);
    mesh->bindAttribute<Vector3>(buffer, PhongShader::Normal);

    SizeBasedCall<IndexBuilder>(uniqueData.size())(mesh, uniqueIndices);

    /* Material ID */
    d->query.setQuery((namespaceDeclaration + "//geometry[%0]/mesh/polylist/@material/string()").arg(id+1));
    d->query.evaluateTo(&tmp);

    shared_ptr<AbstractMaterial> mat(material(d->materialMap[tmp.mid(1).trimmed().toStdString()]));
    if(!mat)
        return nullptr;

    d->geometries[id] = shared_ptr<Object>(new MeshObject(mesh, mat));

    return d->geometries[id];
}

shared_ptr<AbstractMaterial> ColladaImporter::ColladaImporter::material(size_t id) {
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
        Error() << "ColladaImporter:" << tmp.trimmed().toStdString() << "effect profile not supported";
        return nullptr;
    }

    /* Get shader type */
    d->query.setQuery((namespaceDeclaration + "//effect[@id='%0']/profile_COMMON/technique/*/name()").arg(effect));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    /** @todo Other (blinn, goraund) profiles */
    if(tmp != "phong") {
        Error() << "ColladaImporter:" << tmp.toStdString() << "shader not supported";
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
    d->query.setQuery((namespaceDeclaration + "//effect[@id='%0']/profile_COMMON/technique/phong/shininess/color/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    GLfloat shininess = ColladaType<GLfloat>::fromString(tmp);

    /** @todo Emission, IOR */

    /** @todo fixme */
    shared_ptr<PhongShader> shader(new PhongShader());
    shared_ptr<PointLight> light(new PointLight(
        Vector3(), Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f)));
    light->translate(-3, 10, 10);
    d->materials[id] = shared_ptr<Material>(new Material(shader,
        ambientColor, diffuseColor, specularColor, shininess,
        light));

    return d->materials[id];
}

template<class T> vector<T> ColladaImporter::parseSource(const QString& id) {
    vector<T> output;
    QString tmp;

    /* Count of items */
    d->query.setQuery((namespaceDeclaration + "//source[@id='%0']/technique_common/accessor/@count/string()").arg(id));
    d->query.evaluateTo(&tmp);
    GLuint count = ColladaType<GLuint>::fromString(tmp);

    /* Assert that size of each item is size of T */
    d->query.setQuery((namespaceDeclaration + "//source[@id='%0']/technique_common/accessor/@stride/string()").arg(id));
    d->query.evaluateTo(&tmp);
    GLuint size = ColladaType<GLuint>::fromString(tmp);

    /* Data source */
    d->query.setQuery((namespaceDeclaration + "//source[@id='%0']/technique_common/accessor/@source/string()").arg(id));
    d->query.evaluateTo(&tmp);
    QString source = tmp.mid(1).trimmed();

    /** @todo Assert right order of coordinates and type */

    /* Items */
    /** @todo Assert right number of items */
    d->query.setQuery((namespaceDeclaration + "//float_array[@id='%0']/string()").arg(source));
    d->query.evaluateTo(&tmp);

    output.reserve(count);
    int from = 0;
    for(size_t i = 0; i != count; ++i)
        output.push_back(Utility::parseVector<T>(tmp, &from, size));

    return output;
}

}}}
