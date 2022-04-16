/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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
#include <thread> /* std::thread::hardware_concurrency(), sigh */
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
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

    void levels2D();
    void levels2DIncomplete();
    void levels2DInvalidLevelSize();
    void levels2DInvalidTileSize();
    void levelsCubeMap();
    void levelsCubeMapIncomplete();
    void levelsCubeMapInvalidLevelSize();
    void levelsCubeMapInvalidLevelSlices();

    void threads();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _manager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

using namespace Containers::Literals;
using namespace Math::Literals;

const struct {
    const char* name;
    const char* filename;
    bool tiled;
} TiledData[] {
    {"", "rgb16f.exr", false},
    {"force tiled output", "rgb16f-tiled.exr", true}
};

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
    const char* name;
    const char* compression;
    Containers::Optional<Int> zipCompressionLevel;
    Containers::Optional<Float> dwaCompressionLevel;
    std::size_t size;
    std::size_t cubeSize;
} CompressionData[] {
    {"", "", {}, {}, 427, 602},
    {"RLE", "rle", {}, {}, 427, 602},
    /* For consistency with versions before 3.1.3 (where it's hardcoded to
       6 instead of 4 and can't be changed) */
    {"ZIP level 6", "zip", 6, {}, 391, 402},
    #if OPENEXR_VERSION_MAJOR*10000 + OPENEXR_VERSION_MINOR*100 + OPENEXR_VERSION_PATCH >= 30103
    {"ZIP level 0", "zip", 0, {}, 395, 426},
    #endif
    {"ZIPS", "zips", {}, {}, 427, 602},
    {"PIZ", "piz", {}, {}, 395, 426},
    {"DWAA default level", "dwaa", {}, {}, 395, 426},
    /* Sigh, no difference. Data too weird, probably. On OpenEXR < 3.1.3 it
       *does* make a difference, but that's only because there's an additional
       header attribute describing compression level. */
    {"DWAB level 21.7", "dwab", {}, 21.7f,
        #if OPENEXR_VERSION_MAJOR*10000 + OPENEXR_VERSION_MINOR*100 + OPENEXR_VERSION_PATCH >= 30103
        395, 426
        #else
        429, 460
        #endif
    }
};

const struct {
    const char* name;
    const char* filename;
    Vector2i tileSize;
} Levels2DData[]{
    {"", "levels2D.exr", {}},
    {"custom tile size", "levels2D-tile1x1.exr", {1, 1}}
};

const struct {
    const char* name;
    Int threads;
    bool verbose;
    const char* message;
} ThreadsData[]{
    {"default", 1, true,
        ""},
    {"two, verbose", 2, true,
        "Trade::OpenExrImageConverter::convertToData(): increasing global OpenEXR thread pool from 0 to 1 extra worker threads\n"},
    {"three, quiet", 3, false,
        ""},
    /* This gets skipped if the detected thread count is not more than 3 as the
       second message won't get printed then */
    {"all, verbose", 0, true,
        "Trade::OpenExrImageConverter::convertToData(): autodetected hardware concurrency to {} threads\n"
        "Trade::OpenExrImageConverter::convertToData(): increasing global OpenEXR thread pool from 2 to {} extra worker threads\n"},
    {"all, quiet", 0, false,
        ""}
};

