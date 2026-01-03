/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2023 Melike Batihan <melikebatihan@gmail.com>

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

#include <webp/encode.h> /* for version info in importFailed() */
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct WebPImageConverterTest: TestSuite::Tester {
    explicit WebPImageConverterTest();

    void invalidConfiguration();
    void invalidFormat();

    void rgb();
    void rgba();
    void importFailed();
    void encodingFailed();

    void unsupportedMetadata();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _manager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

const struct {
    const char* name;
    const char* preset;
    Containers::Optional<Int> lossless;
    Containers::Optional<Float> lossy;
    Containers::Optional<Int> alphaQuality;
    const char* expectedError;
} InvalidConfigurationData[]{
    {"invalid preset", "portrait", {}, {}, {},
        "expected preset to be one of lossless, default, picture, photo, drawing, icon or text but got portrait"},
    {"invalid lossless level", nullptr, 10, {}, {},
        "cannot apply a lossless preset with level 10"},
    {"invalid lossy quality", "photo", {}, 100.1f, {},
        "cannot apply a photo preset with quality 100.1"},
    {"invalid alpha quality", nullptr, {}, {}, 101,
        "option validation failed, check the alphaQuality configuration option"},
};

const struct {
    const char* name;
    const char* preset;
    Containers::Optional<Int> lossless;
    Containers::Optional<Float> lossy;
    Containers::Optional<bool> useArgb;
    Float maxThreshold, meanThreshold;
    std::size_t maxSize;
} RgbData[]{
    {"default",
        /* Should have no difference */
        nullptr, {}, {}, {}, 0.0f, 0.0f, 118},
    {"lossless, worst compression",
        /* Should have no difference either but be bigger */
        "lossless", 0, {}, {}, 0.0f, 0.0f, 146},
    {"lossless, YUV",
        /* YUV breaks the losslessness slightly but may result in a smaller
           file */
        "lossless", {}, {}, false, 3.34f, 1.67f, 116},
    {"lossy, default, default quality",
        /* Is it "okay" or "meh"? Probably worse than JXL in any case. */
        "default", {}, {}, {}, 13.34f, 5.67f, 76},
    {"lossy, picture, default quality",
        "picture", {}, {}, {}, 13.67f, 6.38f, 76},
    {"lossy, photo, default quality",
        /* Interestingly the output is the same as the picture preset */
        "photo", {}, {}, {}, 13.67f, 6.38f, 76},
    {"lossy, photo, default quality, ARGB",
        /* Interestingly enough the file size is the same but it's different
           less */
        "photo", {}, {}, true, 11.67f, 6.12f, 76},
    {"lossy, icon, quality 100",
        "icon", {}, 100.0f, {}, 3.67f, 1.84f, 144},
    {"lossy, text, quality 0",
        "text", {}, 0.0f, {}, 49.34f, 24.6f, 52},
};

const struct {
    const char* name;
    const char* preset;
    /* The lossless, lossy and useArgb options are tested well enough in rgb(),
       this verifies only what's specific to RGBA */
    Containers::Optional<bool> exactTransparentRgb;
    Containers::Optional<Int> alphaQuality;
    Float maxThreshold, meanThreshold;
    std::size_t maxSize;
} RgbaData[]{
    {"default",
        nullptr, {}, {}, 0.0f, 0.0f, 172},
    {"lossless, don't preserve exact transparent RGB",
        /* Three pixels have zero alpha */
        "lossless", false, {}, 130.25f, 8.91f, 168},
    {"lossy, drawing",
        "drawing", {}, {}, 71.0f, 9.16f, 132},
    {"lossy, drawing, preserve exact transparent RGB",
        "drawing", true, {}, 9.25f, 3.78f, 130},
    {"lossy, drawing, alpha quality 0",
        "drawing", {}, 0, 71.0f, 23.34f, 124},
};

const struct {
    const char* name;
    ImageConverterFlags converterFlags;
    ImageFlags2D imageFlags;
    const char* message;
} UnsupportedMetadataData[]{
    {"1D array", {}, ImageFlag2D::Array,
        "1D array images are unrepresentable in WebP, saving as a regular 2D image"},
    {"1D array, quiet", ImageConverterFlag::Quiet, ImageFlag2D::Array,
        nullptr},
};

WebPImageConverterTest::WebPImageConverterTest() {
    addInstancedTests({&WebPImageConverterTest::invalidConfiguration},
        Containers::arraySize(InvalidConfigurationData));

    addTests({&WebPImageConverterTest::invalidFormat});

    addInstancedTests({&WebPImageConverterTest::rgb},
        Containers::arraySize(RgbData));

    addInstancedTests({&WebPImageConverterTest::rgba},
        Containers::arraySize(RgbaData));

    addTests({&WebPImageConverterTest::importFailed,
              &WebPImageConverterTest::encodingFailed});

    addInstancedTests({&WebPImageConverterTest::unsupportedMetadata},
        Containers::arraySize(UnsupportedMetadataData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef WEBPIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(WEBPIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* The WebPImporter is optional */
    #ifdef WEBPIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(WEBPIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void WebPImageConverterTest::invalidConfiguration() {
    auto&& data = InvalidConfigurationData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("WebPImageConverter");
    if(data.preset)
        converter->configuration().setValue("preset", data.preset);
    if(data.lossless)
        converter->configuration().setValue("lossless", *data.lossless);
    if(data.lossy)
        converter->configuration().setValue("lossy", *data.lossy);
    if(data.alphaQuality)
        converter->configuration().setValue("alphaQuality", *data.alphaQuality);

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, "hah"}));
    CORRADE_COMPARE(out, Utility::format("Trade::WebPImageConverter::convertToData(): {}\n", data.expectedError));
}

void WebPImageConverterTest::invalidFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("WebPImageConverter");

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RG8Unorm, {1, 1}, "hah"}));
    CORRADE_COMPARE(out, "Trade::WebPImageConverter::convertToData(): unsupported format PixelFormat::RG8Unorm\n");
}

