/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013 Vladimír Vondruš <mosra@centrum.cz>

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

#include "ColladaImporter.h"

#include <QtCore/QFile>
#include <QtCore/QStringList>
#include <Utility/Directory.h>
#include <Math/Constants.h>
#include <Trade/ImageData.h>
#include <Trade/MeshData3D.h>
#include <Trade/MeshObjectData3D.h>
#include <Trade/PhongMaterialData.h>
#include <Trade/SceneData.h>

#include "TgaImporter/TgaImporter.h"

namespace Magnum { namespace Trade {

const QString ColladaImporter::namespaceDeclaration =
    "declare default element namespace \"http://www.collada.org/2005/11/COLLADASchema\";\n";

ColladaImporter::ColladaImporter(): d(nullptr), zero(0), app(qApp ? nullptr : new QCoreApplication(zero, nullptr)) {}

ColladaImporter::ColladaImporter(PluginManager::AbstractManager* manager, std::string plugin): AbstractImporter(manager, std::move(plugin)), d(nullptr), zero(0), app(qApp ? nullptr : new QCoreApplication(zero, nullptr)) {}

ColladaImporter::~ColladaImporter() {
    close();
    delete app;
}

ColladaImporter::Document::~Document() {
    for(auto i: scenes) delete i.second;
    for(auto i: objects) delete i.second;
}

auto ColladaImporter::doFeatures() const -> Features { return {}; }

bool ColladaImporter::doIsOpened() const { return d; }

void ColladaImporter::doOpenFile(const std::string& filename) {
    QXmlQuery query;

    /* Open the file and load it into XQuery */
    QFile file(QString::fromStdString(filename));
    if(!file.open(QIODevice::ReadOnly)) {
        Error() << "ColladaImporter: cannot open file" << filename;
        return;
    }
    if(!query.setFocus(&file)) {
        Error() << "ColladaImporter: cannot load XML";
        return;
    }

    QString tmp;

    /* Check namespace */
    query.setQuery("namespace-uri(/*:COLLADA)");
    query.evaluateTo(&tmp);
    tmp = tmp.trimmed();
    if(tmp != "http://www.collada.org/2005/11/COLLADASchema") {
        Error() << "ColladaImporter: unsupported namespace" << ('"'+tmp+'"').toStdString();
        return;
    }

    /* Check version */
    query.setQuery(namespaceDeclaration + "/COLLADA/@version/string()");
    query.evaluateTo(&tmp);
    tmp = tmp.trimmed();
    if(tmp != "1.4.1") {
        Error() << "ColladaImporter: unsupported version" << ('"'+tmp+'"').toStdString();
        return;
    }

    d = new Document;
    d->filename = filename;
    d->query = query;

    /* Scenes */
    query.setQuery(namespaceDeclaration + "count(/COLLADA/library_visual_scenes/visual_scene)");
    query.evaluateTo(&tmp);
    d->scenes.resize(Implementation::ColladaType<UnsignedInt>::fromString(tmp));

    /* Objects */
    query.setQuery(namespaceDeclaration + "count(/COLLADA/library_visual_scenes/visual_scene//node)");
    query.evaluateTo(&tmp);
    d->objects.resize(Implementation::ColladaType<UnsignedInt>::fromString(tmp));

    QStringList tmpList;

    /* Create camera name -> camera id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_cameras/camera/@id/string()");
    query.evaluateTo(&tmpList);
    d->camerasForName.reserve(tmpList.size());
    for(const QString id: tmpList)
        d->camerasForName.insert(std::make_pair(id.trimmed().toStdString(), d->camerasForName.size()));

    /* Create light name -> light id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_lights/light/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->lightsForName.reserve(tmpList.size());
    for(const QString id: tmpList)
        d->lightsForName.insert(std::make_pair(id.trimmed().toStdString(), d->lightsForName.size()));

    /* Create material name -> material id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_materials/material/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->materials.reserve(tmpList.size());
    d->materialsForName.reserve(tmpList.size());
    for(const QString id: tmpList) {
        std::string name = id.trimmed().toStdString();
        d->materials.push_back(name);
        d->materialsForName.insert({name, d->materialsForName.size()});
    }

    /* Create mesh name -> mesh id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_geometries/geometry/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->meshes.reserve(tmpList.size());
    d->meshesForName.reserve(tmpList.size());
    for(const QString id: tmpList) {
        std::string name = id.trimmed().toStdString();
        d->meshes.push_back(name);
        d->meshesForName.insert({name, d->meshesForName.size()});
    }

    /* Create image name -> image id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_images/image/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->images2D.reserve(tmpList.size());
    d->images2DForName.reserve(tmpList.size());
    for(const QString id: tmpList) {
        std::string name = id.trimmed().toStdString();
        d->images2D.push_back(name);
        d->images2DForName.insert({name, d->images2DForName.size()});
    }
}

void ColladaImporter::doClose() {
    delete d;
    d = nullptr;
}

Int ColladaImporter::doDefaultScene() {
    CORRADE_ASSERT(d, "Trade::ColladaImporter::ColladaImporter::defaultScene(): no file opened", -1);

    if(d->scenes.empty()) return -1;
    if(!d->scenes[0].second) parseScenes();

    return d->defaultScene;
}

UnsignedInt ColladaImporter::doSceneCount() const { return d->scenes.size(); }

std::string ColladaImporter::doSceneName(const UnsignedInt id) {
    if(!d->scenes[0].second) parseScenes();
    return d->scenes[id].first;
}

SceneData* ColladaImporter::doScene(const UnsignedInt id) {
    if(!d->scenes[0].second) parseScenes();
    return d->scenes[id].second;
    /** @todo return copy so user can delete it */
}

UnsignedInt ColladaImporter::doObject3DCount() const { return d->objects.size(); }

Int ColladaImporter::doObject3DForName(const std::string& name) {
    if(d->scenes.empty()) return -1;
    if(!d->scenes[0].second) parseScenes();

    auto it = d->objectsForName.find(name);
    return (it == d->objectsForName.end()) ? -1 : it->second;
}

std::string ColladaImporter::doObject3DName(const UnsignedInt id) {
    if(!d->scenes[0].second) parseScenes();
    return d->objects[id].first;
}

ObjectData3D* ColladaImporter::doObject3D(const UnsignedInt id) {
    if(!d->scenes[0].second) parseScenes();
    return d->objects[id].second;
    /** @todo return copy so user can delete it */
}

UnsignedInt ColladaImporter::doMesh3DCount() const { return d->meshes.size(); }

Int ColladaImporter::doMesh3DForName(const std::string& name) {
    auto it = d->meshesForName.find(name);
    return (it == d->meshesForName.end()) ? -1 : it->second;
}

std::string ColladaImporter::doMesh3DName(const UnsignedInt id) { return d->meshes[id]; }

MeshData3D* ColladaImporter::doMesh3D(const UnsignedInt id) {
    /** @todo More polylists in one mesh */

    /* Get polygon count */
    QString tmp;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/@count/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    UnsignedInt polygonCount = Implementation::ColladaType<UnsignedInt>::fromString(tmp);

    /* Get vertex count per polygon */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/vcount/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    std::vector<UnsignedInt> vertexCountPerFace = Implementation::Utility::parseArray<UnsignedInt>(tmp, polygonCount);

    UnsignedInt vertexCount = 0;
    std::vector<UnsignedInt> quads;
    for(std::size_t i = 0; i != vertexCountPerFace.size(); ++i) {
        UnsignedInt count = vertexCountPerFace[i];

        if(count == 4) quads.push_back(i);
        else if(count != 3) {
            Error() << "ColladaImporter:" << count << "vertices per face not supported";
            return nullptr;
        }

        vertexCount += count;
    }

    /* Get input count per vertex */
    d->query.setQuery((namespaceDeclaration + "count(/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input)").arg(id+1));
    d->query.evaluateTo(&tmp);
    UnsignedInt stride = Implementation::ColladaType<UnsignedInt>::fromString(tmp);

    /* Get mesh indices */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/p/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    std::vector<UnsignedInt> originalIndices = Implementation::Utility::parseArray<UnsignedInt>(tmp, vertexCount*stride);

    /** @todo assert size()%stride == 0 */

    /* Get unique combinations of vertices, build resulting index array. Key
       is position of unique index combination from original vertex array,
       value is resulting index. */
    std::unordered_map<UnsignedInt, UnsignedInt, IndexHash, IndexEqual> indexCombinations(originalIndices.size()/stride, IndexHash(originalIndices, stride), IndexEqual(originalIndices, stride));
    std::vector<UnsignedInt> combinedIndices;
    for(std::size_t i = 0, end = originalIndices.size()/stride; i != end; ++i)
        combinedIndices.push_back(indexCombinations.insert(std::make_pair(i, indexCombinations.size())).first->second);

    /* Convert quads to triangles */
    std::vector<UnsignedInt> indices;
    std::size_t quadId = 0;
    for(std::size_t i = 0; i != vertexCountPerFace.size(); ++i) {
        if(quads.size() > quadId && quads[quadId] == i) {
            indices.insert(indices.end(), {
                combinedIndices[i*3+quadId],
                combinedIndices[i*3+quadId+1],
                combinedIndices[i*3+quadId+2],
                combinedIndices[i*3+quadId],
                combinedIndices[i*3+quadId+2],
                combinedIndices[i*3+quadId+3]
            });

            ++quadId;
        } else {
            indices.insert(indices.end(), {
                combinedIndices[i*3+quadId],
                combinedIndices[i*3+quadId+1],
                combinedIndices[i*3+quadId+2]
            });
        }
    }

    /* Get mesh vertices */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input[@semantic='VERTEX']/@source/string()")
        .arg(id+1));
    d->query.evaluateTo(&tmp);
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/vertices[@id='%0']/input[@semantic='POSITION']/@source/string()")
        .arg(tmp.mid(1).trimmed()));
    d->query.evaluateTo(&tmp);
    std::vector<Vector3> originalVertices = parseSource<Vector3>(tmp.mid(1).trimmed());

