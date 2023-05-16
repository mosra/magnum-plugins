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
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/String.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct SpngImporterTest: TestSuite::Tester {
    explicit SpngImporterTest();

    void empty();
    void invalid();

    void gray();
    void gray16();
    void grayAlpha();
    void rgb();
    void rgb16();
    void rgbPalette1bit();
    void rgba();

    void openMemory();
    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

using namespace Containers::Literals;
using namespace Math::Literals;

/* Unless said otherwise, the input files are the same as in PngImporterTest,
   see comments there for how the files were produced. */

const struct {
    const char* name;
    const char* filename;
    std::size_t sizeOrOffset;
    Containers::Optional<char> data;
    const char* error;
} InvalidData[] {
    {"too short header", "gray.png", 3, {},
        "failed to read the header: end of stream"},
    {"corrupted header chunk", "gray.png", 0x0f, 'Z', /* IHDR -> IHDZ */
        "failed to read the header: missing IHDR chunk"},
    {"can't read tRNS chunk", "ga.png", 0x27, 'O', /* IDAT -> iDOT */
        "failed to get the tRNS chunk: unknown critical chunk"},
    {"corrupted data chunk", "ga.png", 0x29, 0,
        "failed to start decoding: IDAT stream error"},
    {"corrupted data", "gray.png", 0x34, '\xff', /* 0 byte -> 255 */
        "failed to decode a row: IDAT stream error"},
    /* These all all pass while they should fail */
    {"too short data", "gray.png", 0x3c, {},
        nullptr},
    {"corrupted end chunk", "gray.png", 0x42, 'A', /* IEND -> IAND */
        nullptr},
    {"too short end chunk", "gray.png", 0x45, {},
        nullptr},
    {"corrupted end chunk data", "gray.png", 0x45, '\xff',
        nullptr},
};

const struct {
    const char* name;
    const char* filename;
} GrayData[]{
    {"8bit", "gray.png"},
    {"4bit", "gray4.png"},
};

const struct {
    const char* name;
    const char* filename;
} GrayAlphaData[]{
    {"8bit", "ga.png"},
    {"tRNS alpha mask", "ga-trns.png"},
};

const struct {
    const char* name;
    const char* filename;
} RgbData[]{
    {"RGB", "rgb.png"},
    {"palette", "rgb-palette.png"},
};

const struct {
    const char* name;
    const char* filename;
} RgbaData[]{
    {"RGBA", "rgba.png"},
    {"CgBI BGRA", "rgba-iphone.png"},
    {"tRNS alpha mask", "rgba-trns.png"},
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

SpngImporterTest::SpngImporterTest() {
    addTests({&SpngImporterTest::empty});

    addInstancedTests({&SpngImporterTest::invalid},
        Containers::arraySize(InvalidData));

    addInstancedTests({&SpngImporterTest::gray},
        Containers::arraySize(GrayData));

    addTests({&SpngImporterTest::gray16});

    addInstancedTests({&SpngImporterTest::grayAlpha},
        Containers::arraySize(GrayAlphaData));

    addInstancedTests({&SpngImporterTest::rgb},
        Containers::arraySize(RgbData));

    addTests({&SpngImporterTest::rgb16,
              &SpngImporterTest::rgbPalette1bit});

    addInstancedTests({&SpngImporterTest::rgba},
        Containers::arraySize(RgbaData));

    addInstancedTests({&SpngImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&SpngImporterTest::openTwice,
              &SpngImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef SPNGIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(SPNGIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void SpngImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("SpngImporter");

    std::ostringstream out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out.str(), "Trade::SpngImporter::openData(): the file is empty\n");
}

void SpngImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("SpngImporter");

    Containers::Optional<Containers::Array<char>> file = Utility::Path::read(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename));
    CORRADE_VERIFY(file);

    /* Either modify or cut the data. The open does just a memory copy, so it
       doesn't fail. */
    if(data.data) {
        (*file)[data.sizeOrOffset] = *data.data;
        CORRADE_VERIFY(importer->openData(*file));
    } else {
        CORRADE_VERIFY(importer->openData(file->prefix(data.sizeOrOffset)));
    }

    std::ostringstream out;
    Error redirectError{&out};
    {
        CORRADE_EXPECT_FAIL_IF(!data.error, "libspng doesn't treat this as an error.");
        CORRADE_VERIFY(!importer->image2D(0));
    }
    if(data.error) CORRADE_COMPARE(out.str(), Utility::formatString("Trade::SpngImporter::image2D(): {}\n", data.error));
}

void SpngImporterTest::gray() {
    auto&& data = GrayData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("SpngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(image->data().size(), 8);
    image->mutableData()[3] = image->mutableData()[7] = 0;

    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xff', '\x88', '\x00', 0,
        '\x88', '\x00', '\xff', 0
    }), TestSuite::Compare::Container);
}

