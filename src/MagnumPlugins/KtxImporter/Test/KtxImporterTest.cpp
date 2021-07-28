/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2021 Pablo Escobar <mail@rvrs.in>

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
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/PixelStorage.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Swizzle.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/TextureData.h>

#include "MagnumPlugins/KtxImporter/KtxHeader.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct KtxImporterTest: TestSuite::Tester {
    explicit KtxImporterTest();

    /** @todo - array textures
              - 3D textures
              - cubemaps
              - depth/stencil formats
              - orientation flips
              - invalid KVD
              - larger formats
              - block-compressed formats
              */

    void openShort();

    void invalid();
    void invalidVersion();
    void invalidFormat();

    void texture();

    void rgb8();
    void rgba8();

    void image1D();
    void image1DMipmaps();

    void image2DMipmaps();

    //void image3D();
    //void image3DMipmaps();

    void keyValueDataEmpty();
    void keyValueDataInvalid();

    void orientationEmpty();
    void orientationInvalid();
    void orientationYDown();
    void orientationYUp();
    void orientationFlipCompressed();

    void cube();
    void cubeIncomplete();

    void swizzle();
    void swizzleIdentity();
    void swizzleUnsupported();

    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

using namespace Math::Literals;
const Color3ub PatternRgb2DData[3][4]{
    /* Origin bottom-left */
    {0xff0000_rgb, 0xffffff_rgb, 0x000000_rgb, 0x00ff00_rgb},
    {0xffffff_rgb, 0xff0000_rgb, 0x000000_rgb, 0x00ff00_rgb},
    {0x0000ff_rgb, 0x00ff00_rgb, 0x7f007f_rgb, 0x7f007f_rgb}
};

const Color4ub PatternRgba2DData[3][4]{
    {PatternRgb2DData[0][0], PatternRgb2DData[0][1], PatternRgb2DData[0][2], PatternRgb2DData[0][3]},
    {PatternRgb2DData[1][0], PatternRgb2DData[1][1], PatternRgb2DData[1][2], PatternRgb2DData[1][3]},
    {PatternRgb2DData[2][0], PatternRgb2DData[2][1], PatternRgb2DData[2][2], PatternRgb2DData[2][3]}
};

const struct {
    const char* name;
    const std::size_t length;
    const char* message;
} ShortData[] {
    {"identifier", sizeof(Implementation::KtxHeader::identifier) - 1,
        "file too short, expected 80 bytes for the header but got only 11"},
    {"header", sizeof(Implementation::KtxHeader) - 1,
        "file too short, expected 80 bytes for the header but got only 79"},
    {"level index", sizeof(Implementation::KtxHeader) + sizeof(Implementation::KtxLevel) - 1,
        "file too short, expected 104 bytes for level index but got only 103"},
    {"key/value data", sizeof(Implementation::KtxHeader) + sizeof(Implementation::KtxLevel) + sizeof(UnsignedInt) + sizeof(Implementation::KdfBasicBlockHeader) + 3*sizeof(Implementation::KdfBasicBlockSample),
        "file too short, expected 252 bytes for key/value data but got only 180"},
    {"level data", 287,
        "file too short, expected 288 bytes for level data but got only 287"}
};

const struct {
    const char* name;
    const char* file;
    const std::size_t offset;
    const char value;
    const char* message;
} InvalidData[] {
    {"signature", "rgb.ktx2",
        offsetof(Implementation::KtxHeader, identifier) + sizeof(Implementation::KtxHeader::identifier) - 1, 0,
        "wrong file signature"},
    {"type size", "rgb.ktx2",
        offsetof(Implementation::KtxHeader, typeSize), 7,
        "unsupported type size 7"},
    {"image size x", "rgb.ktx2",
        offsetof(Implementation::KtxHeader, imageSize), 0,
        "invalid image size, width is 0"},
    /** @todo */
    //{"image size y", "rgb.ktx2",
    //    offsetof(Implementation::KtxHeader, imageSize), 0,
    //    "invalid image size, depth is 5 but height is 0"},
    {"face count", "rgb.ktx2",
        offsetof(Implementation::KtxHeader, faceCount), 3,
        "expected either 1 or 6 faces for cube maps but got 3"},
    {"cube not square", "rgb.ktx2",
        offsetof(Implementation::KtxHeader, faceCount), 6,
        "cube map dimensions must be 2D and square, but got Vector(4, 3, 0)"},
    /** @todo */
    //{"cube 3d", "rgb.ktx2",
    //    offsetof(Implementation::KtxHeader, faceCount), 6,
    //    "invalid cubemap dimensions, must be 2D and square, but got Vector(4, 3, 0)"},
    {"level count", "rgb.ktx2",
        offsetof(Implementation::KtxHeader, levelCount), 7,
        "expected at most 3 mip levels but got 7"},
    {"custom format", "rgb.ktx2",
        offsetof(Implementation::KtxHeader, vkFormat), 0,
        "custom formats are not supported"},
    /** @todo get compressed file */
    //{"compressed type size", "rgb.ktx2",
    //    offsetof(Implementation::KtxHeader, typeSize), 4,
    //    "invalid type size for compressed format, expected 1 but got 4"},
    {"supercompression", "rgb.ktx2",
        offsetof(Implementation::KtxHeader, supercompressionScheme), 1,
        "supercompression is currently not supported"},
    /** @todo get depth format and/or 3D format */
    //{"3d depth", "rgb.ktx2",
    //    0, 0,
    //    "3D images can't have depth/stencil format"},
    {"level data too short", "rgb.ktx2",
        sizeof(Implementation::KtxHeader) + offsetof(Implementation::KtxLevel, byteLength), 1,
        "level data too short, expected at least 36 bytes but got 1"}
};

