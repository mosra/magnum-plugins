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
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
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
    void emptyImage();
    void array1D();

    void rgba();
    void threeDimensions();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

const struct {
    const char* name;
    Int channelCount;
    Containers::Optional<bool> alpha;
    Containers::Optional<bool> highQuality;
    Containers::Optional<PixelFormat> overrideInputFormat;
    ImageFlags2D flags;
    CompressedPixelFormat expectedFormat;
    const char* expectedFile;
} RgbaData[] {
    {"RGBA", 4, {}, {}, {}, {},
        CompressedPixelFormat::Bc3RGBAUnorm, "ship.bc3"},
    {"RGBA, high quality", 4, {}, true, {}, {},
        CompressedPixelFormat::Bc3RGBAUnorm, "ship-hq.bc3"},
    {"RGBA, sRGB", 4, {}, {}, PixelFormat::RGBA8Srgb, {},
        CompressedPixelFormat::Bc3RGBASrgb, "ship.bc3"},
    {"RGBA, alpha disabled", 4, false, {}, {}, {},
        CompressedPixelFormat::Bc1RGBUnorm, "ship.bc1"},
    {"RGBA, alpha disabled, sRGB", 4, false, {}, PixelFormat::RGBA8Srgb, {},
        CompressedPixelFormat::Bc1RGBSrgb, "ship.bc1"},
    {"RGB", 3, {}, {}, {}, {},
        CompressedPixelFormat::Bc1RGBUnorm, "ship.bc1"},
    {"RGB, sRGB", 3, {}, {}, PixelFormat::RGB8Srgb, {},
        CompressedPixelFormat::Bc1RGBSrgb, "ship.bc1"},
    {"RGB, alpha enabled", 3, true, {}, {}, {},
        CompressedPixelFormat::Bc3RGBAUnorm, "ship.bc3"},
    {"RGB, alpha enabled, sRGB", 3, true, {}, PixelFormat::RGB8Srgb, {},
        CompressedPixelFormat::Bc3RGBASrgb, "ship.bc3"},
    {"flag passthrough", 4, {}, {}, PixelFormat::RGBA8Unorm, ImageFlag2D(0xdea0),
        CompressedPixelFormat::Bc3RGBAUnorm, "ship.bc3"},
};

StbDxtImageConverterTest::StbDxtImageConverterTest() {
    addTests({&StbDxtImageConverterTest::unsupportedFormat,
              &StbDxtImageConverterTest::unsupportedSize,
              &StbDxtImageConverterTest::emptyImage,
              &StbDxtImageConverterTest::array1D});

    addInstancedTests({&StbDxtImageConverterTest::rgba},
        Containers::arraySize(RgbaData));

    addTests({&StbDxtImageConverterTest::threeDimensions});

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
    ImageView2D image{PixelFormat::RG8Unorm, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!_converterManager.instantiate("StbDxtImageConverter")->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbDxtImageConverter::convert(): unsupported format PixelFormat::RG8Unorm\n");
}

void StbDxtImageConverterTest::unsupportedSize() {
    ImageView2D image{PixelFormat::RGBA8Unorm, {15, 17}};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!_converterManager.instantiate("StbDxtImageConverter")->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbDxtImageConverter::convert(): expected size to be divisible by 4, got Vector(15, 17)\n");
}

void StbDxtImageConverterTest::emptyImage() {
    ImageView2D image{PixelFormat::RGBA8Unorm, {}, nullptr};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbDxtImageConverter");
    Containers::Optional<Trade::ImageData2D> out = converter->convert(image);
    CORRADE_VERIFY(out);
    CORRADE_VERIFY(out->isCompressed());
    CORRADE_COMPARE(out->size(), Vector2i{});
    CORRADE_COMPARE(out->compressedFormat(), CompressedPixelFormat::Bc3RGBAUnorm);
}

void StbDxtImageConverterTest::array1D() {
    ImageView2D image{PixelFormat::RGBA8Unorm, {4, 4}, ImageFlag2D::Array};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!_converterManager.instantiate("StbDxtImageConverter")->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbDxtImageConverter::convert(): 1D array images are not supported\n");
}

void StbDxtImageConverterTest::rgba() {
    auto&& data = RgbaData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    importer->configuration().setValue("forceChannelCount", data.channelCount);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(STBDXTIMAGECONVERTER_TEST_DIR, "ship.jpg")));
    Containers::Optional<Trade::ImageData2D> uncompressed = importer->image2D(0);
    CORRADE_VERIFY(uncompressed);
    CORRADE_COMPARE(pixelFormatChannelCount(uncompressed->format()), data.channelCount);
    CORRADE_COMPARE(uncompressed->size(), (Vector2i{160, 96}));

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbDxtImageConverter");
    if(data.alpha)
        converter->configuration().setValue("alpha", *data.alpha);
    if(data.highQuality)
        converter->configuration().setValue("highQuality", *data.highQuality);

    Containers::Optional<Trade::ImageData2D> compressed;
    if(data.overrideInputFormat) {
        compressed = converter->convert(ImageView2D{*data.overrideInputFormat, uncompressed->size(), uncompressed->data(), data.flags});
    } else compressed = converter->convert(*uncompressed);
    CORRADE_VERIFY(compressed);
    CORRADE_VERIFY(compressed->isCompressed());
    CORRADE_COMPARE(compressed->flags(), data.flags);
    CORRADE_COMPARE(compressed->compressedFormat(), data.expectedFormat);
    CORRADE_COMPARE(compressed->size(), (Vector2i{160, 96}));
    /* The data should be exactly the size of 4x4 128-bit blocks for BC3 and
       64-bit blocks for BC1 (without alpha) */
    /** @todo drop this and let the ImageData constructor take care of this? */
    CORRADE_COMPARE(compressed->data().size(),
        compressed->size().product()*compressedPixelFormatBlockDataSize(data.expectedFormat)/16);

    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{compressed->data()},
        Utility::Path::join(STBDXTIMAGECONVERTER_TEST_DIR, data.expectedFile),
        TestSuite::Compare::StringToFile);
}

void StbDxtImageConverterTest::threeDimensions() {
    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(STBDXTIMAGECONVERTER_TEST_DIR, "ship.jpg")));
    Containers::Optional<Trade::ImageData2D> uncompressed = importer->image2D(0);
    CORRADE_VERIFY(uncompressed);
    CORRADE_COMPARE(uncompressed->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE(uncompressed->size(), (Vector2i{160, 96}));

    /* Be lazy and just cut up the input 2D image to three horizontal slices,
       forming a 3D input. Set also an array flag to verify it's passed
       through unchanged. */
    ImageView3D uncompressed3D{uncompressed->format(), {160, 32, 3}, uncompressed->data(), ImageFlag3D::Array|ImageFlag3D(0xdea0)};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbDxtImageConverter");
    Containers::Optional<Trade::ImageData3D> compressed = converter->convert(uncompressed3D);
    CORRADE_VERIFY(compressed);
    CORRADE_VERIFY(compressed->isCompressed());
    CORRADE_COMPARE(compressed->flags(), ImageFlag3D::Array|ImageFlag3D(0xdea0));
    CORRADE_COMPARE(compressed->compressedFormat(), CompressedPixelFormat::Bc1RGBUnorm);
    CORRADE_COMPARE(compressed->size(), (Vector3i{160, 32, 3}));

    /* The output data should be exactly the same as for a 2D case, as it's
       just the same input but in a different shape */
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{compressed->data()},
        Utility::Path::join(STBDXTIMAGECONVERTER_TEST_DIR, "ship.bc1"),
        TestSuite::Compare::StringToFile);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StbDxtImageConverterTest)
