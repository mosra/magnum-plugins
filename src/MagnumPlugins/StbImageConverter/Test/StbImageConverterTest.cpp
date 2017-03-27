/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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

#include "MagnumPlugins/StbImageConverter/StbImageConverter.h"
#include "MagnumPlugins/StbImageImporter/StbImageImporter.h"

namespace Magnum { namespace Trade { namespace Test {

struct StbImageConverterTest: TestSuite::Tester {
    explicit StbImageConverterTest();

    void wrongFormat();
    void wrongType();
    void wrongTypeHdr();
    void wrongStorage();

    /** @todo test the string constructor somehow (needs to be properly loaded through plugin manager) */

    void rgBmp();
    void grayscaleHdr();
    void rgbPng();
    void rgbaTga();
};

StbImageConverterTest::StbImageConverterTest() {
    addTests({&StbImageConverterTest::wrongFormat,
              &StbImageConverterTest::wrongType,
              &StbImageConverterTest::wrongTypeHdr,
              &StbImageConverterTest::wrongStorage,

              &StbImageConverterTest::rgBmp,
              &StbImageConverterTest::grayscaleHdr,
              &StbImageConverterTest::rgbPng,
              &StbImageConverterTest::rgbaTga});
}

void StbImageConverterTest::wrongFormat() {
    ImageView2D image{PixelFormat::DepthComponent, PixelType::UnsignedByte, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!StbImageConverter{StbImageConverter::Format::Png}.exportToData(image));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::exportToData(): unsupported pixel format PixelFormat::DepthComponent\n");
}

void StbImageConverterTest::wrongType() {
    ImageView2D image{PixelFormat::RGBA, PixelType::Float, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!StbImageConverter{StbImageConverter::Format::Tga}.exportToData(image));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::exportToData(): PixelType::Float is not supported for BMP/PNG/TGA format\n");
}

void StbImageConverterTest::wrongTypeHdr() {
    ImageView2D image{PixelFormat::RGB, PixelType::UnsignedByte, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!StbImageConverter{StbImageConverter::Format::Hdr}.exportToData(image));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::exportToData(): PixelType::UnsignedByte is not supported for HDR format\n");
}

void StbImageConverterTest::wrongStorage() {
    const ImageView2D image{PixelStorage{}.setSkip({0, 1, 0}),
        PixelFormat::RGB, PixelType::UnsignedByte, {2, 3}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!StbImageConverter{StbImageConverter::Format::Bmp}.exportToData(image));
    CORRADE_COMPARE(out.str(), "Trade::StbImageConverter::exportToData(): data must be tightly packed for all formats except PNG\n");
}

namespace {
    constexpr const char OriginalRgData[] = {
        1, 2, 2, 3,
        3, 4, 4, 5,
        5, 6, 6, 7
    };

    const ImageView2D OriginalRg{
        #if !(defined(MAGNUM_TARGET_WEBGL) && defined(MAGNUM_TARGET_GLES2))
        PixelFormat::RG,
        #else
        PixelFormat::LuminanceAlpha,
        #endif
        PixelType::UnsignedByte, {2, 3}, OriginalRgData};

    constexpr const char ConvertedRgData[] = {
        1, 1, 1, 2, 2, 2,
        3, 3, 3, 4, 4, 4,
        5, 5, 5, 6, 6, 6
    };
}

void StbImageConverterTest::rgBmp() {
    const auto data = StbImageConverter{StbImageConverter::Format::Bmp}.exportToData(OriginalRg);
    CORRADE_VERIFY(data);

    StbImageImporter importer;
    CORRADE_VERIFY(importer.openData(data));
    std::optional<Trade::ImageData2D> converted = importer.image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    /* RG gets expanded to RRR */
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB);
    CORRADE_COMPARE(converted->type(), PixelType::UnsignedByte);
    CORRADE_COMPARE_AS(converted->data(),
        Containers::arrayView(ConvertedRgData), TestSuite::Compare::Container);
}

namespace {
    constexpr const Float OriginalGrayscaleData[] = {
        1.0f, 2.0f,
        3.0f, 4.0f,
        5.0f, 6.0f
    };

