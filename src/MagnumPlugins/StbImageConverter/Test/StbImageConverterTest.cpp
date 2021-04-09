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
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct StbImageConverterTest: TestSuite::Tester {
    explicit StbImageConverterTest();

    void wrongFormat();
    void wrongFormatHdr();
    void wrongOutputFormat();

    /** @todo test the enum constructor somehow (needs to be not loaded through plugin manager) */

    void bmpRg();
    void bmpNegativeSize();

    void hdrGrayscale();
    void hdrRg();
    void hdrRgb();
    void hdrRgba();
    void hdrZeroSize();

    void jpegRgb80Percent();
    void jpegRgb100Percent();
    void jpegRgba80Percent();
    void jpegGrayscale80Percent();
    /* Can't grayscale 100% because stb_image_write expands to RGB */
    void jpegZeroSize();

    void pngRgb();
    void pngGrayscale();
    void pngNegativeSize();

    void tgaRgba();
    void tgaNegativeSize();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

StbImageConverterTest::StbImageConverterTest() {
    addTests({&StbImageConverterTest::wrongFormat,
              &StbImageConverterTest::wrongFormatHdr,
              &StbImageConverterTest::wrongOutputFormat,

              &StbImageConverterTest::bmpRg,
              &StbImageConverterTest::bmpNegativeSize,

              &StbImageConverterTest::hdrGrayscale,
              &StbImageConverterTest::hdrRg,
              &StbImageConverterTest::hdrRgb,
              &StbImageConverterTest::hdrRgba,
              &StbImageConverterTest::hdrZeroSize,

              &StbImageConverterTest::jpegRgb80Percent,
              &StbImageConverterTest::jpegRgb100Percent,
              &StbImageConverterTest::jpegRgba80Percent,
              &StbImageConverterTest::jpegGrayscale80Percent,
              &StbImageConverterTest::jpegZeroSize,

              &StbImageConverterTest::pngRgb,
              &StbImageConverterTest::pngGrayscale,
              &StbImageConverterTest::pngNegativeSize,

              &StbImageConverterTest::tgaRgba,
              &StbImageConverterTest::tgaNegativeSize});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STBIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(STBIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* The PngImporter is optional */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void StbImageConverterTest::wrongFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbTgaImageConverter");
    ImageView2D image{PixelFormat::RGBA32F, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!converter->convertToData(image));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::convertToData(): PixelFormat::RGBA32F is not supported for BMP/JPEG/PNG/TGA output\n");
}

void StbImageConverterTest::wrongFormatHdr() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbHdrImageConverter");
    ImageView2D image{PixelFormat::RGB8Unorm, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!converter->convertToData(image));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::convertToData(): PixelFormat::RGB8Unorm is not supported for HDR output\n");
}

void StbImageConverterTest::wrongOutputFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbImageConverter");
    ImageView2D image{PixelFormat::RGB8Unorm, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!converter->convertToData(image));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::convertToData(): cannot determine output format (plugin loaded as StbImageConverter)\n");
}

constexpr const char OriginalRgData[] = {
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 1,  2,  3,  4,
    0, 0, 0, 0, 5,  6,  7,  8,
    0, 0, 0, 0, 9, 10, 11, 12
};

const ImageView2D OriginalRg{PixelStorage{}.setSkip({2, 1, 0}).setRowLength(4),
    PixelFormat::RG8Unorm, {2, 3}, OriginalRgData};

/* Blue channel gets lost */
constexpr const char ConvertedRgData[] = {
    1, 1, 1, 3, 3, 3,
    5, 5, 5, 7, 7, 7,
    9, 9, 9, 11, 11, 11
};

void StbImageConverterTest::bmpRg() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbBmpImageConverter");

    /* RGBA should be exported as RGB, with the alpha channel ignored (and a
       warning about that printed) */
    std::ostringstream out;
    Containers::Array<char> data;
    {
        Warning redirectWarning{&out};
        data = converter->convertToData(OriginalRg);
    }
    CORRADE_VERIFY(data);
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::convertToData(): ignoring green channel for BMP/JPEG output\n");

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    /* RG gets expanded to RRR */
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(converted->data(),
        Containers::arrayView(ConvertedRgData), TestSuite::Compare::Container);
}

void StbImageConverterTest::bmpNegativeSize() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbBmpImageConverter");

    /* Doesn't fail for zero size */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGB8Unorm, {-1, 0}, nullptr}));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::convertToData(): error while writing the BMP file\n");
}

constexpr const Float OriginalGrayscale32FData[] = {
    1.0f, 2.0f,
    3.0f, 4.0f,
    5.0f, 6.0f
};

