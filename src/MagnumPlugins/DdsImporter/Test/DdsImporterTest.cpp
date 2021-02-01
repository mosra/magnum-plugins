/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

constexpr struct {
    const char* name;
    ImporterFlags flags;
    const char* message2D;
    const char* message3D;
} VerboseData[] {
    {"", {}, "", ""},
    {"verbose", ImporterFlag::Verbose,
        "Trade::DdsImporter::image2D(): converting from BGR to RGB\n",
        "Trade::DdsImporter::image3D(): converting from BGR to RGB\n"}
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
    {"2D_R8G8B8A8_UNORM_SRGB.dds", PixelFormat::RGBA8Unorm},
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
    {"2DMips_R8G8B8A8_UNORM_SRGB.dds", PixelFormat::RGBA8Unorm},
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

DdsImporterTest::DdsImporterTest() {
    addTests({&DdsImporterTest::wrongSignature,
              &DdsImporterTest::unknownFormat,
              &DdsImporterTest::unknownCompression,
              &DdsImporterTest::insufficientData});

    addInstancedTests({
        &DdsImporterTest::rgb,
        &DdsImporterTest::rgbWithMips,
        &DdsImporterTest::rgbVolume},
        Containers::arraySize(VerboseData));

    addTests({&DdsImporterTest::dxt1,
              &DdsImporterTest::dxt3,
              &DdsImporterTest::dxt5});

    addInstancedTests({&DdsImporterTest::dxt10Formats2D},
        Containers::arraySize(Files2D));
    addInstancedTests({&DdsImporterTest::dxt10Formats3D},
        Containers::arraySize(Files3D));

    addTests({&DdsImporterTest::dxt10Data,
              &DdsImporterTest::dxt10TooShort,
              &DdsImporterTest::dxt10UnsupportedFormat,

              &DdsImporterTest::useTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef DDSIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(DDSIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
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
    auto&& data = VerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    importer->setFlags(data.flags);
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgb_uncompressed.dds")));
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
    CORRADE_COMPARE(out.str(), data.message2D);
}

void DdsImporterTest::rgbWithMips() {
    auto&& data = VerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    importer->setFlags(data.flags);
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgb_uncompressed_mips.dds")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 2);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    std::ostringstream out;

    /* check image */
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
    CORRADE_COMPARE(out.str(), data.message2D);

    /* check mip 0 */
    Containers::Optional<ImageData2D> mip;
    {
        out.str({});
        Debug redirectOutput{&out};
        mip = importer->image2D(0, 1);
    }
    CORRADE_VERIFY(mip);
    CORRADE_VERIFY(!mip->isCompressed());
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(mip->size(), Vector2i{1});
    CORRADE_COMPARE(mip->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(mip->data(), Containers::arrayView<char>({
        '\xd4', '\xd5', '\x96'
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE(out.str(), data.message2D);
}

void DdsImporterTest::rgbVolume() {
    auto&& data = VerboseData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    importer->setFlags(data.flags);
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgb_uncompressed_volume.dds")));
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
    CORRADE_COMPARE(image->storage().alignment(), 1);
    CORRADE_COMPARE(image->size(), Vector3i(3, 2, 3));
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Unorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
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
        '\xca', '\xfe', '\x77'
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE(out.str(), data.message3D);
}

void DdsImporterTest::dxt1() {
    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgba_dxt1.dds")));

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc1RGBAUnorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\x76', '\xdd', '\xee', '\xcf', '\x04', '\x51', '\x04', '\x51'
    }), TestSuite::Compare::Container);
}

void DdsImporterTest::dxt3() {
    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgba_dxt3.dds")));

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc2RGBAUnorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xff', '\xff', '\xff', '\xff', '\xff', '\xff', '\xff', '\xff',
        '\x76', '\xdd', '\xee', '\xcf', '\x04', '\x51', '\x04', '\x51'
    }), TestSuite::Compare::Container);
}

void DdsImporterTest::dxt5() {
    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgba_dxt5.dds")));

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc3RGBAUnorm);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({
        '\xff', '\xff', '\x49', '\x92', '\x24', '\x49', '\x92', '\x24',
        '\x76', '\xdd', '\xee', '\xcf', '\x04', '\x51', '\x04', '\x51'
    }), TestSuite::Compare::Container);
}

void DdsImporterTest::dxt10Formats2D() {
    const auto& file = Files2D[testCaseInstanceId()];

    setTestCaseDescription(file.filename);

    Utility::Resource resource{"Dxt10TestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw(file.filename)));
    Containers::Optional<ImageData2D> image = importer->image2D(0);
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
    Containers::Optional<ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->size(), Vector3i(3, 2, 3));
    CORRADE_COMPARE(image->format(), file.format);
}

void DdsImporterTest::dxt10Data() {
    Utility::Resource resource{"Dxt10TestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");

    CORRADE_VERIFY(importer->openData(resource.getRaw("2D_R8G8_UNORM.dds")));
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
    CORRADE_COMPARE(out.str(), "Trade::DdsImporter::openData(): unsupported DXGI format 100\n");
}

void DdsImporterTest::useTwice() {
    Utility::Resource resource{"DdsTestFiles"};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DdsImporter");
    CORRADE_VERIFY(importer->openData(resource.getRaw("rgba_dxt5.dds")));

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