const struct {
    const char* name;
    const char* file;
    const Trade::TextureType type;
} TextureData[] {
    {"1D", "1d.ktx2", Trade::TextureType::Texture1D},
    {"2D", "rgb.ktx2", Trade::TextureType::Texture2D}
    /** @todo one of each texture type */
};

const struct {
    const char* name;
    const char* file;
    const PixelFormat format;
    const Implementation::VkFormat vkFormat;
    const char* message;
    const Containers::ArrayView<const char> data;
} SwizzleData[] {
    {"BGR8 header", "bgr-swizzle-bgr.ktx2",
        PixelFormat::RGB8Srgb, Implementation::VK_FORMAT_UNDEFINED,
        "format requires conversion from BGR to RGB", Containers::arrayCast<const char>(PatternRgb2DData)},
    {"BGRA8 header", "bgra-swizzle-bgra.ktx2",
        PixelFormat::RGBA8Srgb, Implementation::VK_FORMAT_UNDEFINED,
        "format requires conversion from BGRA to RGBA", Containers::arrayCast<const char>(PatternRgba2DData)},
    {"BGR8 format", "bgr.ktx2",
        PixelFormat::RGB8Srgb, Implementation::VK_FORMAT_B8G8R8_SRGB,
        "format requires conversion from BGR to RGB", Containers::arrayCast<const char>(PatternRgb2DData)},
    {"BGRA8 format", "bgra.ktx2",
        PixelFormat::RGBA8Srgb, Implementation::VK_FORMAT_B8G8R8A8_SRGB,
        "format requires conversion from BGRA to RGBA", Containers::arrayCast<const char>(PatternRgba2DData)},
    {"BGR8 format+header cancel", "swizzle-bgr.ktx2",
        PixelFormat::RGB8Srgb, Implementation::VK_FORMAT_B8G8R8_SRGB,
        nullptr, Containers::arrayCast<const char>(PatternRgb2DData)},
    {"BGRA8 format+header cancel", "swizzle-bgra.ktx2",
        PixelFormat::RGBA8Srgb, Implementation::VK_FORMAT_B8G8R8A8_SRGB,
        nullptr, Containers::arrayCast<const char>(PatternRgba2DData)},
    /** @todo Check swizzle of larger formats */
};

