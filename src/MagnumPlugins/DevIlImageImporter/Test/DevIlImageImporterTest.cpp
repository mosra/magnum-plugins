/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2016 Alice Margatroid <loveoverwhelming@gmail.com>

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
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct DevIlImageImporterTest: TestSuite::Tester {
    explicit DevIlImageImporterTest();

    void fileNotFound();
    void empty();
    void invalid();

    void grayPng();
    void grayJpeg();
    void rgbPng();
    void rgbJpeg();
    void rgbaPng();

    void bgrTga();
    void bgraTga();

    void icoBmp();
    void icoPng();

    void animatedGif();

    void openTwice();
    void importTwice();
    void twoImporters();

    void utf8Filename();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

using namespace Math::Literals;

const struct {
    const char* name;
    bool openFile;
    const char* filename;
    const char* type;
    bool succeeds;
} IcoBmpData[] {
    {"openFile", true, nullptr, nullptr, true},
    {"openFile, unexpected filename", true, "icon.dat", nullptr, false},
    {"openFile, unexpected filename, type override", true, "icon.dat", "0x0424", true},
    {"openData", false, nullptr, nullptr, false},
    {"openData, type override", false, nullptr, "0x0424", true},
};

DevIlImageImporterTest::DevIlImageImporterTest() {
    addTests({&DevIlImageImporterTest::fileNotFound,
              &DevIlImageImporterTest::empty,
              &DevIlImageImporterTest::invalid,

              &DevIlImageImporterTest::grayPng,
              &DevIlImageImporterTest::grayJpeg,
              &DevIlImageImporterTest::rgbPng,
              &DevIlImageImporterTest::rgbJpeg,
              &DevIlImageImporterTest::rgbaPng,

              &DevIlImageImporterTest::bgrTga,
              &DevIlImageImporterTest::bgraTga});

    addInstancedTests({&DevIlImageImporterTest::icoBmp},
        Containers::arraySize(IcoBmpData));

    addTests({&DevIlImageImporterTest::icoPng,

              &DevIlImageImporterTest::animatedGif,

              &DevIlImageImporterTest::openTwice,
              &DevIlImageImporterTest::importTwice,
              &DevIlImageImporterTest::twoImporters,

              &DevIlImageImporterTest::utf8Filename});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef DEVILIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(DEVILIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void DevIlImageImporterTest::fileNotFound() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile("nonexistent"));
    CORRADE_COMPARE(out.str(), "Trade::DevIlImageImporter::openFile(): cannot open the image: 0x50b\n");
}

void DevIlImageImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");

    std::ostringstream out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out.str(), "Trade::DevIlImageImporter::openData(): cannot open the image: 0x509\n");
}

void DevIlImageImporterTest::invalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData("invalid"));
    CORRADE_COMPARE(out.str(), "Trade::DevIlImageImporter::openData(): cannot open the image: 0x512\n");
}

void DevIlImageImporterTest::grayPng() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xff', '\x88', '\x00',
        '\x88', '\x00', '\xff'
    }), TestSuite::Compare::Container);
}

void DevIlImageImporterTest::grayJpeg() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(JPEGIMPORTER_TEST_DIR, "gray.jpg")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xff', '\x88', '\x00',
        '\x88', '\x00', '\xff'
    }), TestSuite::Compare::Container);
}

void DevIlImageImporterTest::rgbPng() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "rgb.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
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

void DevIlImageImporterTest::rgbJpeg() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(JPEGIMPORTER_TEST_DIR, "rgb.jpg")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
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

void DevIlImageImporterTest::rgbaPng() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "rgba.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
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

void DevIlImageImporterTest::bgrTga() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");

    /* Copy of TgaImporterTest::colorBits24() */
    constexpr char data[] = {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3, 0, 24, 0,
        1, 2, 3, 2, 3, 4,
        3, 4, 5, 4, 5, 6,
        5, 6, 7, 6, 7, 8
    };
    CORRADE_VERIFY(importer->openData(data));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        3, 2, 1, 4, 3, 2,
        5, 4, 3, 6, 5, 4,
        7, 6, 5, 8, 7, 6
    }), TestSuite::Compare::Container);
}

void DevIlImageImporterTest::bgraTga() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");

    /* Copy of TgaImporterTest::colorBits32() */
    constexpr char data[] = {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3, 0, 32, 0,
        1, 2, 3, 4, 2, 3, 4, 5,
        3, 4, 5, 6, 4, 5, 6, 7,
        5, 6, 7, 8, 6, 7, 8, 9
    };
    CORRADE_VERIFY(importer->openData(data));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        3, 2, 1, 4, 4, 3, 2, 5,
        5, 4, 3, 6, 6, 5, 4, 7,
        7, 6, 5, 8, 8, 7, 6, 9
    }), TestSuite::Compare::Container);
}

