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

/* OpenEXR as a CMake subproject adds the OpenEXR/ directory to include path
   but not the parent directory, so we can't #include <OpenEXR/blah>. This
   can't really be fixed from outside, so unfortunately we have to do the same
   in case of an external OpenEXR. */
#include <OpenEXRConfig.h> /* for version-dependent checks */

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct OpenExrImageConverterTest: TestSuite::Tester {
    explicit OpenExrImageConverterTest();

    void wrongFormat();
    void conversionError();

    void rgb16f();
    void rgba32f();
    void rg32ui();
    void depth32f();

    void envmap2DLatLong();
    void envmap2DLatLongWrongSize();
    void envmap2DInvalid();

    void envmap3DCubeMap();
    void envmap3DCubeMapWrongSize();
    void envmap3DInvalid();
    void arbitrary3D();

    void customChannels();
    void customChannelsDuplicated();
    void customChannelsSomeUnassigned();
    void customChannelsAllUnassigned();
    void customChannelsDepth();
    void customChannelsDepthUnassigned();

    void customWindows();
    void customWindowsCubeMap();

    void compression();
    void compressionCubeMap();
    void compressionInvalid();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _manager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

using namespace Containers::Literals;
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

const Half CubeRg16fData[] = {
    /* Skip */
    {}, {}, {}, {},
    {}, {}, {}, {},
    {}, {}, {}, {},

    00.0_h, 01.0_h, 02.0_h, 03.0_h,
    04.0_h, 05.0_h, 06.0_h, 07.0_h,
    {}, {}, {}, {}, /* Image height */

    10.0_h, 11.0_h, 12.0_h, 13.0_h,
    14.0_h, 15.0_h, 16.0_h, 17.0_h,
    {}, {}, {}, {},

    20.0_h, 21.0_h, 22.0_h, 23.0_h,
    24.0_h, 25.0_h, 26.0_h, 27.0_h,
    {}, {}, {}, {},

    30.0_h, 31.0_h, 32.0_h, 33.0_h,
    34.0_h, 35.0_h, 36.0_h, 37.0_h,
    {}, {}, {}, {},

    40.0_h, 41.0_h, 42.0_h, 43.0_h,
    44.0_h, 45.0_h, 46.0_h, 47.0_h,
    {}, {}, {}, {},

    50.0_h, 51.0_h, 52.0_h, 53.0_h,
    54.0_h, 55.0_h, 56.0_h, 57.0_h,
    {}, {}, {}, {}
};

const ImageView3D CubeRg16f{PixelStorage{}.setSkip({0, 0, 1}).setImageHeight(3),
    PixelFormat::RG16F, {2, 2, 6}, CubeRg16fData};

const struct {
    const char* compression;
    std::size_t size;
    std::size_t cubeSize;
} CompressionData[] {
    /* Just the lossless ones, I don't feel the need to bother with fuzzy
       image comparison */
    {"", 427, 602},
    {"rle", 427, 602},
    {"zip", 391, 402},
    {"zips", 427, 602},
    {"piz", 395, 426}
};

OpenExrImageConverterTest::OpenExrImageConverterTest() {
    addTests({&OpenExrImageConverterTest::wrongFormat,
              &OpenExrImageConverterTest::conversionError,

              &OpenExrImageConverterTest::rgb16f,
              &OpenExrImageConverterTest::rgba32f,
              &OpenExrImageConverterTest::rg32ui,
              &OpenExrImageConverterTest::depth32f,

              &OpenExrImageConverterTest::envmap2DLatLong,
              &OpenExrImageConverterTest::envmap2DLatLongWrongSize,
              &OpenExrImageConverterTest::envmap2DInvalid,

              &OpenExrImageConverterTest::envmap3DCubeMap,
              &OpenExrImageConverterTest::envmap3DCubeMapWrongSize,
              &OpenExrImageConverterTest::envmap3DInvalid,
              &OpenExrImageConverterTest::arbitrary3D,

              &OpenExrImageConverterTest::customChannels,
              &OpenExrImageConverterTest::customChannelsDuplicated,
              &OpenExrImageConverterTest::customChannelsSomeUnassigned,
              &OpenExrImageConverterTest::customChannelsAllUnassigned,
              &OpenExrImageConverterTest::customChannelsDepth,
              &OpenExrImageConverterTest::customChannelsDepthUnassigned,

              &OpenExrImageConverterTest::customWindows,
              &OpenExrImageConverterTest::customWindowsCubeMap});

    addInstancedTests({&OpenExrImageConverterTest::compression,
                       &OpenExrImageConverterTest::compressionCubeMap},
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
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");

    const char data[4]{};
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, data}));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImageConverter::convertToData(): unsupported format PixelFormat::RGBA8Unorm, only *16F, *32F, *32UI and Depth32F formats supported\n");
}

