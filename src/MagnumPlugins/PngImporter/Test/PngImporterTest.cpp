/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
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

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/String.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

#include <png.h> /* PNG_WARNINGS_SUPPORTED */

namespace Magnum { namespace Trade { namespace Test { namespace {

struct PngImporterTest: TestSuite::Tester {
    explicit PngImporterTest();

    void empty();
    void invalid();

    void gray();
    void gray16();
    void grayAlpha();
    void grayAlphaBinaryAlpha();
    void rgb();
    void rgb16();
    void rgbPalette1bit();
    void rgba();
    void rgbaBinaryAlpha();

    void alphaModeInvalid();

    void forceBitDepth8();
    void forceBitDepth16();
    void forceBitDepthInvalid();

    /* These test also combination of forceBitDepth and forceChannelCount */
    void forceChannelCount8();
    void forceChannelCount16();
    void forceChannelCountInvalid();

    void openMemory();
    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

using namespace Containers::Literals;
using namespace Math::Literals;

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    ImporterFlags flags;
    Containers::StringView data;
    const char* message;
} InvalidData[] {
    {"invalid signature", {},
        "invalid!"_s,
        "error: Not a PNG file"},
    {"short signature", {},
        "\x89PNG"_s,
        "error: file too short"},
    {"invalid signature with trailing zeros", {},
        "\x89PNG\x0d\x0a\x1a\x00"_s,
        "error: PNG file corrupted by ASCII conversion"},
    {"only signature", {},
        "\x89PNG\x0d\x0a\x1a\x0a"_s,
        "error: file too short"},
    {"zero width", {},
        "\x89PNG\x0d\x0a\x1a\x0a"
        "\x00\x00\x00\x0DIHDR"  /* HDR chunk, 13 bytes */
        "\x00\x00\x00\x00"      /* width, big-endian */
        "\x00\x00\x00\x02"      /* height, big-endian */
        "\x08\x04"              /* bit depth, color type */
        "\x00\x00\x00"          /* compression, filter, interlace method */
        "\xdc\x4a\x15\x92"_s,   /* hex(zlib.crc32(b'IHDR...')), big-endian */
        /* The version in emscripten-ports for some reason has warnings
           disabled, making only the error printed, not the warning:
            https://github.com/emscripten-core/emscripten/blob/389b097d3c3e62ba1989593242496c232e922f0e/tools/ports/libpng/pnglibconf.h#L277 */
        #ifdef PNG_WARNINGS_SUPPORTED
        "Trade::PngImporter::image2D(): warning: Image width is zero in IHDR\n"
        #endif
        "Trade::PngImporter::image2D(): error: Invalid IHDR data\n"},
    {"zero width, quiet", ImporterFlag::Quiet,
        "\x89PNG\x0d\x0a\x1a\x0a"
        "\x00\x00\x00\x0DIHDR"  /* Same as above */
        "\x00\x00\x00\x00"
        "\x00\x00\x00\x02"
        "\x08\x04"
        "\x00\x00\x00"
        "\xdc\x4a\x15\x92"_s,
        "error: Invalid IHDR data"}
};

constexpr struct {
    const char* name;
    const char* filename;
    bool premultipliedAlpha;
} GrayData[]{
    {"8bit", "gray.png", false},
    /* Enabling the premultiplied alpha shouldn't change the output at all for
       non-alpha formats */
    {"8bit, premultiplied alpha", "gray.png", true},
    /* convert gray.png -depth 4 -colorspace gray -define png:bit-depth=4 -define png:exclude-chunks=date gray-4bit.png */
    {"4bit", "gray4.png", false},
};

constexpr struct {
    const char* name;
    const char* filename;
    bool premultipliedAlpha;
} GrayAlphaData[]{
    {"8bit", "ga.png", false},
    {"8bit, premultiplied alpha", "ga.png", true},
};

const struct {
    const char* name;
    const char* filename;
    bool premultipliedAlpha;
} GrayAlphaBinaryAlphaData[]{
    /* magnum-imageconverter ga-binary-alpha-trns.png --converter StbPngImageConverter ga-binary-alpha.png
       because imagemagick is STUPID and doesn't let me override the rRNS from
       https://www.imagemagick.org/Usage/formats/#png_quality -- the suggested
       -type TruecolorMatte doesn't do anything and PNG32: gives me a RGBA
       (yes, the source file is the one from below) */
    {"", "ga-binary-alpha.png", false},
    /* The alpha is either all FF or 00 with color channels also 00 so this
       changes nothing */
    {"premultiplied alpha", "ga-binary-alpha.png", true},
    /* convert rgba-binary-alpha.png -colorspace gray ga-binary-alpha-trns.png
       According to https://www.imagemagick.org/Usage/formats/#png_quality,
       ImageMagick creates a tRNS chunk if the original image has binary (00
       or FF) alpha. */
    {"tRNS alpha mask", "ga-binary-alpha-trns.png", false},
    /* The alpha is either all FF or 00 with color channels also 00 so this
       changes nothing */
    {"tRNS alpha mask, premultiplied alpha", "ga-binary-alpha-trns.png", true},
};

constexpr struct {
    const char* name;
    const char* filename;
    bool premultipliedAlpha;
} RgbData[]{
    {"RGB", "rgb.png", false},
    /* Enabling the premultiplied alpha shouldn't change the output at all for
       non-alpha formats */
    {"RGB, premultiplied alpha", "rgb.png", true},
    /* convert rgb.png -define png:exclude-chunks=date png8:palette.png */
    {"palette", "rgb-palette.png", false},
};

constexpr struct {
    const char* name;
    const char* filename;
    const char* alphaMode;
    bool premultipliedAlphaUsesSrgb;
} RgbaData[]{
    {"RGBA, no gAMA chunk", "rgba.png",
        nullptr, false},
    /* pngcrush -g 0.45455 rgba.png rgba-srgb.png */
    {"RGBA, sRGB gAMA chunk", "rgba-srgb.png",
        nullptr, false},
    /* pngcrush -g 1.0 rgba.png rgba-linear.png */
    {"RGBA, linear gAMA chunk", "rgba-linear.png",
        nullptr, false},
    {"RGBA, premultiplied alpha, no gAMA chunk", "rgba.png",
        "premultiplied", true},
    {"RGBA, premultiplied alpha, sRGB gAMA chunk", "rgba-srgb.png",
        "premultiplied", true},
    {"RGBA, premultiplied alpha, linear gAMA chunk", "rgba-linear.png",
        "premultiplied", false},
    {"RGBA, premultiplied alpha, no gAMA chunk, forced linear", "rgba.png",
        "premultipliedLinear", false},
    {"RGBA, premultiplied alpha, sRGB gAMA chunk, forced linear", "rgba-srgb.png",
        "premultipliedLinear", false},
    /* See README.md for details on how this file was produced */
    {"CgBI BGRA", "rgba-binary-alpha-iphone.png",
        nullptr, false},
};

const struct {
    const char* name;
    const char* filename;
    bool premultipliedAlpha;
} RgbaBinaryAlphaData[]{
    {"", "rgba-binary-alpha.png", false},
    /* The alpha is either all FF or 00 with color channels also 00 so this
       changes nothing */
    {"premultiplied alpha", "rgba-binary-alpha.png", true},
    /* convert rgba-binary-alpha.png -define png:exclude-chunks=date rgba-binary-alpha-trns.png
       According to https://www.imagemagick.org/Usage/formats/#png_quality,
       ImageMagick creates a tRNS chunk if the original image has binary (00
       or FF) alpha. */
    {"tRNS alpha mask", "rgba-binary-alpha-trns.png", false},
    /* The alpha is either all FF or 00 with color channels also 00 so this
       changes nothing */
    {"tRNS alpha mask, premultiplied alpha", "rgba-binary-alpha-trns.png", true},
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    const char* filename;
    PixelFormat format;
    Vector2i size;
    Containers::Array<char> data;
    bool verbose;
    const char* message;
} ForceBitDepth8Data[]{
    {"already 8-bit", "rgba.png", PixelFormat::RGBA8Unorm, {3, 2}, {InPlaceInit, {
        /* Same as in rgba() */
        '\x66', '\x33', '\xff', '\x99',
        '\xcc', '\x33', '\xff', '\x00',
        '\x99', '\x33', '\xff', '\x66',
        '\x00', '\xcc', '\xff', '\x33',
        '\x33', '\x66', '\x99', '\xff',
        '\xff', '\x00', '\x33', '\xcc'
    }}, true, nullptr},
    {"RGB", "rgb16.png", PixelFormat::RGB8Unorm, {2, 3}, {InPlaceInit, {
        '\x04', '\x08', '\x0c',
        '\x08', '\x0c', '\x10',
        '\x0c', '\x10', '\x13',
        '\x10', '\x13', '\x17',
        '\x13', '\x17', '\x1b',
        '\x17', '\x1b', '\x1f'
    }}, false, nullptr},
    {"RGB, verbose", "rgb16.png", PixelFormat::RGB8Unorm, {2, 3}, {InPlaceInit, {
        '\x04', '\x08', '\x0c',
        '\x08', '\x0c', '\x10',
        '\x0c', '\x10', '\x13',
        '\x10', '\x13', '\x17',
        '\x13', '\x17', '\x1b',
        '\x17', '\x1b', '\x1f'
    }}, true, "Trade::PngImporter::image2D(): stripping 16-bit channels to 8-bit\n"},
    {"gray", "gray16.png", PixelFormat::R8Unorm, {2, 3}, {InPlaceInit, {
        4, 8,
        12, 16,
        19, 23
    }}, false, nullptr}
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    const char* filename;
    PixelFormat format;
    Vector2i size;
    Containers::Array<UnsignedShort> data;
    bool verbose;
    const char* message;
} ForceBitDepth16Data[]{
    {"already 16-bit", "gray16.png", PixelFormat::R16Unorm, {2, 3}, {InPlaceInit, {
        /* Same as in gray16() */
        1000, 2000,
        3000, 4000,
        5000, 6000
    }}, true, nullptr},
    {"RGBA", "rgba.png", PixelFormat::RGBA16Unorm, {3, 2}, {InPlaceInit, {
        26214, 13107, 65535, 39321,
        52428, 13107, 65535, 0,
        39321, 13107, 65535, 26214,
        0, 52428, 65535, 13107,
        13107, 26214, 39321, 65535,
        65535, 0, 13107, 52428,
    }}, false, nullptr},
    {"RGBA, verbose", "rgba.png", PixelFormat::RGBA16Unorm, {3, 2}, {InPlaceInit, {
        26214, 13107, 65535, 39321,
        52428, 13107, 65535, 0,
        39321, 13107, 65535, 26214,
        0, 52428, 65535, 13107,
        13107, 26214, 39321, 65535,
        65535, 0, 13107, 52428,
    }}, true, "Trade::PngImporter::image2D(): expanding 8-bit channels to 16-bit\n"},
    {"gray", "gray.png", PixelFormat::R16Unorm, {3, 2}, {InPlaceInit, {
        65535, 34952, 0,
        34952, 0, 65535
    }}, false, nullptr}
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    const char* filename;
    Int forceChannelCount;
    bool forceBitDepth;
    PixelFormat format;
    Vector2i size;
    Containers::Array<char> data;
    bool verbose;
    const char* message;
} ForceChannelCount8Data[]{
    /* Single-channel source to 1-4 channels */
    {"already single-channel, verbose", "gray.png", 1, false, PixelFormat::R8Unorm, {3, 2}, {InPlaceInit, {
        /* Same as in gray() */
        '\xff', '\x88', '\x00',
        '\x88', '\x00', '\xff'
    }}, true, ""},
    {"single-channel to two-channel", "gray.png", 2, false, PixelFormat::RG8Unorm, {3, 2}, {InPlaceInit, {
        /* Opaque alpha */
        '\xff', '\xff', '\x88', '\xff', '\x00', '\xff',
        '\x88', '\xff', '\x00', '\xff', '\xff', '\xff'
    }}, false, nullptr},
    {"single-channel to RGB", "gray.png", 3, false, PixelFormat::RGB8Unorm, {3, 2}, {InPlaceInit, {
        /* Copying the same channel three times */
        '\xff', '\xff', '\xff',
        '\x88', '\x88', '\x88',
        '\x00', '\x00', '\x00',

        '\x88', '\x88', '\x88',
        '\x00', '\x00', '\x00',
        '\xff', '\xff', '\xff'
    }}, false, nullptr},
    {"single-channel to RGBA, verbose", "gray.png", 4, false, PixelFormat::RGBA8Unorm, {3, 2}, {InPlaceInit, {
        /* Copying the same channel three times, opaque alpha */
        '\xff', '\xff', '\xff', '\xff',
        '\x88', '\x88', '\x88', '\xff',
        '\x00', '\x00', '\x00', '\xff',

        '\x88', '\x88', '\x88', '\xff',
        '\x00', '\x00', '\x00', '\xff',
        '\xff', '\xff', '\xff', '\xff'
    }}, true, "Trade::PngImporter::image2D(): converting 1-channel input to 4-channel by expanding grayscale to RGB and adding opaque alpha\n"},

    /* Two-channel source to 1-4 channels */
    {"already two-channel, verbose", "ga.png", 2, false, PixelFormat::RG8Unorm, {3, 2}, {InPlaceInit, {
        /* Same as in grayAlpha() */
        '\x66', '\x99', '\xcc', '\x00', '\x99', '\x66',
        '\x00', '\x33', '\x33', '\xff', '\xff', '\xcc'
    }}, true, ""},
    {"two-channel to single-channel", "ga.png", 1, false, PixelFormat::R8Unorm, {3, 2}, {InPlaceInit, {
        /* Just the first component */
        '\x66', '\xcc', '\x99',
        '\x00', '\x33', '\xff'
    }}, false, nullptr},
    {"two-channel to RGB", "ga.png", 3, false, PixelFormat::RGB8Unorm, {3, 2}, {InPlaceInit, {
        /* First component expanded, alpha discarded */
        '\x66', '\x66', '\x66',
        '\xcc', '\xcc', '\xcc',
        '\x99', '\x99', '\x99',

        '\x00', '\x00', '\x00',
        '\x33', '\x33', '\x33',
        '\xff', '\xff', '\xff'
    }}, false, nullptr},
    {"two-channel to RGBA", "ga.png", 4, false, PixelFormat::RGBA8Unorm, {3, 2}, {InPlaceInit, {
        /* First component expanded */
        '\x66', '\x66', '\x66', '\x99',
        '\xcc', '\xcc', '\xcc', '\x00',
        '\x99', '\x99', '\x99', '\x66',

        '\x00', '\x00', '\x00', '\x33',
        '\x33', '\x33', '\x33', '\xff',
        '\xff', '\xff', '\xff', '\xcc'
    }}, false, nullptr},

    /* Three-channel source to 1-4 channels */
    {"already three-channel", "rgb.png", 3, false, PixelFormat::RGB8Unorm, {3, 2}, {InPlaceInit, {
        /* Same as in rgb() */
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',

        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5'
    }}, true, ""},
    {"three-channel to single-channel", "rgb.png", 1, false, PixelFormat::R8Unorm, {3, 2}, {InPlaceInit, {
        /* The RGB channels are converted to luminance, the output is however
           *not* consistent with what stb_image produces in
           StbImageImporterTest::rgbPngOneChannel(). Doesn't seem to be due to
           sRGB vs linear, as the variant below also doesn't match. */
        '\xe9', '\xb7', '\xe9',
        '\xb7', '\xe9', '\xb7'
    }}, true, "Trade::PngImporter::image2D(): converting 3-channel input to 1-channel by converting RGB to grayscale\n"},
    {"three-channel to single-channel, sRGB gAMA chunk", "rgb-srgb.png", 1, false, PixelFormat::R8Unorm, {3, 2}, {InPlaceInit, {
        /* Also doesn't match StbImageImporterTest::rgbPngOneChannel(). But is
           different from the above, meaning sRGB is correctly taken into
           account. */
        '\xed', '\xba', '\xed',
        '\xba', '\xed', '\xba'
    }}, false, nullptr},
    {"three-channel to two-channel", "rgb.png", 2, false, PixelFormat::RG8Unorm, {3, 2}, {InPlaceInit, {
        /* Like above plus opaque alpha */
        '\xe9', '\xff', '\xb7', '\xff', '\xe9', '\xff',
        '\xb7', '\xff', '\xe9', '\xff', '\xb7', '\xff'
    }}, false, nullptr},
    {"three-channel to RGBA", "rgb.png", 4, false, PixelFormat::RGBA8Unorm, {3, 2}, {InPlaceInit, {
        /* Opaque alpha */
        '\xca', '\xfe', '\x77', '\xff',
        '\xde', '\xad', '\xb5', '\xff',
        '\xca', '\xfe', '\x77', '\xff',

        '\xde', '\xad', '\xb5', '\xff',
        '\xca', '\xfe', '\x77', '\xff',
        '\xde', '\xad', '\xb5', '\xff'
    }}, false, nullptr},

    /* Four-channel source to 1-4 channels */
    {"already four-channel, verbose", "rgba.png", 4, false, PixelFormat::RGBA8Unorm, {3, 2}, {InPlaceInit, {
        /* Same as in rgba() */
        '\x66', '\x33', '\xff', '\x99',
        '\xcc', '\x33', '\xff', '\x00',
        '\x99', '\x33', '\xff', '\x66',
        '\x00', '\xcc', '\xff', '\x33',
        '\x33', '\x66', '\x99', '\xff',
        '\xff', '\x00', '\x33', '\xcc'
    }}, true, ""},
    {"four-channel to single-channel", "rgba.png", 1, false, PixelFormat::R8Unorm, {3, 2}, {InPlaceInit, {
        /* The RGB channels are converted to luminance, alpha is discarded */
        '\x4c', '\x62', '\x57',
        '\xa4', '\x5e', '\x39'
    }}, false, nullptr},
    {"four-channel to two-channel", "rgba.png", 2, false, PixelFormat::RG8Unorm, {3, 2}, {InPlaceInit, {
        /* The RGB channels are converted to luminance */
        '\x4c', '\x99', '\x62', '\x00', '\x57', '\x66',
        '\xa4', '\x33', '\x5e', '\xff', '\x39', '\xcc'
    }}, false, nullptr},
    {"four-channel to RGB, verbose", "rgba.png", 3, false, PixelFormat::RGB8Unorm, {3, 2}, {InPlaceInit, {
        /* Alpha is discarded */
        '\x66', '\x33', '\xff',
        '\xcc', '\x33', '\xff',
        '\x99', '\x33', '\xff',
        '\x00', '\xcc', '\xff',
        '\x33', '\x66', '\x99',
        '\xff', '\x00', '\x33',
    }}, true, "Trade::PngImporter::image2D(): converting 4-channel input to 3-channel by removing alpha\n"},

    /* Special cases. Libpng has no way to signal that a particular
       combination of transformations is unsupported so attempt to check as
       many weird combinations as possible. */
    {"4-bit gray to two-channel", "gray4.png", 2, false, PixelFormat::RG8Unorm, {3, 2}, {InPlaceInit, {
        /* Same as in gray() + opaque alpha */
        '\xff', '\xff', '\x88', '\xff', '\x00', '\xff',
        '\x88', '\xff', '\x00', '\xff', '\xff', '\xff'
    }}, false, nullptr},
    {"four-channel sRGB to single-channel", "rgba-srgb.png", 1, false, PixelFormat::R8Unorm, {3, 2}, {InPlaceInit, {
        /* Compared to above, the sRGB curve is applied */
        '\x62', '\x81', '\x6f',
        '\xbc', '\x63', '\x7f'
    }}, false, nullptr},
    {"GA binary alpha tRNS to four-channel", "ga-binary-alpha-trns.png", 4, false, PixelFormat::RGBA8Unorm, {3, 2}, {InPlaceInit, {
        /* Like grayAlphaBinaryAlpha() but with the channels expanded */
        '\xb8', '\xb8', '\xb8', '\xff',
        '\xe9', '\xe9', '\xe9', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xe9', '\xe9', '\xe9', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xb8', '\xb8', '\xb8', '\xff',
    }}, false, nullptr},
    {"16-bit RGB to two-channel 8-bit, verbose", "rgb16.png", 2, true, PixelFormat::RG8Unorm, {2, 3}, {InPlaceInit, {
        /* The original values are roughly 1/64 to 1/8 */
        '\x07', '\xff',
        '\x0b', '\xff',
        '\x0f', '\xff',
        '\x13', '\xff',
        '\x17', '\xff',
        '\x1b', '\xff',
    }}, true,
        "Trade::PngImporter::image2D(): converting 3-channel input to 2-channel by converting RGB to grayscale and adding opaque alpha\n"
        "Trade::PngImporter::image2D(): stripping 16-bit channels to 8-bit\n"},
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    const char* filename;
    Int forceChannelCount;
    bool forceBitDepth;
    PixelFormat format;
    Vector2i size;
    Containers::Array<UnsignedShort> data;
    bool verbose;
    const char* message;
} ForceChannelCount16Data[]{
    /* Compared to the 8-bit data above, this verifies just some cases to
       ensure forcing channels works also with 16-bit */

    {"already three-channel, verbose", "rgb16.png", 3, false, PixelFormat::RGB16Unorm, {2, 3}, {InPlaceInit, {
        /* Like in rgb16() */
        1000, 2000, 3000, 2000, 3000, 4000,
        3000, 4000, 5000, 4000, 5000, 6000,
        5000, 6000, 7000, 6000, 7000, 8000
    }}, true, ""},
    {"single-channel to RGBA", "gray16.png", 4, false, PixelFormat::RGBA16Unorm, {2, 3}, {InPlaceInit, {
        /* Expanded, opaque alpha */
        1000, 1000, 1000, 65535, 2000, 2000, 2000, 65535,
        3000, 3000, 3000, 65535, 4000, 4000, 4000, 65535,
        5000, 5000, 5000, 65535, 6000, 6000, 6000, 65535
    }}, false, nullptr},
    {"three-channel to single-channel", "rgb16.png", 1, false, PixelFormat::R16Unorm, {2, 3}, {InPlaceInit, {
        /* Some kind of an average */
        1860, 2860,
        3860, 4860,
        5860, 6860
    }}, false, nullptr},
    {"4-bit gray to 16-bit two-channel", "gray4.png", 2, true, PixelFormat::RG16Unorm, {3, 2}, {InPlaceInit, {
        /* Same as in gray() + opaque alpha, expanded from 8 to 16 bits */
        0xffff, 0xffff, 0x8888, 0xffff, 0x0000, 0xffff,
        0x8888, 0xffff, 0x0000, 0xffff, 0xffff, 0xffff
    }}, true,
        "Trade::PngImporter::image2D(): converting 1-channel input to 2-channel by adding opaque alpha\n"
        "Trade::PngImporter::image2D(): expanding 8-bit channels to 16-bit\n"},
};

/* Shared among all plugins that implement data copying optimizations */
const struct {
    const char* name;
    bool(*open)(AbstractImporter&, Containers::ArrayView<const void>);
} OpenMemoryData[]{
    {"data", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        /* Copy to ensure the original memory isn't referenced */
        Containers::Array<char> copy{InPlaceInit, Containers::arrayCast<const char>(data)};
        return importer.openData(copy);
    }},
    {"memory", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        return importer.openMemory(data);
    }},
};

PngImporterTest::PngImporterTest() {
    addTests({&PngImporterTest::empty});

    addInstancedTests({&PngImporterTest::invalid},
        Containers::arraySize(InvalidData));

    addInstancedTests({&PngImporterTest::gray},
        Containers::arraySize(GrayData));

    addTests({&PngImporterTest::gray16});

    addInstancedTests({&PngImporterTest::grayAlpha},
        Containers::arraySize(GrayAlphaData));

    addInstancedTests({&PngImporterTest::grayAlphaBinaryAlpha},
        Containers::arraySize(GrayAlphaBinaryAlphaData));

    addInstancedTests({&PngImporterTest::rgb},
        Containers::arraySize(RgbData));

    addTests({&PngImporterTest::rgb16,
              &PngImporterTest::rgbPalette1bit});

    addInstancedTests({&PngImporterTest::rgba},
        Containers::arraySize(RgbaData));

    addInstancedTests({&PngImporterTest::rgbaBinaryAlpha},
        Containers::arraySize(RgbaBinaryAlphaData));

    addTests({&PngImporterTest::alphaModeInvalid});

    addInstancedTests({&PngImporterTest::forceBitDepth8},
        Containers::arraySize(ForceBitDepth8Data));

    addInstancedTests({&PngImporterTest::forceBitDepth16},
        Containers::arraySize(ForceBitDepth16Data));

    addTests({&PngImporterTest::forceBitDepthInvalid});

    addInstancedTests({&PngImporterTest::forceChannelCount8},
        Containers::arraySize(ForceChannelCount8Data));

    addInstancedTests({&PngImporterTest::forceChannelCount16},
        Containers::arraySize(ForceChannelCount16Data));

    addTests({&PngImporterTest::forceChannelCountInvalid});

    addInstancedTests({&PngImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&PngImporterTest::openTwice,
              &PngImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef PNGIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(PNGIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void PngImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");

    Containers::String out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out, "Trade::PngImporter::openData(): the file is empty\n");
}

void PngImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->addFlags(data.flags);

    /* The open does just a memory copy, so it doesn't fail */
    CORRADE_VERIFY(importer->openData(data.data));

    Containers::String out;
    Warning redirectWarning{&out};
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    /* Sometimes there's also a warning, in that case take the message
       verbatim */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE(out, data.message);
    else
        CORRADE_COMPARE(out, Utility::format("Trade::PngImporter::image2D(): {}\n", data.message));
}

void PngImporterTest::gray() {
    auto&& data = GrayData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    if(data.premultipliedAlpha)
        importer->configuration().setValue("alphaMode", "premultiplied");

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(image->data().size(), 8);
    image->mutableData()[3] = image->mutableData()[7] = 0;

    /* Premultiplied alpha should have no effect on this */
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xff', '\x88', '\x00', 0,
        '\x88', '\x00', '\xff', 0
    }), TestSuite::Compare::Container);
}