OpenExrImageConverterTest::OpenExrImageConverterTest() {
    addTests({&OpenExrImageConverterTest::wrongFormat,
              &OpenExrImageConverterTest::conversionError});

    addInstancedTests({&OpenExrImageConverterTest::rgb16f},
        Containers::arraySize(TiledData));

    addTests({&OpenExrImageConverterTest::rgba32f,
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

    addInstancedTests({&OpenExrImageConverterTest::levels2D},
        Containers::arraySize(Levels2DData));

    addTests({&OpenExrImageConverterTest::levels2DIncomplete,
              &OpenExrImageConverterTest::levels2DInvalidLevelSize,
              &OpenExrImageConverterTest::levels2DInvalidTileSize,
              &OpenExrImageConverterTest::levelsCubeMap,
              &OpenExrImageConverterTest::levelsCubeMapIncomplete,
              &OpenExrImageConverterTest::levelsCubeMapInvalidLevelSize,
              &OpenExrImageConverterTest::levelsCubeMapInvalidLevelSlices});

    /* Could be addInstancedBenchmarks() to verify there's a difference but
       this would mean the test case gets skipped when CORRADE_NO_BENCHMARKS is
       enabled for a faster build. OTOH the improvement on a 5x3 image would be
       negative so that's useless to measure anyway. */
    /** @todo some way to say "run this but not as a bechmark if those are
        disabled"? */
    addInstancedTests({&OpenExrImageConverterTest::threads}, Containers::arraySize(ThreadsData));

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
    auto&& data = TiledData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    if(data.tiled)
        converter->configuration().setValue("forceTiledOutput", true);

    Containers::Optional<Containers::Array<char>> out = converter->convertToData(Rgb16f);
    CORRADE_VERIFY(out);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, data.filename),
        TestSuite::Compare::StringToFile);

    /* By default we're exporting scanline files, so the metadata should
       contain no tile-related information. */
    if(!data.tiled) {
        CORRADE_VERIFY(!Containers::StringView{*out}.contains("tiles"_s));
        CORRADE_VERIFY(!Containers::StringView{*out}.contains("tiledesc"_s));
    /* In case of a tiled file the imported data should show no difference, but
       the metadata should contain tile-related information. */
    } else {
        CORRADE_VERIFY(Containers::StringView{*out}.contains("tiles\0tiledesc"_s));
    }

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*out));

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Rgb16f, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::rgba32f() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");

    /* Reset ZIP compression level to 6 for consistency with versions before
       3.1.3 (on those it's the hardcoded default) */
    converter->configuration().setValue("zipCompressionLevel", 6);

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(Rgba32f);
    CORRADE_VERIFY(data);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*data));

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Rgba32f, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::rg32ui() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");

    /* Reset ZIP compression level to 6 for consistency with versions before
       3.1.3 (on those it's the hardcoded default) */
    converter->configuration().setValue("zipCompressionLevel", 6);

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(Rg32ui);
    CORRADE_VERIFY(data);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rg32ui.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*data));

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Rg32ui, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::depth32f() {
    Containers::Optional<Containers::Array<char>> data = _manager.instantiate("OpenExrImageConverter")->convertToData(Depth32f);
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "depth32f.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*data));

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Depth32f, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::envmap2DLatLong() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "latlong");

    /* Reset ZIP compression level to 6 for consistency with versions before
       3.1.3 (on those it's the hardcoded default) */
    converter->configuration().setValue("zipCompressionLevel", 6);

    /* The width needs to be 2*height, abuse existing data for that */
    ImageView2D R32ui{PixelFormat::R32UI, {4, 2}, Rg32uiData};
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(R32ui);
    CORRADE_VERIFY(data);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(OPENEXRIMAGECONVERTER_TEST_DIR, "envmap-latlong.exr"),
        TestSuite::Compare::StringToFile);

    /* The metadata has no effect on the actual saved data, so no point in
       importing. Verifying the metadata has to be done using the `exrheader`
       tool, the importer has no API for that. This is only a basic check that
       the metadata got added. */
    CORRADE_VERIFY(Containers::StringView{*data}.contains("envmap\0"_s));
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

    /* Reset ZIP compression level to 6 for consistency with versions before
       3.1.3 (on those it's the hardcoded default) */
    converter->configuration().setValue("zipCompressionLevel", 6);

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(CubeRg16f);
    CORRADE_VERIFY(data);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "envmap-cube.exr"),
        TestSuite::Compare::StringToFile);

    /* The metadata has no effect on the actual saved data, so no point in
       importing. Verifying the metadata has to be done using the `exrheader`
       tool, the importer has no API for that. This is only a basic check that
       the metadata got added. */
    CORRADE_VERIFY(Containers::StringView{*data}.contains("envmap\0"_s));

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*data));
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

    /* Reset ZIP compression level to 6 for consistency with versions before
       3.1.3 (on those it's the hardcoded default) */
    converter->configuration().setValue("zipCompressionLevel", 6);

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(Rgba32f);
    CORRADE_VERIFY(data);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f-custom-channels.exr"),
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
    CORRADE_VERIFY(importer->openData(*data));

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
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(Rgba32f);
    CORRADE_VERIFY(data);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(OPENEXRIMAGECONVERTER_TEST_DIR, "rb32f-custom-channels.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    importer->configuration().setValue("layer", "normal");
    importer->configuration().setValue("r", "X");
    importer->configuration().setValue("g", "Z");
    /* B, A stays at default, but shouldn't get filled */
    CORRADE_VERIFY(importer->openData(*data));

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
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(Depth32f);
    CORRADE_VERIFY(data);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "depth32f-custom-channels.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    importer->configuration().setValue("layer", "left");
    importer->configuration().setValue("depth", "height");
    CORRADE_VERIFY(importer->openData(*data));

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

    /* Reset ZIP compression level to 6 for consistency with versions before
       3.1.3 (on those it's the hardcoded default) */
    converter->configuration().setValue("zipCompressionLevel", 6);

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(Rgb16f);
    CORRADE_VERIFY(data);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgb16f-custom-windows.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*data));

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

    /* Reset ZIP compression level to 6 for consistency with versions before
       3.1.3 (on those it's the hardcoded default) */
    converter->configuration().setValue("zipCompressionLevel", 6);

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(CubeRg16f);
    CORRADE_VERIFY(data);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "envmap-cube-custom-windows.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*data));
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
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("compression", data.compression);

    if(data.zipCompressionLevel)
        converter->configuration().setValue("zipCompressionLevel", *data.zipCompressionLevel);
    if(data.dwaCompressionLevel)
        converter->configuration().setValue("dwaCompressionLevel", *data.dwaCompressionLevel);

    Containers::Optional<Containers::Array<char>> out = converter->convertToData(Rgba32f);
    CORRADE_VERIFY(out);

    /* The sizes should slightly differ at the very least -- this checks that
       the setting isn't just plainly ignored */
    CORRADE_COMPARE(out->size(), data.size);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*out));

    /* Using only lossless compression here, so the data should match */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image, Rgba32f, DebugTools::CompareImage);
}

