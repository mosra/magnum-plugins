/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2018 Jonathan Hale <squareys@googlemail.com>

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

#include "OpenGexImporter.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/LightData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/MeshObjectData3D.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/TextureData.h>

#include "Magnum/OpenDdl/Document.h"
#include "Magnum/OpenDdl/Property.h"
#include "Magnum/OpenDdl/Structure.h"
#include "MagnumPlugins/AnyImageImporter/AnyImageImporter.h"

#include "openGexSpec.hpp"

namespace Magnum { namespace Trade {

using namespace Magnum::Math::Literals;

struct OpenGexImporter::Document {
    /* Clang-CL otherwise complains that Document has no implicit constructor */
    OpenDdl::Document document{};

    /* Default metrics */
    Float distanceMultiplier = 1.0f;
    Rad angleMultiplier = 1.0_radf;
    Float timeMultiplier = 1.0f;
    bool yUp = false;

    Containers::Optional<std::string> filePath;

    std::vector<OpenDdl::Structure> nodes,
        cameras,
        lights,
        meshes,
        materials,
        textures;

    std::unordered_map<std::string, Int> nodesForName,
        materialsForName;

    std::unordered_map<std::string, UnsignedInt> imagesForName;
    std::vector<std::string> images;

    UnsignedInt imageImporterId = ~UnsignedInt{};
    Containers::Optional<AnyImageImporter> imageImporter;
};

namespace {

inline Int findStructureId(const std::vector<OpenDdl::Structure>& structures, OpenDdl::Structure structure) {
    auto found = std::find(structures.begin(), structures.end(), structure);
    return found == structures.end() ? -1 : found - structures.begin();
}

UnsignedInt structureId(const std::vector<OpenDdl::Structure>& structures, OpenDdl::Structure structure) {
    const Int id = findStructureId(structures, structure);
    CORRADE_INTERNAL_ASSERT(id != -1);
    return id;
}

}

OpenGexImporter::OpenGexImporter() = default;

OpenGexImporter::OpenGexImporter(PluginManager::Manager<AbstractImporter>& manager): AbstractImporter(manager) {}

OpenGexImporter::OpenGexImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter(manager, plugin) {}

OpenGexImporter::~OpenGexImporter() = default;

ImporterFeatures OpenGexImporter::doFeatures() const { return ImporterFeature::OpenData|ImporterFeature::FileCallback; }

bool OpenGexImporter::doIsOpened() const { return !!_d; }

namespace {

void gatherNodes(OpenDdl::Structure node, std::vector<OpenDdl::Structure>& nodes, std::unordered_map<std::string, Int>& nodesForName) {
    if(const auto name = node.findFirstChildOf(OpenGex::Name))
        nodesForName.emplace(name->firstChild().as<std::string>(), nodes.size());
    nodes.push_back(node);

    /* Recurse into children */
    for(const OpenDdl::Structure childNode: node.childrenOf(OpenGex::Node, OpenGex::BoneNode, OpenGex::GeometryNode, OpenGex::CameraNode, OpenGex::LightNode))
        gatherNodes(childNode, nodes, nodesForName);
}

}

void OpenGexImporter::doOpenData(const Containers::ArrayView<const char> data) {
    Containers::Pointer<Document> d{Containers::InPlaceInit};

    /* Parse the document */
    if(!d->document.parse(data, OpenGex::structures, OpenGex::properties)) return;

    /* Validate the document */
    if(!d->document.validate(OpenGex::rootStructures, OpenGex::structureInfo)) return;

    /* Metrics */
    for(const OpenDdl::Structure metric: d->document.childrenOf(OpenGex::Metric)) {
        auto&& key = metric.propertyOf(OpenGex::key).as<std::string>();
        const OpenDdl::Structure value = metric.firstChild();

        /* Distance multiplier */
        if(key == "distance") {
            if(value.type() != OpenDdl::Type::Float) {
                Error() << "Trade::OpenGexImporter::openData(): invalid value for distance metric";
                return;
            }

            d->distanceMultiplier = value.as<Float>();

        /* Angle multiplier */
        } else if(key == "angle") {
            if(value.type() != OpenDdl::Type::Float) {
                Error() << "Trade::OpenGexImporter::openData(): invalid value for angle metric";
                return;
            }

            d->angleMultiplier = Rad{value.as<Float>()};

        /* Time multiplier */
        } else if(key == "time") {
            if(value.type() != OpenDdl::Type::Float) {
                Error() << "Trade::OpenGexImporter::openData(): invalid value for time metric";
                return;
            }

            d->timeMultiplier = value.as<Float>();

        /* Up axis */
        } else if(key == "up") {
            if(value.type() != OpenDdl::Type::String || (value.as<std::string>() != "y" && value.as<std::string>() != "z")) {
                Error() << "Trade::OpenGexImporter::openData(): invalid value for up metric";
                return;
            }

            d->yUp = value.as<std::string>() == "y";
        }
    }

    /* Common code for light and material textures */
    auto gatherTexture = [&d](const OpenDdl::Structure& texture) {
        d->textures.push_back(texture);

        /* Add the filename to the list, if not already */
        const std::string filename = texture.firstChildOf(OpenDdl::Type::String).as<std::string>();
        if(d->imagesForName.emplace(filename, d->images.size()).second)
            d->images.emplace_back(filename);
    };

    /* Gather all cameras */
    for(const OpenDdl::Structure camera: d->document.childrenOf(OpenGex::CameraObject))
        d->cameras.push_back(camera);

    /* Gather all meshes */
    /** @todo Support for LOD */
    for(const OpenDdl::Structure geometry: d->document.childrenOf(OpenGex::GeometryObject))
        d->meshes.push_back(geometry);

    /* Gather all lights and light textures */
    for(const OpenDdl::Structure light: d->document.childrenOf(OpenGex::LightObject)) {
        d->lights.push_back(light);

        for(const OpenDdl::Structure texture: light.childrenOf(OpenGex::Texture))
            gatherTexture(texture);
    }

    /* Gather all materials */
    {
        for(const OpenDdl::Structure material: d->document.childrenOf(OpenGex::Material)) {
            if(const auto name = material.findFirstChildOf(OpenGex::Name))
                d->materialsForName.emplace(name->firstChild().as<std::string>(), d->materials.size());
            d->materials.push_back(material);

            /* Gather all material textures */
            for(const OpenDdl::Structure texture: material.childrenOf(OpenGex::Texture))
                gatherTexture(texture);
        }
    }

    /* Gather the scene nodes */
    for(const OpenDdl::Structure node: d->document.childrenOf(OpenGex::Node, OpenGex::BoneNode, OpenGex::GeometryNode, OpenGex::CameraNode, OpenGex::LightNode))
        gatherNodes(node, d->nodes, d->nodesForName);

    /* Everything okay, save the instance */
    _d = std::move(d);
}

void OpenGexImporter::doOpenFile(const std::string& filename) {
    /* Make doOpenData() do the thing */
    AbstractImporter::doOpenFile(filename);

    /* If succeeded, save file path for later */
    if(_d) _d->filePath = Utility::Directory::path(filename);
}

void OpenGexImporter::doClose() { _d = nullptr; }

Int OpenGexImporter::doDefaultScene() const { return 0; }

UnsignedInt OpenGexImporter::doSceneCount() const { return 1; }

Containers::Optional<SceneData> OpenGexImporter::doScene(UnsignedInt) {
    /* Just gather all nodes that have no parent */
    Int i = 0;
    std::vector<UnsignedInt> children;
    for(const OpenDdl::Structure node: _d->nodes) {
        if(!node.parent()) children.push_back(i);
        ++i;
    }

    return SceneData{{}, children};
}

UnsignedInt OpenGexImporter::doCameraCount() const {
    return _d->cameras.size();
}

Containers::Optional<CameraData> OpenGexImporter::doCamera(UnsignedInt id) {
    const OpenDdl::Structure& camera = _d->cameras[id];

    Rad fov = 35.0_degf;
    Float near = 0.01f;
    Float far = 100.0f;
    for(const OpenDdl::Structure param: camera.childrenOf(OpenGex::Param)) {
        const OpenDdl::Structure data = param.firstChild();

        const auto attrib = param.propertyOf(OpenGex::attrib);
        if(attrib.as<std::string>() == "fov")
            fov = data.as<Float>()*_d->angleMultiplier;
        else if(attrib.as<std::string>() == "near")
            near = data.as<Float>()*_d->distanceMultiplier;
        else if(attrib.as<std::string>() == "far")
            far = data.as<Float>()*_d->distanceMultiplier;
        else {
            Error() << "Trade::OpenGexImporter::camera(): invalid parameter";
            return Containers::NullOpt;
        }
    }

    return CameraData{CameraType::Perspective3D, fov, 1.0f, near, far, &camera};
}

UnsignedInt OpenGexImporter::doObject3DCount() const {
    return _d->nodes.size();
}

Int OpenGexImporter::doObject3DForName(const std::string& name) {
    const auto found = _d->nodesForName.find(name);
    return found == _d->nodesForName.end() ? -1 : found->second;
}

std::string OpenGexImporter::doObject3DName(const UnsignedInt id) {
    const auto name = _d->nodes[id].findFirstChildOf(OpenGex::Name);
    return name ? name->firstChild().as<std::string>() : "";
}

namespace {
    inline Matrix4 fixMatrixZUp(const Matrix4& m) {
        /* Rotate back to Z up, apply transformation and then rotate to final Y
           up */
        return Matrix4{{1.0f, 0.0f,  0.0f, 0.0f},
                       {0.0f, 0.0f, -1.0f, 0.0f},
                       {0.0f, 1.0f,  0.0f, 0.0f},
                       {0.0f, 0.0f,  0.0f, 1.0f}}*m*
               Matrix4{{1.0f,  0.0f, 0.0f, 0.0f},
                       {0.0f,  0.0f, 1.0f, 0.0f},
                       {0.0f, -1.0f, 0.0f, 0.0f},
                       {0.0f,  0.0f, 0.0f, 1.0f}};
    }

