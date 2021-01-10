/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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

#include "StanfordSceneConverter.h"

#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/EndiannessBatch.h>
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/MeshTools/Duplicate.h>
#include <Magnum/MeshTools/GenerateIndices.h>
#include <Magnum/Trade/MeshData.h>

namespace Magnum { namespace Trade {

StanfordSceneConverter::StanfordSceneConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractSceneConverter{manager, plugin} {}

StanfordSceneConverter::~StanfordSceneConverter() = default;

SceneConverterFeatures StanfordSceneConverter::doFeatures() const { return SceneConverterFeature::ConvertMeshToData; }

Containers::Array<char> StanfordSceneConverter::doConvertToData(const MeshData& mesh) {
    /* Convert to an indexed triangle mesh if it's a strip/fan */
    MeshData triangles{MeshPrimitive::Triangles, 0};
    if(mesh.primitive() == MeshPrimitive::TriangleStrip || mesh.primitive() == MeshPrimitive::TriangleFan) {
        if(mesh.isIndexed())
            triangles = MeshTools::generateIndices(MeshTools::duplicate(mesh));
        else triangles = MeshTools::generateIndices(std::move(mesh));

    /* If it's triangles already, make a non-owning reference to the original */
    } else if(mesh.primitive() == MeshPrimitive::Triangles) {
        Containers::ArrayView<const char> indexData;
        MeshIndexData indices;
        if(mesh.isIndexed()) {
            indexData = mesh.indexData();
            indices = MeshIndexData{mesh.indices()};
        }
        triangles = MeshData{mesh.primitive(),
            {}, indexData, indices,
            {}, mesh.vertexData(), meshAttributeDataNonOwningArray(mesh.attributeData()),
            mesh.vertexCount()
        };

    /* Otherwise we're sorry */
    } else {
        Error{} << "Trade::StanfordSceneConverter::convertToData(): expected a triangle mesh, got" << mesh.primitive();
        return nullptr;
    }

    /* Decide on endian swapping, write file signature */
    bool endianSwapNeeded;
    std::string header = "ply\n";
    {
        bool isBigEndian;
        if(configuration().value("endianness") == "native") {
            isBigEndian = Utility::Endianness::isBigEndian();
            endianSwapNeeded = false;
        } else if(configuration().value("endianness") == "little") {
            isBigEndian = false;
            endianSwapNeeded = Utility::Endianness::isBigEndian();
        } else if(configuration().value("endianness") == "big") {
            isBigEndian = true;
            endianSwapNeeded = !Utility::Endianness::isBigEndian();
        } else {
            Error{} << "Trade::StanfordSceneConverter::convertToData(): invalid option endianness=" << Debug::nospace << configuration().value("endianness");
            return nullptr;
        }
        header += isBigEndian ?
            "format binary_big_endian 1.0\n" :
            "format binary_little_endian 1.0\n";
    }

    /* Write attribute header and calculate offsets for copying later.
       Attributes that can't be written because the type is not supported by
       PLY or the name is unknown will have offset kept at ~std::size_t{}. */
    Containers::Array<std::size_t> offsets{Containers::DirectInit, triangles.attributeCount(), ~std::size_t{}};
    std::size_t vertexSize = 0;
    header += Utility::formatString("element vertex {}\n", triangles.vertexCount());
    for(UnsignedInt i = 0; i != triangles.attributeCount(); ++i) {
        const MeshAttribute name = triangles.attributeName(i);
        const VertexFormat format = triangles.attributeFormat(i);
        if(isVertexFormatImplementationSpecific(format)) {
            Warning{} << "Trade::StanfordSceneConverter::convertToData(): skipping attribute" << name << "with" << format;
            continue;
        }

        /* Decide on a format string */
        const char* formatString;
        switch(vertexFormatComponentFormat(format)) {
            case VertexFormat::Float:
                formatString = "float";
                break;
            /** @todo convert halves to floats? */
            case VertexFormat::Double:
                formatString = "double";
                break;
            case VertexFormat::UnsignedByte:
            case VertexFormat::UnsignedByteNormalized:
                formatString = "uchar";
                break;
            case VertexFormat::Byte:
            case VertexFormat::ByteNormalized:
                formatString = "char";
                break;
            case VertexFormat::UnsignedShort:
            case VertexFormat::UnsignedShortNormalized:
                formatString = "ushort";
                break;
            case VertexFormat::Short:
            case VertexFormat::ShortNormalized:
                formatString = "short";
                break;
            case VertexFormat::UnsignedInt:
                formatString = "uint";
                break;
            case VertexFormat::Int:
                formatString = "int";
                break;
            default:
                Warning{} << "Trade::StanfordSceneConverter::convertToData(): skipping attribute" << name << "with unsupported format" << format;
                continue;
        }

        /* Positions */
        if(name == MeshAttribute::Position) {
            if(vertexFormatComponentCount(format) != 3) {
                Error{} << "Trade::StanfordSceneConverter::convertToData(): two-component positions are not supported";
                return nullptr;
            }

            header += Utility::formatString(
                "property {0} x\n"
                "property {0} y\n"
                "property {0} z\n", formatString);

        /* Normals */
        } else if(name == MeshAttribute::Normal) {
            header += Utility::formatString(
                "property {0} nx\n"
                "property {0} ny\n"
                "property {0} nz\n", formatString);

        /* Texture coordinates */
        } else if(name == MeshAttribute::TextureCoordinates) {
            header += Utility::formatString(
                "property {0} u\n"
                "property {0} v\n", formatString);

        /* Colors */
        } else if(name == MeshAttribute::Color) {
            header += Utility::formatString(
                vertexFormatComponentCount(format) == 3 ?
                    "property {0} red\n"
                    "property {0} green\n"
                    "property {0} blue\n" :
                    "property {0} red\n"
                    "property {0} green\n"
                    "property {0} blue\n"
                    "property {0} alpha\n", formatString);

        /* Object ID */
        } else if(name == MeshAttribute::ObjectId) {
            header += Utility::formatString("property {} {}\n", formatString,
                configuration().value("objectIdAttribute"));

        /* Something else, skip */
        /** @todo add setMeshAttributeName() and enable this for custom attribs */
        } else {
            Warning{} << "Trade::StanfordSceneConverter::convertToData(): skipping unsupported attribute" << name;
            continue;
        }

        offsets[i] = vertexSize;
        vertexSize += vertexFormatSize(format);
    }

    /* Index type */
    const char* indexTypeString = nullptr;
    if(!triangles.isIndexed())
        indexTypeString = "uint";
    else switch(triangles.indexType()) {
        case MeshIndexType::UnsignedInt:
            indexTypeString = "uint";
            break;
        case MeshIndexType::UnsignedShort:
            indexTypeString = "ushort";
            break;
        case MeshIndexType::UnsignedByte:
            indexTypeString = "uchar";
            break;
    }
    CORRADE_INTERNAL_ASSERT(indexTypeString);

    /* Wrap up the header -- for face attributes we have just the index list */
    /** @todo once multi-mesh conversion is supported, this could accept a
        MeshAttribute::Face with per-face attribs */
    header += Utility::formatString(
        "element face {}\n"
        "property list uchar {} vertex_indices\n"
        "end_header\n",
        triangles.isIndexed() ? triangles.indexCount()/3 : triangles.vertexCount()/3,
        indexTypeString);

    /* Vertex and index data size. For a non-indexed mesh we'll use 32-bit
       indices for simplicity, face size is always 3 so a 1-byte type is
       enough. */
    const std::size_t vertexDataSize = vertexSize*triangles.vertexCount();
    std::size_t indexTypeSize;
    std::size_t indexDataSize;
    if(!triangles.isIndexed()) {
        indexTypeSize = 4;
        indexDataSize = (1 + 3*4)*triangles.vertexCount()/3;
    } else {
        indexTypeSize = meshIndexTypeSize(triangles.indexType());
        indexDataSize = (1 + 3*indexTypeSize)*triangles.indexCount()/3;
    }

    /* Allocate the data, copy header */
    Containers::Array<char> out{Containers::NoInit, header.size() + vertexDataSize + indexDataSize};
    /* Needs an explicit ArrayView constructor, otherwise MSVC 2015, 17 and 19
       creates ArrayView<const void> here (wtf!) */
    Utility::copy(Containers::ArrayView<const char>{header.data(), header.size()}, out.prefix(header.size()));

    /* Copy the vertices */
    Containers::ArrayView<char> vertexData = out.slice(header.size(), header.size() + vertexDataSize);
    for(UnsignedInt i = 0; i != triangles.attributeCount(); ++i) {
        if(offsets[i] == ~std::size_t{}) continue;

        const Containers::StridedArrayView2D<const char> src = triangles.attribute(i);
        const Containers::StridedArrayView2D<char> dst{vertexData,
            vertexData.begin() + offsets[i],
            src.size(), {std::ptrdiff_t(vertexSize), 1}};
        Utility::copy(src, dst);

        /* Endian swap, if needed */
        if(endianSwapNeeded) {
            const VertexFormat format = triangles.attributeFormat(i);
            const UnsignedInt componentSize = vertexFormatSize(vertexFormatComponentFormat(format));
            if(componentSize == 1) continue;

            /* Can't reuse the dst array as it has no information about the
               component layout. Build a sparse view from scratch instead. */
            const Containers::StridedArrayView2D<char> components{vertexData,
                vertexData.begin() + offsets[i],
                {vertexFormatComponentCount(format),
                 triangles.vertexCount()},
                {std::ptrdiff_t(componentSize),
                 std::ptrdiff_t(vertexSize)}};
            for(Containers::StridedArrayView1D<char> component: components) {
                if(componentSize == 8)
                    Utility::Endianness::swapInPlace(Containers::arrayCast<UnsignedLong>(component));
                else if(componentSize == 4)
                    Utility::Endianness::swapInPlace(Containers::arrayCast<UnsignedInt>(component));
                else if(componentSize == 2)
                    Utility::Endianness::swapInPlace(Containers::arrayCast<UnsignedShort>(component));
                else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
            }
        }
    }

    /* Copy the indices. For a non-indexed mesh make a trivial index array. */
    Containers::ArrayView<char> indexData = out.suffix(header.size() + vertexDataSize);
    Containers::StridedArrayView3D<char> indices;
    if(!triangles.isIndexed()) {
        const Containers::StridedArrayView2D<UnsignedInt> indices32{indexData,
            reinterpret_cast<UnsignedInt*>(indexData.begin() + 1),
            {triangles.vertexCount()/3, 3}, {1 + 3*4, 4}};
        for(std::size_t i = 0; i != indices32.size()[0]; ++i) {
            Containers::StridedArrayView1D<UnsignedInt> face = indices32[i];
            for(std::size_t j = 0; j != 3; ++j)
                face[j] = i*3 + j;
        }

        indices = Containers::arrayCast<3, char>(indices32);

    /* For an indexed mesh simply copy the data */
    } else {
        const Containers::StridedArrayView3D<const char> src{
            triangles.indices().asContiguous(),
            {triangles.indexCount()/3, 3, indexTypeSize},
            {std::ptrdiff_t(3*indexTypeSize), std::ptrdiff_t(indexTypeSize), 1}};
        indices = Containers::StridedArrayView3D<char>{indexData,
            indexData.begin() + 1,
            {triangles.indexCount()/3, 3, indexTypeSize},
            {std::ptrdiff_t(1 + 3*indexTypeSize), std::ptrdiff_t(indexTypeSize), 1}};
        Utility::copy(src, indices);
    }

    /* Endian-swap the indices, if needed */
    if(endianSwapNeeded) {
        if(indexTypeSize == 4) {
            for(Containers::StridedArrayView1D<UnsignedInt> i: Containers::arrayCast<2, UnsignedInt>(indices).transposed<0, 1>())
                Utility::Endianness::swapInPlace(i);
        } else if(indexTypeSize == 2) {
            for(Containers::StridedArrayView1D<UnsignedShort> i: Containers::arrayCast<2, UnsignedShort>(indices).transposed<0, 1>())
                Utility::Endianness::swapInPlace(i);
        } else CORRADE_INTERNAL_ASSERT(indexTypeSize == 1);
    }

    /* Fill in face sizes. That's just 3 repeated many times over */
    {
        constexpr UnsignedByte three[]{3};
        Containers::StridedArrayView1D<const UnsignedByte> src = three;
        Containers::StridedArrayView1D<UnsignedByte> dst;
        if(!triangles.isIndexed()) {
            src = src.broadcasted<0>(triangles.vertexCount()/3);
            dst = Containers::StridedArrayView1D<UnsignedByte>{indexData,
                reinterpret_cast<UnsignedByte*>(indexData.begin()),
                triangles.vertexCount()/3, 1 + 3*4};
        } else {
            src = src.broadcasted<0>(triangles.indexCount()/3);
            dst = Containers::StridedArrayView1D<UnsignedByte>{indexData,
                reinterpret_cast<UnsignedByte*>(indexData.begin()),
                triangles.indexCount()/3, std::ptrdiff_t(1 + 3*indexTypeSize)};
        }
        Utility::copy(src, dst);
    }

    return out;
}

}}

CORRADE_PLUGIN_REGISTER(StanfordSceneConverter, Magnum::Trade::StanfordSceneConverter,
    "cz.mosra.magnum.Trade.AbstractSceneConverter/0.1")