void PngImporterTest::gray16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray16.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::R16Unorm);

    CORRADE_COMPARE_AS(image->pixels<UnsignedShort>().asContiguous(), Containers::arrayView<UnsignedShort>({
        1000, 2000,
        3000, 4000,
        5000, 6000
    }), TestSuite::Compare::Container);
}

void PngImporterTest::grayAlpha() {
    auto&& data = GrayAlphaData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    if(data.premultipliedAlpha)
        importer->configuration().setValue("alphaMode", "premultiplied");

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RG8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(image->data().size(), 16);
    image->mutableData()[6] = image->mutableData()[7] =
        image->mutableData()[14] = image->mutableData()[15] = 0;

    if(data.premultipliedAlpha)
        CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
            /* Values get decoded from sRGB, scaled and then encoded back, so
               it's not just a linear multiplication. Alpha values stay the
               same tho, and pixels with 00 or FF alpha also. See rgba() for a
               more thorough check of sRGB vs linear data depending on what's
               actually present in the file metadata, the RG values here should
               match RA values in rgba(). */
            '\x50', '\x99', '\x00', '\x00', '\x65', '\x66', '\x00', '\x00', '\x00', '\x33', '\x33', '\xff', '\xe6', '\xcc', '\x00', '\x00'
        }), TestSuite::Compare::Container);
    else
        CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
            /* Matches PngImageConverterTest::grayscaleAlpha() */
            '\x66', '\x99', '\xcc', '\x00', '\x99', '\x66', 0, 0,
            '\x00', '\x33', '\x33', '\xff', '\xff', '\xcc', 0, 0
        }), TestSuite::Compare::Container);
}