KtxImporterTest::KtxImporterTest() {
    addInstancedTests({&KtxImporterTest::openShort},
        Containers::arraySize(ShortData));

    addInstancedTests({&KtxImporterTest::invalid},
        Containers::arraySize(InvalidData));

    addTests({&KtxImporterTest::invalidVersion,
              &KtxImporterTest::invalidFormat});

    addInstancedTests({&KtxImporterTest::texture},
        Containers::arraySize(TextureData));

    addTests({&KtxImporterTest::rgb8,
              &KtxImporterTest::rgba8,

              & KtxImporterTest::image1D,
              & KtxImporterTest::image1DMipmaps,
              & KtxImporterTest::image2DMipmaps,

              &KtxImporterTest::keyValueDataEmpty,
              &KtxImporterTest::keyValueDataInvalid,

              &KtxImporterTest::orientationEmpty,
              &KtxImporterTest::orientationInvalid,
              &KtxImporterTest::orientationYDown,
              &KtxImporterTest::orientationYUp,
              &KtxImporterTest::orientationFlipCompressed,

              &KtxImporterTest::cube,
              &KtxImporterTest::cubeIncomplete});

    addInstancedTests({&KtxImporterTest::swizzle},
        Containers::arraySize(SwizzleData));

    addTests({&KtxImporterTest::swizzleIdentity,
              &KtxImporterTest::swizzleUnsupported,

              &KtxImporterTest::openTwice,
              &KtxImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef KTXIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(KTXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void KtxImporterTest::openShort() {
    auto&& data = ShortData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");

    const auto fileData = Utility::Directory::read(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2"));
    CORRADE_COMPARE(fileData.size(), 288u);

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openData(fileData.prefix(data.length)));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::KtxImporter::openData(): {}\n", data.message));
}

void KtxImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    auto fileData = Utility::Directory::read(Utility::Directory::join(KTXIMPORTER_TEST_DIR, data.file));
    CORRADE_VERIFY(!fileData.empty());

    fileData[data.offset] = data.value;

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openData(fileData));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::KtxImporter::openData(): {}\n", data.message));
}

void KtxImporterTest::invalidVersion() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "version1.ktx")));
    CORRADE_COMPARE(out.str(), "Trade::KtxImporter::openData(): unsupported KTX version, expected 20 but got 11\n");
}

void KtxImporterTest::invalidFormat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");

    auto fileData = Utility::Directory::read(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2"));
    CORRADE_VERIFY(fileData.size() >= sizeof(Implementation::KtxHeader));

    Implementation::KtxHeader& header = *reinterpret_cast<Implementation::KtxHeader*>(fileData.data());

    /* Implementation::VkFormat only contains swizzled 8-bit formats. Taken
       from magnum/src/MagnumExternal/Vulkan/flextVk.h (9d4a8b49943a084cff64550792bb2eba223e0e03) */
    enum VkFormat : UnsignedInt {
        VK_FORMAT_R4G4_UNORM_PACK8 = 1,
        VK_FORMAT_A1R5G5B5_UNORM_PACK16 = 8,
        VK_FORMAT_R8_USCALED = 11,
        VK_FORMAT_R16_SSCALED = 73,
        VK_FORMAT_R64_UINT = 110,
        VK_FORMAT_R64G64B64A64_SFLOAT = 121,
        VK_FORMAT_G8B8G8R8_422_UNORM = 1000156000,
        VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM = 1000156002,
        VK_FORMAT_R10X6G10X6_UNORM_2PACK16 = 1000156008,
        VK_FORMAT_G16B16G16R16_422_UNORM = 1000156027,
        VK_FORMAT_B16G16R16G16_422_UNORM = 1000156028,
        VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM = 1000156029,
        VK_FORMAT_G16_B16R16_2PLANE_420_UNORM = 1000156030,
        VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM = 1000156031,
        VK_FORMAT_G16_B16R16_2PLANE_422_UNORM = 1000156032,
        VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM = 1000156033,
        VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG = 1000054006,
    };

    constexpr Implementation::VkFormat formats[]{
        /* Not allowed by KTX. All of the unsupported formats happen to not be
           supported by Magnum, either. */
        VK_FORMAT_R4G4_UNORM_PACK8,
        VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        VK_FORMAT_R8_USCALED,
        VK_FORMAT_R16_SSCALED,
        VK_FORMAT_G8B8G8R8_422_UNORM,
        VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
        VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
        VK_FORMAT_G16B16G16R16_422_UNORM,
        /* Not supported by Magnum */
        VK_FORMAT_R64_UINT,
        VK_FORMAT_R64G64B64A64_SFLOAT,
        VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG
    };

    std::ostringstream out;
    Error redirectError{&out};

    for(UnsignedInt i = 0; i != Containers::arraySize(formats); ++i) {
        CORRADE_ITERATION(i);
        out.str({}); /* Reset stream content */
        header.vkFormat = Utility::Endianness::littleEndian(formats[i]);
        CORRADE_VERIFY(!importer->openData(fileData));
        CORRADE_COMPARE(out.str(), Utility::formatString("Trade::KtxImporter::openData(): unsupported format {}\n", UnsignedInt(formats[i])));
    }
}