    /* Build vertex array */
    UnsignedInt vertexOffset = attributeOffset(id, "VERTEX");
    std::vector<Vector3> vertices(indexCombinations.size());
    for(auto i: indexCombinations)
        vertices[i.second] = originalVertices[originalIndices[i.first*stride+vertexOffset]];

    QStringList tmpList;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input/@semantic/string()").arg(id+1));
    d->query.evaluateTo(&tmpList);
    std::vector<std::vector<Vector3>> normals;
    std::vector<std::vector<Vector2>> textureCoords2D;
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

    return new MeshData3D(Mesh::Primitive::Triangles, std::move(indices), {std::move(vertices)}, std::move(normals), std::move(textureCoords2D));
}

UnsignedInt ColladaImporter::doMaterialCount() const { return d->materials.size(); }

Int ColladaImporter::doMaterialForName(const std::string& name) {
    auto it = d->materialsForName.find(name);
    return (it == d->materialsForName.end()) ? -1 : it->second;
}

std::string ColladaImporter::doMaterialName(const UnsignedInt id) {
    return d->materials[id];
}

AbstractMaterialData* ColladaImporter::doMaterial(const UnsignedInt id) {
    /* Get effect ID */
    QString effect;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_materials/material[%0]/instance_effect/@url/string()").arg(id+1));
    d->query.evaluateTo(&effect);
    effect = effect.mid(1).trimmed();

    /* Find out which profile it is */
    QString tmp;
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
    Vector3 ambientColor = Implementation::Utility::parseVector<Vector3>(tmp);

    /* Diffuse color */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/diffuse/color/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    Vector3 diffuseColor = Implementation::Utility::parseVector<Vector3>(tmp);

    /* Specular color */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/specular/color/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    Vector3 specularColor = Implementation::Utility::parseVector<Vector3>(tmp);

    /* Shininess */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/shininess/float/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    Float shininess = Implementation::ColladaType<Float>::fromString(tmp);

    /** @todo Emission, IOR */

    return new PhongMaterialData(ambientColor, diffuseColor, specularColor, shininess);
}

