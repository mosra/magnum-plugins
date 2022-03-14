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
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Half.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct OpenExrImporterTest: TestSuite::Tester {
    explicit OpenExrImporterTest();

    void emptyFile();
    void shortFile();
    void inconsistentFormat();
    void inconsistentDepthFormat();

    void rgb16f();
    void rgba32f();
    void rg32ui();
    void depth32f();

    void cubeMap();

    void forceChannelCountMore();
    void forceChannelCountLess();
    void forceChannelCountWrong();

    void customChannels();
    void customChannelsDuplicated();
    void customChannelsSomeUnassinged();
    void customChannelsAllUnassinged();
    void customChannelsFilled();
    void customChannelsDepth();
    void customChannelsDepthUnassigned();
    void customChannelsNoMatch();

    void levels2D();
    void levels2DIncomplete();
    void levelsCubeMap();
    void levelsCubeMapIncomplete();

    void threads();

    void openMemory();
    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

using namespace Math::Literals;

const struct {
    const char* name;
    const char* filename;
    const char* message;
} Rgb16fData[] {
    {"", "rgb16f.exr", ""},
    {"custom data/display window", "rgb16f-custom-windows.exr", ""},
    {"tiled", "rgb16f-tiled.exr", ""},
    {"ripmap", "rgb16f-ripmap.exr", "Trade::OpenExrImporter::openData(): ripmap files not supported, importing only the top level\n"}
};

const struct {
    const char* name;
    const char* filename;
} CubeMapData[] {
    {"", "envmap-cube.exr"},
    {"custom data/display window", "envmap-cube-custom-windows.exr"},
};

const struct {
    const char* name;
    const char* filename;
} Levels2DData[]{
    {"", "levels2D.exr"},
    {"custom tile size", "levels2D-tile1x1.exr"}
};

const struct {
    const char* name;
    const char* filename;
    Int levelCount;
    bool verbose;
    const char* message;
} Incomplete2DData[] {
    {"", "levels2D.exr", 3, false,
        ""},
    {"incomplete", "levels2D-incomplete.exr", 2, false,
        ""},
    {"verbose", "levels2D.exr", 3, true,
        ""},
    {"incomplete, verbose", "levels2D-incomplete.exr", 2, true,
        "Trade::OpenExrImporter::openData(): last 1 levels are missing in the file, capping at 2 levels\n"},
};

const struct {
    const char* name;
    const char* filename;
    Int levelCount;
    bool verbose;
    const char* message;
} IncompletelCubeMapData[] {
    {"subpixel levels missing", "levels-cube.exr", 3, false,
        ""},
    {"subpixel levels missing, verbose", "levels-cube.exr", 3, true,
        "Trade::OpenExrImporter::openData(): last 2 levels are too small to represent six cubemap faces (Vector(1, 3)), capping at 3 levels\n"},
    {"larger levels missing", "levels-cube-incomplete.exr", 2, false,
        ""},
    {"larger levels missing, verbose", "levels-cube-incomplete.exr", 2, true,
        "Trade::OpenExrImporter::openData(): last 2 levels are too small to represent six cubemap faces (Vector(1, 3)), capping at 3 levels\n"
        "Trade::OpenExrImporter::openData(): last 3 levels are missing in the file, capping at 2 levels\n"},
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
        "Trade::OpenExrImporter::openData(): increasing global OpenEXR thread pool from 0 to 1 extra worker threads\n"},
    {"three, quiet", 3, false,
        ""},
    /* This gets skipped if the detected thread count is not more than 3 as the
       second message won't get printed then */
    {"all, verbose", 0, true,
        "Trade::OpenExrImporter::openData(): autodetected hardware concurrency to {} threads\n"
        "Trade::OpenExrImporter::openData(): increasing global OpenEXR thread pool from 2 to {} extra worker threads\n"},
    {"all, quiet", 0, false,
        ""}
};

