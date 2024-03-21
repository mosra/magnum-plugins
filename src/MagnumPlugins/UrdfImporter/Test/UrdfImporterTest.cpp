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

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Algorithms.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct UrdfImporterTest: TestSuite::Tester {
    explicit UrdfImporterTest();

    void openMemory();
    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

/* Shared among all plugins that implement data copying optimizations */
const struct {
    const char* name;
    bool(*open)(AbstractImporter&, Containers::ArrayView<const void>);
} OpenMemoryData[]{
    {"data", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        /* Copy to ensure the original memory isn't referenced */
        Containers::Array<char> copy{NoInit, data.size()};
        Utility::copy(Containers::arrayCast<const char>(data), copy);
        return importer.openData(copy);
    }},
    {"memory", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        return importer.openMemory(data);
    }},
};

UrdfImporterTest::UrdfImporterTest() {
    addInstancedTests({&UrdfImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&UrdfImporterTest::openTwice,
              &UrdfImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef URDFIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(URDFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void UrdfImporterTest::openMemory() {
    /* Same as (a subset of) binary() except that it uses openData() &
       openMemory() instead of openFile() to test data copying on import */

    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UrdfImporter");
    // TODO
}

void UrdfImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UrdfImporter");

    // TODO

    /* Shouldn't crash, leak or anything */
}

void UrdfImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("UrdfImporter");

    /* Verify that everything is working the same way on second use */
    // TODO
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::UrdfImporterTest)
