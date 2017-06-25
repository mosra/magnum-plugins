/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

#include "MagnumPlugins/DdsImporter/DdsImporter.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

namespace {

enum: std::size_t {
    #ifndef MAGNUM_TARGET_GLES2
    Files2DCount = 46,
    Files3DCount = 12,
    #else
    Files2DCount = 19,
    Files3DCount = 5,
    #endif
};

#ifndef MAGNUM_TARGET_GLES2
constexpr PixelFormat PixelFormat_RG = PixelFormat::RG;
constexpr PixelFormat PixelFormat_Red = PixelFormat::Red;
#else
constexpr PixelFormat PixelFormat_RG = PixelFormat::LuminanceAlpha;
constexpr PixelFormat PixelFormat_Red = PixelFormat::Luminance;
#endif

constexpr struct {
    const char* filename;
    PixelFormat format;
    PixelType type;
} Files2D[Files2DCount] = {
    {"2D_R16G16B16A16_FLOAT.dds", PixelFormat::RGBA, PixelType::HalfFloat},
    {"2D_R16G16B16A16_UNORM.dds", PixelFormat::RGBA, PixelType::UnsignedShort},
    {"2D_R32G32B32A32_FLOAT.dds", PixelFormat::RGBA, PixelType::Float},
    {"2D_R32G32B32_FLOAT.dds", PixelFormat::RGB, PixelType::Float},
    {"2D_R32G32_FLOAT.dds", PixelFormat_RG, PixelType::Float},
    {"2D_R8G8B8A8_UNORM.dds", PixelFormat::RGBA, PixelType::UnsignedByte},
    {"2D_R8G8B8A8_UNORM_SRGB.dds", PixelFormat::RGBA, PixelType::UnsignedByte},
    {"2D_R8G8_UNORM.dds", PixelFormat_RG, PixelType::UnsignedByte},
    {"2DMips_R16G16B16A16_FLOAT.dds", PixelFormat::RGBA, PixelType::HalfFloat},
    {"2DMips_R16G16B16A16_UNORM.dds", PixelFormat::RGBA, PixelType::UnsignedShort},
    {"2DMips_R16G16_FLOAT.dds", PixelFormat_RG, PixelType::HalfFloat},
    {"2DMips_R16G16_UNORM.dds", PixelFormat_RG, PixelType::UnsignedShort},
    {"2DMips_R32_FLOAT.dds", PixelFormat_Red, PixelType::Float},
    {"2DMips_R32G32B32A32_FLOAT.dds", PixelFormat::RGBA, PixelType::Float},
    {"2DMips_R32G32B32_FLOAT.dds", PixelFormat::RGB, PixelType::Float},
    {"2DMips_R32G32_FLOAT.dds", PixelFormat_RG, PixelType::Float},
    {"2DMips_R8G8B8A8_UNORM.dds", PixelFormat::RGBA, PixelType::UnsignedByte},
    {"2DMips_R8G8B8A8_UNORM_SRGB.dds", PixelFormat::RGBA, PixelType::UnsignedByte},
    {"2DMips_R8G8_UNORM.dds", PixelFormat_RG, PixelType::UnsignedByte},
    #ifndef MAGNUM_TARGET_GLES2
    {"2D_R16G16B16A16_SNORM.dds", PixelFormat::RGBA, PixelType::Short},
    {"2D_R8G8B8A8_SNORM.dds", PixelFormat::RGBA, PixelType::Byte},
    {"2D_R16G16B16A16_SINT.dds", PixelFormat::RGBAInteger, PixelType::Short},
    {"2D_R16G16B16A16_UINT.dds", PixelFormat::RGBAInteger, PixelType::UnsignedShort},
    {"2D_R32G32B32A32_SINT.dds", PixelFormat::RGBAInteger, PixelType::Int},
    {"2D_R32G32B32A32_UINT.dds", PixelFormat::RGBAInteger, PixelType::UnsignedInt},
    {"2D_R32G32B32_SINT.dds", PixelFormat::RGBInteger, PixelType::Int},
    {"2D_R32G32B32_UINT.dds", PixelFormat::RGBInteger, PixelType::UnsignedInt},
    {"2D_R8G8B8A8_SINT.dds", PixelFormat::RGBAInteger, PixelType::Byte},
    {"2D_R8G8B8A8_UINT.dds", PixelFormat::RGBAInteger, PixelType::UnsignedByte},
    {"2DMips_R16G16_SNORM.dds", PixelFormat::RG, PixelType::Short},
    {"2DMips_R16G16B16A16_SNORM.dds", PixelFormat::RGBA, PixelType::Short},
    {"2DMips_R8G8B8A8_SNORM.dds", PixelFormat::RGBA, PixelType::Byte},
    {"2DMips_R16G16B16A16_SINT.dds", PixelFormat::RGBAInteger, PixelType::Short},
    {"2DMips_R16G16B16A16_UINT.dds", PixelFormat::RGBAInteger, PixelType::UnsignedShort},
    {"2DMips_R16G16_SINT.dds", PixelFormat::RGInteger, PixelType::Short},
    {"2DMips_R16G16_UINT.dds", PixelFormat::RGInteger, PixelType::UnsignedShort},
    {"2DMips_R32G32B32A32_SINT.dds", PixelFormat::RGBAInteger, PixelType::Int},
    {"2DMips_R32G32B32A32_UINT.dds", PixelFormat::RGBAInteger, PixelType::UnsignedInt},
    {"2DMips_R32G32B32_SINT.dds", PixelFormat::RGBInteger, PixelType::Int},
    {"2DMips_R32G32B32_UINT.dds", PixelFormat::RGBInteger, PixelType::UnsignedInt},
    {"2DMips_R32G32_SINT.dds", PixelFormat::RGInteger, PixelType::Int},
    {"2DMips_R32G32_UINT.dds", PixelFormat::RGInteger, PixelType::UnsignedInt},
    {"2DMips_R32_SINT.dds", PixelFormat::RedInteger, PixelType::Int},
    {"2DMips_R32_UINT.dds", PixelFormat::RedInteger, PixelType::UnsignedInt},
    {"2DMips_R8G8B8A8_SINT.dds", PixelFormat::RGBAInteger, PixelType::Byte},
    {"2DMips_R8G8B8A8_UINT.dds", PixelFormat::RGBAInteger, PixelType::UnsignedByte}
    #endif
};

constexpr struct {
    const char* filename;
    PixelFormat format;
    PixelType type;
} Files3D[Files3DCount] = {
    {"3D_R16G16B16A16_FLOAT.dds", PixelFormat::RGBA, PixelType::HalfFloat},
    {"3D_R16G16B16A16_UNORM.dds", PixelFormat::RGBA, PixelType::UnsignedShort},
    {"3D_R32G32B32A32_FLOAT.dds", PixelFormat::RGBA, PixelType::Float},
    {"3D_R32G32B32_FLOAT.dds", PixelFormat::RGB, PixelType::Float},
    {"3D_R32G32_FLOAT.dds", PixelFormat_RG, PixelType::Float},
    #ifndef MAGNUM_TARGET_GLES2
    {"3D_R16G16B16A16_SNORM.dds", PixelFormat::RGBA, PixelType::Short},
    {"3D_R16G16B16A16_SINT.dds", PixelFormat::RGBAInteger, PixelType::Short},
    {"3D_R16G16B16A16_UINT.dds", PixelFormat::RGBAInteger, PixelType::UnsignedShort},
    {"3D_R32G32B32A32_SINT.dds", PixelFormat::RGBAInteger, PixelType::Int},
    {"3D_R32G32B32A32_UINT.dds", PixelFormat::RGBAInteger, PixelType::UnsignedInt},
    {"3D_R32G32B32_SINT.dds", PixelFormat::RGBInteger, PixelType::Int},
    {"3D_R32G32B32_UINT.dds", PixelFormat::RGBInteger, PixelType::UnsignedInt}
    #endif
};

}