void OpenExrImageConverterTest::conversionError() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");

    /* Because zero-size images are disallowed by the base implementation
       already, we can't abuse that for checking conversion errors. Instead we
       set the display window size to a negative value. */
    converter->configuration().setValue("displayWindow", Vector4i{1, 1, 0, 0});

    const char data[8]{};
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGBA16F, {1, 1}, data}));

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

void OpenExrImageConverterTest::envmap2DLatLong() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "latlong");

    /* The width needs to be 2*height, abuse existing data for that */
    const ImageView2D R32ui{PixelFormat::R32UI, {4, 2}, Rg32uiData};

    const auto data = converter->convertToData(R32ui);
    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(OPENEXRIMAGECONVERTER_TEST_DIR, "envmap-latlong.exr"),
        TestSuite::Compare::StringToFile);

    /* The metadata has no effect on the actual saved data, so no point in
       importing. Verifying the metadata has to be done using the `exrheader`
       tool, the importer has no API for that. This is only a basic check that
       the metadata got added. */
    CORRADE_VERIFY(Containers::StringView{arrayView(data)}.contains("envmap\0"_s));
}

void OpenExrImageConverterTest::envmap2DLatLongWrongSize() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "latlong");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(Rg32ui));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImageConverter::convertToData(): a lat/long environment map has to have a 2:1 aspect ratio, got Vector(2, 2)\n");
}

void OpenExrImageConverterTest::envmap2DInvalid() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "cubemap");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(Rg32ui));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImageConverter::convertToData(): unknown envmap option cubemap for a 2D image, expected either empty or latlong for 2D images and cube for 3D images\n");
}

void OpenExrImageConverterTest::envmap3DCubeMap() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "cube");

    const auto data = converter->convertToData(CubeRg16f);

    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "envmap-cube.exr"),
        TestSuite::Compare::StringToFile);

    /* The metadata has no effect on the actual saved data, so no point in
       importing. Verifying the metadata has to be done using the `exrheader`
       tool, the importer has no API for that. This is only a basic check that
       the metadata got added. */
    CORRADE_VERIFY(Containers::StringView{arrayView(data)}.contains("envmap\0"_s));

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(data));
    CORRADE_COMPARE(importer->image3DCount(), 1);

    /** @todo use CompareImage once it can handle 3D images */
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), (Vector3i{2, 2, 6}));
    CORRADE_COMPARE(image->format(), PixelFormat::RG16F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView({
        00.0_h, 01.0_h, 02.0_h, 03.0_h,
        04.0_h, 05.0_h, 06.0_h, 07.0_h,

        10.0_h, 11.0_h, 12.0_h, 13.0_h,
        14.0_h, 15.0_h, 16.0_h, 17.0_h,

        20.0_h, 21.0_h, 22.0_h, 23.0_h,
        24.0_h, 25.0_h, 26.0_h, 27.0_h,

        30.0_h, 31.0_h, 32.0_h, 33.0_h,
        34.0_h, 35.0_h, 36.0_h, 37.0_h,

        40.0_h, 41.0_h, 42.0_h, 43.0_h,
        44.0_h, 45.0_h, 46.0_h, 47.0_h,

        50.0_h, 51.0_h, 52.0_h, 53.0_h,
        54.0_h, 55.0_h, 56.0_h, 57.0_h,
    }), TestSuite::Compare::Container);
}

