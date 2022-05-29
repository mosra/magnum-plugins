/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2015 Jonathan Hale <squareys@googlemail.com>

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
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/String.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct DdsImporterTest: TestSuite::Tester {
    explicit DdsImporterTest();

    void enumValueMatching();

    void invalid();

    void r();
    void rgb();
    void rgDxt10();
    void rgbMips();
    void rgbMipsDxt10();
    void rgba3D();
    /* 3D DXT10 tested in rgba3D() */

    /* Testing both classic and DXGI version and both 64bit and 128bit blocks */
    void dxt3();
    void dxt3IncompleteBlocks();
    void bc4();
    void bc7();

    void formats();

    void openMemory();
    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

/* Enum taken verbatim from dxgiformat.h */
enum DXGI_FORMAT {
    DXGI_FORMAT_UNKNOWN                     = 0,
    DXGI_FORMAT_R32G32B32A32_TYPELESS       = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT          = 2,
    DXGI_FORMAT_R32G32B32A32_UINT           = 3,
    DXGI_FORMAT_R32G32B32A32_SINT           = 4,
    DXGI_FORMAT_R32G32B32_TYPELESS          = 5,
    DXGI_FORMAT_R32G32B32_FLOAT             = 6,
    DXGI_FORMAT_R32G32B32_UINT              = 7,
    DXGI_FORMAT_R32G32B32_SINT              = 8,
    DXGI_FORMAT_R16G16B16A16_TYPELESS       = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT          = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM          = 11,
    DXGI_FORMAT_R16G16B16A16_UINT           = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM          = 13,
    DXGI_FORMAT_R16G16B16A16_SINT           = 14,
    DXGI_FORMAT_R32G32_TYPELESS             = 15,
    DXGI_FORMAT_R32G32_FLOAT                = 16,
    DXGI_FORMAT_R32G32_UINT                 = 17,
    DXGI_FORMAT_R32G32_SINT                 = 18,
    DXGI_FORMAT_R32G8X24_TYPELESS           = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,
    DXGI_FORMAT_R10G10B10A2_TYPELESS        = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM           = 24,
    DXGI_FORMAT_R10G10B10A2_UINT            = 25,
    DXGI_FORMAT_R11G11B10_FLOAT             = 26,
    DXGI_FORMAT_R8G8B8A8_TYPELESS           = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM              = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB         = 29,
    DXGI_FORMAT_R8G8B8A8_UINT               = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM              = 31,
    DXGI_FORMAT_R8G8B8A8_SINT               = 32,
    DXGI_FORMAT_R16G16_TYPELESS             = 33,
    DXGI_FORMAT_R16G16_FLOAT                = 34,
    DXGI_FORMAT_R16G16_UNORM                = 35,
    DXGI_FORMAT_R16G16_UINT                 = 36,
    DXGI_FORMAT_R16G16_SNORM                = 37,
    DXGI_FORMAT_R16G16_SINT                 = 38,
    DXGI_FORMAT_R32_TYPELESS                = 39,
    DXGI_FORMAT_D32_FLOAT                   = 40,
    DXGI_FORMAT_R32_FLOAT                   = 41,
    DXGI_FORMAT_R32_UINT                    = 42,
    DXGI_FORMAT_R32_SINT                    = 43,
    DXGI_FORMAT_R24G8_TYPELESS              = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT           = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS       = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT        = 47,
    DXGI_FORMAT_R8G8_TYPELESS               = 48,
    DXGI_FORMAT_R8G8_UNORM                  = 49,
    DXGI_FORMAT_R8G8_UINT                   = 50,
    DXGI_FORMAT_R8G8_SNORM                  = 51,
    DXGI_FORMAT_R8G8_SINT                   = 52,
    DXGI_FORMAT_R16_TYPELESS                = 53,
    DXGI_FORMAT_R16_FLOAT                   = 54,
    DXGI_FORMAT_D16_UNORM                   = 55,
    DXGI_FORMAT_R16_UNORM                   = 56,
    DXGI_FORMAT_R16_UINT                    = 57,
    DXGI_FORMAT_R16_SNORM                   = 58,
    DXGI_FORMAT_R16_SINT                    = 59,
    DXGI_FORMAT_R8_TYPELESS                 = 60,
    DXGI_FORMAT_R8_UNORM                    = 61,
    DXGI_FORMAT_R8_UINT                     = 62,
    DXGI_FORMAT_R8_SNORM                    = 63,
    DXGI_FORMAT_R8_SINT                     = 64,
    DXGI_FORMAT_A8_UNORM                    = 65,
    DXGI_FORMAT_R1_UNORM                    = 66,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP          = 67,
    DXGI_FORMAT_R8G8_B8G8_UNORM             = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM             = 69,
    DXGI_FORMAT_BC1_TYPELESS                = 70,
    DXGI_FORMAT_BC1_UNORM                   = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB              = 72,
    DXGI_FORMAT_BC2_TYPELESS                = 73,
    DXGI_FORMAT_BC2_UNORM                   = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB              = 75,
    DXGI_FORMAT_BC3_TYPELESS                = 76,
    DXGI_FORMAT_BC3_UNORM                   = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB              = 78,
    DXGI_FORMAT_BC4_TYPELESS                = 79,
    DXGI_FORMAT_BC4_UNORM                   = 80,
    DXGI_FORMAT_BC4_SNORM                   = 81,
    DXGI_FORMAT_BC5_TYPELESS                = 82,
    DXGI_FORMAT_BC5_UNORM                   = 83,
    DXGI_FORMAT_BC5_SNORM                   = 84,
    DXGI_FORMAT_B5G6R5_UNORM                = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM              = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM              = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM              = 88,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS           = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB         = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS           = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB         = 93,
    DXGI_FORMAT_BC6H_TYPELESS               = 94,
    DXGI_FORMAT_BC6H_UF16                   = 95,
    DXGI_FORMAT_BC6H_SF16                   = 96,
    DXGI_FORMAT_BC7_TYPELESS                = 97,
    DXGI_FORMAT_BC7_UNORM                   = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB              = 99,
    DXGI_FORMAT_AYUV                        = 100,
    DXGI_FORMAT_Y410                        = 101,
    DXGI_FORMAT_Y416                        = 102,
    DXGI_FORMAT_NV12                        = 103,
    DXGI_FORMAT_P010                        = 104,
    DXGI_FORMAT_P016                        = 105,
    DXGI_FORMAT_420_OPAQUE                  = 106,
    DXGI_FORMAT_YUY2                        = 107,
    DXGI_FORMAT_Y210                        = 108,
    DXGI_FORMAT_Y216                        = 109,
    DXGI_FORMAT_NV11                        = 110,
    DXGI_FORMAT_AI44                        = 111,
    DXGI_FORMAT_IA44                        = 112,
    DXGI_FORMAT_P8                          = 113,
    DXGI_FORMAT_A8P8                        = 114,
    DXGI_FORMAT_B4G4R4A4_UNORM              = 115,

    DXGI_FORMAT_P208                        = 130,
    DXGI_FORMAT_V208                        = 131,
    DXGI_FORMAT_V408                        = 132,

    DXGI_FORMAT_FORCE_UINT                  = 0xffffffff
};

const struct {
    DXGI_FORMAT dxgi;
    PixelFormat format;
    CompressedPixelFormat compressedFormat;
} DxgiFormatData[] {
#define _x(name) {DXGI_FORMAT_ ## name, PixelFormat{}, CompressedPixelFormat{}},
#define _u(name, format) {DXGI_FORMAT_ ## name, PixelFormat::format, CompressedPixelFormat{}},
#define _s(name, format, swizzle) {DXGI_FORMAT_ ## name, PixelFormat::format, CompressedPixelFormat{}},
#define _c(name, format) {DXGI_FORMAT_ ## name, PixelFormat{}, CompressedPixelFormat::format},
#include "../DxgiFormat.h"
#undef _c
#undef _s
#undef _u
#undef _x
};

const struct {
    const char* name;
    const char* filename;
    Containers::Optional<std::size_t> size;
    const char* message;
} InvalidData[]{
    {"wrong file signature", "wrong-signature.dds", {},
        "invalid file signature SSD "},
    {"unknown compression", "dxt4.dds", {},
        "unknown compression DXT4"},
    {"unknown format", "unknown-format.dds", {},
        "unknown 64 bits per pixel format with a RGBA mask {0xff0000, 0xff00, 0xff, 0x0}"},
    {"DXT10 format unsupported", "dxt10-ayuv.dds", {},
        "unsupported format DXGI_FORMAT_AYUV"},
    {"DXT10 format out of bounds", "dxt10-v408.dds", {},
        "unknown DXGI format ID 132"},
    {"empty file", "bgr8unorm.dds", 0,
        "file too short, expected at least 128 bytes but got 0"},
    {"header too short", "bgr8unorm.dds", 127,
        "file too short, expected at least 128 bytes but got 127"},
    {"DX10 header too short", "dxt10-rgba8unorm.dds", 128 + 19,
        "DXT10 file too short, expected at least 148 bytes but got 147"},
    {"file too short", "bgr8unorm.dds", 145, /* original is 146 */
        "file too short, expected 146 bytes for image 0 level 0 but got 145"},
    {"file with mips too short", "bgr8unorm-mips.dds", 148, /* original is 149 */
        "file too short, expected 149 bytes for image 0 level 1 but got 148"},
    /** @todo cubemap file too short */
};

constexpr struct {
    const char* name;
    const char* filename;
    ImporterFlags flags;
    const char* message;
} SwizzleData[] {
    {"BGR", "bgr8unorm.dds", {},
        ""},
    {"BGR, verbose", "bgr8unorm.dds", ImporterFlag::Verbose,
        "Trade::DdsImporter::image2D(): converting from BGR to RGB\n"},
    {"RGB, verbose", "rgb8unorm.dds", ImporterFlag::Verbose,
        ""},
    /* No three-component 8-bit format in DXT10, so that's a separate test
       case (and thus no swizzle needs to be tested) */
};

constexpr struct {
    const char* name;
    const char* filename;
    ImporterFlags flags;
    const char* message;
} Swizzle3DData[] {
    {"BGRA", "bgra8unorm-3d.dds", {},
        ""},
    {"BGRA, verbose", "bgra8unorm-3d.dds", ImporterFlag::Verbose,
        "Trade::DdsImporter::image3D(): converting from BGRA to RGBA\n"},
    {"RGBA, verbose", "rgba8unorm-3d.dds", ImporterFlag::Verbose,
        ""},
    {"DXT10 BGRA", "dxt10-bgra8unorm-3d.dds", {},
        ""},
    {"DXT10 BGRA, verbose", "dxt10-bgra8unorm-3d.dds", ImporterFlag::Verbose,
        "Trade::DdsImporter::image3D(): converting from BGRA to RGBA\n"},
    {"DXT10 RGBA, verbose", "dxt10-rgba8unorm-3d.dds", ImporterFlag::Verbose,
        ""},
};

constexpr struct {
    const char* filename;
    PixelFormat format;
    CompressedPixelFormat compressedFormat;
} FormatsData[]{
    {"bgrx8unorm.dds", PixelFormat::RGBA8Unorm, CompressedPixelFormat{}},
    {"rgbx8unorm.dds", PixelFormat::RGBA8Unorm, CompressedPixelFormat{}},
    {"dxt1.dds", PixelFormat{}, CompressedPixelFormat::Bc1RGBAUnorm},
    {"dxt5.dds", PixelFormat{}, CompressedPixelFormat::Bc3RGBAUnorm},
    /* Those have legacy non-recommended FourCCs, so testing each and
       every, except bc4unorm that's already tested in bc4() */
    {"bc4snorm.dds", PixelFormat{}, CompressedPixelFormat::Bc4RSnorm},
    {"bc5unorm.dds", PixelFormat{}, CompressedPixelFormat::Bc5RGUnorm},
    {"bc5snorm.dds", PixelFormat{}, CompressedPixelFormat::Bc5RGSnorm},
    {"dxt10-rg32f.dds", PixelFormat::RG32F, CompressedPixelFormat{}},
    {"dxt10-rgb32i.dds", PixelFormat::RGB32I, CompressedPixelFormat{}},
    {"dxt10-rgba16snorm.dds", PixelFormat::RGBA16Snorm, CompressedPixelFormat{}},
    {"dxt10-rgba32ui.dds", PixelFormat::RGBA32UI, CompressedPixelFormat{}},
    {"dxt10-rgba8unorm.dds", PixelFormat::RGBA8Unorm, CompressedPixelFormat{}},
    {"dxt10-rgba8srgb.dds", PixelFormat::RGBA8Srgb, CompressedPixelFormat{}},
    {"dxt10-depth24unorm-stencil8ui.dds", PixelFormat::Depth24UnormStencil8UI, CompressedPixelFormat{}},
    {"dxt10-depth32f-stencil8ui.dds", PixelFormat::Depth32FStencil8UI, CompressedPixelFormat{}},
};

/* Shared among all plugins that implement data copying optimizations */
const struct {
    const char* name;
    bool(*open)(AbstractImporter&, Containers::ArrayView<const void>);
} OpenMemoryData[]{
    {"data", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        /* Copy to ensure the original memory isn't referenced */
        Containers::Array<char> copy{NoInit, data.size()};
        Utility::copy(Containers::arrayCast<const char>(data), copy);
        return importer.openData(copy);
    }},
    {"memory", [](AbstractImporter& importer, Containers::ArrayView<const void> data) {
        return importer.openMemory(data);
    }},
};

