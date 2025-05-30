/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
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

#include "StlImporter.h"

#include <cstring>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/EndiannessBatch.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/MeshData.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

#ifdef MAGNUM_BUILD_DEPRECATED
StlImporter::StlImporter() = default; /* LCOV_EXCL_LINE */
#endif

StlImporter::StlImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

StlImporter::~StlImporter() = default;

ImporterFeatures StlImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool StlImporter::doIsOpened() const {
    /* Only one of these can be populated at a time; if Assimp is present it's
       also opened */
    CORRADE_INTERNAL_ASSERT(!_in || !_assimpImporter || _assimpImporter->isOpened());
    return _in || _assimpImporter;
}

void StlImporter::doClose() {
    _in = Containers::NullOpt;
    _assimpImporter = nullptr;
}

namespace {
    /* In the input file, the triangle is represented by 12 floats (3D normal
       followed by three 3D vertices) and 2 extra bytes. */
    constexpr std::ptrdiff_t InputTriangleStride = 12*4 + 2;
}

void StlImporter::doOpenData(Containers::Array<char>&& data, const DataFlags dataFlags) {
    /* At this point we can't even check if it's an ASCII or binary file, bail
       out */
    if(data.size() < 5) {
        Error{} << "Trade::StlImporter::openData(): file too short, got only" << data.size() << "bytes";
        return;
    }

    if(std::memcmp(data, "solid", 5) == 0) {
        constexpr Containers::StringView plugin = "AssimpImporter"_s;
        /** @todo remove the !manager() once manager-less instantiation is
            removed */
        if(!manager() || !(manager()->load(plugin) & PluginManager::LoadState::Loaded)) {
            Error{} << "Trade::StlImporter::openData(): can't forward an ASCII file to AssimpImporter";
            return;
        }

        if(flags() & ImporterFlag::Verbose)
            Debug{} << "Trade::StlImporter::openData(): forwarding an ASCII file to AssimpImporter";

        /* Instantiate the plugin, propagate flags. STLs can't reference
           external data so file callbacks don't need to be propagated. */
        Containers::Pointer<AbstractImporter> assimpImporter = static_cast<PluginManager::Manager<AbstractImporter>*>(manager())->instantiate(plugin);
        assimpImporter->setFlags(flags());

        /* Try to open the data with AssimpImporter (error output should be
           printed by the plugin itself). All other functions transparently
           forward to that importer instance if it's populated. */
        if(!assimpImporter->openData(data))
            return;

        /* Success, save the instance */
        _assimpImporter = Utility::move(assimpImporter);
        return;
    }

    if(data.size() < 84) {
        Error{} << "Trade::StlImporter::openData(): file too short, expected at least 84 bytes but got" << data.size();
        return;
    }

    const UnsignedInt triangleCount = Utility::Endianness::littleEndian(*reinterpret_cast<const UnsignedInt*>(data + 80));
    const std::size_t expectedSize = InputTriangleStride*triangleCount;
    if(data.size() != 84 + expectedSize) {
        Error{} << "Trade::StlImporter::openData(): file size doesn't match triangle count, expected" << 84 + expectedSize << "but got" << data.size() << "for" << triangleCount << "triangles";
        return;
    }

    /* Take over the existing array or copy the data if we can't */
    if(dataFlags & (DataFlag::Owned|DataFlag::ExternallyOwned))
        _in = Utility::move(data);
    else
        _in = Containers::Array<char>{InPlaceInit, data};
}

UnsignedInt StlImporter::doMeshCount() const {
    if(_assimpImporter)
        return _assimpImporter->meshCount();

    return 1;
}

UnsignedInt StlImporter::doMeshLevelCount(UnsignedInt id) {
    if(_assimpImporter)
        return _assimpImporter->meshLevelCount(id);

    return configuration().value<bool>("perFaceToPerVertex") ? 1 : 2;
}

