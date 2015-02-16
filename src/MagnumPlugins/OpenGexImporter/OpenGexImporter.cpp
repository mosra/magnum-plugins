/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015
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

#include "OpenGexImporter.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/TextureData.h>

#include "MagnumPlugins/AnyImageImporter/AnyImageImporter.h"
#include "MagnumPlugins/OpenGexImporter/OpenDdl/Document.h"
#include "MagnumPlugins/OpenGexImporter/OpenDdl/Property.h"
#include "MagnumPlugins/OpenGexImporter/OpenDdl/Structure.h"

#include "openGexSpec.hpp"

namespace Magnum { namespace Trade {

struct OpenGexImporter::Document {
    OpenDdl::Document document;

    /* Default metrics */
    Float distanceMultiplier = 1.0f;
    Float angleMultiplier = 1.0f;
    Float timeMultiplier = 1.0f;
    bool yUp = false;

    std::optional<std::string> filePath;

    std::vector<OpenDdl::Structure> meshes,
        materials,
        textures;

    std::unordered_map<std::string, Int> materialsForName;
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

OpenGexImporter::OpenGexImporter(PluginManager::AbstractManager& manager, std::string plugin): AbstractImporter(manager, plugin) {}

OpenGexImporter::~OpenGexImporter() = default;

auto OpenGexImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool OpenGexImporter::doIsOpened() const { return !!_d; }

void OpenGexImporter::doOpenData(const Containers::ArrayReference<const char> data) {
    std::unique_ptr<Document> d{new Document};

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

            d->angleMultiplier = value.as<Float>();

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

    /* Gather all meshes */
    /** @todo Support for LOD */
    for(const OpenDdl::Structure geometry: d->document.childrenOf(OpenGex::GeometryObject))
        d->meshes.push_back(geometry);

    /* Gather all light textures */
    for(const OpenDdl::Structure lightObject: d->document.childrenOf(OpenGex::LightObject))
        for(const OpenDdl::Structure texture: lightObject.childrenOf(OpenGex::Texture))
            d->textures.push_back(texture);

    /* Gather all materials */
    {
        for(const OpenDdl::Structure material: d->document.childrenOf(OpenGex::Material)) {
            if(const auto name = material.findFirstChildOf(OpenGex::Name))
                d->materialsForName.emplace(name->firstChild().as<std::string>(), d->materials.size());
            d->materials.push_back(material);

            /* Gather all material textures */
            for(const OpenDdl::Structure texture: material.childrenOf(OpenGex::Texture))
                d->textures.push_back(texture);
        }
    }

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

UnsignedInt OpenGexImporter::doMesh3DCount() const {
    return _d->meshes.size();
}

namespace {

template<class Result, class Original> std::vector<Result> extractVertexData3(const OpenDdl::Structure vertexArray) {
    const Containers::ArrayReference<const typename Original::Type> data = vertexArray.asArray<typename Original::Type>();
    const std::size_t vertexCount = vertexArray.arraySize()/(vertexArray.subArraySize() ? vertexArray.subArraySize() : 1);

    std::vector<Result> output;
    output.reserve(vertexCount);
    for(std::size_t i = 0; i != vertexCount; ++i)
        /* Convert the data to result type and then pad it to result size */
        output.push_back(Result::pad(Math::Vector<Original::Size, typename Result::Type>{Original::from(data + i*Original::Size)}));

    return output;
}

template<class Result, std::size_t originalSize> std::vector<Result> extractVertexData2(const OpenDdl::Structure vertexArray) {
    switch(vertexArray.type()) {
        /** @todo half */
        case OpenDdl::Type::Float:
            return extractVertexData3<Result, Math::Vector<originalSize, Float>>(vertexArray);
        #ifndef MAGNUM_TARGET_GLES
        case OpenDdl::Type::Double:
            return extractVertexData3<Result, Math::Vector<originalSize, Double>>(vertexArray);
        #endif
        default: CORRADE_ASSERT_UNREACHABLE();
    }

}

template<class Result> std::vector<Result> extractVertexData(const OpenDdl::Structure vertexArray) {
    switch(vertexArray.subArraySize()) {
        case 0:
        case 1:
            return extractVertexData2<Result, 1>(vertexArray);
        case 2:
            return extractVertexData2<Result, 2>(vertexArray);
        case 3:
            return extractVertexData2<Result, 3>(vertexArray);
        case 4:
            return extractVertexData2<Result, 4>(vertexArray);
    }

    CORRADE_ASSERT_UNREACHABLE();
}

template<class T> std::vector<UnsignedInt> extractIndices(const OpenDdl::Structure indexArray) {
    const Containers::ArrayReference<const T> data = indexArray.asArray<T>();
    return {data.begin(), data.end()};
}

inline Vector3 fixZUp(Vector3 vec) { return {vec.x(), vec.z(), -vec.y()}; }

}

std::optional<MeshData3D> OpenGexImporter::doMesh3D(const UnsignedInt id) {
    const OpenDdl::Structure mesh = _d->meshes[id].firstChildOf(OpenGex::Mesh);

    /* Primitive type, triangles by default */
    std::size_t indexArraySubArraySize = 3;
    MeshPrimitive primitive = MeshPrimitive::Triangles;
    if(const std::optional<OpenDdl::Property> primitiveProperty = mesh.findPropertyOf(OpenGex::primitive)) {
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
            Error() << "Trade::OpenGexImporter::mesh3D(): unsupported primitive" << primitiveString;
            return std::nullopt;
        }
    }

    /* Vertices */
    std::vector<std::vector<Vector3>> positions;
    std::vector<std::vector<Vector2>> textureCoordinates;
    std::vector<std::vector<Vector3>> normals;
    for(const OpenDdl::Structure vertexArray: mesh.childrenOf(OpenGex::VertexArray)) {
        /* Skip unsupported ones */
        auto&& attrib = vertexArray.propertyOf(OpenGex::attrib).as<std::string>();
        if(attrib != "position" && attrib != "normal" && attrib != "texcoord")
            continue;

        const OpenDdl::Structure vertexArrayData = vertexArray.firstChild();

        /* Sanity checks (would be too bloaty to do in actual templated code) */
        if(vertexArrayData.type() != OpenDdl::Type::Float
           #ifndef MAGNUM_TARGET_GLES
           && vertexArrayData.type() != OpenDdl::Type::Double
           #endif
        ) {
            Error() << "Trade::OpenGexImporter::mesh3D(): unsupported vertex array type" << vertexArrayData.type();
            return std::nullopt;
        }
        if(vertexArrayData.subArraySize() > 4) {
            Error() << "Trade::OpenGexImporter::mesh3D(): unsupported vertex array vector size" << vertexArrayData.subArraySize();
            return std::nullopt;
        }

        /* Vertex positions */
        if(attrib == "position") {
            std::vector<Vector3> positionData = extractVertexData<Vector3>(vertexArrayData);
            for(auto& i: positionData) i *= _d->distanceMultiplier;
            if(!_d->yUp) for(auto& i: positionData) i = fixZUp(i);
            positions.push_back(std::move(positionData));

        /* Normals */
        } else if(attrib == "normal") {
            std::vector<Vector3> normalData = extractVertexData<Vector3>(vertexArrayData);
            if(!_d->yUp) for(auto& i: normalData) i = fixZUp(i);
            normals.emplace_back(std::move(normalData));

        /* 2D texture coordinates */
        } else if(attrib == "texcoord")
            textureCoordinates.emplace_back(extractVertexData<Vector2>(vertexArrayData));
    }

    /* Sanity checks */
    if(positions.empty()) {
        Error() << "Trade::OpenGexImporter::mesh3D(): no vertex position array found";
        return std::nullopt;
    }
    std::size_t count = 0;
    for(auto&& a: positions) {
        if(!count) count = a.size();
        else if(a.size() != count) count = std::numeric_limits<std::size_t>::max();
    }
    for(auto&& a: normals) {
        if(!count) count = a.size();
        else if(a.size() != count) count = std::numeric_limits<std::size_t>::max();
    }
    for(auto&& a: textureCoordinates) {
        if(!count) count = a.size();
        else if(a.size() != count) count = std::numeric_limits<std::size_t>::max();
    }
    if(count == std::numeric_limits<std::size_t>::max()) {
        Error() << "Trade::OpenGexImporter::mesh3D(): mismatched vertex array sizes";
        return std::nullopt;
    }

    /* Index array */
    std::vector<UnsignedInt> indices;
    if(const std::optional<OpenDdl::Structure> indexArray = mesh.findFirstChildOf(OpenGex::IndexArray)) {
        const OpenDdl::Structure indexArrayData = indexArray->firstChild();

        if(indexArrayData.subArraySize() != indexArraySubArraySize) {
            Error() << "Trade::OpenGexImporter::mesh3D(): invalid index array subarray size" << indexArrayData.subArraySize() << "for" << primitive;
            return std::nullopt;
        }

        switch(indexArrayData.type()) {
            case OpenDdl::Type::UnsignedByte:
                indices = extractIndices<UnsignedByte>(indexArrayData);
                break;
            case OpenDdl::Type::UnsignedShort:
                indices = extractIndices<UnsignedShort>(indexArrayData);
                break;
            case OpenDdl::Type::UnsignedInt:
                indices = extractIndices<UnsignedInt>(indexArrayData);
                break;
            #ifndef MAGNUM_TARGET_GLES
            case OpenDdl::Type::UnsignedLong:
                Error() << "Trade::OpenGexImporter::mesh3D(): unsupported 64bit indices";
                return std::nullopt;
            #endif

            default: CORRADE_ASSERT_UNREACHABLE();
        }
    }

    return MeshData3D{primitive, indices, positions, normals, textureCoordinates};
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

namespace {

template<class Result, std::size_t originalSize> Result extractColorData2(const OpenDdl::Structure floatArray) {
    return Result::pad(*reinterpret_cast<const Math::Vector<originalSize, Float>*>(floatArray.asArray<Float>().data()));
}

template<class Result> Result extractColorData(const OpenDdl::Structure floatArray) {
    switch(floatArray.subArraySize()) {
        case 3: return extractColorData2<Result, 3>(floatArray);
        case 4: return extractColorData2<Result, 4>(floatArray);
    }

    CORRADE_ASSERT_UNREACHABLE();
}

}

std::unique_ptr<AbstractMaterialData> OpenGexImporter::doMaterial(const UnsignedInt id) {
    const OpenDdl::Structure material = _d->materials[id];

    /* Textures */
    PhongMaterialData::Flags flags;
    UnsignedInt diffuseTexture{}, specularTexture{};
    for(const OpenDdl::Structure texture: material.childrenOf(OpenGex::Texture)) {
        const auto& attrib = texture.propertyOf(OpenGex::attrib).as<std::string>();
        if(attrib == "diffuse") {
            diffuseTexture = structureId(_d->textures, texture);
            flags |= PhongMaterialData::Flag::DiffuseTexture;
        } else if(attrib == "specular") {
            specularTexture = structureId(_d->textures, texture);
            flags |= PhongMaterialData::Flag::SpecularTexture;
        }
    }

    /* Colors (used only if matching texture isn't already specified) */
    Vector3 diffuseColor{1.0f};
    Vector3 specularColor{0.0f};
    for(const OpenDdl::Structure color: material.childrenOf(OpenGex::Color)) {
        const OpenDdl::Structure floatArray = color.firstChild();
        if(floatArray.subArraySize() != 3 && floatArray.subArraySize() != 4) {
            Error() << "Trade::OpenGexImporter::material(): invalid color structure";
            return nullptr;
        }

        const auto& attrib = color.propertyOf(OpenGex::attrib).as<std::string>();
        if(attrib == "diffuse")
            diffuseColor = extractColorData<Vector3>(floatArray);
        else if(attrib == "specular")
            specularColor = extractColorData<Vector3>(floatArray);
    }

    /* Parameters */
    Float shininess{1.0f};
    for(const OpenDdl::Structure param: material.childrenOf(OpenGex::Param)) {
        const auto& attrib = param.propertyOf(OpenGex::attrib).as<std::string>();
        if(attrib == "specular_power")
            shininess = param.firstChild().as<Float>();
    }

    /* Put things together */
    std::unique_ptr<PhongMaterialData> data{new PhongMaterialData{flags, shininess}};
    data->ambientColor() = Vector3{0.0f};
    if(flags & PhongMaterialData::Flag::DiffuseTexture)
        data->diffuseTexture() = diffuseTexture;
    else data->diffuseColor() = diffuseColor;
    if(flags & PhongMaterialData::Flag::SpecularTexture)
        data->specularTexture() = specularTexture;
    else data->specularColor() = specularColor;
    return std::move(data);
}

UnsignedInt OpenGexImporter::doTextureCount() const { return _d->textures.size(); }

std::optional<TextureData> OpenGexImporter::doTexture(const UnsignedInt id) {
    const OpenDdl::Structure texture = _d->textures[id];

    if(const auto texcoord = texture.findPropertyOf(OpenGex::texcoord)) if(texcoord->as<Int>() != 0) {
        Error() << "Trade::OpenGexImporter::texture(): unsupported texture coordinate set";
        return std::nullopt;
    }

    /** @todo texture coordinate transformations */

    return TextureData{TextureData::Type::Texture2D, Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Mipmap::Linear, Sampler::Wrapping::ClampToEdge, id};
}

UnsignedInt OpenGexImporter::doImage2DCount() const { return _d->textures.size(); }

std::optional<ImageData2D> OpenGexImporter::doImage2D(const UnsignedInt id) {
    CORRADE_ASSERT(_d->filePath, "Trade::OpenGexImporter::image2D(): images can be imported only when opening files from filesystem", {});
    CORRADE_ASSERT(manager(), "Trade::OpenGexImporter::image2D(): the plugin must be instantiated with access to plugin manager in order to open image files", {});

    const OpenDdl::Structure texture = _d->textures[id];

    AnyImageImporter imageImporter{static_cast<PluginManager::Manager<AbstractImporter>&>(*manager())};
    if(!imageImporter.openFile(Utility::Directory::join(*_d->filePath, texture.firstChildOf(OpenDdl::Type::String).as<std::string>())))
        return std::nullopt;

    return imageImporter.image2D(0);
}

}}