void KtxImporterTest::texture() {
    auto&& data = TextureData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, data.file)));

    const Vector3ui counts{
        importer->image1DCount(),
        importer->image2DCount(),
        importer->image3DCount()
    };
    const UnsignedInt total = counts.sum();

    CORRADE_VERIFY(total > 0);
    CORRADE_COMPARE(counts.max(), total);
    CORRADE_COMPARE(importer->textureCount(), total);

    for(UnsignedInt i = 0; i != total; ++i) {
        CORRADE_ITERATION(i);
        const auto texture = importer->texture(i);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->minificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->magnificationFilter(), SamplerFilter::Linear);
        CORRADE_COMPARE(texture->mipmapFilter(), SamplerMipmap::Linear);
        CORRADE_COMPARE(texture->wrapping(), Math::Vector3<SamplerWrapping>{SamplerWrapping::Repeat});
        CORRADE_COMPARE(texture->image(), i);
        CORRADE_COMPARE(texture->importerState(), nullptr);
        CORRADE_COMPARE(texture->type(), data.type);
    }

    UnsignedInt dimensions;
    switch(data.type) {
        case TextureData::Type::Texture1D:
            dimensions = 1;
            break;
        case TextureData::Type::Texture1DArray:
        case TextureData::Type::Texture2D:
            dimensions = 2;
            break;
        case TextureData::Type::Texture2DArray:
        case TextureData::Type::Texture3D:
        case TextureData::Type::CubeMap:
        case TextureData::Type::CubeMapArray:
            dimensions = 3;
            break;
        default: CORRADE_INTERNAL_ASSERT_UNREACHABLE();
    }
    CORRADE_COMPARE(counts[dimensions - 1], total);
}

void KtxImporterTest::rgb8() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);

    auto image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 4);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(PatternRgb2DData), TestSuite::Compare::Container);
}

void KtxImporterTest::rgba8() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgba.ktx2")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);

    auto image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 4);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(PatternRgba2DData), TestSuite::Compare::Container);
}

void KtxImporterTest::image1D() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "1d.ktx2")));

    CORRADE_COMPARE(importer->image1DCount(), 1);
    CORRADE_COMPARE(importer->image1DLevelCount(0), 1);

    auto image = importer->image1D(0);
    CORRADE_VERIFY(image);

    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
    CORRADE_COMPARE(image->size(), (Math::Vector<1, Int>{3}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 1);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    const Color3ub data[3]{
        0xff0000_rgb, 0xffffff_rgb, 0x000000_rgb
    };

    CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(data), TestSuite::Compare::Container);
}

void KtxImporterTest::image1DMipmaps() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "1d-mipmaps.ktx2")));

    const Containers::Array<Color3ub> mipData[2]{
        {InPlaceInit, {0xff0000_rgb, 0xffffff_rgb, 0x000000_rgb}},
        {InPlaceInit, {0xffffff_rgb}}
    };

    CORRADE_COMPARE(importer->image1DCount(), 1);
    CORRADE_COMPARE(importer->image1DLevelCount(0), Containers::arraySize(mipData));

    Math::Vector<1, Int> mipSize{3};
    for(UnsignedInt i = 0; i != importer->image1DLevelCount(0); ++i) {
        CORRADE_ITERATION(i);

        auto image = importer->image1D(0, i);
        CORRADE_VERIFY(image);

        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), mipSize);

        const PixelStorage storage = image->storage();
        CORRADE_COMPARE(storage.alignment(), 1);
        CORRADE_COMPARE(storage.rowLength(), 0);
        CORRADE_COMPARE(storage.imageHeight(), 0);
        CORRADE_COMPARE(storage.skip(), Vector3i{});

        CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(mipData[i]), TestSuite::Compare::Container);

        mipSize = Math::max(mipSize >> 1, 1);
    }
}

void KtxImporterTest::image2DMipmaps() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb-mipmaps.ktx2")));

    /* Is there a nicer way to get a flat view for a nested array? */
    const auto mipData0 = Containers::arrayCast<const Color3ub>(Containers::arrayView(PatternRgb2DData));
    Containers::Array<Color3ub> mipData[3]{
        Containers::Array<Color3ub>{mipData0.size()},
        {InPlaceInit, {0xff0000_rgb, 0x000000_rgb}},
        {InPlaceInit, {0x000000_rgb}}
    };
    Utility::copy(mipData0, mipData[0]);

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), Containers::arraySize(mipData));

    Vector2i mipSize{4, 3};
    for(UnsignedInt i = 0; i != importer->image2DLevelCount(0); ++i) {
        CORRADE_ITERATION(i);

        auto image = importer->image2D(0, i);
        CORRADE_VERIFY(image);

        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), mipSize);

        const PixelStorage storage = image->storage();
        /* Alignment is 4 when row length is a multiple of 4 */
        const Int alignment = ((mipSize.x()*image->pixelSize())%4 == 0) ? 4 : 1;
        CORRADE_COMPARE(storage.alignment(), alignment);
        CORRADE_COMPARE(storage.rowLength(), 0);
        CORRADE_COMPARE(storage.imageHeight(), 0);
        CORRADE_COMPARE(storage.skip(), Vector3i{});

        CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(mipData[i]), TestSuite::Compare::Container);

        mipSize = Math::max(mipSize >> 1, 1);
    }
}

