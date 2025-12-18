/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
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
#include <Corrade/Containers/String.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include <avif/avif.h> /* AVIF_VERSION_MAJOR, AVIF_VERSION_MINOR */

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct AvifImporterTest: TestSuite::Tester {
    explicit AvifImporterTest();

    void empty();
    void invalid();

    void gray();
    void gray12();
    void grayAlpha();

    void rgb();
    void rgb10();

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
    std::size_t size;
    const char* error;
} InvalidData[] {
    {"header too short", "rgb.avif", 3,
        "cannot parse file header: BMFF parsing failed: File-level box header: Failed to read 4 bytes, truncated data?\n"},
    {"image too short", "rgb.avif", 334, /* The file is 335 bytes */
        "cannot decode the image: Truncated data: Item ID 1 tried to read 60 bytes, but only received 59 bytes\n"},
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

AvifImporterTest::AvifImporterTest() {
    addTests({&AvifImporterTest::empty});

    addInstancedTests({&AvifImporterTest::invalid},
        Containers::arraySize(InvalidData));

    addTests({&AvifImporterTest::gray,
              &AvifImporterTest::gray12,
              &AvifImporterTest::grayAlpha,

              &AvifImporterTest::rgb,
              &AvifImporterTest::rgb10,

              &AvifImporterTest::rgba});

    addInstancedTests({&AvifImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&AvifImporterTest::openTwice,
              &AvifImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef AVIFIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(AVIFIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void AvifImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AvifImporter");

    Containers::String out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out, "Trade::AvifImporter::openData(): the file is empty\n");
}

void AvifImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AvifImporter");

    Containers::Optional<Containers::Array<char>> in = Utility::Path::read(Utility::Path::join(AVIFIMPORTER_TEST_DIR, data.filename));
    CORRADE_VERIFY(in);

    /* The open does just a memory copy, so it doesn't fail */
    CORRADE_VERIFY(importer->openData(in->prefix(data.size)));

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out, Utility::format("Trade::AvifImporter::image2D(): {}", data.error));
}

void AvifImporterTest::gray() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AvifImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(AVIFIMPORTER_TEST_DIR, "gray.avif")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
    #if AVIF_VERSION_MAJOR*100 + AVIF_VERSION_MINOR >= 103
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);
    #else
    CORRADE_INFO("libavif before 1.3.0 used, grayscale is decoded as RGB");
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
    #endif

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    #if AVIF_VERSION_MAJOR*100 + AVIF_VERSION_MINOR >= 103
    CORRADE_COMPARE(image->data().size(), 8);
    image->mutableData()[3] = image->mutableData()[7] = 0;
    #else
    CORRADE_COMPARE(image->data().size(), 24);
    image->mutableData()[9] = image->mutableData()[10] =
        image->mutableData()[11] = image->mutableData()[21] =
            image->mutableData()[22] = image->mutableData()[23] = 0;
    #endif

    /* Matches PngImporterTest::gray() and thus gray.png exactly */
    #if AVIF_VERSION_MAJOR*100 + AVIF_VERSION_MINOR >= 103
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xff', '\x88', '\x00', 0,
        '\x88', '\x00', '\xff', 0
    }), TestSuite::Compare::Container);
    #else
    /* When imported as RGB it's just the byte expanded three times */
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xff', '\xff', '\xff',
        '\x88', '\x88', '\x88',
        '\x00', '\x00', '\x00', 0, 0, 0,
        '\x88', '\x88', '\x88',
        '\x00', '\x00', '\x00',
        '\xff', '\xff', '\xff', 0, 0, 0
    }), TestSuite::Compare::Container);
    #endif
}

void AvifImporterTest::gray12() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AvifImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(AVIFIMPORTER_TEST_DIR, "gray12.avif")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), (Vector2i{2, 3}));
    #if AVIF_VERSION_MAJOR*100 + AVIF_VERSION_MINOR >= 103
    CORRADE_COMPARE(image->format(), PixelFormat::R16Unorm);
    #else
    CORRADE_INFO("libavif before 1.3.0 used, grayscale is decoded as RGB");
    CORRADE_COMPARE(image->format(), PixelFormat::RGB16Unorm);
    #endif

    /* Unlike all others, does not match PngImporterTest::gray16(). Instead
       avifenc seems to take the input values as-if they'd be in the 12-bit
       range already, i.e. from 0 to 4095. Then the 12-bit input is scaled to
       16 bits (so, multiplied by 16) and clamped to the 16-bit range.

       In comparison, the rgb10() case is taking the input as proper 16-bit
       image and so the output roughly matches. */
    /** @todo looks mainly like a bug in avifenc than anything else, try with a
        different input format or with AvifImageConverter once that's a thing */
    UnsignedShort expected[]{
        #if AVIF_VERSION_MAJOR*100 + AVIF_VERSION_MINOR >= 103
        16*1000, 16*2000,
        16*3000, 16*4000,
        Math::min(16*5000, 65535), Math::min(16*6000, 65535)
        #else
        16*1000, 16*1000, 16*1000,
        16*2000, 16*2000, 16*2000,
        16*3000, 16*3000, 16*3000,
        16*4000, 16*4000, 16*4000,
        65535, 65535, 65535,
        65535, 65535, 65535,
        #endif
    };
    CORRADE_COMPARE_WITH(*image,
        (ImageView2D{image->format(), {2, 3}, expected}),
        (DebugTools::CompareImage{15.0f, 6.2f}));
}

