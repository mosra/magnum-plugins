/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
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
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

/* Enum taken verbatim from dxgiformat.h */
enum DXGI_FORMAT {
    DXGI_FORMAT_UNKNOWN	                    = 0,
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

constexpr DXGI_FORMAT DxgiFormats[] {
#define _x(name) DXGI_FORMAT_ ## name,
#define _u(name, format) DXGI_FORMAT_ ## name,
#define _s(name, format, swizzle) DXGI_FORMAT_ ## name,
#define _c(name, format) DXGI_FORMAT_ ## name,
#include "../DxgiFormat.h"
#undef _c
#undef _s
#undef _u
#undef _x
};

constexpr struct {
    const char* filename;
    PixelFormat format;
} Files2D[]{
    {"2D_R16G16B16A16_FLOAT.dds", PixelFormat::RGBA16F},
    {"2D_R16G16B16A16_UNORM.dds", PixelFormat::RGBA16Unorm},
    {"2D_R32G32B32A32_FLOAT.dds", PixelFormat::RGBA32F},
    {"2D_R32G32B32_FLOAT.dds", PixelFormat::RGB32F},
    {"2D_R32G32_FLOAT.dds", PixelFormat::RG32F},
    {"2D_R8G8B8A8_UNORM.dds", PixelFormat::RGBA8Unorm},
    {"2D_R8G8B8A8_UNORM_SRGB.dds", PixelFormat::RGBA8Srgb},
    {"2D_R8G8_UNORM.dds", PixelFormat::RG8Unorm},
    {"2DMips_R16G16B16A16_FLOAT.dds", PixelFormat::RGBA16F},
    {"2DMips_R16G16B16A16_UNORM.dds", PixelFormat::RGBA16Unorm},
    {"2DMips_R16G16_FLOAT.dds", PixelFormat::RG16F},
    {"2DMips_R16G16_UNORM.dds", PixelFormat::RG16Unorm},
    {"2DMips_R32_FLOAT.dds", PixelFormat::R32F},
    {"2DMips_R32G32B32A32_FLOAT.dds", PixelFormat::RGBA32F},
    {"2DMips_R32G32B32_FLOAT.dds", PixelFormat::RGB32F},
    {"2DMips_R32G32_FLOAT.dds", PixelFormat::RG32F},
    {"2DMips_R8G8B8A8_UNORM.dds", PixelFormat::RGBA8Unorm},
    {"2DMips_R8G8B8A8_UNORM_SRGB.dds", PixelFormat::RGBA8Srgb},
    {"2DMips_R8G8_UNORM.dds", PixelFormat::RG8Unorm},
    {"2D_R16G16B16A16_SNORM.dds", PixelFormat::RGBA16Snorm},
    {"2D_R8G8B8A8_SNORM.dds", PixelFormat::RGBA8Snorm},
    {"2D_R16G16B16A16_SINT.dds", PixelFormat::RGBA16I},
    {"2D_R16G16B16A16_UINT.dds", PixelFormat::RGBA16UI},
    {"2D_R32G32B32A32_SINT.dds", PixelFormat::RGBA32I},
    {"2D_R32G32B32A32_UINT.dds", PixelFormat::RGBA32UI},
    {"2D_R32G32B32_SINT.dds", PixelFormat::RGB32I},
    {"2D_R32G32B32_UINT.dds", PixelFormat::RGB32UI},
    {"2D_R8G8B8A8_SINT.dds", PixelFormat::RGBA8I},
    {"2D_R8G8B8A8_UINT.dds", PixelFormat::RGBA8UI},
    {"2DMips_R16G16_SNORM.dds", PixelFormat::RG16Snorm},
    {"2DMips_R16G16B16A16_SNORM.dds", PixelFormat::RGBA16Snorm},
    {"2DMips_R8G8B8A8_SNORM.dds", PixelFormat::RGBA8Snorm},
    {"2DMips_R16G16B16A16_SINT.dds", PixelFormat::RGBA16I},
    {"2DMips_R16G16B16A16_UINT.dds", PixelFormat::RGBA16UI},
    {"2DMips_R16G16_SINT.dds", PixelFormat::RG16I},
    {"2DMips_R16G16_UINT.dds", PixelFormat::RG16UI},
    {"2DMips_R32G32B32A32_SINT.dds", PixelFormat::RGBA32I},
    {"2DMips_R32G32B32A32_UINT.dds", PixelFormat::RGBA32UI},
    {"2DMips_R32G32B32_SINT.dds", PixelFormat::RGB32I},
    {"2DMips_R32G32B32_UINT.dds", PixelFormat::RGB32UI},
    {"2DMips_R32G32_SINT.dds", PixelFormat::RG32I},
    {"2DMips_R32G32_UINT.dds", PixelFormat::RG32UI},
    {"2DMips_R32_SINT.dds", PixelFormat::R32I},
    {"2DMips_R32_UINT.dds", PixelFormat::R32UI},
    {"2DMips_R8G8B8A8_SINT.dds", PixelFormat::RGBA8I},
    {"2DMips_R8G8B8A8_UINT.dds", PixelFormat::RGBA8UI}
};

constexpr struct {
    const char* filename;
    PixelFormat format;
} Files3D[]{
    {"3D_R16G16B16A16_FLOAT.dds", PixelFormat::RGBA16F},
    {"3D_R16G16B16A16_UNORM.dds", PixelFormat::RGBA16Unorm},
    {"3D_R32G32B32A32_FLOAT.dds", PixelFormat::RGBA32F},
    {"3D_R32G32B32_FLOAT.dds", PixelFormat::RGB32F},
    {"3D_R32G32_FLOAT.dds", PixelFormat::RG32F},
    {"3D_R16G16B16A16_SNORM.dds", PixelFormat::RGBA16Snorm},
    {"3D_R16G16B16A16_SINT.dds", PixelFormat::RGBA16I},
    {"3D_R16G16B16A16_UINT.dds", PixelFormat::RGBA16UI},
    {"3D_R32G32B32A32_SINT.dds", PixelFormat::RGBA32I},
    {"3D_R32G32B32A32_UINT.dds", PixelFormat::RGBA32UI},
    {"3D_R32G32B32_SINT.dds", PixelFormat::RGB32I},
    {"3D_R32G32B32_UINT.dds", PixelFormat::RGB32UI}
};

struct DdsImporterTest: TestSuite::Tester {
    explicit DdsImporterTest();