/* Shared among all plugins that implement data copying optimizations */
const struct {
    const char* name;
    bool(*open)(AbstractImporter&, Containers::ArrayView<const void>);
} OpenMemoryData[]{
    {"data", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        /* Copy to ensure the original memory isn't referenced */
        Containers::Array<char> copy{NoInit, data.size()};
        Utility::copy(Containers::arrayCast<const char>(data), copy);
        return importer.openData(copy);
    }},
    {"memory", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        return importer.openMemory(data);
    }},
};

OpenExrImporterTest::OpenExrImporterTest() {
    addTests({&OpenExrImporterTest::emptyFile,
              &OpenExrImporterTest::shortFile,
              &OpenExrImporterTest::inconsistentFormat,
              &OpenExrImporterTest::inconsistentDepthFormat});

    addInstancedTests({&OpenExrImporterTest::rgb16f},
        Containers::arraySize(Rgb16fData));

    addTests({&OpenExrImporterTest::rgba32f,
              &OpenExrImporterTest::rg32ui,
              &OpenExrImporterTest::depth32f});

    addInstancedTests({&OpenExrImporterTest::cubeMap},
        Containers::arraySize(CubeMapData));

    addTests({&OpenExrImporterTest::forceChannelCountMore,
              &OpenExrImporterTest::forceChannelCountLess,
              &OpenExrImporterTest::forceChannelCountWrong,

              &OpenExrImporterTest::customChannels,
              &OpenExrImporterTest::customChannelsDuplicated,
              &OpenExrImporterTest::customChannelsSomeUnassinged,
              &OpenExrImporterTest::customChannelsAllUnassinged,
              &OpenExrImporterTest::customChannelsFilled,
              &OpenExrImporterTest::customChannelsDepth,
              &OpenExrImporterTest::customChannelsDepthUnassigned,
              &OpenExrImporterTest::customChannelsNoMatch});

    addInstancedTests({&OpenExrImporterTest::levels2D},
        Containers::arraySize(Levels2DData));

    addInstancedTests({&OpenExrImporterTest::levels2DIncomplete},
        Containers::arraySize(Incomplete2DData));

    addTests({&OpenExrImporterTest::levelsCubeMap});

    addInstancedTests({&OpenExrImporterTest::levelsCubeMapIncomplete},
        Containers::arraySize(IncompletelCubeMapData));

    /* Could be addInstancedBenchmarks() to verify there's a difference but
       this would mean the test case gets skipped when CORRADE_NO_BENCHMARKS is
       enabled for a faster build. OTOH the improvement on a 5x3 image would be
       negative so that's useless to measure anyway. */
    /** @todo some way to say "run this but not as a bechmark if those are
        disabled"? */
    addInstancedTests({&OpenExrImporterTest::threads},
        Containers::arraySize(ThreadsData));

    addInstancedTests({&OpenExrImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&OpenExrImporterTest::openTwice,
              &OpenExrImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef OPENEXRIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(OPENEXRIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void OpenExrImporterTest::emptyFile() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData({}));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::openData(): import error: Cannot read image file \"\". Reading past end of file.\n");
}

void OpenExrImporterTest::shortFile() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");

    Containers::Optional<Containers::Array<char>> data = Utility::Path::read(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr"));
    CORRADE_VERIFY(importer->openData(data->exceptSuffix(1)));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): import error: Error reading pixel data from image file \"\". Reading past end of file.\n");
}

void OpenExrImporterTest::inconsistentFormat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgb32fa32ui.exr")));

    /* Opening succeeds, but the image import won't */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): channel A expected to be a FLOAT but got UINT\n");
}

void OpenExrImporterTest::inconsistentDepthFormat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "depth32ui.exr")));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): channel Z expected to be a FLOAT but got UINT\n");
}

void OpenExrImporterTest::rgb16f() {
    auto&& data = Rgb16fData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, data.filename)));
    }

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(out.str(), data.message);
    CORRADE_COMPARE(image->size(), Vector2i(1, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB16F);

    /* Data should be aligned to 4 bytes, clear padding to a zero value for
       predictable output. */
    CORRADE_COMPARE(image->data().size(), 3*8);
    Containers::ArrayView<char> imageData = image->mutableData();
    imageData[0*8 + 6] = imageData[0*8 + 7] =
        imageData[1*8 + 6] = imageData[1*8 + 7] =
            imageData[2*8 + 6] = imageData[2*8 + 7] = 0;

    CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView<Half>({
        0.0_h, 1.0_h, 2.0_h, {},
        3.0_h, 4.0_h, 5.0_h, {},
        6.0_h, 7.0_h, 8.0_h, {}
    }), TestSuite::Compare::Container);
}