UnsignedInt ColladaImporter::doImage2DCount() const { return d->images2D.size(); }

Int ColladaImporter::doImage2DForName(const std::string& name) {
    auto it = d->images2DForName.find(name);
    return (it == d->images2DForName.end()) ? -1 : it->second;
}

std::string ColladaImporter::doImage2DName(const UnsignedInt id) {
    return d->images2D[id];
}

ImageData2D* ColladaImporter::doImage2D(const UnsignedInt id) {
    /* Image filename */
    QString tmp;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_images/image[%0]/init_from/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    if(tmp.right(3) != "tga") {
        Error() << "ColladaImporter:" << '"' + tmp.toStdString() + '"' << "has unsupported format";
        return nullptr;
    }

    TgaImporter tgaImporter;
    if(!tgaImporter.openFile(Utility::Directory::join(Utility::Directory::path(d->filename), tmp.toStdString())))
        return nullptr;

    ImageData2D* image;
    if(!(image = tgaImporter.image2D(0)))
        return nullptr;

    return image;
}

UnsignedInt ColladaImporter::attributeOffset(UnsignedInt meshId, const QString& attribute, UnsignedInt id) {
    QString tmp;

    /* Get attribute offset in indices */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input[@semantic='%1'][%2]/@offset/string()")
        .arg(meshId+1).arg(attribute).arg(id+1));
    d->query.evaluateTo(&tmp);
    return Implementation::ColladaType<UnsignedInt>::fromString(tmp);
}

