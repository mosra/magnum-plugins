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

#include <sstream>
#include <Corrade/Containers/Optional.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct StbDxtImageConverterTest: TestSuite::Tester {
    explicit StbDxtImageConverterTest();

    void unsupportedFormat();
    void unsupportedSize();

    void rgba();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

const struct {
    const char* name;
    Containers::Optional<bool> alpha;
    Containers::Optional<bool> highQuality;
    Containers::Optional<PixelFormat> overrideInputFormat;
    CompressedPixelFormat expectedFormat;
    const char* expectedFile;
} RgbaData[] {
    {"", {}, {}, {},
        CompressedPixelFormat::Bc3RGBAUnorm, "ship.bc3"},
    {"high quality", {}, true, {},
        CompressedPixelFormat::Bc3RGBAUnorm, "ship-hq.bc3"},
    {"sRGB", {}, {}, PixelFormat::RGBA8Srgb,
        CompressedPixelFormat::Bc3RGBASrgb, "ship.bc3"},
    {"no alpha", false, {}, {},
        CompressedPixelFormat::Bc1RGBUnorm, "ship.bc1"},
    {"no alpha + sRGB", false, {}, PixelFormat::RGBA8Srgb,
        CompressedPixelFormat::Bc1RGBSrgb, "ship.bc1"},
};

StbDxtImageConverterTest::StbDxtImageConverterTest() {
    addTests({&StbDxtImageConverterTest::unsupportedFormat,
              &StbDxtImageConverterTest::unsupportedSize});

    addInstancedTests({&StbDxtImageConverterTest::rgba},
        Containers::arraySize(RgbaData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STBDXTIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(STBDXTIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* The StbImageImporter is optional */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void StbDxtImageConverterTest::unsupportedFormat() {
    ImageView2D image{PixelFormat::RGB8Unorm, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!_converterManager.instantiate("StbDxtImageConverter")->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbDxtImageConverter::convert(): unsupported format PixelFormat::RGB8Unorm\n");
}

void StbDxtImageConverterTest::unsupportedSize() {
    ImageView2D image{PixelFormat::RGBA8Unorm, {15, 17}};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!_converterManager.instantiate("StbDxtImageConverter")->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbDxtImageConverter::convert(): expected size to be divisible by 4, got Vector(15, 17)\n");
}

void StbDxtImageConverterTest::rgba() {
    auto&& data = RgbaData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    /* Force four channels */
    importer->configuration().setValue("forceChannelCount", 4);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STBDXTIMAGECONVERTER_TEST_DIR, "ship.jpg")));
    Containers::Optional<Trade::ImageData2D> uncompressed = importer->image2D(0);
    CORRADE_VERIFY(uncompressed);
    CORRADE_COMPARE(uncompressed->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE(uncompressed->size(), (Vector2i{160, 96}));

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbDxtImageConverter");
    if(data.alpha)
        converter->configuration().setValue("alpha", *data.alpha);
    if(data.highQuality)
        converter->configuration().setValue("highQuality", *data.highQuality);

    Containers::Optional<Trade::ImageData2D> compressed;
    if(data.overrideInputFormat) {
        compressed = converter->convert(ImageView2D{*data.overrideInputFormat, uncompressed->size(), uncompressed->data()});
    } else compressed = converter->convert(*uncompressed);
    CORRADE_VERIFY(compressed);
    CORRADE_VERIFY(compressed->isCompressed());
    CORRADE_COMPARE(compressed->compressedFormat(), data.expectedFormat);
    CORRADE_COMPARE(compressed->size(), (Vector2i{160, 96}));
    /* The data should be exactly the size of 4x4 128-bit blocks for BC3 and
       64-bit blocks for BC1 (without alpha) */
    /** @todo drop this and let the ImageData constructor take care of this? */
    CORRADE_COMPARE(compressed->data().size(),
        compressed->size().product()*(!data.alpha || *data.alpha ? 16 : 8)/16);

    CORRADE_COMPARE_AS((std::string{compressed->data(), compressed->data().size()}),
        Utility::Directory::join(STBDXTIMAGECONVERTER_TEST_DIR, data.expectedFile),
        TestSuite::Compare::StringToFile);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StbDxtImageConverterTest)