void OpenExrImageConverterTest::compressionCubeMap() {
    auto&& data = CompressionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "cube");
    converter->configuration().setValue("compression", data.compression);

    if(data.zipCompressionLevel)
        converter->configuration().setValue("zipCompressionLevel", *data.zipCompressionLevel);
    if(data.dwaCompressionLevel)
        converter->configuration().setValue("dwaCompressionLevel", *data.dwaCompressionLevel);

    Containers::Optional<Containers::Array<char>> out = converter->convertToData(CubeRg16f);
    CORRADE_VERIFY(out);

    /* The sizes should slightly differ at the very least -- this checks that
       the setting isn't just plainly ignored */
    CORRADE_COMPARE(out->size(), data.cubeSize);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*out));

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

void OpenExrImageConverterTest::levels2D() {
    auto&& data = Levels2DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");

    /* There's not really a way to verify the config is applied except for
       checking the output with exrheader, the imported data should be the same
       for both. */
    if(!data.tileSize.isZero()) {
        converter->configuration().setValue("tileSize", data.tileSize);
    }

    /* Test that round down is done correctly and that the larger dimension is
       used to calculate level count (otherwise image2 would have zero height).
       Sizes divisible by two are tested in levelsCubeMap(). */
    const Half data0[] = {
         0.0_h,  1.0_h,  2.0_h,  3.0_h,  4.0_h,
         5.0_h,  6.0_h,  7.0_h,  8.0_h,  9.0_h,
        10.0_h, 11.0_h, 12.0_h, 13.0_h, 14.0_h
    };
    const Half data1[] = {
        0.5_h, 2.5_h,
    };
    const Half data2[] = {
        1.5_h
    };
    ImageView2D image0{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {5, 3}, data0};
    ImageView2D image1{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {2, 1}, data1};
    ImageView2D image2{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {1, 1}, data2};
    Containers::Optional<Containers::Array<char>> out = converter->convertToData({image0, image1, image2});
    CORRADE_VERIFY(out);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, data.filename),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*out));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 3);

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE_AS(*image, image0, DebugTools::CompareImage);
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE_AS(*image, image1, DebugTools::CompareImage);
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 2);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE_AS(*image, image2, DebugTools::CompareImage);
    }
}

void OpenExrImageConverterTest::levels2DIncomplete() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");

    /* Use nicely rounded sizes here to test this case as well */
    const Half data0[] = {
         0.0_h,  1.0_h,  2.0_h,  3.0_h,  4.0_h,
         5.0_h,  6.0_h,  7.0_h,  8.0_h,  9.0_h,
        10.0_h, 11.0_h, 12.0_h, 13.0_h, 14.0_h
    };
    const Half data1[] = {
        0.5_h, 2.5_h,
    };
    ImageView2D image0{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {5, 3}, data0};
    ImageView2D image1{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {2, 1}, data1};
    Containers::Optional<Containers::Array<char>> data = converter->convertToData({image0, image1});
    CORRADE_VERIFY(data);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "levels2D-incomplete.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*data));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 2);

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE_AS(*image, image0, DebugTools::CompareImage);
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE_AS(*image, image1, DebugTools::CompareImage);
    }
}

