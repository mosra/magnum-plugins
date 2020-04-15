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

#include "MagnumImporter.h"

#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/EndiannessBatch.h>
#include <Magnum/Trade/MeshData.h>

#include "MagnumPlugins/MagnumImporter/Implementation/DataLayout.h"

namespace Magnum { namespace Trade {

struct MagnumImporter::State {
    Containers::Array<char> in;
    DataChunkSignature signature;
    DataChunkType type;
    UnsignedShort typeVersion;
};

MagnumImporter::MagnumImporter() = default;

MagnumImporter::MagnumImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

MagnumImporter::~MagnumImporter() = default;

ImporterFeatures MagnumImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool MagnumImporter::doIsOpened() const { return !!_state; }

void MagnumImporter::doClose() { _state = nullptr; }

namespace {

template<class T> void extractHeader(T& header, UnsignedLong& size, UnsignedShort& typeVersion, const bool endianSwapNeeded) {
    decltype(header.size) sizeOriginal = header.size;
    typeVersion = header.typeVersion;

    if(endianSwapNeeded) Utility::Endianness::swapInPlace(sizeOriginal, typeVersion);

    size = sizeOriginal;
}

}

void MagnumImporter::doOpenData(Containers::ArrayView<const char> data) {
    if(data.size() < 20) {
        Error{} << "Trade::MagnumImporter::openData(): file too short, expected at least 20 bytes for a header but got" << data.size();
        return;
    }

    /* Only first 20 bytes is valid */
    const DataChunkHeader& header = *reinterpret_cast<const DataChunkHeader*>(data.data());
    if(header.version != 128) {
        Error{} << "Trade::MagnumImporter::openData(): expected version 128 but got" << header.version;
        return;
    }
    if(header.eolUnix[0] != '\x0a' ||
       header.eolDos[0] != '\x0d' ||
       header.eolDos[1] != '\x0a' ||
       header.zero != 0) {
        Error{} << "Trade::MagnumImporter::openData(): invalid header check bytes";
        return;
    }

    /* Decide on endian swapping. The actual check for header signature
       validity is below, so this might have a arbitrary value in case the
       signature is invalid. */
    const bool endianSwapNeeded = Utility::Endianness::isBigEndian() !=
        (header.signature == DataChunkSignature::Big32 ||
         header.signature == DataChunkSignature::Big64);

    /* Now decide how to parse the rest depending on the signature */
    UnsignedLong size;
    UnsignedShort typeVersion;
    if(header.signature == DataChunkSignature::Little32 ||
       header.signature == DataChunkSignature::Big32) {

        extractHeader(*reinterpret_cast<const Implementation::DataChunkHeader32*>(data.data()), size, typeVersion, endianSwapNeeded);

    } else if(header.signature == DataChunkSignature::Little64 ||
              header.signature == DataChunkSignature::Big64) {
        if(data.size() < 24) {
            Error{} << "Trade::MagnumImporter::openData(): 64-bit file too short, expected at least 24 bytes for a header but got" << data.size();
            return;
        }

        extractHeader(*reinterpret_cast<const Implementation::DataChunkHeader64*>(data.data()), size, typeVersion, endianSwapNeeded);

        if(size > ~std::size_t{}) {
            Error{} << "Trade::MagnumImporter::openData(): chunk size" << size << "too large to process on a 32-bit platform";
            return;
        }

    } else {
        Error{} << "Trade::MagnumImporter::openData(): invalid signature" << header.signature;
        return;
    }

    if(size > data.size()) {
        Error{} << "Trade::MagnumImporter::openData(): file too short, expected at least" << size << "bytes but got" << data.size();
        return;
    }

    _state.emplace();
    _state->signature = header.signature;
    _state->type = header.type;
    _state->typeVersion = typeVersion;
    _state->in = Containers::Array<char>{Containers::NoInit, std::size_t(size)};
    Utility::copy(data.prefix(size), _state->in);

    if(_state->type != DataChunkType::Mesh) {
        Warning{} << "Trade::MagnumImporter::openData(): ignoring unknown chunk" << _state->type;
    }
}

UnsignedInt MagnumImporter::doMeshCount() const {
    return _state->type == DataChunkType::Mesh ? 1 : 0;
}

namespace {

/* Essentially the same what's done in MeshData::deserialize() with endian
   swapping (and thus inevitable copies) on top */
template<class MeshDataHeader, class MeshAttributeData> Containers::Optional<MeshData> extractMesh(Containers::ArrayView<const char> data, bool endianSwapNeeded) {
    if(data.size() < sizeof(MeshDataHeader)) {
        Error{} << "Trade::MagnumImporter::mesh(): expected at least a" << sizeof(MeshDataHeader) << Debug::nospace << "-byte chunk for a header but got" << data.size();
        return Containers::NullOpt;
    }

    /* Get a mutable copy of the mesh data header, endian-swap if necessary */
    MeshDataHeader header = *reinterpret_cast<const MeshDataHeader*>(data.data());
    if(endianSwapNeeded) Utility::Endianness::swapInPlace(header.vertexCount,
        header.indexCount, header.primitive, header.indexOffset,
        header.attributeCount, header.indexDataSize, header.vertexDataSize);

    /* Check that everything can fit */
    const std::size_t size = sizeof(MeshDataHeader) + header.attributeCount*sizeof(MeshAttributeData) + header.indexDataSize + header.vertexDataSize;
    if(data.size() != size) {
        Error{} << "Trade::MagnumImporter::mesh(): expected a" << size << Debug::nospace << "-byte chunk but got" << data.size();
        return Containers::NullOpt;
    }

    /* Make a mutable copy of vertex data. Endian swapping is done while
       parsing attributes. */
    Containers::Array<char> vertexData{Containers::NoInit, std::size_t(header.vertexDataSize)};
    Utility::copy(Containers::ArrayView<const char>{reinterpret_cast<const char*>(data.data()) + sizeof(MeshDataHeader) + header.attributeCount*sizeof(MeshAttributeData) + header.indexDataSize, std::size_t(header.vertexDataSize)}, vertexData);

    /* Make a mutable copy of all index data, check bounds and endian-swap if
       needed */
    /** @todo this will assert on invalid index type */
    Containers::Array<char> indexData;
    MeshIndexData indices;
    if(header.indexType != MeshIndexType{}) {
        const std::size_t indexEnd = header.indexOffset + header.indexCount*meshIndexTypeSize(header.indexType);
        if(indexEnd > header.indexDataSize) {
            Error{} << "Trade::MagnumImporter::mesh(): indices [" <<  Debug::nospace << header.indexOffset << Debug::nospace << ":" << Debug::nospace << indexEnd << Debug::nospace << "] out of range for" << header.indexDataSize << "bytes of index data";
            return Containers::NullOpt;
        }

        indexData = Containers::Array<char>{Containers::NoInit, std::size_t(header.indexDataSize)};
        Utility::copy(Containers::ArrayView<const char>{reinterpret_cast<const char*>(data.data()) + sizeof(MeshDataHeader) + header.attributeCount*sizeof(MeshAttributeData), std::size_t(header.indexDataSize)}, indexData);
        if(endianSwapNeeded) {
            if(header.indexType == MeshIndexType::UnsignedInt)
                Utility::Endianness::swapInPlace(Containers::arrayCast<UnsignedInt>(indexData));
            else if(header.indexType == MeshIndexType::UnsignedShort)
                Utility::Endianness::swapInPlace(Containers::arrayCast<UnsignedShort>(indexData));
            else CORRADE_INTERNAL_ASSERT(header.indexType == MeshIndexType::UnsignedByte);
        }

        indices = MeshIndexData{header.indexType, indexData.suffix(header.indexOffset)};
    }

    /* Parse attributes, endian-swap vertex data */
    Containers::Array<Trade::MeshAttributeData> attributeData{header.attributeCount};
    const auto attributes = Containers::arrayCast<const MeshAttributeData>(
        Containers::arrayCast<const char>(data).slice(sizeof(MeshDataHeader),
        sizeof(MeshDataHeader) + header.attributeCount*sizeof(MeshAttributeData)));
    for(std::size_t i = 0; i != attributes.size(); ++i) {
        MeshAttributeData attribute = attributes[i];

        /* Get a mutable copy of attribute data, endian-swap if needed */
        if(endianSwapNeeded) Utility::Endianness::swapInPlace(attribute.format,
            attribute.name, attribute.vertexCount, attribute.stride,
            attribute.arraySize, attribute.offset);

        /** @todo this will assert on invalid vertex format */
        /** @todo check also consistency of vertex count and _isOffsetOnly? */
        /* Check that the view fits into the provided vertex data array. For
           implementation-specific formats we don't know the size so use 0 to
           check at least partially. */
        const UnsignedInt typeSize =
            isVertexFormatImplementationSpecific(attribute.format) ? 0 :
            vertexFormatSize(attribute.format);
        const std::size_t attributeEnd = attribute.offset + (header.vertexCount - 1)*attribute.stride + typeSize;
        if(header.vertexCount && attributeEnd > header.vertexDataSize) {
            Error{} << "Trade::MagnumImporter::mesh(): attribute" << i << "[" << Debug::nospace << attribute.offset << Debug::nospace << ":" << Debug::nospace << attributeEnd << Debug::nospace << "] out of range for" << header.vertexDataSize << "bytes of vertex data";
            return Containers::NullOpt;
        }

        if(endianSwapNeeded) {
            if(isVertexFormatImplementationSpecific(attribute.format)) {
                Error{} << "Trade::MagnumImporter::mesh(): cannot perform endian swap on" << attribute.format;
                return Containers::NullOpt;
            }

            const UnsignedInt componentSize = vertexFormatSize(vertexFormatComponentFormat(attribute.format));
            if(componentSize != 1) {
                const Containers::StridedArrayView3D<char> data{
                    vertexData, vertexData.data() + attribute.offset,
                    {vertexFormatVectorCount(attribute.format),
                     vertexFormatComponentCount(attribute.format),
                     header.vertexCount},
                    {std::ptrdiff_t(vertexFormatVectorStride(attribute.format)),
                     componentSize,
                     attribute.stride}};
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

        attributeData[i] = Trade::MeshAttributeData{
            attribute.name, attribute.format, attribute.arraySize,
            Containers::StridedArrayView1D<const void>{vertexData, vertexData.data() + attribute.offset, header.vertexCount, attribute.stride}};
    }

    return MeshData{header.primitive,
        std::move(indexData), indices,
        std::move(vertexData), std::move(attributeData), header.vertexCount};
}

}

Containers::Optional<MeshData> MagnumImporter::doMesh(UnsignedInt, UnsignedInt) {
    if(_state->typeVersion != 0) {
        Error{} << "Trade::MagnumImporter::mesh(): invalid chunk type version, expected 0 but got" << _state->typeVersion;
        return Containers::NullOpt;
    }

    const bool endianSwapNeeded = Utility::Endianness::isBigEndian() !=
        (_state->signature == DataChunkSignature::Big32 ||
         _state->signature == DataChunkSignature::Big64);

    if(_state->signature == DataChunkSignature::Little32 ||
       _state->signature == DataChunkSignature::Big32)
        return extractMesh<Implementation::MeshDataHeader32, Implementation::MeshAttributeData32>(_state->in, endianSwapNeeded);
    else if(_state->signature == DataChunkSignature::Little64 ||
            _state->signature == DataChunkSignature::Big64)
        return extractMesh<Implementation::MeshDataHeader64, Implementation::MeshAttributeData64>(_state->in, endianSwapNeeded);
    else CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

}}

CORRADE_PLUGIN_REGISTER(MagnumImporter, Magnum::Trade::MagnumImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.1")
