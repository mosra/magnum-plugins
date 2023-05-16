/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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

#include <Corrade/Containers/Iterable.h>
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
#include <Corrade/Containers/StringView.h>
namespace Magnum { namespace Trade {

MeshOptimizerSceneConverter::MeshOptimizerSceneConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractSceneConverter{manager, plugin} {}

MeshOptimizerSceneConverter::~MeshOptimizerSceneConverter() = default;

SceneConverterFeatures MeshOptimizerSceneConverter::doFeatures() const {
    return SceneConverterFeature::ConvertMeshInPlace|SceneConverterFeature::ConvertMesh;
}

namespace {

template<class T> void analyze(const MeshData& mesh, const Utility::ConfigurationGroup& configuration, const UnsignedInt vertexSize, const Containers::StridedArrayView1D<const Vector3> positions, meshopt_VertexCacheStatistics& vertexCacheStats, meshopt_VertexFetchStatistics& vertexFetchStats, meshopt_OverdrawStatistics& overdrawStats) {
    const auto indices = mesh.indices<T>().asContiguous();
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

void analyzePost(const char* prefix, const MeshData& mesh, const Utility::ConfigurationGroup& configuration, const SceneConverterFlags flags, const Containers::StridedArrayView1D<const Vector3> positions, Containers::Optional<UnsignedInt>& vertexSize, meshopt_VertexCacheStatistics& vertexCacheStatsBefore, meshopt_VertexFetchStatistics& vertexFetchStatsBefore, meshopt_OverdrawStatistics& overdrawStatsBefore) {
    /* If vertex size is zero, it means there was an implementation-specific
       vertex format somewhere. Print a warning about that. */
    CORRADE_INTERNAL_ASSERT(vertexSize);
    if(!*vertexSize) for(UnsignedInt i = 0; i != mesh.attributeCount(); ++i) {
        VertexFormat format = mesh.attributeFormat(i);
        if(isVertexFormatImplementationSpecific(format)) {
            if(!(flags & SceneConverterFlag::Quiet))
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
            Containers::ArrayView<UnsignedInt> indices = mesh.mutableIndices<UnsignedInt>().asContiguous();
            meshopt_optimizeVertexCache(indices.data(), indices.data(), mesh.indexCount(), mesh.vertexCount());
        } else if(mesh.indexType() == MeshIndexType::UnsignedShort) {
            Containers::ArrayView<UnsignedShort> indices = mesh.mutableIndices<UnsignedShort>().asContiguous();
            meshopt_optimizeVertexCache(indices.data(), indices.data(), mesh.indexCount(), mesh.vertexCount());
        } else if(mesh.indexType() == MeshIndexType::UnsignedByte) {
            Containers::ArrayView<UnsignedByte> indices = mesh.mutableIndices<UnsignedByte>().asContiguous();
            meshopt_optimizeVertexCache(indices.data(), indices.data(), mesh.indexCount(), mesh.vertexCount());
        } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }

    /* Overdraw optimization. Goes after vertex cache optimization. */
    if(configuration.value<bool>("optimizeOverdraw")) {
        const Float optimizeOverdrawThreshold = configuration.value<Float>("optimizeOverdrawThreshold");

        if(mesh.indexType() == MeshIndexType::UnsignedInt) {
            Containers::ArrayView<UnsignedInt> indices = mesh.mutableIndices<UnsignedInt>().asContiguous();
            meshopt_optimizeOverdraw(indices.data(), indices.data(), mesh.indexCount(), static_cast<const Float*>(positions.data()), mesh.vertexCount(), positions.stride(), optimizeOverdrawThreshold);
        } else if(mesh.indexType() == MeshIndexType::UnsignedShort) {
            Containers::ArrayView<UnsignedShort> indices = mesh.mutableIndices<UnsignedShort>().asContiguous();
            meshopt_optimizeOverdraw(indices.data(), indices.data(), mesh.indexCount(), static_cast<const Float*>(positions.data()), mesh.vertexCount(), positions.stride(), optimizeOverdrawThreshold);
        } else if(mesh.indexType() == MeshIndexType::UnsignedByte) {
            Containers::ArrayView<UnsignedByte> indices = mesh.mutableIndices<UnsignedByte>().asContiguous();
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
            Containers::ArrayView<UnsignedInt> indices = mesh.mutableIndices<UnsignedInt>().asContiguous();
            meshopt_optimizeVertexFetch(interleavedData.data(), indices.data(), mesh.indexCount(), interleavedData.data(), mesh.vertexCount(), interleavedData.stride()[0]);
        } else if(mesh.indexType() == MeshIndexType::UnsignedShort) {
            Containers::ArrayView<UnsignedShort> indices = mesh.mutableIndices<UnsignedShort>().asContiguous();
            meshopt_optimizeVertexFetch(interleavedData.data(), indices.data(), mesh.indexCount(), interleavedData.data(), mesh.vertexCount(), interleavedData.stride()[0]);
        } else if(mesh.indexType() == MeshIndexType::UnsignedByte) {
            Containers::ArrayView<UnsignedByte> indices = mesh.mutableIndices<UnsignedByte>().asContiguous();
            meshopt_optimizeVertexFetch(interleavedData.data(), indices.data(), mesh.indexCount(), interleavedData.data(), mesh.vertexCount(), interleavedData.stride()[0]);
        } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }

    if((configuration.value<bool>("encodeVertex") || configuration.value<bool>("decodeVertex")) && (mesh.vertexData().size() > 256)) {
        Error{} << prefix << "Compression and decompression is not possible with vertex data bigger than 256 bytes";
        return false;
    }

    if(configuration.value<bool>("encodeVertex")) {
        unsigned int encoded_attribute_count = 0;
        for(UnsignedInt i = 0; i < mesh.attributeCount(); ++i) {
            if(isVertexFormatImplementationSpecific(mesh.attributeFormat(i))) encoded_attribute_count++;
        }
        if(encoded_attribute_count == mesh.attributeCount()) {
            Error{} << prefix << "Compression is not possible when all attributes are in implementation-specific format";
            return false;
        }
    }

    if(configuration.value<bool>("decodeVertex")) {
        for(UnsignedInt i = 0; i < mesh.attributeCount(); ++i) {
            if(!isVertexFormatImplementationSpecific(mesh.attributeFormat(i))) {
                Error{} << prefix << "Decompression of the vertex data requires all attributes to be compressed";
                return false;
            }
        }
    }

    if((configuration.value<bool>("encodeIndex") || configuration.value<bool>("decodeIndex")) && mesh.indexCount()%3 != 0) {
        Error{} << prefix << "Only the index data mapped to triangles -hence divisible by 3- may be de/compressed";
        return false;
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

    /* Errors for non-indexed meshes and implementation-specific index buffers
       are printed directly in convertInPlaceInternal() */
    if(mesh.isIndexed()) {
        if(isMeshIndexTypeImplementationSpecific(mesh.indexType())) {
            Error{} << "Trade::MeshOptimizerSceneConverter::convertInPlace(): can't perform any operation on an implementation-specific index type" << reinterpret_cast<void*>(meshIndexTypeUnwrap(mesh.indexType()));
            return false;
        }

        if(Short(meshIndexTypeSize(mesh.indexType())) != mesh.indexStride()) {
            Error{} << "Trade::MeshOptimizerSceneConverter::convertInPlace(): in-place conversion is possible only with contiguous index buffers";
            return false;
        }
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
        analyzePost("Trade::MeshOptimizerSceneConverter::convertInPlace():", mesh, configuration(), flags(), positions, vertexSize, vertexCacheStatsBefore, vertexFetchStatsBefore, overdrawStatsBefore);

    return true;
}

namespace {

/* As of version 0.18 (https://github.com/zeux/meshoptimizer/pull/432), the
   meshopt_simplify() has a new second-to-last options argument, this pair of
   templates adapts to that -- by either passing the function pointer through
   unchanged or by wrapping it in a lambda that ignores the options argument
   (and the result_error on versions before 0.16) */
/** @todo switch to #if MESHOPTIMIZER_VERSION >= 180 once enough time passes
    (released on 2022-08-01) */
template<std::size_t(*F)(UnsignedInt*, const UnsignedInt*, std::size_t, const Float*, std::size_t, std::size_t, std::size_t, Float
    #if MESHOPTIMIZER_VERSION >= 160
    , Float*
    #endif
)> auto adaptMeshOptSimplifySignature() -> std::size_t(*)(UnsignedInt*, const UnsignedInt*, std::size_t, const Float*, std::size_t, std::size_t, std::size_t, Float, UnsignedInt, Float*) {
    return [](UnsignedInt* destination, const UnsignedInt* indices, size_t index_count, const float* vertex_positions, size_t vertex_count, size_t vertex_positions_stride, size_t target_index_count, float target_error, unsigned int, float*
        #if MESHOPTIMIZER_VERSION >= 160
        result_error
        #endif
    ) {
        return F(destination, indices, index_count, vertex_positions, vertex_count, vertex_positions_stride, target_index_count, target_error
            #if MESHOPTIMIZER_VERSION >= 160
            , result_error
            #endif
        );
    };
}

template<std::size_t(*F)(UnsignedInt*, const UnsignedInt*, std::size_t, const Float*, std::size_t, std::size_t, std::size_t, Float, UnsignedInt, Float*)> auto adaptMeshOptSimplifySignature() -> std::size_t(*)(UnsignedInt*, const UnsignedInt*, std::size_t, const Float*, std::size_t, std::size_t, std::size_t, Float, UnsignedInt, Float*) {
    return F;
}

}

std::vector<unsigned char> encodeVertex(const MeshData& mesh) {
    auto owned_mesh = MeshTools::owned(mesh);
    std::vector<unsigned char> buffer;
    if(MeshTools::isInterleaved(mesh)) {
        buffer.resize(meshopt_encodeVertexBufferBound(owned_mesh.vertexCount(), owned_mesh.attributeStride(0)));
        buffer.resize(meshopt_encodeVertexBuffer(&buffer[0], buffer.size(), owned_mesh.vertexData(), owned_mesh.vertexCount(),
                                                 owned_mesh.attributeStride(0)));
    } else {
        buffer.reserve(owned_mesh.vertexData().size());

        /* Non-interleaved vertex buffer is handled by encoding each attribute data separately due to the stride */
        for(UnsignedInt i = 0; i < owned_mesh.attributeCount(); i++) {
            auto format_size = sizeof(owned_mesh.attributeFormat(i));

            std::vector<unsigned char> buf(meshopt_encodeVertexBufferBound(owned_mesh.vertexCount(), owned_mesh.attributeStride(i)));
            if(owned_mesh.attributeName(i) == Trade::MeshAttribute::Position){
                if(owned_mesh.attributeStride(i)/format_size == 2) {
                    buf.resize(meshopt_encodeVertexBuffer(&buf[0], buf.size(), owned_mesh.positions2DAsArray(), owned_mesh.vertexCount(), owned_mesh.attributeStride(i)));
                }
                else if(owned_mesh.attributeStride(i)/format_size == 3) {
                    buf.resize(meshopt_encodeVertexBuffer(&buf[0], buf.size(), owned_mesh.positions3DAsArray(), owned_mesh.vertexCount(), owned_mesh.attributeStride(i)));
                }
            }
            if(owned_mesh.attributeName(i) == Trade::MeshAttribute::TextureCoordinates) {
                buf.resize(meshopt_encodeVertexBuffer(&buf[0], buf.size(), owned_mesh.textureCoordinates2DAsArray(), owned_mesh.vertexCount(), owned_mesh.attributeStride(i)));
            }
            if(owned_mesh.attributeName(i) == Trade::MeshAttribute::Color) {
                buf.resize(meshopt_encodeVertexBuffer(&buf[0], buf.size(), owned_mesh.colorsAsArray(), owned_mesh.vertexCount(), owned_mesh.attributeStride(i)));
            }
            std::move(buf.begin(), buf.end(), std::back_inserter(buffer));
        }
        buffer.shrink_to_fit();
    }
    CORRADE_INTERNAL_ASSERT(!buffer.empty() && buffer.size() < owned_mesh.vertexData().size());
    return buffer;
}

Containers::Array<char> decodeVertex(const MeshData& mesh) {
    auto owned_mesh = MeshTools::owned(mesh);
    if(MeshTools::isInterleaved(owned_mesh)) {
        Containers::Array<char> decoded{NoInit, owned_mesh.vertexCount()*owned_mesh.attributeStride(0)};
        Containers::ArrayView<const unsigned char> encoded_data = Containers::arrayCast<const unsigned char>(owned_mesh.vertexData());
        int result = meshopt_decodeVertexBuffer(decoded.data(), owned_mesh.vertexCount(), owned_mesh.attributeStride(0),
                                                encoded_data.data(), encoded_data.size());
        return decoded;
    } else {
        auto last_attribute_size = owned_mesh.vertexCount()*owned_mesh.attributeStride(owned_mesh.attributeCount()-1);
        Containers::Array<char> decoded{NoInit, owned_mesh.attributeOffset(owned_mesh.attributeCount()-1)+last_attribute_size};
        for(unsigned int i=0; i < owned_mesh.attributeCount(); ++i) {
            Containers::ArrayView<const unsigned char> data = Containers::arrayCast<const unsigned char>(owned_mesh.attribute(i).asContiguous());
            const std::size_t vertexSize = owned_mesh.attributeStride(i);
            int result = meshopt_decodeVertexBuffer(decoded.data(), owned_mesh.vertexCount(), vertexSize,
                                                     data, data.size());
        }
        return decoded;
    }
}

Containers::Optional<MeshData> MeshOptimizerSceneConverter::doConvert(const MeshData& mesh) {
    /* If the mesh is indexed with an implementation-specific index type,
       interleave() won't be able to turn its index buffer into a contiguous
       one. So fail early if that's the case. The mesh doesn't necessarily have
       to be indexed though -- it could be e.g. a triangle strip which we turn
       into an indexed mesh right after. */
    if(mesh.isIndexed() && isMeshIndexTypeImplementationSpecific(mesh.indexType())) {
        Error{} << "Trade::MeshOptimizerSceneConverter::convert(): can't perform any operation on an implementation-specific index type" << reinterpret_cast<void*>(meshIndexTypeUnwrap(mesh.indexType()));
        return {};
    }

    /* Make the mesh interleaved (with a contiguous index array) and owned
       first */
    MeshData out = MeshTools::owned(MeshTools::interleave(mesh));
    CORRADE_INTERNAL_ASSERT(MeshTools::isInterleaved(out));
    CORRADE_INTERNAL_ASSERT(!out.isIndexed() || out.indices().isContiguous());

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
            inputIndices = out.indices<UnsignedInt>().asContiguous();
        else {
            inputIndicesStorage = out.indicesAsArray();
            inputIndices = inputIndicesStorage;
        }

        Containers::Array<UnsignedInt> outputIndices;
        Containers::arrayResize<Trade::ArrayAllocator>(outputIndices, NoInit, mesh.indexCount());

        UnsignedInt vertexCount;
        if(configuration().value<bool>("simplifySloppy")) {
            /* The nullptr at the end is not needed but without it GCC's
               -Wzero-as-null-pointer-constant fires due to the default
               argument being `= 0`. WHAT THE FUCK, how is this warning
               useful?! Why everything today feels like hastily patched
               together by incompetent idiots?! */
            vertexCount = meshopt_simplifySloppy(outputIndices.data(), inputIndices.data(), out.indexCount(), static_cast<const float*>(positions.data()), out.vertexCount(), positions.stride(), targetIndexCount
                #if MESHOPTIMIZER_VERSION >= 160
                , targetError, nullptr
                #endif
            );
        } else {
            /* See adaptMeshOptSimplifySignature() above for details */
            vertexCount = adaptMeshOptSimplifySignature<meshopt_simplify>()(
                outputIndices.data(),
                inputIndices.data(),
                out.indexCount(),
                static_cast<const Float*>(positions.data()),
                out.vertexCount(),
                positions.stride(),
                targetIndexCount,
                targetError,
                configuration().value<bool>("simplifyLockBorder") ?
                    /** @todo switch to #if MESHOPTIMIZER_VERSION >= 180 once
                        enough time passes (released on 2022-08-01) */
                    1 /*meshopt_SimplifyLockBorder*/ : 0,
                nullptr);
        }

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

    if(configuration().value<bool>("encodeIndex")) {
        out = MeshTools::owned(mesh);

        std::vector<unsigned char> index_buffer(meshopt_encodeIndexBufferBound(out.indexCount(), out.vertexCount()));
        index_buffer.resize(meshopt_encodeIndexBuffer(&index_buffer[0], index_buffer.size(), out.indexData().data(), out.indexCount()));

        if(index_buffer.empty() || index_buffer.size() > out.indexData().size()) {
            Error{} << "Trade::MeshOptimizerSceneConverter::convert(): index buffer encoding failed: encoded buffer size:"
                    << index_buffer.size() << "original index data size:" << out.indexData().size();
            return Containers::NullOpt;
        }

        out = Trade::MeshData{out.primitive(), {}, index_buffer, Trade::MeshIndexData{index_buffer}, {},
                              out.vertexData(), out.releaseAttributeData(), out.vertexCount()};
    }

    if(configuration().value<bool>("decodeIndex")) {
        out = MeshTools::owned(mesh);
        Containers::Array<unsigned int> decodedIndex{NoInit, out.indexCount()*meshIndexTypeSize(out.indexType())};
        int result = meshopt_decodeIndexBuffer(decodedIndex.data(), out.indexCount(),
                                              reinterpret_cast<const unsigned char *>(out.indexData().data()), out.indexData().size());
        if(result != 0) {
            Error{} << "Trade::MeshOptimizerSceneConverter::convert(): index buffer decoding failed";
            return Containers::NullOpt;
        }

        out = Trade::MeshData{out.primitive(), {}, decodedIndex, Trade::MeshIndexData{decodedIndex}, {},
                              out.vertexData(), out.releaseAttributeData(), out.vertexCount()};
    }

    if(configuration().value<bool>("encodeVertex")) {
        meshopt_encodeVertexVersion(0);

        out = MeshTools::owned(mesh);
        auto attributes = MeshTools::owned(mesh).releaseAttributeData();
        for(UnsignedInt i = 0; i < out.attributeCount(); ++i) {
            const VertexFormat encodedFormat = !isVertexFormatImplementationSpecific(out.attributeFormat(i)) ?
                                               vertexFormatWrap(out.attributeFormat(i)) : out.attributeFormat(i);
            attributes[i] = Trade::MeshAttributeData{out.attributeName(i), encodedFormat,
                                                out.attributeOffset(i), out.vertexCount(),
                                                out.attributeStride(i), out.attributeArraySize(i)};
        }
        std::vector<unsigned char> encoded_buffer = encodeVertex(out);
        out = Trade::MeshData{out.primitive(), Trade::DataFlags{}, out.indexData(), Trade::MeshIndexData{out.indices()}, Trade::DataFlag::Mutable,
                              encoded_buffer, std::move(attributes)};
    }

    if(configuration().value<bool>("decodeVertex")) {
        out = MeshTools::owned(mesh);
        auto attributes = MeshTools::owned(mesh).releaseAttributeData();
        for(UnsignedInt i = 0; i != out.attributeCount(); ++i) {
            const VertexFormat originalFormat = vertexFormatUnwrap<VertexFormat>(out.attributeFormat(i));
            attributes[i] = Trade::MeshAttributeData{out.attributeName(i), originalFormat,
                                                     out.attributeOffset(i), out.vertexCount(),
                                                     out.attributeStride(i), out.attributeArraySize(i)};
        }
        Containers::Array<char> decoded{NoInit, out.vertexCount()*out.attributeStride(0)};
        Containers::ArrayView<const unsigned char> encoded_data = Containers::arrayCast<const unsigned char>(out.vertexData());
        int result = meshopt_decodeVertexBuffer(decoded.data(), out.vertexCount(), out.attributeStride(0),
                                                encoded_data.data(), encoded_data.size());
        out = Trade::MeshData{out.primitive(), Trade::DataFlags{}, out.indexData(), Trade::MeshIndexData{out.indices()}, Trade::DataFlag::Mutable,
                              decoded, std::move(attributes)};
    }

    /* Print before & after stats if verbose output is requested */
    if(flags() & SceneConverterFlag::Verbose)
        analyzePost("Trade::MeshOptimizerSceneConverter::convert():", out, configuration(), flags(), positions, vertexSize, vertexCacheStatsBefore, vertexFetchStatsBefore, overdrawStatsBefore);

    /* GCC 4.8 needs an explicit conversion, otherwise it tries to copy the
       thing and fails */
    return Containers::optional(std::move(out));
}

}}

CORRADE_PLUGIN_REGISTER(MeshOptimizerSceneConverter, Magnum::Trade::MeshOptimizerSceneConverter,
    MAGNUM_TRADE_ABSTRACTSCENECONVERTER_PLUGIN_INTERFACE)
