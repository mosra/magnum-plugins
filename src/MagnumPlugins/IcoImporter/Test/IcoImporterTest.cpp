/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2020 bowling-allie <allie.smith.epic@gmail.com>

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
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct IcoImporterTest: TestSuite::Tester {
    explicit IcoImporterTest();

    void tooShort();
    void pngImporterNotFound();
    void pngLoadFailed();

    void bmp();
    void png();

    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

const struct {
    const char* name;
    std::size_t prefix;
    const char* message;
} TooShortData[] {
    {"header too short", 5,
        "file header too short, expected at least 6 bytes but got 5"},
    {"image header too short", 21,
        "image header too short, expected at least 22 bytes but got 21"},
    {"image too short", 973,
        "image too short, expected at least 974 bytes but got 973"}
};

IcoImporterTest::IcoImporterTest() {
    addInstancedTests({&IcoImporterTest::tooShort},
        Containers::arraySize(TooShortData));

    addTests({&IcoImporterTest::pngImporterNotFound,
              &IcoImporterTest::pngLoadFailed,

              &IcoImporterTest::bmp,
              &IcoImporterTest::png,

              &IcoImporterTest::openTwice,
              &IcoImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef ICOIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(ICOIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

using namespace Math::Literals;

void IcoImporterTest::tooShort() {
    auto&& data = TooShortData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Array<char> file = Utility::Directory::read(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "pngs.ico"));
    CORRADE_VERIFY(file);
    CORRADE_COMPARE_AS(file.size(), data.prefix, TestSuite::Compare::Greater);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(file.prefix(data.prefix)));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::IcoImporter::openData(): {}\n", data.message));
}

void IcoImporterTest::pngImporterNotFound() {
    PluginManager::Manager<AbstractImporter> manager{"nonexistent"};
    #ifdef ICOIMPORTER_PLUGIN_FILENAME
    CORRADE_VERIFY(manager.load(ICOIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    if(manager.loadState("PngImporter") & PluginManager::LoadState::Loaded)
        CORRADE_SKIP("PngImporter is available, can't test.");

    Containers::Pointer<AbstractImporter> importer = manager.instantiate("IcoImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "pngs.ico")));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(),
        "PluginManager::Manager::load(): plugin PngImporter is not static and was not found in nonexistent\n"
        "Trade::IcoImporter::image2D(): PngImporter is not available\n");
}

void IcoImporterTest::pngLoadFailed() {
    Containers::Array<char> file = Utility::Directory::read(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "pngs.ico"));
    CORRADE_VERIFY(file);
    /* Break the PNG data, but not the signature, as we need that to detect
       embedded PNGs */
    file[6 + 3*16 + 8] = 'X';

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");
    CORRADE_VERIFY(importer->openData(file));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out.str(), "Trade::StbImageImporter::image2D(): cannot open the image: bad IHDR len\n");
}

void IcoImporterTest::bmp() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "bmp+png.ico")));

    /* Opening the file shouldn't fail -- if we have a mixed bmp+png file, it
       should allow to open at least the PNGs */
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 2);

    /* First is a BMP, should fail */
    {
        std::ostringstream out;
        {
            Error redirectError{&out};
            CORRADE_EXPECT_FAIL("IcoImporter does not support BMPs yet.");
            CORRADE_VERIFY(importer->image2D(0, 0));
        }
        CORRADE_COMPARE(out.str(), "Trade::IcoImporter::image2D(): only files with embedded PNGs are supported\n");

    /* Second is a PNG, should succeed */
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), Vector2i{256});
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0x0000ff_rgb);
    }
}

void IcoImporterTest::png() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "pngs.ico")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 3);

    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), (Vector2i{16, 8}));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0x00ff00_rgb);
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), Vector2i{256});
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0x0000ff_rgb);
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 2);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE(image->size(), (Vector2i{32, 64}));
        CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0xff0000_rgb);
    }
}

void IcoImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "pngs.ico")));
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "pngs.ico")));

    /* Shouldn't crash, leak or anything */
}

void IcoImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "pngs.ico")));

    /* Verify that everything is working the same way on second use */
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{256}));
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, 1);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{256}));
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::IcoImporterTest)
