/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2021 Pablo Escobar <mail@rvrs.in>

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

#include <algorithm>
#include <sstream>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/FormatStl.h>

#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct KtxImageConverterTest: TestSuite::Tester {
    explicit KtxImageConverterTest();

    /** @todo - depth/stencil
    */

    void supportedFormat();
    void supportedCompressedFormat();
    void unsupportedCompressedFormat();
    void implementationSpecificFormat();

    void tooManyLevels();
    void levelWrongFormat();
    void levelWrongSize();

    void pvrtcRgb();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

const struct {
    const char* name;
    CompressedPixelFormat inputFormat;
    CompressedPixelFormat outputFormat;
} PvrtcRgbData[]{
    {"2bppUnorm", CompressedPixelFormat::PvrtcRGB2bppUnorm, CompressedPixelFormat::PvrtcRGBA2bppUnorm},
    {"2bppSrgb", CompressedPixelFormat::PvrtcRGB2bppSrgb, CompressedPixelFormat::PvrtcRGBA2bppSrgb},
    {"4bppUnorm", CompressedPixelFormat::PvrtcRGB4bppUnorm, CompressedPixelFormat::PvrtcRGBA4bppUnorm},
    {"4bppSrgb", CompressedPixelFormat::PvrtcRGB4bppSrgb, CompressedPixelFormat::PvrtcRGBA4bppSrgb},
};

KtxImageConverterTest::KtxImageConverterTest() {

    addTests({&KtxImageConverterTest::supportedFormat,
              &KtxImageConverterTest::supportedCompressedFormat,
              &KtxImageConverterTest::unsupportedCompressedFormat,
              &KtxImageConverterTest::implementationSpecificFormat,

              &KtxImageConverterTest::tooManyLevels,
              &KtxImageConverterTest::levelWrongFormat,
              &KtxImageConverterTest::levelWrongSize});

    addInstancedTests({&KtxImageConverterTest::pvrtcRgb},
        Containers::arraySize(PvrtcRgbData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef KTXIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(KTXIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Optional plugins that don't have to be here */
    #ifdef KTXIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(KTXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}


void KtxImageConverterTest::supportedFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte data[32]{};

    /* All the formats in PixelFormat are supported */
    /** @todo This needs to be extended when formats are added to PixelFormat */
    constexpr PixelFormat start = PixelFormat::R8Unorm;
    constexpr PixelFormat end = PixelFormat::Depth32FStencil8UI;

    for(UnsignedInt format = UnsignedInt(start); format != UnsignedInt(end); ++format) {
        CORRADE_ITERATION(format);
        CORRADE_INTERNAL_ASSERT(Containers::arraySize(data) >= pixelSize(PixelFormat(format)));
        CORRADE_VERIFY(converter->convertToData(ImageView2D{PixelFormat(format), {1, 1}, data}));
    }
}

const CompressedPixelFormat UnsupportedCompressedFormats[]{
    /* Vulkan has no support (core or extension) for 3D ASTC formats.
        KTX supports them, but through an unreleased extension. */
    CompressedPixelFormat::Astc3x3x3RGBAUnorm,
    CompressedPixelFormat::Astc3x3x3RGBASrgb,
    CompressedPixelFormat::Astc3x3x3RGBAF,
    CompressedPixelFormat::Astc4x3x3RGBAUnorm,
    CompressedPixelFormat::Astc4x3x3RGBASrgb,
    CompressedPixelFormat::Astc4x3x3RGBAF,
    CompressedPixelFormat::Astc4x4x3RGBAUnorm,
    CompressedPixelFormat::Astc4x4x3RGBASrgb,
    CompressedPixelFormat::Astc4x4x3RGBAF,
    CompressedPixelFormat::Astc4x4x4RGBAUnorm,
    CompressedPixelFormat::Astc4x4x4RGBASrgb,
    CompressedPixelFormat::Astc4x4x4RGBAF,
    CompressedPixelFormat::Astc5x4x4RGBAUnorm,
    CompressedPixelFormat::Astc5x4x4RGBASrgb,
    CompressedPixelFormat::Astc5x4x4RGBAF,
    CompressedPixelFormat::Astc5x5x4RGBAUnorm,
    CompressedPixelFormat::Astc5x5x4RGBASrgb,
    CompressedPixelFormat::Astc5x5x4RGBAF,
    CompressedPixelFormat::Astc5x5x5RGBAUnorm,
    CompressedPixelFormat::Astc5x5x5RGBASrgb,
    CompressedPixelFormat::Astc5x5x5RGBAF,
    CompressedPixelFormat::Astc6x5x5RGBAUnorm,
    CompressedPixelFormat::Astc6x5x5RGBASrgb,
    CompressedPixelFormat::Astc6x5x5RGBAF,
    CompressedPixelFormat::Astc6x6x5RGBAUnorm,
    CompressedPixelFormat::Astc6x6x5RGBASrgb,
    CompressedPixelFormat::Astc6x6x5RGBAF,
    CompressedPixelFormat::Astc6x6x6RGBAUnorm,
    CompressedPixelFormat::Astc6x6x6RGBASrgb,
    CompressedPixelFormat::Astc6x6x6RGBAF,
};

void KtxImageConverterTest::supportedCompressedFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[32]{};
    const auto unsupported = Containers::arrayView(UnsupportedCompressedFormats);

    /** @todo This needs to be extended when formats are added to CompressedPixelFormat */
    constexpr CompressedPixelFormat start = CompressedPixelFormat::Bc1RGBUnorm;
    constexpr CompressedPixelFormat end = CompressedPixelFormat::PvrtcRGBA4bppSrgb;

    for(UnsignedInt format = UnsignedInt(start); format != UnsignedInt(end); ++format) {
        if(std::find(unsupported.begin(), unsupported.end(), CompressedPixelFormat(format)) == unsupported.end()) {
            CORRADE_ITERATION(format);
            CORRADE_INTERNAL_ASSERT(Containers::arraySize(bytes) >= compressedBlockDataSize(CompressedPixelFormat(format)));
            CORRADE_VERIFY(converter->convertToData(CompressedImageView2D{CompressedPixelFormat(format), {1, 1}, bytes}));
        }
    }
}

void KtxImageConverterTest::unsupportedCompressedFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    std::ostringstream out;
    Error redirectError{&out};

    for(CompressedPixelFormat format: UnsupportedCompressedFormats) {
        CORRADE_ITERATION(format);
        out.str({}); /* Reset stream */
        CORRADE_VERIFY(!converter->convertToData(CompressedImageView2D{format, {1, 1}, {}}));

        /** @todo Is there no better way to do this? */
        std::ostringstream formattedOut;
        Debug redirectDebug{&formattedOut};
        Debug{} << "Trade::KtxImageConverter::convertToData(): unsupported format" << format;

        CORRADE_COMPARE(out.str(), formattedOut.str());
    }
}

/* Actual type needed for ADL, typedef of integer type isn't enough */
enum CustomPixelFormat : UnsignedInt {
    Value
};

UnsignedInt pixelSize(CustomPixelFormat) {
    return 1u;
}

void KtxImageConverterTest::implementationSpecificFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[]{1};

    std::ostringstream out;
    Error redirectError{&out};

    PixelStorage storage;
    storage.setAlignment(1);
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{storage, CustomPixelFormat{}, {1, 1}, bytes}));
    CORRADE_COMPARE(out.str(),
        "Trade::KtxImageConverter::convertToData(): implementation-specific formats are not supported\n");
}