DdsImporterTest::DdsImporterTest() {
    addRepeatedTests({&DdsImporterTest::enumValueMatching},
        Containers::arraySize(DxgiFormatData));

    addInstancedTests({&DdsImporterTest::invalid},
        Containers::arraySize(InvalidData));

    addTests({&DdsImporterTest::r});

    addInstancedTests({&DdsImporterTest::rgb},
        Containers::arraySize(SwizzleData));

    addTests({&DdsImporterTest::rgDxt10,

              &DdsImporterTest::rgbMips,
              &DdsImporterTest::rgbMipsDxt10});

    addInstancedTests({&DdsImporterTest::rgba3D},
        Containers::arraySize(Swizzle3DData));

    addTests({&DdsImporterTest::dxt3,
              &DdsImporterTest::dxt3IncompleteBlocks,
              &DdsImporterTest::bc4,
              &DdsImporterTest::bc7});

    addInstancedTests({&DdsImporterTest::formats},
        Containers::arraySize(FormatsData));

    addInstancedTests({&DdsImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&DdsImporterTest::openTwice,
              &DdsImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef DDSIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(DDSIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void DdsImporterTest::enumValueMatching() {
    CORRADE_COMPARE(DxgiFormatData[testCaseRepeatId()].dxgi, DXGI_FORMAT(testCaseRepeatId()));

    /* Check the format value fits into 8 bits, as that's how it's packed in
       the plugin */
    if(UnsignedInt(DxgiFormatData[testCaseRepeatId()].format)) {
        CORRADE_ITERATION(DxgiFormatData[testCaseRepeatId()].format);
        CORRADE_COMPARE_AS(UnsignedInt(DxgiFormatData[testCaseRepeatId()].format), 256,
            TestSuite::Compare::Less);
    }
    if(UnsignedInt(DxgiFormatData[testCaseRepeatId()].compressedFormat)) {
        CORRADE_ITERATION(DxgiFormatData[testCaseRepeatId()].compressedFormat);
        CORRADE_COMPARE_AS(UnsignedInt(DxgiFormatData[testCaseRepeatId()].compressedFormat), 256,
            TestSuite::Compare::Less);
    }
}

void DdsImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    Containers::Optional<Containers::Array<char>> in = Utility::Path::read(Utility::Path::join(DDSIMPORTER_TEST_DIR, data.filename));
    CORRADE_VERIFY(in);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(data.size ? in->prefix(*data.size) : *in));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::DdsImporter::openData(): {}\n", data.message));
}

void DdsImporterTest::r() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, "r8unorm.dds")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::R8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xde', '\xca', '\xde',
        '\xca', '\xde', '\xca',
    }), TestSuite::Compare::Container);
}

