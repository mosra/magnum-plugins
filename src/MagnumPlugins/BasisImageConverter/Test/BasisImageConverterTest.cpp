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
#include <Corrade/Containers/StringView.h>
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
    void invalidSwizzle();

    void tooManyLevels();
    void levelWrongSize();

    void processError();

    void r();
    void rg();
    void rgb();
    void rgba();

    void ktx();

    void customLevels();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

constexpr struct {
    const char* name;
    const char* threads;
} ThreadsData[] {
    {"", nullptr},
    {"2 threads", "2"},
    {"all threads", "0"}
};

constexpr struct {
    const char* name;
    const bool yFlip;
} FlippedData[] {
    {"y-flip", true},
    {"no y-flip", false}
};

BasisImageConverterTest::BasisImageConverterTest() {
    addTests({&BasisImageConverterTest::wrongFormat,
              &BasisImageConverterTest::invalidSwizzle,


              &BasisImageConverterTest::tooManyLevels,
              &BasisImageConverterTest::levelWrongSize,

              &BasisImageConverterTest::processError,

              &BasisImageConverterTest::r,
              &BasisImageConverterTest::rg,

              &BasisImageConverterTest::rgb});

    addInstancedTests({&BasisImageConverterTest::rgba},
        Containers::arraySize(ThreadsData));

    addInstancedTests({&BasisImageConverterTest::ktx},
        Containers::arraySize(FlippedData));

    addTests({&BasisImageConverterTest::customLevels});

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

void BasisImageConverterTest::invalidSwizzle() {
    Containers::Pointer<AbstractImageConverter> converter =
        _converterManager.instantiate("BasisImageConverter");

    const char data[8]{};
    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(converter->configuration().setValue("swizzle", "gbgbg"));
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, data}));

    CORRADE_VERIFY(converter->configuration().setValue("swizzle", "xaaa"));
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, data}));

    CORRADE_COMPARE(out.str(),
        "Trade::BasisImageConverter::convertToData(): invalid swizzle length, expected 4 but got 5\n"
        "Trade::BasisImageConverter::convertToData(): invalid characters in swizzle xaaa\n");
}

void BasisImageConverterTest::tooManyLevels() {
    Containers::Pointer<AbstractImageConverter> converter =
        _converterManager.instantiate("BasisImageConverter");

    const UnsignedByte bytes[4]{};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData({
        ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, bytes},
        ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, bytes}
    }));
    CORRADE_COMPARE(out.str(),
        "Trade::BasisImageConverter::convertToData(): there can be only 1 levels with base image size Vector(1, 1) but got 2\n");
}

void BasisImageConverterTest::levelWrongSize() {
    Containers::Pointer<AbstractImageConverter> converter =
        _converterManager.instantiate("BasisImageConverter");

    const UnsignedByte bytes[16]{};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData({
        ImageView2D{PixelFormat::RGB8Unorm, {2, 2}, bytes},
        ImageView2D{PixelFormat::RGB8Unorm, {2, 1}, bytes}
    }));
    CORRADE_COMPARE(out.str(),
        "Trade::BasisImageConverter::convertToData(): expected size Vector(1, 1) for level 1 but got Vector(2, 1)\n");
}

void BasisImageConverterTest::processError() {
    Containers::Pointer<AbstractImageConverter> converter =
        _converterManager.instantiate("BasisImageConverter");
    converter->configuration().setValue("max_endpoint_clusters",
        16128 /* basisu_frontend::cMaxEndpointClusters */ + 1);

    Image2D imageWithSkip{PixelFormat::RGBA8Unorm, Vector2i{16},
        Containers::Array<char>{ValueInit, 16*16*4}};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(imageWithSkip));
    CORRADE_COMPARE(out.str(),
        "Trade::BasisImageConverter::convertToData(): frontend processing failed\n");
}

template<typename SourceType, typename DestinationType = SourceType>
Image2D copyImageWithSkip(const ImageView2D& originalImage, Vector3i skip, PixelFormat format) {
    const Vector2i size = originalImage.size();
    /* Width includes row alignment to 4 bytes */
    const UnsignedInt widthWithSkip = ((size.x() + skip.x())*DestinationType::Size + 3)/4*4;
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

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png")));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly. During copy, we only use R channel to retrieve an
       R8 image. */
    const Image2D imageWithSkip = copyImageWithSkip<Color3ub, Math::Vector<1, UnsignedByte>>(
        *originalImage, {7, 8, 0}, PixelFormat::R8Unorm);

    const auto compressedData = _converterManager.instantiate("BasisImageConverter")->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    /* Basis can only load RGBA8 uncompressed data, which corresponds to RRR1
       from our R8 image data. We chose the red channel from the imported image
       to compare to our original data */
    CORRADE_COMPARE_WITH(
        (Containers::arrayCast<2, const UnsignedByte>(image->pixels().prefix(
            {std::size_t(image->size()[1]), std::size_t(image->size()[0]), 1}))),
        imageWithSkip,
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImage{21.0f, 0.822f}));
}