void OpenExrImporterTest::rgba32f() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(1, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA32F);

    /* Data should be tightly packed here */
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(image->data()), Containers::arrayView<Float>({
        0.0f, 1.0f, 2.0f, 3.0f,
        4.0f, 5.0f, 6.0f, 7.0f,
        8.0f, 9.0f, 10.0f, 11.0f
    }), TestSuite::Compare::Container);
}

void OpenExrImporterTest::rg32ui() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rg32ui.exr")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(2, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RG32UI);

    /* Data should be tightly packed here as well */
    CORRADE_COMPARE_AS(Containers::arrayCast<const UnsignedInt>(image->data()), Containers::arrayView<UnsignedInt>({
        0x1111, 0x2222, 0x3333, 0x4444,
        0x5555, 0x6666, 0x7777, 0x8888
    }), TestSuite::Compare::Container);
}

void OpenExrImporterTest::depth32f() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "depth32f.exr")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::Depth32F);

    /* Data should be tightly packed here as well */
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(image->data()), Containers::arrayView<Float>({
        0.125f, 0.250f, 0.375f,
        0.500f, 0.625f, 0.750f,
    }), TestSuite::Compare::Container);
}

void OpenExrImporterTest::cubeMap() {
    auto&& data = CubeMapData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, data.filename)));

    /* A cube map image should be exposed only as a 3D image, not 2D */
    CORRADE_COMPARE(importer->image2DCount(), 0);
    CORRADE_COMPARE(importer->image3DCount(), 1);

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

void OpenExrImporterTest::forceChannelCountMore() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rg32ui.exr")));

    /* Missing channels should be filled */
    importer->configuration().setValue("forceChannelCount", 4);
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(2, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA32UI);
    CORRADE_COMPARE_AS(Containers::arrayCast<const UnsignedInt>(image->data()), Containers::arrayView<UnsignedInt>({
        0x1111, 0x2222, 0, 1, 0x3333, 0x4444, 0, 1,
        0x5555, 0x6666, 0, 1, 0x7777, 0x8888, 0, 1
    }), TestSuite::Compare::Container);
}

void OpenExrImporterTest::forceChannelCountLess() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

    /* Excessive channels should be ignored */
    importer->configuration().setValue("forceChannelCount", 2);
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(1, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RG32F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(image->data()), Containers::arrayView<Float>({
        0.0f, 1.0f,
        4.0f, 5.0f,
        8.0f, 9.0f
    }), TestSuite::Compare::Container);
}

void OpenExrImporterTest::forceChannelCountWrong() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

    importer->configuration().setValue("forceChannelCount", 5);
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): forceChannelCount is expected to be 0-4, got 5\n");
}

void OpenExrImporterTest::customChannels() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f-custom-channels.exr")));

    /* Should work directly before opening the image */
    importer->configuration().setValue("layer", "tangent");
    importer->configuration().setValue("r", "X");
    importer->configuration().setValue("g", "Y");
    importer->configuration().setValue("b", "Z");
    importer->configuration().setValue("a", "handedness");
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(1, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA32F);

    /* Data should be tightly packed here */
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(image->data()), Containers::arrayView<Float>({
        0.0f, 1.0f, 2.0f, 3.0f,
        4.0f, 5.0f, 6.0f, 7.0f,
        8.0f, 9.0f, 10.0f, 11.0f
    }), TestSuite::Compare::Container);
}

void OpenExrImporterTest::customChannelsDuplicated() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    importer->configuration().setValue("a", "G");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): duplicate mapping for channel G\n");
}