void DdsImporterTest::rgb() {
    auto&& data = SwizzleData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    importer->setFlags(data.flags);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, data.filename)));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    std::ostringstream out;
    Containers::Optional<ImageData2D> image;
    {
        Debug redirectOutput{&out};
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77'
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE(out.str(), data.message);
}

void DdsImporterTest::rgDxt10() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt10-rg8unorm.dds")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RG8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xde', '\xad', '\xca', '\xfe',
        '\xde', '\xad', '\xca', '\xfe',
        '\xde', '\xad', '\xca', '\xfe'
    }), TestSuite::Compare::Container);
}

void DdsImporterTest::rgbMips() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, "bgr8unorm-mips.dds")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 2);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    {
        Containers::Optional<ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->storage().alignment(), 1);
        CORRADE_COMPARE(image->size(), Vector2i(3, 2));
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE_AS(image->data(), Containers::arrayView({
            '\xde', '\xad', '\xb5',
            '\xca', '\xfe', '\x77',
            '\xde', '\xad', '\xb5',
            '\xca', '\xfe', '\x77',
            '\xde', '\xad', '\xb5',
            '\xca', '\xfe', '\x77'
        }), TestSuite::Compare::Container);
    } {
        Containers::Optional<ImageData2D> image = importer->image2D(0, 1);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->storage().alignment(), 1);
        CORRADE_COMPARE(image->size(), Vector2i(1, 1));
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
        CORRADE_COMPARE_AS(image->data(), Containers::arrayView({
            '\xd4', '\xd5', '\x96'
        }), TestSuite::Compare::Container);
    }
}

