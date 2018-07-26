/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
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
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

struct StbImageConverterTest: TestSuite::Tester {
    explicit StbImageConverterTest();

    void wrongFormat();
    void wrongFormatHdr();

    /** @todo test the enum constructor somehow (needs to be not loaded through plugin manager) */

    void rgBmp();
    void grayscaleHdr();
    void rgbPng();
    void rgbaTga();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

StbImageConverterTest::StbImageConverterTest() {
    addTests({&StbImageConverterTest::wrongFormat,
              &StbImageConverterTest::wrongFormatHdr,

              &StbImageConverterTest::rgBmp,
              &StbImageConverterTest::grayscaleHdr,
              &StbImageConverterTest::rgbPng,
              &StbImageConverterTest::rgbaTga});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STBIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_converterManager.load(STBIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* The PngImporter is optional */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_importerManager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void StbImageConverterTest::wrongFormat() {
    std::unique_ptr<AbstractImageConverter> converter = _converterManager.instantiate("StbTgaImageConverter");
    ImageView2D image{PixelFormat::RGBA32F, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!converter->exportToData(image));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::exportToData(): PixelFormat::RGBA32F is not supported for BMP/PNG/TGA output\n");
}

void StbImageConverterTest::wrongFormatHdr() {
    std::unique_ptr<AbstractImageConverter> converter = _converterManager.instantiate("StbHdrImageConverter");
    ImageView2D image{PixelFormat::RGB8Unorm, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!converter->exportToData(image));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::exportToData(): PixelFormat::RGB8Unorm is not supported for HDR output\n");
}

namespace {
    constexpr const char OriginalRgData[] = {
        0, 0, 0, 0, 0, 0, 0, 0,

        0, 0, 0, 0, 1,  2,  3,  4,
        0, 0, 0, 0, 5,  6,  7,  8,
        0, 0, 0, 0, 9, 10, 11, 12
    };

    const ImageView2D OriginalRg{PixelStorage{}.setSkip({2, 1, 0}).setRowLength(4),
        PixelFormat::RG8Unorm, {2, 3}, OriginalRgData};

    constexpr const char ConvertedRgData[] = {
        1, 1, 1, 3, 3, 3,
        5, 5, 5, 7, 7, 7,
        9, 9, 9, 11, 11, 11
    };
}

void StbImageConverterTest::rgBmp() {
    const auto data = _converterManager.instantiate("StbBmpImageConverter")->exportToData(OriginalRg);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    std::unique_ptr<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    /* RG gets expanded to RRR */
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(converted->data(),
        Containers::arrayView(ConvertedRgData), TestSuite::Compare::Container);
}

namespace {
    constexpr const Float OriginalGrayscaleData[] = {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f
    };

    const ImageView2D OriginalGrayscale{PixelFormat::R32F, {2, 3}, OriginalGrayscaleData};

    constexpr const Float ConvertedGrayscaleData[] = {
        1.0f, 1.0f, 1.0f, 2.0f, 2.0f, 2.0f,
        3.0f, 3.0f, 3.0f, 4.0f, 4.0f, 4.0f,
        5.0f, 5.0f, 5.0f, 6.0f, 6.0f, 6.0f
    };
}

void StbImageConverterTest::grayscaleHdr() {
    const auto data = _converterManager.instantiate("StbHdrImageConverter")->exportToData(OriginalGrayscale);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    std::unique_ptr<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    /* R gets converted to RRR */
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB32F);
    CORRADE_COMPARE_AS(Containers::arrayCast<Float>(converted->data()),
        Containers::arrayView(ConvertedGrayscaleData),
        TestSuite::Compare::Container);
}

namespace {
    constexpr const char OriginalRgbData[] = {
        /* Skip */
        0, 0, 0, 0, 0, 0, 0, 0,

        1, 2, 3, 2, 3, 4, 0, 0,
        3, 4, 5, 4, 5, 6, 0, 0,
        5, 6, 7, 6, 7, 8, 0, 0
    };

    const ImageView2D OriginalRgb{PixelStorage{}.setSkip({0, 1, 0}),
        PixelFormat::RGB8Unorm, {2, 3}, OriginalRgbData};

    constexpr const char ConvertedRgbData[] = {
        1, 2, 3, 2, 3, 4,
        3, 4, 5, 4, 5, 6,
        5, 6, 7, 6, 7, 8
    };
}

void StbImageConverterTest::rgbPng() {
    const auto data = _converterManager.instantiate("StbPngImageConverter")->exportToData(OriginalRgb);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    std::unique_ptr<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(converted->data(),
        Containers::arrayView(ConvertedRgbData),
        TestSuite::Compare::Container);
}

namespace {
    constexpr const char OriginalRgbaData[] = {
        0, 0, 0, 0, 0, 0, 0, 0, /* Skip */

        1, 2, 3, 4, 2, 3, 4, 5,
        3, 4, 5, 6, 4, 5, 6, 7,
        5, 6, 7, 8, 6, 7, 8, 9
    };

    const ImageView2D OriginalRgba{PixelStorage{}.setSkip({0, 1, 0}),
        PixelFormat::RGBA8Unorm, {2, 3}, OriginalRgbaData};

    constexpr const char ConvertedRgbaData[] = {
        1, 2, 3, 4, 2, 3, 4, 5,
        3, 4, 5, 6, 4, 5, 6, 7,
        5, 6, 7, 8, 6, 7, 8, 9
    };
}

void StbImageConverterTest::rgbaTga() {
    const auto data = _converterManager.instantiate("StbTgaImageConverter")->exportToData(OriginalRgba);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    std::unique_ptr<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(ConvertedRgbaData),
        TestSuite::Compare::Container);
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StbImageConverterTest)
