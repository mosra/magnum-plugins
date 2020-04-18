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

#include "MeshOptimizerSceneConverter.h"

#include <Magnum/Trade/MeshData.h>
#include <meshoptimizer.h>

namespace Magnum { namespace Trade {

MeshOptimizerSceneConverter::MeshOptimizerSceneConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractSceneConverter{manager, plugin} {}

MeshOptimizerSceneConverter::~MeshOptimizerSceneConverter() = default;

SceneConverterFeatures MeshOptimizerSceneConverter::doFeatures() const { return SceneConverterFeature::ConvertMeshInPlace; }

// TODO: configurable option, enabled by default, do nothing if not enabled
// TODO: convert index buffer to uint if not already
// TODO: fail if immutable
// TODO: fail if a strip etc, as reordering breaks that

bool MeshOptimizerSceneConverter::doConvertInPlace(MeshData& mesh) {
    const auto indices = mesh.mutableIndices<UnsignedInt>();
    meshopt_optimizeVertexCache(indices, indices, mesh.indexCount(), mesh.vertexCount());
    return true;
}

}}

CORRADE_PLUGIN_REGISTER(MeshOptimizerSceneConverter, Magnum::Trade::MeshOptimizerSceneConverter,
    "cz.mosra.magnum.Trade.AbstractSceneConverter/0.1")
