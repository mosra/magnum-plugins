/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Math/ConfigurationValue.h>
#include <Magnum/Math/Half.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include <OpenEXR/OpenEXRConfig.h> /* for version-dependent checks */

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct OpenExrImageConverterTest: TestSuite::Tester {
    explicit OpenExrImageConverterTest();

    void wrongFormat();
    void zeroSizeImage();

    void rgb16f();
    void rgba32f();
    void rg32ui();
    void depth32f();

    void customChannels();
    void customChannelsDuplicated();
    void customChannelsSomeUnassigned();
    void customChannelsAllUnassigned();
    void customChannelsDepth();
    void customChannelsDepthUnassigned();

    void customWindows();

    void compression();
    void compressionInvalid();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _manager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

using namespace Math::Literals;

const Half Rgb16fData[] = {
    /* Skip */
    {}, {}, {}, {},

    0.0_h, 1.0_h, 2.0_h, {},
    3.0_h, 4.0_h, 5.0_h, {},
    6.0_h, 7.0_h, 8.0_h, {}
};

const ImageView2D Rgb16f{PixelStorage{}.setSkip({0, 1, 0}),
    PixelFormat::RGB16F, {1, 3}, Rgb16fData};

const Float Rgba32fData[] = {
    0.0f, 1.0f, 2.0f, 3.0f,
    4.0f, 5.0f, 6.0f, 7.0f,
    8.0f, 9.0f, 10.0f, 11.0f
};

const ImageView2D Rgba32f{PixelFormat::RGBA32F, {1, 3}, Rgba32fData};

const UnsignedInt Rg32uiData[] = {
    0x1111, 0x2222, 0x3333, 0x4444,
    0x5555, 0x6666, 0x7777, 0x8888
};

const ImageView2D Rg32ui{PixelFormat::RG32UI, {2, 2}, Rg32uiData};

const Float Depth32fData[] = {
    0.125f, 0.250f, 0.375f,
    0.500f, 0.625f, 0.750f
};

const ImageView2D Depth32f{PixelFormat::Depth32F, {3, 2}, Depth32fData};

const struct {
    const char* compression;
    std::size_t size;
} CompressionData[] {
    /* Just the lossless ones, I don't feel the need to bother with fuzzy
       image comparison */
    {"", 427},
    {"rle", 427},
    {"zip", 391},
    {"zips", 427},
    {"piz", 395}
};

OpenExrImageConverterTest::OpenExrImageConverterTest() {
    addTests({&OpenExrImageConverterTest::wrongFormat,
              &OpenExrImageConverterTest::zeroSizeImage,

              &OpenExrImageConverterTest::rgb16f,
              &OpenExrImageConverterTest::rgba32f,
              &OpenExrImageConverterTest::rg32ui,
              &OpenExrImageConverterTest::depth32f,

              &OpenExrImageConverterTest::customChannels,
              &OpenExrImageConverterTest::customChannelsDuplicated,
              &OpenExrImageConverterTest::customChannelsSomeUnassigned,
              &OpenExrImageConverterTest::customChannelsAllUnassigned,
              &OpenExrImageConverterTest::customChannelsDepth,
              &OpenExrImageConverterTest::customChannelsDepthUnassigned,

              &OpenExrImageConverterTest::customWindows});

    addInstancedTests({&OpenExrImageConverterTest::compression},
        Containers::arraySize(CompressionData));

    addTests({&OpenExrImageConverterTest::compressionInvalid});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef OPENEXRIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(OPENEXRIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* The OpenExrImporter is optional */
    #ifdef OPENEXRIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(OPENEXRIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void OpenExrImageConverterTest::wrongFormat() {
    ImageView2D image{PixelFormat::RGBA8Unorm, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!_manager.instantiate("OpenExrImageConverter")->convertToData(image));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImageConverter::convertToData(): unsupported format PixelFormat::RGBA8Unorm, only *16F, *32F, *32UI and Depth32F formats supported\n");
}

void OpenExrImageConverterTest::zeroSizeImage() {
    ImageView2D image{PixelFormat::RGBA16F, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!_manager.instantiate("OpenExrImageConverter")->convertToData(image));

    /* Originally, before custom data windows got introduced in the converter,
       in OpenEXR 2.5.3 and newer the message was just
        conversion error: Invalid display window in image header.
       Now it's the same in both older and newer versions. */
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImageConverter::convertToData(): conversion error: Cannot open image file \"\". Invalid display window in image header.\n");
}

void OpenExrImageConverterTest::rgb16f() {
    const auto data = _manager.instantiate("OpenExrImageConverter")->convertToData(Rgb16f);

    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgb16f.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(data));

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Rgb16f, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::rgba32f() {
    const auto data = _manager.instantiate("OpenExrImageConverter")->convertToData(Rgba32f);

    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(data));

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Rgba32f, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::rg32ui() {
    const auto data = _manager.instantiate("OpenExrImageConverter")->convertToData(Rg32ui);

    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rg32ui.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(data));

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Rg32ui, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::depth32f() {
    const auto data = _manager.instantiate("OpenExrImageConverter")->convertToData(Depth32f);

    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "depth32f.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(data));

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Depth32f, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::customChannels() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("layer", "tangent");
    converter->configuration().setValue("r", "X");
    converter->configuration().setValue("g", "Y");
    converter->configuration().setValue("b", "Z");
    converter->configuration().setValue("a", "handedness");
    const auto data = converter->convertToData(Rgba32f);

    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f-custom-channels.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    importer->configuration().setValue("layer", "tangent");
    importer->configuration().setValue("r", "X");
    importer->configuration().setValue("g", "Y");
    importer->configuration().setValue("b", "Z");
    importer->configuration().setValue("a", "handedness");
    CORRADE_VERIFY(importer->openData(data));

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Rgba32f, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::customChannelsDuplicated() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("a", "G");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(Rgba32f));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImageConverter::convertToData(): duplicate mapping for channel G\n");
}