constexpr const char OriginalRgbData[] = {
    /* Skip */
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    '\x00', '\x27', '\x48', '\x10', '\x34', '\x54',
    '\x22', '\x46', '\x60', '\x25', '\x49', '\x63',
    '\x21', '\x46', '\x63', '\x13', '\x3a', '\x59', 0, 0,

    '\x5b', '\x87', '\xae', '\x85', '\xaf', '\xd5',
    '\x94', '\xbd', '\xdd', '\x96', '\xbf', '\xdf',
    '\x91', '\xbc', '\xdf', '\x72', '\x9e', '\xc1', 0, 0,

    '\x3c', '\x71', '\xa7', '\x68', '\x9c', '\xce',
    '\x8b', '\xbb', '\xe9', '\x92', '\xc3', '\xee',
    '\x8b', '\xbe', '\xed', '\x73', '\xa7', '\xd6', 0, 0,

    '\x00', '\x34', '\x70', '\x12', '\x4a', '\x83',
    '\x35', '\x6a', '\x9e', '\x45', '\x7a', '\xac',
    '\x34', '\x6c', '\x9f', '\x1d', '\x56', '\x8b', 0, 0
};

const ImageView2D OriginalRgb{PixelStorage{}.setSkip({0, 1, 0}),
    PixelFormat::RGB8Unorm, {6, 4}, OriginalRgbData};