struct DdsImporterTest: TestSuite::Tester {
    explicit DdsImporterTest();

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

    void useTwice();
};

DdsImporterTest::DdsImporterTest() {
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

    addInstancedTests({&DdsImporterTest::dxt10Formats2D}, Files2DCount);
    addInstancedTests({&DdsImporterTest::dxt10Formats3D}, Files3DCount);

    addTests({&DdsImporterTest::dxt10Data,
              &DdsImporterTest::dxt10TooShort,
              &DdsImporterTest::dxt10UnsupportedFormat,

              &DdsImporterTest::useTwice});
}

void DdsImporterTest::unknownCompression() {
    std::ostringstream out;
    Error redirectError{&out};

    Utility::Resource resource{"DdsTestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(!importer.openData(resource.getRaw("unknown_compression.dds")));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): unknown compression DXT4\n");
}

void DdsImporterTest::wrongSignature() {
    std::ostringstream out;
    Error redirectError{&out};

    Utility::Resource resource{"DdsTestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(!importer.openData(resource.getRaw("wrong_signature.dds")));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): wrong file signature\n");
}

void DdsImporterTest::unknownFormat() {
    std::ostringstream out;
    Error redirectError{&out};

    Utility::Resource resource{"DdsTestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(!importer.openData(resource.getRaw("unknown_format.dds")));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): unknown format\n");
}

void DdsImporterTest::insufficientData() {
    std::ostringstream out;
    Error redirectError{&out};

    Utility::Resource resource{"DdsTestFiles"};

    DdsImporter importer;
    auto data = resource.getRaw("rgb_uncompressed.dds");
    CORRADE_VERIFY(!importer.openData(data.prefix(data.size()-1)));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): not enough image data\n");
}

void DdsImporterTest::rgb() {
    Utility::Resource resource{"DdsTestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(importer.openData(resource.getRaw("rgb_uncompressed.dds")));

    const char pixels[] = {'\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77',
                           '\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77',
                           '\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77'};

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB);
    CORRADE_COMPARE(image->type(), PixelType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
        TestSuite::Compare::Container);
}