void OpenExrImageConverterTest::customChannelsSomeUnassigned() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("layer", "normal");
    converter->configuration().setValue("r", "X");
    converter->configuration().setValue("g", "");
    converter->configuration().setValue("b", "Z");
    converter->configuration().setValue("a", "");
    const auto data = converter->convertToData(Rgba32f);

    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(OPENEXRIMAGECONVERTER_TEST_DIR, "rb32f-custom-channels.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    importer->configuration().setValue("layer", "normal");
    importer->configuration().setValue("r", "X");
    importer->configuration().setValue("g", "Z");
    /* B, A stays at default, but shouldn't get filled */
    CORRADE_VERIFY(importer->openData(data));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, (ImageView2D{PixelFormat::RG32F, {1, 3}, Containers::arrayView<Float>({
        0.0f, 2.0f,
        4.0f, 6.0f,
        8.0f, 10.0f,
    })}), DebugTools::CompareImage);
}

void OpenExrImageConverterTest::customChannelsAllUnassigned() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("layer", "normal");
    converter->configuration().setValue("r", "");
    converter->configuration().setValue("g", "");
    converter->configuration().setValue("b", "");
    converter->configuration().setValue("a", "");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(Rgba32f));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImageConverter::convertToData(): no channels assigned in plugin configuration\n");
}

void OpenExrImageConverterTest::customChannelsDepth() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("layer", "left");
    converter->configuration().setValue("depth", "height");
    const auto data = converter->convertToData(Depth32f);

    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "depth32f-custom-channels.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    importer->configuration().setValue("layer", "left");
    importer->configuration().setValue("depth", "height");
    CORRADE_VERIFY(importer->openData(data));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Depth32f, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::customChannelsDepthUnassigned() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("layer", "normal");
    converter->configuration().setValue("depth", "");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(Depth32f));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImageConverter::convertToData(): no channels assigned in plugin configuration\n");
}

void OpenExrImageConverterTest::customWindows() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("displayWindow", Vector4i{38, 56, 47, 72});
    converter->configuration().setValue("dataOffset", Vector2i{375, 226});

    const auto data = converter->convertToData(Rgb16f);

    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgb16f-custom-windows.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(data));

    /* No matter how crazy the windows are, the imported data should be the
       same */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Rgb16f, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::compression() {
    auto&& data = CompressionData[testCaseInstanceId()];
    setTestCaseDescription(data.compression);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("compression", data.compression);

    const auto out = converter->convertToData(Rgba32f);
    CORRADE_VERIFY(out);

    /* The sizes should slightly differ at the very least -- this checks that
       the setting isn't just plainly ignored */
    CORRADE_COMPARE(out.size(), data.size);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(out));

    /* Using only lossless compression here, so the data should match */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Rgba32f, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::compressionInvalid() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("compression", "zstd");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(Rgba32f));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImageConverter::convertToData(): unknown compression zstd, allowed values are rle, zip, zips, piz, pxr24, b44, b44a, dwaa, dwab or empty for uncompressed output\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::OpenExrImageConverterTest)
