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

#include <cassert>
#include <QtCore/QFile>
#include <QtCore/QStringList>

#include "Utility/Directory.h"
#include "SizeTraits.h"
#include "Trade/PhongMaterialData.h"
#include "Trade/MeshData.h"
#include "Trade/ObjectData.h"
#include "Trade/SceneData.h"
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

    /* Scenes */
    query.setQuery(namespaceDeclaration + "count(/COLLADA/library_visual_scenes/visual_scene)");
    query.evaluateTo(&tmp);
    GLuint sceneCount = ColladaType<GLuint>::fromString(tmp);

    /* Objects */
    query.setQuery(namespaceDeclaration + "count(/COLLADA/library_visual_scenes/visual_scene//node)");
    query.evaluateTo(&tmp);
    GLuint objectCount = ColladaType<GLuint>::fromString(tmp);

    /* Meshes */
    query.setQuery(namespaceDeclaration + "count(/COLLADA/library_geometries/geometry)");
    query.evaluateTo(&tmp);
    GLuint meshCount = ColladaType<GLuint>::fromString(tmp);

    /* Materials */
    query.setQuery(namespaceDeclaration + "count(/COLLADA/library_materials/material/@id/string())");
    query.evaluateTo(&tmp);
    GLuint materialCount = ColladaType<GLuint>::fromString(tmp);

    /* Images */
    query.setQuery(namespaceDeclaration + "count(/COLLADA/library_images/image)");
    query.evaluateTo(&tmp);
    GLuint image2DCount = ColladaType<GLuint>::fromString(tmp);

    d = new Document(sceneCount, objectCount, meshCount, materialCount, image2DCount);
    d->filename = filename;
    d->query = query;

    return true;
}

void ColladaImporter::close() {
    if(!d) return;

    delete d;
    d = 0;
}

size_t ColladaImporter::ColladaImporter::defaultScene() {
    if(!d || d->scenes.empty()) return 0;
    if(!d->scenes[0]) parseScenes();

    return d->defaultScene;
}

SceneData* ColladaImporter::ColladaImporter::scene(size_t id) {
    if(!d || id >= d->scenes.size()) return nullptr;
    if(!d->scenes[0]) parseScenes();

    return d->scenes[id];
}