void ColladaImporter::parseScenes() {
    QStringList tmpList;
    QString tmp;

    /* Default scene */
    d->defaultScene = 0;
    d->query.setQuery(namespaceDeclaration + "/COLLADA/scene/instance_visual_scene/@url/string()");
    d->query.evaluateTo(&tmp);
    std::string defaultScene = tmp.trimmed().mid(1).toStdString();

    /* Parse all objects in all scenes */
    for(UnsignedInt sceneId = 0; sceneId != d->scenes.size(); ++sceneId) {
        /* Is this the default scene? */
        d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene[%0]/@id/string()").arg(sceneId+1));
        d->query.evaluateTo(&tmp);
        std::string name = tmp.trimmed().toStdString();
        if(defaultScene == name)
            d->defaultScene = sceneId;

        UnsignedInt nextObjectId = 0;
        std::vector<UnsignedInt> children;
        d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene[%0]/node/@id/string()").arg(sceneId+1));
        tmpList.clear();
        d->query.evaluateTo(&tmpList);
        for(QString childId: tmpList) {
            children.push_back(nextObjectId);
            nextObjectId = parseObject(nextObjectId, childId.trimmed());
        }

        d->scenes[sceneId] = {name, new SceneData({}, std::move(children))};
    }
}

UnsignedInt ColladaImporter::parseObject(UnsignedInt id, const QString& name) {
    QString tmp;
    QStringList tmpList, tmpList2;

    /* Transformations */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/(translate|rotate|scale)/name()").arg(name));
    d->query.evaluateTo(&tmpList);

    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/(translate|rotate|scale)/string()").arg(name));
    d->query.evaluateTo(&tmpList2);

    Matrix4 transformation;
    for(std::size_t i = 0; i != std::size_t(tmpList.size()); ++i) {
        QString type = tmpList[i].trimmed();
        /* Translation */
        if(type == "translate")
            transformation = transformation*Matrix4::translation(Implementation::Utility::parseVector<Vector3>(tmpList2[i]));

        /* Rotation */
        else if(type == "rotate") {
            int pos = 0;
            Vector3 axis = Implementation::Utility::parseVector<Vector3>(tmpList2[i], &pos);
            Deg angle(Implementation::ColladaType<Float>::fromString(tmpList2[i].mid(pos)));
            transformation = transformation*Matrix4::rotation(angle, axis);

        /* Scaling */
        } else if(type == "scale")
            transformation = transformation*Matrix4::scaling(Implementation::Utility::parseVector<Vector3>(tmpList2[i]));

        /* It shouldn't get here */
        else CORRADE_ASSERT(0, ("ColladaImporter: unknown translation " + type).toStdString(), id);
    }

    /* Instance type */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/*[substring(name(), 1, 9) = 'instance_']/name()").arg(name));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    /* Camera instance */
    if(tmp == "instance_camera") {
        std::string cameraName = instanceName(name, "instance_camera");
        auto cameraId = d->camerasForName.find(cameraName);
        if(cameraId == d->camerasForName.end()) {
            Error() << "ColladaImporter: camera" << '"'+cameraName+'"' << "was not found";
            return id;
        }

        d->objects[id] = {name.toStdString(), new ObjectData3D({}, transformation, ObjectData3D::InstanceType::Camera, cameraId->second)};

    /* Light instance */
    } else if(tmp == "instance_light") {
        std::string lightName = instanceName(name, "instance_light");
        auto lightId = d->lightsForName.find(lightName);
        if(lightId == d->lightsForName.end()) {
            Error() << "ColladaImporter: light" << '"'+lightName+'"' << "was not found";
            return id;
        }

        d->objects[id] = {name.toStdString(), new ObjectData3D({}, transformation, ObjectData3D::InstanceType::Light, lightId->second)};

    /* Mesh instance */
    } else if(tmp == "instance_geometry") {
        std::string meshName = instanceName(name, "instance_geometry");
        auto meshId = d->meshesForName.find(meshName);
        if(meshId == d->meshesForName.end()) {
            Error() << "ColladaImporter: mesh" << '"'+meshName+'"' << "was not found";
            return id;
        }

        d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/instance_geometry/bind_material/technique_common/instance_material/@target/string()").arg(name));
        d->query.evaluateTo(&tmp);
        std::string materialName = tmp.trimmed().mid(1).toStdString();

        /* Mesh doesn't have bound material, add default one */
        /** @todo Solution for unknown materials etc.: -1 ? */
        if(materialName.empty()) d->objects[id] = {name.toStdString(), new MeshObjectData3D({}, transformation, meshId->second, 0)};

        /* Else find material ID */
        else {
            auto materialId = d->materialsForName.find(materialName);
            if(materialId == d->materialsForName.end()) {
                Error() << "ColladaImporter: material" << '"'+materialName+'"' << "was not found";
                return id;
            }

            d->objects[id] = {name.toStdString(), new MeshObjectData3D({}, transformation, meshId->second, materialId->second)};
        }

    /* Blender group instance */
    } else if(tmp.isEmpty()) {
        d->objects[id] = {name.toStdString(), new ObjectData3D({}, transformation)};

    } else {
        Error() << "ColladaImporter:" << '"'+tmp.toStdString()+'"' << "instance type not supported";
        return id;
    }

    /* Add to object name map */
    d->objectsForName.insert({name.toStdString(), id});

    /* Parse child objects */
    UnsignedInt nextObjectId = id+1;
    std::vector<UnsignedInt> children;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/node/@id/string()").arg(name));
    tmpList.clear();
    d->query.evaluateTo(&tmpList);
    for(QString childId: tmpList) {
        children.push_back(nextObjectId);
        nextObjectId = parseObject(nextObjectId, childId.trimmed());
    }
    d->objects[id].second->children().swap(children);

    return nextObjectId;
}

std::string ColladaImporter::instanceName(const QString& name, const QString& instanceTag) {
    QString tmp;

    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/%1/@url/string()").arg(name).arg(instanceTag));
    d->query.evaluateTo(&tmp);
    return tmp.trimmed().mid(1).toStdString();
}

}}