void DdsImporterTest::rgbMipsDxt10() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt10-r32i-mips.dds")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 2);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    {
        Containers::Optional<ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->storage().alignment(), 4);
        CORRADE_COMPARE(image->size(), Vector2i(3, 2));
        CORRADE_COMPARE(image->format(), PixelFormat::R32I);
        CORRADE_COMPARE_AS(image->data(), Containers::arrayView({
            '\x00', '\x00', '\x11', '\x11',
            '\x22', '\x22', '\x33', '\x33',
            '\x44', '\x44', '\x55', '\x55',

            '\x66', '\x66', '\x77', '\x77',
            '\x88', '\x88', '\x99', '\x99',
            '\xaa', '\xaa', '\xbb', '\xbb'
        }), TestSuite::Compare::Container);
    } {
        Containers::Optional<ImageData2D> image = importer->image2D(0, 1);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->storage().alignment(), 4);
        CORRADE_COMPARE(image->size(), Vector2i(1, 1));
        CORRADE_COMPARE(image->format(), PixelFormat::R32I);
        CORRADE_COMPARE_AS(image->data(), Containers::arrayView({
            '\xcc', '\xcc', '\xdd', '\xdd'
        }), TestSuite::Compare::Container);
    }
}

void DdsImporterTest::rgba3D() {
    auto&& data = Swizzle3DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    importer->setFlags(data.flags);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, data.filename)));
    CORRADE_COMPARE(importer->image2DCount(), 0);
    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), 1);

    std::ostringstream out;
    Containers::Optional<ImageData3D> image;
    {
        Debug redirectOutput{&out};
        image = importer->image3D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->storage().alignment(), 4);
    CORRADE_COMPARE(image->size(), Vector3i(3, 2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        /* Slice 0 */
        '\xde', '\xad', '\xb5', '\x00',
        '\xca', '\xfe', '\x77', '\x11',
        '\xde', '\xad', '\xb5', '\x22',
        '\xca', '\xfe', '\x77', '\x33',
        '\xde', '\xad', '\xb5', '\x44',
        '\xca', '\xfe', '\x77', '\x55',

        /* Slice 1 */
        '\xca', '\xfe', '\x77', '\x66',
        '\xde', '\xad', '\xb5', '\x77',
        '\xca', '\xfe', '\x77', '\x88',
        '\xde', '\xad', '\xb5', '\x99',
        '\xca', '\xfe', '\x77', '\xaa',
        '\xde', '\xad', '\xb5', '\xbb',

        /* Slice 2 */
        '\xde', '\xad', '\xb5', '\xcc',
        '\xca', '\xfe', '\x77', '\xdd',
        '\xde', '\xad', '\xb5', '\xee',
        '\xca', '\xfe', '\x77', '\xff',
        '\xde', '\xad', '\xb5', '\x00',
        '\xca', '\xfe', '\x77', '\x11'
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE(out.str(), data.message);
}

void DdsImporterTest::dxt3() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt3.dds")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), (Vector2i{64, 32}));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc2RGBAUnorm);
    /* Verify just a small prefix and suffix to be sure the data got copied */
    CORRADE_COMPARE_AS(image->data().prefix(16), Containers::array({
        '\x22', '\x22', '\x22', '\x22', '\x22', '\x22', '\x22', '\x22',
        '\xc6', '\xd1', '\x86', '\xc1', '\xaa', '\xff', '\xaa', '\xff'
    }), TestSuite::Compare::Container);
    /** @todo suffix() once it takes N last bytes */
    CORRADE_COMPARE_AS(image->data().exceptPrefix(image->data().size() - 16), Containers::array({
        '\xaa', '\xaa', '\xaa', '\xaa', '\x99', '\x99', '\x99', '\x99',
        '\xa6', '\xc9', '\xa6', '\xc1', '\xaa', '\x00', '\x00', '\x00'
    }), TestSuite::Compare::Container);
}

