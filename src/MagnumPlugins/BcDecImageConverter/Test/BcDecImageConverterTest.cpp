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
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct BcDecImageConverterTest: TestSuite::Tester {
    explicit BcDecImageConverterTest();

    void test();
    void preserveFlags();

    void unsupportedFormat();
    void unsupportedStorage();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _manager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

using namespace Containers::Literals;

const struct {
    const char* name;
    Containers::String file;
    CompressedPixelFormat format;
    Containers::String expected;
    PixelFormat expectedFormat;
    Containers::Optional<Int> forceChannelCount;
    Containers::Optional<bool> bc6hToFloat;
    Float maxThreshold, meanThreshold;
} TestData[]{
    /* Correspondence of the DDS files to the uncompressed input can be seen in
       DdsImporter/Test/convert.sh */
    {"BC1 RGBA, single incomplete block",
        Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt1.dds"),
        CompressedPixelFormat::Bc1RGBAUnorm,
        Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb.png"),
        PixelFormat::RGBA8Unorm,
        4, {}, 2.25f, 1.25f},
    {"BC1 RGBA, single incomplete block, sRGB",
        Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-compressed-bc1.ktx2"),
        CompressedPixelFormat::Bc1RGBASrgb,
        Utility::Path::join(KTXIMPORTER_TEST_DIR, "pattern-pot.png"),
        PixelFormat::RGBA8Srgb,
        4, {}, 2.0f, 0.5f},
    {"BC2 RGBA",
        Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt3.dds"),
        CompressedPixelFormat::Bc2RGBAUnorm,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-64x32.png"),
        PixelFormat::RGBA8Unorm,
        {}, {}, 16.25f, 1.92f},
    {"BC2 RGBA, incomplete blocks",
        Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt3-incomplete-blocks.dds"),
        CompressedPixelFormat::Bc2RGBAUnorm,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        PixelFormat::RGBA8Unorm,
        {}, {}, 73.0f, 2.2f},
    {"BC3 RGBA, single incomplete block",
        Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt5.dds"),
        CompressedPixelFormat::Bc3RGBAUnorm,
        Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb.png"),
        PixelFormat::RGBA8Unorm,
        4, {}, 2.25f, 1.25f},
    /** @todo the thresholds are way too high for the single/two component
        formats, why? when testing with the dice_bc4.dds / dice_bc5.dds from
        the bcdec repo it seems to work properly, but generating the data from
        rgba-64x32.png isn't any better, so I suspect Compressonator is just
        a total shit for these formats. */
    {"BC4 unsigned",
        Utility::Path::join(DDSIMPORTER_TEST_DIR, "bc4unorm.dds"),
        CompressedPixelFormat::Bc4RUnorm,
        Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb.png"),
        PixelFormat::R8Unorm,
        1, {}, 34.0f, 27.5f},
    {"BC4 signed",
        Utility::Path::join(DDSIMPORTER_TEST_DIR, "bc4snorm.dds"),
        CompressedPixelFormat::Bc4RSnorm,
        Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb.png"),
        PixelFormat::R8Snorm,
        1, {}, 162.0f, 134.5f},
    {"BC5 unsigned",
        Utility::Path::join(DDSIMPORTER_TEST_DIR, "bc5unorm.dds"),
        CompressedPixelFormat::Bc5RGUnorm,
        Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb.png"),
        PixelFormat::RG8Unorm,
        2, {}, 58.0f, 34.5f},
    {"BC5 signed",
        Utility::Path::join(DDSIMPORTER_TEST_DIR, "bc5snorm.dds"),
        CompressedPixelFormat::Bc5RGSnorm,
        Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb.png"),
        PixelFormat::RG8Snorm,
        2, {}, 117.0f, 110.5f},
    {"BC6H unsigned, incomplete blocks",
        Utility::Path::join(BCDECIMAGECONVERTER_TEST_DIR, "bc6h.dds"),
        CompressedPixelFormat::Bc6hRGBUfloat,
        Utility::Path::join(BCDECIMAGECONVERTER_TEST_DIR, "rgb16f.ktx2"),
        PixelFormat::RGB16F,
        {}, {}, 2.24f, 1.02f},
    {"BC6H unsigned, incomplete blocks, to float",
        Utility::Path::join(BCDECIMAGECONVERTER_TEST_DIR, "bc6h.dds"),
        CompressedPixelFormat::Bc6hRGBUfloat,
        Utility::Path::join(STBIMAGEIMPORTER_TEST_DIR, "rgb.hdr"),
        PixelFormat::RGB32F,
        {}, true, 2.24f, 1.02f},
    {"BC6H signed, incomplete blocks",
        Utility::Path::join(BCDECIMAGECONVERTER_TEST_DIR, "bc6hs.dds"),
        CompressedPixelFormat::Bc6hRGBSfloat,
        Utility::Path::join(BCDECIMAGECONVERTER_TEST_DIR, "rgb16f.ktx2"),
        PixelFormat::RGB16F,
        {}, {}, 0.79f, 0.34f},
    {"BC6H signed, incomplete blocks, to float",
        Utility::Path::join(BCDECIMAGECONVERTER_TEST_DIR, "bc6hs.dds"),
        CompressedPixelFormat::Bc6hRGBSfloat,
        Utility::Path::join(STBIMAGEIMPORTER_TEST_DIR, "rgb.hdr"),
        PixelFormat::RGB32F,
        {}, true, 0.79f, 0.34f},
    {"BC7",
        Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt10-bc7.dds"),
        CompressedPixelFormat::Bc7RGBAUnorm,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-64x32.png"),
        PixelFormat::RGBA8Unorm,
        {}, {}, 3.5f, 0.41f},
};

BcDecImageConverterTest::BcDecImageConverterTest() {
    addInstancedTests({&BcDecImageConverterTest::test},
        Containers::arraySize(TestData));

    addTests({&BcDecImageConverterTest::preserveFlags,

              &BcDecImageConverterTest::unsupportedFormat,
              &BcDecImageConverterTest::unsupportedStorage});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef BCDECIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(BCDECIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef DDSIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(DDSIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef KTXIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(KTXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void BcDecImageConverterTest::test() {
    auto&& data = TestData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    const char* importerName;
    if(data.file.hasSuffix(".dds")) {
        importerName = "DdsImporter";
    } else if(data.file.hasSuffix(".ktx2")) {
        importerName = "KtxImporter";
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE();

    if(_importerManager.loadState(importerName) == PluginManager::LoadState::NotFound)
        CORRADE_SKIP(importerName << "plugin not found, cannot test conversion");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate(importerName);
    /* The DDS / KTX files are not with Y up but we don't want the plugin to Y
       flip (or warn), as that could be another source of error. Instead we
       tell the importers to assume they're Y up and the expected image is
       flipped to Y down on load. */
    /** @todo clean this up once it's possible to configure Y flipping behavior
        via a flag */
    if(importerName == "DdsImporter"_s)
        importer->configuration().setValue("assumeYUpZBackward", true);
    else if(importerName == "KtxImporter"_s)
        importer->configuration().setValue("assumeOrientation", "ruo");
    CORRADE_VERIFY(importer->openFile(data.file));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->compressedFormat(), data.format);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("BcDecImageConverter");
    if(data.bc6hToFloat)
        converter->configuration().setValue("bc6hToFloat", *data.bc6hToFloat);
    Containers::Optional<Trade::ImageData2D> converted = converter->convert(*image);
    CORRADE_VERIFY(converted);
    CORRADE_VERIFY(!converted->isCompressed());
    CORRADE_COMPARE(converted->format(), data.expectedFormat);
    CORRADE_COMPARE(converted->size(), image->size());

    const char* expectedImporterName;
    if(data.expected.hasSuffix(".png") ||
       data.expected.hasSuffix(".hdr")) {
        expectedImporterName = "StbImageImporter";
    } else if(data.expected.hasSuffix(".ktx2")) {
        expectedImporterName = "KtxImporter";
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE();

    if(_importerManager.loadState(expectedImporterName) == PluginManager::LoadState::NotFound)
        CORRADE_SKIP(expectedImporterName << "plugin not found, cannot compare converted output");

    /* Not using CompareImageToFile as we need to override the channel count
       in some cases and Y-flip the expected  */
    Containers::Pointer<AbstractImporter> expectedImporter = _importerManager.instantiate(expectedImporterName);
    if(data.forceChannelCount)
        expectedImporter->configuration().setValue("forceChannelCount", *data.forceChannelCount);
    CORRADE_VERIFY(expectedImporter->openFile(data.expected));

    /* Since the input DDS was not Y-flipped, flip the expected image
       instead */
    /** @todo clean this up once it's possible to configure Y flipping behavior
        via a flag */
    Containers::Optional<Trade::ImageData2D> expectedImage = expectedImporter->image2D(0);
    CORRADE_VERIFY(expectedImage);
    Utility::flipInPlace<0>(expectedImage->mutablePixels());
    /* And override the pixel format to match the expected */
    CORRADE_COMPARE(pixelFormatSize(data.expectedFormat), pixelFormatSize(expectedImage->format()));
    CORRADE_COMPARE_WITH(*converted,
        (ImageView2D{expectedImage->storage(), data.expectedFormat, expectedImage->size(), expectedImage->data()}),
        (DebugTools::CompareImage{data.maxThreshold, data.meanThreshold}));
}

void BcDecImageConverterTest::preserveFlags() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("BcDecImageConverter");

    /* Just verify that the flags don't get lost in the process. Everything
       else is tested well enough above. */
    Containers::Optional<ImageData2D> converted = converter->convert(CompressedImageView2D{CompressedPixelFormat::Bc1RGBAUnorm, {1, 1}, "yeyhey!", ImageFlag2D::Array});
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE(converted->flags(), ImageFlag2D::Array);
}

void BcDecImageConverterTest::unsupportedFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("BcDecImageConverter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(CompressedImageView2D{CompressedPixelFormat::Etc2RGB8Srgb, {1, 1}, "yey"}));
    CORRADE_COMPARE(out.str(), "Trade::BcDecImageConverter::convert(): unsupported format CompressedPixelFormat::Etc2RGB8Srgb\n");
}

void BcDecImageConverterTest::unsupportedStorage() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("BcDecImageConverter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(CompressedImageView2D{CompressedPixelStorage{}.setCompressedBlockDataSize(16), CompressedPixelFormat::Bc3RGBASrgb, {1, 1}, "yey"}));
    CORRADE_COMPARE(out.str(), "Trade::BcDecImageConverter::convert(): non-default compressed storage is not supported\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::BcDecImageConverterTest)