void PngImporterTest::grayAlphaBinaryAlpha() {
    auto&& data = GrayAlphaBinaryAlphaData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    if(data.premultipliedAlpha)
        importer->configuration().setValue("alphaMode", "premultiplied");

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RG8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(image->data().size(), 16);
    image->mutableData()[6] = image->mutableData()[7] =
        image->mutableData()[14] = image->mutableData()[15] = 0;

    /* If premultiplied alpha is enabled, the result should be the same as
       pixels with full alpha stay unchanged and pixels with zero alpha are
       zero already */
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xb8', '\xff', '\xe9', '\xff', '\x00', '\x00', 0, 0,
        '\xe9', '\xff', '\x00', '\x00', '\xb8', '\xff', 0, 0
    }), TestSuite::Compare::Container);
}

void PngImporterTest::rgb() {
    auto&& data = RgbData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    if(data.premultipliedAlpha)
        importer->configuration().setValue("alphaMode", "premultiplied");

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);

    /* The image has four-byte aligned rows, clear the padding to deterministic
       values */
    CORRADE_COMPARE(image->data().size(), 24);
    image->mutableData()[9] = image->mutableData()[10] =
        image->mutableData()[11] = image->mutableData()[21] =
            image->mutableData()[22] = image->mutableData()[23] = 0;

    /* Premultiplied alpha should have no effect on this */
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77', 0, 0, 0,

        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5', 0, 0, 0
    }), TestSuite::Compare::Container);
}