void OpenExrImporterTest::customChannelsSomeUnassinged() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

    importer->configuration().setValue("r", "");
    importer->configuration().setValue("g", "");
    /* B left as-is, as otherwise we'd get a failure because no channels match */
    importer->configuration().setValue("a", "");
    /* These shouldn't get used, memory should be zeroed */
    importer->configuration().setValue("rFill", 10.0);
    importer->configuration().setValue("gFill", 20.0);
    importer->configuration().setValue("aFill", 30.0);
    /* Forcing channel count so we verify that it works for all channels */
    importer->configuration().setValue("forceChannelCount", 4);
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(1, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA32F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(image->data()), Containers::arrayView<Float>({
        0.0f, 0.0f, 2.0f, 0.0f,
        0.0f, 0.0f, 6.0f, 0.0f,
        0.0f, 0.0f, 10.0f, 0.0f,
    }), TestSuite::Compare::Container);
}

void OpenExrImporterTest::customChannelsAllUnassinged() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

    /* Not even forceChannelCount will help here, as at least one channel has
       to match to pick RGBA */
    importer->configuration().setValue("r", "");
    importer->configuration().setValue("g", "");
    importer->configuration().setValue("b", "");
    importer->configuration().setValue("a", "");
    importer->configuration().setValue("forceChannelCount", 4);
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    /* The order is only because std::map orders keys and string orders
       alphabetically */
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): can't perform automatic mapping for channels named {A, B, G, R}, to either {, , , } or Z, provide desired layer and/or channel names in plugin configuration\n");
}

void OpenExrImporterTest::customChannelsFilled() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

    importer->configuration().setValue("r", "Red");
    /* G left as-is, as otherwise we'd get a failure because no channels match */
    importer->configuration().setValue("b", "Blue");
    importer->configuration().setValue("a", "Alpha");
    /* These shouldn't get used, memory should be zeroed */
    importer->configuration().setValue("rFill", 10.0);
    importer->configuration().setValue("bFill", 20.0);
    importer->configuration().setValue("aFill", 30.0);
    /* Forcing channel count so we verify that it works for all channels */
    importer->configuration().setValue("forceChannelCount", 4);
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(1, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA32F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(image->data()), Containers::arrayView<Float>({
        10.0f, 1.0f, 20.0f, 30.0f,
        10.0f, 5.0f, 20.0f, 30.0f,
        10.0f, 9.0f, 20.0f, 30.0f,
    }), TestSuite::Compare::Container);
}

void OpenExrImporterTest::customChannelsDepth() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    importer->configuration().setValue("layer", "left");
    importer->configuration().setValue("depth", "height");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "depth32f-custom-channels.exr")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::Depth32F);

    /* Data should be tightly packed here */
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(image->data()), Containers::arrayView<Float>({
        0.125f, 0.250f, 0.375f,
        0.500f, 0.625f, 0.750f
    }), TestSuite::Compare::Container);
}

void OpenExrImporterTest::customChannelsDepthUnassigned() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "depth32f.exr")));

    /* This will fail the same way as customChannelsAllUnassinged(), as there's
       no reason to not import anything */
    importer->configuration().setValue("depth", "");
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    /* The order is only because std::map orders keys and string orders
       alphabetically */
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): can't perform automatic mapping for channels named {Z}, to either {R, G, B, A} or , provide desired layer and/or channel names in plugin configuration\n");
}

void OpenExrImporterTest::customChannelsNoMatch() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

    /* Even just setting a layer should make it fail */
    importer->configuration().setValue("layer", "left");
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    /* The order is only because std::map orders keys and string orders
       alphabetically */
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): can't perform automatic mapping for channels named {A, B, G, R}, to either {left.R, left.G, left.B, left.A} or left.Z, provide desired layer and/or channel names in plugin configuration\n");
}

