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
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <sstream>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
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

    void rgbaThreads();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

BasisImageConverterTest::BasisImageConverterTest() {
    addTests({&BasisImageConverterTest::wrongFormat,
              &BasisImageConverterTest::zeroSize,
              &BasisImageConverterTest::emptyData,
              &BasisImageConverterTest::processError,

              &BasisImageConverterTest::r,
              &BasisImageConverterTest::rg,

              &BasisImageConverterTest::rgb,
              &BasisImageConverterTest::rgba,

              &BasisImageConverterTest::rgbaThreads});

    /* Pull in the AnyImageImporter dependency for image comparison, load
       StbImageImporter from the build tree, if defined. Otherwise it's static
       and already loaded. */
    _manager.load("AnyImageImporter");
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    _manager.setPluginDirectory({});
    CORRADE_INTERNAL_ASSERT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef BASISIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_converterManager.load(BASISIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef BASISIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(BASISIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
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

    std::ostringstream out;
    Error redirectError{&out};

    const UnsignedInt dataSize = (27+8)*(63+7)*4;
    Image2D imageWithSkip{PixelStorage{}.setSkip({8, 7, 0}),
        PixelFormat::RGBA8Unorm, {27, 63},
        Containers::Array<char>{Containers::ValueInit, dataSize}};

    CORRADE_VERIFY(!converter->exportToData(imageWithSkip));
    CORRADE_COMPARE(out.str(),
        "Trade::BasisImageConverter::exportToData(): frontend processing failed\n");
}

void BasisImageConverterTest::r() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter =
        _manager.instantiate("PngImporter");
    pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-27x63.png"));
    const auto originalImage = pngImporter->image2D(0);

    /* Use the original image and add a skip of {8, 7} to ensure the converter
       reads the image data properly. Data size is computed with row alignment
       to 4 bytes. During copy, we only use R channel to retrieve a R8 image */
    const UnsignedInt dataSize = ((27+8) + 1)*(63+7);
    Image2D imageWithSkip{PixelStorage{}.setSkip({8, 7, 0}),
        PixelFormat::R8Unorm, originalImage->size(), Containers::Array<char>{Containers::ValueInit, dataSize}};

    auto sourcePixels = Containers::arrayCast<const UnsignedByte>(originalImage->pixels<Color3ub>());
    auto destPixels = imageWithSkip.pixels<UnsignedByte>();
    for(std::size_t y = 0; y != sourcePixels.size()[0]; ++y)
        for(std::size_t x = 0; x != sourcePixels.size()[1]; ++x)
            destPixels[y][x] = sourcePixels[y][x];

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
        (Containers::arrayCast<2, UnsignedByte>(image->pixels().prefix({size_t(image->size()[1]), size_t(image->size()[0]), 1}))),
        imageWithSkip,
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImage{21.0f, 0.740742f}));
}


void BasisImageConverterTest::rg() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter =
        _manager.instantiate("PngImporter");
    pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-27x63.png"));
    const auto originalImage = pngImporter->image2D(0);

    /* Use the original image and add a skip of {8, 7} to ensure the converter
       reads the image data properly. Data size is computed with row alignment
       to 4 bytes. During copy, we only use R and G channels to retrieve a RG8
       image. */
    const UnsignedInt dataSize = ((27+8)*2 + 2)*(63+7);
    Image2D imageWithSkip{PixelStorage{}.setSkip({8, 7, 0}),
        PixelFormat::RG8Unorm, originalImage->size(), Containers::Array<char>{Containers::ValueInit, dataSize}};

    auto sourcePixels = originalImage->pixels<Color3ub>();
    auto destPixels = imageWithSkip.pixels<Math::Vector2<UnsignedByte>>();
    for(std::size_t y = 0; y != sourcePixels.size()[0]; ++y)
        for(std::size_t x = 0; x != sourcePixels.size()[1]; ++x)
            destPixels[y][x] = sourcePixels[y][x].xy();

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
        (Containers::arrayCast<2, Math::Vector2<UnsignedByte>>(image->pixels().suffix({0, 0, 2}))),
        imageWithSkip,
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImage{21.0f, 0.800423f}));
}

