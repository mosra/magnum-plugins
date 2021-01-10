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
    void zeroSize();
    void emptyData();
    void processError();

    void r();
    void rg();
    void rgb();
    void rgba();

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

BasisImageConverterTest::BasisImageConverterTest() {
    addTests({&BasisImageConverterTest::wrongFormat,
              &BasisImageConverterTest::zeroSize,
              &BasisImageConverterTest::emptyData,
              &BasisImageConverterTest::processError,

              &BasisImageConverterTest::r,
              &BasisImageConverterTest::rg,

              &BasisImageConverterTest::rgb});

    addInstancedTests({&BasisImageConverterTest::rgba},
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
    ImageView2D image{PixelFormat::RG32F, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->exportToData(image));
    CORRADE_COMPARE(out.str(), "Trade::BasisImageConverter::exportToData(): unsupported format PixelFormat::RG32F\n");
}

void BasisImageConverterTest::zeroSize() {
    Containers::Pointer<AbstractImageConverter> converter =
        _converterManager.instantiate("BasisImageConverter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->exportToData(ImageView2D{PixelFormat::RGB8Unorm, {}, nullptr}));
    CORRADE_COMPARE(out.str(),
        "Trade::BasisImageConverter::exportToData(): source image is empty\n");
}

void BasisImageConverterTest::emptyData() {
    Containers::Pointer<AbstractImageConverter> converter =
        _converterManager.instantiate("BasisImageConverter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->exportToData(ImageView2D{PixelFormat::RGB8Unorm, {9192, 8192}}));
    CORRADE_COMPARE(out.str(),
        "Trade::BasisImageConverter::exportToData(): source image data is nullptr\n");
}

void BasisImageConverterTest::processError() {
    Containers::Pointer<AbstractImageConverter> converter =
        _converterManager.instantiate("BasisImageConverter");
    converter->configuration().setValue("max_endpoint_clusters",
        16128 /* basisu_frontend::cMaxEndpointClusters */ + 1);

    Image2D imageWithSkip{PixelFormat::RGBA8Unorm, Vector2i{16},
        Containers::Array<char>{Containers::ValueInit, 16*16*4}};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->exportToData(imageWithSkip));
    CORRADE_COMPARE(out.str(),
        "Trade::BasisImageConverter::exportToData(): frontend processing failed\n");
}

void BasisImageConverterTest::r() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter =
        _manager.instantiate("PngImporter");
    pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip of {7, 8} to ensure the converter
       reads the image data properly. Data size is computed with row alignment
       to 4 bytes. During copy, we only use R channel to retrieve a R8 image */
    const UnsignedInt dataSize = (63 + 7 + 2)*(27 + 8);
    Image2D imageWithSkip{PixelStorage{}.setSkip({7, 8, 0}),
        PixelFormat::R8Unorm, originalImage->size(), Containers::Array<char>{Containers::ValueInit, dataSize}};
    Utility::copy(Containers::arrayCast<const UnsignedByte>(
        originalImage->pixels<Color3ub>()),
        imageWithSkip.pixels<UnsignedByte>());

    const auto compressedData = _converterManager.instantiate("BasisImageConverter")->exportToData(imageWithSkip);
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

    Containers::Pointer<AbstractImporter> pngImporter =
        _manager.instantiate("PngImporter");
    pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip of {7, 8} to ensure the converter
       reads the image data properly. Data size is computed with row alignment
       to 4 bytes. During copy, we only use R and G channels to retrieve a RG8
       image. */
    const UnsignedInt dataSize = ((63 + 8)*2 + 2)*(27 + 7);
    Image2D imageWithSkip{PixelStorage{}.setSkip({7, 8, 0}),
        PixelFormat::RG8Unorm, originalImage->size(), Containers::Array<char>{Containers::ValueInit, dataSize}};
    Utility::copy(Containers::arrayCast<const Vector2ub>(
        originalImage->pixels<Color3ub>()),
        imageWithSkip.pixels<Vector2ub>());

    const auto compressedData = _converterManager.instantiate("BasisImageConverter")->exportToData(imageWithSkip);
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

    Containers::Pointer<AbstractImporter> pngImporter =
        _manager.instantiate("PngImporter");
    pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip of {7, 8} to ensure the converter
       reads the image data properly. Data size is computed with row alignment
       to 4 bytes. */
    const UnsignedInt dataSize = ((63 + 7)*3 + 3)*(27 + 8);
    Image2D imageWithSkip{PixelStorage{}.setSkip({7, 8, 0}),
        PixelFormat::RGB8Unorm, originalImage->size(),
        Containers::Array<char>{Containers::ValueInit, dataSize}};
    Utility::copy(originalImage->pixels<Color3ub>(),
        imageWithSkip.pixels<Color3ub>());

    const auto compressedData = _converterManager.instantiate("BasisImageConverter")->exportToData(imageWithSkip);
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

    Containers::Pointer<AbstractImporter> pngImporter =
        _manager.instantiate("PngImporter");
    pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"));
    const auto originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip of {7, 8} to ensure the converter
       reads the image data properly. */
    const UnsignedInt dataSize = ((63 + 7)*4)*(27 + 7);
    Image2D imageWithSkip{PixelStorage{}.setSkip({7, 8, 0}),
        PixelFormat::RGBA8Unorm, originalImage->size(),
        Containers::Array<char>{Containers::ValueInit, dataSize}};
    Utility::copy(originalImage->pixels<Color4ub>(),
        imageWithSkip.pixels<Color4ub>());

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    if(data.threads) converter->configuration().setValue("threads", data.threads);
    const auto compressedData = converter->exportToData(imageWithSkip);
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

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::BasisImageConverterTest)