void KtxImporterTest::keyValueDataEmpty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2")));

    /** @todo */
}

void KtxImporterTest::keyValueDataInvalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2")));

    /** @todo */
}

void KtxImporterTest::orientationEmpty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->addFlags(ImporterFlag::Verbose);

    std::ostringstream outDebug;
    Debug redirectDebug{&outDebug};

    std::ostringstream outWarning;
    Warning redirectWarning{&outWarning};

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "orientation-empty.ktx2")));
    CORRADE_COMPARE(outWarning.str(), "Trade::KtxImporter::openData(): missing or invalid orientation, assuming right, down\n");
    CORRADE_COMPARE(outDebug.str(), "Trade::KtxImporter::openData(): image will be flipped along y\n");
}

void KtxImporterTest::orientationInvalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2")));

    /** @todo */
}

void KtxImporterTest::orientationYDown() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2")));

    /** @todo */
}

void KtxImporterTest::orientationYUp() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2")));

    /** @todo */
}

void KtxImporterTest::orientationFlipCompressed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2")));

    /** @todo */
}

void KtxImporterTest::cube() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2")));

    /** @todo */
}

void KtxImporterTest::cubeIncomplete() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2")));

    /** @todo */
}

void KtxImporterTest::swizzle() {
    auto&& data = SwizzleData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->addFlags(ImporterFlag::Verbose);

    auto fileData = Utility::Directory::read(Utility::Directory::join(KTXIMPORTER_TEST_DIR, data.file));
    CORRADE_VERIFY(fileData.size() > sizeof(Implementation::KtxHeader));

    /* toktx lets us swizzle the input data, but doesn't turn the format into
       a swizzled one. Patch the header manually. */
    if(data.vkFormat != Implementation::VK_FORMAT_UNDEFINED) {
        auto& header = *reinterpret_cast<Implementation::KtxHeader*>(fileData.data());
        header.vkFormat = Utility::Endianness::littleEndian(data.vkFormat);
    }

    std::ostringstream outDebug;
    Debug redirectDebug{&outDebug};

    CORRADE_VERIFY(importer->openData(fileData));

    /** @todo Change origin to top-left for the swizzle test files so we can
              check the relevant messages only. Relevant for swizzleIdentity(), too */
    std::string expectedMessage = "Trade::KtxImporter::openData(): image will be flipped along y\n";
    if(data.message)
        expectedMessage += Utility::formatString("Trade::KtxImporter::openData(): {}\n", data.message);
    CORRADE_COMPARE(outDebug.str(), expectedMessage);

    CORRADE_COMPARE(importer->image2DCount(), 1);
    auto image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->format(), data.format);
    CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));
    CORRADE_COMPARE_AS(image->data(), data.data, TestSuite::Compare::Container);
}

void KtxImporterTest::swizzleIdentity() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->addFlags(ImporterFlag::Verbose);

    std::ostringstream out;
    Debug redirectError{&out};

    /* RGB1 swizzle. This also checks that the correct prefix based on channel
       count is used, since swizzle is always a constant length 4 in the
       key/value data. */
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "swizzle-identity.ktx2")));
    /* No message about format requiring conversion */
    CORRADE_COMPARE(out.str(), "Trade::KtxImporter::openData(): image will be flipped along y\n");
}

void KtxImporterTest::swizzleUnsupported() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");

    std::ostringstream out;
    Error redirectError{&out};

    /* Only identity (RG?B?A?), BGR and BGRA swizzle supported. This is the same
       swizzle string as in swizzle-identity.ktx2, but this file is RGBA instead
       of RGB, so the 1 shouldn't be ignored. */
    CORRADE_VERIFY(!importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "swizzle-unsupported.ktx2")));
    CORRADE_COMPARE(out.str(), "Trade::KtxImporter::openData(): unsupported channel mapping rgb1\n");
}

void KtxImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->textureCount(), 1);

    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->textureCount(), 1);

    /* Shouldn't crash, leak or anything */
}

void KtxImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(KTXIMPORTER_TEST_DIR, "rgb.ktx2")));

    /* Verify that everything is working the same way on second use */
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::KtxImporterTest)
