/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015
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
#include <Magnum/ColorFormat.h>
#include <Magnum/Trade/ImageData.h>

#include "MagnumPlugins/PngImporter/PngImporter.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

struct PngImporterTest: TestSuite::Tester {
    explicit PngImporterTest();

    void gray();
    void rgb();
    void rgba();
};

PngImporterTest::PngImporterTest() {
    addTests({&PngImporterTest::gray,
              &PngImporterTest::rgb,
              &PngImporterTest::rgba});
}

void PngImporterTest::gray() {
    PngImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    #ifndef MAGNUM_TARGET_GLES2
    CORRADE_COMPARE(image->format(), ColorFormat::Red);
    #else
    CORRADE_COMPARE(image->format(), ColorFormat::Luminance);
    #endif
    CORRADE_COMPARE(image->type(), ColorType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), Containers::Array<char>::from(
        '\xff', '\x88', '\x00',
        '\x88', '\x00', '\xff'),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void PngImporterTest::rgb() {
    PngImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "rgb.png")));

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), ColorFormat::RGB);
    CORRADE_COMPARE(image->type(), ColorType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), Containers::Array<char>::from(
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5'),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void PngImporterTest::rgba() {
    PngImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(PNGIMPORTER_TEST_DIR, "rgba.png")));

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), ColorFormat::RGBA);
    CORRADE_COMPARE(image->type(), ColorType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), Containers::Array<char>::from(
        '\xde', '\xad', '\xb5', '\xff',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xde', '\xad', '\xb5', '\xff'),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::PngImporterTest)
