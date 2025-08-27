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

#include <Corrade/Containers/Optional.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Format.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/MeshTools/CompressIndices.h>
#include <Magnum/MeshTools/GenerateIndices.h>
#include <Magnum/MeshTools/Interleave.h>
#include <Magnum/Primitives/Circle.h>
#include <Magnum/Primitives/Icosphere.h>
#include <Magnum/Primitives/Plane.h>
#include <Magnum/Primitives/Square.h>
#include <Magnum/Primitives/UVSphere.h>
#include <Magnum/Trade/AbstractSceneConverter.h>
#include <Magnum/Trade/MeshData.h>
#include <meshoptimizer.h> /* MESHOPTIMIZER_VERSION */

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct MeshOptimizerSceneConverterTest: TestSuite::Tester {
    explicit MeshOptimizerSceneConverterTest();

    void notTriangles();
    void notIndexed();
    void implementationSpecificIndexType();
    void immutableIndexData();

    void inPlaceOptimizeVertexFetchImmutableVertexData();
    void inPlaceOptimizeVertexFetchNotInterleaved();
    void inPlaceOptimizeOverdrawNoPositions();
    void inPlaceNonContiguousIndexBuffer();

    void inPlaceOptimizeNone();

    template<class T> void inPlaceOptimizeVertexCache();
    template<class T> void inPlaceOptimizeOverdraw();
    void inPlaceOptimizeOverdrawPositionsNotFourByteAligned();
    template<class T> void inPlaceOptimizeVertexFetch();
    void inPlaceOptimizeVertexFetchNoAttributes();

    template<class T> void inPlaceOptimizeEmpty();

    template<class T> void verbose();
    void verboseCustomAttribute();
    void verboseImplementationSpecificAttribute();

    /* Those test the copy-making function */
    void copy();
    void copyTriangleStrip2DPositions();
    void copyTriangleFanIndexed();
    template<class T> void copyNonContiguousIndexBuffer();
    void copyNegativeAttributeStride();

    void simplifyInPlace();
    void simplifyNoPositions();
    template<class T> void simplify();
    template<class T> void simplifySloppy();
    void simplifyEmptyIndexBuffer();
    void simplifyVerbose();
    void simplifyEmpty();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractSceneConverter> _manager{"nonexistent"};
};

const struct {
    const char* name;
    SceneConverterFlags flags;
    bool quiet;
} QuietVerboseData[]{
    {"verbose", SceneConverterFlag::Verbose, false},
    /* A bit silly at first, but makes it possible to hide the warning that's
       only present in case the verbose output is enabled. Which means it's
       not an error to pass both, it has a valid use case! */
    {"verbose + quiet", SceneConverterFlag::Verbose|SceneConverterFlag::Quiet, true},
};

const struct {
    const char* name;
    const char* option;
} SimplifyErrorData[] {
    {"", "simplify"},
    {"sloppy", "simplifySloppy"}
};

const struct {
    const char* name;
    Containers::Optional<bool> failEmpty;
    Containers::Optional<UnsignedInt> inputIndexCount;
    Float targetError;
    const char* message;
} SimplifyEmptyData[]{
    {"simplified to empty", {}, {}, 2.0f, nullptr},
    {"simplified to empty, failEmpty", true, {}, 2.0f,
        "Trade::MeshOptimizerSceneConverter::convert(): simplification resulted in an empty mesh\n"},
    /* If the input is empty already, don't fail anything */
    {"empty input", {}, 0, 1.0e-2f, nullptr},
    {"empty input, failEmpty", {}, 0, 1.0e-2f, nullptr},
};

MeshOptimizerSceneConverterTest::MeshOptimizerSceneConverterTest() {
    addTests({
        &MeshOptimizerSceneConverterTest::notTriangles,
        &MeshOptimizerSceneConverterTest::notIndexed,
        &MeshOptimizerSceneConverterTest::implementationSpecificIndexType,
        &MeshOptimizerSceneConverterTest::immutableIndexData,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexFetchImmutableVertexData,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexFetchNotInterleaved,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeOverdrawNoPositions,
        &MeshOptimizerSceneConverterTest::inPlaceNonContiguousIndexBuffer,

        &MeshOptimizerSceneConverterTest::inPlaceOptimizeNone,

        &MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexCache<UnsignedByte>,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexCache<UnsignedShort>,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexCache<UnsignedInt>,

        &MeshOptimizerSceneConverterTest::inPlaceOptimizeOverdraw<UnsignedByte>,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeOverdraw<UnsignedShort>,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeOverdraw<UnsignedInt>,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeOverdrawPositionsNotFourByteAligned,

        &MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexFetch<UnsignedByte>,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexFetch<UnsignedShort>,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexFetch<UnsignedInt>,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexFetchNoAttributes,

        &MeshOptimizerSceneConverterTest::verbose<UnsignedByte>,
        &MeshOptimizerSceneConverterTest::verbose<UnsignedShort>,
        &MeshOptimizerSceneConverterTest::verbose<UnsignedInt>,
        &MeshOptimizerSceneConverterTest::verboseCustomAttribute});

    addInstancedTests({&MeshOptimizerSceneConverterTest::verboseImplementationSpecificAttribute},
        Containers::arraySize(QuietVerboseData));

    addTests({
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeEmpty<UnsignedByte>,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeEmpty<UnsignedShort>,
        &MeshOptimizerSceneConverterTest::inPlaceOptimizeEmpty<UnsignedInt>,

        &MeshOptimizerSceneConverterTest::copy,
        &MeshOptimizerSceneConverterTest::copyTriangleStrip2DPositions,
        &MeshOptimizerSceneConverterTest::copyTriangleFanIndexed,
        &MeshOptimizerSceneConverterTest::copyNonContiguousIndexBuffer<UnsignedByte>,
        &MeshOptimizerSceneConverterTest::copyNonContiguousIndexBuffer<UnsignedShort>,
        &MeshOptimizerSceneConverterTest::copyNonContiguousIndexBuffer<UnsignedInt>,
        &MeshOptimizerSceneConverterTest::copyNegativeAttributeStride});

    addInstancedTests({
        &MeshOptimizerSceneConverterTest::simplifyInPlace,
        &MeshOptimizerSceneConverterTest::simplifyNoPositions},
        Containers::arraySize(SimplifyErrorData));

    addTests({
        &MeshOptimizerSceneConverterTest::simplify<UnsignedByte>,
        &MeshOptimizerSceneConverterTest::simplify<UnsignedShort>,
        &MeshOptimizerSceneConverterTest::simplify<UnsignedInt>,
        &MeshOptimizerSceneConverterTest::simplifySloppy<UnsignedByte>,
        &MeshOptimizerSceneConverterTest::simplifySloppy<UnsignedShort>,
        &MeshOptimizerSceneConverterTest::simplifySloppy<UnsignedInt>,
        &MeshOptimizerSceneConverterTest::simplifyEmptyIndexBuffer,
        &MeshOptimizerSceneConverterTest::simplifyVerbose});

    addInstancedTests({&MeshOptimizerSceneConverterTest::simplifyEmpty},
        Containers::arraySize(SimplifyEmptyData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef MESHOPTIMIZERSCENECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(MESHOPTIMIZERSCENECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void MeshOptimizerSceneConverterTest::notTriangles() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");

    MeshData mesh{MeshPrimitive::Instances, 3};
    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(mesh));
    CORRADE_VERIFY(!converter->convertInPlace(mesh));
    CORRADE_COMPARE(out,
        "Trade::MeshOptimizerSceneConverter::convert(): expected a triangle mesh, got MeshPrimitive::Instances\n"
        "Trade::MeshOptimizerSceneConverter::convertInPlace(): expected a triangle mesh, got MeshPrimitive::Instances\n");
}

void MeshOptimizerSceneConverterTest::notIndexed() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");

    MeshData mesh{MeshPrimitive::Triangles, 3};
    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(mesh));
    CORRADE_VERIFY(!converter->convertInPlace(mesh));
    CORRADE_COMPARE(out,
        "Trade::MeshOptimizerSceneConverter::convert(): expected an indexed mesh\n"
        "Trade::MeshOptimizerSceneConverter::convertInPlace(): expected an indexed mesh\n");
}