void AvifImporterTest::grayAlpha() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AvifImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(AVIFIMPORTER_TEST_DIR, "ga.avif")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
    #if AVIF_VERSION_MAJOR*100 + AVIF_VERSION_MINOR >= 103
    CORRADE_COMPARE(image->format(), PixelFormat::RG8Unorm);
    #else
    CORRADE_INFO("libavif before 1.3.0 used, gray+alpha is decoded as RGBA");
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    #endif

    /* The RG image has four-byte aligned rows, clear the padding to
       deterministic values */
    #if AVIF_VERSION_MAJOR*100 + AVIF_VERSION_MINOR >= 103
    CORRADE_COMPARE(image->data().size(), 16);
    image->mutableData()[6] = image->mutableData()[7] =
        image->mutableData()[14] = image->mutableData()[15] = 0;
    #endif

    /* Matches PngImporterTest::grayAlpha() and thus ga.png exactly */
    #if AVIF_VERSION_MAJOR*100 + AVIF_VERSION_MINOR >= 103
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\x66', '\x99', '\xcc', '\x00', '\x99', '\x66', 0, 0,
        '\x00', '\x33', '\x33', '\xff', '\xff', '\xcc', 0, 0
    }), TestSuite::Compare::Container);
    #else
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\x66', '\x66', '\x66', '\x99',
        '\xcc', '\xcc', '\xcc', '\x00',
        '\x99', '\x99', '\x99', '\x66',
        '\x00', '\x00', '\x00', '\x33',
        '\x33', '\x33', '\x33', '\xff',
        '\xff', '\xff', '\xff', '\xcc',
    }), TestSuite::Compare::Container);
    #endif
}

void AvifImporterTest::rgb() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AvifImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(AVIFIMPORTER_TEST_DIR, "rgb.avif")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(image->data().size(), 24);
    image->mutableData()[9] = image->mutableData()[10] =
        image->mutableData()[11] = image->mutableData()[21] =
            image->mutableData()[22] = image->mutableData()[23] = 0;

    /* Matches PngImporterTest::rgb() and thus rgb.png exactly */
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77', 0, 0, 0,

        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5', 0, 0, 0
    }), TestSuite::Compare::Container);
}

void AvifImporterTest::rgb10() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AvifImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(AVIFIMPORTER_TEST_DIR, "rgb10.avif")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), (Vector2i{2, 3}));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB16Unorm);

    /* Matches PngImporterTest::rgb16() and thus rgb16.png within a five-bit
       difference, which is completely acceptable given the 10-bit depth */
    Vector3us expected[]{
        {1000, 2000, 3000}, {2000, 3000, 4000},
        {3000, 4000, 5000}, {4000, 5000, 6000},
        {5000, 6000, 7000}, {6000, 7000, 8000}
    };
    CORRADE_COMPARE_WITH(*image,
        (ImageView2D{PixelFormat::RGB16Unorm, {2, 3}, expected}),
        (DebugTools::CompareImage{18.0f, 16.0f}));
}

void AvifImporterTest::rgba() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AvifImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(AVIFIMPORTER_TEST_DIR, "rgba.avif")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);

    /* Matches PngImporterTest::rgba() and thus rgba.png exactly */
    CORRADE_COMPARE_AS(image->data(),  Containers::arrayView<char>({
        /* Matches PngImageConverterTest::rgba() */
        '\x66', '\x33', '\xff', '\x99',
        '\xcc', '\x33', '\xff', '\x00',
        '\x99', '\x33', '\xff', '\x66',
        '\x00', '\xcc', '\xff', '\x33',
        '\x33', '\x66', '\x99', '\xff',
        '\xff', '\x00', '\x33', '\xcc'
    }), TestSuite::Compare::Container);
}

void AvifImporterTest::openMemory() {
    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AvifImporter");
    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(AVIFIMPORTER_TEST_DIR, "rgba.avif"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*importer, *memory));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);

    /* Matches PngImporterTest::rgba() and thus rgba.png exactly */
    CORRADE_COMPARE_AS(image->data(),  Containers::arrayView<char>({
        /* Matches PngImageConverterTest::rgba() */
        '\x66', '\x33', '\xff', '\x99',
        '\xcc', '\x33', '\xff', '\x00',
        '\x99', '\x33', '\xff', '\x66',
        '\x00', '\xcc', '\xff', '\x33',
        '\x33', '\x66', '\x99', '\xff',
        '\xff', '\x00', '\x33', '\xcc'
    }), TestSuite::Compare::Container);
}

void AvifImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AvifImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(AVIFIMPORTER_TEST_DIR, "gray.avif")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(AVIFIMPORTER_TEST_DIR, "gray.avif")));

    /* Shouldn't crash, leak or anything */
}

void AvifImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AvifImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(AVIFIMPORTER_TEST_DIR, "gray.avif")));

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

CORRADE_TEST_MAIN(Magnum::Trade::Test::AvifImporterTest)