void OpenExrImporterTest::levels2D() {
    auto&& data = Levels2DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, data.filename)));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 3);

    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{5, 3}));
        CORRADE_COMPARE(image->format(), PixelFormat::R16F);

        /* Data should be aligned to 4 bytes, clear padding to a zero value for
           predictable output. */
        CORRADE_COMPARE(image->data().size(), 3*12);
        Containers::ArrayView<char> imageData = image->mutableData();
        imageData[0*12 + 10] = imageData[0*12 + 11] =
            imageData[1*12 + 10] = imageData[1*12 + 11] =
                imageData[2*12 + 10] = imageData[2*12 + 11] = 0;

        CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView<Half>({
             0.0_h,  1.0_h,  2.0_h,  3.0_h,  4.0_h, {},
             5.0_h,  6.0_h,  7.0_h,  8.0_h,  9.0_h, {},
            10.0_h, 11.0_h, 12.0_h, 13.0_h, 14.0_h, {}
        }), TestSuite::Compare::Container);
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{2, 1}));
        CORRADE_COMPARE(image->format(), PixelFormat::R16F);

        CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView<Half>({
             0.5_h,  2.5_h
        }), TestSuite::Compare::Container);
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 2);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{1, 1}));
        CORRADE_COMPARE(image->format(), PixelFormat::R16F);

        /* Data should be aligned to 4 bytes, clear padding to a zero value for
           predictable output. */
        CORRADE_COMPARE(image->data().size(), 4);
        Containers::ArrayView<char> imageData = image->mutableData();
        imageData[2] = imageData[3] = 0;

        CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView<Half>({
             1.5_h, {}
        }), TestSuite::Compare::Container);
    }
}

void OpenExrImporterTest::levels2DIncomplete() {
    auto&& data = Incomplete2DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    if(data.verbose) importer->addFlags(ImporterFlag::Verbose);

    std::ostringstream out;
    {
        Debug redirectDebug{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, data.filename)));
    }

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), data.levelCount);
    CORRADE_COMPARE(out.str(), data.message);

    /* The first two level should be the same as with levels2D() */
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{5, 3}));
        CORRADE_COMPARE(image->format(), PixelFormat::R16F);

        /* Data should be aligned to 4 bytes, clear padding to a zero value for
           predictable output. */
        CORRADE_COMPARE(image->data().size(), 3*12);
        Containers::ArrayView<char> imageData = image->mutableData();
        imageData[0*12 + 10] = imageData[0*12 + 11] =
            imageData[1*12 + 10] = imageData[1*12 + 11] =
                imageData[2*12 + 10] = imageData[2*12 + 11] = 0;

        CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView<Half>({
             0.0_h,  1.0_h,  2.0_h,  3.0_h,  4.0_h, {},
             5.0_h,  6.0_h,  7.0_h,  8.0_h,  9.0_h, {},
            10.0_h, 11.0_h, 12.0_h, 13.0_h, 14.0_h, {}
        }), TestSuite::Compare::Container);
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{2, 1}));
        CORRADE_COMPARE(image->format(), PixelFormat::R16F);

        CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView<Half>({
             0.5_h,  2.5_h
        }), TestSuite::Compare::Container);
    }
}

void OpenExrImporterTest::levelsCubeMap() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "levels-cube.exr")));
    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), 3);

    {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, 0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector3i{4, 4, 6}));
        CORRADE_COMPARE(image->format(), PixelFormat::R16F);

        CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView<Half>({
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
        }), TestSuite::Compare::Container);
    } {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, 1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector3i{2, 2, 6}));
        CORRADE_COMPARE(image->format(), PixelFormat::R16F);

        CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView<Half>({
             0.5_h,  2.5_h,  8.5_h, 10.5_h,
            16.5_h, 18.5_h, 24.5_h, 26.5_h,
            32.5_h, 34.5_h, 40.5_h, 42.5_h,
            48.5_h, 50.5_h, 56.5_h, 58.5_h,
            64.5_h, 66.5_h, 72.5_h, 74.5_h,
            80.5_h, 82.5_h, 88.5_h, 90.5_h
        }), TestSuite::Compare::Container);
    } {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, 2);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector3i{1, 1, 6}));
        CORRADE_COMPARE(image->format(), PixelFormat::R16F);

        /* Data should be aligned to 4 bytes, clear padding to a zero value for
           predictable output. */
        CORRADE_COMPARE(image->data().size(), 6*4);
        Containers::ArrayView<char> imageData = image->mutableData();
        imageData[0*4 + 2] = imageData[0*4 + 3] =
            imageData[1*4 + 2] = imageData[1*4 + 3] =
                imageData[2*4 + 2] = imageData[2*4 + 3] =
                    imageData[3*4 + 2] = imageData[3*4 + 3] =
                        imageData[4*4 + 2] = imageData[4*4 + 3] =
                            imageData[5*4 + 2] = imageData[5*4 + 3] = 0;

        CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView<Half>({
             0.5_h, {},
             4.5_h, {},
             8.5_h, {},
            12.5_h, {},
            16.5_h, {},
            20.5_h, {}
        }), TestSuite::Compare::Container);
    }
}