void BasisImageConverterTest::rgb() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter =
        _manager.instantiate("PngImporter");
    pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-27x63.png"));
    const auto originalImage = pngImporter->image2D(0);

    /* Use the original image and add a skip of {8, 7} to ensure the converter
       reads the image data properly. Data size is computed with row alignment
       to 4 bytes. */
    const UnsignedInt dataSize = ((27+8)*3 + 3)*(63+7);
    Image2D imageWithSkip{PixelStorage{}.setSkip({8, 7, 0}),
        PixelFormat::RGB8Unorm, originalImage->size(),
        Containers::Array<char>{Containers::ValueInit, dataSize}};

    auto sourcePixels = originalImage->pixels<Color3ub>();
    auto destPixels = imageWithSkip.pixels<Color3ub>();
    for(std::size_t y = 0; y != sourcePixels.size()[0]; ++y)
        for(std::size_t x = 0; x != sourcePixels.size()[1]; ++x)
            destPixels[y][x] = sourcePixels[y][x];

    const auto compressedData = _converterManager.instantiate("BasisImageConverter")->exportToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_COMPARE_WITH(Containers::arrayCast<Color3ub>(image->pixels<Color4ub>()),
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgb-27x63.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 81.0f, 9.46542f}));
}

void BasisImageConverterTest::rgba() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter =
        _manager.instantiate("PngImporter");
    pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-27x63.png"));
    const auto originalImage = pngImporter->image2D(0);

    /* Use the original image and add a skip of {8, 7} to ensure the converter
       reads the image data properly. */
    const UnsignedInt dataSize = (27+8)*(63+7)*4;
    Image2D imageWithSkip{PixelStorage{}.setSkip({8, 7, 0}),
        PixelFormat::RGBA8Unorm, originalImage->size(),
        Containers::Array<char>{Containers::ValueInit, dataSize}};

    auto sourcePixels = originalImage->pixels<Color4ub>();
    auto destPixels = imageWithSkip.pixels<Color4ub>();
    for(std::size_t y = 0; y != sourcePixels.size()[0]; ++y)
        for(std::size_t x = 0; x != sourcePixels.size()[1]; ++x)
            destPixels[y][x] = sourcePixels[y][x];

    const auto data = _converterManager.instantiate("BasisImageConverter")->exportToData(imageWithSkip);
    CORRADE_VERIFY(data);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    /* Basis can only load RGBA8 uncompressed data, which corresponds to RGB1
       from our RGB8 image data. */
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>(),
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-27x63.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 87.75f, 9.955f}));
}

void BasisImageConverterTest::rgbaThreads() {
    /* Same as rgba, except that it's using all available threads instead of
       no threading. Expecting the exact same output (and no crashes, ofc.) */

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter =
        _manager.instantiate("PngImporter");
    pngImporter->openFile(Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-27x63.png"));
    const auto originalImage = pngImporter->image2D(0);

    /* Use the original image and add a skip of {8, 7} to ensure the converter
       reads the image data properly. */
    const UnsignedInt dataSize = (27+8)*(63+7)*4;
    Image2D imageWithSkip{PixelStorage{}.setSkip({8, 7, 0}),
        PixelFormat::RGBA8Unorm, originalImage->size(),
        Containers::Array<char>{Containers::ValueInit, dataSize}};

    auto sourcePixels = originalImage->pixels<Color4ub>();
    auto destPixels = imageWithSkip.pixels<Color4ub>();
    for(std::size_t y = 0; y != sourcePixels.size()[0]; ++y)
        for(std::size_t x = 0; x != sourcePixels.size()[1]; ++x)
            destPixels[y][x] = sourcePixels[y][x];

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    converter->configuration().setValue("threads", "0");
    const auto data = converter->exportToData(imageWithSkip);
    CORRADE_VERIFY(data);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer =
        _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(data));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    /* Basis can only load RGBA8 uncompressed data, which corresponds to RGB1
       from our RGB8 image data. */
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>(),
        Utility::Directory::join(BASISIMPORTER_TEST_DIR, "rgba-27x63.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 87.75f, 9.955f}));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::BasisImageConverterTest)
