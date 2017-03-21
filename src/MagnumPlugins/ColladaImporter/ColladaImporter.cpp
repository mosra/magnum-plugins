/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
              Vladimír Vondruš <mosra@centrum.cz>

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

#include <unordered_map>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QStringList>
#include <QtXmlPatterns/QXmlQuery>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Constants.h>
#include <Magnum/MeshTools/CombineIndexedArrays.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>

#include "MagnumPlugins/AnyImageImporter/AnyImageImporter.h"
#include "MagnumPlugins/ColladaImporter/ColladaType.h"
#include "MagnumPlugins/ColladaImporter/Utility.h"

namespace Magnum { namespace Trade {

struct ColladaImporter::Document {
    std::string filename;

    /* Data */
    /** @todo Camera, light names, deduplicate the relevant code */
    std::vector<std::string> scenes,
        objects,
        meshes,
        materials,
        textures,
        images2D;

    /** @todo Make public use for camerasForName, lightsForName */
    std::unordered_map<std::string, UnsignedInt> camerasForName,
        lightsForName,
        scenesForName,
        objectsForName,
        meshesForName,
        materialsForName,
        texturesForName,
        images2DForName;

    QXmlQuery query;
};

const QString ColladaImporter::namespaceDeclaration =
    "declare default element namespace \"http://www.collada.org/2005/11/COLLADASchema\";\n";

/** @todo use init()/fini() for this */
/** @todo somehow delegating constructors for this? */
ColladaImporter::ColladaImporter(): d(nullptr), zero(0), app(qApp ? nullptr : new QCoreApplication(zero, nullptr)) {}

ColladaImporter::ColladaImporter(PluginManager::Manager<AbstractImporter>& manager): AbstractImporter{manager}, d(nullptr), zero(0), app(qApp ? nullptr : new QCoreApplication(zero, nullptr)) {}

ColladaImporter::ColladaImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin}, d(nullptr), zero(0), app(qApp ? nullptr : new QCoreApplication(zero, nullptr)) {}

ColladaImporter::~ColladaImporter() {
    close();
    delete app;
}

auto ColladaImporter::doFeatures() const -> Features { return {}; }

bool ColladaImporter::doIsOpened() const { return d; }

void ColladaImporter::doOpenFile(const std::string& filename) {
    QXmlQuery query;

    /* Open the file and load it into XQuery */
    QFile file(QString::fromStdString(filename));
    if(!file.open(QIODevice::ReadOnly)) {
        Error() << "Trade::ColladaImporter:openFile(): cannot open file" << filename;
        return;
    }
    if(!query.setFocus(&file)) {
        Error() << "Trade::ColladaImporter::openFile(): cannot load XML";
        return;
    }

    QString tmp;

    /* Check namespace */
    query.setQuery("namespace-uri(/*:COLLADA)");
    query.evaluateTo(&tmp);
    tmp = tmp.trimmed();
    if(tmp != "http://www.collada.org/2005/11/COLLADASchema") {
        Error() << "Trade::ColladaImporter::openFile(): unsupported namespace" << ('"'+tmp+'"').toStdString();
        return;
    }

    /* Check version */
    query.setQuery(namespaceDeclaration + "/COLLADA/@version/string()");
    query.evaluateTo(&tmp);
    tmp = tmp.trimmed();
    if(tmp != "1.4.1") {
        Error() << "Trade::ColladaImporter::openFile(): unsupported version" << ('"'+tmp+'"').toStdString();
        return;
    }

    d = new Document;
    d->filename = filename;
    d->query = query;

    QStringList tmpList;

    /* Create scene name -> scene id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene/@id/string()");
    query.evaluateTo(&tmpList);
    d->scenes.reserve(tmpList.size());
    d->scenesForName.reserve(tmpList.size());
    for(const QString& id: tmpList) {
        std::string name = id.trimmed().toStdString();
        d->scenes.push_back(name);
        d->scenesForName.emplace(std::move(name), d->scenesForName.size());
    }

    /* Create object name -> object id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->objects.reserve(tmpList.size());
    d->objectsForName.reserve(tmpList.size());
    for(const QString& id: tmpList) {
        std::string name = id.trimmed().toStdString();
        d->objects.push_back(name);
        d->objectsForName.emplace(std::move(name), d->objectsForName.size());
    }

    /* Create camera name -> camera id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_cameras/camera/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->camerasForName.reserve(tmpList.size());
    for(const QString& id: tmpList) {
        d->camerasForName.emplace(id.trimmed().toStdString(), d->camerasForName.size());
    }

    /* Create light name -> light id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_lights/light/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->lightsForName.reserve(tmpList.size());
    for(const QString& id: tmpList) {
        d->lightsForName.emplace(id.trimmed().toStdString(), d->lightsForName.size());
    }

    /* Create mesh name -> mesh id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_geometries/geometry/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->meshes.reserve(tmpList.size());
    d->meshesForName.reserve(tmpList.size());
    for(const QString& id: tmpList) {
        std::string name = id.trimmed().toStdString();
        d->meshes.push_back(name);
        d->meshesForName.emplace(std::move(name), d->meshesForName.size());
    }

    /* Create material name -> material id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_materials/material/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->materials.reserve(tmpList.size());
    d->materialsForName.reserve(tmpList.size());
    for(const QString& id: tmpList) {
        std::string name = id.trimmed().toStdString();
        d->materials.push_back(name);
        d->materialsForName.emplace(std::move(name), d->materialsForName.size());
    }

    /* Create texture name -> texture id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_effects/effect/profile_COMMON/newparam/*[starts-with(name(), 'sampler')]/../@sid/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->textures.reserve(tmpList.size());
    d->materialsForName.reserve(tmpList.size());
    for(const QString& id: tmpList) {
        std::string name = id.trimmed().toStdString();
        d->textures.push_back(name);
        d->texturesForName.emplace(std::move(name), d->texturesForName.size());
    }

    /* Create image name -> image id map */
    query.setQuery(namespaceDeclaration + "/COLLADA/library_images/image/@id/string()");
    tmpList.clear();
    query.evaluateTo(&tmpList);
    d->images2D.reserve(tmpList.size());
    d->images2DForName.reserve(tmpList.size());
    for(const QString& id: tmpList) {
        std::string name = id.trimmed().toStdString();
        d->images2D.push_back(name);
        d->images2DForName.emplace(std::move(name), d->images2DForName.size());
    }
}

void ColladaImporter::doClose() {
    delete d;
    d = nullptr;
}

Int ColladaImporter::doDefaultScene() {
    QString tmp;

    /* Default scene */
    d->query.setQuery(namespaceDeclaration + "/COLLADA/scene/instance_visual_scene/@url/string()");
    d->query.evaluateTo(&tmp);

    auto it = d->scenesForName.find(tmp.trimmed().mid(1).toStdString());
    return it == d->scenesForName.end() ? -1 : it->second;
}

UnsignedInt ColladaImporter::doSceneCount() const { return d->scenes.size(); }

Int ColladaImporter::doSceneForName(const std::string& name) {
    auto it = d->scenesForName.find(name);
    return it == d->scenesForName.end() ? -1 : it->second;
}

std::string ColladaImporter::doSceneName(const UnsignedInt id) { return d->scenes[id]; }

std::optional<SceneData> ColladaImporter::doScene(const UnsignedInt id) {
    QStringList tmpList;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene[%0]/node/@id/string()").arg(id+1));
    d->query.evaluateTo(&tmpList);

    std::vector<UnsignedInt> children;
    children.reserve(tmpList.size());
    for(const QString& childId: tmpList) {
        const std::string childIdTrimmed = childId.trimmed().toStdString();
        const Int id = doObject3DForName(childIdTrimmed);
        if(id == -1) {
            Error() << "Trade::ColladaImporter::scene(): object" << '"' + childIdTrimmed + '"' << "was not found";
            return std::nullopt;
        }
        children.push_back(id);
    }

    return SceneData({}, std::move(children));
}

UnsignedInt ColladaImporter::doObject3DCount() const { return d->objects.size(); }

Int ColladaImporter::doObject3DForName(const std::string& name) {
    auto it = d->objectsForName.find(name);
    return it == d->objectsForName.end() ? -1 : it->second;
}

std::string ColladaImporter::doObject3DName(const UnsignedInt id) { return d->objects[id]; }

std::unique_ptr<ObjectData3D> ColladaImporter::doObject3D(const UnsignedInt id) {
    /* Referring to <node>s with numbers somehow doesn't work (i.e. it selects
       many extra elements), we need to refer to them by id attribute instead */
    const auto name = QString::fromStdString(doObject3DName(id));

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
        else CORRADE_ASSERT(0, ("Trade::ColladaImporter::object3D(): unknown translation " + type).toStdString(), nullptr);
    }

