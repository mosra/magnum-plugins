/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2022 Melike Batihan <melikebatihan@gmail.com>

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
#include <Corrade/Containers/String.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct WebPImporterTest: TestSuite::Tester {
    explicit WebPImporterTest();

    void empty();
    void invalid();

    void rgb();
    void rgba();

    void openMemory();
    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

using namespace Math::Literals;

const struct {
    const char* name;
    Containers::String filename;
    Containers::Optional<std::size_t> size;
    const char* error;
} InvalidData[] {
    {"wrong file signature", Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb.png"), {}, "WebP image features not found: bitstream error\n"},
    {"animated file", "animated.webp", {}, "animated WebP images aren't supported\n"},
    /* The header information of a lossless bitstream takes 25 bytes according
       to its specification: https://developers.google.com/speed/webp/docs/webp_lossless_bitstream_specification#2_riff_header.
       Hence, 24 bytes would cause an error while trying to extract the header
       information with WebPGetInfo(). */
    {"too short signature", "rgb-lossless.webp", 24, "WebP image features not found: not enough data\n"},
    /* The file is 54 bytes originally */
    {"too short data", "rgb-lossless.webp", 53, "decoding error: not enough data\n"},
};

constexpr struct {
    const char* name;
    const char* filename;
    Float maxThreshold, meanThreshold;
} RgbData[]{
    {"lossless", "rgb-lossless.webp", 0.0f, 0.0f},
    {"lossy with 90% image quality", "rgb-lossy-90.webp", 41.5f, 27.3f},
    {"lossy with 45% image quality", "rgb-lossy-45.webp", 45.5f, 27.6f},
    {"lossy with 0% image quality", "rgb-lossy-0.webp", 82.9f, 52.3f},
};

constexpr struct {
    const char* name;
    const char* filename;
    Float maxThreshold, meanThreshold;
} RgbaData[]{
    {"lossless", "rgba-lossless.webp", 0.0f, 0.0f},
    {"lossy with 90% image quality", "rgba-lossy-90.webp", 28.95f, 22.2f},
    {"lossy with 45% image quality", "rgba-lossy-45.webp", 28.95f, 22.7f},
    {"lossy with 0% image quality", "rgba-lossy-0.webp", 35.45f, 27.9f},
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

WebPImporterTest::WebPImporterTest() {
    addTests({&WebPImporterTest::empty});

    addInstancedTests({&WebPImporterTest::invalid},
        Containers::arraySize(InvalidData));

    addInstancedTests({&WebPImporterTest::rgb},
        Containers::arraySize(RgbData));

    addInstancedTests({&WebPImporterTest::rgba},
        Containers::arraySize(RgbaData));

    addInstancedTests({&WebPImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&WebPImporterTest::openTwice,
              &WebPImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef WEBPIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(WEBPIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void WebPImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("WebPImporter");

    std::ostringstream out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out.str(), "Trade::WebPImporter::openData(): the file is empty\n");
}

void WebPImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("WebPImporter");

    Containers::Optional<Containers::Array<char>> in = Utility::Path::read(Utility::Path::join(WEBPIMPORTER_TEST_DIR, data.filename));
    CORRADE_VERIFY(in);

    /* The open does just a memory copy, so it doesn't fail */
    CORRADE_VERIFY(importer->openData(data.size ? in->prefix(*data.size) : *in));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::WebPImporter::image2D(): {}", data.error));
}

void WebPImporterTest::rgb() {
    auto&& data = RgbData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("WebPImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(WEBPIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(3, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE(image->storage().alignment(), 4);
    const char expected[] {
        '\x52', '\x52', '\xbe',
        '\x52', '\x52', '\xbe',
        '\x52', '\x52', '\xbe', 0, 0, 0,

        '\xef', '\x91', '\x91',
        '\xef', '\x91', '\x91',
        '\xef', '\x91', '\x91', 0, 0, 0,

        '\x1e', '\x6e', '\x1e',
        '\x1e', '\x6e', '\x1e',
        '\x1e', '\x6e', '\x1e', 0, 0, 0,
    };
    CORRADE_COMPARE_WITH(*image,
        (Magnum::ImageView2D{PixelFormat::RGB8Unorm, {3, 3}, expected}),
        (DebugTools::CompareImage{data.maxThreshold, data.meanThreshold}));
}

void WebPImporterTest::rgba() {
    auto&& data = RgbaData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("WebPImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(WEBPIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(3, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    const char expected[] {
        '\x52', '\x52', '\xbe', '\x80',
        '\x52', '\x52', '\xbe', '\xff',
        '\x52', '\x52', '\xbe', '\x80',

        '\xef', '\x91', '\x91', '\xff',
        '\xef', '\x91', '\x91', '\xff',
        '\xef', '\x91', '\x91', '\xff',

        '\x1e', '\x6e', '\x1e', '\x80',
        '\x1e', '\x6e', '\x1e', '\xff',
        '\x1e', '\x6e', '\x1e', '\x80',
    };
    CORRADE_COMPARE_WITH(*image,
        (Magnum::ImageView2D{PixelFormat::RGBA8Unorm, {3, 3}, expected}),
        (DebugTools::CompareImage{data.maxThreshold, data.meanThreshold}));
}

void WebPImporterTest::openMemory() {
    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("WebPImporter");
    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(WEBPIMPORTER_TEST_DIR, "rgb-lossless.webp"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*importer, *memory));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(3, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
      values */
    CORRADE_COMPARE(image->data().size(), 36);
    image->mutableData()[9] = image->mutableData()[10] =
        image->mutableData()[11] = image->mutableData()[21] =
            image->mutableData()[22] = image->mutableData()[23] =
                image->mutableData()[33] = image->mutableData()[34] =
                    image->mutableData()[35] = 0;
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\x52', '\x52', '\xbe',
        '\x52', '\x52', '\xbe',
        '\x52', '\x52', '\xbe', 0, 0, 0,

        '\xef', '\x91', '\x91',
        '\xef', '\x91', '\x91',
        '\xef', '\x91', '\x91', 0, 0, 0,

        '\x1e', '\x6e', '\x1e',
        '\x1e', '\x6e', '\x1e',
        '\x1e', '\x6e', '\x1e', 0, 0, 0,
    }), TestSuite::Compare::Container);
}

void WebPImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("WebPImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(WEBPIMPORTER_TEST_DIR, "rgb-lossless.webp")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(WEBPIMPORTER_TEST_DIR, "rgb-lossless.webp")));

    /* Shouldn't crash, leak or anything */
}

void WebPImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("WebPImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(WEBPIMPORTER_TEST_DIR, "rgb-lossless.webp")));

    /* Verify that everything is working the same way on second use */
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{3, 3}));
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{3, 3}));
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::WebPImporterTest)