void OpenExrImageConverterTest::levels2DInvalidLevelSize() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");

    const Half data[16];

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData({
        ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {5, 3}, data},
        ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {3, 2}, data}
    }));
    /* Test also that it doesn't say "expected Vector(2, 0)" */
    CORRADE_VERIFY(!converter->convertToData({
        ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {8, 2}, data},
        ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {4, 1}, data},
        ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {1, 1}, data},
    }));
    CORRADE_VERIFY(!converter->convertToData({
        ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {2, 2}, data},
        ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {1, 1}, data},
        ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {1, 1}, data},
    }));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenExrImageConverter::convertToData(): size of image at level 1 expected to be Vector(2, 1) but got Vector(3, 2)\n"
        "Trade::OpenExrImageConverter::convertToData(): size of image at level 2 expected to be Vector(2, 1) but got Vector(1, 1)\n"
        "Trade::OpenExrImageConverter::convertToData(): there can be only 2 levels with base image size Vector(2, 2) but got 3\n");
}

void OpenExrImageConverterTest::levels2DInvalidTileSize() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    /* Force tiled output to avoid the need to invent two images */
    converter->configuration().setValue("forceTiledOutput", true);
    converter->configuration().setValue("tileSize", Vector2i{});

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(Rgb16f));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenExrImageConverter::convertToData(): conversion error: Cannot open image file \"\". Invalid tile size in image header.\n");
}

void OpenExrImageConverterTest::levelsCubeMap() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "cube");

    /* Reset ZIP compression level to 6 for consistency with versions before
       3.1.3 (on those it's the hardcoded default) */
    converter->configuration().setValue("zipCompressionLevel", 6);

    const Half data0[]{
         0.0_h,  1.0_h,  2.0_h,  3.0_h,
         4.0_h,  5.0_h,  6.0_h,  7.0_h,
         8.0_h,  9.0_h, 10.0_h, 11.0_h,
        12.0_h, 13.0_h, 14.0_h, 15.0_h,

        16.0_h, 17.0_h, 18.0_h, 19.0_h,
        20.0_h, 21.0_h, 22.0_h, 23.0_h,
        24.0_h, 25.0_h, 26.0_h, 27.0_h,
        28.0_h, 29.0_h, 30.0_h, 31.0_h,

        32.0_h, 33.0_h, 34.0_h, 35.0_h,
        36.0_h, 37.0_h, 38.0_h, 39.0_h,
        40.0_h, 41.0_h, 42.0_h, 43.0_h,
        44.0_h, 45.0_h, 46.0_h, 47.0_h,

        48.0_h, 49.0_h, 50.0_h, 51.0_h,
        52.0_h, 53.0_h, 54.0_h, 55.0_h,
        56.0_h, 57.0_h, 58.0_h, 59.0_h,
        60.0_h, 61.0_h, 62.0_h, 63.0_h,

        64.0_h, 65.0_h, 66.0_h, 67.0_h,
        68.0_h, 69.0_h, 70.0_h, 71.0_h,
        72.0_h, 73.0_h, 74.0_h, 75.0_h,
        76.0_h, 77.0_h, 78.0_h, 79.0_h,

        80.0_h, 81.0_h, 82.0_h, 83.0_h,
        84.0_h, 85.0_h, 86.0_h, 87.0_h,
        88.0_h, 89.0_h, 90.0_h, 91.0_h,
        92.0_h, 93.0_h, 94.0_h, 95.0_h
    };
    const Half data1[]{
         0.5_h,  2.5_h,  8.5_h, 10.5_h,
        16.5_h, 18.5_h, 24.5_h, 26.5_h,
        32.5_h, 34.5_h, 40.5_h, 42.5_h,
        48.5_h, 50.5_h, 56.5_h, 58.5_h,
        64.5_h, 66.5_h, 72.5_h, 74.5_h,
        80.5_h, 82.5_h, 88.5_h, 90.5_h
    };
    const Half data2[]{
         0.5_h,
         4.5_h,
         8.5_h,
        12.5_h,
        16.5_h,
        20.5_h
    };
    ImageView3D image0{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {4, 4, 6}, data0};
    ImageView3D image1{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {2, 2, 6}, data1};
    ImageView3D image2{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {1, 1, 6}, data2};
    Containers::Optional<Containers::Array<char>> data = converter->convertToData({image0, image1, image2});
    CORRADE_VERIFY(data);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*data},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "levels-cube.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*data));
    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), 3);

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, 0);
        CORRADE_VERIFY(image);
        /** @todo clean up once we can compare 3D images, or at least slice
            them */
        for(Int i = 0; i != 6; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE_AS(
                (ImageView2D{image->storage().setSkip({0, 0, i}), image->format(), image->size().xy(), image->data()}),
                (ImageView2D{image0.storage().setSkip({0, 0, i}), image0.format(), image0.size().xy(), image0.data()}),
                DebugTools::CompareImage);
        }
    } {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, 1);
        CORRADE_VERIFY(image);
        /** @todo clean up once we can compare 3D images, or at least slice
            them */
        for(Int i = 0; i != 6; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE_AS(
                (ImageView2D{image->storage().setSkip({0, 0, i}), image->format(), image->size().xy(), image->data()}),
                (ImageView2D{image1.storage().setSkip({0, 0, i}), image1.format(), image1.size().xy(), image1.data()}),
                DebugTools::CompareImage);
        }
    } {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, 2);
        CORRADE_VERIFY(image);
        /** @todo clean up once we can compare 3D images, or at least slice
            them */
        for(Int i = 0; i != 6; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE_AS(
                (ImageView2D{image->storage().setSkip({0, 0, i}), image->format(), image->size().xy(), image->data()}),
                (ImageView2D{image2.storage().setSkip({0, 0, i}), image2.format(), image2.size().xy(), image2.data()}),
                DebugTools::CompareImage);
        }
    }
}

