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

#include <sstream>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

#include "MagnumPlugins/PngImageConverter/PngImageConverter.h"
#include "MagnumPlugins/PngImporter/PngImporter.h"

namespace Magnum { namespace Trade { namespace Test {

struct PngImageConverterTest: TestSuite::Tester {
    explicit PngImageConverterTest();

    void wrongFormat();
    void wrongType();

    void data();
    void data16();
};

namespace {
    constexpr const char OriginalData[] = {
        /* Skip */
        0, 0, 0, 0, 0, 0, 0, 0,

        1, 2, 3, 2, 3, 4, 0, 0,
        3, 4, 5, 4, 5, 6, 0, 0,
        5, 6, 7, 6, 7, 8, 0, 0
    };

    const ImageView2D original{PixelStorage{}.setSkip({0, 1, 0}),
        PixelFormat::RGB, PixelType::UnsignedByte, {2, 3}, OriginalData};

    constexpr const char ConvertedData[] = {
        1, 2, 3, 2, 3, 4, 0, 0,
        3, 4, 5, 4, 5, 6, 0, 0,
        5, 6, 7, 6, 7, 8, 0, 0
    };

    constexpr const UnsignedShort OriginalData16[] = {
        /* Skip */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

        1, 2, 3, 2, 3, 4, 0, 0, 0, 0,
        3, 4, 5, 4, 5, 6, 0, 0, 0, 0,
        5, 6, 7, 6, 7, 8, 0, 0, 0, 0
    };

    const ImageView2D original16{PixelStorage{}.setSkip({0, 1, 0}).setRowLength(3),
        PixelFormat::RGB, PixelType::UnsignedShort, {2, 3}, OriginalData16};

    constexpr const UnsignedShort ConvertedData16[] = {
        1, 2, 3, 2, 3, 4,
        3, 4, 5, 4, 5, 6,
        5, 6, 7, 6, 7, 8
    };
}

PngImageConverterTest::PngImageConverterTest() {
    addTests({&PngImageConverterTest::wrongFormat,
              &PngImageConverterTest::wrongType,

              &PngImageConverterTest::data,
              &PngImageConverterTest::data16});
}

void PngImageConverterTest::wrongFormat() {
    ImageView2D image{PixelFormat::DepthComponent, PixelType::UnsignedByte, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    const auto data = PngImageConverter{}.exportToData(image);
    CORRADE_VERIFY(!data);
    CORRADE_COMPARE(out.str(), "Trade::PngImageConverter::exportToData(): unsupported pixel format PixelFormat::DepthComponent\n");
}

void PngImageConverterTest::wrongType() {
    ImageView2D image{PixelFormat::Red, PixelType::Float, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    const auto data = PngImageConverter{}.exportToData(image);
    CORRADE_VERIFY(!data);
    CORRADE_COMPARE(out.str(), "Trade::PngImageConverter::exportToData(): unsupported pixel type PixelType::Float\n");
}

void PngImageConverterTest::data() {
    const auto data = PngImageConverter{}.exportToData(original);

    PngImporter importer;
    CORRADE_VERIFY(importer.openData(data));
    std::optional<Trade::ImageData2D> converted = importer.image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB);
    CORRADE_COMPARE(converted->type(), PixelType::UnsignedByte);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(converted->data().size(), 24);
    converted->data()[6] = converted->data()[7] = converted->data()[14] =
         converted->data()[15] = converted->data()[22] = converted->data()[23] = 0;

    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(ConvertedData),
        TestSuite::Compare::Container);
}

void PngImageConverterTest::data16() {
    const auto data = PngImageConverter{}.exportToData(original16);

    PngImporter importer;
    CORRADE_VERIFY(importer.openData(data));
    std::optional<Trade::ImageData2D> converted = importer.image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB);
    CORRADE_COMPARE(converted->type(), PixelType::UnsignedShort);
    CORRADE_COMPARE_AS(Containers::arrayCast<UnsignedShort>(converted->data()),
        Containers::arrayView(ConvertedData16),
        TestSuite::Compare::Container);
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::PngImageConverterTest)
