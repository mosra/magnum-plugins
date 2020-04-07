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

#include "StlImporter.h"

#include <cstring>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Endianness.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/MeshData.h>

namespace Magnum { namespace Trade {

StlImporter::StlImporter() = default;

StlImporter::StlImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

StlImporter::~StlImporter() = default;

ImporterFeatures StlImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool StlImporter::doIsOpened() const { return !!_in; }

void StlImporter::doClose() { _in = nullptr; }

namespace {
    /* In the input file, the triangle is represented by 12 floats (3D normal
       followed by three 3D vertices) and 2 extra bytes. */
    constexpr std::ptrdiff_t InputTriangleStride = 12*4 + 2;
}

void StlImporter::doOpenData(Containers::ArrayView<const char> data) {
    /* At this point we can't even check if it's an ASCII or binary file, bail
       out */
    if(data.size() < 5) {
        Error{} << "Trade::StlImporter::openData(): file too short, got only" << data.size() << "bytes";
        return;
    }

    if(std::strncmp(data, "solid", 5) == 0) {
        Error{} << "Trade::StlImporter::openData(): ASCII STL files are not supported, sorry";
        return;
    }

    if(data.size() < 84) {
        Error{} << "Trade::StlImporter::openData(): file too short, expected at least 84 bytes but got" << data.size();
        return;
    }

    const UnsignedInt triangleCount = Utility::Endianness::littleEndian(*reinterpret_cast<const UnsignedInt*>(data + 80));
    const Containers::ArrayView<const char> triangleData = data.suffix(84);
    const std::size_t expectedSize = InputTriangleStride*triangleCount;
    if(triangleData.size() != expectedSize) {
        Error{} << "Trade::StlImporter::openData(): file size doesn't match triangle count, expected" << 84 + expectedSize << "but got" << data.size() << "for" << triangleCount << "triangles";
        return;
    }

    _in = Containers::Array<char>{Containers::NoInit, triangleData.size()};
    Utility::copy(triangleData, _in);
}

UnsignedInt StlImporter::doMeshCount() const { return 1; }

Containers::Optional<MeshData> StlImporter::doMesh(UnsignedInt, UnsignedInt) {
    /* Make 2D views on input normals and positions */
    const std::size_t triangleCount = _in.size()/InputTriangleStride;
    Containers::StridedArrayView2D<const Vector3> inputNormals{_in,
        reinterpret_cast<const Vector3*>(_in.data() + 0),
        {triangleCount, 3}, {InputTriangleStride, 0}};
    Containers::StridedArrayView2D<const Vector3> inputPositions{_in,
        reinterpret_cast<const Vector3*>(_in.data() + sizeof(Vector3)), {triangleCount, 3}, {InputTriangleStride, sizeof(Vector3)}};

    /* The output stores a 3D position and 3D normal for each vertex */
    constexpr std::ptrdiff_t outputVertexStride = 6*4;
    Containers::Array<char> vertexData{Containers::NoInit, std::size_t(3*outputVertexStride*triangleCount)};
    Containers::StridedArrayView2D<Vector3> outputPositions{vertexData,
        reinterpret_cast<Vector3*>(vertexData.data() + 0),
        {triangleCount, 3}, {outputVertexStride*3, outputVertexStride}};
    Containers::StridedArrayView2D<Vector3> outputNormals{vertexData,
        reinterpret_cast<Vector3*>(vertexData.data() + sizeof(Vector3)),
        {triangleCount, 3}, {outputVertexStride*3, outputVertexStride}};

    /* Copy the things over */
    Utility::copy(inputNormals, outputNormals);
    Utility::copy(inputPositions, outputPositions);

    Containers::StridedArrayView1D<Vector3> positions{vertexData,
        reinterpret_cast<Vector3*>(vertexData.data() + 0),
        3*triangleCount, outputVertexStride};
    Containers::StridedArrayView1D<Vector3> normals{vertexData,
        reinterpret_cast<Vector3*>(vertexData.data() + sizeof(Vector3)),
        3*triangleCount, outputVertexStride};
    return MeshData{MeshPrimitive::Triangles, std::move(vertexData), {
        Trade::MeshAttributeData{MeshAttribute::Position, positions},
        Trade::MeshAttributeData{MeshAttribute::Normal, normals}}};
}

}}

CORRADE_PLUGIN_REGISTER(StlImporter, Magnum::Trade::StlImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.1")