const ImageView2D OriginalGrayscale32F{PixelFormat::R32F, {2, 3}, OriginalGrayscale32FData};

constexpr const Float OriginalRg32FData[] = {
    1.0f, 1.5f, 2.0f, 2.5f,
    3.0f, 3.5f, 4.0f, 4.5f,
    5.0f, 5.5f, 6.0f, 6.5f
};

const ImageView2D OriginalRg32F{PixelFormat::RG32F, {2, 3}, OriginalRg32FData};

constexpr const Float ConvertedGrayscale32FData[] = {
    1.0f, 1.0f, 1.0f, 2.0f, 2.0f, 2.0f,
    3.0f, 3.0f, 3.0f, 4.0f, 4.0f, 4.0f,
    5.0f, 5.0f, 5.0f, 6.0f, 6.0f, 6.0f
};

void StbImageConverterTest::hdrGrayscale() {
    Containers::Pointer<Trade::AbstractImageConverter> converter = _converterManager.instantiate("StbHdrImageConverter");
    const auto data = converter->convertToData(OriginalGrayscale32F);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    /* R gets converted to RRR */
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB32F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(converted->data()),
        Containers::arrayView(ConvertedGrayscale32FData),
        TestSuite::Compare::Container);
}

void StbImageConverterTest::hdrRg() {
    Containers::Pointer<Trade::AbstractImageConverter> converter = _converterManager.instantiate("StbHdrImageConverter");

    /* RG should be exported as RRR, with the green channel ignored */
    std::ostringstream out;
    Containers::Array<char> data;
    {
        Warning redirectWarning{&out};
        data = converter->convertToData(OriginalRg32F);
    }
    CORRADE_VERIFY(data);
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::convertToData(): ignoring green channel for HDR output\n");

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB32F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(converted->data()),
        Containers::arrayView(ConvertedGrayscale32FData),
        TestSuite::Compare::Container);
}

constexpr const Float OriginalRgb32FData[] = {
    1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f,
    4.0f, 4.5f, 5.0f, 5.5f, 6.0f, 6.5f,
    7.0f, 7.5f, 8.0f, 8.5f, 9.0f, 9.5f
};

const ImageView2D OriginalRgb32F{PixelFormat::RGB32F, {2, 3}, OriginalRgb32FData};

constexpr const Float OriginalRgba32FData[] = {
    1.0f, 1.5f, 2.0f, 0.0f, 2.5f, 3.0f, 3.5f, 0.0f,
    4.0f, 4.5f, 5.0f, 0.0f, 5.5f, 6.0f, 6.5f, 0.0f,
    7.0f, 7.5f, 8.0f, 0.0f, 8.5f, 9.0f, 9.5f, 0.0f
};

const ImageView2D OriginalRgba32F{PixelFormat::RGBA32F, {2, 3}, OriginalRgba32FData};

void StbImageConverterTest::hdrRgb() {
    Containers::Pointer<Trade::AbstractImageConverter> converter = _converterManager.instantiate("StbHdrImageConverter");
    const auto data = converter->convertToData(OriginalRgb32F);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB32F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(converted->data()),
        Containers::arrayView(OriginalRgb32FData),
        TestSuite::Compare::Container);
}

void StbImageConverterTest::hdrRgba() {
   Containers::Pointer<Trade::AbstractImageConverter> converter = _converterManager.instantiate("StbHdrImageConverter");

    /* RGBA should be exported as RGB, with the alpha channel ignored */
    std::ostringstream out;
    Containers::Array<char> data;
    {
        Warning redirectWarning{&out};
        data = converter->convertToData(OriginalRgba32F);
    }
    CORRADE_VERIFY(data);
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::convertToData(): ignoring alpha channel for HDR output\n");

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB32F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(converted->data()),
        Containers::arrayView(OriginalRgb32FData),
        TestSuite::Compare::Container);
}

void StbImageConverterTest::hdrZeroSize() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbHdrImageConverter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGB32F, {}, nullptr}));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::convertToData(): error while writing the HDR file\n");
}