    /* Child object names */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/node/@id/string()").arg(name));
    tmpList.clear();
    d->query.evaluateTo(&tmpList);

    /* Child object IDs */
    std::vector<UnsignedInt> children;
    children.reserve(tmpList.size());
    for(const QString& childId: tmpList) {
        const std::string childIdTrimmed = childId.trimmed().toStdString();
        const Int id = doObject3DForName(childIdTrimmed);
        if(id == -1) {
            Error() << "Trade::ColladaImporter::object3D(): object" << '"' + childIdTrimmed + '"' << "was not found";
            return nullptr;
        }
        children.push_back(id);
    }

    /* Instance type */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/*[starts-with(name(), 'instance_')]/name()").arg(name));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    /* Camera instance */
    if(tmp == "instance_camera") {
        /** @todo use doCameraForName() */
        std::string cameraName = instanceName(name, "instance_camera");
        auto cameraId = d->camerasForName.find(cameraName);
        if(cameraId == d->camerasForName.end()) {
            Error() << "Trade::ColladaImporter::object3D(): camera" << '"'+cameraName+'"' << "was not found";
            return nullptr;
        }

        /** @todo C++14: std::make_unique() */
        return std::unique_ptr<ObjectData3D>(new ObjectData3D(std::move(children), transformation, ObjectInstanceType3D::Camera, cameraId->second));

    /* Light instance */
    } else if(tmp == "instance_light") {
        /** @todo use doLightForName() */
        std::string lightName = instanceName(name, "instance_light");
        auto lightId = d->lightsForName.find(lightName);
        if(lightId == d->lightsForName.end()) {
            Error() << "Trade::ColladaImporter::object3D(): light" << '"'+lightName+'"' << "was not found";
            return nullptr;
        }

        /** @todo C++14: std::make_unique() */
        return std::unique_ptr<ObjectData3D>(new ObjectData3D(std::move(children), transformation, ObjectInstanceType3D::Light, lightId->second));

    /* Mesh instance */
    } else if(tmp == "instance_geometry") {
        std::string meshName = instanceName(name, "instance_geometry");
        const Int meshId = doMesh3DForName(meshName);
        if(meshId == -1) {
            Error() << "Trade::ColladaImporter::object3D(): mesh" << '"'+meshName+'"' << "was not found";
            return nullptr;
        }

        d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/instance_geometry/bind_material/technique_common/count(instance_material)").arg(name));
        d->query.evaluateTo(&tmp);

        const Int materialCount = tmp.toInt();
        Int materialId = -1;
        if(materialCount > 1) {
            Error() << "Trade::ColladaImporter::object3D(): multiple materials per object are not supported";
            return nullptr;
        } else if(materialCount != 0) {
            d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/instance_geometry/bind_material/technique_common/instance_material/@target/string()").arg(name));
            d->query.evaluateTo(&tmp);
            std::string materialName = tmp.trimmed().mid(1).toStdString();

            /* If the mesh doesn't have bound material, add default one, else find
               its ID */
            /** @todo Solution for unknown materials etc.: -1 ? */
            if(!materialName.empty()) {
                materialId = doMaterialForName(materialName);
                if(materialId == -1) {
                    Error() << "Trade::ColladaImporter::object3D(): material" << '"'+materialName+'"' << "was not found";
                    return nullptr;
                }
            }
        }

        /** @todo C++14: std::make_unique() */
        return std::unique_ptr<ObjectData3D>(new MeshObjectData3D(std::move(children), transformation, meshId, materialId));

    /* Blender group instance */
    } else if(tmp.isEmpty())
        return std::unique_ptr<ObjectData3D>(new ObjectData3D(std::move(children), transformation));

