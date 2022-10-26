/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/ConfigurationValue.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct StbResizeImageConverterTest: TestSuite::Tester {
    explicit StbResizeImageConverterTest();

    void emptySize();
    void emptyInputImage();
    void emptyOutputImage();
    void unsupportedFormat();
    void invalidFilter();
    void invalidEdge();
    void array1D();

    /* Just random variants to cover all component counts, formats and pixel
       storage properties. Options checked in the instanced rgba8Srgb() test. */
    void rgb8Padded();
    void rgba8();
    void rg16();
    void r32f();

    void threeDimensions();
    void array2D();

    void upsample();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};
};

using namespace Math::Literals;

/* Again just random variants to ensure the options get actually used. */
const struct {
    const char* name;
    PixelFormat format;
    Containers::Optional<bool> alphaPremultiplied;
    Containers::Optional<bool> alphaUsesSrgb;
    Containers::StringView edge;
    Containers::StringView filter;
    Color4ub expected[2];
} Rgba8Data[]{
    {"", PixelFormat::RGBA8Unorm, {}, {}, {}, {},
        /* RGB values not same as in rgb8Padded(), because the RGB is
           multiplied with the alpha before filtering. The "premultiplied
           alpha" case is, though. */
        {0xc450849a_rgba, 0x8eb2b467_rgba}},
    {"sRGB", PixelFormat::RGBA8Srgb, {}, {}, {}, {},
        {0xda5d9a9a_rgba, 0xa8bac567_rgba}},
    {"premultiplied alpha", PixelFormat::RGBA8Unorm, true, {}, {}, {},
        /* RGB values same as in rgb8Padded() */
        {0xba4d779a_rgba, 0x99c3aa67_rgba}},
    {"alpha uses sRGB", PixelFormat::RGBA8Unorm, {}, true, {}, {},
        /* Same as the linear case -- alphaUsesSrgb does nothing here */
        {0xc450849a_rgba, 0x8eb2b467_rgba}},
    {"sRGB, alpha uses sRGB", PixelFormat::RGBA8Srgb, {}, true, {}, {},
        /* RGB values not same as in "sRGB" because the alpha gets interpreted
           and thus premultiplied differently */
        {0xe25a9bb6_rgba, 0x90acdb76_rgba}},
    {"edge wrap", PixelFormat::RGBA8Unorm, {}, {}, "wrap", {},
        {0xc05b8890_rgba, 0x9d9ba76f_rgba}},
    {"box filter", PixelFormat::RGBA8Unorm, {}, {}, {}, "box",
        {0xc64f8299_rgba, 0x91b3b366_rgba}},
};

const struct {
    const char* name;
    Containers::Optional<bool> upsample;
    Vector2i inputSize;
    Vector2i targetSize;
    Vector2i expectedSize;
    Color3ub expected[4];
} UpsampleData[]{
    {"downsample on X, upsample on Y",
        {}, {3, 1}, {2, 2}, {2, 2},
        {0xff4353_rgb, 0x9bca96_rgb,
         0xff4353_rgb, 0x9bca96_rgb}},
    {"downsample on Y, upsample on X",
        {}, {1, 3}, {2, 2}, {2, 2},
        {0xff4353_rgb, 0xff4353_rgb,
         0x9bca96_rgb, 0x9bca96_rgb}},
    {"downsample on X, upsample on Y disabled",
        false, {3, 1}, {2, 2}, {2, 1},
        {0xff4353_rgb, 0x9bca96_rgb, {}, {}}},
    {"downsample on Y, upsample on X disabled",
        false, {1, 3}, {2, 2}, {1, 2},
        {0xff4353_rgb, 0x9bca96_rgb, {}, {}}},
    {"direct copy, no size change",
        {}, {3, 1}, {3, 1}, {3, 1},
        {0xff3366_rgb, 0xff6633_rgb, 0x66ffcc_rgb, {}}},
    {"direct copy, upsample on XY disabled",
        /* It shouldn't attempt to allocate the whole target size if it's not
           used */
        false, {3, 1}, {0xfffffff, 0xfffffff}, {3, 1},
        {0xff3366_rgb, 0xff6633_rgb, 0x66ffcc_rgb, {}}},
};

