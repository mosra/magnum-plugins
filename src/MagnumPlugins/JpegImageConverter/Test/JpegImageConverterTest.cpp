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

/* jpeglib.h is needed to query if RGBA output is supported (JCS_EXTENSIONS).
   See JpegImageConverter.cpp for details why the define below is needed. */
#ifdef CORRADE_TARGET_WINDOWS
#define XMD_H
#endif
#include <jpeglib.h>

namespace Magnum { namespace Trade { namespace Test { namespace {

struct JpegImageConverterTest: TestSuite::Tester {
    explicit JpegImageConverterTest();

    void wrongFormat();
    void zeroSize();

    void rgb80Percent();
    void rgb100Percent();

    void rgba80Percent();

    void grayscale80Percent();
    void grayscale100Percent();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

JpegImageConverterTest::JpegImageConverterTest() {
    addTests({&JpegImageConverterTest::wrongFormat,
              &JpegImageConverterTest::zeroSize,

              &JpegImageConverterTest::rgb80Percent,
              &JpegImageConverterTest::rgb100Percent,

              &JpegImageConverterTest::rgba80Percent,

              &JpegImageConverterTest::grayscale80Percent,
              &JpegImageConverterTest::grayscale100Percent});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef JPEGIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(JPEGIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* The JpegImporter is optional */
    #ifdef JPEGIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(JPEGIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void JpegImageConverterTest::wrongFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");
    ImageView2D image{PixelFormat::R16F, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!converter->exportToData(image));
    CORRADE_COMPARE(out.str(), "Trade::JpegImageConverter::exportToData(): unsupported pixel format PixelFormat::R16F\n");
}

void JpegImageConverterTest::zeroSize() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->exportToData(ImageView2D{PixelFormat::RGB8Unorm, {}, nullptr}));
    CORRADE_COMPARE(out.str(), "Trade::JpegImageConverter::exportToData(): error: Empty JPEG image (DNL not supported)\n");
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

constexpr const char OriginalRgbaData[] = {
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

const ImageView2D OriginalRgba{PixelStorage{}.setSkip({0, 1, 0}),
    PixelFormat::RGBA8Unorm, {6, 4}, OriginalRgbaData};

/* Slightly different due to compression artifacts. See the 100% test for
   a threshold verification. Needs to have a bigger size otherwise the
   compression makes a total mess. */
constexpr const char ConvertedRgbData[] = {
    #ifndef CORRADE_TARGET_APPLE
    '\x00', '\x29', '\x50', '\x0c', '\x38', '\x5f',
    '\x1c', '\x48', '\x6f', '\x1f', '\x4b', '\x72',
    '\x15', '\x42', '\x69', '\x0a', '\x37', '\x5e', 0, 0,

    '\x5f', '\x8a', '\xb4', '\x76', '\xa1', '\xcb',
    '\x91', '\xbc', '\xe6', '\x98', '\xc5', '\xee',
    '\x8d', '\xba', '\xe3', '\x7c', '\xa9', '\xd2', 0, 0,

    '\x4d', '\x79', '\xa6', '\x6b', '\x97', '\xc4',
    '\x8d', '\xb9', '\xe6', '\x97', '\xc6', '\xf2',
    '\x8b', '\xba', '\xe6', '\x7a', '\xa9', '\xd5', 0, 0,

    '\x01', '\x2d', '\x5c', '\x20', '\x4c', '\x7b',
    '\x3f', '\x6e', '\x9c', '\x48', '\x77', '\xa5',
    '\x39', '\x68', '\x96', '\x28', '\x57', '\x85', 0, 0
    #else
    /* libJPEG on macOS is special. This will all go away once I can use
       DebugTools::CompareImage. */
    '\x01', '\x27', '\x4e', '\x10', '\x36', '\x5d',
    '\x20', '\x46', '\x6d', '\x23', '\x49', '\x70',
    '\x19', '\x40', '\x67', '\x0e', '\x35', '\x5c', 0, 0,

    '\x5f', '\x8a', '\xb5', '\x76', '\xa1', '\xcc',
    '\x91', '\xbc', '\xe7', '\x99', '\xc4', '\xef',
    '\x8e', '\xb9', '\xe4', '\x7c', '\xa9', '\xd3', 0, 0,

    '\x4b', '\x79', '\xaa', '\x68', '\x98', '\xc8',
    '\x8a', '\xba', '\xea', '\x96', '\xc6', '\xf6',
    '\x8a', '\xba', '\xea', '\x79', '\xa9', '\xd9', 0, 0,

    '\x00', '\x2e', '\x61', '\x1b', '\x4d', '\x80',
    '\x3c', '\x6e', '\xa1', '\x45', '\x77', '\xaa',
    '\x36', '\x68', '\x9b', '\x24', '\x58', '\x8a', 0, 0
    #endif
};

void JpegImageConverterTest::rgb80Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");
    CORRADE_COMPARE(converter->configuration().value<Float>("jpegQuality"), 0.8f);

    const auto data = converter->exportToData(OriginalRgb);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("JpegImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("JpegImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("JpegImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE(converted->size(), Vector2i(6, 4));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(converted->mutableData().size(), 80);
    converted->mutableData()[18] = converted->mutableData()[19] =
        converted->mutableData()[38] = converted->mutableData()[39] =
            converted->mutableData()[58] = converted->mutableData()[59] =
                converted->mutableData()[78] = converted->mutableData()[79] = 0;

    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(ConvertedRgbData),
        TestSuite::Compare::Container);
}

void JpegImageConverterTest::rgb100Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");
    converter->configuration().setValue("jpegQuality", 1.0f);

    const auto data = converter->exportToData(OriginalRgb);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("JpegImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("JpegImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("JpegImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    /* Expect only minimal difference (single bits) */
    CORRADE_COMPARE_WITH(*converted, OriginalRgb,
        (DebugTools::CompareImage{3.1f, 1.4f}));
}

void JpegImageConverterTest::rgba80Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");
    CORRADE_COMPARE(converter->configuration().value<Float>("jpegQuality"), 0.8f);

    /* If we don't have libjpeg-turbo, exporting RGBA will fail */
    #ifndef JCS_EXTENSIONS
    {
        std::ostringstream out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->exportToData(OriginalRgba));
        CORRADE_COMPARE(out.str(), "Trade::JpegImageConverter::exportToData(): RGBA input (with alpha ignored) requires libjpeg-turbo\n");
    }

    CORRADE_SKIP("libjpeg-turbo is required for RGBA support.");
    #endif

    /* RGBA should be exported as RGB, with the alpha channel ignored (and a
       warning about that printed) */
    std::ostringstream out;
    Containers::Array<char> data;
    {
        Warning redirectWarning{&out};
        data = converter->exportToData(OriginalRgba);
    }
    CORRADE_VERIFY(data);
    CORRADE_COMPARE(out.str(), "Trade::JpegImageConverter::exportToData(): ignoring alpha channel\n");

    if(_importerManager.loadState("JpegImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("JpegImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("JpegImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE(converted->size(), Vector2i(6, 4));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(converted->data().size(), 80);
    converted->mutableData()[18] = converted->mutableData()[19] =
        converted->mutableData()[38] = converted->mutableData()[39] =
            converted->mutableData()[58] = converted->mutableData()[59] =
                converted->mutableData()[78] = converted->mutableData()[79] = 0;

    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(ConvertedRgbData),
        TestSuite::Compare::Container);

    /* Finally, the output should be exactly the same as when exporting RGB,
       bit to bit, to ensure we don't produce anything that would cause
       problems for traditional non-turbo libjpeg */
    const auto dataRgb = converter->exportToData(OriginalRgb);
    CORRADE_COMPARE_AS(data, dataRgb, TestSuite::Compare::Container);
}

constexpr const char OriginalGrayscaleData[] = {
    0, 0, 0, 0, 0, 0, 0, 0, /* Skip */

    '\x00', '\x10', '\x22', '\x25', '\x21', '\x13', 0, 0,
    '\x5b', '\x85', '\x94', '\x96', '\x91', '\x72', 0, 0,
    '\x3c', '\x68', '\x8b', '\x92', '\x8b', '\x73', 0, 0,
    '\x00', '\x12', '\x35', '\x45', '\x34', '\x1d', 0, 0
};

/* Slightly different due to compression artifacts. See the 100% test for
   a threshold verification. Needs to have a bigger size otherwise the
   compression makes a total mess. */
constexpr const char ConvertedGrayscaleData[] = {
    '\x01', '\x11', '\x23', '\x27', '\x1c', '\x11', 0, 0,
    '\x65', '\x7d', '\x97', '\x9d', '\x8e', '\x7a', 0, 0,
    '\x3f', '\x60', '\x85', '\x93', '\x88', '\x78', 0, 0,
    '\x00', '\x19', '\x3b', '\x43', '\x32', '\x1e', 0, 0
};

const ImageView2D OriginalGrayscale{PixelStorage{}.setSkip({0, 1, 0}),
    PixelFormat::R8Unorm, {6, 4}, OriginalGrayscaleData};

void JpegImageConverterTest::grayscale80Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");
    CORRADE_COMPARE(converter->configuration().value<Float>("jpegQuality"), 0.8f);

    const auto data = converter->exportToData(OriginalGrayscale);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("JpegImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("JpegImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("JpegImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE(converted->size(), Vector2i(6, 4));
    CORRADE_COMPARE(converted->format(), PixelFormat::R8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(converted->data().size(), 32);
    converted->mutableData()[6] = converted->mutableData()[7] =
        converted->mutableData()[14] = converted->mutableData()[15] =
            converted->mutableData()[22] = converted->mutableData()[23] =
                converted->mutableData()[30] = converted->mutableData()[31] = 0;

    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(ConvertedGrayscaleData),
        TestSuite::Compare::Container);
}

void JpegImageConverterTest::grayscale100Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");
    converter->configuration().setValue("jpegQuality", 1.0f);

    const auto data = converter->exportToData(OriginalGrayscale);
    CORRADE_VERIFY(data);

    if(_importerManager.loadState("JpegImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("JpegImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("JpegImporter");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);

    /* Expect only minimal difference (single bits) */
    CORRADE_COMPARE_WITH(*converted, OriginalGrayscale,
        (DebugTools::CompareImage{1.0f, 0.085f}));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::JpegImageConverterTest)