void DdsImporterTest::dxt3IncompleteBlocks() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt3-incomplete-blocks.dds")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc2RGBAUnorm);
    /* Verify just a small prefix and suffix to be sure the data got copied */
    CORRADE_COMPARE_AS(image->data().prefix(16), Containers::array({
        '\x22', '\x22', '\x22', '\x22', '\x22', '\x22', '\x33', '\x33',
        '\xa6', '\xc9', '\xa5', '\xc1', '\x00', '\xaa', '\x00', '\x00'
    }), TestSuite::Compare::Container);
    /** @todo suffix() once it takes N last bytes */
    CORRADE_COMPARE_AS(image->data().exceptPrefix(image->data().size() - 16), Containers::array({
        '\xaa', '\xaa', '\xaa', '\xaa', '\xaa', '\xaa', '\xaa', '\xaa',
        '\xa6', '\xc9', '\xa6', '\xc1', '\x00', '\x00', '\xaa', '\x00'
    }), TestSuite::Compare::Container);
}

void DdsImporterTest::bc4() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, "bc4unorm.dds")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc4RUnorm);
    CORRADE_COMPARE_AS(image->data(), Containers::array({
        '\xde', '\xca', '\x08', '\x10', '\x24', '\x08', '\x10', '\x24'
    }), TestSuite::Compare::Container);
}