void OpenExrImageConverterTest::envmap3DCubeMapWrongSize() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "cube");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView3D{PixelFormat::R32UI, {2, 2, 5}, CubeRg16fData}));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImageConverter::convertToData(): a cubemap has to have six square slices, got Vector(2, 2, 5)\n");
}

void OpenExrImageConverterTest::envmap3DInvalid() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "latlong");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(CubeRg16f));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImageConverter::convertToData(): unknown envmap option latlong for a 3D image, expected either empty or latlong for 2D images and cube for 3D images\n");
}

void OpenExrImageConverterTest::arbitrary3D() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(CubeRg16f));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImageConverter::convertToData(): arbitrary 3D image saving not implemented yet, the envmap option has to be set to cube in the configuration in order to save a cube map\n");
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
       same. Verifying the metadata has to be done using the `exrheader` tool,
       the importer has no API for that. */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Rgb16f, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::customWindowsCubeMap() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "cube");
    converter->configuration().setValue("displayWindow", Vector4i{38, 56, 47, 72});
    converter->configuration().setValue("dataOffset", Vector2i{375, 226});

    const auto data = converter->convertToData(CubeRg16f);

    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "envmap-cube-custom-windows.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(data));
    CORRADE_COMPARE(importer->image3DCount(), 1);

    /** @todo use CompareImage once it can handle 3D images */
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), (Vector3i{2, 2, 6}));
    CORRADE_COMPARE(image->format(), PixelFormat::RG16F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView({
        00.0_h, 01.0_h, 02.0_h, 03.0_h,
        04.0_h, 05.0_h, 06.0_h, 07.0_h,

        10.0_h, 11.0_h, 12.0_h, 13.0_h,
        14.0_h, 15.0_h, 16.0_h, 17.0_h,

        20.0_h, 21.0_h, 22.0_h, 23.0_h,
        24.0_h, 25.0_h, 26.0_h, 27.0_h,

        30.0_h, 31.0_h, 32.0_h, 33.0_h,
        34.0_h, 35.0_h, 36.0_h, 37.0_h,

        40.0_h, 41.0_h, 42.0_h, 43.0_h,
        44.0_h, 45.0_h, 46.0_h, 47.0_h,

        50.0_h, 51.0_h, 52.0_h, 53.0_h,
        54.0_h, 55.0_h, 56.0_h, 57.0_h,
    }), TestSuite::Compare::Container);
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

void OpenExrImageConverterTest::compressionCubeMap() {
    auto&& data = CompressionData[testCaseInstanceId()];
    setTestCaseDescription(data.compression);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "cube");
    converter->configuration().setValue("compression", data.compression);

    const auto out = converter->convertToData(CubeRg16f);
    CORRADE_VERIFY(out);

    /* The sizes should slightly differ at the very least -- this checks that
       the setting isn't just plainly ignored */
    CORRADE_COMPARE(out.size(), data.cubeSize);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(out));

    /** @todo use CompareImage once it can handle 3D images */
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), (Vector3i{2, 2, 6}));
    CORRADE_COMPARE(image->format(), PixelFormat::RG16F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView({
        00.0_h, 01.0_h, 02.0_h, 03.0_h,
        04.0_h, 05.0_h, 06.0_h, 07.0_h,

        10.0_h, 11.0_h, 12.0_h, 13.0_h,
        14.0_h, 15.0_h, 16.0_h, 17.0_h,

        20.0_h, 21.0_h, 22.0_h, 23.0_h,
        24.0_h, 25.0_h, 26.0_h, 27.0_h,

        30.0_h, 31.0_h, 32.0_h, 33.0_h,
        34.0_h, 35.0_h, 36.0_h, 37.0_h,

        40.0_h, 41.0_h, 42.0_h, 43.0_h,
        44.0_h, 45.0_h, 46.0_h, 47.0_h,

        50.0_h, 51.0_h, 52.0_h, 53.0_h,
        54.0_h, 55.0_h, 56.0_h, 57.0_h,
    }), TestSuite::Compare::Container);
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
