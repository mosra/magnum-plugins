/*
    Copyright © 2010, 2011 Vladimír Vondruš <mosra@centrum.cz>

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

#include <QtCore/QDebug>
#include <QtCore/QVector>
#include <QtCore/QFile>
#include <QtCore/QStringList>

#include "Utility/Debug.h"

#include "IndexedMesh.h"

#include "MeshObject.h"
#include "PhongShader.h"
#include "Material.h"
#include "PointLight.h"
#include "ColladaType.h"
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

    /* Geometry count */
    query.setQuery(namespaceDeclaration + "count(//library_geometries/geometry)");
    query.evaluateTo(&tmp);
    GLuint geometryCount = ColladaType<GLuint>::fromString(tmp);

    d = new Document(geometryCount);
    d->query = query;

    Debug() << QString("ColladaImporter: file contains\n    %1 geometries")
        .arg(geometryCount).toStdString();

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
    vector<GLuint> vertexCountPerFace = parseArray<GLuint>(tmp, polygonCount);

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
    vector<GLuint> indices = parseArray<GLuint>(tmp, vertexCount*stride);

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

    /** @todo fixme */
    shared_ptr<PhongShader> shader(new PhongShader());
    shared_ptr<PointLight> light(new PointLight(
        Vector3(), Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f)));
    light->translate(-3, 10, 10);
    shared_ptr<Material> material (new Material(shader,
        Vector3(), Vector3(1.0f, 1.0f, 0.9f), Vector3(1.0f, 1.0f, 1.0f),
        128.0f, light));
    d->geometries[id] = shared_ptr<Object>(new MeshObject(mesh, material));

    return d->geometries[id];
}

template<class T> vector<T> ColladaImporter::parseSource(const QString& id) {
    vector<T> output;
    QString tmp;

    /* Count of items */
    d->query.setQuery((namespaceDeclaration + "//source[@id='%0']/technique_common/accessor/@count/string()").arg(id));
    d->query.evaluateTo(&tmp);
    GLuint count = ColladaType<GLuint>::fromString(tmp);

    /* Size of each item */
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
    int to;
    for(size_t i = 0; i != count; ++i) {
        T item;
        for(size_t j = 0; j != size; ++j) {
            to = tmp.indexOf(' ', from);
            while(to == from)
                to = tmp.indexOf(' ', ++from);
            item.set(j, ColladaType<typename T::Type>::fromString(tmp.mid(from, to-from)));
            from = to+1;
        }
        output.push_back(item);
    }

    return output;
}

template<class T> vector<T> ColladaImporter::parseArray(const QString& data, size_t count) {
    vector<T> output;
    output.reserve(count);

    int from = 0;
    int to;
    for(size_t i = 0; i != count; ++i) {
        to = data.indexOf(' ', from);
        while(to == from)
            to = data.indexOf(' ', ++from);
        output.push_back(ColladaType<T>::fromString(data.mid(from, to-from)));
        from = to+1;
    }

    return output;
}

}}}