    void enumValueMatching();

    void wrongSignature();
    void unknownFormat();
    void unknownCompression();
    void insufficientData();

    void rgb();
    void rgbWithMips();
    void rgbVolume();

    void dxt1();
    void dxt3();
    void dxt5();

    void dxt10Formats2D();
    void dxt10Formats3D();

    void dxt10Data();
    void dxt10TooShort();
    void dxt10UnsupportedFormat();
    void dxt10UnknownFormatId();

    void useTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

DdsImporterTest::DdsImporterTest() {
    addRepeatedTests({&DdsImporterTest::enumValueMatching},
        Containers::arraySize(DxgiFormats));

    addTests({&DdsImporterTest::wrongSignature,
              &DdsImporterTest::unknownFormat,
              &DdsImporterTest::unknownCompression,
              &DdsImporterTest::insufficientData,

              &DdsImporterTest::rgb,
              &DdsImporterTest::rgbWithMips,
              &DdsImporterTest::rgbVolume,

              &DdsImporterTest::dxt1,
              &DdsImporterTest::dxt3,
              &DdsImporterTest::dxt5});

    addInstancedTests({&DdsImporterTest::dxt10Formats2D},
        Containers::arraySize(Files2D));
    addInstancedTests({&DdsImporterTest::dxt10Formats3D},
        Containers::arraySize(Files3D));

    addTests({&DdsImporterTest::dxt10Data,
              &DdsImporterTest::dxt10TooShort,
              &DdsImporterTest::dxt10UnsupportedFormat,
              &DdsImporterTest::dxt10UnknownFormatId,

              &DdsImporterTest::useTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef DDSIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(DDSIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void DdsImporterTest::enumValueMatching() {
    CORRADE_COMPARE(DxgiFormats[testCaseRepeatId()], DXGI_FORMAT(testCaseRepeatId()));
}

void DdsImporterTest::unknownCompression() {
    std::ostringstream out;
    Error redirectError{&out};

    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(!importer->openData(resource.getRaw("unknown_compression.dds")));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): unknown compression DXT4\n");
}

void DdsImporterTest::wrongSignature() {
    std::ostringstream out;
    Error redirectError{&out};

    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(!importer->openData(resource.getRaw("wrong_signature.dds")));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): wrong file signature\n");
}

void DdsImporterTest::unknownFormat() {
    std::ostringstream out;
    Error redirectError{&out};

    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(!importer->openData(resource.getRaw("unknown_format.dds")));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): unknown format\n");
}

void DdsImporterTest::insufficientData() {
    std::ostringstream out;
    Error redirectError{&out};

    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    auto data = resource.getRaw("rgb_uncompressed.dds");
    CORRADE_VERIFY(!importer->openData(data.prefix(data.size()-1)));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): not enough image data\n");
}

void DdsImporterTest::rgb() {
    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgb_uncompressed.dds")));

    const char pixels[] = {'\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77',
                           '\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77',
                           '\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77'};

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
        TestSuite::Compare::Container);
}

void DdsImporterTest::rgbWithMips() {
    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgb_uncompressed_mips.dds")));

    const char pixels[] = {'\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77',
                           '\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77',
                           '\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77'};
    const char mipPixels[] = {'\xd4', '\xd5', '\x96'};

    /* check image */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
            TestSuite::Compare::Container);

