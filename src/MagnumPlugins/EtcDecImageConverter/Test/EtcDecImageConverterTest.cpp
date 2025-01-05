/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024
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
#include <Corrade/Containers/String.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct EtcDecImageConverterTest: TestSuite::Tester {
    explicit EtcDecImageConverterTest();

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
    bool yDown;
    Containers::Optional<Int> forceChannelCount;
    Containers::Optional<Int> forceBitDepth;
    Containers::Optional<bool> eacToFloat;
    Float maxThreshold, meanThreshold;
} TestData[]{
    /* Correspondence of the KTX files to the uncompressed input can be seen in
       convert.sh */
    /** @todo same as with BcDecImageConverter, the thresholds are way too high
        for the single- and two-channel images, for some reason ... investigate
        using some different source than Basis for these */
    {"EAC R unsigned, incomplete blocks",
        Utility::Path::join(ETCDECIMAGECONVERTER_TEST_DIR, "eac-r.ktx2"),
        CompressedPixelFormat::EacR11Unorm,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        PixelFormat::R16Unorm,
        false, 1, 16, {}, 37218.0f, 18393.0f},
    {"EAC RG unsigned",
        Utility::Path::join(ETCDECIMAGECONVERTER_TEST_DIR, "eac-rg.ktx2"),
        CompressedPixelFormat::EacRG11Unorm,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-64x32.png"),
        PixelFormat::RG16Unorm,
        false, 2, 16, {}, 14634.0f, 9414.0f},
    {"EAC R unsigned, incomplete blocks, to float",
        Utility::Path::join(ETCDECIMAGECONVERTER_TEST_DIR, "eac-r.ktx2"),
        CompressedPixelFormat::EacR11Unorm,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        PixelFormat::R32F,
        false, 1, 32, true, 0.834f, 0.422f},
    {"EAC RG unsigned, to float",
        Utility::Path::join(ETCDECIMAGECONVERTER_TEST_DIR, "eac-rg.ktx2"),
        CompressedPixelFormat::EacRG11Unorm,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-64x32.png"),
        PixelFormat::RG32F,
        false, 2, 32, true, 0.354f, 0.209f},
    /** @todo signed EAC, once a tool capable of producing it is discovered */
    {"ETC2 RGB8 sRGB, incomplete blocks",
        Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-compressed-etc2.ktx2"),
        CompressedPixelFormat::Etc2RGB8Srgb,
        Utility::Path::join(KTXIMPORTER_TEST_DIR, "pattern-uneven.png"),
        PixelFormat::RGBA8Srgb,
        true, 4, {}, {}, 1.0f, 0.18f},
    {"ETC2 RGB8A1, incomplete blocks",
        Utility::Path::join(ETCDECIMAGECONVERTER_TEST_DIR, "etc-rgb8a1.ktx2"),
        CompressedPixelFormat::Etc2RGB8A1Unorm,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"),
        PixelFormat::RGBA8Unorm,
        true, 4, {}, {}, 18.75f, 1.17f},
    {"ETC2 RGBA8",
        Utility::Path::join(ETCDECIMAGECONVERTER_TEST_DIR, "etc-rgba8.ktx2"),
        CompressedPixelFormat::Etc2RGBA8Unorm,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-64x32.png"),
        PixelFormat::RGBA8Unorm,
        true, {}, {}, {}, 17.0f, 1.62f},
};

