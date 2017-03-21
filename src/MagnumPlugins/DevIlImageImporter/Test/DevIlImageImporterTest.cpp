/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
              Vladimír Vondruš <mosra@centrum.cz>
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

#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

#include "MagnumPlugins/DevIlImageImporter/DevIlImageImporter.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

struct DevIlImageImporterTest: TestSuite::Tester {
    explicit DevIlImageImporterTest();

    void grayPng();
    void grayJpeg();
    void rgbPng();
    void rgbJpeg();
    void rgbaPng();

    void useTwice();
};

DevIlImageImporterTest::DevIlImageImporterTest() {
    addTests({&DevIlImageImporterTest::grayPng,
              &DevIlImageImporterTest::grayJpeg,
              &DevIlImageImporterTest::rgbPng,
              &DevIlImageImporterTest::rgbJpeg,
              &DevIlImageImporterTest::rgbaPng,

              &DevIlImageImporterTest::useTwice});
}

void DevIlImageImporterTest::grayPng() {
    DevIlImageImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    #ifndef MAGNUM_TARGET_GLES2
    CORRADE_COMPARE(image->format(), PixelFormat::Red);
    #else
    CORRADE_COMPARE(image->format(), PixelFormat::Luminance);
    #endif
    CORRADE_COMPARE(image->type(), PixelType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), (Containers::Array<char>{Containers::InPlaceInit, {
        '\xff', '\x88', '\x00',
        '\x88', '\x00', '\xff'}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void DevIlImageImporterTest::grayJpeg() {
    DevIlImageImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(JPEGIMPORTER_TEST_DIR, "gray.jpg")));

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    #ifndef MAGNUM_TARGET_GLES2
    CORRADE_COMPARE(image->format(), PixelFormat::Red);
    #else
    CORRADE_COMPARE(image->format(), PixelFormat::Luminance);
    #endif
    CORRADE_COMPARE(image->type(), PixelType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), (Containers::Array<char>{Containers::InPlaceInit, {
        '\xff', '\x88', '\x00',
        '\x88', '\x00', '\xff'}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void DevIlImageImporterTest::rgbPng() {
    DevIlImageImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "rgb.png")));

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB);
    CORRADE_COMPARE(image->type(), PixelType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), (Containers::Array<char>{Containers::InPlaceInit, {
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5'}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void DevIlImageImporterTest::rgbJpeg() {
    DevIlImageImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(JPEGIMPORTER_TEST_DIR, "rgb.jpg")));

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB);
    CORRADE_COMPARE(image->type(), PixelType::UnsignedByte);
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

void DevIlImageImporterTest::rgbaPng() {
    DevIlImageImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "rgba.png")));

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA);
    CORRADE_COMPARE(image->type(), PixelType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), (Containers::Array<char>{Containers::InPlaceInit, {
        '\xde', '\xad', '\xb5', '\xff',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xde', '\xad', '\xb5', '\xff'}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void DevIlImageImporterTest::useTwice() {
    DevIlImageImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    /* Verify that the file is rewound for second use */
    {
        std::optional<Trade::ImageData2D> image = importer.image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
    } {
        std::optional<Trade::ImageData2D> image = importer.image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
    }
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::DevIlImageImporterTest)