    /* Something else */
    Error() << "Trade::ColladaImporter::object3D():" << '"'+tmp.toStdString()+'"' << "instance type not supported";
    return nullptr;
}

UnsignedInt ColladaImporter::doMesh3DCount() const { return d->meshes.size(); }

Int ColladaImporter::doMesh3DForName(const std::string& name) {
    auto it = d->meshesForName.find(name);
    return it == d->meshesForName.end() ? -1 : it->second;
}

std::string ColladaImporter::doMesh3DName(const UnsignedInt id) { return d->meshes[id]; }

std::optional<MeshData3D> ColladaImporter::doMesh3D(const UnsignedInt id) {
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
            Error() << "Trade::ColladaImporter::mesh3D():" << count << "vertices per face not supported";
            return std::nullopt;
        }

        vertexCount += count;
    }

    /* Get input count per vertex */
    d->query.setQuery((namespaceDeclaration + "count(/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input)").arg(id+1));
    d->query.evaluateTo(&tmp);
    UnsignedInt stride = Implementation::ColladaType<UnsignedInt>::fromString(tmp);

    /* Get mesh index arrays */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/p/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    std::vector<UnsignedInt> interleavedIndexArrays = Implementation::Utility::parseArray<UnsignedInt>(tmp, vertexCount*stride);

    /* Combine index arrays */
    std::vector<UnsignedInt> combinedIndices;
    std::tie(combinedIndices, interleavedIndexArrays) = MeshTools::combineIndexArrays(interleavedIndexArrays, stride);

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
    std::vector<Vector3> vertices(interleavedIndexArrays.size()/stride);
    for(UnsignedInt i = 0; i != vertices.size(); ++i)
        vertices[i] = originalVertices[interleavedIndexArrays[i*stride + vertexOffset]];

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
            normals.push_back(buildAttributeArray<Vector3>(id, "NORMAL", normals.size(), stride, interleavedIndexArrays));

        /* 2D texture coords */
        else if(attribute == "TEXCOORD")
            textureCoords2D.push_back(buildAttributeArray<Vector2>(id, "TEXCOORD", textureCoords2D.size(), stride, interleavedIndexArrays));

        /* Something other */
        else Warning() << "Trade::ColladaImporter::mesh3D():" << '"' + attribute.toStdString() + '"' << "input semantic not supported";
    }

    return MeshData3D{MeshPrimitive::Triangles, std::move(indices), {std::move(vertices)}, std::move(normals), std::move(textureCoords2D), {}, nullptr};
}