void KtxImageConverterTest::tooManyLevels() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[4]{};

    std::ostringstream out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(converter->convertToData({
        ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, bytes},
        ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, bytes}
    }));
    CORRADE_COMPARE(out.str(),
        "Trade::KtxImageConverter::convertToData(): expected at most 1 mip "
        "level images but got 2, extra images will be ignored\n");
}

void KtxImageConverterTest::levelWrongFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[16]{};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData({
        ImageView2D{PixelFormat::RGB8Unorm, {2, 2}, bytes},
        ImageView2D{PixelFormat::RGB8Snorm, {1, 1}, bytes}
    }));
    CORRADE_COMPARE(out.str(),
        "Trade::KtxImageConverter::convertToData(): expected format PixelFormat::RGB8Unorm "
        "for level 1 but got PixelFormat::RGB8Snorm\n");
}

void KtxImageConverterTest::levelWrongSize() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[16]{};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData({
        ImageView2D{PixelFormat::RGB8Unorm, {2, 2}, bytes},
        ImageView2D{PixelFormat::RGB8Unorm, {2, 1}, bytes}
    }));
    CORRADE_COMPARE(out.str(),
        "Trade::KtxImageConverter::convertToData(): expected size Vector(1, 1) for level 1 but got Vector(2, 1)\n");
}

void KtxImageConverterTest::pvrtcRgb() {
    auto&& data = PvrtcRgbData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[16]{};

    const UnsignedInt dataSize = compressedBlockDataSize(data.inputFormat);
    CORRADE_INTERNAL_ASSERT(Containers::arraySize(bytes) >= dataSize);

    const Vector2i imageSize = {2, 2};
    CORRADE_INTERNAL_ASSERT((Vector3i{imageSize, 1}) <= compressedBlockSize(data.inputFormat));

    const CompressedImageView2D inputImage{data.inputFormat, imageSize, Containers::arrayView(bytes).prefix(dataSize)};
    const auto output = converter->convertToData(inputImage);
    CORRADE_VERIFY(output);

    if(_importerManager.loadState("KtxImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openData(output));

    const auto image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->compressedFormat(), data.outputFormat);
    CORRADE_COMPARE_AS(image->data(), inputImage.data(), TestSuite::Compare::Container);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::KtxImageConverterTest)