void DdsImporterTest::bc7() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt10-bc7.dds")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), (Vector2i{64, 32}));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc7RGBAUnorm);
    /* Verify just a small prefix and suffix to be sure the data got copied */
    CORRADE_COMPARE_AS(image->data().prefix(16), Containers::array({
        '\xc0', '\x35', '\xb9', '\x93', '\xb1', '\x64', '\x1c', '\x94',
        '\x6c', '\x66', '\xbb', '\xbb', '\x99', '\x99', '\xcc', '\xcc'
    }), TestSuite::Compare::Container);
    /** @todo suffix() once it takes N last bytes */
    CORRADE_COMPARE_AS(image->data().exceptPrefix(image->data().size() - 16), Containers::array({
        '\x40', '\xf3', '\x59', '\xa3', '\xc9', '\x60', '\xa6', '\x50',
        '\x12', '\x11', '\x66', '\x66', '\xbb', '\xbb', '\xff', '\xff'
    }), TestSuite::Compare::Container);
}

void DdsImporterTest::formats() {
    auto&& data = FormatsData[testCaseInstanceId()];
    setTestCaseDescription(Utility::Path::splitExtension(data.filename).first());

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, data.filename)));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    if(data.format != PixelFormat{}) {
        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->format(), data.format);
    } else {
        CORRADE_VERIFY(image->isCompressed());
        CORRADE_COMPARE(image->compressedFormat(), data.compressedFormat);
    }
}

void DdsImporterTest::openMemory() {
    /* compared to dxt3() uses openData() & openMemory() instead of openFile()
       to test data copying on import, and a deliberately small file */

    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt1.dds"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*importer, *memory));

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc1RGBAUnorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xee', '\xcf', '\x76', '\xdd', '\x51', '\x04', '\x51', '\x04'
    }), TestSuite::Compare::Container);
}

void DdsImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt5.dds")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt5.dds")));

    /* Shouldn't crash, leak or anything */
}

void DdsImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DDSIMPORTER_TEST_DIR, "dxt5.dds")));

    /* Verify that the file is rewinded for second use */
    {
        Containers::Optional<ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
    } {
        Containers::Optional<ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{3, 2}));
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::DdsImporterTest)
