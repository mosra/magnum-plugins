/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
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

#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

struct PngImporterTest: TestSuite::Tester {
    explicit PngImporterTest();

    void gray();
    void rgb();
    void rgba();

    void useTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

namespace {

const struct {
    const char* name;
    const char* filename;
} RgbaTestData[]{
    {"RGBA", "rgba.png"},
    {"CgBI BGRA", "rgba-iphone.png"}
};

}

PngImporterTest::PngImporterTest() {
    addTests({&PngImporterTest::gray,
              &PngImporterTest::rgb});

    addInstancedTests({&PngImporterTest::rgba}, Containers::arraySize(RgbaTestData));

    addTests({&PngImporterTest::useTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef PNGIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(PNGIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void PngImporterTest::gray() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(image->data().size(), 8);
    image->data()[3] = image->data()[7] = 0;

    CORRADE_COMPARE_AS(image->data(),
        (Containers::Array<char>{Containers::InPlaceInit, {
            '\xff', '\x88', '\x00', 0,
            '\x88', '\x00', '\xff', 0}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void PngImporterTest::rgb() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "rgb.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(image->data().size(), 24);
    image->data()[9] = image->data()[10] = image->data()[11] =
         image->data()[21] = image->data()[22] = image->data()[23] = 0;

    CORRADE_COMPARE_AS(image->data(),
        (Containers::Array<char>{Containers::InPlaceInit, {
            '\xca', '\xfe', '\x77',
            '\xde', '\xad', '\xb5',
            '\xca', '\xfe', '\x77', 0, 0, 0,

            '\xde', '\xad', '\xb5',
            '\xca', '\xfe', '\x77',
            '\xde', '\xad', '\xb5', 0, 0, 0}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void PngImporterTest::rgba() {
    auto&& data = RgbaTestData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    {
        CORRADE_EXPECT_FAIL_IF(testCaseInstanceId() == 1,
            "Stock libPNG can't handle CgBI.");
        CORRADE_VERIFY(image);
    }

    if(!image) CORRADE_SKIP("Loading failed, skipping the rest.");

    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(),
        (Containers::Array<char>{Containers::InPlaceInit, {
            '\xde', '\xad', '\xb5', '\xff',
            '\xca', '\xfe', '\x77', '\xff',
            '\x00', '\x00', '\x00', '\x00',
            '\xca', '\xfe', '\x77', '\xff',
            '\x00', '\x00', '\x00', '\x00',
            '\xde', '\xad', '\xb5', '\xff'}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void PngImporterTest::useTwice() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    /* Verify that the file is rewound for second use */
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

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::PngImporterTest)