    const ImageView2D OriginalGrayscale{
        #if !(defined(MAGNUM_TARGET_WEBGL) && defined(MAGNUM_TARGET_GLES2))
        PixelFormat::Red,
        #else
        PixelFormat::Luminance,
        #endif
        PixelType::Float, {2, 3}, OriginalGrayscaleData};

    constexpr const Float ConvertedGrayscaleData[] = {
        1.0f, 1.0f, 1.0f, 2.0f, 2.0f, 2.0f,
        3.0f, 3.0f, 3.0f, 4.0f, 4.0f, 4.0f,
        5.0f, 5.0f, 5.0f, 6.0f, 6.0f, 6.0f
    };
}

void StbImageConverterTest::grayscaleHdr() {
    const auto data = StbImageConverter{StbImageConverter::Format::Hdr}.exportToData(OriginalGrayscale);
    CORRADE_VERIFY(data);

    StbImageImporter importer;
    CORRADE_VERIFY(importer.openData(data));
    std::optional<Trade::ImageData2D> converted = importer.image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    /* R gets converted to RRR */
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB);
    CORRADE_COMPARE(converted->type(), PixelType::Float);
    CORRADE_COMPARE_AS(Containers::arrayCast<Float>(converted->data()),
        Containers::arrayView(ConvertedGrayscaleData),
        TestSuite::Compare::Container);
}

namespace {
    constexpr const char OriginalRgbData[] = {
        /* Skip */
        0, 0, 0, 0, 0, 0, 0, 0,

        1, 2, 3, 2, 3, 4, 0, 0,
        3, 4, 5, 4, 5, 6, 0, 0,
        5, 6, 7, 6, 7, 8, 0, 0
    };

    const ImageView2D OriginalRgb{PixelStorage{}.setSkip({0, 1, 0}),
        PixelFormat::RGB, PixelType::UnsignedByte, {2, 3}, OriginalRgbData};

    constexpr const char ConvertedRgbData[] = {
        1, 2, 3, 2, 3, 4,
        3, 4, 5, 4, 5, 6,
        5, 6, 7, 6, 7, 8
    };
}

void StbImageConverterTest::rgbPng() {
    const auto data = StbImageConverter{StbImageConverter::Format::Png}.exportToData(OriginalRgb);
    CORRADE_VERIFY(data);

    StbImageImporter importer;
    CORRADE_VERIFY(importer.openData(data));
    std::optional<Trade::ImageData2D> converted = importer.image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGB);
    CORRADE_COMPARE(converted->type(), PixelType::UnsignedByte);
    CORRADE_COMPARE_AS(converted->data(),
        Containers::arrayView(ConvertedRgbData),
        TestSuite::Compare::Container);
}

namespace {
    constexpr const char RgbaData[] = {
        1, 2, 3, 4, 2, 3, 4, 5,
        3, 4, 5, 6, 4, 5, 6, 7,
        5, 6, 7, 8, 6, 7, 8, 9
    };

    const ImageView2D OriginalRgba{PixelFormat::RGBA, PixelType::UnsignedByte, {2, 3}, RgbaData};
}

void StbImageConverterTest::rgbaTga() {
    const auto data = StbImageConverter{StbImageConverter::Format::Tga}.exportToData(OriginalRgba);
    CORRADE_VERIFY(data);

    StbImageImporter importer;
    CORRADE_VERIFY(importer.openData(data));
    std::optional<Trade::ImageData2D> converted = importer.image2D(0);
    CORRADE_VERIFY(converted);

    CORRADE_COMPARE(converted->size(), Vector2i(2, 3));
    CORRADE_COMPARE(converted->format(), PixelFormat::RGBA);
    CORRADE_COMPARE(converted->type(), PixelType::UnsignedByte);
    CORRADE_COMPARE_AS(converted->data(), Containers::arrayView(RgbaData),
        TestSuite::Compare::Container);
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StbImageConverterTest)
