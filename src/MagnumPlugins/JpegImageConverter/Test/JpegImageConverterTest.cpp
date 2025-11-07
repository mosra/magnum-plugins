/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
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

#include <Corrade/Containers/Optional.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
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

/* jpeglib.h is needed to query if RGBA output is supported (JCS_EXTENSIONS).
   See JpegImageConverter.cpp for details why the define below, and the
   <cstdio> include, are needed. */
#ifdef CORRADE_TARGET_WINDOWS
#define XMD_H
#endif
#include <cstdio>
#include <jpeglib.h>

namespace Magnum { namespace Trade { namespace Test { namespace {

struct JpegImageConverterTest: TestSuite::Tester {
    explicit JpegImageConverterTest();

    void wrongFormat();
    void conversionError();

    void rgb80Percent();
    void rgb100Percent();

    void rgba80Percent();

    void grayscale80Percent();
    void grayscale100Percent();

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
        "1D array images are unrepresentable in JPEG, saving as a regular 2D image"},
    {"1D array, quiet", ImageConverterFlag::Quiet, ImageFlag2D::Array,
        nullptr},
};

JpegImageConverterTest::JpegImageConverterTest() {
    addTests({&JpegImageConverterTest::wrongFormat,
              &JpegImageConverterTest::conversionError,

              &JpegImageConverterTest::rgb80Percent,
              &JpegImageConverterTest::rgb100Percent});

    addInstancedTests({&JpegImageConverterTest::rgba80Percent},
        Containers::arraySize(QuietData));

    addTests({&JpegImageConverterTest::grayscale80Percent,
              &JpegImageConverterTest::grayscale100Percent});

    addInstancedTests({&JpegImageConverterTest::unsupportedMetadata},
        Containers::arraySize(UnsupportedMetadataData));

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

    const char data[4]{};
    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::R16F, {1, 1}, data}));
    CORRADE_COMPARE(out, "Trade::JpegImageConverter::convertToData(): unsupported pixel format PixelFormat::R16F\n");
}

void JpegImageConverterTest::conversionError() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");

    /* Because zero-size images are disallowed by the base implementation
       already, we can't abuse that for checking conversion errors. JPEG image
       width/height is limited to 24 bits, so let's pretend we have a 16 MB
       image. Hope this won't trigger sanitizers. */
    const char data[1]{};
    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::R8Unorm, {16*1024*1024, 1}, {data, 16*1024*1024}}));
    CORRADE_COMPARE(out, "Trade::JpegImageConverter::convertToData(): error: Maximum supported image dimension is 65500 pixels\n");
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
};

const ImageView2D ConvertedRgb{PixelFormat::RGB8Unorm, {6, 4}, ConvertedRgbData};

void JpegImageConverterTest::rgb80Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");
    CORRADE_COMPARE(converter->extension(), "jpg");
    CORRADE_COMPARE(converter->mimeType(), "image/jpeg");
    CORRADE_COMPARE(converter->configuration().value<Float>("jpegQuality"), 0.8f);

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(OriginalRgb);
    CORRADE_VERIFY(data);
    /* Vanilla libjpeg 9f (i.e., not libjpeg-turbo which defines
       JCS_EXTENSIONS, and which has JPEG_LIB_VERSION set to 80 always) and
       older produce different results in this case. The minor version is 0
       for nothing, 1 for a, 2 for b, etc.  */
    #if defined(JCS_EXTENSIONS) || JPEG_LIB_VERSION_MAJOR*100 + JPEG_LIB_VERSION_MINOR >= 907
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(JPEGIMAGECONVERTER_TEST_DIR, "rgb-80.jpg"),
        TestSuite::Compare::StringToFile);
    #elif JPEG_LIB_VERSION_MAJOR*100 + JPEG_LIB_VERSION_MINOR >= 905
    /* This matches also 9f */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(JPEGIMAGECONVERTER_TEST_DIR, "rgb-80-jpeg9e.jpg"),
        TestSuite::Compare::StringToFile);
    #else
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(JPEGIMAGECONVERTER_TEST_DIR, "rgb-80-jpeg9d.jpg"),
        TestSuite::Compare::StringToFile);
    #endif

    if(_importerManager.loadState("JpegImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("JpegImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("JpegImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    #if defined(JCS_EXTENSIONS) || JPEG_LIB_VERSION_MAJOR*100 + JPEG_LIB_VERSION_MINOR >= 907
    CORRADE_COMPARE_AS(*converted, ConvertedRgb,
        DebugTools::CompareImage);
    #elif JPEG_LIB_VERSION_MAJOR*100 + JPEG_LIB_VERSION_MINOR >= 905
    /* This matches also 9f */
    CORRADE_COMPARE_WITH(*converted, ConvertedRgb,
        (DebugTools::CompareImage{3.67f, 2.21f}));
    #else
    CORRADE_COMPARE_WITH(*converted, ConvertedRgb,
        (DebugTools::CompareImage{3.67f, 2.0f}));
    #endif
}

void JpegImageConverterTest::rgb100Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");
    converter->configuration().setValue("jpegQuality", 1.0f);

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(OriginalRgb);
    CORRADE_VERIFY(data);
    /* Vanilla libjpeg 9f (i.e., not libjpeg-turbo which defines
       JCS_EXTENSIONS, and which has JPEG_LIB_VERSION set to 80 always) and
       older produce a different result in this case. The minor version is 0
       for nothing, 1 for a, 2 for b, etc.  */
    #if defined(JCS_EXTENSIONS) || JPEG_LIB_VERSION_MAJOR*100 + JPEG_LIB_VERSION_MINOR >= 907
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(JPEGIMAGECONVERTER_TEST_DIR, "rgb-100.jpg"),
        TestSuite::Compare::StringToFile);
    #else
    /* This matches also 9f */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(JPEGIMAGECONVERTER_TEST_DIR, "rgb-100-jpeg9e.jpg"),
        TestSuite::Compare::StringToFile);
    #endif

    if(_importerManager.loadState("JpegImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("JpegImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("JpegImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_WITH(*converted, OriginalRgb,
        /* Expect only minimal difference (single bits) */
        (DebugTools::CompareImage{3.1f, 1.4f}));
}

void JpegImageConverterTest::rgba80Percent() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");
    converter->addFlags(data.flags);
    CORRADE_COMPARE(converter->configuration().value<Float>("jpegQuality"), 0.8f);

    /* If we don't have libjpeg-turbo, exporting RGBA will fail */
    #ifndef JCS_EXTENSIONS
    {
        Containers::String out;
        Error redirectError{&out};
        CORRADE_VERIFY(!converter->convertToData(OriginalRgba));
        CORRADE_COMPARE(out, "Trade::JpegImageConverter::convertToData(): RGBA input (with alpha ignored) requires libjpeg-turbo\n");
    }

    CORRADE_SKIP("libjpeg-turbo is required for RGBA support.");
    #endif

    /* RGBA should be exported as RGB, with the alpha channel ignored (and a
       warning about that printed) */
    Containers::String out;
    Containers::Optional<Containers::Array<char>> imageData;
    {
        Warning redirectWarning{&out};
        imageData = converter->convertToData(OriginalRgba);
    }
    CORRADE_VERIFY(imageData);
    if(data.quiet)
        CORRADE_COMPARE(out, "");
    else
        CORRADE_COMPARE(out, "Trade::JpegImageConverter::convertToData(): ignoring alpha channel\n");
    /* The output should be exactly the same as when exporting RGB, bit to bit,
       to ensure we don't produce anything that would cause problems for
       traditional non-turbo libjpeg */
    CORRADE_COMPARE_AS(Containers::StringView{*imageData},
        Utility::Path::join(JPEGIMAGECONVERTER_TEST_DIR, "rgb-80.jpg"),
        TestSuite::Compare::StringToFile);
}

