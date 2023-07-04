/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct PngImageConverterTest: TestSuite::Tester {
    explicit PngImageConverterTest();

    void wrongFormat();
    void conversionError();

    void rgb();
    void rgb16();

    void grayscale();
    void grayscale16();

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
        "1D array images are unrepresentable in PNG, saving as a regular 2D image"},
    {"1D array, quiet", ImageConverterFlag::Quiet, ImageFlag2D::Array,
        nullptr},
};

PngImageConverterTest::PngImageConverterTest() {
    addTests({&PngImageConverterTest::wrongFormat});

    addInstancedTests({&PngImageConverterTest::conversionError},
        Containers::arraySize(QuietData));

    addTests({&PngImageConverterTest::rgb,
              &PngImageConverterTest::rgb16,

              &PngImageConverterTest::grayscale,
              &PngImageConverterTest::grayscale16});

    addInstancedTests({&PngImageConverterTest::unsupportedMetadata},
        Containers::arraySize(UnsupportedMetadataData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef PNGIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(PNGIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* The PngImporter is optional */
    #ifdef PNGIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(PNGIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void PngImageConverterTest::wrongFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");

    const char data[8]{};
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RG32F, {1, 1}, data}));
    CORRADE_COMPARE(out.str(), "Trade::PngImageConverter::convertToData(): unsupported pixel format PixelFormat::RG32F\n");
}

void PngImageConverterTest::conversionError() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Important: this also tests warning suppression! */

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");
    converter->addFlags(data.flags);

    /* Because zero-size images are disallowed by the base implementation
       already, we can't abuse that for checking conversion errors. PNG image
       width/height is limited to 31 bits, so let's pretend we have a 2 GB
       image. Hope this won't trigger sanitizers. */
    const char imageData[1]{};
    std::ostringstream out;
    Warning redirectWarning{&out};
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::R8Unorm, {0x7fffffff, 1}, {imageData, 1u << 31}}));
    if(data.quiet)
        CORRADE_COMPARE(out.str(),
            "Trade::PngImageConverter::convertToData(): error: Invalid IHDR data\n");
    else CORRADE_COMPARE(out.str(),
            "Trade::PngImageConverter::convertToData(): warning: Image width exceeds user limit in IHDR\n"
            "Trade::PngImageConverter::convertToData(): error: Invalid IHDR data\n");
}

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
    1, 2, 3, 2, 3, 4, 0, 0,
    3, 4, 5, 4, 5, 6, 0, 0,
    5, 6, 7, 6, 7, 8, 0, 0
};

const ImageView2D ConvertedRgb{PixelFormat::RGB8Unorm, {2, 3}, ConvertedRgbData};

void PngImageConverterTest::rgb() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");
    CORRADE_COMPARE(converter->extension(), "png");
    CORRADE_COMPARE(converter->mimeType(), "image/png");

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(OriginalRgb);
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(*data,
        Utility::Path::join(PNGIMAGECONVERTER_TEST_DIR, "rgb.png"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_AS(*converted, ConvertedRgb,
        DebugTools::CompareImage);
}

constexpr const UnsignedShort OriginalRgbData16[] = {
    /* Skip */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    1000, 2000, 3000, 2000, 3000, 4000, 0, 0, 0, 0,
    3000, 4000, 5000, 4000, 5000, 6000, 0, 0, 0, 0,
    5000, 6000, 7000, 6000, 7000, 8000, 0, 0, 0, 0
};

const ImageView2D OriginalRgb16{PixelStorage{}.setSkip({0, 1, 0}).setRowLength(3),
    PixelFormat::RGB16Unorm, {2, 3}, OriginalRgbData16};

constexpr const UnsignedShort ConvertedRgbData16[] = {
    1000, 2000, 3000, 2000, 3000, 4000,
    3000, 4000, 5000, 4000, 5000, 6000,
    5000, 6000, 7000, 6000, 7000, 8000
};

const ImageView2D ConvertedRgb16{PixelFormat::RGB16Unorm, {2, 3}, ConvertedRgbData16};

void PngImageConverterTest::rgb16() {
    Containers::Optional<Containers::Array<char>> data = _converterManager.instantiate("PngImageConverter")->convertToData(OriginalRgb16);
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(*data,
        Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb16.png"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_AS(*converted, ConvertedRgb16,
        DebugTools::CompareImage);
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
    1, 2, 0, 0,
    3, 4, 0, 0,
    5, 6, 0, 0
};

const ImageView2D ConvertedGrayscale{PixelFormat::R8Unorm, {2, 3}, ConvertedGrayscaleData};

void PngImageConverterTest::grayscale() {
    Containers::Optional<Containers::Array<char>> data = _converterManager.instantiate("PngImageConverter")->convertToData(OriginalGrayscale);
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(*data,
        Utility::Path::join(PNGIMAGECONVERTER_TEST_DIR, "gray.png"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_AS(*converted, ConvertedGrayscale,
        DebugTools::CompareImage);
}

constexpr const UnsignedShort OriginalGrayscaleData16[] = {
    /* Skip */
    0, 0, 0, 0,

    1000, 2000, 0, 0,
    3000, 4000, 0, 0,
    5000, 6000, 0, 0
};

const ImageView2D OriginalGrayscale16{PixelStorage{}.setSkip({0, 1, 0}).setRowLength(3),
    PixelFormat::R16Unorm, {2, 3}, OriginalGrayscaleData16};

constexpr const UnsignedShort ConvertedGrayscaleData16[] = {
    1000, 2000,
    3000, 4000,
    5000, 6000
};

const ImageView2D ConvertedGrayscale16{PixelFormat::R16Unorm, {2, 3}, ConvertedGrayscaleData16};

void PngImageConverterTest::grayscale16() {
    Containers::Optional<Containers::Array<char>> data = _converterManager.instantiate("PngImageConverter")->convertToData(OriginalGrayscale16);
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(*data,
        Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray16.png"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_AS(*converted, ConvertedGrayscale16,
        DebugTools::CompareImage);
}

void PngImageConverterTest::unsupportedMetadata() {
    auto&& data = UnsupportedMetadataData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");
    converter->addFlags(data.converterFlags);

    const char imageData[4]{};
    ImageView2D image{PixelFormat::RGBA8Unorm, {1, 1}, imageData, data.imageFlags};

    std::ostringstream out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(converter->convertToData(image));
    if(!data.message)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::PngImageConverter::convertToData(): {}\n", data.message));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::PngImageConverterTest)