ObjectData* ColladaImporter::ColladaImporter::object(size_t id) {
    if(!d || id >= d->objects.size()) return nullptr;
    if(!d->scenes[0]) parseScenes();

    return d->objects[id];
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

void ColladaImporter::parseScenes() {
    QStringList tmpList;
    QString tmp;

    /* Create camera name -> camera id map */
    d->query.setQuery(namespaceDeclaration + "/COLLADA/library_cameras/camera/@id/string()");
    tmpList.clear();
    d->query.evaluateTo(&tmpList);
    unordered_map<string, size_t> cameraMap;
    for(const QString id: tmpList)
        cameraMap.insert(make_pair(id.trimmed().toStdString(), cameraMap.size()));

    /* Create light name -> light id map */
    d->query.setQuery(namespaceDeclaration + "/COLLADA/library_lights/light/@id/string()");
    tmpList.clear();
    d->query.evaluateTo(&tmpList);
    unordered_map<string, size_t> lightMap;
    for(const QString id: tmpList)
        lightMap.insert(make_pair(id.trimmed().toStdString(), lightMap.size()));

    /* Create mesh name -> mesh id map */
    d->query.setQuery(namespaceDeclaration + "/COLLADA/library_geometries/geometry/@id/string()");
    tmpList.clear();
    d->query.evaluateTo(&tmpList);
    unordered_map<string, size_t> meshMap;
    for(const QString id: tmpList)
        meshMap.insert(make_pair(id.trimmed().toStdString(), meshMap.size()));

    /* Default scene */
    d->defaultScene = 0;
    d->query.setQuery(namespaceDeclaration + "/COLLADA/scene/instance_visual_scene/@url/string()");
    d->query.evaluateTo(&tmp);
    QString defaultScene = tmp.trimmed().mid(1);

    /* Parse all objects in all scenes */
    for(size_t sceneId = 0; sceneId != d->scenes.size(); ++sceneId) {
        /* Is this the default scene? */
        d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene[%0]/@id/string()").arg(sceneId+1));
        d->query.evaluateTo(&tmp);
        if(defaultScene == tmp.trimmed())
            d->defaultScene = sceneId;

        size_t nextObjectId = 0;
        vector<size_t> children;
        d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene[%0]/node/@id/string()").arg(sceneId+1));
        tmpList.clear();
        d->query.evaluateTo(&tmpList);
        for(QString childId: tmpList) {
            children.push_back(nextObjectId);
            nextObjectId = parseObject(nextObjectId, childId.trimmed(), cameraMap, lightMap, meshMap);
        }

        d->scenes[sceneId] = new SceneData(children);
    }
}

size_t ColladaImporter::parseObject(size_t id, const QString& name, const unordered_map<string, size_t>& cameraMap, const unordered_map<string, size_t>& lightMap, const unordered_map<string, size_t>& meshMap) {
    QString tmp;
    QStringList tmpList, tmpList2;

    /* Transformations */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/(translate|rotate|scale)/name()").arg(name));
    d->query.evaluateTo(&tmpList);

    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/(translate|rotate|scale)/string()").arg(name));
    d->query.evaluateTo(&tmpList2);

    Matrix4 transformation;
    for(size_t i = 0; i != static_cast<size_t>(tmpList.size()); ++i) {
        /* Translation */
        if(tmpList[i].trimmed() == "translate")
            transformation *= Matrix4::translation(Utility::parseVector<Vector3>(tmpList2[i]));

        /* Rotation */
        else if(tmpList[i].trimmed() == "rotate") {
            int pos = 0;
            Vector3 axis = Utility::parseVector<Vector3>(tmpList2[i], &pos);
            GLfloat angle = ColladaType<GLfloat>::fromString(tmpList2[i].mid(pos));
            transformation *= Matrix4::rotation(deg(angle), axis);

        /* Scaling */
        } else if(tmpList[i].trimmed() == "scale")
            transformation *= Matrix4::scaling(Utility::parseVector<Vector3>(tmpList2[i]));

        /* It shouldn't get here */
        else assert(0);
    }

    /* Instance type */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/*[substring(name(), 1, 9) = 'instance_']/name()").arg(name));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    ObjectData::InstanceType instanceType;
    size_t instanceId = 0;

    /* Camera instance */
    if(tmp == "instance_camera") {
        instanceType = ObjectData::InstanceType::Camera;
        string cameraName = instanceName(name, "instance_camera");
        auto cameraId = cameraMap.find(cameraName);
        if(cameraId == cameraMap.end()) {
            Error() << "ColladaImporter: camera" << '"'+cameraName+'"' << "was not found";
            return id;
        }
        instanceId = cameraId->second;

    /* Light instance */
    } else if(tmp == "instance_light") {
        instanceType = ObjectData::InstanceType::Light;

        string lightName = instanceName(name, "instance_light");
        auto lightId = lightMap.find(lightName);
        if(lightId == lightMap.end()) {
            Error() << "ColladaImporter: light" << '"'+lightName+'"' << "was not found";
            return id;
        }
        instanceId = lightId->second;

    /* Mesh instance */
    } else if(tmp == "instance_geometry") {
        instanceType = ObjectData::InstanceType::Mesh;

        string meshName = instanceName(name, "instance_geometry");
        auto meshId = meshMap.find(meshName);
        if(meshId == meshMap.end()) {
            Error() << "ColladaImporter: mesh" << '"'+meshName+'"' << "was not found";
            return id;
        }
        instanceId = meshId->second;

    } else {
        Error() << "ColladaImporter:" << '"'+tmp.toStdString()+'"' << "instance type not supported";
        return id;
    }

    /* Parse child objects */
    size_t nextObjectId = id+1;
    vector<size_t> children;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/node/@id/string()").arg(name));
    tmpList.clear();
    d->query.evaluateTo(&tmpList);
    for(QString childId: tmpList) {
        children.push_back(nextObjectId);
        nextObjectId = parseObject(nextObjectId, childId.trimmed(), cameraMap, lightMap, meshMap);
    }

    d->objects[id] = new ObjectData(children, transformation, instanceType, instanceId);
    return nextObjectId;
}

string ColladaImporter::instanceName(const QString& name, const QString& instanceTag) {
    QString tmp;

    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/%1/@url/string()").arg(name).arg(instanceTag));
    d->query.evaluateTo(&tmp);
    return tmp.trimmed().mid(1).toStdString();
}

}}}
