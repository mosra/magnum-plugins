/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <sstream>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Image.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct BasisImageConverterTest: TestSuite::Tester {
    explicit BasisImageConverterTest();

    void wrongFormat();
    void processError();

    void configPerceptual();

    void r();
    void rg();
    void rgb();
    void rgba();

    void threads();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

enum TransferFunction: std::size_t {
    Linear,
    Srgb
};

constexpr PixelFormat TransferFunctionFormats[2][4]{
    {PixelFormat::R8Unorm, PixelFormat::RG8Unorm, PixelFormat::RGB8Unorm, PixelFormat::RGBA8Unorm},
    {PixelFormat::R8Srgb, PixelFormat::RG8Srgb, PixelFormat::RGB8Srgb, PixelFormat::RGBA8Srgb}
};

constexpr struct {
    const char* name;
    const TransferFunction transferFunction;
} FormatTransferFunctionData[] {
    {"Unorm", TransferFunction::Linear},
    {"Srgb", TransferFunction::Srgb}
};

constexpr struct {
    const char* name;
    const char* threads;
} ThreadsData[] {
    {"", nullptr},
    {"2 threads", "2"},
    {"all threads", "0"}
};

BasisImageConverterTest::BasisImageConverterTest() {
    addTests({&BasisImageConverterTest::wrongFormat,
              &BasisImageConverterTest::processError,

              &BasisImageConverterTest::configPerceptual});

    addInstancedTests({&BasisImageConverterTest::r,
                       &BasisImageConverterTest::rg,
                       &BasisImageConverterTest::rgb,
                       &BasisImageConverterTest::rgba},
        Containers::arraySize(FormatTransferFunctionData));

    addInstancedTests({&BasisImageConverterTest::threads},
        Containers::arraySize(ThreadsData));

    /* Pull in the AnyImageImporter dependency for image comparison, load
       StbImageImporter from the build tree, if defined. Otherwise it's static
       and already loaded. */
    _manager.load("AnyImageImporter");
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    _manager.setPluginDirectory({});
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef BASISIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(BASISIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef BASISIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(BASISIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void BasisImageConverterTest::wrongFormat() {
    Containers::Pointer<AbstractImageConverter> converter =
        _converterManager.instantiate("BasisImageConverter");

    const char data[8]{};
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RG32F, {1, 1}, data}));
    CORRADE_COMPARE(out.str(), "Trade::BasisImageConverter::convertToData(): unsupported format PixelFormat::RG32F\n");
}

void BasisImageConverterTest::processError() {
    Containers::Pointer<AbstractImageConverter> converter =
        _converterManager.instantiate("BasisImageConverter");
    converter->configuration().setValue("max_endpoint_clusters",
        16128 /* basisu_frontend::cMaxEndpointClusters */ + 1);

    const char bytes[4]{};
    ImageView2D image{PixelFormat::RGBA8Unorm, Vector2i{1}, bytes};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(image));
    CORRADE_COMPARE(out.str(),
        "Trade::BasisImageConverter::convertToData(): frontend processing failed\n");
}

void BasisImageConverterTest::configPerceptual() {
    const char bytes[4]{};
    ImageView2D originalImage{PixelFormat::RGBA8Unorm, Vector2i{1}, bytes};

    Containers::Pointer<AbstractImageConverter> converter =
        _converterManager.instantiate("BasisImageConverter");
    /* Empty by default */
    CORRADE_COMPARE(converter->configuration().value("perceptual"), "");

    const auto compressedDataAutomatic = converter->convertToData(originalImage);
    CORRADE_VERIFY(compressedDataAutomatic);

    converter->configuration().setValue("perceptual", true);

    const auto compressedDataOverridden = converter->convertToData(originalImage);
    CORRADE_VERIFY(compressedDataOverridden);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");

    /* Empty perceptual config means to use the image format to determine if
       the output data should be sRGB */
    CORRADE_VERIFY(importer->openData(compressedDataAutomatic));
    auto image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);

    /* Perceptual true/false overrides the input format and forces sRGB on/off */
    CORRADE_VERIFY(importer->openData(compressedDataOverridden));
    image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
}

template<typename SourceType, typename DestinationType = SourceType>
Image2D copyImageWithSkip(const ImageView2D& originalImage, Vector3i skip, PixelFormat format) {
    const Vector2i size = originalImage.size();
    /* Width includes row alignment to 4 bytes */
    const UnsignedInt formatSize = pixelSize(format);
    const UnsignedInt widthWithSkip = ((size.x() + skip.x())*DestinationType::Size + 3)/formatSize*formatSize;
    const UnsignedInt dataSize = widthWithSkip*(size.y() + skip.y());
    Image2D imageWithSkip{PixelStorage{}.setSkip(skip), format,
        size, Containers::Array<char>{ValueInit, dataSize}};
    Utility::copy(Containers::arrayCast<const DestinationType>(
        originalImage.pixels<SourceType>()),
        imageWithSkip.pixels<DestinationType>());
    return imageWithSkip;
}