void PngImporterTest::rgb16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb16.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB16Unorm);

    CORRADE_COMPARE_AS(image->pixels<Vector3us>().asContiguous(), Containers::arrayView<Vector3us>({
        {1000, 2000, 3000}, {2000, 3000, 4000},
        {3000, 4000, 5000}, {4000, 5000, 6000},
        {5000, 6000, 7000}, {6000, 7000, 8000}
    }), TestSuite::Compare::Container);
}

void PngImporterTest::rgbPalette1bit() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");

    /* convert IcoImporter/Test/icon256x256.png rgb-palette-1bit.png
       (the file is all 0x0000ff, so I guess that makes it a 1-bit palette).
       Taking this file as a repro case because it crashed the importer. */
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb-palette1.png")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(256, 256));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);

    CORRADE_COMPARE(image->pixels<Color3ub>()[0][0], 0x0000ff_rgb);
}

void PngImporterTest::rgba() {
    auto&& data = RgbaData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    if(data.alphaMode)
        importer->configuration().setValue("alphaMode", data.alphaMode);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    {
        CORRADE_EXPECT_FAIL_IF(Containers::StringView{data.name}.contains("CgBI"),
            "Stock libPNG can't handle CgBI.");
        CORRADE_VERIFY(image);
    }

    /* Skip the rest if loading the image failed */
    if(!image)
        return;

    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    if(data.alphaMode) {
        if(data.premultipliedAlphaUsesSrgb) {
            CORRADE_COMPARE_AS(image->data(),  Containers::arrayView<char>({
                /* Values get decoded from sRGB, scaled and then encoded back,
                   so it's not just a linear multiplication. Alpha values stay
                   the same tho, and pixels with 00 or FF alpha also. */
                /** @todo expand this test once there's an option to encode
                    either linear or sRGB into metadata */
                '\x50', '\x27', '\xca', '\x99',
                '\x00', '\x00', '\x00', '\x00',
                '\x65', '\x22', '\xa8', '\x66',
                '\x00', '\x62', '\x7b', '\x33',
                '\x33', '\x66', '\x99', '\xff',
                '\xe6', '\x00', '\x2e', '\xcc'
            }), TestSuite::Compare::Container);
            /* Check that this is indeed a sRGB conversion. The _srgbaf literal
               converts the sRGB pixel value (which matches the
               non-premultiplied pixel value below) to linear floats and
               premultiplies it accordingly, the Color4::fromSrgbAlpha()
               extracts an actual premultiplied sRGB value from the imported
               image. */
            CORRADE_COMPARE_WITH(
                Color4::fromSrgbAlpha(image->pixels<Color4ub>()[0][0]),
                0x6633ffff_srgbaf*(0x99/255.0f),
                TestSuite::Compare::around(Color4{0.0125f, 0.002f}));
        } else {
            CORRADE_COMPARE_AS(image->data(),  Containers::arrayView<char>({
                /* Values get simply unpacked, scaled and then packed back,
                   with no sRGB (de/en)coding. */
                '\x3d', '\x1f', '\x99', '\x99',
                '\x00', '\x00', '\x00', '\x00',
                '\x3d', '\x14', '\x66', '\x66',
                '\x00', '\x29', '\x33', '\x33',
                '\x33', '\x66', '\x99', '\xff',
                '\xcc', '\x00', '\x29', '\xcc'
            }), TestSuite::Compare::Container);
            /* Check that this is indeed a linear conversion. The _rgbaf
               literal converts the (linear) pixel value (which matches the
               non-premultiplied pixel value below) to floats and premultiplies
               it accordingly, the Math::unpack() extracts an actual
               premultiplied (linear) value from the imported image. */
            CORRADE_COMPARE_WITH(
                Math::unpack<Color4>(image->pixels<Color4ub>()[0][0]),
                0x6633ffff_rgbaf*(0x99/255.0f),
                TestSuite::Compare::around(Color4{0.002f, 0.002f}));
        }
    } else CORRADE_COMPARE_AS(image->data(),  Containers::arrayView<char>({
        /* Matches PngImageConverterTest::rgba() */
        '\x66', '\x33', '\xff', '\x99',
        '\xcc', '\x33', '\xff', '\x00',
        '\x99', '\x33', '\xff', '\x66',
        '\x00', '\xcc', '\xff', '\x33',
        '\x33', '\x66', '\x99', '\xff',
        '\xff', '\x00', '\x33', '\xcc'
    }), TestSuite::Compare::Container);
}

