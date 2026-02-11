/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
              Vladimír Vondruš <mosra@centrum.cz>

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

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

#ifndef CORRADE_TARGET_EMSCRIPTEN
#include <functional> /* std::ref() */
#include <thread>
#endif

namespace Magnum { namespace Trade { namespace Test { namespace {

struct StbImageImporterTest: TestSuite::Tester {
    explicit StbImageImporterTest();

    void empty();
    void invalid();

    void grayPng();
    void grayPngFourChannel();
    void grayPngFiveChannel();
    void grayPng16();
    void grayPng16FourChannel();
    void grayJpeg();

    void rgbPng();
    void rgbPngOneChannel();
    void rgbPng16();
    void rgbPng16OneChannel();
    void rgbJpeg();
    void rgbHdr();
    void rgbHdrOneChannel();
    void rgbHdrFourChannels();
    void rgbHdrInvalid();

    void rgbaPng();

    void animatedGif();

    void forceBitDepth8();
    void forceBitDepth16();
    void forceBitDepth32();
    void forceBitDepthInvalid();

    void openMemory();
    void openTwice();
    void importTwice();

    #ifndef CORRADE_TARGET_EMSCRIPTEN
    void multithreaded();
    #endif

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

const struct {
    const char* name;
    const char* filename;
} RgbaPngTestData[]{
    {"RGBA", "rgba-binary-alpha.png"},
    {"CgBI BGRA", "rgba-binary-alpha-iphone.png"}
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    Containers::String filename;
    PixelFormat format;
    Vector2i size;
    Containers::Array<char> data;
    bool verbose;
    const char* message;
} ForceBitDepth8Data[]{
    {"already 8-bit", Utility::Path::join(JPEGIMPORTER_TEST_DIR, "rgb.jpg"), PixelFormat::RGB8Unorm, {3, 2}, {InPlaceInit, {
        /* Same as in rgbJpeg() */
        '\xca', '\xfe', '\x76',
        '\xdf', '\xad', '\xb6',
        '\xca', '\xfe', '\x76',
        '\xe0', '\xad', '\xb6',
        '\xc9', '\xff', '\x76',
        '\xdf', '\xad', '\xb6'
    }}, true, nullptr},
    {"16-bit RGB PNG", Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb16.png"), PixelFormat::RGB8Unorm, {2, 3}, {InPlaceInit, {
        /* Same as in the corresponding case in PngImporterTest, stb_image has
           off-by-one differences that are accounted for in CompareImage
           thresholds */
        '\x04', '\x08', '\x0c',
        '\x08', '\x0c', '\x10',
        '\x0c', '\x10', '\x13',
        '\x10', '\x13', '\x17',
        '\x13', '\x17', '\x1b',
        '\x17', '\x1b', '\x1f'
    }}, false, nullptr},
    {"16-bit RGB PNG, verbose", Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb16.png"), PixelFormat::RGB8Unorm, {2, 3}, {InPlaceInit, {
        '\x04', '\x08', '\x0c',
        '\x08', '\x0c', '\x10',
        '\x0c', '\x10', '\x13',
        '\x10', '\x13', '\x17',
        '\x13', '\x17', '\x1b',
        '\x17', '\x1b', '\x1f'
    }}, true, "Trade::StbImageImporter::image2D(): stripping 16-bit channels to 8-bit\n"},
    {"16-bit gray PNG", "gray16.png", PixelFormat::R8Unorm, {2, 3}, {InPlaceInit, {
        4, 8,
        12, 16,
        19, 23
    }}, false, nullptr},
    {"HDR, verbose", Utility::Path::join(STBIMAGEIMPORTER_TEST_DIR, "rgb.hdr"), PixelFormat::RGB8Unorm, {2, 3}, {InPlaceInit, {
        '\x00', '\xba', '\xff',
        '\xff', '\xff', '\xff',

        '\xff', '\xff', '\xff',
        '\xff', '\xba', '\x00',

        '\x00', '\xba', '\xff',
        '\xff', '\xff', '\xff',
    }}, true, "Trade::StbImageImporter::image2D(): stripping 32-bit float channels to 8-bit\n"},
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    Containers::String filename;
    PixelFormat format;
    Vector2i size;
    Containers::Array<UnsignedShort> data;
    bool verbose;
    const char* message;
} ForceBitDepth16Data[]{
    {"already 16-bit", Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray16.png"), PixelFormat::R16Unorm, {2, 3}, {InPlaceInit, {
        /* Same as in grayPng16() */
        1000, 2000,
        3000, 4000,
        5000, 6000
    }}, true, nullptr},
    {"8-bit RGB JPEG", Utility::Path::join(JPEGIMPORTER_TEST_DIR, "rgb.jpg"), PixelFormat::RGB16Unorm, {3, 2}, {InPlaceInit, {
        51914, 65278, 30326,
        57311, 44461, 46774,
        51914, 65278, 30326,
        57568, 44461, 46774,
        51657, 65535, 30326,
        57311, 44461, 46774
    }}, false, nullptr},
    {"8-bit RGB JPEG, verbose", Utility::Path::join(JPEGIMPORTER_TEST_DIR, "rgb.jpg"), PixelFormat::RGB16Unorm, {3, 2}, {InPlaceInit, {
        51914, 65278, 30326,
        57311, 44461, 46774,
        51914, 65278, 30326,
        57568, 44461, 46774,
        51657, 65535, 30326,
        57311, 44461, 46774
    }}, true, "Trade::StbImageImporter::image2D(): expanding 8-bit channels to 16-bit\n"},
    {"8-bit gray PNG", "gray.png", PixelFormat::R16Unorm, {3, 2}, {InPlaceInit, {
        65535, 34952, 0,
        34952, 0, 65535
    }}, false, nullptr},
    {"HDR, verbose", Utility::Path::join(STBIMAGEIMPORTER_TEST_DIR, "rgb.hdr"), PixelFormat::RGB16Unorm, {2, 3}, {InPlaceInit, {
        0, 47802, 65535,
        65535, 65535, 65535,
        65535, 65535, 65535,
        65535, 47802, 0,
        0, 47802, 65535,
        65535, 65535, 65535
    }}, true, "Trade::StbImageImporter::image2D(): stripping 32-bit float channels to 16-bit\n"}
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    Containers::String filename;
    PixelFormat format;
    Vector2i size;
    Containers::Array<Float> data;
    bool verbose;
    const char* message;
} ForceBitDepth32Data[]{
    {"already float", Utility::Path::join(STBIMAGEIMPORTER_TEST_DIR, "rgb.hdr"), PixelFormat::RGB32F, {2, 3}, {InPlaceInit, {
        /* Same as rgbHdr() */
        0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f,
        3.0f, 3.5f, 4.0f, 1.0f, 0.5f, 0.0f,
        0.0f, 0.5f, 1.0f, 4.5f, 5.0f, 5.5f,
    }}, true, nullptr},
    {"8-bit RG PNG", Utility::Path::join(PNGIMPORTER_TEST_DIR, "ga.png"), PixelFormat::RG32F, {3, 2}, {InPlaceInit, {
        0.133209f, 0.6f,
        0.612066f, 0.0f,
        0.325037f, 0.4f,
        0.0f, 0.2f,
        0.0289912f, 1.0f,
        1.0f, 0.8f,
    }}, false, nullptr},
    {"8-bit RG PNG, verbose", Utility::Path::join(PNGIMPORTER_TEST_DIR, "ga.png"), PixelFormat::RG32F, {3, 2}, {InPlaceInit, {
        0.133209f, 0.6f,
        0.612066f, 0.0f,
        0.325037f, 0.4f,
        0.0f, 0.2f,
        0.0289912f, 1.0f,
        1.0f, 0.8f,
    }}, true, "Trade::StbImageImporter::image2D(): expanding 8-bit channels to 32-bit float\n"},
    {"16-bit RGB PNG", Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb16.png"), PixelFormat::RGB32F, {2, 3}, {InPlaceInit, {
        0.0000569f, 0.0003671f, 0.0009924f,
        0.0003671f, 0.0009924f, 0.0019634f,
        0.0009924f, 0.0019634f, 0.0033027f,
        0.0019634f, 0.0033027f, 0.0050282f,
        0.0033027f, 0.0050282f, 0.0071550f,
        0.0050282f, 0.0071550f, 0.0096963f
    }}, true, "Trade::StbImageImporter::image2D(): expanding 16-bit channels to 32-bit float\n"}
};