constexpr const char OriginalJpegRgbData[] = {
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

const ImageView2D OriginalJpegRgb{PixelStorage{}.setSkip({0, 1, 0}),
    PixelFormat::RGB8Unorm, {6, 4}, OriginalJpegRgbData};

constexpr const char OriginalJpegRgbaData[] = {
    /* Skip */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    '\x00', '\x27', '\x48', 0, '\x10', '\x34', '\x54', 0,
    '\x22', '\x46', '\x60', 0, '\x25', '\x49', '\x63', 0,
    '\x21', '\x46', '\x63', 0, '\x13', '\x3a', '\x59', 0,

    '\x5b', '\x87', '\xae', 0, '\x85', '\xaf', '\xd5', 0,
    '\x94', '\xbd', '\xdd', 0, '\x96', '\xbf', '\xdf', 0,
    '\x91', '\xbc', '\xdf', 0, '\x72', '\x9e', '\xc1', 0,

    '\x3c', '\x71', '\xa7', 0, '\x68', '\x9c', '\xce', 0,
    '\x8b', '\xbb', '\xe9', 0, '\x92', '\xc3', '\xee', 0,
    '\x8b', '\xbe', '\xed', 0, '\x73', '\xa7', '\xd6', 0,

    '\x00', '\x34', '\x70', 0, '\x12', '\x4a', '\x83', 0,
    '\x35', '\x6a', '\x9e', 0, '\x45', '\x7a', '\xac', 0,
    '\x34', '\x6c', '\x9f', 0, '\x1d', '\x56', '\x8b', 0
};

const ImageView2D OriginalJpegRgba{PixelStorage{}.setSkip({0, 1, 0}),
    PixelFormat::RGBA8Unorm, {6, 4}, OriginalJpegRgbaData};

/* Slightly different due to compression artifacts. See the 100% test for
   a threshold verification. Needs to have a bigger size otherwise the
   compression makes a total mess. */
constexpr const char ConvertedJpegRgbData[] = {
    '\x03', '\x26', '\x4c', '\x12', '\x36', '\x5a',
    '\x22', '\x46', '\x68', '\x25', '\x49', '\x69',
    '\x1c', '\x40', '\x60', '\x11', '\x35', '\x57',

    '\x5f', '\x8a', '\xb5', '\x76', '\xa1', '\xcb',
    '\x91', '\xbd', '\xe4', '\x99', '\xc5', '\xea',
    '\x8e', '\xba', '\xdf', '\x7d', '\xa9', '\xd0',

    '\x48', '\x7a', '\xad', '\x66', '\x98', '\xc9',
    '\x88', '\xbb', '\xe8', '\x94', '\xc7', '\xf2',
    '\x88', '\xbb', '\xe6', '\x77', '\xaa', '\xd7',

    '\x00', '\x2f', '\x65', '\x17', '\x4f', '\x82',
    '\x38', '\x70', '\xa1', '\x41', '\x79', '\xa8',
    '\x32', '\x6a', '\x99', '\x21', '\x59', '\x8a'
};

void StbImageConverterTest::jpegRgb80Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbJpegImageConverter");
    CORRADE_COMPARE(converter->configuration().value<Float>("jpegQuality"), 0.8f);

    const auto data = converter->convertToData(OriginalJpegRgb);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE(converted->size(), Vector2i(6, 4));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(ConvertedJpegRgbData),
        TestSuite::Compare::Container);
}

void StbImageConverterTest::jpegRgb100Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbJpegImageConverter");
    converter->configuration().setValue("jpegQuality", 1.0f);

    const auto data = converter->convertToData(OriginalJpegRgb);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    /* Expect only minimal difference (single bits) */
    CORRADE_COMPARE_WITH(*converted, OriginalJpegRgb,
        (DebugTools::CompareImage{1.0f, 0.39f}));
}

void StbImageConverterTest::jpegRgba80Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbJpegImageConverter");
    CORRADE_COMPARE(converter->configuration().value<Float>("jpegQuality"), 0.8f);

    /* RGBA should be exported as RGB, with the alpha channel ignored (and a
       warning about that printed) */
    std::ostringstream out;
    Containers::Array<char> data;
    {
        Warning redirectWarning{&out};
        data = converter->convertToData(OriginalJpegRgba);
    }
    CORRADE_VERIFY(data);
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::convertToData(): ignoring alpha channel for BMP/JPEG output\n");

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE(converted->size(), Vector2i(6, 4));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(ConvertedJpegRgbData),
        TestSuite::Compare::Container);

    /* Finally, the output should be exactly the same as when exporting RGB,
       bit to bit, to ensure we don't produce anything that would cause
       problems for traditional non-turbo libjpeg */
    const auto dataRgb = converter->convertToData(OriginalJpegRgb);
    CORRADE_COMPARE_AS(data, dataRgb, TestSuite::Compare::Container);
}

constexpr const char OriginalJpegGrayscaleData[] = {
    0, 0, 0, 0, 0, 0, 0, 0, /* Skip */

    '\x00', '\x10', '\x22', '\x25', '\x21', '\x13', 0, 0,
    '\x5b', '\x85', '\x94', '\x96', '\x91', '\x72', 0, 0,
    '\x3c', '\x68', '\x8b', '\x92', '\x8b', '\x73', 0, 0,
    '\x00', '\x12', '\x35', '\x45', '\x34', '\x1d', 0, 0
};