void MeshOptimizerSceneConverterTest::implementationSpecificIndexType() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");

    Containers::Array<char> indexData{3};
    Containers::StridedArrayView1D<UnsignedByte> indices = Containers::arrayCast<UnsignedByte>(indexData);
    MeshData mesh{MeshPrimitive::Triangles,
        Utility::move(indexData), MeshIndexData{meshIndexTypeWrap(0xcaca), indices}, 1};

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(mesh));
    CORRADE_VERIFY(!converter->convertInPlace(mesh));
    CORRADE_COMPARE(out,
        "Trade::MeshOptimizerSceneConverter::convert(): can't perform any operation on an implementation-specific index type 0xcaca\n"
        "Trade::MeshOptimizerSceneConverter::convertInPlace(): can't perform any operation on an implementation-specific index type 0xcaca\n");
}

void MeshOptimizerSceneConverterTest::immutableIndexData() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", true);
    converter->configuration().setValue("optimizeOverdraw", false);
    converter->configuration().setValue("optimizeVertexFetch", false);

    constexpr UnsignedByte indices[3]{};
    MeshData mesh{MeshPrimitive::Triangles,
        {}, indices, MeshIndexData{indices}, 1};

    CORRADE_VERIFY(converter->convert(mesh)); /* Here it's not a problem */

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertInPlace(mesh));
    CORRADE_COMPARE(out,
        "Trade::MeshOptimizerSceneConverter::convertInPlace(): optimizeVertexCache, optimizeOverdraw and optimizeVertexFetch require index data to be mutable\n");
}

void MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexFetchImmutableVertexData() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", false);
    converter->configuration().setValue("optimizeOverdraw", false);
    converter->configuration().setValue("optimizeVertexFetch", true);

    Containers::Array<char> indexData{3};
    MeshIndexData indices{MeshIndexType::UnsignedByte, indexData};
    constexpr UnsignedByte vertices[3]{};
    MeshData mesh{MeshPrimitive::Triangles,
        Utility::move(indexData), indices,
        {}, vertices, {}, 1};

    CORRADE_VERIFY(converter->convert(mesh)); /* Here it's not a problem */

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertInPlace(mesh));
    CORRADE_COMPARE(out,
        "Trade::MeshOptimizerSceneConverter::convertInPlace(): optimizeVertexFetch requires vertex data to be mutable\n");
}

void MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexFetchNotInterleaved() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", false);
    converter->configuration().setValue("optimizeOverdraw", false);
    converter->configuration().setValue("optimizeVertexFetch", true);

    Containers::Array<char> indexData{3};
    MeshIndexData indices{MeshIndexType::UnsignedByte, indexData};
    Containers::Array<char> vertexData{6};
    MeshData mesh{MeshPrimitive::Triangles,
        Utility::move(indexData), indices,
        Utility::move(vertexData), {
            MeshAttributeData{meshAttributeCustom(0), VertexFormat::Byte,
                0, 3, 1},
            MeshAttributeData{meshAttributeCustom(1), VertexFormat::Byte,
                3, 3, 1},
        }, 3};

    CORRADE_VERIFY(converter->convert(mesh)); /* Here it's not a problem */

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertInPlace(mesh));
    CORRADE_COMPARE(out,
        "Trade::MeshOptimizerSceneConverter::convertInPlace(): optimizeVertexFetch requires the mesh to be interleaved\n");
}

void MeshOptimizerSceneConverterTest::inPlaceOptimizeOverdrawNoPositions() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", false);
    converter->configuration().setValue("optimizeOverdraw", true);
    converter->configuration().setValue("optimizeVertexFetch", false);

    Containers::Array<char> indexData{3};
    MeshIndexData indices{MeshIndexType::UnsignedByte, indexData};
    MeshData mesh{MeshPrimitive::Triangles,
        Utility::move(indexData), indices,
        nullptr, {}, 1};
    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(mesh));
    CORRADE_VERIFY(!converter->convertInPlace(mesh));
    CORRADE_COMPARE(out,
        "Trade::MeshOptimizerSceneConverter::convert(): optimizeOverdraw and simplify require the mesh to have positions\n"
        "Trade::MeshOptimizerSceneConverter::convertInPlace(): optimizeOverdraw and simplify require the mesh to have positions\n");
}

void MeshOptimizerSceneConverterTest::inPlaceNonContiguousIndexBuffer() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    /* This is checked even if nothing is enabled, same as it's always checked
       that the mesh is indexed */
    converter->configuration().setValue("optimizeVertexCache", false);
    converter->configuration().setValue("optimizeOverdraw", false);
    converter->configuration().setValue("optimizeVertexFetch", false);

    constexpr UnsignedByte indices[3]{};
    MeshData mesh{MeshPrimitive::Triangles,
        {}, indices, MeshIndexData{Containers::stridedArrayView(indices).flipped<0>()}, 1};

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertInPlace(mesh));
    CORRADE_COMPARE(out,
        "Trade::MeshOptimizerSceneConverter::convertInPlace(): in-place conversion is possible only with contiguous index buffers\n");
}

