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
#include <Corrade/Containers/Optional.h>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/File.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/AbstractSceneConverter.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct GltfSceneConverterTest: TestSuite::Tester {
    explicit GltfSceneConverterTest();

    void empty();
    void outputFormatDetectionToData();
    void outputFormatDetectionToFile();
    void metadata();

    void abort();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractSceneConverter> _converterManager{"nonexistent"};
    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _importerManager;
};

using namespace Containers::Literals;

const struct {
    const char* name;
    bool binary;
    Containers::StringView suffix;
} FileVariantData[] {
    {"text", false, ".gltf"},
    {"binary", true, ".glb"}
};

const struct {
    const char* name;
    Containers::Optional<bool> binary;
    const char* expected;
} OutputFormatDetectionToDataData[]{
    {"default", {}, "empty.glb"},
    {"binary=false", false, "empty.gltf"},
    {"binary=true", true, "empty.glb"}
};

const struct {
    const char* name;
    Containers::Optional<bool> binary;
    Containers::StringView suffix;
    const char* expected;
} OutputFormatDetectionToFileData[]{
    {".gltf", {}, ".gltf", "empty.gltf"},
    {".gltf + binary=false", false, ".gltf", "empty.gltf"},
    {".gltf + binary=true", true, ".gltf", "empty.glb"},
    {".glb", {}, ".glb", "empty.glb"},
    {".glb + binary=false", false, ".gltf", "empty.gltf"},
    {".glb + binary=true", true, ".gltf", "empty.glb"},
    {"arbitrary extension", {}, ".foo", "empty.glb"}
};

GltfSceneConverterTest::GltfSceneConverterTest() {
    addInstancedTests({&GltfSceneConverterTest::empty},
        Containers::arraySize(FileVariantData));

    addInstancedTests({&GltfSceneConverterTest::outputFormatDetectionToData},
        Containers::arraySize(OutputFormatDetectionToDataData));

    addInstancedTests({&GltfSceneConverterTest::outputFormatDetectionToFile},
        Containers::arraySize(OutputFormatDetectionToFileData));

    addTests({&GltfSceneConverterTest::metadata,
              &GltfSceneConverterTest::abort});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef GLTFSCENECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(GLTFSCENECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Load the importer plugin directly from the build tree. Otherwise it's
       static and already loaded. It also pulls in the AnyImageImporter
       dependency. */
    #ifdef GLTFIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(GLTFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    _importerManager.setPluginDirectory({});
    #endif

    /* By default don't write the generator name for smaller test files */
    CORRADE_INTERNAL_ASSERT_EXPRESSION(_converterManager.metadata("GltfSceneConverter"))->configuration().setValue("generator", "");

    /* Create the output directory if it doesn't exist yet */
    CORRADE_INTERNAL_ASSERT_OUTPUT(Utility::Path::make(GLTFSCENECONVERTER_TEST_OUTPUT_DIR));
}

void GltfSceneConverterTest::empty() {
    auto&& data = FileVariantData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->configuration().setValue("binary", data.binary);

    converter->beginData();
    Containers::Optional<Containers::Array<char>> out = converter->endData();
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "empty" + data.suffix),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

    /* The file should load without errors */
    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    CORRADE_VERIFY(importer->openData(*out));
}

void GltfSceneConverterTest::outputFormatDetectionToData() {
    auto&& data = OutputFormatDetectionToDataData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    if(data.binary)
        converter->configuration().setValue("binary", *data.binary);

    converter->beginData();
    Containers::Optional<Containers::Array<char>> out = converter->endData();
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::StringToFile);

    /* File contents verified in empty() already, this just verifies that a
       correct output format was chosen */
}

void GltfSceneConverterTest::outputFormatDetectionToFile() {
    auto&& data = OutputFormatDetectionToFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    if(data.binary)
        converter->configuration().setValue("binary", *data.binary);

    const Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "file" + data.suffix);

    CORRADE_VERIFY(converter->beginFile(filename));
    CORRADE_VERIFY(converter->endFile());
    CORRADE_COMPARE_AS(filename,
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::File);

    /* File contents verified in empty() already, this just verifies that a
       correct output format was chosen */
}

void GltfSceneConverterTest::metadata() {
    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->configuration().setValue("binary", false);

    converter->configuration().setValue("generator", "I have bugs, do I?");
    converter->configuration().setValue("copyright", "© always, Me Mememe ME");
    converter->configuration().addValue("extensionUsed", "MAGNUM_exported_this_file");
    converter->configuration().addValue("extensionUsed", "MAGNUM_can_write_json");
    converter->configuration().addValue("extensionRequired", "MAGNUM_is_amazing");
    converter->configuration().addValue("extensionRequired", "MAGNUM_exported_this_file");

    converter->beginData();
    Containers::Optional<Containers::Array<char>> out = converter->endData();
    CORRADE_VERIFY(out);

    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "metadata.gltf"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("GltfImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("GltfImporter plugin not found, cannot test a rountrip");

    /* The file should load if we ignore required extensions */
    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("GltfImporter");
    {
        Error silenceError{nullptr};
        CORRADE_VERIFY(!importer->openData(*out));
    }
    importer->configuration().setValue("ignoreRequiredExtensions", true);
    CORRADE_VERIFY(importer->openData(*out));
    /** @todo once ImporterExtraAttribute is a thing, verify these are parsed */
}

void GltfSceneConverterTest::abort() {
    Containers::Pointer<AbstractSceneConverter> converter =  _converterManager.instantiate("GltfSceneConverter");
    converter->configuration().setValue("binary", false);

    Containers::String filename = Utility::Path::join(GLTFSCENECONVERTER_TEST_OUTPUT_DIR, "file.gltf");
    CORRADE_VERIFY(converter->beginFile(filename));

    /* Starting a new file should clean up the previous state */
    converter->beginData();
    Containers::Optional<Containers::Array<char>> out = converter->endData();
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(GLTFSCENECONVERTER_TEST_DIR, "empty.gltf"),
        TestSuite::Compare::StringToFile);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::GltfSceneConverterTest)
