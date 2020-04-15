/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
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

#include "MagnumSceneConverter.h"

#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/EndiannessBatch.h>
#include <Magnum/Trade/MeshData.h>

#include "MagnumPlugins/MagnumImporter/Implementation/DataLayout.h"

namespace Magnum { namespace Trade {

MagnumSceneConverter::MagnumSceneConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractSceneConverter{manager, plugin} {
    if(plugin == "MagnumLittle32SceneConverter")
        _signature = DataChunkSignature::Little32;
    else if(plugin == "MagnumLittle64SceneConverter")
        _signature = DataChunkSignature::Little64;
    else if(plugin == "MagnumBig32SceneConverter")
        _signature = DataChunkSignature::Big32;
    else if(plugin == "MagnumBig64SceneConverter")
        _signature = DataChunkSignature::Big64;
    else if(plugin == "MagnumSceneConverter")
        _signature = DataChunkSignature::Current;
    else CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

MagnumSceneConverter::~MagnumSceneConverter() = default;

SceneConverterFeatures MagnumSceneConverter::doFeatures() const { return SceneConverterFeature::ConvertMeshToData; }

namespace {

template<class T> void fillHeader(T& header, const DataChunkSignature signature, const DataChunkType type, const UnsignedShort typeVersion, const std::size_t size, bool endianSwapNeeded) {
    header.version = 128;
    header.eolUnix[0] = '\x0a';
    header.eolDos[0] = '\x0d';
    header.eolDos[1] = '\x0a';
    header.signature = signature;
    header.zero = 0;
    header.typeVersion = typeVersion;
    header.type = type;
    header.size = size;

    if(endianSwapNeeded) Utility::Endianness::swapInPlace(header.typeVersion,
        header.size);
}

template<class T> void fillMeshHeader(T& header, const MeshData& mesh, bool endianSwapNeeded) {
    header.vertexCount = mesh.vertexCount();
    header.primitive = mesh.primitive();
    header.attributeCount = mesh.attributeCount();
    header.vertexDataSize = mesh.vertexData().size();

    if(mesh.isIndexed()) {
        header.indexCount = mesh.indexCount();
        header.indexType = mesh.indexType();
        header.indexOffset = mesh.indexOffset();
        header.indexDataSize = mesh.indexData().size();
    } else {
        header.indexCount = 0;
        header.indexType = MeshIndexType{};
        header.indexOffset = 0;
        header.indexDataSize = 0;
    }

    if(endianSwapNeeded) Utility::Endianness::swapInPlace(header.vertexCount,
        header.indexCount, header.primitive, header.indexOffset,
        header.attributeCount, header.indexDataSize, header.vertexDataSize);
}

template<class T> void fillMeshAttribute(T& attribute, const MeshData& mesh, UnsignedInt id, bool endianSwapNeeded) {
    attribute.format = mesh.attributeFormat(id);
    attribute.name = mesh.attributeName(id);
    attribute.isOffsetOnly = true;
    attribute.vertexCount = mesh.vertexCount();
    attribute.stride = mesh.attributeStride(id);
    attribute.arraySize = mesh.attributeArraySize(id);
    attribute.offset = mesh.attributeOffset(id);

    if(endianSwapNeeded) Utility::Endianness::swapInPlace(attribute.format,
        attribute.name, attribute.vertexCount, attribute.stride,
        attribute.arraySize, attribute.offset);
}

}

Containers::Array<char> MagnumSceneConverter::doConvertToData(const MeshData& mesh) {
    /* Calculate output size based on the signature */
    UnsignedLong size = mesh.indexData().size() + mesh.vertexData().size();
    bool is32bit;
    if(_signature == DataChunkSignature::Little32 ||
       _signature == DataChunkSignature::Big32)
    {
        size += sizeof(Implementation::MeshDataHeader32) +
            sizeof(Implementation::MeshAttributeData32)*mesh.attributeCount();
        is32bit = true;
        if(size > ~UnsignedInt{}) {
            Error{} << "Trade::MagnumSceneConverter::convertToData(): data size" << size << "too large for a 32-bit output platform";
            return nullptr;
        }
    } else if(_signature == DataChunkSignature::Little64 ||
              _signature == DataChunkSignature::Big64)
    {
        size += sizeof(Implementation::MeshDataHeader64) +
            sizeof(Implementation::MeshAttributeData64)*mesh.attributeCount();
        is32bit = false;
    } else CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    const bool endianSwapNeeded = Utility::Endianness::isBigEndian() !=
        (_signature == DataChunkSignature::Big32 ||
         _signature == DataChunkSignature::Big64);

    /* Allocate the output, memset the header first to avoid padding getting
       random values */
    Containers::Array<char> out{Containers::NoInit, std::size_t(size)};

    /* Memset the header to avoid padding getting random values */
    if(is32bit)
        std::memset(out.data() + sizeof(Implementation::DataChunkHeader32), 0, sizeof(Implementation::MeshDataHeader32) + mesh.attributeCount()*sizeof(Implementation::MeshAttributeData32) - sizeof(Implementation::DataChunkHeader32));
    else
        std::memset(out.data() + sizeof(Implementation::DataChunkHeader64), 0, sizeof(Implementation::MeshDataHeader64) + mesh.attributeCount()*sizeof(Implementation::MeshAttributeData64) - sizeof(Implementation::DataChunkHeader64));

    /* Fill the headers */
    std::size_t offset = 0;
    if(is32bit) {
        fillHeader(
            *reinterpret_cast<Implementation::DataChunkHeader32*>(out.data()),
            _signature, DataChunkType::Mesh, 0, size, endianSwapNeeded);
        fillMeshHeader(
            *reinterpret_cast<Implementation::MeshDataHeader32*>(out.data()),
            mesh, endianSwapNeeded);
        offset += sizeof(Implementation::MeshDataHeader32);
    } else {
        fillHeader(
            *reinterpret_cast<Implementation::DataChunkHeader64*>(out.data() + offset),
            _signature, DataChunkType::Mesh, 0, size, endianSwapNeeded);
        fillMeshHeader(
            *reinterpret_cast<Implementation::MeshDataHeader64*>(out.data()),
            mesh, endianSwapNeeded);
        offset += sizeof(Implementation::MeshDataHeader64);
    }

    /* Fill in the attributes */
    for(UnsignedInt i = 0; i != mesh.attributeCount(); ++i) {
        if(is32bit) {
            fillMeshAttribute(
                *reinterpret_cast<Implementation::MeshAttributeData32*>(out.data()+ offset),
                mesh, i, endianSwapNeeded);
            offset += sizeof(Implementation::MeshAttributeData32);
        } else {
            fillMeshAttribute(
                *reinterpret_cast<Implementation::MeshAttributeData64*>(out.data()+ offset),
                mesh, i, endianSwapNeeded);
            offset += sizeof(Implementation::MeshAttributeData64);
        }
    }

    /* Copy index data, if any; perform endian swap */
    if(mesh.isIndexed()) {
        auto indexData = out.slice(offset, offset + mesh.indexData().size());
        Utility::copy(mesh.indexData(), indexData);
        if(endianSwapNeeded) {
            if(mesh.indexType() == MeshIndexType::UnsignedInt)
                Utility::Endianness::swapInPlace(Containers::arrayCast<UnsignedInt>(indexData));
            else if(mesh.indexType() == MeshIndexType::UnsignedShort)
                Utility::Endianness::swapInPlace(Containers::arrayCast<UnsignedShort>(indexData));
            else CORRADE_INTERNAL_ASSERT(mesh.indexType() == MeshIndexType::UnsignedByte);
        }

        offset += mesh.indexData().size();
    }

    /* Copy vertex data, if any; perform endian swap */
    Utility::copy(mesh.vertexData(), out.slice(offset, offset + mesh.vertexData().size()));
    if(endianSwapNeeded) {
        for(UnsignedInt i = 0, maxI = mesh.attributeCount(); i != maxI; ++i) {
            const VertexFormat format = mesh.attributeFormat(i);
            if(isVertexFormatImplementationSpecific(format)) {
                Error{} << "Trade::MagnumSceneConverter::convertToData(): cannot perform endian swap on" << format;
                return nullptr;
            }

            const UnsignedInt componentSize = vertexFormatSize(vertexFormatComponentFormat(format));
            if(componentSize == 1) continue;

            const Containers::StridedArrayView3D<char> data{
                out, out.data() + offset + mesh.attributeOffset(i),
                {vertexFormatVectorCount(format),
                 vertexFormatComponentCount(format),
                 mesh.vertexCount()},
                {std::ptrdiff_t(vertexFormatVectorStride(format)),
                 componentSize,
                 mesh.attributeStride(i)}};
            for(Containers::StridedArrayView2D<char> vector: data) {
                for(Containers::StridedArrayView1D<char> component: vector) {
                    if(componentSize == 8)
                        Utility::Endianness::swapInPlace(Containers::arrayCast<UnsignedLong>(component));
                    else if(componentSize == 4)
                        Utility::Endianness::swapInPlace(Containers::arrayCast<UnsignedInt>(component));
                    else if(componentSize == 2)
                        Utility::Endianness::swapInPlace(Containers::arrayCast<UnsignedShort>(component));
                    else CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
                }
            }
        }
    }

    offset += mesh.vertexData().size();
    CORRADE_INTERNAL_ASSERT(offset == size);
    return out;
}

}}

CORRADE_PLUGIN_REGISTER(MagnumSceneConverter, Magnum::Trade::MagnumSceneConverter,
    "cz.mosra.magnum.Trade.AbstractSceneConverter/0.1")