    /* check mip 0 */
    Containers::Optional<Trade::ImageData2D> mip = importer->image2D(1);
    CORRADE_VERIFY(mip);
    CORRADE_VERIFY(!mip->isCompressed());
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(mip->size(), Vector2i{1});
    CORRADE_COMPARE(mip->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(mip->data(), Containers::arrayView(mipPixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::rgbVolume() {
    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgb_uncompressed_volume.dds")));

    const char pixels[] = {
        /* slice 0 */
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        /* slice 1 */
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        /* slice 2 */
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77'};

    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector3i(3, 2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
        TestSuite::Compare::Container);
}


void DdsImporterTest::dxt1() {
    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgba_dxt1.dds")));

    const char pixels[] = {'\x76', '\xdd', '\xee', '\xcf', '\x04', '\x51', '\x04', '\x51'};

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc1RGBAUnorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::dxt3() {
    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgba_dxt3.dds")));

    const char pixels[] = {'\xff', '\xff', '\xff', '\xff', '\xff', '\xff', '\xff', '\xff',
                           '\x76', '\xdd', '\xee', '\xcf', '\x04', '\x51', '\x04', '\x51'};

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc2RGBAUnorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::dxt5() {
    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgba_dxt5.dds")));

    const char pixels[] = {'\xff', '\xff', '\x49', '\x92', '\x24', '\x49', '\x92', '\x24',
                           '\x76', '\xdd', '\xee', '\xcf', '\x04', '\x51', '\x04', '\x51'};

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc3RGBAUnorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::dxt10Formats2D() {
    const auto& file = Files2D[testCaseInstanceId()];

    setTestCaseDescription(file.filename);

    Utility::Resource resource{"Dxt10TestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw(file.filename)));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), file.format);
}

void DdsImporterTest::dxt10Formats3D() {
    const auto& file = Files3D[testCaseInstanceId()];

    setTestCaseDescription(file.filename);

    Utility::Resource resource{"Dxt10TestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw(file.filename)));
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector3i(3, 2, 3));
    CORRADE_COMPARE(image->format(), file.format);
}

void DdsImporterTest::dxt10Data() {
    Utility::Resource resource{"Dxt10TestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");

    const char pixels[] = {
        '\xde', '\xad', '\xca', '\xfe',
        '\xde', '\xad', '\xca', '\xfe',
        '\xde', '\xad', '\xca', '\xfe'};

    CORRADE_VERIFY(importer->openData(resource.getRaw("2D_R8G8_UNORM.dds")));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RG8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::dxt10TooShort() {
    Utility::Resource resource{"DdsTestFiles"};

    std::ostringstream out;
    Error redirectError{&out};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(!importer->openData(resource.getRaw("too_short_dxt10.dds")));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): fourcc was DX10 but file is too short to contain DXT10 header\n");
}

void DdsImporterTest::dxt10UnsupportedFormat() {
    std::ostringstream out;
    Error redirectError{&out};

    Utility::Resource resource{"Dxt10TestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(!importer->openData(resource.getRaw("2D_AYUV.dds")));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): unsupported format DXGI_FORMAT_AYUV\n");
}

void DdsImporterTest::dxt10UnknownFormatId() {
    std::ostringstream out;
    Error redirectError{&out};

    Utility::Resource resource{"Dxt10TestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(!importer->openData(resource.getRaw("2D_V408.dds")));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): unknown DXGI format ID 132\n");
}

void DdsImporterTest::useTwice() {
    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgba_dxt5.dds")));

    /* Verify that the file is rewinded for second use */
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

CORRADE_TEST_MAIN(Magnum::Trade::Test::DdsImporterTest)
