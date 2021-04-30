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

#include "MeshOptimizerSceneConverter.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Reference.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Magnum/Math/PackingBatch.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/MeshTools/Combine.h>
#include <Magnum/MeshTools/Duplicate.h>
#include <Magnum/MeshTools/GenerateIndices.h>
#include <Magnum/MeshTools/Interleave.h>
#include <Magnum/MeshTools/Reference.h>
#include <Magnum/Trade/ArrayAllocator.h>
#include <Magnum/Trade/MeshData.h>
#include <meshoptimizer.h>

namespace Magnum { namespace Trade {

MeshOptimizerSceneConverter::MeshOptimizerSceneConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractSceneConverter{manager, plugin} {}

MeshOptimizerSceneConverter::~MeshOptimizerSceneConverter() = default;

SceneConverterFeatures MeshOptimizerSceneConverter::doFeatures() const {
    return SceneConverterFeature::ConvertMeshInPlace|SceneConverterFeature::ConvertMesh;
}

namespace {

template<class T> void analyze(const MeshData& mesh, const Utility::ConfigurationGroup& configuration, const UnsignedInt vertexSize, const Containers::StridedArrayView1D<const Vector3> positions, meshopt_VertexCacheStatistics& vertexCacheStats, meshopt_VertexFetchStatistics& vertexFetchStats, meshopt_OverdrawStatistics& overdrawStats) {
    const auto indices = mesh.indices<T>();
    vertexCacheStats = meshopt_analyzeVertexCache(indices.data(), mesh.indexCount(), mesh.vertexCount(), configuration.value<UnsignedInt>("analyzeCacheSize"), configuration.value<UnsignedInt>("analyzeWarpSize"), configuration.value<UnsignedInt>("analyzePrimitiveGroupSize"));
    if(vertexSize) vertexFetchStats = meshopt_analyzeVertexFetch(indices.data(), mesh.indexCount(), mesh.vertexCount(), vertexSize);
    if(positions) overdrawStats = meshopt_analyzeOverdraw(indices.data(), mesh.indexCount(), static_cast<const float*>(positions.data()), mesh.vertexCount(), positions.stride());
}

void analyze(const MeshData& mesh, const Utility::ConfigurationGroup& configuration, const Containers::StridedArrayView1D<const Vector3> positions, Containers::Optional<UnsignedInt>& vertexSize, meshopt_VertexCacheStatistics& vertexCacheStats, meshopt_VertexFetchStatistics& vertexFetchStats, meshopt_OverdrawStatistics& overdrawStats) {
    /* Calculate vertex size out of all attributes. If any attribute is
       implementation-specific, do nothing (warning will be printed by the
       caller) */
    if(!vertexSize) {
        vertexSize = 0;
        for(UnsignedInt i = 0; i != mesh.attributeCount(); ++i) {
            VertexFormat format = mesh.attributeFormat(i);
            const UnsignedInt arraySize = mesh.attributeArraySize(i);
            if(isVertexFormatImplementationSpecific(format)) {
                vertexSize = 0;
                break;
            }
            *vertexSize += vertexFormatSize(format)*(arraySize ? arraySize : 1);
        }
    }

    if(mesh.indexType() == MeshIndexType::UnsignedInt)
        analyze<UnsignedInt>(mesh, configuration, *vertexSize, positions, vertexCacheStats, vertexFetchStats, overdrawStats);
    else if(mesh.indexType() == MeshIndexType::UnsignedShort)
        analyze<UnsignedShort>(mesh, configuration, *vertexSize, positions, vertexCacheStats, vertexFetchStats, overdrawStats);
    else if(mesh.indexType() == MeshIndexType::UnsignedByte)
        analyze<UnsignedByte>(mesh, configuration, *vertexSize, positions, vertexCacheStats, vertexFetchStats, overdrawStats);
    else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

void analyzePost(const char* prefix, const MeshData& mesh, const Utility::ConfigurationGroup& configuration, const Containers::StridedArrayView1D<const Vector3> positions, Containers::Optional<UnsignedInt>& vertexSize, meshopt_VertexCacheStatistics& vertexCacheStatsBefore, meshopt_VertexFetchStatistics& vertexFetchStatsBefore, meshopt_OverdrawStatistics& overdrawStatsBefore) {
    /* If vertex size is zero, it means there was an implementation-specific
       vertex format somewhere. Print a warning about that. */
    CORRADE_INTERNAL_ASSERT(vertexSize);
    if(!*vertexSize) for(UnsignedInt i = 0; i != mesh.attributeCount(); ++i) {
        VertexFormat format = mesh.attributeFormat(i);
        if(isVertexFormatImplementationSpecific(format)) {
            Warning{} << prefix << "can't analyze vertex fetch for" << format;
            break;
        }
    }

    meshopt_VertexCacheStatistics vertexCacheStats;
    meshopt_VertexFetchStatistics vertexFetchStats;
    meshopt_OverdrawStatistics overdrawStats;
    analyze(mesh, configuration, positions, vertexSize, vertexCacheStats, vertexFetchStats, overdrawStats);

    Debug{} << prefix << "processing stats:";
    Debug{} << "  vertex cache:\n   "
        << vertexCacheStatsBefore.vertices_transformed << "->"
        << vertexCacheStats.vertices_transformed
        << "transformed vertices\n   "
        << vertexCacheStatsBefore.warps_executed << "->"
        << vertexCacheStats.warps_executed << "executed warps\n    ACMR"
        << vertexCacheStatsBefore.acmr << "->" << vertexCacheStats.acmr
        << Debug::newline << "    ATVR" << vertexCacheStatsBefore.atvr
        << "->" << vertexCacheStats.atvr;
    if(*vertexSize) Debug{} << "  vertex fetch:\n   "
        << vertexFetchStatsBefore.bytes_fetched << "->"
        << vertexFetchStats.bytes_fetched << "bytes fetched\n    overfetch"
        << vertexFetchStatsBefore.overfetch << "->"
        << vertexFetchStats.overfetch;
    if(positions) Debug{} << "  overdraw:\n   "
        << overdrawStatsBefore.pixels_shaded << "->"
        << overdrawStats.pixels_shaded << "shaded pixels\n   "
        << overdrawStatsBefore.pixels_covered << "->"
        << overdrawStats.pixels_covered << "covered pixels\n    overdraw"
        << overdrawStatsBefore.overdraw << "->" << overdrawStats.overdraw;
}

void populatePositions(const MeshData& mesh, Containers::Array<Vector3>& positionStorage, Containers::StridedArrayView1D<const Vector3>& positions) {
    /* MeshOptimizer accepts float positions with stride divisible by four. If
       the input doesn't have that (for example because it's a tightly-packed
       PLY with 24bit RGB colors), we need to supply unpacked aligned copy. */
    if(mesh.attributeFormat(MeshAttribute::Position) == VertexFormat::Vector3 && mesh.attributeStride(MeshAttribute::Position) % 4 == 0)
        positions = mesh.attribute<Vector3>(MeshAttribute::Position);
    else {
        positionStorage = mesh.positions3DAsArray();
        positions = positionStorage;
    }
}

bool convertInPlaceInternal(const char* prefix, MeshData& mesh, const SceneConverterFlags flags, const Utility::ConfigurationGroup& configuration, Containers::Array<Vector3>& positionStorage, Containers::StridedArrayView1D<const Vector3>& positions, Containers::Optional<UnsignedInt>& vertexSize,  meshopt_VertexCacheStatistics& vertexCacheStatsBefore, meshopt_VertexFetchStatistics& vertexFetchStatsBefore, meshopt_OverdrawStatistics& overdrawStatsBefore) {
    /* Only doConvert() can handle triangle strips etc, in-place only triangles */
    if(mesh.primitive() != MeshPrimitive::Triangles) {
        Error{} << prefix << "expected a triangle mesh, got" << mesh.primitive();
        return false;
    }

    /* Can't really do anything with non-indexed meshes, sorry */
    if(!mesh.isIndexed()) {
        Error{} << prefix << "expected an indexed mesh";
        return false;
    }

    /* If we need it, get the position attribute, unpack if packed. It's used
       by the verbose stats also but in that case the processing shouldn't fail
       if there are no positions -- so check the hasAttribute() earlier. */
    if((flags & SceneConverterFlag::Verbose && mesh.hasAttribute(MeshAttribute::Position)) ||
       configuration.value<bool>("optimizeOverdraw") ||
       configuration.value<bool>("simplify") ||
       configuration.value<bool>("simplifySloppy"))
    {
        if(!mesh.hasAttribute(MeshAttribute::Position)) {
            Error{} << prefix << "optimizeOverdraw and simplify require the mesh to have positions";
            return false;
        }

        populatePositions(mesh, positionStorage, positions);
    }

    /* Save "before" stats if verbose output is requested. No messages as those
       will be printed only at the end if the processing passes. */
    if(flags & SceneConverterFlag::Verbose) {
        analyze(mesh, configuration, positions, vertexSize, vertexCacheStatsBefore, vertexFetchStatsBefore, overdrawStatsBefore);
    }

    /* Vertex cache optimization. Goes first. */
    if(configuration.value<bool>("optimizeVertexCache")) {
        if(mesh.indexType() == MeshIndexType::UnsignedInt) {
            Containers::ArrayView<UnsignedInt> indices = mesh.mutableIndices<UnsignedInt>();
            meshopt_optimizeVertexCache(indices.data(), indices.data(), mesh.indexCount(), mesh.vertexCount());
        } else if(mesh.indexType() == MeshIndexType::UnsignedShort) {
            Containers::ArrayView<UnsignedShort> indices = mesh.mutableIndices<UnsignedShort>();
            meshopt_optimizeVertexCache(indices.data(), indices.data(), mesh.indexCount(), mesh.vertexCount());
        } else if(mesh.indexType() == MeshIndexType::UnsignedByte) {
            Containers::ArrayView<UnsignedByte> indices = mesh.mutableIndices<UnsignedByte>();
            meshopt_optimizeVertexCache(indices.data(), indices.data(), mesh.indexCount(), mesh.vertexCount());
        } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }

    /* Overdraw optimization. Goes after vertex cache optimization. */
    if(configuration.value<bool>("optimizeOverdraw")) {
        const Float optimizeOverdrawThreshold = configuration.value<Float>("optimizeOverdrawThreshold");

        if(mesh.indexType() == MeshIndexType::UnsignedInt) {
            Containers::ArrayView<UnsignedInt> indices = mesh.mutableIndices<UnsignedInt>();
            meshopt_optimizeOverdraw(indices.data(), indices.data(), mesh.indexCount(), static_cast<const Float*>(positions.data()), mesh.vertexCount(), positions.stride(), optimizeOverdrawThreshold);
        } else if(mesh.indexType() == MeshIndexType::UnsignedShort) {
            Containers::ArrayView<UnsignedShort> indices = mesh.mutableIndices<UnsignedShort>();
            meshopt_optimizeOverdraw(indices.data(), indices.data(), mesh.indexCount(), static_cast<const Float*>(positions.data()), mesh.vertexCount(), positions.stride(), optimizeOverdrawThreshold);
        } else if(mesh.indexType() == MeshIndexType::UnsignedByte) {
            Containers::ArrayView<UnsignedByte> indices = mesh.mutableIndices<UnsignedByte>();
            meshopt_optimizeOverdraw(indices.data(), indices.data(), mesh.indexCount(), static_cast<const Float*>(positions.data()), mesh.vertexCount(), positions.stride(), optimizeOverdrawThreshold);
        } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }

    /* Vertex fetch optimization. Goes after overdraw optimization. Reorders
       the vertex buffer for better memory locality, so if we have no
       attributes it's of no use (also meshoptimizer asserts in that case).
       Skipping silently instead of failing hard, as an attribute-less mesh
       always *is* optimized for vertex fetch, so there's nothing wrong. */
    if(configuration.value<bool>("optimizeVertexFetch") && mesh.attributeCount()) {
        /* This assumes the mesh is interleaved. doConvert() already ensures
           that, doConvertInPlace() has a runtime check */
        Containers::StridedArrayView2D<char> interleavedData = MeshTools::interleavedMutableData(mesh);

        if(mesh.indexType() == MeshIndexType::UnsignedInt) {
            Containers::ArrayView<UnsignedInt> indices = mesh.mutableIndices<UnsignedInt>();
            meshopt_optimizeVertexFetch(interleavedData.data(), indices.data(), mesh.indexCount(), interleavedData.data(), mesh.vertexCount(), interleavedData.stride()[0]);
        } else if(mesh.indexType() == MeshIndexType::UnsignedShort) {
            Containers::ArrayView<UnsignedShort> indices = mesh.mutableIndices<UnsignedShort>();
            meshopt_optimizeVertexFetch(interleavedData.data(), indices.data(), mesh.indexCount(), interleavedData.data(), mesh.vertexCount(), interleavedData.stride()[0]);
        } else if(mesh.indexType() == MeshIndexType::UnsignedByte) {
            Containers::ArrayView<UnsignedByte> indices = mesh.mutableIndices<UnsignedByte>();
            meshopt_optimizeVertexFetch(interleavedData.data(), indices.data(), mesh.indexCount(), interleavedData.data(), mesh.vertexCount(), interleavedData.stride()[0]);
        } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }

    return true;
}

}

bool MeshOptimizerSceneConverter::doConvertInPlace(MeshData& mesh) {
    if((configuration().value<bool>("optimizeVertexCache") ||
        configuration().value<bool>("optimizeOverdraw") ||
        configuration().value<bool>("optimizeVertexFetch")) &&
       !(mesh.indexDataFlags() & DataFlag::Mutable))
    {
        Error{} << "Trade::MeshOptimizerSceneConverter::convertInPlace(): optimizeVertexCache, optimizeOverdraw and optimizeVertexFetch require index data to be mutable";
        return false;
    }

    if(configuration().value<bool>("optimizeVertexFetch")) {
        if(!(mesh.vertexDataFlags() & DataFlag::Mutable)) {
            Error{} << "Trade::MeshOptimizerSceneConverter::convertInPlace(): optimizeVertexFetch requires vertex data to be mutable";
            return false;
        }

        if(!MeshTools::isInterleaved(mesh)) {
            Error{} << "Trade::MeshOptimizerSceneConverter::convertInPlace(): optimizeVertexFetch requires the mesh to be interleaved";
            return false;
        }
    }


    if(configuration().value<bool>("simplify") ||
       configuration().value<bool>("simplifySloppy"))
    {
        Error{} << "Trade::MeshOptimizerSceneConverter::convertInPlace(): mesh simplification can't be performed in-place, use convert() instead";
        return false;
    }

    meshopt_VertexCacheStatistics vertexCacheStatsBefore;
    meshopt_VertexFetchStatistics vertexFetchStatsBefore;
    meshopt_OverdrawStatistics overdrawStatsBefore;
    Containers::Array<Vector3> positionStorage;
    Containers::StridedArrayView1D<const Vector3> positions;
    Containers::Optional<UnsignedInt> vertexSize;
    if(!convertInPlaceInternal("Trade::MeshOptimizerSceneConverter::convertInPlace():", mesh, flags(), configuration(), positionStorage, positions, vertexSize, vertexCacheStatsBefore, vertexFetchStatsBefore, overdrawStatsBefore))
        return false;

    if(flags() & SceneConverterFlag::Verbose)
        analyzePost("Trade::MeshOptimizerSceneConverter::convertInPlace():", mesh, configuration(), positions, vertexSize, vertexCacheStatsBefore, vertexFetchStatsBefore, overdrawStatsBefore);

    return true;
}

Containers::Optional<MeshData> MeshOptimizerSceneConverter::doConvert(const MeshData& mesh) {
    /* Make the mesh interleaved and owned first */
    MeshData out = MeshTools::owned(MeshTools::interleave(mesh));
    CORRADE_INTERNAL_ASSERT(MeshTools::isInterleaved(out));

    /* Convert to an indexed triangle mesh if we have a strip or a fan */
    if(out.primitive() == MeshPrimitive::TriangleStrip || out.primitive() == MeshPrimitive::TriangleFan) {
        if(out.isIndexed()) out = MeshTools::duplicate(out);
        out = MeshTools::generateIndices(std::move(out));
    }

    meshopt_VertexCacheStatistics vertexCacheStatsBefore;
    meshopt_VertexFetchStatistics vertexFetchStatsBefore;
    meshopt_OverdrawStatistics overdrawStatsBefore;
    Containers::Array<Vector3> positionStorage;
    Containers::StridedArrayView1D<const Vector3> positions;
    Containers::Optional<UnsignedInt> vertexSize;
    if(!convertInPlaceInternal("Trade::MeshOptimizerSceneConverter::convert():", out, flags(), configuration(), positionStorage, positions, vertexSize, vertexCacheStatsBefore, vertexFetchStatsBefore, overdrawStatsBefore))
        return Containers::NullOpt;

    if(configuration().value<bool>("simplify") ||
       configuration().value<bool>("simplifySloppy"))
    {
        const UnsignedInt targetIndexCount = out.indexCount()*configuration().value<Float>("simplifyTargetIndexCountThreshold");
        const Float targetError = configuration().value<Float>("simplifyTargetError");

        /* In this case meshoptimizer doesn't provide overloads, so let's do
           this on our side instead */
        Containers::Array<UnsignedInt> inputIndicesStorage;
        Containers::ArrayView<const UnsignedInt> inputIndices;
        if(out.indexType() == MeshIndexType::UnsignedInt)
            inputIndices = out.indices<UnsignedInt>();
        else {
            inputIndicesStorage = out.indicesAsArray();
            inputIndices = inputIndicesStorage;
        }

        Containers::Array<UnsignedInt> outputIndices;
        Containers::arrayResize<Trade::ArrayAllocator>(outputIndices, NoInit, mesh.indexCount());

        UnsignedInt vertexCount;
        if(configuration().value<bool>("simplifySloppy"))
            vertexCount = meshopt_simplifySloppy(outputIndices.data(), inputIndices, out.indexCount(), static_cast<const float*>(positions.data()), out.vertexCount(), positions.stride(), targetIndexCount);
        else
            vertexCount = meshopt_simplify(outputIndices.data(), inputIndices, out.indexCount(), static_cast<const float*>(positions.data()), out.vertexCount(), positions.stride(), targetIndexCount, targetError);

        Containers::arrayResize<Trade::ArrayAllocator>(outputIndices, vertexCount);

        /* Take the original mesh vertex data with the reduced index buffer and
           call combineIndexedAttributes() to throw away the unused vertices */
        /** @todo provide a way to use the new vertices with the original
            vertex buffer for LODs */
        MeshIndexData indices{outputIndices};
        out = Trade::MeshData{out.primitive(),
            Containers::arrayAllocatorCast<char, Trade::ArrayAllocator>(std::move(outputIndices)), indices,
            out.releaseVertexData(), out.releaseAttributeData()};
        out = MeshTools::combineIndexedAttributes({out});

        /* If we're printing stats after, repopulate the positions to avoid
           using a now-gone array */
        if(flags() & SceneConverterFlag::Verbose)
            populatePositions(out, positionStorage, positions);
    }

    /* Print before & after stats if verbose output is requested */
    if(flags() & SceneConverterFlag::Verbose)
        analyzePost("Trade::MeshOptimizerSceneConverter::convert():", out, configuration(), positions, vertexSize, vertexCacheStatsBefore, vertexFetchStatsBefore, overdrawStatsBefore);

    /* GCC 4.8 needs an explicit conversion, otherwise it tries to copy the
       thing and fails */
    return Containers::optional(std::move(out));
}

}}

CORRADE_PLUGIN_REGISTER(MeshOptimizerSceneConverter, Magnum::Trade::MeshOptimizerSceneConverter,
    "cz.mosra.magnum.Trade.AbstractSceneConverter/0.1.1")