void BasisImageConverterTest::r() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    auto&& data = FormatTransferFunctionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png")));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly. During copy, we only use R channel to retrieve an
       R8 image. */
    const Image2D imageWithSkip = copyImageWithSkip<Color3ub, Math::Vector<1, UnsignedByte>>(
        *originalImage, {7, 8, 0}, TransferFunctionFormats[data.transferFunction][0]);

    const auto compressedData = _converterManager.instantiate("BasisImageConverter")->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), TransferFunctionFormats[data.transferFunction][3]);

    /* CompareImage doesn't support Srgb formats, so we need to create a view
       on the original image, but with a Unorm format */
    const ImageView2D imageViewUnorm{imageWithSkip.storage(),
        TransferFunctionFormats[TransferFunction::Linear][0], imageWithSkip.size(), imageWithSkip.data()};
    /* Basis can only load RGBA8 uncompressed data, which corresponds to RRR1
       from our R8 image data. We chose the red channel from the imported image
       to compare to our original data */
    CORRADE_COMPARE_WITH(
        (Containers::arrayCast<2, const UnsignedByte>(image->pixels().prefix(
            {std::size_t(image->size()[1]), std::size_t(image->size()[0]), 1}))),
        imageViewUnorm,
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImage{21.0f, 0.968f}));
}

void BasisImageConverterTest::rg() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    auto&& data = FormatTransferFunctionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png")));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly. During copy, we only use R and G channels to
       retrieve an RG8 image. */
    const Image2D imageWithSkip = copyImageWithSkip<Color3ub, Vector2ub>(
        *originalImage, {7, 8, 0}, TransferFunctionFormats[data.transferFunction][1]);

    const auto compressedData = _converterManager.instantiate("BasisImageConverter")->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), TransferFunctionFormats[data.transferFunction][3]);

    /* CompareImage doesn't support Srgb formats, so we need to create a view
       on the original image, but with a Unorm format */
    const ImageView2D imageViewUnorm{imageWithSkip.storage(),
        TransferFunctionFormats[TransferFunction::Linear][1], imageWithSkip.size(), imageWithSkip.data()};
    /* Basis can only load RGBA8 uncompressed data, which corresponds to RRRG
       from our RG8 image data. We chose the B and A channels from the imported
       image to compare to our original data */
    CORRADE_COMPARE_WITH(
        (Containers::arrayCast<2, const Math::Vector2<UnsignedByte>>(image->pixels().suffix({0, 0, 2}))),
        imageViewUnorm,
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImage{22.0f, 1.039f}));
}

void BasisImageConverterTest::rgb() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    auto&& data = FormatTransferFunctionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png")));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly */
    const Image2D imageWithSkip = copyImageWithSkip<Color3ub>(
        *originalImage, {7, 8, 0}, TransferFunctionFormats[data.transferFunction][2]);

    const auto compressedData = _converterManager.instantiate("BasisImageConverter")->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), TransferFunctionFormats[data.transferFunction][3]);

    CORRADE_COMPARE_WITH(Containers::arrayCast<const Color3ub>(image->pixels<Color4ub>()),
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 61.0f, 6.622f}));
}

void BasisImageConverterTest::rgba() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    auto&& data = FormatTransferFunctionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png")));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly */
    const Image2D imageWithSkip = copyImageWithSkip<Color4ub>(
        *originalImage, {7, 8, 0}, TransferFunctionFormats[data.transferFunction][3]);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    const auto compressedData = converter->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), TransferFunctionFormats[data.transferFunction][3]);

    /* Basis can only load RGBA8 uncompressed data, which corresponds to RGB1
       from our RGB8 image data. */
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>(),
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 94.0f, 8.122f}));
}

void BasisImageConverterTest::threads() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    auto&& data = ThreadsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png")));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly */
    const Image2D imageWithSkip = copyImageWithSkip<Color4ub>(
        *originalImage, {7, 8, 0}, PixelFormat::RGBA8Srgb);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    if(data.threads) converter->configuration().setValue("threads", data.threads);
    const auto compressedData = converter->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    /* Basis can only load RGBA8 uncompressed data, which corresponds to RGB1
       from our RGB8 image data. */
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>(),
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 94.0f, 8.039f}));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::BasisImageConverterTest)
