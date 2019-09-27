/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019 Jonathan Hale <squareys@googlemail.com>

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
    FROM, OUT OF OR IN CONNETCION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <sstream>
#include <Corrade/Containers/Optional.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct BasisImporterTest: TestSuite::Tester {
    explicit BasisImporterTest();

    void empty();
    void invalid();
    void unconfigured();
    void invalidConfiguredFormat();
    void fileTooShort();
    void transcodingFailure();

    void openTwice();

    void rgb();
    void rgba();
    void importMultipleFormats();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

constexpr struct {
    const char* file;
    const char* fileAlpha;
    const char* suffix;
    const CompressedPixelFormat expectedFormat;
    const Vector2i expectedSize;
} FormatData[] {
    {
        "rgb_2_images.basis",
        "rgba_2_images.basis",
        "Etc1",
        CompressedPixelFormat::Etc2RGB8Unorm,
        {63, 27}
    }, {
        "rgb_2_images.basis",
        "rgba_2_images.basis",
        "Etc2",
        CompressedPixelFormat::Etc2RGBA8Unorm,
        {63, 27}
    }, {
        "rgb_2_images.basis",
        "rgba_2_images.basis",
        "Bc1",
        CompressedPixelFormat::Bc1RGBUnorm,
        {63, 27}
    }, {
        "rgb_2_images.basis",
        "rgba_2_images.basis",
        "Bc3",
        CompressedPixelFormat::Bc3RGBAUnorm,
        {63, 27}
    }, {
        "rgb_2_images.basis",
        "rgba_2_images.basis",
        "Bc4",
        CompressedPixelFormat::Bc4RUnorm,
        {63, 27}
    }, {
        "rgb_2_images.basis",
        "rgba_2_images.basis",
        "Bc5",
        CompressedPixelFormat::Bc5RGUnorm,
        {63, 27}
    }, {
        "rgb_2_images.basis",
        "rgba_2_images.basis",
        "Bc7M6OpaqueOnly",
        CompressedPixelFormat::Bc7RGBAUnorm,
        {63, 27}
    }, {
        "rgb_2_images_pow2.basis",
        "rgba_2_images_pow2.basis",
        "Pvrtc1_4OpaqueOnly",
        CompressedPixelFormat::PvrtcRGB4bppUnorm,
        {64, 32}
    }
};

BasisImporterTest::BasisImporterTest() {
    addTests({&BasisImporterTest::empty,
              &BasisImporterTest::invalid,
              &BasisImporterTest::unconfigured,
              &BasisImporterTest::invalidConfiguredFormat,
              &BasisImporterTest::fileTooShort,
              &BasisImporterTest::transcodingFailure,

              &BasisImporterTest::openTwice,
              &BasisImporterTest::importMultipleFormats});

    addInstancedTests({&BasisImporterTest::rgb,
                       &BasisImporterTest::rgba},
                       Containers::arraySize(FormatData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef BASISIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(BASISIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void BasisImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");

    std::ostringstream out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::openData(): the file is empty\n");
}

void BasisImporterTest::invalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData("NotABasisFile"));

    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::openData(): invalid header\n");
}

void BasisImporterTest::unconfigured() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(importer->openFile(
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));
    CORRADE_VERIFY(!importer->image2D(0));

    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): no format to transcode to was specified. Either load the plugin via one of its BasisImporterEtc1, ... aliases, or set the format explicitly via plugin configuration.\n");
}

void BasisImporterTest::invalidConfiguredFormat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    CORRADE_VERIFY(importer->openFile(
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));

    std::ostringstream out;
    Error redirectError{&out};
    importer->configuration().setValue("format", "Banana");
    CORRADE_VERIFY(!importer->image2D(0));

    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): invalid transcoding target format Banana, expected to be one of Etc1, Etc2, Bc1, Bc3, Bc4, Bc5, Bc7M6OpaqueOnly, Pvrtc1_4OpaqueOnly\n");
}

void BasisImporterTest::fileTooShort() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    auto basisData = Utility::Directory::read(
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb.basis"));

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openData(basisData.prefix(64)));

    /* Corrupt the header */
    basisData[100] = 100;
    CORRADE_VERIFY(!importer->openData(basisData));

    CORRADE_COMPARE(out.str(),
        "Trade::BasisImporter::openData(): invalid header\n"
        "Trade::BasisImporter::openData(): bad basis file\n");
}


void BasisImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    CORRADE_VERIFY(importer->openFile(
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));
    CORRADE_VERIFY(importer->openFile(
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));

    /* Shouldn't crash, leak or anything */
}

void BasisImporterTest::transcodingFailure() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));

    std::ostringstream out;
    Error redirectError{&out};

    /* PVRTC1 requires power of 2 image dimensions, but rgb.basis is 27x63, hence
       basis will fail during transcoding. */
    importer->configuration().setValue("format", "Pvrtc1_4OpaqueOnly");
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(!image);
    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): transcoding failed\n");
}

void BasisImporterTest::rgb() {
    auto& formatData = FormatData[testCaseInstanceId()];
    const std::string pluginName = "BasisImporter" + std::string(formatData.suffix);
    setTestCaseDescription(formatData.suffix);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate(pluginName);
    CORRADE_VERIFY(importer);
    CORRADE_COMPARE(importer->configuration().value<std::string>("format"),
        formatData.suffix);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR,
        formatData.file)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->compressedFormat(), formatData.expectedFormat);
    CORRADE_COMPARE(image->size(), formatData.expectedSize);

    /* Verify that the 90° rotated second image can be loaded also */
    image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->compressedFormat(), formatData.expectedFormat);
    CORRADE_COMPARE(image->size(), formatData.expectedSize.flipped());
}

void BasisImporterTest::rgba() {
    auto& formatData = FormatData[testCaseInstanceId()];
    const std::string pluginName = "BasisImporter" + std::string(formatData.suffix);
    setTestCaseDescription(formatData.suffix);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate(pluginName);
    CORRADE_VERIFY(importer);
    CORRADE_COMPARE(importer->configuration().value<std::string>("format"),
        formatData.suffix);
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR,
        formatData.fileAlpha)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->compressedFormat(), formatData.expectedFormat);
    CORRADE_COMPARE(image->size(), formatData.expectedSize);

    /* Verify that the 90° rotated second image can be loaded also */
    image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->compressedFormat(), formatData.expectedFormat);
    CORRADE_COMPARE(image->size(), formatData.expectedSize.flipped());
}

void BasisImporterTest::importMultipleFormats() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));

    /* Verify that everything is working the same way on second use */
    {
        importer->configuration().setValue("format", "Etc2");

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGBA8Unorm);
        CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
    } {
        importer->configuration().setValue("format", "Bc1");

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc1RGBUnorm);
        CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::BasisImporterTest)