/* Shared among all plugins that implement data copying optimizations */
const struct {
    const char* name;
    bool(*open)(AbstractImporter&, Containers::ArrayView<const void>);
} OpenMemoryData[]{
    {"data", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        /* Copy to ensure the original memory isn't referenced */
        Containers::Array<char> copy{InPlaceInit, Containers::arrayCast<const char>(data)};
        return importer.openData(copy);
    }},
    {"memory", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        return importer.openMemory(data);
    }},
};

StbImageImporterTest::StbImageImporterTest() {
    addTests({&StbImageImporterTest::empty,
              &StbImageImporterTest::invalid,

              &StbImageImporterTest::grayPng,
              &StbImageImporterTest::grayPngFourChannel,
              &StbImageImporterTest::grayPngFiveChannel,
              &StbImageImporterTest::grayPng16,
              &StbImageImporterTest::grayPng16FourChannel,
              &StbImageImporterTest::grayJpeg,

              &StbImageImporterTest::rgbPng,
              &StbImageImporterTest::rgbPngOneChannel,
              &StbImageImporterTest::rgbPng16,
              &StbImageImporterTest::rgbPng16OneChannel,
              &StbImageImporterTest::rgbJpeg,
              &StbImageImporterTest::rgbHdr,
              &StbImageImporterTest::rgbHdrOneChannel,
              &StbImageImporterTest::rgbHdrFourChannels,
              &StbImageImporterTest::rgbHdrInvalid});

    addInstancedTests({&StbImageImporterTest::rgbaPng}, Containers::arraySize(RgbaPngTestData));

    addTests({&StbImageImporterTest::animatedGif});

    addInstancedTests({&StbImageImporterTest::forceBitDepth8},
        Containers::arraySize(ForceBitDepth8Data));

    addInstancedTests({&StbImageImporterTest::forceBitDepth16},
        Containers::arraySize(ForceBitDepth16Data));

    addInstancedTests({&StbImageImporterTest::forceBitDepth32},
        Containers::arraySize(ForceBitDepth32Data));

    addTests({&StbImageImporterTest::forceBitDepthInvalid});

    addInstancedTests({&StbImageImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&StbImageImporterTest::openTwice,
              &StbImageImporterTest::importTwice});

    #ifndef CORRADE_TARGET_EMSCRIPTEN
    addRepeatedTests({&StbImageImporterTest::multithreaded}, 100);
    #endif

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void StbImageImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");

    Containers::String out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out, "Trade::StbImageImporter::openData(): the file is empty\n");
}