Containers::Optional<MeshData> StlImporter::doMesh(UnsignedInt id, UnsignedInt level) {
    if(_assimpImporter)
        return _assimpImporter->mesh(id, level);

    /* We either have per-face in the second level or we convert them to
       per-vertex, never both */
    const bool perFaceToPerVertex = configuration().value<bool>("perFaceToPerVertex");
    CORRADE_INTERNAL_ASSERT(!(level == 1 && perFaceToPerVertex));

    Containers::ArrayView<const char> in = _in->exceptPrefix(84);

    /* Make 2D views on input normals and positions */
    const std::size_t triangleCount = in.size()/InputTriangleStride;
    Containers::StridedArrayView2D<const Vector3> inputNormals{in,
        reinterpret_cast<const Vector3*>(in.data() + 0),
        {triangleCount, 1}, {InputTriangleStride, 0}};
    Containers::StridedArrayView2D<const Vector3> inputPositions{in,
        reinterpret_cast<const Vector3*>(in.data() + sizeof(Vector3)),
        {triangleCount, 3}, {InputTriangleStride, sizeof(Vector3)}};

    /* Decide on output vertex stride and attribute count */
    std::size_t vertexCount;
    std::size_t attributeCount = 1;
    std::ptrdiff_t outputVertexStride = 3*4;
    if(level == 0) {
        vertexCount = 3*triangleCount;
        if(perFaceToPerVertex) {
            outputVertexStride += 3*4;
            ++attributeCount;
        }
    } else vertexCount = triangleCount;

    /* Allocate output data */
    Containers::Array<char> vertexData{NoInit, std::size_t(outputVertexStride*vertexCount)};
    Containers::Array<MeshAttributeData> attributeData{attributeCount};

    /* Copy positions */
    std::size_t offset = 0;
    std::size_t attributeIndex = 0;
    if(level == 0) {
        Containers::StridedArrayView2D<Vector3> outputPositions{vertexData,
            reinterpret_cast<Vector3*>(vertexData.data() + offset),
            {triangleCount, 3}, {outputVertexStride*3, outputVertexStride}};
        Utility::copy(inputPositions, outputPositions);
        Containers::StridedArrayView1D<Vector3> positions{vertexData,
            reinterpret_cast<Vector3*>(vertexData.data() + offset),
            vertexCount, outputVertexStride};

        /* Endian conversion. This is needed only on Big-Endian systems, but
           it's enabled always to minimize a risk of accidental breakage when
           we can't test. */
        for(Containers::StridedArrayView1D<Float> component:
            Containers::arrayCast<2, Float>(positions).transposed<0, 1>())
                Utility::Endianness::littleEndianInPlace(component);

        offset += sizeof(Vector3);
        attributeData[attributeIndex++] = MeshAttributeData{MeshAttribute::Position, positions};
    }

    /* Copy normals */
    if(perFaceToPerVertex || level == 1) {
        const std::size_t normalRepeatCount = perFaceToPerVertex ? 3 : 1;
        Containers::StridedArrayView2D<Vector3> outputNormals{vertexData,
            reinterpret_cast<Vector3*>(vertexData.data() + offset),
            {triangleCount, normalRepeatCount},
            {std::ptrdiff_t(outputVertexStride*normalRepeatCount), outputVertexStride}};
        Utility::copy(inputNormals.broadcasted<1>(normalRepeatCount),
            outputNormals);
        Containers::StridedArrayView1D<Vector3> normals{vertexData,
            reinterpret_cast<Vector3*>(vertexData.data() + offset),
            vertexCount, outputVertexStride};

        /* Endian conversion. This is needed only on Big-Endian systems, but
           it's enabled always to minimize a risk of accidental breakage when
           we can't test. */
        for(Containers::StridedArrayView1D<Float> component:
            Containers::arrayCast<2, Float>(normals).transposed<0, 1>())
                Utility::Endianness::littleEndianInPlace(component);

        offset += sizeof(Vector3);
        attributeData[attributeIndex++] = MeshAttributeData{MeshAttribute::Normal, normals};
    }

    CORRADE_INTERNAL_ASSERT(offset == std::size_t(outputVertexStride));
    CORRADE_INTERNAL_ASSERT(attributeIndex == attributeCount);

    return MeshData{level == 0 ? MeshPrimitive::Triangles : MeshPrimitive::Faces,
        Utility::move(vertexData), Utility::move(attributeData)};
}

}}

CORRADE_PLUGIN_REGISTER(StlImporter, Magnum::Trade::StlImporter,
    MAGNUM_TRADE_ABSTRACTIMPORTER_PLUGIN_INTERFACE)
