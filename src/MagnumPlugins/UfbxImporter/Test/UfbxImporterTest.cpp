/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2022 Samuli Raivio <bqqbarbhg@gmail.com>

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
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct UfbxImporterTest: TestSuite::Tester {
    explicit UfbxImporterTest();

    void dummy();

    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

UfbxImporterTest::UfbxImporterTest() {
    addTests({&UfbxImporterTest::dummy});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. */
    #ifdef UFBXIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(UFBXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif

    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    _manager.setPluginDirectory({});
    #endif
}

void UfbxImporterTest::dummy() {
    CORRADE_COMPARE(1 + 1, 2);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::UfbxImporterTest)