void StbImageImporterTest::invalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    /* The open does just a memory copy, so it doesn't fail */
    CORRADE_VERIFY(importer->openData("invalid"));

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out, "Trade::StbImageImporter::image2D(): cannot open the image: unknown image type\n");
}

void StbImageImporterTest::grayPng() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xff', '\x88', '\x00',
        '\x88', '\x00', '\xff'
    }), TestSuite::Compare::Container);
}

void StbImageImporterTest::grayPngFourChannel() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    importer->configuration().setValue("forceChannelCount", 4);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        /* First channel expanded three times, full alpha */
        '\xff', '\xff', '\xff', '\xff',
        '\x88', '\x88', '\x88', '\xff',
        '\x00', '\x00', '\x00', '\xff',
        '\x88', '\x88', '\x88', '\xff',
        '\x00', '\x00', '\x00', '\xff',
        '\xff', '\xff', '\xff', '\xff'
    }), TestSuite::Compare::Container);
}

void StbImageImporterTest::grayPngFiveChannel() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    importer->configuration().setValue("forceChannelCount", 5);

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out, "Trade::StbImageImporter::image2D(): cannot open the image: bad req_comp\n");
}

void StbImageImporterTest::grayPng16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray16.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::R16Unorm);

    CORRADE_COMPARE_AS(image->pixels<UnsignedShort>().asContiguous(), Containers::arrayView<UnsignedShort>({
        1000, 2000,
        3000, 4000,
        5000, 6000
    }), TestSuite::Compare::Container);
}

void StbImageImporterTest::grayPng16FourChannel() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray16.png")));

    importer->configuration().setValue("forceChannelCount", 4);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA16Unorm);

    CORRADE_COMPARE_AS(Containers::arrayCast<const UnsignedShort>(image->pixels().asContiguous()), Containers::arrayView<UnsignedShort>({
        /* First channel expanded three times, full alpha */
        1000, 1000, 1000, 65535, 2000, 2000, 2000, 65535,
        3000, 3000, 3000, 65535, 4000, 4000, 4000, 65535,
        5000, 5000, 5000, 65535, 6000, 6000, 6000, 65535
    }), TestSuite::Compare::Container);
}

void StbImageImporterTest::grayJpeg() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(JPEGIMPORTER_TEST_DIR, "gray.jpg")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xff', '\x88', '\x00',
        '\x88', '\x00', '\xff'
    }), TestSuite::Compare::Container);
}

void StbImageImporterTest::rgbPng() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5'
    }), TestSuite::Compare::Container);
}