void OpenExrImageConverterTest::levelsCubeMapIncomplete() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "cube");

    /* Reset ZIP compression level to 6 for consistency with versions before
       3.1.3 (on those it's the hardcoded default) */
    converter->configuration().setValue("zipCompressionLevel", 6);

    const Half data0[]{
         0.0_h,  1.0_h,  2.0_h,  3.0_h,
         4.0_h,  5.0_h,  6.0_h,  7.0_h,
         8.0_h,  9.0_h, 10.0_h, 11.0_h,
        12.0_h, 13.0_h, 14.0_h, 15.0_h,

        16.0_h, 17.0_h, 18.0_h, 19.0_h,
        20.0_h, 21.0_h, 22.0_h, 23.0_h,
        24.0_h, 25.0_h, 26.0_h, 27.0_h,
        28.0_h, 29.0_h, 30.0_h, 31.0_h,

        32.0_h, 33.0_h, 34.0_h, 35.0_h,
        36.0_h, 37.0_h, 38.0_h, 39.0_h,
        40.0_h, 41.0_h, 42.0_h, 43.0_h,
        44.0_h, 45.0_h, 46.0_h, 47.0_h,

        48.0_h, 49.0_h, 50.0_h, 51.0_h,
        52.0_h, 53.0_h, 54.0_h, 55.0_h,
        56.0_h, 57.0_h, 58.0_h, 59.0_h,
        60.0_h, 61.0_h, 62.0_h, 63.0_h,

        64.0_h, 65.0_h, 66.0_h, 67.0_h,
        68.0_h, 69.0_h, 70.0_h, 71.0_h,
        72.0_h, 73.0_h, 74.0_h, 75.0_h,
        76.0_h, 77.0_h, 78.0_h, 79.0_h,

        80.0_h, 81.0_h, 82.0_h, 83.0_h,
        84.0_h, 85.0_h, 86.0_h, 87.0_h,
        88.0_h, 89.0_h, 90.0_h, 91.0_h,
        92.0_h, 93.0_h, 94.0_h, 95.0_h
    };
    const Half data1[]{
         0.5_h,  2.5_h,  8.5_h, 10.5_h,
        16.5_h, 18.5_h, 24.5_h, 26.5_h,
        32.5_h, 34.5_h, 40.5_h, 42.5_h,
        48.5_h, 50.5_h, 56.5_h, 58.5_h,
        64.5_h, 66.5_h, 72.5_h, 74.5_h,
        80.5_h, 82.5_h, 88.5_h, 90.5_h
    };
    ImageView3D image0{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {4, 4, 6}, data0};
    ImageView3D image1{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {2, 2, 6}, data1};
    Containers::Optional<Containers::Array<char>> out = converter->convertToData({image0, image1});
    CORRADE_VERIFY(out);
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*out},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "levels-cube-incomplete.exr"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("OpenExrImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("OpenExrImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openData(*out));
    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), 2);

    /* This is thoroughly tested in OpenExrImporter, do just a basic check of
       the contents and not the actual data layout */
    {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, 0);
        CORRADE_VERIFY(image);
        /** @todo clean up once we can compare 3D images, or at least slice
            them */
        for(Int i = 0; i != 6; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE_AS(
                (ImageView2D{image->storage().setSkip({0, 0, i}), image->format(), image->size().xy(), image->data()}),
                (ImageView2D{image0.storage().setSkip({0, 0, i}), image0.format(), image0.size().xy(), image0.data()}),
                DebugTools::CompareImage);
        }
    } {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, 1);
        CORRADE_VERIFY(image);
        /** @todo clean up once we can compare 3D images, or at least slice
            them */
        for(Int i = 0; i != 6; ++i) {
            CORRADE_ITERATION(i);
            CORRADE_COMPARE_AS(
                (ImageView2D{image->storage().setSkip({0, 0, i}), image->format(), image->size().xy(), image->data()}),
                (ImageView2D{image1.storage().setSkip({0, 0, i}), image1.format(), image1.size().xy(), image1.data()}),
                DebugTools::CompareImage);
        }
    }
}

