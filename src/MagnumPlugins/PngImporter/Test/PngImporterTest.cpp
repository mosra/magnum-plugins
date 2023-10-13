/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct PngImporterTest: TestSuite::Tester {
    explicit PngImporterTest();

    void empty();
    void invalid();

    void gray();
    void gray16();
    void grayAlpha();
    void rgb();
    void rgb16();
    void rgbPalette1bit();
    void rgba();

    void forceBitDepth8();
    void forceBitDepth16();
    void forceBitDepthInvalid();

    void openMemory();
    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

using namespace Containers::Literals;
using namespace Math::Literals;

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    ImporterFlags flags;
    Containers::StringView data;
    const char* message;
} InvalidData[] {
    {"invalid signature", {},
        "invalid!"_s,
        "error: Not a PNG file"},
    {"short signature", {},
        "\x89PNG"_s,
        "error: file too short"},
    {"invalid signature with trailing zeros", {},
        "\x89PNG\x0d\x0a\x1a\x00"_s,
        "error: PNG file corrupted by ASCII conversion"},
    {"only signature", {},
        "\x89PNG\x0d\x0a\x1a\x0a"_s,
        "error: file too short"},
    {"zero width", {},
        "\x89PNG\x0d\x0a\x1a\x0a"
        "\x00\x00\x00\x0DIHDR"  /* HDR chunk, 13 bytes */
        "\x00\x00\x00\x00"      /* width, big-endian */
        "\x00\x00\x00\x02"      /* height, big-endian */
        "\x08\x04"              /* bit depth, color type */
        "\x00\x00\x00"          /* compression, filter, interlace method */
        "\xdc\x4a\x15\x92"_s,   /* hex(zlib.crc32(b'IHDR...')), big-endian */
        "Trade::PngImporter::image2D(): warning: Image width is zero in IHDR\n"
        "Trade::PngImporter::image2D(): error: Invalid IHDR data\n"},
    {"zero width, quiet", ImporterFlag::Quiet,
        "\x89PNG\x0d\x0a\x1a\x0a"
        "\x00\x00\x00\x0DIHDR"  /* Same as above */
        "\x00\x00\x00\x00"
        "\x00\x00\x00\x02"
        "\x08\x04"
        "\x00\x00\x00"
        "\xdc\x4a\x15\x92"_s,
        "error: Invalid IHDR data"}
};

constexpr struct {
    const char* name;
    const char* filename;
} GrayData[]{
    {"8bit", "gray.png"},
    /* convert gray.png -depth 4 -colorspace gray -define png:bit-depth=4 -define png:exclude-chunks=date gray-4bit.png */
    {"4bit", "gray4.png"},
};

constexpr struct {
    const char* name;
    const char* filename;
} GrayAlphaData[]{
    /* magnum-imageconverter ga-trns.png --converter StbPngImageConverter ga.png
       because imagemagick is STUPID and doesn't let me override the rRNS from
       https://www.imagemagick.org/Usage/formats/#png_quality -- the suggested
       -type TruecolorMatte doesn't do anything and PNG32: gives me a RGBA
       (yes, the source file is the one from below) */
    {"8bit", "ga.png"},
    /* convert rgba.png -colorspace gray ga-trns.png
       According to https://www.imagemagick.org/Usage/formats/#png_quality,
       ImageMagick creates a tRNS chunk if the original image has binary (00
       or FF) alpha. */
    {"tRNS alpha mask", "ga-trns.png"},
};

constexpr struct {
    const char* name;
    const char* filename;
} RgbData[]{
    {"RGB", "rgb.png"},
    /* convert rgb.png -define png:exclude-chunks=date png8:palette.png */
    {"palette", "rgb-palette.png"},
};