constexpr const char OriginalGrayscaleData[] = {
    0, 0, 0, 0, 0, 0, 0, 0, /* Skip */

    '\x00', '\x10', '\x22', '\x25', '\x21', '\x13', 0, 0,
    '\x5b', '\x85', '\x94', '\x96', '\x91', '\x72', 0, 0,
    '\x3c', '\x68', '\x8b', '\x92', '\x8b', '\x73', 0, 0,
    '\x00', '\x12', '\x35', '\x45', '\x34', '\x1d', 0, 0
};

const ImageView2D OriginalGrayscale{PixelStorage{}.setSkip({0, 1, 0}),
    PixelFormat::R8Unorm, {6, 4}, OriginalGrayscaleData};

/* Slightly different due to compression artifacts. See the 100% test for
   a threshold verification. Needs to have a bigger size otherwise the
   compression makes a total mess. */
constexpr const char ConvertedGrayscaleData[] = {
    '\x01', '\x11', '\x23', '\x27', '\x1c', '\x11', 0, 0,
    '\x65', '\x7d', '\x97', '\x9d', '\x8e', '\x7a', 0, 0,
    '\x3f', '\x60', '\x85', '\x93', '\x88', '\x78', 0, 0,
    '\x00', '\x19', '\x3b', '\x43', '\x32', '\x1e', 0, 0
};

const ImageView2D ConvertedGrayscale{PixelFormat::R8Unorm, {6, 4}, ConvertedGrayscaleData};

void JpegImageConverterTest::grayscale80Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");
    CORRADE_COMPARE(converter->configuration().value<Float>("jpegQuality"), 0.8f);

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(OriginalGrayscale);
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(JPEGIMAGECONVERTER_TEST_DIR, "grayscale-80.jpg"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("JpegImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("JpegImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("JpegImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_AS(*converted, ConvertedGrayscale,
        DebugTools::CompareImage);
}

void JpegImageConverterTest::grayscale100Percent() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");
    converter->configuration().setValue("jpegQuality", 1.0f);

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(OriginalGrayscale);
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(JPEGIMAGECONVERTER_TEST_DIR, "grayscale-100.jpg"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("JpegImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("JpegImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("JpegImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_WITH(*converted, OriginalGrayscale,
        /* Expect only minimal difference (single bits) */
        (DebugTools::CompareImage{1.0f, 0.085f}));
}

void JpegImageConverterTest::unsupportedMetadata() {
    auto&& data = UnsupportedMetadataData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("JpegImageConverter");
    converter->addFlags(data.converterFlags);

    const char imageData[4]{};
    ImageView2D image{PixelFormat::RGB8Unorm, {1, 1}, imageData, data.imageFlags};

    Containers::String out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(converter->convertToData(image));
    if(!data.message)
        CORRADE_COMPARE(out, "");
    else
        CORRADE_COMPARE(out, Utility::format("Trade::JpegImageConverter::convertToData(): {}\n", data.message));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::JpegImageConverterTest)