EtcDecImageConverterTest::EtcDecImageConverterTest() {
    addInstancedTests({&EtcDecImageConverterTest::test},
        Containers::arraySize(TestData));

    addTests({&EtcDecImageConverterTest::preserveFlags,

              &EtcDecImageConverterTest::unsupportedFormat,
              &EtcDecImageConverterTest::unsupportedStorage});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef ETCDECIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(ETCDECIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef KTXIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(KTXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void EtcDecImageConverterTest::test() {
    auto&& data = TestData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    const char* importerName;
    if(data.file.hasSuffix(".ktx2")) {
        importerName = "KtxImporter";
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE();

    if(_importerManager.loadState(importerName) == PluginManager::LoadState::NotFound)
        CORRADE_SKIP(importerName << "plugin not found, cannot test conversion");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate(importerName);
    /* If the file isn't with Y up, we don't want the plugin to Y flip (or
       warn), as that could be another source of error. Instead we
       tell the importers to assume they're Y up and the expected image is
       flipped to Y down on load. */
    /** @todo clean this up once it's possible to configure Y flipping behavior
        via a flag */
    if(data.yDown) {
        if(importerName == "KtxImporter"_s)
            importer->configuration().setValue("assumeOrientation", "ruo");
        else CORRADE_INTERNAL_ASSERT_UNREACHABLE();
    }
    CORRADE_VERIFY(importer->openFile(data.file));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->compressedFormat(), data.format);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("EtcDecImageConverter");
    if(data.eacToFloat)
        converter->configuration().setValue("eacToFloat", *data.eacToFloat);
    Containers::Optional<Trade::ImageData2D> converted = converter->convert(*image);
    CORRADE_VERIFY(converted);
    CORRADE_VERIFY(!converted->isCompressed());
    CORRADE_COMPARE(converted->format(), data.expectedFormat);
    CORRADE_COMPARE(converted->size(), image->size());

    const char* expectedImporterName;
    if(data.expected.hasSuffix(".png")) {
        expectedImporterName = "StbImageImporter";
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE();

    if(_importerManager.loadState(expectedImporterName) == PluginManager::LoadState::NotFound)
        CORRADE_SKIP(expectedImporterName << "plugin not found, cannot compare converted output");

    /* Not using CompareImageToFile as we need to override the channel count
       in some cases and Y-flip the expected  */
    Containers::Pointer<AbstractImporter> expectedImporter = _importerManager.instantiate(expectedImporterName);
    if(data.forceChannelCount)
        expectedImporter->configuration().setValue("forceChannelCount", *data.forceChannelCount);
    if(data.forceBitDepth)
        expectedImporter->configuration().setValue("forceBitDepth", *data.forceBitDepth);
    CORRADE_VERIFY(expectedImporter->openFile(data.expected));

    Containers::Optional<Trade::ImageData2D> expectedImage = expectedImporter->image2D(0);
    CORRADE_VERIFY(expectedImage);
    /* If the input KTX was not Y up, flip the expected image instead */
    /** @todo clean this up once it's possible to configure Y flipping behavior
        via a flag */
    if(data.yDown)
        Utility::flipInPlace<0>(expectedImage->mutablePixels());
    /* And override the pixel format to match the expected */
    CORRADE_COMPARE(pixelFormatSize(data.expectedFormat), pixelFormatSize(expectedImage->format()));
    CORRADE_COMPARE_WITH(*converted,
        (ImageView2D{expectedImage->storage(), data.expectedFormat, expectedImage->size(), expectedImage->data()}),
        (DebugTools::CompareImage{data.maxThreshold, data.meanThreshold}));
}

void EtcDecImageConverterTest::preserveFlags() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("EtcDecImageConverter");

    /* Just verify that the flags don't get lost in the process. Everything
       else is tested well enough above. */
    Containers::Optional<ImageData2D> converted = converter->convert(CompressedImageView2D{CompressedPixelFormat::EacR11Snorm, {1, 1}, "yeyhey!", ImageFlag2D::Array});
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE(converted->flags(), ImageFlag2D::Array);
}

void EtcDecImageConverterTest::unsupportedFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("EtcDecImageConverter");

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(CompressedImageView2D{CompressedPixelFormat::Bc1RGBASrgb, {1, 1}, "yey"}));
    CORRADE_COMPARE(out, "Trade::EtcDecImageConverter::convert(): unsupported format CompressedPixelFormat::Bc1RGBASrgb\n");
}

void EtcDecImageConverterTest::unsupportedStorage() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("EtcDecImageConverter");

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(CompressedImageView2D{CompressedPixelStorage{}.setCompressedBlockDataSize(8), CompressedPixelFormat::EacR11Snorm, {1, 1}, "yey"}));
    CORRADE_COMPARE(out, "Trade::EtcDecImageConverter::convert(): non-default compressed storage is not supported\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::EtcDecImageConverterTest)