void SpngImporterTest::gray16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("SpngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray16.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::R16Unorm);

    CORRADE_COMPARE_AS(image->pixels<UnsignedShort>().asContiguous(), Containers::arrayView<UnsignedShort>({
        1000, 2000,
        3000, 4000,
        5000, 6000
    }), TestSuite::Compare::Container);
}

void SpngImporterTest::grayAlpha() {
    auto&& data = GrayAlphaData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("SpngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    {
        /* https://github.com/randy408/libspng/blob/ea6ca5bc18246a338a40b8ae0a55f77928442e28/spng/spng.c#L642-L647 */
        CORRADE_EXPECT_FAIL_IF(!Containers::StringView{data.name}.contains("tRNS"),
            "libspng doesn't implement expansion of 8-bit gray+alpha formats.");
        CORRADE_COMPARE(image->format(), PixelFormat::RG8Unorm);
    }

    if(image->format() == PixelFormat::RG8Unorm) {
        /* The image has four-byte aligned rows, clear the padding to deterministic
        values */
        CORRADE_COMPARE(image->data().size(), 16);
        image->mutableData()[6] = image->mutableData()[7] =
            image->mutableData()[14] = image->mutableData()[15] = 0;

        CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
            '\xb8', '\xff', '\xe9', '\xff', '\x00', '\x00', 0, 0,
            '\xe9', '\xff', '\x00', '\x00', '\xb8', '\xff', 0, 0
        }), TestSuite::Compare::Container);
    } else {
        CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
        /* R is expanded to RRR */
        CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
            '\xb8', '\xb8', '\xb8', '\xff',
                '\xe9', '\xe9', '\xe9', '\xff',
                    '\x00', '\x00', '\x00', '\x00',
            '\xe9', '\xe9', '\xe9', '\xff',
                '\x00', '\x00', '\x00', '\x00',
                    '\xb8', '\xb8', '\xb8', '\xff',
        }), TestSuite::Compare::Container);
    }
}

void SpngImporterTest::rgb() {
    auto&& data = RgbData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("SpngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(image->data().size(), 24);
    image->mutableData()[9] = image->mutableData()[10] =
        image->mutableData()[11] = image->mutableData()[21] =
            image->mutableData()[22] = image->mutableData()[23] = 0;

    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77', 0, 0, 0,

        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5', 0, 0, 0
    }), TestSuite::Compare::Container);
}

void SpngImporterTest::rgb16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("SpngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb16.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB16Unorm);

    CORRADE_COMPARE_AS(image->pixels<Vector3us>().asContiguous(), Containers::arrayView<Vector3us>({
        {1000, 2000, 3000}, {2000, 3000, 4000},
        {3000, 4000, 5000}, {4000, 5000, 6000},
        {5000, 6000, 7000}, {6000, 7000, 8000}
    }), TestSuite::Compare::Container);
}

void SpngImporterTest::rgbPalette1bit() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("SpngImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb-palette1.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(256, 256));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);

    CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0x0000ff_rgb);
}

void SpngImporterTest::rgba() {
    auto&& data = RgbaData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("SpngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    {
        /* https://github.com/randy408/libspng/issues/16 */
        CORRADE_EXPECT_FAIL_IF(Containers::StringView{data.name}.contains("CgBI"),
            "Libspng can't handle CgBI.");
        CORRADE_VERIFY(image);
    }

    if(!image) CORRADE_SKIP("Loading failed, skipping the rest.");

    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(),  Containers::arrayView<char>({
        '\xde', '\xad', '\xb5', '\xff',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xde', '\xad', '\xb5', '\xff'
    }), TestSuite::Compare::Container);
}

void SpngImporterTest::openMemory() {
    /* Same as gray16() except that it uses openData() & openMemory() instead
       of openFile() to test data copying on import */

    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("SpngImporter");
    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray16.png"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*importer, *memory));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::R16Unorm);

    CORRADE_COMPARE_AS(image->pixels<UnsignedShort>().asContiguous(), Containers::arrayView<UnsignedShort>({
        1000, 2000,
        3000, 4000,
        5000, 6000
    }), TestSuite::Compare::Container);
}

void SpngImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("SpngImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    /* Shouldn't crash, leak or anything */
}

void SpngImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("SpngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    /* Verify that everything is working the same way on second use */
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::SpngImporterTest)
