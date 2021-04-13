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
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
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

    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

using namespace Math::Literals;

const struct {
    const char* name;
    const char* filename;
} Rgb16fData[] {
    {"", "rgb16f.exr"},
    {"custom data/display window", "rgb16f-custom-windows.exr"},
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
              &OpenExrImporterTest::depth32f,

              &OpenExrImporterTest::forceChannelCountMore,
              &OpenExrImporterTest::forceChannelCountLess,
              &OpenExrImporterTest::forceChannelCountWrong,

              &OpenExrImporterTest::customChannels,
              &OpenExrImporterTest::customChannelsDuplicated,
              &OpenExrImporterTest::customChannelsSomeUnassinged,
              &OpenExrImporterTest::customChannelsAllUnassinged,
              &OpenExrImporterTest::customChannelsFilled,
              &OpenExrImporterTest::customChannelsDepth,
              &OpenExrImporterTest::customChannelsDepthUnassigned,
              &OpenExrImporterTest::customChannelsNoMatch,

              &OpenExrImporterTest::openTwice,
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

    CORRADE_VERIFY(importer->openData(Utility::Directory::read(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")).except(1)));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): import error: Error reading pixel data from image file \"\". Reading past end of file.\n");
}

void OpenExrImporterTest::inconsistentFormat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgb32fa32ui.exr")));

    /* Opening succeeds, but the image import won't */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): channel A expected to be a FLOAT but got UINT\n");
}

void OpenExrImporterTest::inconsistentDepthFormat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "depth32ui.exr")));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): channel Z expected to be a FLOAT but got UINT\n");
}

void OpenExrImporterTest::rgb16f() {
    auto&& data = Rgb16fData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
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
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rg32ui.exr")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "depth32f.exr")));

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

void OpenExrImporterTest::forceChannelCountMore() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rg32ui.exr")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

    importer->configuration().setValue("forceChannelCount", 5);
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): forceChannelCount is expected to be 0-4, got 5\n");
}

void OpenExrImporterTest::customChannels() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f-custom-channels.exr")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): duplicate mapping for channel G\n");
}

void OpenExrImporterTest::customChannelsSomeUnassinged() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "depth32f-custom-channels.exr")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "depth32f.exr")));

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
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgba32f.exr")));

    /* Even just setting a layer should make it fail */
    importer->configuration().setValue("layer", "left");
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    /* The order is only because std::map orders keys and string orders
       alphabetically */
    CORRADE_COMPARE(out.str(), "Trade::OpenExrImporter::image2D(): can't perform automatic mapping for channels named {A, B, G, R}, to either {left.R, left.G, left.B, left.A} or left.Z, provide desired layer and/or channel names in plugin configuration\n");
}

void OpenExrImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgb16f.exr")));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgb16f.exr")));

    /* Shouldn't crash, leak or anything */
}

void OpenExrImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("OpenExrImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(OPENEXRIMPORTER_TEST_DIR, "rgb16f.exr")));

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