void OpenExrImageConverterTest::levelsCubeMapInvalidLevelSize() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "cube");

    const Half data[150];

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData({
        ImageView3D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {5, 5, 6}, data},
        ImageView3D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {3, 3, 6}, data}
    }));
    /* Unlike with the 2D case, the slices have to be square so there's no
       way this could say e.g. "expected Vector(2, 0, 6)" so that test is
       omitted. */
    CORRADE_VERIFY(!converter->convertToData({
        ImageView3D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {2, 2, 6}, data},
        ImageView3D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {1, 1, 6}, data},
        ImageView3D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {1, 1, 6}, data},
    }));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenExrImageConverter::convertToData(): size of cubemap image at level 1 expected to be Vector(2, 2, 6) but got Vector(3, 3, 6)\n"
        "Trade::OpenExrImageConverter::convertToData(): there can be only 2 levels with base cubemap image size Vector(2, 2, 6) but got 3\n");
}

void OpenExrImageConverterTest::levelsCubeMapInvalidLevelSlices() {
    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    converter->configuration().setValue("envmap", "cube");

    const Half data[96];

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData({
        ImageView3D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {4, 4, 6}, data},
        ImageView3D{PixelStorage{}.setAlignment(1), PixelFormat::R16F, {3, 3, 7}, data}
    }));
    CORRADE_COMPARE(out.str(),
        "Trade::OpenExrImageConverter::convertToData(): size of cubemap image at level 1 expected to be Vector(2, 2, 6) but got Vector(3, 3, 7)\n");
}

void OpenExrImageConverterTest::threads() {
    auto&& data = ThreadsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    #ifdef CORRADE_TARGET_MINGW
    CORRADE_SKIP("Running this test causes a freeze on exit on MinGW. Or something like that. Needs investigation.");
    #endif

    /* Assuming the tests were run in order, if the autodetected thread count
       is less than 3 then the message about increasing global thread pool size
       won't be printed. Skip the test in that case. */
    if(data.threads == 0 && std::thread::hardware_concurrency() <= 3 && data.verbose)
        CORRADE_SKIP("Autodetected thread count less than expected, can't verify the full message.");

    Containers::Pointer<AbstractImageConverter> converter = _manager.instantiate("OpenExrImageConverter");
    if(data.threads != 1)
        converter->configuration().setValue("threads", data.threads);
    if(data.verbose)
        converter->addFlags(ImageConverterFlag::Verbose);

    std::ostringstream out;
    Debug redirectOutput{&out};
    Containers::Optional<Containers::Array<char>> outData = converter->convertToData(Rgb16f);
    CORRADE_VERIFY(outData);
    /* The file should be always the same, no need to test the contents */
    /** @todo Compare::DataToFile */
    CORRADE_COMPARE_AS(Containers::StringView{*outData},
        Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgb16f.exr"),
        TestSuite::Compare::StringToFile);
    CORRADE_COMPARE(out.str(), Utility::formatString(data.message,
        std::thread::hardware_concurrency(),
        std::thread::hardware_concurrency() - 1));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::OpenExrImageConverterTest)