    inline Vector3 fixVectorZUp(Vector3 vec) { return {vec.x(), vec.z(), -vec.y()}; }

    inline Vector3 fixScalingZUp(Vector3 vec) { return {vec.x(), vec.z(), vec.y()}; }

    inline Matrix4 fixMatrixTranslation(Matrix4 m, Float distanceMultiplier) {
        m[3].xyz() *= distanceMultiplier;
        return m;
    }
}

Containers::Pointer<ObjectData3D> OpenGexImporter::doObject3D(const UnsignedInt id) {
    const OpenDdl::Structure& node = _d->nodes[id];

    /* Compute total transformation */
    Matrix4 transformation;
    for(const OpenDdl::Structure t: node.childrenOf(OpenGex::Transform, OpenGex::Translation, OpenGex::Rotation, OpenGex::Scale)) {
        /** @todo support object transformations */
        if(const auto object = t.findPropertyOf(OpenGex::object)) if(object->as<bool>()) {
            Error() << "Trade::OpenGexImporter::object3D(): unsupported object-only transformation";
            return nullptr;
        }

        Matrix4 matrix;

        /* 4x4 matrix */
        if(t.identifier() == OpenGex::Transform) {
            const OpenDdl::Structure data = t.firstChild();
            if(data.subArraySize() != 16) {
                Error() << "Trade::OpenGexImporter::object3D(): invalid transformation";
                return nullptr;
            }

            const Matrix4 m = fixMatrixTranslation(Matrix4::from(data.asArray<Float>()), _d->distanceMultiplier);
            matrix = _d->yUp ? m : fixMatrixZUp(m);

        /* Translation */
        } else if(t.identifier() == OpenGex::Translation) {
            const OpenDdl::Structure data = t.firstChild();

            /* "xyz" is the default if no kind property is specified */
            Vector3 v;
            const auto kind = t.findPropertyOf(OpenGex::kind);
            if((!kind || kind->as<std::string>() == "xyz") && data.subArraySize() == 3)
                v = Vector3::from(data.asArray<Float>())*_d->distanceMultiplier;
            else if(kind && kind->as<std::string>() == "x" && data.subArraySize() == 0)
                v = Vector3::xAxis(data.as<Float>()*_d->distanceMultiplier);
            else if(kind && kind->as<std::string>() == "y" && data.subArraySize() == 0)
                v = Vector3::yAxis(data.as<Float>()*_d->distanceMultiplier);
            else if(kind && kind->as<std::string>() == "z" && data.subArraySize() == 0)
                v = Vector3::zAxis(data.as<Float>()*_d->distanceMultiplier);
            else {
                Error() << "Trade::OpenGexImporter::object3D(): invalid translation";
                return nullptr;
            }

            matrix = Matrix4::translation((_d->yUp ? v : fixVectorZUp(v)));

        /* Rotation */
        } else if(t.identifier() == OpenGex::Rotation) {
            const OpenDdl::Structure data = t.firstChild();

            /* "axis" is the default if no kind property is specified */
            Matrix4 m;
            const auto kind = t.findPropertyOf(OpenGex::kind);
            if((!kind || kind->as<std::string>() == "axis") && data.subArraySize() == 4) {
                const auto angle = data.asArray<Float>()[0]*_d->angleMultiplier;
                const auto axis = Vector3::from(data.asArray<Float>() + 1).normalized();
                m = Matrix4::rotation(angle, axis);
            } else if(kind && kind->as<std::string>() == "x" && data.subArraySize() == 0) {
                const auto angle = data.as<Float>()*_d->angleMultiplier;
                m = Matrix4::rotationX(angle);
            } else if(kind && kind->as<std::string>() == "y" && data.subArraySize() == 0) {
                const auto angle = data.as<Float>()*_d->angleMultiplier;
                m = Matrix4::rotationY(angle);
            } else if(kind && kind->as<std::string>() == "z" && data.subArraySize() == 0) {
                const auto angle = data.as<Float>()*_d->angleMultiplier;
                m = Matrix4::rotationZ(angle);
            } else if(kind && kind->as<std::string>() == "quaternion" && data.subArraySize() == 4) {
                const auto vector = Vector3::from(data.asArray<Float>());
                const auto scalar = data.asArray<Float>()[3];
                m = Matrix4::from(Quaternion(vector, scalar).normalized().toMatrix(), {});
            } else {
                Error() << "Trade::OpenGexImporter::object3D(): invalid rotation";
                return nullptr;
            }

            matrix = _d->yUp ? m : fixMatrixZUp(m);

        /* Scaling */
        } else if(t.identifier() == OpenGex::Scale) {
            const OpenDdl::Structure data = t.firstChild();

            /* "xyz" is the default if no kind property is specified */
            Vector3 v;
            const auto kind = t.findPropertyOf(OpenGex::kind);
            if((!kind || kind->as<std::string>() == "xyz") && data.subArraySize() == 3)
                v = Vector3::from(data.asArray<Float>());
            else if(kind && kind->as<std::string>() == "x" && data.subArraySize() == 0)
                v = Vector3::xScale(data.as<Float>());
            else if(kind && kind->as<std::string>() == "y" && data.subArraySize() == 0)
                v = Vector3::yScale(data.as<Float>());
            else if(kind && kind->as<std::string>() == "z" && data.subArraySize() == 0)
                v = Vector3::zScale(data.as<Float>());
            else {
                Error() << "Trade::OpenGexImporter::object3D(): invalid scaling";
                return nullptr;
            }

            matrix = Matrix4::scaling(_d->yUp ? v : fixScalingZUp(v));

        } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

        transformation = transformation*matrix;
    }

    /* Child node IDs */
    std::vector<UnsignedInt> children;
    for(const OpenDdl::Structure childNode: node.childrenOf(OpenGex::Node, OpenGex::BoneNode, OpenGex::GeometryNode, OpenGex::CameraNode, OpenGex::LightNode))
        children.push_back(structureId(_d->nodes, childNode));

    /* Mesh object */
    if(node.identifier() == OpenGex::GeometryNode) {
        /* Mesh ID */
        const auto mesh = node.firstChildOf(OpenGex::ObjectRef).firstChildOf(OpenDdl::Type::Reference).asReference();
        if(!mesh) {
            Error() << "Trade::OpenGexImporter::object3D(): null geometry reference";
            return nullptr;
        }
        const UnsignedInt meshId = structureId(_d->meshes, *mesh);

        /* Material ID, if present */
        /** @todo support more materials per mesh */
        Int materialId = -1;
        if(const auto materialRef = node.findFirstChildOf(OpenGex::MaterialRef))
            if(const auto material = materialRef->firstChildOf(OpenDdl::Type::Reference).asReference())
                materialId = structureId(_d->materials, *material);

        return Containers::pointer(new MeshObjectData3D{children, transformation, meshId, materialId, -1, &node});

    /* Camera object */
    } else if(node.identifier() == OpenGex::CameraNode) {
        /* Camera ID */
        const auto camera = node.firstChildOf(OpenGex::ObjectRef).firstChildOf(OpenDdl::Type::Reference).asReference();
        if(!camera) {
            Error() << "Trade::OpenGexImporter::object3D(): null camera reference";
            return nullptr;
        }
        const UnsignedInt cameraId = structureId(_d->cameras, *camera);

        return Containers::pointer(new ObjectData3D{children, transformation, ObjectInstanceType3D::Camera, cameraId, &node});

    /* Light object */
    } else if(node.identifier() == OpenGex::LightNode) {
        /* Light ID */
        const auto light = node.firstChildOf(OpenGex::ObjectRef).firstChildOf(OpenDdl::Type::Reference).asReference();
        if(!light) {
            Error() << "Trade::OpenGexImporter::object3D(): null light reference";
            return nullptr;
        }
        const UnsignedInt lightId = structureId(_d->lights, *light);

        return Containers::pointer(new ObjectData3D{children, transformation, ObjectInstanceType3D::Light, lightId, &node});
    }

    /* Bone or empty object otherwise */
    /** @todo support for bone nodes */
    return Containers::pointer(new ObjectData3D{children, transformation, &node});
}

namespace {

template<class Result, std::size_t originalSize> Result extractColorData2(const OpenDdl::Structure floatArray) {
    return Result::pad(*reinterpret_cast<const Math::Vector<originalSize, Float>*>(floatArray.asArray<Float>().data()), 1.0f);
}

template<class Result> Result extractColorData(const OpenDdl::Structure floatArray) {
    switch(floatArray.subArraySize()) {
        case 3: return extractColorData2<Result, 3>(floatArray);
        case 4: return extractColorData2<Result, 4>(floatArray);
    }

    CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

}

UnsignedInt OpenGexImporter::doLightCount() const {
    return _d->lights.size();
}

Containers::Optional<LightData> OpenGexImporter::doLight(UnsignedInt id) {
    const OpenDdl::Structure& light = _d->lights[id];

    Color3 lightColor{1.0f};
    Float lightIntensity{1.0f};
    LightData::Type lightType;
    const auto type = light.propertyOf(OpenGex::type);
    if(type.as<std::string>() == "infinite")
        lightType = LightData::Type::Directional;
    else if(type.as<std::string>() == "point")
        lightType = LightData::Type::Point;
    else if(type.as<std::string>() == "spot")
        lightType = LightData::Type::Spot;
    else {
        Error() << "Trade::OpenGexImporter::light(): invalid type";
        return Containers::NullOpt;
    }

    for(const OpenDdl::Structure color: light.childrenOf(OpenGex::Color)) {
        const OpenDdl::Structure floatArray = color.firstChild();
        if(floatArray.subArraySize() != 3 && floatArray.subArraySize() != 4) {
            Error() << "Trade::OpenGexImporter::light(): invalid color structure";
            return Containers::NullOpt;
        }

        const auto attrib = color.propertyOf(OpenGex::attrib);
        if(attrib.as<std::string>() == "light") {
            lightColor = extractColorData<Color3>(floatArray);
        } else {
            Error() << "Trade::OpenGexImporter::light(): invalid color";
            return Containers::NullOpt;
        }
    }

    for(const OpenDdl::Structure param: light.childrenOf(OpenGex::Param)) {
        const OpenDdl::Structure data = param.firstChild();

        const auto attrib = param.propertyOf(OpenGex::attrib);
        if(attrib.as<std::string>() == "intensity")
            lightIntensity = data.as<Float>();
        else {
            Error() << "Trade::OpenGexImporter::light(): invalid parameter";
            return Containers::NullOpt;
        }
    }

    /** @todo parse the Atten structure */

    return LightData{lightType, lightColor, lightIntensity, &light};
}

UnsignedInt OpenGexImporter::doMeshCount() const {
    return _d->meshes.size();
}

Containers::Optional<MeshData> OpenGexImporter::doMesh(const UnsignedInt id, UnsignedInt) {
    const OpenDdl::Structure& mesh = _d->meshes[id].firstChildOf(OpenGex::Mesh);

    /* Primitive type, triangles by default */
    std::size_t indexArraySubArraySize = 3;
    MeshPrimitive primitive = MeshPrimitive::Triangles;
    if(const Containers::Optional<OpenDdl::Property> primitiveProperty = mesh.findPropertyOf(OpenGex::primitive)) {
        auto&& primitiveString = primitiveProperty->as<std::string>();
        if(primitiveString == "points") {
            primitive = MeshPrimitive::Points;
            indexArraySubArraySize = 0;
        } else if(primitiveString == "lines") {
            primitive = MeshPrimitive::Lines;
            indexArraySubArraySize = 1;
        } else if(primitiveString == "line_strip") {
            primitive = MeshPrimitive::LineStrip;
            indexArraySubArraySize = 1;
        } else if(primitiveString == "triangle_strip") {
            primitive = MeshPrimitive::TriangleStrip;
            indexArraySubArraySize = 1;
        } else if(primitiveString != "triangles") {
            /** @todo quads */
            Error() << "Trade::OpenGexImporter::mesh(): unsupported primitive" << primitiveString;
            return Containers::NullOpt;
        }
    }

    /* Gather all attributes. Position is optional as well. */
    std::size_t attributeCount = 0;
    std::ptrdiff_t stride = 0;
    UnsignedInt vertexCount = 0;
    for(const OpenDdl::Structure vertexArray: mesh.childrenOf(OpenGex::VertexArray)) {
        /* Skip unsupported ones */
        auto&& attrib = vertexArray.propertyOf(OpenGex::attrib).as<std::string>();
        if(attrib != "position" && attrib != "normal" && attrib != "texcoord")
            continue;

        /* Doubles were supported before (and converted to floats), I could do
           a cast now as well but I don't bother as nobody uses this format
           anymore anyway */
        const OpenDdl::Structure vertexArrayData = vertexArray.firstChild();
        if(vertexArrayData.type() != OpenDdl::Type::Float) {
            Error() << "Trade::OpenGexImporter::mesh(): unsupported vertex array type" << vertexArrayData.type();
            return Containers::NullOpt;
        }

        if(attrib == "position") {
            /* 2D positions could be supported too but I don't bother due to
               the same reason as above */
            if(vertexArrayData.subArraySize() != 3) {
                Error{} << "Trade::OpenGexImporter::mesh(): unsupported position vector size" << vertexArrayData.subArraySize();
                return Containers::NullOpt;
            }

            stride += sizeof(Vector3);

        } else if(attrib == "normal") {
            if(vertexArrayData.subArraySize() != 3) {
                Error{} << "Trade::OpenGexImporter::mesh(): unsupported normal vector size" << vertexArrayData.subArraySize();
                return Containers::NullOpt;
            }

            stride += sizeof(Vector3);

        } else if(attrib == "texcoord") {
            if(vertexArrayData.subArraySize() != 2) {
                Error{} << "Trade::OpenGexImporter::mesh(): unsupported texture coordinate vector size" << vertexArrayData.subArraySize();
                return Containers::NullOpt;
            }

            stride += sizeof(Vector2);

        } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

        /* If this is the first attribute, save the vertex count; otherwise
           check that all attributes have the same amount of vertices */
        const UnsignedInt attributeVertexCount = vertexArrayData.arraySize()/(vertexArrayData.subArraySize() ? vertexArrayData.subArraySize() : 1);
        if(!attributeCount) vertexCount = attributeVertexCount;
        else if(vertexCount != attributeVertexCount) {
            Error{} << "Trade::OpenGexImporter::mesh(): mismatched vertex count for attribute" << attrib << Debug::nospace << ", expected" << vertexCount << "but got" << attributeVertexCount;
            return Containers::NullOpt;
        }

        ++attributeCount;
    }

    /* Allocate vertex data, fill attributes */
    Containers::Array<char> vertexData{Containers::NoInit, std::size_t(stride)*vertexCount};
    Containers::Array<MeshAttributeData> attributeData{attributeCount};
    std::size_t attributeIndex = 0;
    std::size_t attributeOffset = 0;

    for(const OpenDdl::Structure vertexArray: mesh.childrenOf(OpenGex::VertexArray)) {
        /* Skip unsupported ones */
        const OpenDdl::Structure vertexArrayData = vertexArray.firstChild();

        /* Vertex positions */
        auto&& attrib = vertexArray.propertyOf(OpenGex::attrib).as<std::string>();
        if(attrib == "position") {
            Containers::StridedArrayView1D<Vector3> positions{vertexData,
                reinterpret_cast<Vector3*>(vertexData + attributeOffset),
                vertexCount, stride};
            Utility::copy(Containers::arrayCast<const Vector3>(vertexArrayData.asArray<Float>()), positions);
            for(auto& i: positions) i *= _d->distanceMultiplier;
            if(!_d->yUp) for(auto& i: positions) i = fixVectorZUp(i);

            attributeData[attributeIndex++] = MeshAttributeData{
                MeshAttribute::Position, positions};
            attributeOffset += sizeof(Vector3);

        /* Normals */
        } else if(attrib == "normal") {
            Containers::StridedArrayView1D<Vector3> normals{vertexData,
                reinterpret_cast<Vector3*>(vertexData + attributeOffset),
                vertexCount, stride};
            Utility::copy(Containers::arrayCast<const Vector3>(vertexArrayData.asArray<Float>()), normals);
            if(!_d->yUp) for(auto& i: normals) i = fixVectorZUp(i);

            attributeData[attributeIndex++] = MeshAttributeData{
                MeshAttribute::Normal, normals};
            attributeOffset += sizeof(Vector3);

        /* 2D texture coordinates */
        } else if(attrib == "texcoord") {
            Containers::StridedArrayView1D<Vector2> textureCoordinates{vertexData,
                reinterpret_cast<Vector2*>(vertexData + attributeOffset),
                vertexCount, stride};
            Utility::copy(Containers::arrayCast<const Vector2>(vertexArrayData.asArray<Float>()), textureCoordinates);

            attributeData[attributeIndex++] = MeshAttributeData{
                MeshAttribute::TextureCoordinates, textureCoordinates};
            attributeOffset += sizeof(Vector2);

        /* Some other thing that wasn't handled above, ignore */
        }
    }

    /* Check we pre-calculated well */
    CORRADE_INTERNAL_ASSERT(attributeOffset == std::size_t(stride));
    CORRADE_INTERNAL_ASSERT(attributeIndex == attributeCount);

    /* Mesh indices */
    MeshIndexData indices;
    Containers::Array<char> indexData;
    if(const Containers::Optional<OpenDdl::Structure> indexArray = mesh.findFirstChildOf(OpenGex::IndexArray)) {
        const OpenDdl::Structure indexArrayData = indexArray->firstChild();

        if(indexArrayData.subArraySize() != indexArraySubArraySize) {
            Error() << "Trade::OpenGexImporter::mesh(): invalid index array subarray size" << indexArrayData.subArraySize() << "for" << primitive;
            return Containers::NullOpt;
        }

        switch(indexArrayData.type()) {
            case OpenDdl::Type::UnsignedByte: {
                Containers::ArrayView<const UnsignedByte> src =
                    indexArrayData.asArray<UnsignedByte>();
                indexData = Containers::Array<char>{src.size()};
                auto indexData8 = Containers::arrayCast<UnsignedByte>(indexData);
                Utility::copy(src, indexData8);
                indices = MeshIndexData{indexData8};
            } break;
            case OpenDdl::Type::UnsignedShort: {
                Containers::ArrayView<const UnsignedShort> src =
                    indexArrayData.asArray<UnsignedShort>();
                indexData = Containers::Array<char>{src.size()*2};
                auto indexData16 = Containers::arrayCast<UnsignedShort>(indexData);
                Utility::copy(src, indexData16);
                indices = MeshIndexData{indexData16};
            } break;
            case OpenDdl::Type::UnsignedInt: {
                Containers::ArrayView<const UnsignedInt> src =
                    indexArrayData.asArray<UnsignedInt>();
                indexData = Containers::Array<char>{src.size()*4};
                auto indexData32 = Containers::arrayCast<UnsignedInt>(indexData);
                Utility::copy(src, indexData32);
                indices = MeshIndexData{indexData32};
            } break;
            #ifndef CORRADE_TARGET_EMSCRIPTEN
            case OpenDdl::Type::UnsignedLong:
                Error() << "Trade::OpenGexImporter::mesh(): 64bit indices are not supported";
                return Containers::NullOpt;
            #endif

            default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }
    }

    return MeshData{primitive,
        std::move(indexData), indices,
        std::move(vertexData), std::move(attributeData)};
}

UnsignedInt OpenGexImporter::doMaterialCount() const { return _d->materials.size(); }

Int OpenGexImporter::doMaterialForName(const std::string& name) {
    const auto found = _d->materialsForName.find(name);
    return found == _d->materialsForName.end() ? -1 : found->second;
}

std::string OpenGexImporter::doMaterialName(const UnsignedInt id) {
    const auto name = _d->materials[id].findFirstChildOf(OpenGex::Name);
    return name ? name->firstChild().as<std::string>() : "";
}

Containers::Optional<MaterialData> OpenGexImporter::doMaterial(const UnsignedInt id) {
    const OpenDdl::Structure& material = _d->materials[id];

    Containers::Array<MaterialAttributeData> attributes;

    /* Textures */
    for(const OpenDdl::Structure texture: material.childrenOf(OpenGex::Texture)) {
        const auto& attrib = texture.propertyOf(OpenGex::attrib).as<std::string>();
        if(attrib == "diffuse") {
            arrayAppend(attributes, Containers::InPlaceInit, MaterialAttribute::DiffuseTexture, structureId(_d->textures, texture));
        } else if(attrib == "specular") {
            arrayAppend(attributes, Containers::InPlaceInit, MaterialAttribute::SpecularTexture, structureId(_d->textures, texture));
        }
    }

    /* Colors */
    for(const OpenDdl::Structure color: material.childrenOf(OpenGex::Color)) {
        const OpenDdl::Structure floatArray = color.firstChild();
        if(floatArray.subArraySize() != 3 && floatArray.subArraySize() != 4) {
            Error() << "Trade::OpenGexImporter::material(): invalid color structure";
            return Containers::NullOpt;
        }

        const auto& attrib = color.propertyOf(OpenGex::attrib).as<std::string>();
        if(attrib == "diffuse")
            arrayAppend(attributes, Containers::InPlaceInit, MaterialAttribute::DiffuseColor, extractColorData<Color4>(floatArray));
        else if(attrib == "specular")
            arrayAppend(attributes, Containers::InPlaceInit, MaterialAttribute::SpecularColor, extractColorData<Color4>(floatArray));
    }

    /* Parameters */
    for(const OpenDdl::Structure param: material.childrenOf(OpenGex::Param)) {
        const auto& attrib = param.propertyOf(OpenGex::attrib).as<std::string>();
        if(attrib == "specular_power")
            arrayAppend(attributes, Containers::InPlaceInit, MaterialAttribute::Shininess, param.firstChild().as<Float>());
    }

    arrayShrink(attributes, Containers::DefaultInit);
    return MaterialData{MaterialType::Phong, std::move(attributes), &material};
}

UnsignedInt OpenGexImporter::doTextureCount() const { return _d->textures.size(); }

Containers::Optional<TextureData> OpenGexImporter::doTexture(const UnsignedInt id) {
    const OpenDdl::Structure& texture = _d->textures[id];

    if(const auto texcoord = texture.findPropertyOf(OpenGex::texcoord)) if(texcoord->as<Int>() != 0) {
        Error() << "Trade::OpenGexImporter::texture(): unsupported texture coordinate set";
        return Containers::NullOpt;
    }

    /** @todo texture coordinate transformations */

    return TextureData{TextureData::Type::Texture2D, SamplerFilter::Linear, SamplerFilter::Linear, SamplerMipmap::Linear, SamplerWrapping::ClampToEdge, _d->imagesForName[texture.firstChildOf(OpenDdl::Type::String).as<std::string>()], &texture};
}

UnsignedInt OpenGexImporter::doImage2DCount() const { return _d->images.size(); }

AbstractImporter* OpenGexImporter::setupOrReuseImporterForImage(const UnsignedInt id, const char* const errorPrefix) {
    /* Looking for the same ID, so reuse an importer populated before. If the
       previous attempt failed, the importer is not set, so return nullptr in
       that case. Going through everything below again would not change the
       outcome anyway, only spam the output with redundant messages. */
    if(_d->imageImporterId == id)
        return _d->imageImporter ? &*_d->imageImporter : nullptr;

    /* Otherwise reset the importer and remember the new ID. If the import
       fails, the importer will stay unset, but the ID will be updated so the
       next round can again just return nullptr above instead of going through
       the doomed-to-fail process again. */
    _d->imageImporter = Containers::NullOpt;
    _d->imageImporterId = id;

    if(!_d->filePath && !fileCallback()) {
        Error{} << errorPrefix << "images can be imported only when opening files from the filesystem or if a file callback is present";
        return {};
    }

    AnyImageImporter importer{*manager()};
    if(fileCallback()) importer.setFileCallback(fileCallback(), fileCallbackUserData());

    const std::string imageFile = Utility::Directory::join(_d->filePath ? *_d->filePath : "", _d->images[id]);
    if(!importer.openFile(imageFile))
        return nullptr;
    return &_d->imageImporter.emplace(std::move(importer));
}

UnsignedInt OpenGexImporter::doImage2DLevelCount(const UnsignedInt id) {
    CORRADE_ASSERT(manager(), "Trade::OpenGexImporter::image2DLevelCount(): the plugin must be instantiated with access to plugin manager in order to open image files", {});

    AbstractImporter* importer = setupOrReuseImporterForImage(id, "Trade::OpenGexImporter::image2DLevelCount():");
    /* image2DLevelCount() isn't supposed to fail (image2D() is, instead), so
       report 1 on failure and expect image2D() to fail later */
    if(!importer) return 1;

    return importer->image2DLevelCount(0);
}

Containers::Optional<ImageData2D> OpenGexImporter::doImage2D(const UnsignedInt id, const UnsignedInt level) {
    CORRADE_ASSERT(manager(), "Trade::OpenGexImporter::image2D(): the plugin must be instantiated with access to plugin manager in order to open image files", {});

    AbstractImporter* importer = setupOrReuseImporterForImage(id, "Trade::OpenGexImporter::image2D():");
    if(!importer) return Containers::NullOpt;

    return importer->image2D(0, level);
}

const void* OpenGexImporter::doImporterState() const {
    return &_d->document;
}

}}

CORRADE_PLUGIN_REGISTER(OpenGexImporter, Magnum::Trade::OpenGexImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.3")