void BasisImageConverterTest::rg() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png")));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly. During copy, we only use R and G channels to
       retrieve an RG8 image. */
    const Image2D imageWithSkip = copyImageWithSkip<Color3ub, Vector2ub>(
        *originalImage, {7, 8, 0}, PixelFormat::RG8Unorm);

    const auto compressedData = _converterManager.instantiate("BasisImageConverter")->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    /* Basis can only load RGBA8 uncompressed data, which corresponds to RRRG
       from our RG8 image data. We chose the B and A channels from the imported
       image to compare to our original data */
    CORRADE_COMPARE_WITH(
        (Containers::arrayCast<2, const Math::Vector2<UnsignedByte>>(image->pixels().suffix({0, 0, 2}))),
        imageWithSkip,
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImage{22.0f, 0.775f}));
}

void BasisImageConverterTest::rgb() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png")));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly */
    const Image2D imageWithSkip = copyImageWithSkip<Color3ub>(
        *originalImage, {7, 8, 0}, PixelFormat::RGB8Unorm);

    const auto compressedData = _converterManager.instantiate("BasisImageConverter")->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_COMPARE_WITH(Containers::arrayCast<const Color3ub>(image->pixels<Color4ub>()),
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 55.7f, 6.589f}));
}

void BasisImageConverterTest::rgba() {
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
        *originalImage, {7, 8, 0}, PixelFormat::RGBA8Unorm);

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
        (DebugTools::CompareImageToFile{_manager, 78.3f, 8.302f}));
}

void BasisImageConverterTest::ktx() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    auto&& data = FlippedData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png")));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly */
    const Image2D imageWithSkip = copyImageWithSkip<Color4ub>(
        *originalImage, {7, 8, 0}, PixelFormat::RGBA8Unorm);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    converter->configuration().setValue("create_ktx2_file", true);
    converter->configuration().setValue("y_flip", data.yFlip);
    const auto compressedData = converter->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    char KTXorientation[] = "KTXorientation\0r?";
    KTXorientation[sizeof(KTXorientation) - 1] = data.yFlip ? 'u' : 'd';
    CORRADE_VERIFY(Containers::StringView{compressedData, compressedData.size()}.contains(KTXorientation));

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    /* Basis can only load RGBA8 uncompressed data, which corresponds to RGB1
       from our RGB8 image data. */
    auto pixels = image->pixels<Color4ub>();
    if(!data.yFlip) pixels = pixels.flipped<0>();
    CORRADE_COMPARE_WITH(pixels,
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 78.3f, 8.302f}));
}

void BasisImageConverterTest::customLevels() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png")));
    const auto originalLevel0 = pngImporter->image2D(0);
    CORRADE_VERIFY(pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-31x13.png")));
    const auto originalLevel1 = pngImporter->image2D(0);
    CORRADE_VERIFY(pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-15x6.png")));
    const auto originalLevel2 = pngImporter->image2D(0);
    CORRADE_VERIFY(originalLevel0);
    CORRADE_VERIFY(originalLevel1);
    CORRADE_VERIFY(originalLevel2);

    /* Use the original images and add a skip to ensure the converter reads the
       image data properly */
    const Image2D level0WithSkip = copyImageWithSkip<Color4ub>(
        *originalLevel0, {7, 8, 0}, PixelFormat::RGBA8Unorm);
    const Image2D level1WithSkip = copyImageWithSkip<Color4ub>(
        *originalLevel1, {7, 8, 0}, PixelFormat::RGBA8Unorm);
    const Image2D level2WithSkip = copyImageWithSkip<Color4ub>(
        *originalLevel2, {7, 8, 0}, PixelFormat::RGBA8Unorm);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    const auto compressedData = converter->convertToData({level0WithSkip, level1WithSkip, level2WithSkip});
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(compressedData));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 3);

    Containers::Optional<Trade::ImageData2D> level0 = importer->image2D(0, 0);
    Containers::Optional<Trade::ImageData2D> level1 = importer->image2D(0, 1);
    Containers::Optional<Trade::ImageData2D> level2 = importer->image2D(0, 2);
    CORRADE_VERIFY(level0);
    CORRADE_VERIFY(level1);
    CORRADE_VERIFY(level2);

    CORRADE_COMPARE(level0->size(), (Vector2i{63, 27}));
    CORRADE_COMPARE(level1->size(), (Vector2i{31, 13}));
    CORRADE_COMPARE(level2->size(), (Vector2i{15, 6}));

    /* Basis can only load RGBA8 uncompressed data, which corresponds to RGB1
       from our RGB8 image data. */
    CORRADE_COMPARE_WITH(level0->pixels<Color4ub>(),
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 97.25f, 7.927f}));
    CORRADE_COMPARE_WITH(level1->pixels<Color4ub>(),
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-31x13.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 81.0f, 14.322f}));
    CORRADE_COMPARE_WITH(level2->pixels<Color4ub>(),
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-15x6.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 76.25f, 24.5f}));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::BasisImageConverterTest)
