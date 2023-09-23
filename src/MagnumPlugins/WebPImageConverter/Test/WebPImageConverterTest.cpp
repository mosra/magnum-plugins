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
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct WebPImageConverterTest: TestSuite::Tester {
    explicit WebPImageConverterTest();

    void bitDepthAndFormat();
    void sizeError();

    void rgb();
    void rgba();

    void unsupportedMetadata();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

const struct {
    const char* name;
    ImageConverterFlags flags;
    bool quiet;
} QuietData[]{
    {"", {}, false},
    {"quiet", ImageConverterFlag::Quiet, true}
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
    addTests({&WebPImageConverterTest::bitDepthAndFormat});

    addInstancedTests({&WebPImageConverterTest::sizeError},
        Containers::arraySize(QuietData));

    addTests({&WebPImageConverterTest::rgb,
              &WebPImageConverterTest::rgba});

    addInstancedTests({&WebPImageConverterTest::unsupportedMetadata},
        Containers::arraySize(UnsupportedMetadataData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef WEBPIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(WEBPIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* The WebPImporter is optional */
    #ifdef WEBPIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(WEBPIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void WebPImageConverterTest::bitDepthAndFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("WebPImageConverter");

    const char data[16]{};

    for(UnsignedInt i = 1; i <= static_cast<UnsignedInt>(PixelFormat::RGBA32F); i++) {
        auto image = ImageView2D{PixelFormat(i), {1, 1}, data};

        if(image.format() == PixelFormat::RGB8Unorm || image.format() == PixelFormat::RGBA8Unorm) {
            Containers::Optional<Containers::Array<char>> converted_data = converter->convertToData(image);

            if (_importerManager.loadState("WebPImporter") == PluginManager::LoadState::NotFound)
                CORRADE_SKIP("WebPImporter plugin not found, cannot test");

            Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("WebPImporter");
            CORRADE_VERIFY(importer->openData(*converted_data));
            Containers::Optional<Trade::ImageData2D> converted_img = importer->image2D(0);
            CORRADE_VERIFY(converted_img);
            CORRADE_COMPARE(converted_img->size(), Vector2i(1, 1));

            /* WebP converter returns only 8-bit depth-RGB or 8-bit depth-RGBA data depending on the channel count of the original pixel format. */
            CORRADE_COMPARE(converted_img->format(),
                            pixelFormatChannelCount(converted_img->format()) == 4 ? PixelFormat::RGBA8Unorm : PixelFormat::RGB8Unorm);
        } else {
            std::ostringstream out;
            Warning redirectWarning{&out};
            Error redirectError{&out};

            CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat(i), {1, 1}, data}));
            CORRADE_COMPARE(out.str(), "Trade::WebPImageConverter::convertToData(): unsupported pixel format\n");
            out.flush();
        }
    }
}

void WebPImageConverterTest::sizeError() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("WebPImageConverter");
    converter->addFlags(data.flags);

    /* Since WEBP image width/height is limited to 16383 pixels, we do the test with a bigger image. */
    const char imageData[49152]{};
    std::ostringstream out;
    Warning redirectWarning{&out};
    Error redirectError{&out};

    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGB8Unorm, {16384, 1}, imageData}));
    CORRADE_COMPARE(out.str(), "Encoding error: invalid picture size\n");
    out.flush();
}

constexpr const char RgbData[24] {
    100, 90, 8, 100, 90, 8, 0, 0,
    100, 90, 8, 100, 75, 91, 0, 0,
    100, 75, 91, 100, 75, 91, 0, 0
};

const ImageView2D OriginalRgb{PixelFormat::RGB8Unorm, {2, 3}, RgbData};

constexpr const char ConvertedRgbData[] = {
    100, 90, 8, 100, 90, 8,  0, 0,
    100, 90, 8, 100, 75, 91, 0, 0,
    100, 75, 91, 100, 75, 91, 0, 0
};

void WebPImageConverterTest::rgb() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("WebPImageConverter");
    CORRADE_COMPARE(converter->extension(), "webp");
    CORRADE_COMPARE(converter->mimeType(), "image/webp");
    CORRADE_VERIFY(converter->convertToData(OriginalRgb));

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(OriginalRgb);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("WebPImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("WebPImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("WebPImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic values */
    CORRADE_COMPARE(converted->mutableData().size(), 24);
    converted->mutableData()[6] = converted->mutableData()[7] =
        converted->mutableData()[14] = converted->mutableData()[15] =
            converted->mutableData()[22] = converted->mutableData()[23] = 0;

    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(ConvertedRgbData),
        TestSuite::Compare::Container);
}

constexpr const char OriginalRgbaData[24] = {
        10, 20, 30, 80, 10, 20, 30, 80,
        40, 50, 60, 90, 40, 50, 60, 90,
        70, 80, 90, 100,70, 80, 90, 100,
};

const ImageView2D OriginalRgba{PixelFormat::RGBA8Unorm, {2, 3}, OriginalRgbaData};

constexpr const char ConvertedRgbaData[] = {
        10, 20, 30, 80, 10, 20, 30, 80,
        40, 50, 60, 90, 40, 50, 60, 90,
        70, 80, 90, 100,70, 80, 90, 100,
};

void WebPImageConverterTest::rgba() {
    Containers::Optional<Containers::Array<char>> data = _converterManager.instantiate("WebPImageConverter")->convertToData(OriginalRgba);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("WebPImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("WebPImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("WebPImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->mutableData().size(), 24);
    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(ConvertedRgbaData),
                       TestSuite::Compare::Container);
}

void WebPImageConverterTest::unsupportedMetadata() {
    auto&& data = UnsupportedMetadataData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("WebPImageConverter");
    converter->addFlags(data.converterFlags);

    const char imageData[4]{};
    ImageView2D image{PixelFormat::RGBA8Unorm, {1, 1}, imageData, data.imageFlags};

    std::ostringstream out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(converter->convertToData(image));
    if(!data.message)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::WebPImageConverter::convertToData(): {}\n", data.message));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::WebPImageConverterTest)