UnsignedInt ColladaImporter::doMaterialCount() const { return d->materials.size(); }

Int ColladaImporter::doMaterialForName(const std::string& name) {
    auto it = d->materialsForName.find(name);
    return it == d->materialsForName.end() ? -1 : it->second;
}

std::string ColladaImporter::doMaterialName(const UnsignedInt id) {
    return d->materials[id];
}

std::unique_ptr<AbstractMaterialData> ColladaImporter::doMaterial(const UnsignedInt id) {
    /* Get effect ID */
    QString effect;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_materials/material[%0]/instance_effect/@url/string()").arg(id+1));
    d->query.evaluateTo(&effect);
    effect = effect.mid(1).trimmed();

    /* Find out which profile it is */
    QString tmp;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/*[starts-with(name(), 'profile_')]/name()").arg(effect));
    d->query.evaluateTo(&tmp);

    /** @todo Support other profiles */

    if(tmp.trimmed() != "profile_COMMON") {
        Error() << "Trade::ColladaImporter::material():" << ('"'+tmp.trimmed()+'"').toStdString() << "effect profile not supported";
        return nullptr;
    }

    /* Get shader type */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/*/name()").arg(effect));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    /** @todo Other (blinn, goraund) profiles */
    if(tmp != "phong") {
        Error() << "Trade::ColladaImporter::material():" << ('"'+tmp+'"').toStdString() << "shader not supported";
        return nullptr;
    }

    /* Shininess */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/shininess/float/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    const Float shininess = Implementation::ColladaType<Float>::fromString(tmp);

    /* Decide about what is textured in the material */
    PhongMaterialData::Flags flags;

    /* Ambient texture */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/ambient/texture/@texture/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();
    UnsignedInt ambientTexture = 0;
    if(!tmp.isEmpty()) {
        auto it = d->texturesForName.find(tmp.toStdString());
        if(it == d->texturesForName.end()) {
            Error() << "Trade::ColladaImporter::material(): ambient texture" << tmp.toStdString() << "not found";
            return nullptr;
        }

        flags |= PhongMaterialData::Flag::AmbientTexture;
        ambientTexture = it->second;
    }

    /* Diffuse texture */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/diffuse/texture/@texture/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();
    UnsignedInt diffuseTexture = 0;
    if(!tmp.isEmpty()) {
        auto it = d->texturesForName.find(tmp.toStdString());
        if(it == d->texturesForName.end()) {
            Error() << "Trade::ColladaImporter::material(): diffuse texture" << tmp.toStdString() << "not found";
            return nullptr;
        }

        flags |= PhongMaterialData::Flag::DiffuseTexture;
        diffuseTexture = it->second;
    }

    /* Specular texture */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/specular/texture/@texture/string()").arg(effect));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();
    UnsignedInt specularTexture = 0;
    if(!tmp.isEmpty()) {
        auto it = d->texturesForName.find(tmp.toStdString());
        if(it == d->texturesForName.end()) {
            Error() << "Trade::ColladaImporter::material(): specular texture" << tmp.toStdString() << "not found";
            return nullptr;
        }

        flags |= PhongMaterialData::Flag::SpecularTexture;
        specularTexture = it->second;
    }

    auto material = new PhongMaterialData(flags, shininess);

    /* Ambient texture or color, if not textured */
    if(flags & PhongMaterialData::Flag::AmbientTexture)
        material->ambientTexture() = ambientTexture;
    else {
        d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/ambient/color/string()").arg(effect));
        d->query.evaluateTo(&tmp);
        material->ambientColor() = Implementation::Utility::parseVector<Vector3>(tmp);
    }

    /* Diffuse texture or color, if not textured */
    if(flags & PhongMaterialData::Flag::DiffuseTexture)
        material->diffuseTexture() = diffuseTexture;
    else {
        d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/diffuse/color/string()").arg(effect));
        d->query.evaluateTo(&tmp);
        material->diffuseColor() = Implementation::Utility::parseVector<Vector3>(tmp);
    }

    /* Specular color */
    if(flags & PhongMaterialData::Flag::SpecularTexture)
        material->specularTexture() = specularTexture;
    else {
        d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect[@id='%0']/profile_COMMON/technique/phong/specular/color/string()").arg(effect));
        d->query.evaluateTo(&tmp);
        material->specularColor() = Implementation::Utility::parseVector<Vector3>(tmp);
    }

    /** @todo Emission, IOR */

    /** @todo C++14: std::make_unique */
    return std::unique_ptr<AbstractMaterialData>(material);
}