void PngImporterTest::rgbaBinaryAlpha() {
    auto&& data = RgbaBinaryAlphaData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    if(data.premultipliedAlpha)
        importer->configuration().setValue("alphaMode", "premultiplied");

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    /* If premultiplied alpha is enabled, the result should be the same as
       pixels with full alpha stay unchanged and pixels with zero alpha are
       zero already */
    CORRADE_COMPARE_AS(image->data(),  Containers::arrayView<char>({
        '\xde', '\xad', '\xb5', '\xff',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xca', '\xfe', '\x77', '\xff',
        '\x00', '\x00', '\x00', '\x00',
        '\xde', '\xad', '\xb5', '\xff'
    }), TestSuite::Compare::Container);
}

void PngImporterTest::alphaModeInvalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("alphaMode", "premultipliedSrgb");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgba.png")));

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out, "Trade::PngImporter::image2D(): expected alphaMode to be either empty, premultiplied or premultipliedLinear but got premultipliedSrgb\n");
}

void PngImporterTest::forceBitDepth8() {
    auto&& data = ForceBitDepth8Data[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceBitDepth", 8);
    if(data.verbose)
        importer->addFlags(ImporterFlag::Verbose);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::String out;
    Containers::Optional<ImageData2D> image;
    {
        Debug redirectOutput{&out};
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image,
        (ImageView2D{PixelStorage{}.setAlignment(1), data.format, data.size, data.data}),
        DebugTools::CompareImage);
    CORRADE_COMPARE(out, data.message);
}

void PngImporterTest::forceBitDepth16() {
    auto&& data = ForceBitDepth16Data[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Like forceBitDepth8(), just using a different type for the expected
       data */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceBitDepth", 16);
    if(data.verbose)
        importer->addFlags(ImporterFlag::Verbose);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::String out;
    Containers::Optional<ImageData2D> image;
    {
        Debug redirectOutput{&out};
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image,
        (ImageView2D{PixelStorage{}.setAlignment(1), data.format, data.size, data.data}),
        DebugTools::CompareImage);
    CORRADE_COMPARE(out, data.message);
}

void PngImporterTest::forceBitDepthInvalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceBitDepth", 4);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgba.png")));

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out, "Trade::PngImporter::image2D(): expected forceBitDepth to be 0, 8 or 16 but got 4\n");
}