void StbImageImporterTest::rgbPngOneChannel() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb.png")));

    importer->configuration().setValue("forceChannelCount", 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        /* The RGB channels are converted to luminance */
        '\xdf', '\xbc', '\xdf',
        '\xbc', '\xdf', '\xbc',
    }), TestSuite::Compare::Container);
}

void StbImageImporterTest::rgbPng16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb16.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB16Unorm);

    CORRADE_COMPARE_AS(Containers::arrayCast<const UnsignedShort>(image->pixels().asContiguous()), Containers::arrayView<UnsignedShort>({
        1000, 2000, 3000, 2000, 3000, 4000,
        3000, 4000, 5000, 4000, 5000, 6000,
        5000, 6000, 7000, 6000, 7000, 8000
    }), TestSuite::Compare::Container);
}

void StbImageImporterTest::rgbPng16OneChannel() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb16.png")));

    importer->configuration().setValue("forceChannelCount", 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::R16Unorm);

    CORRADE_COMPARE_AS(Containers::arrayCast<const UnsignedShort>(image->pixels().asContiguous()), Containers::arrayView<UnsignedShort>({
        /* The RGB channels are converted to luminance... though with an 8-bit
           equation so I have no idea what the heck is happening */
        1812, 2812,
        3812, 4812,
        5812, 6812
    }), TestSuite::Compare::Container);
}

void StbImageImporterTest::rgbJpeg() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(JPEGIMPORTER_TEST_DIR, "rgb.jpg")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
    /* Data should be similar to the PNG */
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xca', '\xfe', '\x76',
        '\xdf', '\xad', '\xb6',
        '\xca', '\xfe', '\x76',
        '\xe0', '\xad', '\xb6',
        '\xc9', '\xff', '\x76',
        '\xdf', '\xad', '\xb6'
    }), TestSuite::Compare::Container);
}

void StbImageImporterTest::rgbHdr() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(STBIMAGEIMPORTER_TEST_DIR, "rgb.hdr")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB32F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(image->data()),
        Containers::arrayView<float>({
            0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f,
            3.0f, 3.5f, 4.0f, 1.0f, 0.5f, 0.0f,
            0.0f, 0.5f, 1.0f, 4.5f, 5.0f, 5.5f,
        }), TestSuite::Compare::Container);
}

void StbImageImporterTest::rgbHdrOneChannel() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");

    importer->configuration().setValue("forceChannelCount", 1);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(STBIMAGEIMPORTER_TEST_DIR, "rgb.hdr")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::R32F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(image->data()),
        Containers::arrayView<Float>({
            0.5f, 2.0f,
            3.5f, 0.5f,
            0.5f, 5.0f
        }), TestSuite::Compare::Container);
}

void StbImageImporterTest::rgbHdrFourChannels() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");

    importer->configuration().setValue("forceChannelCount", 4);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(STBIMAGEIMPORTER_TEST_DIR, "rgb.hdr")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA32F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(image->data()),
        Containers::arrayView<Float>({
            /* Implicit full alpha */
            0.0f, 0.5f, 1.0f, 1.0f, 1.5f, 2.0f, 2.5f, 1.0f,
            3.0f, 3.5f, 4.0f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f,
            0.0f, 0.5f, 1.0f, 1.0f, 4.5f, 5.0f, 5.5f, 1.0f
        }), TestSuite::Compare::Container);
}

void StbImageImporterTest::rgbHdrInvalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    /* The open does just a memory copy, so it doesn't fail. Supply just the
       header so the HDR detection succeeds, but the subsequent import fails. */
    CORRADE_VERIFY(importer->openData("#?RADIANCE\n"));

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out, "Trade::StbImageImporter::image2D(): cannot open the image: unsupported format\n");
}

void StbImageImporterTest::rgbaPng() {
    auto&& data = RgbaPngTestData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xde', '\xad', '\xb5', '\xff',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xde', '\xad', '\xb5', '\xff'
    }), TestSuite::Compare::Container);
}