UnsignedInt ColladaImporter::doTextureCount() const { return d->textures.size(); }

Int ColladaImporter::doTextureForName(const std::string& name) {
    auto it = d->texturesForName.find(name);
    return it == d->texturesForName.end() ? -1 : it->second;
}

std::string ColladaImporter::doTextureName(const UnsignedInt id) {
    return d->textures[id];
}

namespace {

Sampler::Wrapping wrappingFromString(const QString& string) {
    /* Treat NONE and element not present as default */
    if(string.isEmpty() || string == "WRAP" || string == "NONE") return Sampler::Wrapping::Repeat;
    if(string == "MIRROR") return Sampler::Wrapping::MirroredRepeat;
    if(string == "CLAMP") return Sampler::Wrapping::ClampToEdge;
    if(string == "BORDER") return Sampler::Wrapping::ClampToBorder;

    Error() << "Trade::ColladaImporter::texture(): unknown texture wrapping" << string.toStdString();
    return Sampler::Wrapping(-1);
}

Sampler::Filter filterFromString(const QString& string) {
    /* Treat NONE and element not present as default */
    if(string.isEmpty() || string == "NEAREST" || string == "NONE") return Sampler::Filter::Nearest;
    if(string == "LINEAR") return Sampler::Filter::Linear;

    Error() << "Trade::ColladaImporter::texture(): unknown texture filter" << string.toStdString();
    return Sampler::Filter(-1);
}

Sampler::Mipmap mipmapFromString(const QString& string) {
    /* Treat element not present as default */
    if(string.isEmpty() || string == "NONE") return Sampler::Mipmap::Base;
    if(string == "NEAREST") return Sampler::Mipmap::Nearest;
    if(string == "LINEAR") return Sampler::Mipmap::Linear;

    Error() << "Trade::ColladaImporter::texture(): unknown texture mipmap filter" << string.toStdString();
    return Sampler::Mipmap(-1);
}

}