void PngImporterTest::forceChannelCount8() {
    auto&& data = ForceChannelCount8Data[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceChannelCount", data.forceChannelCount);
    if(data.forceBitDepth)
        importer->configuration().setValue("forceBitDepth", 8);
    if(data.verbose)
        importer->addFlags(ImporterFlag::Verbose);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::String out;
    Containers::Optional<ImageData2D> image;
    {
        Debug redirectOutput{&out};
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image,
        (ImageView2D{PixelStorage{}.setAlignment(1), data.format, data.size, data.data}),
        DebugTools::CompareImage);
    CORRADE_COMPARE(out, data.message);
}

void PngImporterTest::forceChannelCount16() {
    auto&& data = ForceChannelCount16Data[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Like forceChannelCount8(), just using a different type for the expected
       data */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceChannelCount", data.forceChannelCount);
    if(data.forceBitDepth)
        importer->configuration().setValue("forceBitDepth", 16);
    if(data.verbose)
        importer->addFlags(ImporterFlag::Verbose);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, data.filename)));

    Containers::String out;
    Containers::Optional<ImageData2D> image;
    {
        Debug redirectOutput{&out};
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(*image,
        (ImageView2D{PixelStorage{}.setAlignment(1), data.format, data.size, data.data}),
        DebugTools::CompareImage);
    CORRADE_COMPARE(out, data.message);
}

void PngImporterTest::forceChannelCountInvalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    importer->configuration().setValue("forceChannelCount", 5);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgba.png")));

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out, "Trade::PngImporter::image2D(): expected forceChannelCount to be 0, 1, 2, 3 or 4 but got 5\n");
}

void PngImporterTest::openMemory() {
    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Same as gray16() except that it uses openData() & openMemory() instead
       of openFile() to test data copying on import */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray16.png"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*importer, *memory));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->size(), Vector2i(2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::R16Unorm);

    CORRADE_COMPARE_AS(image->pixels<UnsignedShort>().asContiguous(), Containers::arrayView<UnsignedShort>({
        1000, 2000,
        3000, 4000,
        5000, 6000
    }), TestSuite::Compare::Container);
}

void PngImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));

    /* Shouldn't crash, leak or anything */
}

void PngImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray.png")));

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

CORRADE_TEST_MAIN(Magnum::Trade::Test::PngImporterTest)