void DevIlImageImporterTest::icoBmp() {
    auto&& data = IcoBmpData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");

    /* Set the type override, if desired. It's a string to test it can
       correctly recognize hexadecimal values. */
    if(data.type)
        importer->configuration().setValue("type", data.type);

    /* Open a file or data -- the ICO format has no magic header or anything,
       so we can use it to test file format autodetection and forcing. */
    std::string filename = Utility::Directory::join(ICOIMPORTER_TEST_DIR, "bmp+png.ico");
    if(data.openFile) {
        /* Copy to a differently named file, if desired */
        if(data.filename) {
            CORRADE_VERIFY(Utility::Directory::copy(filename, Utility::Directory::join(DEVILIMAGEIMPORTER_WRITE_TEST_DIR, data.filename)));
            filename = data.filename;
        }

        CORRADE_COMPARE(importer->openFile(filename), data.succeeds);
    } else CORRADE_COMPARE(importer->openData(Utility::Directory::read(filename)), data.succeeds);
    if(!data.succeeds) return;

    {
        CORRADE_EXPECT_FAIL("DevIL does not report ICO sizes as image levels, but instead as separate images.");
        CORRADE_COMPARE(importer->image2DCount(), 1);
    }
    CORRADE_COMPARE(importer->image2DCount(), 2);

    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
        CORRADE_COMPARE(image->size(), (Vector2i{16, 8}));
        CORRADE_COMPARE(image->pixels<Color4ub>()[0][0], 0x00ff00_rgb);
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
        CORRADE_COMPARE(image->size(), Vector2i{256});

        Color4ub pngColor = image->pixels<Color4ub>()[0][0];
        {
            CORRADE_EXPECT_FAIL_IF(pngColor.a() != 255, "DevIL doesn't correctly import alpha for PNGs embedded in ICO files.");
            CORRADE_COMPARE(pngColor, 0x0000ff_rgb);
        }
        CORRADE_COMPARE(pngColor.rgb(), 0x0000ff_rgb);
    }
}

void DevIlImageImporterTest::icoPng() {
    /* Last checked with version 1.8, May 2020 */
    CORRADE_SKIP("DevIL crashes on some ICOs with embedded PNGs, skipping the test.");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "pngs.ico")));

    {
        CORRADE_EXPECT_FAIL("DevIL does not report ICO sizes as image levels, but instead as separate images.");
        CORRADE_COMPARE(importer->image2DCount(), 1);
    }
    CORRADE_COMPARE(importer->image2DCount(), 3);

    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), (Vector2i{16, 8}));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0x00ff00_rgb);
    }
}

void DevIlImageImporterTest::animatedGif() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");

    /* Basically the same as StbImageImporterTest::animatedGif(), except that
       we don't import image delays here */

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(STBIMAGEIMPORTER_TEST_DIR, "dispose_bgnd.gif")));
    CORRADE_COMPARE(importer->image2DCount(), 5);

    /* All images should have the same format & size */
    for(UnsignedInt i = 0; i != importer->image2DCount(); ++i) {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(i);
        CORRADE_VERIFY(image);
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

void DevIlImageImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png")));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    /* Shouldn't crash, leak or anything */
}

void DevIlImageImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "rgba.png")));

    /* Verify that everything is working the same way on second use and that
       the data are the same -- some APIs (such as iluFlipImage()) mutate the
       original data and we would get a different result every time. */
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
        CORRADE_COMPARE(image->pixels<Color4ub>()[0][0], 0xdeadb5ff_rgba);
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
        CORRADE_COMPARE(image->pixels<Color4ub>()[0][0], 0xdeadb5ff_rgba);
    }
}

void DevIlImageImporterTest::twoImporters() {
    Containers::Pointer<AbstractImporter> a = _manager.instantiate("DevIlImageImporter");
    Containers::Pointer<AbstractImporter> b = _manager.instantiate("DevIlImageImporter");

    CORRADE_VERIFY(a->openFile(Utility::Directory::join(JPEGIMPORTER_TEST_DIR, "rgb.jpg")));
    CORRADE_VERIFY(b->openFile(Utility::Directory::join(STBIMAGEIMPORTER_TEST_DIR, "dispose_bgnd.gif")));

    /* Ask for image A metadata after loading file B to test that the two
       importers don't get their state mixed together */
    CORRADE_COMPARE(a->image2DCount(), 1);
    CORRADE_COMPARE(b->image2DCount(), 5);

    /* Import image A after loading file B to test that the two importers don't
       get their state mixed together */
    Containers::Optional<Trade::ImageData2D> imageA = a->image2D(0);
    Containers::Optional<Trade::ImageData2D> imageB = b->image2D(0);

    /* Colors the same as above */
    CORRADE_VERIFY(imageA);
    CORRADE_COMPARE(imageA->size(), (Vector2i{3, 2}));
    CORRADE_COMPARE(imageA->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE(imageA->pixels<Color3ub>()[0][0], 0xcafe76_rgb);

    CORRADE_VERIFY(imageB);
    CORRADE_COMPARE(imageB->size(), (Vector2i{100, 100}));
    CORRADE_COMPARE(imageB->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE(imageB->pixels<Color4ub>()[0][0], 0x87ceeb_rgb);
}

void DevIlImageImporterTest::utf8Filename() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DevIlImageImporter");

    std::string filename = Utility::Directory::join(DEVILIMAGEIMPORTER_WRITE_TEST_DIR, "hýždě.png");
    CORRADE_VERIFY(Utility::Directory::copy(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png"), filename));
    CORRADE_VERIFY(importer->openFile(filename));

    /* Same as in grayPng() */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xff', '\x88', '\x00',
        '\x88', '\x00', '\xff'
    }), TestSuite::Compare::Container);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::DevIlImageImporterTest)