std::optional<TextureData> ColladaImporter::doTexture(const UnsignedInt id) {
    const auto name = QString::fromStdString(d->textures[id]);

    /* Texture type */
    QString tmp;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect/profile_COMMON/newparam[@sid='%0']/*[starts-with(name(), 'sampler')]/name()").arg(name));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    TextureData::Type type;
    if(tmp == "sampler1D")          type = TextureData::Type::Texture1D;
    else if(tmp == "sampler2D")     type = TextureData::Type::Texture2D;
    else if(tmp == "sampler3D")     type = TextureData::Type::Texture3D;
    else if(tmp == "samplerCUBE")   type = TextureData::Type::Cube;
    else {
        Error() << "Trade::ColladaImporter::texture(): unsupported sampler type" << tmp.toStdString();
        return std::nullopt;
    }

    /* Texture image */
    /** @todo Verify that surface type is the same as sampler type */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect/profile_COMMON/newparam[surface][@sid=/COLLADA/library_effects/effect/profile_COMMON/newparam[@sid='%0']/*[starts-with(name(), 'sampler')]/source/string()]/surface/init_from/string()").arg(name));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    auto it = d->images2DForName.find(tmp.toStdString());
    if(it == d->images2DForName.end()) {
        Error() << "Trade::ColladaImporter::texture(): image" << tmp.toStdString() << "not found";
        return std::nullopt;
    }
    const UnsignedInt image = it->second;

    /* Texture sampler wrapping */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect/profile_COMMON/newparam[@sid='%0']/*[starts-with(name(), 'sampler')]/wrap_s/string()").arg(name));
    d->query.evaluateTo(&tmp);
    const Sampler::Wrapping wrappingX = wrappingFromString(tmp.trimmed());
    if(wrappingX == Sampler::Wrapping(-1)) return std::nullopt;

    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect/profile_COMMON/newparam[@sid='%0']/*[starts-with(name(), 'sampler')]/wrap_t/string()").arg(name));
    d->query.evaluateTo(&tmp);
    const Sampler::Wrapping wrappingY = wrappingFromString(tmp.trimmed());
    if(wrappingY == Sampler::Wrapping(-1)) return std::nullopt;

    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect/profile_COMMON/newparam[@sid='%0']/*[starts-with(name(), 'sampler')]/wrap_p/string()").arg(name));
    d->query.evaluateTo(&tmp);
    const Sampler::Wrapping wrappingZ = wrappingFromString(tmp.trimmed());
    if(wrappingZ == Sampler::Wrapping(-1)) return std::nullopt;

    /* Texture minification filter */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect/profile_COMMON/newparam[@sid='%0']/*[starts-with(name(), 'sampler')]/minfilter/string()").arg(name));
    d->query.evaluateTo(&tmp);
    const Sampler::Filter minificationFilter = filterFromString(tmp.trimmed());
    if(minificationFilter == Sampler::Filter(-1)) return std::nullopt;

    /* Texture magnification filter */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect/profile_COMMON/newparam[@sid='%0']/*[starts-with(name(), 'sampler')]/magfilter/string()").arg(name));
    d->query.evaluateTo(&tmp);
    const Sampler::Filter magnificationFilter = filterFromString(tmp.trimmed());
    if(magnificationFilter == Sampler::Filter(-1)) return std::nullopt;

    /* Texture mipmap filter */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_effects/effect/profile_COMMON/newparam[@sid='%0']/*[starts-with(name(), 'sampler')]/mipfilter/string()").arg(name));
    d->query.evaluateTo(&tmp);
    const Sampler::Mipmap mipmapFilter = mipmapFromString(tmp.trimmed());
    if(mipmapFilter == Sampler::Mipmap(-1)) return std::nullopt;

    return TextureData(type, minificationFilter, magnificationFilter, mipmapFilter, {wrappingX, wrappingY, wrappingZ}, image);
}

UnsignedInt ColladaImporter::doImage2DCount() const { return d->images2D.size(); }

Int ColladaImporter::doImage2DForName(const std::string& name) {
    auto it = d->images2DForName.find(name);
    return it == d->images2DForName.end() ? -1 : it->second;
}

std::string ColladaImporter::doImage2DName(const UnsignedInt id) {
    return d->images2D[id];
}

std::optional<ImageData2D> ColladaImporter::doImage2D(const UnsignedInt id) {
    /* Image filename */
    QString tmp;
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_images/image[%0]/init_from/string()").arg(id+1));
    d->query.evaluateTo(&tmp);
    tmp = tmp.trimmed();

    CORRADE_ASSERT(manager(), "Trade::ColladaImporter::image2D(): the plugin must be instantiated with access to plugin manager in order to open image files", std::nullopt);

    AnyImageImporter imageImporter{static_cast<PluginManager::Manager<AbstractImporter>&>(*manager())};
    if(!imageImporter.openFile(Utility::Directory::join(Utility::Directory::path(d->filename), tmp.toStdString())))
        return std::nullopt;

    return imageImporter.image2D(0);
}

UnsignedInt ColladaImporter::attributeOffset(UnsignedInt meshId, const QString& attribute, UnsignedInt id) {
    QString tmp;

    /* Get attribute offset in indices */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input[@semantic='%1'][%2]/@offset/string()")
        .arg(meshId+1).arg(attribute).arg(id+1));
    d->query.evaluateTo(&tmp);
    return Implementation::ColladaType<UnsignedInt>::fromString(tmp);
}