void StbImageImporterTest::animatedGif() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(STBIMAGEIMPORTER_TEST_DIR, "dispose_bgnd.gif")));
    CORRADE_COMPARE(importer->image2DCount(), 5);

    /* Importer state should expose the delays, in milliseconds */
    CORRADE_VERIFY(importer->importerState());
    CORRADE_COMPARE_AS(
        Containers::arrayView(reinterpret_cast<const Int*>(importer->importerState()), importer->image2DCount()),
        Containers::arrayView<Int>({1000, 1000, 1000, 1000, 1000}),
        TestSuite::Compare::Container);

    /* All images should have the same format & size */
    for(UnsignedInt i = 0; i != importer->image2DCount(); ++i) {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(i);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->flags(), ImageFlags2D{});
        CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
        CORRADE_COMPARE(image->size(), Vector2i(100, 100));
    }

    /* Second frame should have a pixel on top left a different kind of blue
       than the first */
    {
        using namespace Math::Literals;

        Containers::Optional<Trade::ImageData2D> image0 = importer->image2D(0);
        Containers::Optional<Trade::ImageData2D> image1 = importer->image2D(1);
        CORRADE_VERIFY(image0);
        CORRADE_VERIFY(image1);

        /* Uncomment for debugging purposes */
        //Debug{} << Debug::color << Debug::packed << image1->pixels<Color4ub>().every({3, 3}).flipped<0>();

        CORRADE_COMPARE(image0->pixels<Color4ub>()[88][30], 0x87ceeb_rgb);
        CORRADE_COMPARE(image1->pixels<Color4ub>()[88][30], 0x0000ff_rgb);
    }
}

void StbImageImporterTest::forceBitDepth8() {
    auto&& data = ForceBitDepth8Data[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceBitDepth", 8);
    if(data.verbose) importer->addFlags(ImporterFlag::Verbose);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::String out;
    Containers::Optional<ImageData2D> image;
    {
        Debug redirectOutput{&out};
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_WITH(*image,
        (ImageView2D{PixelStorage{}.setAlignment(1), data.format, data.size, data.data}),
        /* To account for rounding differences between stb_image and libpng */
        (DebugTools::CompareImage{1.0f, 0.67f}));
    if(data.message)
        CORRADE_COMPARE(out, data.message);
    else
        CORRADE_COMPARE(out, "");
}

void StbImageImporterTest::forceBitDepth16() {
    auto&& data = ForceBitDepth16Data[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceBitDepth", 16);
    if(data.verbose) importer->addFlags(ImporterFlag::Verbose);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::String out;
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
        CORRADE_COMPARE(out, data.message);
    else
        CORRADE_COMPARE(out, "");
}

void StbImageImporterTest::forceBitDepth32() {
    auto&& data = ForceBitDepth32Data[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceBitDepth", 32);
    if(data.verbose) importer->addFlags(ImporterFlag::Verbose);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::String out;
    Containers::Optional<ImageData2D> image;
    {
        Debug redirectOutput{&out};
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_WITH(*image,
        (ImageView2D{PixelStorage{}.setAlignment(1), data.format, data.size, data.data}),
        /* Fuzzy comparison for floats */
        (DebugTools::CompareImage{1.0e-6f, 1.0e-6f}));
    if(data.message)
        CORRADE_COMPARE(out, data.message);
    else
        CORRADE_COMPARE(out, "");
}

void StbImageImporterTest::forceBitDepthInvalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceBitDepth", 4);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgba-binary-alpha.png")));

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out, "Trade::StbImageImporter::image2D(): expected forceBitDepth to be 0, 8, 16 or 32 but got 4\n");
}

void StbImageImporterTest::openMemory() {
    /* Same as grayPng() except that it uses openData() & openMemory() instead
       of openFile() to test data copying on import */

    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*importer, *memory));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xff', '\x88', '\x00',
        '\x88', '\x00', '\xff'
    }), TestSuite::Compare::Container);
}

void StbImageImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    /* Shouldn't crash, leak or anything */
}

void StbImageImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
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

#ifndef CORRADE_TARGET_EMSCRIPTEN
void StbImageImporterTest::multithreaded() {
    #ifndef CORRADE_BUILD_MULTITHREADED
    CORRADE_SKIP("CORRADE_BUILD_MULTITHREADED is not enabled.");
    #endif

    Containers::Pointer<AbstractImporter> a = _manager.instantiate("StbImageImporter");
    Containers::Pointer<AbstractImporter> b = _manager.instantiate("StbImageImporter");

    int counterA = 0, counterB = 0;
    {
        constexpr const char data[1]{};
        auto fn = [&](AbstractImporter& importer, int& counter) {
            for(std::size_t i = 0; i != 1000; ++i) {
                /* This function should call stbi_convert_iphone_png_to_rgb()
                   etc. internally, which accesses global (or thread-local)
                   state. If that changes, the test needs to be updated. */
                importer.openData(data);
                ++counter;
            }
        };

        std::thread threadA{fn, std::ref(*a), std::ref(counterA)};
        std::thread threadB{fn, std::ref(*b), std::ref(counterB)};

        threadA.join();
        threadB.join();
    }

    CORRADE_COMPARE(counterA, 1000);
    CORRADE_COMPARE(counterB, 1000);
}
#endif

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StbImageImporterTest)