StbResizeImageConverterTest::StbResizeImageConverterTest() {
    addTests({&StbResizeImageConverterTest::emptySize,
              &StbResizeImageConverterTest::emptyInputImage,
              &StbResizeImageConverterTest::emptyOutputImage,
              &StbResizeImageConverterTest::unsupportedFormat,
              &StbResizeImageConverterTest::invalidFilter,
              &StbResizeImageConverterTest::invalidEdge,
              &StbResizeImageConverterTest::array1D,

              &StbResizeImageConverterTest::rgb8Padded});

    addInstancedTests({&StbResizeImageConverterTest::rgba8},
        Containers::arraySize(Rgba8Data));

    addTests({&StbResizeImageConverterTest::rg16,
              &StbResizeImageConverterTest::r32f,

              &StbResizeImageConverterTest::threeDimensions,
              &StbResizeImageConverterTest::array2D});

    addInstancedTests({&StbResizeImageConverterTest::upsample},
        Containers::arraySize(UpsampleData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STBRESIZEIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(STBRESIZEIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void StbResizeImageConverterTest::emptySize() {
    char data[4]{};
    ImageView2D image{PixelFormat::RGBA8Unorm, {1, 1}, data};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbResizeImageConverter::convert(): output size was not specified\n");
}

void StbResizeImageConverterTest::emptyInputImage() {
    ImageView2D image{PixelFormat::RGBA8Unorm, {0, 1}, nullptr};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");
    converter->configuration().setValue("size", Vector2i{1, 1});

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbResizeImageConverter::convert(): invalid input image size {0, 1}\n");
}

void StbResizeImageConverterTest::emptyOutputImage() {
    char input[4]{};
    ImageView2D image{PixelFormat::RG8Srgb, {1, 1}, input};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");
    converter->configuration().setValue("size", (Vector2i{1, 0}));

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbResizeImageConverter::convert(): invalid output image size {1, 0}\n");
}

void StbResizeImageConverterTest::unsupportedFormat() {
    char input[4]{};
    ImageView2D image{PixelFormat::RGBA8UI, {1, 1}, input};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");
    converter->configuration().setValue("size", Vector2i{1, 1});

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbResizeImageConverter::convert(): unsupported format PixelFormat::RGBA8UI\n");
}

void StbResizeImageConverterTest::invalidFilter() {
    char input[4]{};
    ImageView2D image{PixelFormat::RGBA8Unorm, {1, 1}, input};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");
    converter->configuration().setValue("size", Vector2i{1, 1});
    converter->configuration().setValue("filter", "trilinear");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbResizeImageConverter::convert(): expected filter to be empty or one of box, triangle, cubicpline, catmullrom or mitchell, got trilinear\n");
}

void StbResizeImageConverterTest::invalidEdge() {
    char input[4]{};
    ImageView2D image{PixelFormat::RGBA8Unorm, {1, 1}, input};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");
    converter->configuration().setValue("size", Vector2i{1, 1});
    converter->configuration().setValue("edge", "cramp");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbResizeImageConverter::convert(): expected edge mode to be one of clamp, reflect, wrap or zero, got cramp\n");
}

void StbResizeImageConverterTest::array1D() {
    char data[4]{};
    ImageView2D image{PixelFormat::RGBA8Unorm, {1, 1}, data, ImageFlag2D::Array};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");
    converter->configuration().setValue("size", Vector2i{1, 1});

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbResizeImageConverter::convert(): 1D array images are not supported\n");
}

void StbResizeImageConverterTest::rgb8Padded() {
    const Color3ub input[]{
        {}, {}, {}, {}, {}, {},
        0xff3366_rgb, 0xff6633_rgb, 0x66ffcc_rgb, {}, {}, {},
        0x993366_rgb, 0x3399ff_rgb, 0xcccc99_rgb, {}, {}, {}
    };
    const Color3ub expected[]{
        0xba4d77_rgb, 0x99c3aa_rgb
    };

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");
    converter->configuration().setValue("size", (Vector2i{2, 1}));

    Containers::Optional<ImageData2D> out = converter->convert(ImageView2D{
        PixelStorage{}.setAlignment(1).setSkip({0, 1, 0}).setRowLength(6),
        PixelFormat::RGB8Unorm, {3, 2}, input});
    CORRADE_VERIFY(out);
    /* The image should have a four-byte alignment always */
    CORRADE_COMPARE(out->storage().alignment(), 4);
    CORRADE_COMPARE_AS(*out,
        (ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::RGB8Unorm, {2, 1}, expected}),
        DebugTools::CompareImage);
}

void StbResizeImageConverterTest::rgba8() {
    auto&& data = Rgba8Data[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    const Color4ub input[]{
        /* Like with rgb8Padded, but rotated and alpha added, thus the first
           test case should also have the exact same output in RGB channels */
        0xff3366ff_rgba, 0x99336633_rgba,
        0xff663366_rgba, 0x3399ffcc_rgba,
        0x66ffcc33_rgba, 0xcccc9966_rgba
    };

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");
    converter->configuration().setValue("size", (Vector2i{1, 2}));
    if(data.alphaPremultiplied)
        converter->configuration().setValue("alphaPremultiplied", *data.alphaPremultiplied);
    if(data.alphaUsesSrgb)
        converter->configuration().setValue("alphaUsesSrgb", *data.alphaUsesSrgb);
    if(data.edge)
        converter->configuration().setValue("edge", data.edge);
    if(data.filter)
        converter->configuration().setValue("filter", data.filter);

    Containers::Optional<ImageData2D> out = converter->convert(ImageView2D{PixelStorage{}.setAlignment(1), data.format, {2, 3}, input, ImageFlag2D(0xdea0)});
    CORRADE_VERIFY(out);
    /* Flags should be passed through unchanged */
    CORRADE_COMPARE(out->flags(), ImageFlag2D(0xdea0));
    CORRADE_COMPARE_WITH(*out,
        (ImageView2D{data.format, {1, 2}, data.expected}),
        /* There's a slight difference between debug and release build (haha),
           allow that */
        (DebugTools::CompareImage{0.25f, 0.125f}));
}

void StbResizeImageConverterTest::rg16() {
    /* Like rgb8Padded(), but expanded to 16 bits and dropping the B channel */
    const Vector2us input[]{
        {0xffff, 0x3333}, {0xffff, 0x6666}, {0x6666, 0xffff},
        {0x9999, 0x3333}, {0x3333, 0x9999}, {0xcccc, 0xcccc}
    };
    const Vector2us expected[]{
        /* {0xba, 0x4d} and {0x99, 0xc3} was in the 8-bit case */
        {0xbb05, 0x4ceb}, {0x9920, 0xc38d}
    };

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");
    converter->configuration().setValue("size", (Vector2i{2, 1}));

    Containers::Optional<ImageData2D> out = converter->convert(ImageView2D{
        PixelStorage{}.setAlignment(1),
        PixelFormat::RG16Unorm, {3, 2}, input});
    CORRADE_VERIFY(out);
    CORRADE_COMPARE_AS(*out,
        (ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::RG16Unorm, {2, 1}, expected}),
        DebugTools::CompareImage);
}

void StbResizeImageConverterTest::r32f() {
    /* Like rgb8Padded(), but converted the R channel to floats */
    const Float input[]{
        Math::unpack<Float, UnsignedByte>(0xff),
            Math::unpack<Float, UnsignedByte>(0xff),
                Math::unpack<Float, UnsignedByte>(0x66),
        Math::unpack<Float, UnsignedByte>(0x99),
            Math::unpack<Float, UnsignedByte>(0x33),
                Math::unpack<Float, UnsignedByte>(0xcc)
    };
    const Float expected[]{
        /* 0xba (0.729412), 0x99 (0.6) was in the 8-bit case */
        0.730556f, 0.598148f
    };

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");
    converter->configuration().setValue("size", (Vector2i{2, 1}));

    Containers::Optional<ImageData2D> out = converter->convert(ImageView2D{
        PixelFormat::R32F, {3, 2}, input});
    CORRADE_VERIFY(out);
    /* The image should have a four-byte alignment always */
    CORRADE_COMPARE(out->storage().alignment(), 4);
    CORRADE_COMPARE_WITH(*out,
        (ImageView2D{PixelFormat::R32F, {2, 1}, expected}),
        (DebugTools::CompareImage{1.0e-6f, 1.0e-6f}));
}

void StbResizeImageConverterTest::threeDimensions() {
    char data[4]{};
    ImageView3D image{PixelFormat::RGBA8Unorm, {1, 1, 1}, data};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");
    converter->configuration().setValue("size", Vector2i{1, 1});

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convert(image));
    CORRADE_COMPARE(out.str(), "Trade::StbResizeImageConverter::convert(): 3D images are not supported\n");
}

void StbResizeImageConverterTest::array2D() {
    /* Same as rgb8Padded(), except that there's two layers with the second one
       flipped so the second should have the output reversed. No cross-layer
       filtering should happen. */
    const Color3ub input[]{
        0xff3366_rgb, 0xff6633_rgb, 0x66ffcc_rgb,
        0x993366_rgb, 0x3399ff_rgb, 0xcccc99_rgb,
        {}, {}, {},

        0xcccc99_rgb, 0x3399ff_rgb, 0x993366_rgb,
        0x66ffcc_rgb, 0xff6633_rgb, 0xff3366_rgb,
        {}, {}, {},
    };
    const Color3ub expected[]{
        0xba4d77_rgb, 0x99c3aa_rgb,
        0x99c3aa_rgb, 0xba4d77_rgb,
    };

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");
    converter->configuration().setValue("size", (Vector2i{2, 1}));

    Containers::Optional<ImageData3D> out = converter->convert(ImageView3D{
        PixelStorage{}.setAlignment(1).setImageHeight(3),
        PixelFormat::RGB8Unorm, {3, 2, 2}, input, ImageFlag3D(0xdea0)|ImageFlag3D::Array});
    CORRADE_VERIFY(out);
    /* Flags should be passed through unchanged */
    CORRADE_COMPARE(out->flags(), ImageFlag3D(0xdea0)|ImageFlag3D::Array);
    CORRADE_COMPARE(out->format(), PixelFormat::RGB8Unorm);
    for(std::size_t i: {0, 1}) {
        CORRADE_ITERATION(i);
        /** @todo 3D support in CompareImage, ugh */
        CORRADE_COMPARE_AS(out->pixels<Color3ub>()[i],
            (ImageView2D{PixelStorage{}.setAlignment(1), PixelFormat::RGB8Unorm, {2, 1}, Containers::arrayView(expected).exceptPrefix(i*2)}),
            DebugTools::CompareImage);
    }
}

void StbResizeImageConverterTest::upsample() {
    auto&& data = UpsampleData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    const Color3ub input[]{
        0xff3366_rgb, 0xff6633_rgb, 0x66ffcc_rgb
    };

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("StbResizeImageConverter");

    if(data.upsample)
        converter->configuration().setValue("upsample", *data.upsample);

    converter->configuration().setValue("size", data.targetSize);

    Containers::Optional<ImageData2D> out = converter->convert(ImageView2D{
        PixelStorage{}.setAlignment(1),
        PixelFormat::RGB8Unorm, data.inputSize, input});
    CORRADE_VERIFY(out);

    /* If the size doesn't change, the data shouldn't either */
    if(data.expectedSize == data.inputSize)
        CORRADE_COMPARE_AS(*out, (ImageView2D{
            PixelStorage{}.setAlignment(1),
            PixelFormat::RGB8Unorm, data.expectedSize, input
        }), DebugTools::CompareImage);
    else CORRADE_COMPARE_AS(*out, (ImageView2D{
            PixelStorage{}.setAlignment(1),
            PixelFormat::RGB8Unorm, data.expectedSize, data.expected
        }), DebugTools::CompareImage);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::StbResizeImageConverterTest)