constexpr struct {
    const char* name;
    const char* filename;
} RgbaData[]{
    {"RGBA", "rgba.png"},
    /* See README.md for details on how this file was produced */
    {"CgBI BGRA", "rgba-iphone.png"},
    /* convert rgba.png -define png:exclude-chunks=date rgba-trns.png
       According to https://www.imagemagick.org/Usage/formats/#png_quality,
       ImageMagick creates a tRNS chunk if the original image has binary (00
       or FF) alpha. */
    {"tRNS alpha mask", "rgba-trns.png"},
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    const char* filename;
    PixelFormat format;
    Vector2i size;
    Containers::Array<char> data;
    bool verbose;
    const char* message;
} ForceBitDepth8Data[]{
    {"already 8-bit", "rgba.png", PixelFormat::RGBA8Unorm, {3, 2}, {InPlaceInit, {
        /* Same as in rgba() */
        '\xde', '\xad', '\xb5', '\xff',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xde', '\xad', '\xb5', '\xff'
    }}, true, nullptr},
    {"RGB", "rgb16.png", PixelFormat::RGB8Unorm, {2, 3}, {InPlaceInit, {
        '\x04', '\x08', '\x0c',
        '\x08', '\x0c', '\x10',
        '\x0c', '\x10', '\x13',
        '\x10', '\x13', '\x17',
        '\x13', '\x17', '\x1b',
        '\x17', '\x1b', '\x1f'
    }}, false, nullptr},
    {"RGB, verbose", "rgb16.png", PixelFormat::RGB8Unorm, {2, 3}, {InPlaceInit, {
        '\x04', '\x08', '\x0c',
        '\x08', '\x0c', '\x10',
        '\x0c', '\x10', '\x13',
        '\x10', '\x13', '\x17',
        '\x13', '\x17', '\x1b',
        '\x17', '\x1b', '\x1f'
    }}, true, "Trade::PngImporter::image2D(): stripping 16-bit channels to 8-bit\n"},
    {"gray", "gray16.png", PixelFormat::R8Unorm, {2, 3}, {InPlaceInit, {
        4, 8,
        12, 16,
        19, 23
    }}, false, nullptr}
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    const char* filename;
    PixelFormat format;
    Vector2i size;
    Containers::Array<UnsignedShort> data;
    bool verbose;
    const char* message;
} ForceBitDepth16Data[]{
    {"already 16-bit", "gray16.png", PixelFormat::R16Unorm, {2, 3}, {InPlaceInit, {
        /* Same as in gray16() */
        1000, 2000,
        3000, 4000,
        5000, 6000
    }}, true, nullptr},
    {"RGBA", "rgba.png", PixelFormat::RGBA16Unorm, {3, 2}, {InPlaceInit, {
        /* Yes, those pixels are zero in the original as well. Huh. */
        57054, 44461, 46517, 65535,
        51914, 65278, 30583, 65535,
        0, 0, 0, 0,
        51914, 65278, 30583, 65535,
        0, 0, 0, 0,
        57054, 44461, 46517, 65535,
    }}, false, nullptr},
    {"RGBA, verbose", "rgba.png", PixelFormat::RGBA16Unorm, {3, 2}, {InPlaceInit, {
        57054, 44461, 46517, 65535,
        51914, 65278, 30583, 65535,
        0, 0, 0, 0,
        51914, 65278, 30583, 65535,
        0, 0, 0, 0,
        57054, 44461, 46517, 65535,
    }}, true, "Trade::PngImporter::image2D(): expanding 8-bit channels to 16-bit\n"},
    {"gray", "gray.png", PixelFormat::R16Unorm, {3, 2}, {InPlaceInit, {
        65535, 34952, 0,
        34952, 0, 65535
    }}, false, nullptr}
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

PngImporterTest::PngImporterTest() {
    addTests({&PngImporterTest::empty});

    addInstancedTests({&PngImporterTest::invalid},
        Containers::arraySize(InvalidData));

    addInstancedTests({&PngImporterTest::gray},
        Containers::arraySize(GrayData));

    addTests({&PngImporterTest::gray16});

    addInstancedTests({&PngImporterTest::grayAlpha},
        Containers::arraySize(GrayAlphaData));

    addInstancedTests({&PngImporterTest::rgb},
        Containers::arraySize(RgbData));

    addTests({&PngImporterTest::rgb16,
              &PngImporterTest::rgbPalette1bit});

    addInstancedTests({&PngImporterTest::rgba},
        Containers::arraySize(RgbaData));

    addInstancedTests({&PngImporterTest::forceBitDepth8},
        Containers::arraySize(ForceBitDepth8Data));

    addInstancedTests({&PngImporterTest::forceBitDepth16},
        Containers::arraySize(ForceBitDepth16Data));

    addTests({&PngImporterTest::forceBitDepthInvalid});

    addInstancedTests({&PngImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&PngImporterTest::openTwice,
              &PngImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef PNGIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(PNGIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void PngImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");

    std::ostringstream out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out.str(), "Trade::PngImporter::openData(): the file is empty\n");
}

void PngImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->addFlags(data.flags);

    /* The open does just a memory copy, so it doesn't fail */
    CORRADE_VERIFY(importer->openData(data.data));

    std::ostringstream out;
    Warning redirectWarning{&out};
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    /* Sometimes there's also a warning, in that case take the message
       verbatim */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out.str(), data.message);
    else
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::PngImporter::image2D(): {}\n", data.message));
}

void PngImporterTest::gray() {
    auto&& data = GrayData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
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

void PngImporterTest::gray16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
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

void PngImporterTest::grayAlpha() {
    auto&& data = GrayAlphaData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RG8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(image->data().size(), 16);
    image->mutableData()[6] = image->mutableData()[7] =
        image->mutableData()[14] = image->mutableData()[15] = 0;

    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xb8', '\xff', '\xe9', '\xff', '\x00', '\x00', 0, 0,
        '\xe9', '\xff', '\x00', '\x00', '\xb8', '\xff', 0, 0
    }), TestSuite::Compare::Container);
}