void MeshOptimizerSceneConverterTest::inPlaceOptimizeNone() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", false);
    converter->configuration().setValue("optimizeOverdraw", false);
    converter->configuration().setValue("optimizeVertexFetch", false);

    const UnsignedInt indices[] {
        12, 13, 14, 15, 16, 12, 17, 18, 19, 17, 20, 21, 22, 23, 24, 22
    };

    const Vector3 positionsOrNormals[] {
        {0.0f, -0.525731f, 0.850651f},
        {0.850651f, 0.0f, 0.525731f},
        {0.850651f, 0.0f, -0.525731f},
        {-0.850651f, 0.0f, -0.525731f}
    };

    MeshData icosphere = Primitives::icosphereSolid(1);
    CORRADE_COMPARE_AS(icosphere.indices<UnsignedInt>().prefix(16),
        Containers::arrayView(indices),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(icosphere.attribute<Vector3>(MeshAttribute::Position).prefix(4),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(icosphere.attribute<Vector3>(MeshAttribute::Normal).prefix(4),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);

    /* Make an immutable reference to verify that mutable data aren't required
       when everything is disabled */
    MeshData icosphereImmutable{icosphere.primitive(),
        {}, icosphere.indexData(), MeshIndexData{icosphere.indices()},
        {}, icosphere.vertexData(), Trade::meshAttributeDataNonOwningArray(icosphere.attributeData())};

    /* This shouldn't change anything */
    CORRADE_VERIFY(converter->convertInPlace(icosphereImmutable));
    CORRADE_COMPARE_AS(icosphere.indices<UnsignedInt>().prefix(16),
        Containers::arrayView(indices),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(icosphere.attribute<Vector3>(MeshAttribute::Position).prefix(4),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(icosphere.attribute<Vector3>(MeshAttribute::Normal).prefix(4),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
}

template<class T> void MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexCache() {
    setTestCaseTemplateName(Math::TypeTraits<T>::name());

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", true);
    converter->configuration().setValue("optimizeOverdraw", false);
    converter->configuration().setValue("optimizeVertexFetch", false);

    /* Tried with a cubeSolid() first, but that one seems to have an optimal
       layout already, hah. With 0 subdivisions the overdraw optimization does
       nothing.  */
    MeshData icosphere = MeshTools::compressIndices(
        Primitives::icosphereSolid(1),
        Implementation::meshIndexTypeFor<T>());
    CORRADE_COMPARE(icosphere.indexType(), Implementation::meshIndexTypeFor<T>());

    CORRADE_VERIFY(converter->convertInPlace(icosphere));
    CORRADE_COMPARE_AS(icosphere.indices<T>().prefix(16), Containers::arrayView<T>({
        12, 13, 14, 14, 13, 6, 6, 13, 25, 14, 6, 24, 22, 6, 25, 6
    }), TestSuite::Compare::Container);

    /* No change, same as in inPlaceOptimizeNone() */
    const Vector3 positionsOrNormals[]{
        {0.0f, -0.525731f, 0.850651f},
        {0.850651f, 0.0f, 0.525731f},
        {0.850651f, 0.0f, -0.525731f},
        {-0.850651f, 0.0f, -0.525731f}
    };
    CORRADE_COMPARE_AS(icosphere.attribute<Vector3>(MeshAttribute::Position).prefix(4),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(icosphere.attribute<Vector3>(MeshAttribute::Normal).prefix(4),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
}

template<class T> void MeshOptimizerSceneConverterTest::inPlaceOptimizeOverdraw() {
    setTestCaseTemplateName(Math::TypeTraits<T>::name());

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", true);
    converter->configuration().setValue("optimizeOverdraw", true);
    converter->configuration().setValue("optimizeVertexFetch", false);

    MeshData icosphere = MeshTools::compressIndices(
        Primitives::icosphereSolid(1),
        Implementation::meshIndexTypeFor<T>());
    CORRADE_COMPARE(icosphere.indexType(), Implementation::meshIndexTypeFor<T>());

    /* The default 1.05 doesn't do anything */
    CORRADE_VERIFY(converter->convertInPlace(icosphere));
    CORRADE_COMPARE_AS(icosphere.indices<T>().prefix(16), Containers::arrayView<T>({
        12, 13, 14, 14, 13, 6, 6, 13, 25, 14, 6, 24, 22, 6, 25, 6
    }), TestSuite::Compare::Container);

    /* Try again with a higher value. Disable vertex cache optimization to
       avoid it being performed twice. */
    converter->configuration().setValue("optimizeVertexCache", false);
    converter->configuration().setValue("optimizeOverdrawThreshold", 2.5f);
    CORRADE_VERIFY(converter->convertInPlace(icosphere));
    CORRADE_COMPARE_AS(icosphere.indices<T>().prefix(16), Containers::arrayView<T>({
        3, 17, 19, 3, 19, 31, 3, 30, 20, 31, 30, 3, 12, 13, 14, 14
    }), TestSuite::Compare::Container);

    /* No change, same as in inPlaceOptimizeNone() */
    const Vector3 positionsOrNormals[]{
        {0.0f, -0.525731f, 0.850651f},
        {0.850651f, 0.0f, 0.525731f},
        {0.850651f, 0.0f, -0.525731f},
        {-0.850651f, 0.0f, -0.525731f}
    };
    CORRADE_COMPARE_AS(icosphere.attribute<Vector3>(MeshAttribute::Position).prefix(4),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(icosphere.attribute<Vector3>(MeshAttribute::Normal).prefix(4),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
}

void MeshOptimizerSceneConverterTest::inPlaceOptimizeOverdrawPositionsNotFourByteAligned() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", true);
    converter->configuration().setValue("optimizeOverdraw", true);
    converter->configuration().setValue("optimizeVertexFetch", false);
    /* Same as in inPlaceOptimizeOverdraw() */
    converter->configuration().setValue("optimizeOverdrawThreshold", 2.5f);

    MeshData icosphere = MeshTools::interleave(
        Primitives::icosphereSolid(1),
        {MeshAttributeData{1}});
    /* Should be not divisible by 4 (which meshoptimizer expects) */
    CORRADE_COMPARE(icosphere.attributeStride(MeshAttribute::Position), 25);

    CORRADE_VERIFY(converter->convertInPlace(icosphere));
    CORRADE_COMPARE_AS(icosphere.indices<UnsignedInt>().prefix(16), Containers::arrayView<UnsignedInt>({
        /* Same as in inPlaceOptimizeOverdraw() */
        3, 17, 19, 3, 19, 31, 3, 30, 20, 31, 30, 3, 12, 13, 14, 14
    }), TestSuite::Compare::Container);
}

template<class T> void MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexFetch() {
    setTestCaseTemplateName(Math::TypeTraits<T>::name());

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", true);
    converter->configuration().setValue("optimizeOverdraw", true);
    converter->configuration().setValue("optimizeVertexFetch", true);

    MeshData icosphere = MeshTools::compressIndices(
        Primitives::icosphereSolid(1),
        Implementation::meshIndexTypeFor<T>());
    CORRADE_COMPARE(icosphere.indexType(), Implementation::meshIndexTypeFor<T>());

    CORRADE_VERIFY(converter->convertInPlace(icosphere));
    CORRADE_COMPARE_AS(icosphere.indices<T>().prefix(16), Containers::arrayView<T>({
        0, 1, 2, 2, 1, 3, 3, 1, 4, 2, 3, 5, 6, 3, 4, 3
    }), TestSuite::Compare::Container);

    /* Gets reordered so the earliest values in the original index buffer are
       early in memory also */
    const Vector3 positionsOrNormals[]{
        {1.0f, 0.0f, 0.0f},
        {0.809017f, 0.5f, -0.309017f},
        {0.809017f, 0.5f, 0.309017f},
        {0.525731f, 0.850651f, 0.0f}
    };
    CORRADE_COMPARE_AS(icosphere.attribute<Vector3>(MeshAttribute::Position).prefix(4),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(icosphere.attribute<Vector3>(MeshAttribute::Normal).prefix(4),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
}

void MeshOptimizerSceneConverterTest::inPlaceOptimizeVertexFetchNoAttributes() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", true);
    converter->configuration().setValue("optimizeOverdraw", false);
    converter->configuration().setValue("optimizeVertexFetch", true);

    MeshData icosphere = Primitives::icosphereSolid(1);
    MeshIndexData indices{icosphere.indices()};
    MeshData icosphereIndicesOnly{icosphere.primitive(),
        icosphere.releaseIndexData(), indices, icosphere.vertexCount()};

    CORRADE_VERIFY(converter->convertInPlace(icosphereIndicesOnly));
    CORRADE_COMPARE_AS(icosphereIndicesOnly.indices<UnsignedInt>().prefix(16), Containers::arrayView<UnsignedInt>({
        /* Same as in inPlaceOptimizeVertexCache, as optimizeOverdraw would
           need positions and optimizeVertexFetch is (silently) skipped because
           there are no attribute data */
        12, 13, 14, 14, 13, 6, 6, 13, 25, 14, 6, 24, 22, 6, 25, 6
    }), TestSuite::Compare::Container);
}

template<class T> void MeshOptimizerSceneConverterTest::verbose() {
    setTestCaseTemplateName(Math::TypeTraits<T>::name());

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->setFlags(SceneConverterFlag::Verbose);

    /* We need enough vertices for the optimization to make any difference, and
       that unfortunately means 8-bit indices can't be verified here. Instead
       it's done in verboseImplementationSpecificAttribute() below. */
    if(Implementation::meshIndexTypeFor<T>() == MeshIndexType::UnsignedByte)
        CORRADE_SKIP("The mesh is too large to fit into 8-bit indices.");

    MeshData icosphere = MeshTools::compressIndices(
        Primitives::icosphereSolid(6),
        Implementation::meshIndexTypeFor<T>());
    CORRADE_COMPARE(icosphere.indexType(), Implementation::meshIndexTypeFor<T>());

    Containers::String out;
    {
        Debug redirectDebug{&out};
        CORRADE_VERIFY(converter->convert(icosphere));
        CORRADE_VERIFY(converter->convertInPlace(icosphere));
    }

    /* We get a slightly different result on MSVC STL (but apparently not with
       2019 anymore for some reason); moreover GCC 4.8 can't handle raw string
       literals inside macros */
    const char* acmr =
        #if defined(CORRADE_TARGET_DINKUMWARE) && _MSC_VER < 1920
        "2.01563"
        #else
        "2.01562"
        #endif
        ;
    /* On version 0.24 the overdraw increases instead of decreasing. On 0.25
       other things change as well, due to the transformed vertex count being
       different. */
    #if MESHOPTIMIZER_VERSION >= 240
    const char* shaded = "308756";
    const char* overdraw = "1.00003";
    #else
    const char* shaded = "308750";
    const char* overdraw = "1.00001";
    #endif
    #if MESHOPTIMIZER_VERSION >= 250
    const char* transformedOut = "58525";
    const char* acmrOut = "0.714417";
    const char* atvrOut = "1.42876";
    const char* bytesOut = "1585472";
    const char* overfetchOut = "1.61275";
    #else
    const char* transformedOut = "58521";
    const char* acmrOut = "0.714368";
    const char* atvrOut = "1.42867";
    const char* bytesOut = "1582144";
    const char* overfetchOut = "1.60936";
    #endif
    const char* expected = R"(Trade::MeshOptimizerSceneConverter::convert(): processing stats:
  vertex cache:
    165120 -> {0} transformed vertices
    1 -> 1 executed warps
    ACMR {1} -> {2}
    ATVR 4.03105 -> {3}
  vertex fetch:
    3891008 -> {4} bytes fetched
    overfetch 3.95794 -> {5}
  overdraw:
    308753 -> {6} shaded pixels
    308748 -> 308748 covered pixels
    overdraw 1.00002 -> {7}
Trade::MeshOptimizerSceneConverter::convertInPlace(): processing stats:
  vertex cache:
    165120 -> {0} transformed vertices
    1 -> 1 executed warps
    ACMR {1} -> {2}
    ATVR 4.03105 -> {3}
  vertex fetch:
    3891008 -> {4} bytes fetched
    overfetch 3.95794 -> {5}
  overdraw:
    308753 -> {6} shaded pixels
    308748 -> 308748 covered pixels
    overdraw 1.00002 -> {7}
)";
    CORRADE_COMPARE_AS(out,
        Utility::format(expected,
            transformedOut,
            acmr, acmrOut,
            atvrOut,
            bytesOut,
            overfetchOut,
            shaded,
            overdraw),
        TestSuite::Compare::String);
}

void MeshOptimizerSceneConverterTest::verboseCustomAttribute() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->setFlags(SceneConverterFlag::Verbose);
    /* All options on their defaults, should be the same as
       optimizeVertexFetch() */

    MeshData icosphere = Primitives::icosphereSolid(6);
    MeshIndexData indices{icosphere.indices()};
    auto attributes = Containers::array({
        icosphere.attributeData(0),
        /* Reinterpret the 12-byte normal as a Matrix3x2b[2] to verify vertex
           fetch bytes are calculated correctly even for custom / matrix /
           array attribs */
        Trade::MeshAttributeData{meshAttributeCustom(1),
            VertexFormat::Matrix3x2bNormalized, icosphere.attribute(1), 2}
    });
    MeshData icosphereCustom{icosphere.primitive(),
        icosphere.releaseIndexData(), indices,
        icosphere.releaseVertexData(), Utility::move(attributes)};

    Containers::String out;
    {
        Debug redirectDebug{&out};
        CORRADE_VERIFY(converter->convertInPlace(icosphereCustom));
    }

    /* We get a slightly different result on MSVC STL (but apparently not with
       2019 anymore for some reason); moreover GCC 4.8 can't handle raw string
       literals inside macros */
    const char* acmr =
        #if defined(CORRADE_TARGET_DINKUMWARE) && _MSC_VER < 1920
        "2.01563"
        #else
        "2.01562"
        #endif
        ;
    /* On version 0.24 the overdraw increases instead of decreasing. On 0.25
       other things change as well, due to the transformed vertex count being
       different. */
    #if MESHOPTIMIZER_VERSION >= 240
    const char* shaded = "308756";
    const char* overdraw = "1.00003";
    #else
    const char* shaded = "308750";
    const char* overdraw = "1.00001";
    #endif
    #if MESHOPTIMIZER_VERSION >= 250
    const char* transformedOut = "58525";
    const char* acmrOut = "0.714417";
    const char* atvrOut = "1.42876";
    const char* bytesOut = "1585472";
    const char* overfetchOut = "1.61275";
    #else
    const char* transformedOut = "58521";
    const char* acmrOut = "0.714368";
    const char* atvrOut = "1.42867";
    const char* bytesOut = "1582144";
    const char* overfetchOut = "1.60936";
    #endif
    const char* expected = R"(Trade::MeshOptimizerSceneConverter::convertInPlace(): processing stats:
  vertex cache:
    165120 -> {0} transformed vertices
    1 -> 1 executed warps
    ACMR {1} -> {2}
    ATVR 4.03105 -> {3}
  vertex fetch:
    3891008 -> {4} bytes fetched
    overfetch 3.95794 -> {5}
  overdraw:
    308753 -> {6} shaded pixels
    308748 -> 308748 covered pixels
    overdraw 1.00002 -> {7}
)";
    CORRADE_COMPARE_AS(out,
        Utility::format(expected,
            transformedOut,
            acmr, acmrOut,
            atvrOut,
            bytesOut,
            overfetchOut,
            shaded,
            overdraw),
        TestSuite::Compare::String);
}

void MeshOptimizerSceneConverterTest::verboseImplementationSpecificAttribute() {
    auto&& data = QuietVerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->setFlags(data.flags);

    /* Using an 8-bit type to complement the verbose() test, which can't fit
       into there */
    MeshData icosphere = MeshTools::compressIndices(
        Primitives::icosphereSolid(1),
        MeshIndexType::UnsignedByte);

    MeshIndexData indices{icosphere.indices()};
    auto attributes = Containers::array({
        icosphere.attributeData(0),
        Trade::MeshAttributeData{icosphere.attributeName(1),
            vertexFormatWrap(0x1234), icosphere.attribute(1)}
    });
    MeshData icosphereExtra{icosphere.primitive(),
        icosphere.releaseIndexData(), indices,
        icosphere.releaseVertexData(), Utility::move(attributes)};

    Containers::String out;
    {
        Debug redirectDebug{&out};
        Warning redirectWarning{&out};
        /** @todo test also convert() once the interleave() inside of it
            isn't crashing on impl-specific vertex formats */
        CORRADE_VERIFY(converter->convertInPlace(icosphereExtra));
    }
    CORRADE_COMPARE_AS(icosphereExtra.indices<UnsignedByte>().prefix(16),
        Containers::arrayView<UnsignedByte>({
            /* Same as in inPlaceOptimizeVertexFetch() */
            0, 1, 2, 2, 1, 3, 3, 1, 4, 2, 3, 5, 6, 3, 4, 3
        }), TestSuite::Compare::Container);
    const char* warning = data.quiet ? "" : "Trade::MeshOptimizerSceneConverter::convertInPlace(): can't analyze vertex fetch for VertexFormat::ImplementationSpecific(0x1234)\n";
    /* Not sure what's up with the overdraw calculation in 0.24 */
    const char* overdraw =
        #if MESHOPTIMIZER_VERSION >= 240
        "    285312 -> 285312 shaded pixels\n"
        "    285312 -> 285312 covered pixels\n"
        #else
        "    149965 -> 149965 shaded pixels\n"
        "    149965 -> 149965 covered pixels\n"
        #endif
        ;
    CORRADE_COMPARE_AS(out,
        Utility::format("{}Trade::MeshOptimizerSceneConverter::convertInPlace(): processing stats:\n"
        "  vertex cache:\n"
        "    136 -> 49 transformed vertices\n"
        "    1 -> 1 executed warps\n"
        "    ACMR 1.7 -> 0.6125\n"
        "    ATVR 3.2381 -> 1.16667\n"
        "  overdraw:\n"
        "{}"
        "    overdraw 1 -> 1\n", warning, overdraw),
        TestSuite::Compare::String);
}

template<class T> void MeshOptimizerSceneConverterTest::inPlaceOptimizeEmpty() {
    setTestCaseTemplateName(Math::TypeTraits<T>::name());

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->setFlags(SceneConverterFlag::Verbose);

    MeshData icosphere = Primitives::icosphereSolid(0);
    icosphere.releaseIndexData();
    icosphere.releaseVertexData();
    CORRADE_VERIFY(icosphere.isIndexed());
    CORRADE_COMPARE(icosphere.indexCount(), 0);
    CORRADE_COMPARE(icosphere.vertexCount(), 0);
    CORRADE_COMPARE(icosphere.attributeCount(), 2);

    /* It should simply do nothing (and it should especially not crash) */
    Containers::String out;
    {
        Debug redirectDebug{&out};
        CORRADE_VERIFY(converter->convertInPlace(icosphere));
    }
    /* GCC 4.8 can't handle raw string literals inside macros */
    const char* expected = R"(Trade::MeshOptimizerSceneConverter::convertInPlace(): processing stats:
  vertex cache:
    0 -> 0 transformed vertices
    0 -> 0 executed warps
    ACMR 0 -> 0
    ATVR 0 -> 0
  vertex fetch:
    0 -> 0 bytes fetched
    overfetch 0 -> 0
  overdraw:
    0 -> 0 shaded pixels
    0 -> 0 covered pixels
    overdraw 0 -> 0
)";
    CORRADE_COMPARE(out, expected);
}

void MeshOptimizerSceneConverterTest::copy() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");

    /* Convert to a 16-bit indices to verify the type is preserved */
    MeshData original = MeshTools::compressIndices(Primitives::icosphereSolid(1));
    CORRADE_COMPARE(original.indexType(), MeshIndexType::UnsignedShort);
    Containers::Optional<MeshData> optimized = converter->convert(original);

    CORRADE_VERIFY(optimized);
    CORRADE_COMPARE(optimized->primitive(), original.primitive());
    CORRADE_COMPARE(optimized->indexCount(), original.indexCount());
    CORRADE_COMPARE(optimized->indexType(), MeshIndexType::UnsignedShort);
    CORRADE_COMPARE(optimized->vertexCount(), original.vertexCount());
    CORRADE_COMPARE(optimized->attributeCount(), original.attributeCount());
    CORRADE_COMPARE(optimized->indexDataFlags(), DataFlag::Owned|DataFlag::Mutable);
    CORRADE_COMPARE(optimized->vertexDataFlags(), DataFlag::Owned|DataFlag::Mutable);

    CORRADE_COMPARE_AS(optimized->indices<UnsignedShort>().prefix(16),
        Containers::arrayView<UnsignedShort>({
            /* Same as in inPlaceOptimizeVertexFetch() */
            0, 1, 2, 2, 1, 3, 3, 1, 4, 2, 3, 5, 6, 3, 4, 3
        }), TestSuite::Compare::Container);

    /* Same as in inPlaceOptimizeVertexFetch() */
    const Vector3 positionsOrNormals[]{
        {1.0f, 0.0f, 0.0f},
        {0.809017f, 0.5f, -0.309017f},
        {0.809017f, 0.5f, 0.309017f},
        {0.525731f, 0.850651f, 0.0f}
    };
    CORRADE_COMPARE_AS(optimized->attribute<Vector3>(MeshAttribute::Position).prefix(4),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(optimized->attribute<Vector3>(MeshAttribute::Normal).prefix(4),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
}

void MeshOptimizerSceneConverterTest::copyTriangleStrip2DPositions() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");

    /* Take a simple mesh just to verify it gets correctly converted to indexed
       triangles; additionally it's 2D to check that the positions get expanded
       to 3D internally */
    MeshData original = Primitives::squareSolid();
    Containers::Optional<MeshData> optimized = converter->convert(original);

    CORRADE_VERIFY(optimized);
    CORRADE_COMPARE(optimized->primitive(), MeshPrimitive::Triangles);
    CORRADE_COMPARE(optimized->indexCount(), 6);
    CORRADE_COMPARE(optimized->vertexCount(), original.vertexCount());
    CORRADE_COMPARE(optimized->attributeCount(), original.attributeCount());
    CORRADE_COMPARE(optimized->indexDataFlags(), DataFlag::Owned|DataFlag::Mutable);
    CORRADE_COMPARE(optimized->vertexDataFlags(), DataFlag::Owned|DataFlag::Mutable);

    CORRADE_COMPARE_AS(optimized->indices<UnsignedInt>(), Containers::arrayView<UnsignedInt>({
        0, 1, 2, 2, 1, 3
    }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(optimized->attribute<Vector2>(MeshAttribute::Position),
        Containers::arrayView<Vector2>({
            { 1.0f, -1.0f},
            { 1.0f,  1.0f},
            {-1.0f, -1.0f},
            {-1.0f,  1.0f}
        }), TestSuite::Compare::Container);
}

void MeshOptimizerSceneConverterTest::copyTriangleFanIndexed() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");

    /* Take a circle (which is a fan) and add a trivial index buffer to it */
    MeshData original = Primitives::circle3DSolid(3);
    const UnsignedByte indices[] { 0, 1, 2, 3, 4 };
    CORRADE_COMPARE(Containers::arraySize(indices), original.vertexCount());
    MeshData indexed{original.primitive(),
        {}, indices, MeshIndexData{indices},
        {}, original.vertexData(), meshAttributeDataNonOwningArray(original.attributeData())};

    Containers::Optional<MeshData> optimized = converter->convert(indexed);

    CORRADE_VERIFY(optimized);
    CORRADE_COMPARE(optimized->primitive(), MeshPrimitive::Triangles);
    CORRADE_COMPARE(optimized->indexCount(), 9);
    CORRADE_COMPARE(optimized->vertexCount(), original.vertexCount());
    CORRADE_COMPARE(optimized->attributeCount(), original.attributeCount());
    CORRADE_COMPARE(optimized->indexDataFlags(), DataFlag::Owned|DataFlag::Mutable);
    CORRADE_COMPARE(optimized->vertexDataFlags(), DataFlag::Owned|DataFlag::Mutable);

    CORRADE_COMPARE_AS(optimized->indices<UnsignedInt>(), Containers::arrayView<UnsignedInt>({
        0, 1, 2, 0, 2, 3, 0, 3, 4
    }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(optimized->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView<Vector3>({
            {0.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {-0.5f, 0.866025f, 0.0f},
            {-0.5f, -0.866025f, 0.0f},
            {1.0f, 0.0f, 0.0f}
        }), TestSuite::Compare::Container);
}

template<class T> void MeshOptimizerSceneConverterTest::copyNonContiguousIndexBuffer() {
    setTestCaseTemplateName(Math::TypeTraits<T>::name());

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");

    /* Like copyTriangleStrip2DPositions() but with the index buffer expanded,
       stored in reverse order and negative stride */
    const T indicesReversed[]{3, 1, 2, 2, 1, 0};
    const Vector2 positions[]{
        { 1.0f, -1.0f},
        { 1.0f,  1.0f},
        {-1.0f, -1.0f},
        {-1.0f,  1.0f}
    };
    MeshData original{MeshPrimitive::Triangles, {}, indicesReversed,
        MeshIndexData{Containers::stridedArrayView(indicesReversed).template flipped<0>()},
        {}, positions, {
            MeshAttributeData{MeshAttribute::Position, Containers::arrayView(positions)},
        }
    };
    Containers::Optional<MeshData> optimized = converter->convert(original);

    /* Same as in copyTriangleStrip2DPositions() */
    CORRADE_VERIFY(optimized);
    CORRADE_COMPARE(optimized->primitive(), MeshPrimitive::Triangles);
    CORRADE_COMPARE(optimized->indexCount(), 6);
    CORRADE_COMPARE(optimized->vertexCount(), original.vertexCount());
    CORRADE_COMPARE(optimized->attributeCount(), original.attributeCount());
    CORRADE_COMPARE(optimized->indexDataFlags(), DataFlag::Owned|DataFlag::Mutable);
    CORRADE_COMPARE(optimized->vertexDataFlags(), DataFlag::Owned|DataFlag::Mutable);

    /* Indices should get tightly packed, but keeping their type */
    CORRADE_COMPARE(optimized->indexType(), Implementation::meshIndexTypeFor<T>());
    CORRADE_COMPARE(optimized->indexStride(), sizeof(T));
    CORRADE_COMPARE_AS(optimized->indices<T>(), Containers::arrayView<T>({
        0, 1, 2, 2, 1, 3
    }), TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(optimized->attribute<Vector2>(MeshAttribute::Position),
        Containers::arrayView<Vector2>({
            { 1.0f, -1.0f},
            { 1.0f,  1.0f},
            {-1.0f, -1.0f},
            {-1.0f,  1.0f}
        }), TestSuite::Compare::Container);
}

void MeshOptimizerSceneConverterTest::copyNegativeAttributeStride() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");

    /* Like copyTriangleStrip2DPositions() but with the index buffer expanded,
       stored in reverse order and negative stride */
    const UnsignedShort indices[]{0, 1, 2, 2, 1, 3};
    const Vector2 positionsReversed[]{
        {-1.0f,  1.0f},
        {-1.0f, -1.0f},
        { 1.0f,  1.0f},
        { 1.0f, -1.0f}
    };
    MeshData original{MeshPrimitive::Triangles, {}, indices,
        MeshIndexData{indices},
        {}, positionsReversed, {
            MeshAttributeData{MeshAttribute::Position, Containers::stridedArrayView(positionsReversed).flipped<0>()},
        }
    };
    Containers::Optional<MeshData> optimized = converter->convert(original);

    /* Same as in copyTriangleStrip2DPositions() */
    CORRADE_VERIFY(optimized);
    CORRADE_COMPARE(optimized->primitive(), MeshPrimitive::Triangles);
    CORRADE_COMPARE(optimized->indexCount(), 6);
    CORRADE_COMPARE(optimized->vertexCount(), original.vertexCount());
    CORRADE_COMPARE(optimized->attributeCount(), original.attributeCount());
    CORRADE_COMPARE(optimized->indexDataFlags(), DataFlag::Owned|DataFlag::Mutable);
    CORRADE_COMPARE(optimized->vertexDataFlags(), DataFlag::Owned|DataFlag::Mutable);

    CORRADE_COMPARE_AS(optimized->indices<UnsignedShort>(), Containers::arrayView<UnsignedShort>({
        0, 1, 2, 2, 1, 3
    }), TestSuite::Compare::Container);

    /* Positions should get a positive stride */
    CORRADE_COMPARE(optimized->attributeStride(MeshAttribute::Position), sizeof(Vector2));
    CORRADE_COMPARE_AS(optimized->attribute<Vector2>(MeshAttribute::Position),
        Containers::arrayView<Vector2>({
            { 1.0f, -1.0f},
            { 1.0f,  1.0f},
            {-1.0f, -1.0f},
            {-1.0f,  1.0f}
        }), TestSuite::Compare::Container);
}

void MeshOptimizerSceneConverterTest::simplifyInPlace() {
    auto&& data = SimplifyErrorData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", false);
    converter->configuration().setValue("optimizeOverdraw", false);
    converter->configuration().setValue("optimizeVertexFetch", false);
    converter->configuration().setValue(data.option, true);

    const UnsignedByte indexData[3]{};
    MeshData mesh{MeshPrimitive::Triangles,
        {}, indexData, MeshIndexData{indexData},
        nullptr, {}, 1};
    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertInPlace(mesh));
    CORRADE_COMPARE(out,
        "Trade::MeshOptimizerSceneConverter::convertInPlace(): mesh simplification can't be performed in-place, use convert() instead\n");
}

void MeshOptimizerSceneConverterTest::simplifyNoPositions() {
    auto&& data = SimplifyErrorData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", false);
    converter->configuration().setValue("optimizeOverdraw", false);
    converter->configuration().setValue("optimizeVertexFetch", false);
    converter->configuration().setValue(data.option, true);

    const UnsignedByte indexData[3]{};
    MeshData mesh{MeshPrimitive::Triangles,
        {}, indexData, MeshIndexData{indexData},
        nullptr, {}, 1};
    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(mesh));
    CORRADE_COMPARE(out,
        "Trade::MeshOptimizerSceneConverter::convert(): optimizeOverdraw and simplify require the mesh to have positions\n");
}

template<class T> void MeshOptimizerSceneConverterTest::simplify() {
    setTestCaseTemplateName(Math::TypeTraits<T>::name());

    /* We're interested only in the simplifier here, nothing else. Reducing to
       half the vertices */
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", false);
    converter->configuration().setValue("optimizeOverdraw", false);
    converter->configuration().setValue("optimizeVertexFetch", false);
    converter->configuration().setValue("simplify", true);
    converter->configuration().setValue("simplifyTargetIndexCountThreshold", 0.5f);
    /* The default 1.0e-2 is too little for this */
    converter->configuration().setValue("simplifyTargetError", 0.25f);

    MeshData sphere = MeshTools::compressIndices(
        Primitives::uvSphereSolid(4, 6, Primitives::UVSphereFlag::TextureCoordinates),
        Implementation::meshIndexTypeFor<T>());
    CORRADE_COMPARE(sphere.indexType(), Implementation::meshIndexTypeFor<T>());
    CORRADE_COMPARE(sphere.indexCount(), 108);
    CORRADE_COMPARE(sphere.vertexCount(), 23);

    Containers::Optional<MeshData> simplified = converter->convert(sphere);
    CORRADE_VERIFY(simplified);
    CORRADE_COMPARE(simplified->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE(simplified->indexCount(), 54); /* The half, yay */

    /* Version 0.22 has 12 vertices instead of 13, with the {1, 0.75} /
       {0, 0.75} texture coordinate seam missing and {0.666667, 0.5} added
       instead. Not sure why. */
    CORRADE_EXPECT_FAIL_IF(MESHOPTIMIZER_VERSION >= 220,
        "MeshOptimizer 0.22 partially merges the seam for some reason");

    CORRADE_COMPARE(simplified->vertexCount(), 13);
    CORRADE_COMPARE_AS(simplified->indices<UnsignedInt>(), Containers::arrayView<UnsignedInt>({
        0, 1, 2, 0, 3, 1, 0, 4, 3, 0, 5, 4, 2, 1, 6, 1, 3, 7, 2, 6, 8, 6, 1, 9,
        6, 9, 8, 1, 7, 9, 7, 3, 10, 7, 10, 9, 3, 4, 10, 4, 5, 11, 4, 11, 10, 8,
        9, 12, 9, 10, 12, 10, 11, 12
    }), TestSuite::Compare::Container);

    /* Attributes should have the seam preserved */
    const Vector3 positionsOrNormals[]{
        {0.0f, -1.0f, 0.0f},
        {0.612372f, -0.707107f, -0.353553f},
        {0.0f, 0.0f, 1.0f}, /* Seam #1 */
        {-0.612372f, -0.707107f, -0.353553f},
        {-0.866025f, 0.0f, 0.5f},
        {0.0f, 0.0f, 1.0f}, /* Seam #1 */
        {0.866025f, 0.0f, 0.5f},
        {0.0f, 0.0f, -1.0f},
        {0.0f, 0.707107f, 0.707107f}, /* Seam #2 */
        {0.612372f, 0.707107f, -0.353553f},
        {-0.612372f, 0.707107f, -0.353553f},
        {0.0f, 0.707107f, 0.707107f}, /* Seam #2 */
        {0.0f, 1.0f, 0.0f}
    };
    CORRADE_COMPARE_AS(simplified->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(simplified->attribute<Vector3>(MeshAttribute::Normal),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);

    CORRADE_COMPARE_AS(simplified->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        Containers::arrayView<Vector2>({
            {0.5f, 0.0f},
            {0.333333f, 0.25f},
            {0.0f, 0.5f}, /* Seam #1 */
            {0.666667f, 0.25f},
            {0.833333f, 0.5f},
            {1.0f, 0.5f}, /* Seam #1 */
            {0.166667f, 0.5f},
            {0.5f, 0.5f},
            {0.0f, 0.75f}, /* Seam #2 */
            {0.333333f, 0.75f},
            {0.666667f, 0.75f},
            {1.0f, 0.75f}, /* Seam #2 */
            {0.5f, 1.0f}
        }), TestSuite::Compare::Container);
}

template<class T> void MeshOptimizerSceneConverterTest::simplifySloppy() {
    setTestCaseTemplateName(Math::TypeTraits<T>::name());

    /* We're interested only in the simplifier here, nothing else. Reducing to
       half the vertices */
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("optimizeVertexCache", false);
    converter->configuration().setValue("optimizeOverdraw", false);
    converter->configuration().setValue("optimizeVertexFetch", false);
    converter->configuration().setValue("simplifySloppy", true);
    converter->configuration().setValue("simplifyTargetIndexCountThreshold", 0.5f);
    /* Used only on 0.16+, on 0.15 simplifyTargetIndexCountThreshold is
       enough. */
    converter->configuration().setValue("simplifyTargetError", 0.5f);

    MeshData sphere = MeshTools::compressIndices(
        Primitives::uvSphereSolid(4, 6, Primitives::UVSphereFlag::TextureCoordinates),
        Implementation::meshIndexTypeFor<T>());
    CORRADE_COMPARE(sphere.indexType(), Implementation::meshIndexTypeFor<T>());
    CORRADE_COMPARE(sphere.indexCount(), 108);
    CORRADE_COMPARE(sphere.vertexCount(), 23);

    Containers::Optional<MeshData> simplified = converter->convert(sphere);
    CORRADE_VERIFY(simplified);
    CORRADE_COMPARE(simplified->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE(simplified->indexCount(), 36); /* Less than a half */
    CORRADE_COMPARE(simplified->vertexCount(), 8);
    CORRADE_COMPARE_AS(simplified->indices<UnsignedInt>(), Containers::arrayView<UnsignedInt>({
        0, 1, 2, 0, 3, 1, 0, 2, 4, 0, 4, 5, 2, 1, 6, 2, 6, 4, 1, 3, 7, 1, 7, 6,
        0, 5, 3, 3, 5, 7, 4, 6, 5, 6, 7, 5
    }), TestSuite::Compare::Container);

    /* Vertex data unique, with no seam preserved ... */
    const Vector3 positionsOrNormals[]{
        {0.0f, -0.707107f, 0.707107f},
        {0.612372f, -0.707107f, -0.353553f},
        {0.612372f, -0.707107f, 0.353553f},
        {-0.612372f, -0.707107f, -0.353553f},
        {0.866025f, 0.0f, 0.5f},
        {-0.612372f, 0.707107f, 0.353553f},
        {0.866025f, 0.0f, -0.5f},
        {0.0f, 0.707107f, -0.707107f}
    };
    CORRADE_COMPARE_AS(simplified->attribute<Vector3>(MeshAttribute::Position),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(simplified->attribute<Vector3>(MeshAttribute::Normal),
        Containers::arrayView(positionsOrNormals),
        TestSuite::Compare::Container);

    /* ... which of course breaks the UVs */
    CORRADE_COMPARE_AS(simplified->attribute<Vector2>(MeshAttribute::TextureCoordinates),
        Containers::arrayView<Vector2>({
            {0.0f, 0.25f},
            {0.333333f, 0.25f},
            {0.166667f, 0.25f},
            {0.666667f, 0.25f},
            {0.166667f, 0.5f},
            {0.833333f, 0.75f},
            {0.333333f, 0.5f},
            {0.5f, 0.75}
        }), TestSuite::Compare::Container);
}

void MeshOptimizerSceneConverterTest::simplifyEmptyIndexBuffer() {
    Vector3 positions[2]{};

    MeshData mesh{MeshPrimitive::Triangles,
        nullptr, MeshIndexData{Containers::ArrayView<UnsignedShort>{}},
        {}, positions, {
            MeshAttributeData{MeshAttribute::Position,
                Containers::arrayView(positions)}
        }};
    CORRADE_VERIFY(mesh.isIndexed());
    CORRADE_COMPARE(mesh.indexCount(), 0);
    CORRADE_COMPARE(mesh.vertexCount(), 2);

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("simplify", true);

    Containers::Optional<MeshData> simplified = converter->convert(mesh);
    CORRADE_VERIFY(simplified);
    CORRADE_COMPARE(simplified->indexCount(), 0);
    CORRADE_COMPARE(simplified->vertexCount(), 0);
    CORRADE_COMPARE(simplified->attributeCount(), 1);
    CORRADE_COMPARE_AS(simplified->attribute<Vector3>(MeshAttribute::Position),
        nullptr,
        TestSuite::Compare::Container);
}

void MeshOptimizerSceneConverterTest::simplifyVerbose() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->setFlags(SceneConverterFlag::Verbose);
    /* Without these three, meshoptimizer 0.15 produces 12 vertices while 0.14
       13 vertices */
    converter->configuration().setValue("optimizeVertexCache", false);
    converter->configuration().setValue("optimizeOverdraw", false);
    converter->configuration().setValue("optimizeVertexFetch", false);
    converter->configuration().setValue("simplify", true);
    converter->configuration().setValue("simplifyTargetIndexCountThreshold", 0.5f);
    /* The default 1.0e-2 is too little for this */
    converter->configuration().setValue("simplifyTargetError", 0.25f);

    Containers::String out;
    Containers::Optional<MeshData> simplified;
    {
        Debug redirectDebug{&out};
        simplified = converter->convert(Primitives::uvSphereSolid(4, 6, Primitives::UVSphereFlag::TextureCoordinates));
    }
    CORRADE_VERIFY(simplified);
    CORRADE_COMPARE(simplified->indexType(), MeshIndexType::UnsignedInt);
    CORRADE_COMPARE(simplified->indexCount(), 54); /* The half, yay */

    /* Version 0.22 has 12 vertices instead of 13, with the {1, 0.75} /
       {0, 0.75} texture coordinate seam missing and {0.666667, 0.5} added
       instead. Not sure why. */
    CORRADE_EXPECT_FAIL_IF(MESHOPTIMIZER_VERSION >= 220,
        "MeshOptimizer 0.22 partially merges the seam for some reason");

    CORRADE_COMPARE(simplified->vertexCount(), 13);

    const char* expected = R"(Trade::MeshOptimizerSceneConverter::convert(): processing stats:
  vertex cache:
    23 -> 13 transformed vertices
    1 -> 1 executed warps
    ACMR 0.638889 -> 0.722222
    ATVR 1 -> 1
  vertex fetch:
    768 -> 448 bytes fetched
    overfetch 1.04348 -> 1.07692
  overdraw:
    127149 -> 131437 shaded pixels
    127149 -> 131437 covered pixels
    overdraw 1 -> 1
)";
    CORRADE_COMPARE(out, expected);
}

void MeshOptimizerSceneConverterTest::simplifyEmpty() {
    auto&& data = SimplifyEmptyData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    MeshData mesh = MeshTools::generateIndices(Primitives::planeSolid());

    MeshData input{mesh.primitive(),
        {}, data.inputIndexCount ? nullptr : mesh.indexData(),
            MeshIndexData{mesh.indices().prefix(data.inputIndexCount ? *data.inputIndexCount : mesh.indexCount())},
        {}, mesh.vertexData(),
            meshAttributeDataNonOwningArray(mesh.attributeData())};

    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");
    converter->configuration().setValue("simplifySloppy", true);
    converter->configuration().setValue("simplifyTargetIndexCountThreshold", 0.5f);
    converter->configuration().setValue("simplifyTargetError", data.targetError);
    if(data.failEmpty)
        converter->configuration().setValue("simplifyFailEmpty", *data.failEmpty);

    Containers::String out;
    Containers::Optional<MeshData> simplified;
    {
        Error redirectError{&out};
        simplified = converter->convert(input);
    }

    if(data.message) {
        CORRADE_VERIFY(!simplified);
        CORRADE_COMPARE(out, data.message);
    } else {
        CORRADE_VERIFY(simplified);
        CORRADE_COMPARE(simplified->indexCount(), 0);
        CORRADE_COMPARE(out, "");
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::MeshOptimizerSceneConverterTest)