template<class T> std::vector<T> ColladaImporter::parseSource(const QString& id) {
    std::vector<T> output;
    QString tmp;

    /* Count of items */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@count/string()").arg(id));
    d->query.evaluateTo(&tmp);
    UnsignedInt count = Implementation::ColladaType<UnsignedInt>::fromString(tmp);

    /* Size of each item */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@stride/string()").arg(id));
    d->query.evaluateTo(&tmp);
    UnsignedInt size = Implementation::ColladaType<UnsignedInt>::fromString(tmp);

    /* Data source */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source[@id='%0']/technique_common/accessor/@source/string()").arg(id));
    d->query.evaluateTo(&tmp);
    QString source = tmp.mid(1).trimmed();

    /* Verify total count */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source/float_array[@id='%0']/@count/string()").arg(source));
    d->query.evaluateTo(&tmp);
    if(Implementation::ColladaType<UnsignedInt>::fromString(tmp) != count*size) {
        Error() << "Trade::ColladaImporter::mesh3D(): wrong total count in source" << ('"'+id+'"').toStdString();
        return output;
    }

    /** @todo Assert right order of coordinates and type */

    /* Items */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry/mesh/source/float_array[@id='%0']/string()").arg(source));
    d->query.evaluateTo(&tmp);

    output.reserve(count);
    Int from = 0;
    for(std::size_t i = 0; i != count; ++i)
        output.push_back(Implementation::Utility::parseVector<T>(tmp, &from, size));

    return output;
}

/* Doxygen got confused by the templates and thinks this is undocumented */
#ifndef DOXYGEN_GENERATING_OUTPUT
template<class T> std::vector<T> ColladaImporter::buildAttributeArray(UnsignedInt meshId, const QString& attribute, UnsignedInt id, UnsignedInt stride, const std::vector<UnsignedInt>& interleavedIndexArrays) {
    QString tmp;

    /* Original attribute array */
    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_geometries/geometry[%0]/mesh/polylist/input[@semantic='%1'][%2]/@source/string()")
        .arg(meshId+1).arg(attribute).arg(id+1));
    d->query.evaluateTo(&tmp);
    std::vector<T> originalArray = parseSource<T>(tmp.mid(1).trimmed());

    /* Attribute offset in original index array */
    UnsignedInt offset = attributeOffset(meshId, attribute, id);

    /* Build resulting array */
    std::vector<T> array(interleavedIndexArrays.size()/stride);
    for(UnsignedInt i = 0; i != array.size(); ++i)
        array[i] = originalArray[interleavedIndexArrays[i*stride+offset]];

    return array;
}
#endif

std::string ColladaImporter::instanceName(const QString& name, const QString& instanceTag) {
    QString tmp;

    d->query.setQuery((namespaceDeclaration + "/COLLADA/library_visual_scenes/visual_scene//node[@id='%0']/%1/@url/string()").arg(name).arg(instanceTag));
    d->query.evaluateTo(&tmp);
    return tmp.trimmed().mid(1).toStdString();
}

}}
