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

#include <sstream>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct CgltfImporterTest: TestSuite::Tester {
    explicit CgltfImporterTest();

    /* Just a very rudimentary test to verify that configuration options are
       correctly exposed and propagated to the base. Everything else is tested
       in GltfImporter. */
    void requiredExtensionsUnsupported();
    void requiredExtensionsUnsupportedDisabled();
    /* Ensure this is still supported -- for Cgltf/TinyGltf, the API was used
       to make them preferred over Assimp that's picked because it's lexically
       first */
    void setPreferredPlugins();

    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

CgltfImporterTest::CgltfImporterTest() {
    addTests({&CgltfImporterTest::requiredExtensionsUnsupported,
              &CgltfImporterTest::requiredExtensionsUnsupportedDisabled,

              &CgltfImporterTest::setPreferredPlugins});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. It also pulls in the AnyImageImporter dependency. */
    #if defined(GLTFIMPORTER_PLUGIN_FILENAME) && defined(CGLTFIMPORTER_PLUGIN_FILENAME)
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(GLTFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(CGLTFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    _manager.setPluginDirectory({});
    #endif
}

void CgltfImporterTest::requiredExtensionsUnsupported() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    /* The option should be present and disabled by default */
    CORRADE_COMPARE(importer->configuration().value("ignoreRequiredExtensions"), "false");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(CGLTFIMPORTER_TEST_DIR, "required-extensions-unsupported.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::openData(): required extension EXT_lights_image_based not supported\n");
}

void CgltfImporterTest::requiredExtensionsUnsupportedDisabled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("CgltfImporter");
    CORRADE_VERIFY(importer->configuration().setValue("ignoreRequiredExtensions", true));

    std::ostringstream out;
    Warning redirectError{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(CGLTFIMPORTER_TEST_DIR, "required-extensions-unsupported.gltf")));
    CORRADE_COMPARE(out.str(), "Trade::GltfImporter::openData(): required extension EXT_lights_image_based not supported\n");
}

void CgltfImporterTest::setPreferredPlugins() {
    _manager.setPreferredPlugins("GltfImporter", {"CgltfImporter"});

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("GltfImporter");
    CORRADE_COMPARE(importer->metadata()->name(), "CgltfImporter");
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::CgltfImporterTest)