/* Slightly different due to compression artifacts. See the 100% test for
   a threshold verification. Needs to have a bigger size otherwise the
   compression makes a total mess. Also, stb_image_write expands to RGB,
   so the data are inflated. */
constexpr const char ConvertedJpegGrayscaleData[] = {
    '\x01', '\x01', '\x01', '\x11', '\x11', '\x11',
    '\x23', '\x23', '\x23', '\x27', '\x27', '\x27',
    '\x1c', '\x1c', '\x1c', '\x11', '\x11', '\x11',

    '\x65', '\x65', '\x65', '\x7d', '\x7d', '\x7d',
    '\x97', '\x97', '\x97', '\x9d', '\x9d', '\x9d',
    '\x8e', '\x8e', '\x8e', '\x7a', '\x7a', '\x7a',

    '\x3f', '\x3f', '\x3f', '\x60', '\x60', '\x60',
    '\x85', '\x85', '\x85', '\x93', '\x93', '\x93',
    '\x88', '\x88', '\x88', '\x78', '\x78', '\x78',

    '\x00', '\x00', '\x00', '\x19', '\x19', '\x19',
    '\x3b', '\x3b', '\x3b', '\x43', '\x43', '\x43',
    '\x32', '\x32', '\x32', '\x1e', '\x1e', '\x1e'
};

const ImageView2D OriginalJpegGrayscale{PixelStorage{}.setSkip({0, 1, 0}),
    PixelFormat::R8Unorm, {6, 4}, OriginalJpegGrayscaleData};

void StbImageConverterTest::jpegGrayscale80Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbJpegImageConverter");
    CORRADE_COMPARE(converter->configuration().value<Float>("jpegQuality"), 0.8f);

    const auto data = converter->convertToData(OriginalJpegGrayscale);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE(converted->size(), Vector2i(6, 4));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(ConvertedJpegGrayscaleData),
        TestSuite::Compare::Container);
}

void StbImageConverterTest::jpegZeroSize() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbJpegImageConverter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGB8Unorm, {}, nullptr}));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::convertToData(): error while writing the JPEG file\n");
}

constexpr const char OriginalRgbData[] = {
    0, 0, 0, 0, 0, 0, 0, 0, /* Skip */

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

void StbImageConverterTest::pngRgb() {
    const auto data = _converterManager.instantiate("StbPngImageConverter")->convertToData(OriginalRgb);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(converted->data(),
        Containers::arrayView(ConvertedRgbData),
        TestSuite::Compare::Container);
}

constexpr const char OriginalGrayscaleData[] = {
    /* Skip */
    0, 0, 0, 0,

    1, 2, 0, 0,
    3, 4, 0, 0,
    5, 6, 0, 0
};

const ImageView2D OriginalGrayscale{PixelStorage{}.setSkip({0, 1, 0}),
    PixelFormat::R8Unorm, {2, 3}, OriginalGrayscaleData};

constexpr const char ConvertedGrayscaleData[] = {
    1, 2,
    3, 4,
    5, 6
};

void StbImageConverterTest::pngGrayscale() {
    const auto data = _converterManager.instantiate("StbPngImageConverter")->convertToData(OriginalGrayscale);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::R8Unorm);
    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(ConvertedGrayscaleData),
        TestSuite::Compare::Container);
}

void StbImageConverterTest::pngNegativeSize() {
    /* Doesn't fail for zero size. Hard to trigger an error any other way than
       having a negative size, which results in some petabytes being allocated
       and thus failing. This, however, triggers sanitizers, so we have to
       disable this test there. Also, I suspect this test will start causing
       serious problems once OSes are actually able to allocate petabytes :D */
    #if defined(__has_feature)
    #if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
    CORRADE_SKIP("This tests the failure by attempting to allocate 0xfffffffffffffffd bytes. That makes AddressSanitizer and ThreadSanitizer upset, so not doing that with it.");
    #endif
    #endif

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbPngImageConverter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGB8Unorm, {-1, 0}, nullptr}));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::convertToData(): error while writing the PNG file\n");
}

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

void StbImageConverterTest::tgaRgba() {
    const auto data = _converterManager.instantiate("StbTgaImageConverter")->convertToData(OriginalRgba);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("StbImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("StbImageImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(ConvertedRgbaData),
        TestSuite::Compare::Container);
}

void StbImageConverterTest::tgaNegativeSize() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbTgaImageConverter");

    /* Doesn't fail for zero size */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGB8Unorm, {-1, 0}, nullptr}));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::convertToData(): error while writing the TGA file\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StbImageConverterTest)