void OpenExrImporterTest::levelsCubeMapIncomplete() {
    auto&& data = IncompletelCubeMapData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    if(data.verbose) importer->addFlags(ImporterFlag::Verbose);

    std::ostringstream out;
    {
        Debug redirectDebug{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, data.filename)));
    }

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), data.levelCount);
    CORRADE_COMPARE(out.str(), data.message);

    /* The first two level should be the same as with levelsCubeMap() */
    {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, 0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector3i{4, 4, 6}));
        CORRADE_COMPARE(image->format(), PixelFormat::R16F);

        CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView<Half>({
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
        }), TestSuite::Compare::Container);
    } {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, 1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector3i{2, 2, 6}));
        CORRADE_COMPARE(image->format(), PixelFormat::R16F);

        CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView<Half>({
             0.5_h,  2.5_h,  8.5_h, 10.5_h,
            16.5_h, 18.5_h, 24.5_h, 26.5_h,
            32.5_h, 34.5_h, 40.5_h, 42.5_h,
            48.5_h, 50.5_h, 56.5_h, 58.5_h,
            64.5_h, 66.5_h, 72.5_h, 74.5_h,
            80.5_h, 82.5_h, 88.5_h, 90.5_h
        }), TestSuite::Compare::Container);
    }
}

void OpenExrImporterTest::threads() {
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

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    if(data.threads != 1)
        importer->configuration().setValue("threads", data.threads);
    if(data.verbose)
        importer->addFlags(ImporterFlag::Verbose);

    std::ostringstream out;
    {
        Debug redirectOutput{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgb16f.exr")));
    }

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(out.str(), Utility::formatString(data.message,
        std::thread::hardware_concurrency(),
        std::thread::hardware_concurrency() - 1));
    CORRADE_COMPARE(image->size(), Vector2i(1, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB16F);

    /* Data should be aligned to 4 bytes, clear padding to a zero value for
       predictable output. */
    CORRADE_COMPARE(image->data().size(), 3*8);
    Containers::ArrayView<char> imageData = image->mutableData();
    imageData[0*8 + 6] = imageData[0*8 + 7] =
        imageData[1*8 + 6] = imageData[1*8 + 7] =
            imageData[2*8 + 6] = imageData[2*8 + 7] = 0;

    CORRADE_COMPARE_AS(Containers::arrayCast<const Half>(image->data()), Containers::arrayView<Half>({
        0.0_h, 1.0_h, 2.0_h, {},
        3.0_h, 4.0_h, 5.0_h, {},
        6.0_h, 7.0_h, 8.0_h, {}
    }), TestSuite::Compare::Container);
}

void OpenExrImporterTest::openMemory() {
    /* Same as rgba32f() except that it uses openData() & openMemory() instead
       of openFile() to test data copying on import */

    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*importer, *memory));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(1, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA32F);

    /* Data should be tightly packed here */
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(image->data()), Containers::arrayView<Float>({
        0.0f, 1.0f, 2.0f, 3.0f,
        4.0f, 5.0f, 6.0f, 7.0f,
        8.0f, 9.0f, 10.0f, 11.0f
    }), TestSuite::Compare::Container);
}

void OpenExrImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgb16f.exr")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgb16f.exr")));

    /* Shouldn't crash, leak or anything */
}

void OpenExrImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(OPENEXRIMPORTER_TEST_DIR, "rgb16f.exr")));

    /* Verify that everything is working the same way on second use */
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{1, 3}));
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{1, 3}));
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::OpenExrImporterTest)