void WebPImageConverterTest::rgb() {
    auto&& data = RgbData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("WebPImageConverter");
    if(data.preset)
        converter->configuration().setValue("preset", data.preset);
    if(data.lossless)
        converter->configuration().setValue("lossless", *data.lossless);
    if(data.lossy)
        converter->configuration().setValue("lossy", *data.lossy);
    if(data.useArgb)
        converter->configuration().setValue("useArgb", *data.useArgb);
    CORRADE_COMPARE(converter->extension(), "webp");
    CORRADE_COMPARE(converter->mimeType(), "image/webp");

    Containers::Optional<Containers::Array<char>> output = converter->convertToData(OriginalRgb);
    CORRADE_VERIFY(output);

    if(_importerManager.loadState("WebPImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("WebPImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("WebPImporter");
    CORRADE_VERIFY(importer->openData(*output));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_WITH(*image, OriginalRgb,
        (DebugTools::CompareImage{data.maxThreshold, data.meanThreshold}));

    CORRADE_COMPARE_AS(output->size(),
        data.maxSize,
        TestSuite::Compare::LessOrEqual);
}

constexpr const char OriginalRgbaData[] = {
    /* Skip */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    '\x00', '\x27', '\x48', 0,      '\x10', '\x34', '\x54', '\x66',
    '\x22', '\x46', '\x60', '\x33', '\x25', '\x49', '\x63', '\xcc',
    '\x21', '\x46', '\x63', '\x99', '\x13', '\x3a', '\x59', '\xee',

    '\x5b', '\x87', '\xae', '\xff', '\x85', '\xaf', '\xd5', 0,
    '\x94', '\xbd', '\xdd', '\x11', '\x96', '\xbf', '\xdf', '\xaa',
    '\x91', '\xbc', '\xdf', '\x44', '\x72', '\x9e', '\xc1', '\xec',

    '\x3c', '\x71', '\xa7', '\xaa', '\x68', '\x9c', '\xce', '\x88',
    '\x8b', '\xbb', '\xe9', '\x77', '\x92', '\xc3', '\xee', '\xab',
    '\x8b', '\xbe', '\xed', '\x22', '\x73', '\xa7', '\xd6', '\x55',

    '\x00', '\x34', '\x70', '\x01', '\x12', '\x4a', '\x83', 0,
    '\x35', '\x6a', '\x9e', '\x78', '\x45', '\x7a', '\xac', '\xbb',
    '\x34', '\x6c', '\x9f', '\x9a', '\x1d', '\x56', '\x8b', '\xdd'
};

const ImageView2D OriginalRgba{PixelStorage{}.setSkip({0, 1, 0}),
    PixelFormat::RGBA8Unorm, {6, 4}, OriginalRgbaData};

void WebPImageConverterTest::rgba() {
    auto&& data = RgbaData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("WebPImageConverter");
    if(data.preset)
        converter->configuration().setValue("preset", data.preset);
    /* The lossless, lossy and useArgb options are tested well enough in rgb(),
       this verifies only what's specific to RGBA */
    if(data.exactTransparentRgb)
        converter->configuration().setValue("exactTransparentRgb", *data.exactTransparentRgb);
    if(data.alphaQuality)
        converter->configuration().setValue("alphaQuality", *data.alphaQuality);

    Containers::Optional<Containers::Array<char>> output = converter->convertToData(OriginalRgba);
    CORRADE_VERIFY(output);

    if(_importerManager.loadState("WebPImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("WebPImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("WebPImporter");
    CORRADE_VERIFY(importer->openData(*output));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_WITH(*image, OriginalRgba,
        (DebugTools::CompareImage{data.maxThreshold, data.meanThreshold}));

    CORRADE_COMPARE_AS(output->size(),
        data.maxSize,
        TestSuite::Compare::LessOrEqual);
}

void WebPImageConverterTest::importFailed() {
    /* https://github.com/webmproject/libwebp/commit/6c45cef7ff27d84330d2034b014716f75d76302e */
    if(WebPGetEncoderVersion() < 0x010203)
        CORRADE_SKIP("This failure is triggerable only on libwebp 1.2.3+");

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("WebPImageConverter");

    /* WebP requires the stride to be larger than width */
    ImageView2D image{
        PixelStorage{}.setRowLength(1),
        PixelFormat::RGB8Unorm, {2, 1}, "hello"};

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(image));
    CORRADE_COMPARE(out, "Trade::WebPImageConverter::convertToData(): importing an image failed\n");
}

void WebPImageConverterTest::encodingFailed()   {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("WebPImageConverter");

    /* WebP image width/height is limited to 16383 pixels */
    const char imageData[16384*3]{};
    ImageView2D image{PixelFormat::RGB8Unorm, {16384, 1}, imageData};

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(image));
    CORRADE_COMPARE(out, "Trade::WebPImageConverter::convertToData(): encoding an image failed: invalid picture size\n");
}

void WebPImageConverterTest::unsupportedMetadata() {
    auto&& data = UnsupportedMetadataData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("WebPImageConverter");
    converter->addFlags(data.converterFlags);

    Containers::String out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, "hey", data.imageFlags}));
    if(!data.message)
        CORRADE_COMPARE(out, "");
    else
        CORRADE_COMPARE(out, Utility::format("Trade::WebPImageConverter::convertToData(): {}\n", data.message));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::WebPImageConverterTest)
