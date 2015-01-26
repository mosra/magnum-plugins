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

#include <limits>
#include <unordered_map>
#include <Corrade/Containers/Array.h>
#include <Magnum/Mesh.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/MeshData3D.h>

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

    std::vector<OpenDdl::Structure> meshes;
};

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

    /* Everything okay, save the instance */
    _d = std::move(d);
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

}}