void PngImporterTest::rgb() {
    auto&& data = RgbData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
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

void PngImporterTest::rgb16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
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

void PngImporterTest::rgbPalette1bit() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");

    /* convert IcoImporter/Test/icon256x256.png rgb-palette-1bit.png
       (the file is all 0x0000ff, so I guess that makes it a 1-bit palette).
       Taking this file as a repro case because it crashed the importer. */
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb-palette1.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(256, 256));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);

    CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0x0000ff_rgb);
}

void PngImporterTest::rgba() {
    auto&& data = RgbaData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    {
        CORRADE_EXPECT_FAIL_IF(Containers::StringView{data.name}.contains("CgBI"),
            "Stock libPNG can't handle CgBI.");
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

void PngImporterTest::forceBitDepth8() {
    auto&& data = ForceBitDepth8Data[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceBitDepth", 8);
    if(data.verbose) importer->addFlags(ImporterFlag::Verbose);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    std::ostringstream out;
    Containers::Optional<ImageData2D> image;
    {
        Debug redirectOutput{&out};
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image,
        (ImageView2D{PixelStorage{}.setAlignment(1), data.format, data.size, data.data}),
        DebugTools::CompareImage);
    if(data.message)
        CORRADE_COMPARE(out.str(), data.message);
    else
        CORRADE_COMPARE(out.str(), "");
}

void PngImporterTest::forceBitDepth16() {
    auto&& data = ForceBitDepth16Data[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceBitDepth", 16);
    if(data.verbose) importer->addFlags(ImporterFlag::Verbose);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    std::ostringstream out;
    Containers::Optional<ImageData2D> image;
    {
        Debug redirectOutput{&out};
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image,
        (ImageView2D{PixelStorage{}.setAlignment(1), data.format, data.size, data.data}),
        DebugTools::CompareImage);
    if(data.message)
        CORRADE_COMPARE(out.str(), data.message);
    else
        CORRADE_COMPARE(out.str(), "");
}

void PngImporterTest::forceBitDepthInvalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceBitDepth", 4);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgba.png")));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::PngImporter::image2D(): expected forceBitDepth to be 0, 8 or 16 but got 4\n");
}

void PngImporterTest::openMemory() {
    /* Same as gray16() except that it uses openData() & openMemory() instead
       of openFile() to test data copying on import */

    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
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

void PngImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    /* Shouldn't crash, leak or anything */
}

void PngImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
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

CORRADE_TEST_MAIN(Magnum::Trade::Test::PngImporterTest)
