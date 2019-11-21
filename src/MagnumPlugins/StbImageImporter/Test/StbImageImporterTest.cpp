/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
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

#include <sstream>
#include <Corrade/Containers/Optional.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct StbImageImporterTest: TestSuite::Tester {
    explicit StbImageImporterTest();

    void empty();
    void invalid();

    void grayPng();
    void grayJpeg();

    void rgbPng();
    void rgbJpeg();
    void rgbHdr();
    void rgbHdrInvalid();

    void rgbaPng();

    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

const struct {
    const char* name;
    const char* filename;
} RgbaPngTestData[]{
    {"RGBA", "rgba.png"},
    {"CgBI BGRA", "rgba-iphone.png"}
};

StbImageImporterTest::StbImageImporterTest() {
    addTests({&StbImageImporterTest::empty,
              &StbImageImporterTest::invalid,

              &StbImageImporterTest::grayPng,
              &StbImageImporterTest::grayJpeg,

              &StbImageImporterTest::rgbPng,
              &StbImageImporterTest::rgbJpeg,
              &StbImageImporterTest::rgbHdr,
              &StbImageImporterTest::rgbHdrInvalid});

    addInstancedTests({&StbImageImporterTest::rgbaPng}, Containers::arraySize(RgbaPngTestData));

    addTests({&StbImageImporterTest::openTwice,
              &StbImageImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void StbImageImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");

    std::ostringstream out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out.str(), "Trade::StbImageImporter::openData(): the file is empty\n");
}

void StbImageImporterTest::invalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    /* The open does just a memory copy, so it doesn't fail */
    CORRADE_VERIFY(importer->openData("invalid"));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::StbImageImporter::image2D(): cannot open the image: unknown image type\n");
}

void StbImageImporterTest::grayPng() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);
    CORRADE_COMPARE_AS(image->data(), (Containers::Array<char>{Containers::InPlaceInit, {
        '\xff', '\x88', '\x00',
        '\x88', '\x00', '\xff'}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void StbImageImporterTest::grayJpeg() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(JPEGIMPORTER_TEST_DIR, "gray.jpg")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);
    CORRADE_COMPARE_AS(image->data(), (Containers::Array<char>{Containers::InPlaceInit, {
        '\xff', '\x88', '\x00',
        '\x88', '\x00', '\xff'}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void StbImageImporterTest::rgbPng() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "rgb.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image->data(), (Containers::Array<char>{Containers::InPlaceInit, {
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5'}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void StbImageImporterTest::rgbJpeg() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(JPEGIMPORTER_TEST_DIR, "rgb.jpg")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
    /* Data should be similar to the PNG */
    CORRADE_COMPARE_AS(image->data(), (Containers::Array<char>{Containers::InPlaceInit, {
        '\xca', '\xfe', '\x76',
        '\xdf', '\xad', '\xb6',
        '\xca', '\xfe', '\x76',
        '\xe0', '\xad', '\xb6',
        '\xc9', '\xff', '\x76',
        '\xdf', '\xad', '\xb6'}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void StbImageImporterTest::rgbHdr() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STBIMAGEIMPORTER_TEST_DIR, "rgb.hdr")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB32F);
    CORRADE_COMPARE_AS(Containers::arrayCast<const Float>(image->data()),
        (Containers::Array<float>{Containers::InPlaceInit, {
            1.0f, 1.0f, 1.0f, 2.0f, 2.0f, 2.0f,
            3.0f, 3.0f, 3.0f, 4.0f, 4.0f, 4.0f,
            5.0f, 5.0f, 5.0f, 6.0f, 6.0f, 6.0f}}),
        TestSuite::Compare::Container<Containers::ArrayView<const float>>);
}

void StbImageImporterTest::rgbHdrInvalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    /* The open does just a memory copy, so it doesn't fail. Supply just the
       header so the HDR detection succeeds, but the subsequent import fails. */
    CORRADE_VERIFY(importer->openData("#?RADIANCE\n"));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::StbImageImporter::image2D(): cannot open the image: unsupported format\n");
}

void StbImageImporterTest::rgbaPng() {
    auto&& data = RgbaPngTestData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), (Containers::Array<char>{Containers::InPlaceInit, {
        '\xde', '\xad', '\xb5', '\xff',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xde', '\xad', '\xb5', '\xff'}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void StbImageImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png")));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    /* Shouldn't crash, leak or anything */
}

void StbImageImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("StbImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png")));

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

CORRADE_TEST_MAIN(Magnum::Trade::Test::StbImageImporterTest)