void DdsImporterTest::rgbWithMips() {
    Utility::Resource resource{"DdsTestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(importer.openData(resource.getRaw("rgb_uncompressed_mips.dds")));

    const char pixels[] = {'\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77',
                           '\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77',
                           '\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77'};
    const char mipPixels[] = {'\xd4', '\xd5', '\x96'};

    /* check image */
    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB);
    CORRADE_COMPARE(image->type(), PixelType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
            TestSuite::Compare::Container);

    /* check mip 0 */
    std::optional<Trade::ImageData2D> mip = importer.image2D(1);
    CORRADE_VERIFY(mip);
    CORRADE_VERIFY(!mip->isCompressed());
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(mip->size(), Vector2i{1});
    CORRADE_COMPARE(mip->format(), PixelFormat::RGB);
    CORRADE_COMPARE(mip->type(), PixelType::UnsignedByte);
    CORRADE_COMPARE_AS(mip->data(), Containers::arrayView(mipPixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::rgbVolume() {
    Utility::Resource resource{"DdsTestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(importer.openData(resource.getRaw("rgb_uncompressed_volume.dds")));

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

    std::optional<Trade::ImageData3D> image = importer.image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector3i(3, 2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB);
    CORRADE_COMPARE(image->type(), PixelType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
        TestSuite::Compare::Container);
}


void DdsImporterTest::dxt1() {
    Utility::Resource resource{"DdsTestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(importer.openData(resource.getRaw("rgba_dxt1.dds")));

    const char pixels[] = {'\x76', '\xdd', '\xee', '\xcf', '\x04', '\x51', '\x04', '\x51'};

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::RGBAS3tcDxt1);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::dxt3() {
    Utility::Resource resource{"DdsTestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(importer.openData(resource.getRaw("rgba_dxt3.dds")));

    const char pixels[] = {'\xff', '\xff', '\xff', '\xff', '\xff', '\xff', '\xff', '\xff',
                           '\x76', '\xdd', '\xee', '\xcf', '\x04', '\x51', '\x04', '\x51'};

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::RGBAS3tcDxt3);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::dxt5() {
    Utility::Resource resource{"DdsTestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(importer.openData(resource.getRaw("rgba_dxt5.dds")));

    const char pixels[] = {'\xff', '\xff', '\x49', '\x92', '\x24', '\x49', '\x92', '\x24',
                           '\x76', '\xdd', '\xee', '\xcf', '\x04', '\x51', '\x04', '\x51'};

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::RGBAS3tcDxt5);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::dxt10Formats2D() {
    const auto& file = Files2D[testCaseInstanceId()];

    setTestCaseDescription(file.filename);

    Utility::Resource resource{"Dxt10TestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(importer.openData(resource.getRaw(file.filename)));
    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), file.format);
    CORRADE_COMPARE(image->type(), file.type);
}

void DdsImporterTest::dxt10Formats3D() {
    const auto& file = Files3D[testCaseInstanceId()];

    setTestCaseDescription(file.filename);

    Utility::Resource resource{"Dxt10TestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(importer.openData(resource.getRaw(file.filename)));
    std::optional<Trade::ImageData3D> image = importer.image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector3i(3, 2, 3));
    CORRADE_COMPARE(image->format(), file.format);
    CORRADE_COMPARE(image->type(), file.type);
}

void DdsImporterTest::dxt10Data() {
    Utility::Resource resource{"Dxt10TestFiles"};

    DdsImporter importer;

    const char pixels[] = {
        '\xde', '\xad', '\xca', '\xfe',
        '\xde', '\xad', '\xca', '\xfe',
        '\xde', '\xad', '\xca', '\xfe'};

    CORRADE_VERIFY(importer.openData(resource.getRaw("2D_R8G8_UNORM.dds")));
    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), PixelFormat_RG);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView(pixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::dxt10TooShort() {
    Utility::Resource resource{"DdsTestFiles"};

    std::ostringstream out;
    Error redirectError{&out};

    DdsImporter importer;
    CORRADE_VERIFY(!importer.openData(resource.getRaw("too_short_dxt10.dds")));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): fourcc was DX10 but file is too short to contain DXT10 header\n");
}

void DdsImporterTest::dxt10UnsupportedFormat() {
    std::ostringstream out;
    Error redirectError{&out};

    Utility::Resource resource{"Dxt10TestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(!importer.openData(resource.getRaw("2D_AYUV.dds")));
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): unsupported DXGI format 100\n");
}

void DdsImporterTest::useTwice() {
    Utility::Resource resource{"DdsTestFiles"};

    DdsImporter importer;
    CORRADE_VERIFY(importer.openData(resource.getRaw("rgba_dxt5.dds")));

    /* Verify that the file is rewinded for second use */
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

CORRADE_TEST_MAIN(Magnum::Trade::Test::DdsImporterTest)
