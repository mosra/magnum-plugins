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

#include <sstream>
#include <Corrade/Containers/Optional.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Magnum/MeshTools/CompressIndices.h>
#include <Magnum/Primitives/Icosphere.h>
#include <Magnum/Trade/AbstractSceneConverter.h>
#include <Magnum/Trade/MeshData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct MeshOptimizerSceneConverterTest: TestSuite::Tester {
    explicit MeshOptimizerSceneConverterTest();

    void optimizeVertexCache();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractSceneConverter> _manager{"nonexistent"};
};

MeshOptimizerSceneConverterTest::MeshOptimizerSceneConverterTest() {
    addTests({&MeshOptimizerSceneConverterTest::optimizeVertexCache});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef MESHOPTIMIZERSCENECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(MESHOPTIMIZERSCENECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void MeshOptimizerSceneConverterTest::optimizeVertexCache() {
    Containers::Pointer<AbstractSceneConverter> converter = _manager.instantiate("MeshOptimizerSceneConverter");

    /* Tried with a cubeSolid() first, but that one seems to have an optimal
       layout already, hah. */
    Trade::MeshData icosphere = Primitives::icosphereSolid(0);
    CORRADE_COMPARE_AS(icosphere.indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({
            1, 2, 6, 1, 7, 2, 3, 4, 5, 4, 3, 8,
            6, 5, 11, 5, 6, 10, 9, 10, 2, 10, 9, 3,
            7, 8, 9, 8, 7, 0, 11, 0, 1, 0, 11, 4,
            6, 2, 10, 1, 6, 11, 3, 5, 10, 5, 4, 11,
            2, 7, 9, 7, 1, 0, 3, 9, 8, 4, 8, 0
        }), TestSuite::Compare::Container);

    CORRADE_VERIFY(converter->convertInPlace(icosphere));
    CORRADE_COMPARE_AS(icosphere.indices<UnsignedInt>(),
        Containers::arrayView<UnsignedInt>({
            1, 2, 6, 6, 2, 10, 1, 7, 2, 9, 10, 2,
            2, 7, 9, 7, 1, 0, 1, 6, 11, 11, 0, 1,
            5, 6, 10, 6, 5, 11, 10, 9, 3, 3, 5, 10,
            7, 8, 9, 3, 9, 8, 8, 7, 0, 3, 4, 5,
            4, 3, 8, 5, 4, 11, 4, 8, 0, 0, 11, 4
        }), TestSuite::Compare::Container);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::MeshOptimizerSceneConverterTest)
