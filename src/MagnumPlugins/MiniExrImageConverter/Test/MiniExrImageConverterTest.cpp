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
#include <Corrade/Containers/Array.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

#include "MagnumPlugins/MiniExrImageConverter/MiniExrImageConverter.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

struct MiniExrImageConverterTest: TestSuite::Tester {
    explicit MiniExrImageConverterTest();

    void wrongFormat();
    void wrongType();

    void rgb();
    void rgba();
};

namespace {
    constexpr const char RgbData[] = {
        /* Skip */
        0, 0, 0, 0, 0, 0, 0, 0,

        1, 2, 3, 2, 3, 4, 0, 0,
        3, 4, 5, 4, 5, 6, 0, 0,
        5, 6, 7, 6, 7, 8, 0, 0
    };

    const ImageView2D Rgb{PixelStorage{}.setSkip({0, 1, 0}),
        PixelFormat::RGB, PixelType::HalfFloat, {1, 3}, RgbData};

    constexpr const char RgbaData[] = {
        1, 2, 3, 2, 3, 4, 9, 9,
        3, 4, 5, 4, 5, 6, 9, 9,
        5, 6, 7, 6, 7, 8, 9, 9
    };

    const ImageView2D Rgba{PixelFormat::RGBA, PixelType::HalfFloat, {1, 3}, RgbaData};
}

MiniExrImageConverterTest::MiniExrImageConverterTest() {
    addTests({&MiniExrImageConverterTest::wrongFormat,
              &MiniExrImageConverterTest::wrongType,

              &MiniExrImageConverterTest::rgb,
              &MiniExrImageConverterTest::rgba});
}

void MiniExrImageConverterTest::wrongFormat() {
    ImageView2D image{
        #if !(defined(MAGNUM_TARGET_WEBGL) && defined(MAGNUM_TARGET_GLES2))
        PixelFormat::Red,
        #else
        PixelFormat::Luminance,
        #endif
        PixelType::HalfFloat, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    const auto data = MiniExrImageConverter{}.exportToData(image);
    CORRADE_VERIFY(!data);
    #if !(defined(MAGNUM_TARGET_WEBGL) && defined(MAGNUM_TARGET_GLES2))
    CORRADE_COMPARE(out.str(), "Trade::MiniExrImageConverter::exportToData(): unsupported pixel format PixelFormat::Red\n");
    #else
    CORRADE_COMPARE(out.str(), "Trade::MiniExrImageConverter::exportToData(): unsupported pixel format PixelFormat::Luminance\n");
    #endif
}

void MiniExrImageConverterTest::wrongType() {
    ImageView2D image{PixelFormat::RGB, PixelType::Float, {}, nullptr};

    std::ostringstream out;
    Error redirectError{&out};

    const auto data = MiniExrImageConverter{}.exportToData(image);
    CORRADE_VERIFY(!data);
    CORRADE_COMPARE(out.str(), "Trade::MiniExrImageConverter::exportToData(): unsupported pixel type PixelType::Float\n");
}

void MiniExrImageConverterTest::rgb() {
    const auto data = MiniExrImageConverter{}.exportToData(Rgb);

    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(MINIEXRIMAGECONVERTER_TEST_DIR, "image.exr"),
        TestSuite::Compare::StringToFile);
}

void MiniExrImageConverterTest::rgba() {
    const auto data = MiniExrImageConverter{}.exportToData(Rgba);

    /* Alpha is ignored, so it is the same file */
    CORRADE_COMPARE_AS((std::string{data, data.size()}),
        Utility::Directory::join(MINIEXRIMAGECONVERTER_TEST_DIR, "image.exr"),
        TestSuite::Compare::StringToFile);
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::MiniExrImageConverterTest)
