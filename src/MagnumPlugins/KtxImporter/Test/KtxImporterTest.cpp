/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
              Vladimír Vondruš <mosra@centrum.cz>
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

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/StringIterable.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/PixelStorage.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Swizzle.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#ifdef MAGNUM_BUILD_DEPRECATED
#include <Magnum/Trade/TextureData.h>
#endif

#include "MagnumPlugins/KtxImporter/KtxHeader.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct KtxImporterTest: TestSuite::Tester {
    explicit KtxImporterTest();

    void openShort();

    void invalid();
    void invalidVersion();
    void invalidFormat();

    #ifdef MAGNUM_BUILD_DEPRECATED
    void texture();
    #endif

    void imageRgba();
    void imageRgb32U();
    void imageRgb32F();
    void imageDepthStencil();

    void image1D();
    void image1DMipmaps();
    void image1DLayers();
    void image1DCompressed();
    void image1DCompressedMipmaps();

    void image2D();
    void image2DMipmaps();
    void image2DMipmapsIncomplete();
    void image2DLayers();
    void image2DMipmapsAndLayers();
    void image2DCompressed();
    void image2DCompressedMipmaps();
    void image2DCompressedLayers();

    void imageCubeMapIncomplete();
    void imageCubeMap();
    void imageCubeMapLayers();
    void imageCubeMapMipmaps();

    void image3D();
    void image3DMipmaps();
    void image3DLayers();
    void image3DCompressed();
    void image3DCompressedMipmaps();

    void forwardBasis();
    void forwardBasisFormat();
    void forwardBasisInvalid();
    void forwardBasisPluginNotFound();

    void keyValueDataEmpty();
    template<ImporterFlag flag = ImporterFlag{}> void keyValueDataInvalid();
    template<ImporterFlag flag = ImporterFlag{}> void keyValueDataInvalidIgnored();

    template<ImporterFlag flag = ImporterFlag{}> void orientationInvalid();
    void orientationFlip();
    void orientationFlipCompressed();
    void orientationFlipCompressed3D();

    void swizzle();
    void swizzleMultipleBytes();
    void swizzleIdentity();
    void swizzleUnsupported();
    void swizzleCompressed();

    void openMemory();
    void openTwice();
    void openNormalAfterBasis();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies. A dedicated manager
       that has BasisImporter loaded as well in order to test both with and
       without BasisImporter present. */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _managerWithBasisImporter{"nonexistent"};
    /* For testing Y-flip of block-compressed formats */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};
};

using namespace Math::Literals;

const Color3ub PatternRgb1DData[3][4]{
    /* pattern-1d.png */
    {0xff0000_rgb, 0xffffff_rgb, 0x000000_rgb, 0x007f7f_rgb},
    /* pattern-1d.png */
    {0xff0000_rgb, 0xffffff_rgb, 0x000000_rgb, 0x007f7f_rgb},
    /* black-1d.png */
    {0x000000_rgb, 0x000000_rgb, 0x000000_rgb, 0x000000_rgb}
};

/* Origin bottom-left-forward */
const Color3ub PatternRgbData[3][3][4]{
    /* pattern.png */
    {{0xff0000_rgb, 0xffffff_rgb, 0x000000_rgb, 0x00ff00_rgb},
     {0xffffff_rgb, 0xff0000_rgb, 0x000000_rgb, 0x00ff00_rgb},
     {0x0000ff_rgb, 0x00ff00_rgb, 0x7f007f_rgb, 0x7f007f_rgb}},
    /* pattern.png */
    {{0xff0000_rgb, 0xffffff_rgb, 0x000000_rgb, 0x00ff00_rgb},
     {0xffffff_rgb, 0xff0000_rgb, 0x000000_rgb, 0x00ff00_rgb},
     {0x0000ff_rgb, 0x00ff00_rgb, 0x7f007f_rgb, 0x7f007f_rgb}},
    /* black.png */
    {{0x000000_rgb, 0x000000_rgb, 0x000000_rgb, 0x000000_rgb},
     {0x000000_rgb, 0x000000_rgb, 0x000000_rgb, 0x000000_rgb},
     {0x000000_rgb, 0x000000_rgb, 0x000000_rgb, 0x000000_rgb}}
};

const Color4ub PatternRgba2DData[3][4]{
    {PatternRgbData[0][0][0], PatternRgbData[0][0][1], PatternRgbData[0][0][2], PatternRgbData[0][0][3]},
    {PatternRgbData[0][1][0], PatternRgbData[0][1][1], PatternRgbData[0][1][2], PatternRgbData[0][1][3]},
    {PatternRgbData[0][2][0], PatternRgbData[0][2][1], PatternRgbData[0][2][2], PatternRgbData[0][2][3]}
};

constexpr UnsignedByte PatternStencil8UIData[4*3]{
    1,  2,  3,  4,
    5,  6,  7,  8,
    9, 10, 11, 12
};

constexpr UnsignedShort PatternDepth16UnormData[4*3]{
    0xff01, 0xff02, 0xff03, 0xff04,
    0xff05, 0xff06, 0xff07, 0xff08,
    0xff09, 0xff10, 0xff11, 0xff12
};

constexpr UnsignedInt PatternDepth24UnormStencil8UIData[4*3]{
    0xffffff01, 0xffffff02, 0xffffff03, 0xffffff04,
    0xffffff05, 0xffffff06, 0xffffff07, 0xffffff08,
    0xffffff09, 0xffffff10, 0xffffff11, 0xffffff12
};

constexpr UnsignedLong HalfL = 0x7f7f7f7f7f7f7f7f;
constexpr UnsignedLong FullL = 0xffffffffffffffff;
constexpr UnsignedLong PatternDepth32FStencil8UIData[4*3]{
    0,     0,     0, HalfL,
    0, FullL, FullL, HalfL,
    0, FullL,     0, FullL
};

const struct {
    const char* name;
    std::size_t length;
    const char* message;
} ShortData[]{
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

constexpr UnsignedByte VK_FORMAT_D32_SFLOAT = 126;

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    const char* file;
    ImporterFlags flags;
    std::size_t offset;
    const char value;
    const char* message;
} InvalidData[]{
    {"signature", "2d-rgb.ktx2", {},
        offsetof(Implementation::KtxHeader, identifier) + sizeof(Implementation::KtxHeader::identifier) - 1, 0,
        "wrong file signature"},
    {"type size", "2d-rgb.ktx2", {},
        offsetof(Implementation::KtxHeader, typeSize), 7,
        "unsupported type size 7"},
    {"image size x", "2d-rgb.ktx2", {},
        offsetof(Implementation::KtxHeader, imageSize), 0,
        "invalid image size, width is 0"},
    {"image size y", "3d.ktx2", {},
        offsetof(Implementation::KtxHeader, imageSize) + sizeof(UnsignedInt), 0,
        "invalid image size, depth is 3 but height is 0"},
    {"face count", "2d-rgb.ktx2", {},
        offsetof(Implementation::KtxHeader, faceCount), 3,
        "expected either 1 or 6 faces for cube maps but got 3"},
    {"cube not square", "2d-rgb.ktx2", {},
        offsetof(Implementation::KtxHeader, faceCount), 6,
        "cube map dimensions must be 2D and square, but got Vector(4, 3, 0)"},
    {"cube 3d", "3d.ktx2", {},
        offsetof(Implementation::KtxHeader, faceCount), 6,
        "cube map dimensions must be 2D and square, but got Vector(4, 3, 3)"},
    {"level count", "2d-rgb.ktx2", {},
        offsetof(Implementation::KtxHeader, levelCount), 7,
        "expected at most 3 mip levels but got 7"},
    {"custom format", "2d-rgb.ktx2", {},
        offsetof(Implementation::KtxHeader, vkFormat), 0,
        "custom formats are not supported"},
    {"compressed type size", "2d-compressed-etc2.ktx2", {},
        offsetof(Implementation::KtxHeader, typeSize), 4,
        "invalid type size for compressed format, expected 1 but got 4"},
    {"supercompression", "2d-rgb.ktx2", {},
        offsetof(Implementation::KtxHeader, supercompressionScheme), 1,
        "supercompression is currently not supported"},
    {"3d depth", "3d.ktx2", {},
        offsetof(Implementation::KtxHeader, vkFormat), VK_FORMAT_D32_SFLOAT,
        "3D images can't have depth/stencil format"},
    {"level data too short", "2d-rgb.ktx2", {},
        sizeof(Implementation::KtxHeader) + offsetof(Implementation::KtxLevel, byteLength), 1,
        "Trade::KtxImporter::openData(): byte length 1 is not equal to uncompressed byte length 36 for an image without supercompression, ignoring the latter\n"
        "Trade::KtxImporter::openData(): level data too short, expected at least 36 bytes but got 1\n"},
    {"level data too short, quiet", "2d-rgb.ktx2", ImporterFlag::Quiet,
        sizeof(Implementation::KtxHeader) + offsetof(Implementation::KtxLevel, byteLength), 1,
        "level data too short, expected at least 36 bytes but got 1"},
    {"3D layered level data too short", "3d-layers.ktx2", {},
        sizeof(Implementation::KtxHeader) + offsetof(Implementation::KtxLevel, byteLength), 108,
        "Trade::KtxImporter::openData(): byte length 108 is not equal to uncompressed byte length 216 for an image without supercompression, ignoring the latter\n"
        "Trade::KtxImporter::openData(): level data too short, expected at least 216 bytes but got 108\n"},
    {"3D layered level data too short, quiet", "3d-layers.ktx2", ImporterFlag::Quiet,
        sizeof(Implementation::KtxHeader) + offsetof(Implementation::KtxLevel, byteLength), 108,
        "level data too short, expected at least 216 bytes but got 108"}
};

#ifdef MAGNUM_BUILD_DEPRECATED
const struct {
    const char* name;
    const char* file;
    TextureType type;
} TextureData[]{
    {"1D", "1d.ktx2", TextureType::Texture1D},
    {"1D array", "1d-layers.ktx2", TextureType::Texture1DArray},
    {"2D", "2d-rgb.ktx2", TextureType::Texture2D},
    {"2D array", "2d-layers.ktx2", TextureType::Texture2DArray},
    {"cube map", "cubemap.ktx2", TextureType::CubeMap},
    {"cube map array", "cubemap-layers.ktx2", TextureType::CubeMapArray},
    {"3D", "3d.ktx2", TextureType::Texture3D},
    {"3D array", "3d-layers.ktx2", TextureType::Texture3D}
};
#endif

const struct {
    const char* name;
    const char* file;
    PixelFormat format;
    Containers::ArrayView<const char> data;
} DepthStencilImageData[]{
    {"Stencil8UI", "2d-s8.ktx2", PixelFormat::Stencil8UI,
        Containers::arrayCast<const char>(PatternStencil8UIData)},
    {"Depth16Unorm", "2d-d16.ktx2", PixelFormat::Depth16Unorm,
        Containers::arrayCast<const char>(PatternDepth16UnormData)},
    {"Depth24UnormStencil8UI", "2d-d24s8.ktx2", PixelFormat::Depth24UnormStencil8UI,
        Containers::arrayCast<const char>(PatternDepth24UnormStencil8UIData)},
    {"Depth32FStencil8UI", "2d-d32fs8.ktx2", PixelFormat::Depth32FStencil8UI,
        Containers::arrayCast<const char>(PatternDepth32FStencil8UIData)}
};

const struct {
    const char* name;
    const char* file;
    CompressedPixelFormat format;
    Math::Vector<1, Int> size;
} CompressedImage1DData[]{
    {"BC1", "1d-compressed-bc1.ktx2", CompressedPixelFormat::Bc1RGBASrgb, {4}},
    {"ETC2", "1d-compressed-etc2.ktx2", CompressedPixelFormat::Etc2RGB8Srgb, {7}}
};

const struct {
    const char* name;
    const char* file;
    CompressedPixelFormat format;
    Vector2i size;
} CompressedImage2DData[]{
    {"PVRTC", "2d-compressed-pvrtc.ktx2", CompressedPixelFormat::PvrtcRGBA4bppSrgb, {8, 8}},
    {"BC1", "2d-compressed-bc1.ktx2", CompressedPixelFormat::Bc1RGBASrgb, {8, 8}},
    {"BC3", "2d-compressed-bc3.ktx2", CompressedPixelFormat::Bc3RGBASrgb, {8, 8}},
    {"ETC2", "2d-compressed-etc2.ktx2", CompressedPixelFormat::Etc2RGB8Srgb, {9, 10}},
    {"ASTC", "2d-compressed-astc.ktx2", CompressedPixelFormat::Astc12x10RGBASrgb, {9, 10}}
};

const struct {
    const char* name;
    const char* file;
    CompressedPixelFormat format;
    Vector3i size;
} CompressedImage3DData[]{
    {"ETC2 RGB8", "3d-compressed-etc2rgb8.ktx2", CompressedPixelFormat::Etc2RGB8Srgb, {9, 10, 3}},
    {"ASTC 3D", "3d-compressed-astc3d.ktx2", CompressedPixelFormat::Astc3x3x3RGBAUnorm, {27, 27, 3}}
};

const struct {
    const char* name;
    ImporterFlags flags;
    bool quiet;
} QuietData[]{
    {"", {}, false},
    {"quiet", ImporterFlag::Quiet, true}
};

const struct {
    const char* name;
    ImporterFlags flags;
    const char* file;
    ImageFlags3D imageFlags;
    #ifdef MAGNUM_BUILD_DEPRECATED
    TextureType type;
    #endif
    UnsignedInt images;
    Vector3i size;
    const char* verboseMessage;
} ForwardBasisData[]{
    /* Basis has no 1D image support */
    {"2D", ImporterFlags{}, "rgba.ktx2", {},
        #ifdef MAGNUM_BUILD_DEPRECATED
        TextureType::Texture2D,
        #endif
        1, {63, 27, 0}, ""},
    {"2D verbose", ImporterFlag::Verbose, "rgba.ktx2", {},
        #ifdef MAGNUM_BUILD_DEPRECATED
        TextureType::Texture2D,
        #endif
        1, {63, 27, 0},
        "Trade::KtxImporter::openData(): image is compressed with Basis Universal, forwarding to BasisImporter\n"
        "Trade::BasisImporter::openData(): image will be flipped along Y\n"},
    {"2D array", ImporterFlags{}, "rgba-array.ktx2", ImageFlag3D::Array,
        #ifdef MAGNUM_BUILD_DEPRECATED
        TextureType::Texture2DArray,
        #endif
        1, {63, 27, 3}, ""},
    {"video", ImporterFlags{}, "rgba-video.ktx2", {},
        #ifdef MAGNUM_BUILD_DEPRECATED
        TextureType::Texture2D,
        #endif
        3, {63, 27, 0}, ""},
    {"video verbose", ImporterFlag::Verbose, "rgba-video.ktx2", {},
        #ifdef MAGNUM_BUILD_DEPRECATED
        TextureType::Texture2D,
        #endif
        3, {63, 27, 0},
        "Trade::KtxImporter::openData(): image is compressed with Basis Universal, forwarding to BasisImporter\n"
        "Trade::BasisImporter::openData(): image will be flipped along Y\n"
        "Trade::BasisImporter::openData(): file contains video frames, images must be transcoded sequentially\n"}
};

const struct {
    const char* name;
    const char* file;
    bool requiresBasisImporter;
    std::size_t offset;
    char value;
    const char* message;
} ForwardBasisInvalidData[]{
    /* Change ktx2_etc1s_global_data_header::m_endpoint_count */
    {"fails in BasisImporter", "rgba.ktx2", true, 200, 0,
        "Trade::BasisImporter::openData(): bad KTX2 file\n"},
    {"file too short for DFD", "rgba-uastc.ktx2", false, offsetof(Implementation::KtxHeader, dfdByteOffset) + 2, 1,
        "Trade::KtxImporter::openData(): file too short, expected 65684 bytes for data format descriptor but got only 789\n"},
    {"DFD too short", "rgba-uastc.ktx2", false, offsetof(Implementation::KtxHeader, dfdByteLength), sizeof(UnsignedInt) + sizeof(Implementation::KdfBasicBlockHeader) - 1,
        "Trade::KtxImporter::openData(): data format descriptor too short, expected at least 28 bytes but got 27\n"}
};

const struct {
    const char* name;
    const char* ktxFormat;
    const char* basisFormat;
    ImporterFlags flags;
    CompressedPixelFormat expectedFormat;
    const char* expectedWarning;
} ForwardBasisFormatData[]{
    {"set in KtxImporter", "Etc2RGBA", nullptr, {},
        CompressedPixelFormat::Etc2RGBA8Srgb,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"},
    {"set in BasisImporter", nullptr, "Bc3RGBA", {},
        CompressedPixelFormat::Bc3RGBASrgb,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"},
    {"set in both to different", "Etc2RGBA", "Bc3RGBA", {},
        CompressedPixelFormat::Etc2RGBA8Srgb,
        "Trade::KtxImporter::openData(): overwriting BasisImporter format from Bc3RGBA to Etc2RGBA\n"
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"},
    {"set in both to different, quiet", "Etc2RGBA", "Bc3RGBA", ImporterFlag::Quiet,
        CompressedPixelFormat::Etc2RGBA8Srgb,
        ""},
    {"set in both to the same", "Bc3RGBA", "Bc3RGBA", {},
        CompressedPixelFormat::Bc3RGBASrgb,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"}
};

using namespace Containers::Literals;

const struct {
    const char* name;
    Containers::StringView data;
    const char* message;
} InvalidKeyValueData[]{
    /* Entry has length 0, followed by a valid entry (with an empty value,
       that's allowed) */
    {"zero length", "\x00\x00\x00\x00\x02\x00\x00\x00k\x00\x00\x00"_s, "invalid key/value entry, skipping"},
    /* Key has length 0, followed by padding + a valid entry */
    {"empty key", "\x02\x00\x00\x00\x00v\x00\x00\x02\x00\x00\x00k\x00\x00\x00"_s, "invalid key/value entry, skipping"},
    /* Duplicate key check only happens for specific keys used later */
    {"duplicate key", "\x10\x00\x00\x00KTXswizzle\x00rgba\x00\x10\x00\x00\x00KTXswizzle\x00rgba\x00"_s, "key KTXswizzle already set, skipping"},
};

const struct {
    const char* name;
    Containers::StringView data;
} IgnoredInvalidKeyValueData[]{
    /* Length extends beyond key/value data */
    {"length out of range", "\xff\x00\x00\x00k\x00\x00\x00"_s},
    /* Importer shouldn't care about order of keys */
    {"unsorted keys", "\x02\x00\x00\x00b\x00\x00\x00\x02\x00\x00\x00a\x00\x00\x00"_s}
};

const struct {
    const char* name;
    const char* file;
    UnsignedInt dimensions;
    Containers::StringView orientation;
    bool assume;
} InvalidOrientationData[]{
    {"empty", "1d.ktx2", 1, ""_s, false},
    {"short", "2d-rgb.ktx2", 2, "r"_s, false},
    /** @todo test too long (e.g., 2D-layers.ktx2 but with rdi) */
    {"invalid x", "2d-rgb.ktx2", 2, "xd"_s, false},
    {"invalid y", "2d-rgb.ktx2", 2, "rx"_s, false},
    {"invalid y from assumeOrientation", "2d-rgb.ktx2", 2, "rx"_s, true},
    {"invalid z", "3d.ktx2", 3, "rux"_s, false},
};

const struct {
    const char* name;
    const char* orientation;
    const char* file;
    Vector3i size;
    PixelFormat format;
    Containers::ArrayView<const char> data;
    ImporterFlags flags;
    Vector3ub flipped;
    bool assume;
    const char* message;
} FlipData[]{
    /* Don't test everything, just a few common and interesting orientations */
    {"l", "l", "1d.ktx2", {4, 0, 0}, PixelFormat::RGB8Srgb,
        Containers::arrayCast<const char>(PatternRgb1DData[0]),
        {}, {true, false, false}, false, nullptr},
    {"l, verbose", "l", "1d.ktx2", {4, 0, 0}, PixelFormat::RGB8Srgb,
        Containers::arrayCast<const char>(PatternRgb1DData[0]),
        ImporterFlag::Verbose, {true, false, false}, false,
        "Trade::KtxImporter::openData(): image will be flipped along X\n"},
    {"l, assumed, verbose", "l", "1d.ktx2", {4, 0, 0}, PixelFormat::RGB8Srgb,
        Containers::arrayCast<const char>(PatternRgb1DData[0]),
        ImporterFlag::Verbose, {true, false, false}, true,
        "Trade::KtxImporter::openData(): image will be flipped along X\n"},
    /* Nothing is flipped, so shouldn't print any message */
    {"r, verbose", "r", "1d.ktx2", {4, 0, 0}, PixelFormat::RGB8Srgb,
        Containers::arrayCast<const char>(PatternRgb1DData[0]),
        ImporterFlag::Verbose, {false, false, false}, false, nullptr},
    /* Value of flipped is relative to the orientation on disk. Files are rd[i],
       the ground truth data expects a flip to ru[o]. */
    {"lu", "lu", "2d-rgb.ktx2", {4, 3, 0}, PixelFormat::RGB8Srgb,
        Containers::arrayCast<const char>(PatternRgbData[0]),
        {}, {true, true, false}, false, nullptr},
    {"rd, verbose", "rd", "2d-rgb.ktx2", {4, 3, 0}, PixelFormat::RGB8Srgb,
        Containers::arrayCast<const char>(PatternRgbData[0]),
        ImporterFlag::Verbose, {false, false, false}, false,
        "Trade::KtxImporter::openData(): image will be flipped along Y\n"},
    {"luo", "luo", "3d.ktx2", {4, 3, 3}, PixelFormat::RGB8Srgb,
        Containers::arrayCast<const char>(PatternRgbData),
        {}, {true, true, true}, false, nullptr},
    {"rdo", "rdo", "3d.ktx2", {4, 3, 3}, PixelFormat::RGB8Srgb,
        Containers::arrayCast<const char>(PatternRgbData),
        {}, {false, false, true}, false, nullptr},
    {"rdi, verbose", "rdi", "3d.ktx2", {4, 3, 3}, PixelFormat::RGB8Srgb,
        Containers::arrayCast<const char>(PatternRgbData),
        ImporterFlag::Verbose, {false, false, false}, false,
        "Trade::KtxImporter::openData(): image will be flipped along Y and Z\n"},
    /* Nothing is flipped, so shouldn't print any message */
    {"ruo, verbose", "ruo", "3d.ktx2", {4, 3, 3}, PixelFormat::RGB8Srgb,
        Containers::arrayCast<const char>(PatternRgbData),
        ImporterFlag::Verbose, {false, true, true}, false, nullptr},
    {"ruo, assumed, verbose", "ruo", "3d.ktx2", {4, 3, 3}, PixelFormat::RGB8Srgb,
        Containers::arrayCast<const char>(PatternRgbData),
        ImporterFlag::Verbose, {false, true, true}, true, nullptr},
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    const char* filename;
    ImporterFlags flags;
    const char* assumeOrientation;
    bool flippedY;
    const char* message;
} FlipCompressedData[]{
    /* Picking files that look like being converted from a real image in
       convert.sh, and not from something empty in which case a Y-flipped file
       would look the same as not Y-flipped */
    {"BC1",
        "2d-compressed-bc1.ktx2", {}, nullptr, true, nullptr},
    {"BC1, verbose",
        "2d-compressed-bc1.ktx2", ImporterFlag::Verbose, nullptr, true,
        "Trade::KtxImporter::openData(): image will be flipped along Y\n"},
    {"BC1, assume Y up, verbose",
        "2d-compressed-bc1.ktx2", ImporterFlag::Verbose, "ruo", false, nullptr},
    {"BC1, assume X left",
        "2d-compressed-bc1.ktx2", {}, "ld", true,
        "Trade::KtxImporter::openData(): X flip is not implemented for block-compressed images, imported data will have wrong orientation. Set the assumeOrientation option to suppress this warning.\n"},
    {"BC1, assume X left, verbose",
        "2d-compressed-bc1.ktx2", ImporterFlag::Verbose, "ld", true,
        "Trade::KtxImporter::openData(): X flip is not implemented for block-compressed images, imported data will have wrong orientation. Set the assumeOrientation option to suppress this warning.\n"
        "Trade::KtxImporter::openData(): image will be flipped along Y\n"},
    {"BC1, assume X left, quiet",
        "2d-compressed-bc1.ktx2", ImporterFlag::Quiet, "ld", true, nullptr},
    {"BC2",
        "2d-compressed-bc2.ktx2", {}, nullptr, true, nullptr},
    {"BC2, Y up, verbose",
        "2d-compressed-bc2-ru.ktx2", ImporterFlag::Verbose, nullptr, false, nullptr},
    {"BC2, assume Y up, verbose",
        "2d-compressed-bc3.ktx2", ImporterFlag::Verbose, "ru", false, nullptr},
    {"BC3",
        "2d-compressed-bc3.ktx2", {}, nullptr, true, nullptr},
    {"BC4, incomplete blocks",
        "2d-compressed-bc4.ktx2", {}, nullptr, true,
        "Trade::KtxImporter::image2D(): Y-flipping a compressed image that's not whole blocks, the result will be shifted by 2 pixels\n"},
    {"BC5, incomplete blocks",
        "2d-compressed-bc5.ktx2", {}, nullptr, true,
        "Trade::KtxImporter::image2D(): Y-flipping a compressed image that's not whole blocks, the result will be shifted by 2 pixels\n"},
    {"ETC2, flip not implemented",
        "2d-compressed-etc2.ktx2", {}, nullptr, false,
        "Trade::KtxImporter::openData(): Y flip is not yet implemented for CompressedPixelFormat::Etc2RGB8Srgb, imported data will have wrong orientation. Set the assumeOrientation option to suppress this warning.\n"},
    {"ETC2, flip not implemented, quiet",
        "2d-compressed-etc2.ktx2", ImporterFlag::Quiet, nullptr, false, nullptr},
    {"ETC2, flip not implemented, assume Y up, verbose",
        "2d-compressed-etc2.ktx2", ImporterFlag::Verbose, "ru", false, nullptr},
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    const char* filename;
    ImporterFlags flags;
    const char* assumeOrientation;
    bool flippedY, flippedZ;
    const char* message;
} FlipCompressed3DData[]{
    {"BC1, incomplete blocks",
        "3d-compressed-bc1.ktx2", {}, nullptr, true, true,
        "Trade::KtxImporter::image3D(): Y-flipping a compressed image that's not whole blocks, the result will be shifted by 3 pixels\n"},
    {"BC1, incomplete blocks, quiet",
        "3d-compressed-bc1.ktx2", ImporterFlag::Quiet, nullptr, true, true, nullptr},
    {"BC1, incomplete blocks, verbose",
        "3d-compressed-bc1.ktx2", ImporterFlag::Verbose, nullptr, true, true,
        "Trade::KtxImporter::openData(): image will be flipped along Y and Z\n"
        "Trade::KtxImporter::image3D(): Y-flipping a compressed image that's not whole blocks, the result will be shifted by 3 pixels\n"},
    {"BC1, incomplete blocks, Y up Z backward, verbose",
        "3d-compressed-bc1-ruo.ktx2", ImporterFlag::Verbose, nullptr, false, false, nullptr},
    {"BC1, incomplete blocks, assume Y up, verbose",
        "3d-compressed-bc1.ktx2", ImporterFlag::Verbose, "rui", false, true,
        "Trade::KtxImporter::openData(): image will be flipped along Z\n"},
    {"BC1, incomplete blocks, assume Z backward, verbose",
        "3d-compressed-bc1.ktx2", ImporterFlag::Verbose, "rdo", true, false,
        "Trade::KtxImporter::openData(): image will be flipped along Y\n"
        "Trade::KtxImporter::image3D(): Y-flipping a compressed image that's not whole blocks, the result will be shifted by 3 pixels\n"},
    {"BC7, Y flip not implemented, verbose",
        "3d-compressed-bc7.ktx2", ImporterFlag::Verbose, nullptr, false, true,
        "Trade::KtxImporter::openData(): Y flip is not yet implemented for CompressedPixelFormat::Bc7RGBAUnorm, imported data will have wrong orientation. Set the assumeOrientation option to suppress this warning.\n"
        "Trade::KtxImporter::openData(): image will be flipped along Z\n"},
    {"ASTC 3D, neither Y nor Z flip implemented, verbose",
        "3d-compressed-astc3d.ktx2", ImporterFlag::Verbose, nullptr, false, false,
        "Trade::KtxImporter::openData(): Y flip is not yet implemented for CompressedPixelFormat::Astc3x3x3RGBAUnorm, imported data will have wrong orientation. Set the assumeOrientation option to suppress this warning.\n"
        "Trade::KtxImporter::openData(): Z flip is not yet implemented for CompressedPixelFormat::Astc3x3x3RGBAUnorm, imported data will have wrong orientation. Set the assumeOrientation option to suppress this warning.\n"},
    {"ASTC 3D, Y up Z forward, verbose",
        "3d-compressed-astc3d-ruo.ktx2", ImporterFlag::Verbose, nullptr, false, false, nullptr},
};

const struct {
    const char* name;
    const char* file;
    PixelFormat format;
    Implementation::VkFormat vkFormat;
    const char* message;
    Containers::ArrayView<const char> data;
} SwizzleData[]{
    {"BGR8 header", "bgr-swizzle-bgr.ktx2",
        PixelFormat::RGB8Srgb, Implementation::VK_FORMAT_UNDEFINED,
        "format requires conversion from BGR to RGB", Containers::arrayCast<const char>(PatternRgbData[0])},
    {"BGRA8 header", "bgra-swizzle-bgra.ktx2",
        PixelFormat::RGBA8Srgb, Implementation::VK_FORMAT_UNDEFINED,
        "format requires conversion from BGRA to RGBA", Containers::arrayCast<const char>(PatternRgba2DData)},
    {"BGR8 format", "bgr.ktx2",
        PixelFormat::RGB8Srgb, Implementation::VK_FORMAT_B8G8R8_SRGB,
        "format requires conversion from BGR to RGB", Containers::arrayCast<const char>(PatternRgbData[0])},
    {"BGRA8 format", "bgra.ktx2",
        PixelFormat::RGBA8Srgb, Implementation::VK_FORMAT_B8G8R8A8_SRGB,
        "format requires conversion from BGRA to RGBA", Containers::arrayCast<const char>(PatternRgba2DData)},
    {"BGR8 format+header cancel", "swizzle-bgr.ktx2",
        PixelFormat::RGB8Srgb, Implementation::VK_FORMAT_B8G8R8_SRGB,
        nullptr, Containers::arrayCast<const char>(PatternRgbData[0])},
    {"BGRA8 format+header cancel", "swizzle-bgra.ktx2",
        PixelFormat::RGBA8Srgb, Implementation::VK_FORMAT_B8G8R8A8_SRGB,
        nullptr, Containers::arrayCast<const char>(PatternRgba2DData)},
    {"depth header ignored", "swizzle-bgra.ktx2",
        PixelFormat::Depth32F, VK_FORMAT_D32_SFLOAT,
        nullptr, Containers::arrayCast<const char>(PatternRgba2DData)}
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

Containers::Array<char> createKeyValueData(Containers::StringView key, Containers::ArrayView<const char> value, bool terminatingZero = false) {
    UnsignedInt size = key.size() + 1 + value.size() + UnsignedInt(terminatingZero);
    size = (size + 3)/4*4;
    Containers::Array<char> keyValueData{ValueInit, sizeof(UnsignedInt) + size};

    std::size_t offset = 0;
    *reinterpret_cast<UnsignedInt*>(keyValueData.data()) = Utility::Endianness::littleEndian(size);
    offset += sizeof(size);
    Utility::copy(key, keyValueData.sliceSize(offset, key.size()));
    offset += key.size() + 1;
    Utility::copy(value, keyValueData.sliceSize(offset, value.size()));

    return keyValueData;
}

Containers::Array<char> createKeyValueData(Containers::StringView key, Containers::StringView value) {
    return createKeyValueData(key, value, true);
}

void patchKeyValueData(Containers::ArrayView<const char> keyValueData, Containers::ArrayView<char> fileData) {
    CORRADE_INTERNAL_ASSERT(fileData.size() >= sizeof(Implementation::KtxHeader));
    Implementation::KtxHeader& header = *reinterpret_cast<Implementation::KtxHeader*>(fileData.data());
    Utility::Endianness::littleEndianInPlace(header.kvdByteOffset, header.kvdByteLength);

    CORRADE_INTERNAL_ASSERT(header.kvdByteOffset + keyValueData.size() <= fileData.size());
    CORRADE_INTERNAL_ASSERT(header.kvdByteLength >= keyValueData.size());
    header.kvdByteLength = keyValueData.size();
    Utility::copy(keyValueData, fileData.sliceSize(header.kvdByteOffset, header.kvdByteLength));

    Utility::Endianness::littleEndianInPlace(header.kvdByteOffset, header.kvdByteLength);
}

KtxImporterTest::KtxImporterTest() {
    addInstancedTests({&KtxImporterTest::openShort},
        Containers::arraySize(ShortData));

    addInstancedTests({&KtxImporterTest::invalid},
        Containers::arraySize(InvalidData));

    addTests({&KtxImporterTest::invalidVersion,
              &KtxImporterTest::invalidFormat});

    #ifdef MAGNUM_BUILD_DEPRECATED
    addInstancedTests({&KtxImporterTest::texture},
        Containers::arraySize(TextureData));
    #endif

    addTests({&KtxImporterTest::imageRgba,
              &KtxImporterTest::imageRgb32U,
              &KtxImporterTest::imageRgb32F});

    addInstancedTests({&KtxImporterTest::imageDepthStencil},
        Containers::arraySize(DepthStencilImageData));

    addTests({&KtxImporterTest::image1D,
              &KtxImporterTest::image1DMipmaps,
              &KtxImporterTest::image1DLayers});

    addInstancedTests({&KtxImporterTest::image1DCompressed},
        Containers::arraySize(CompressedImage1DData));

    addTests({&KtxImporterTest::image1DCompressedMipmaps,

              &KtxImporterTest::image2D,
              &KtxImporterTest::image2DMipmaps,
              &KtxImporterTest::image2DMipmapsIncomplete,
              &KtxImporterTest::image2DLayers,
              &KtxImporterTest::image2DMipmapsAndLayers});

    addInstancedTests({&KtxImporterTest::image2DCompressed},
        Containers::arraySize(CompressedImage2DData));

    addTests({&KtxImporterTest::image2DCompressedMipmaps,
              &KtxImporterTest::image2DCompressedLayers});

    addInstancedTests({&KtxImporterTest::imageCubeMapIncomplete},
        Containers::arraySize(QuietData));

    addTests({&KtxImporterTest::imageCubeMap,
              &KtxImporterTest::imageCubeMapLayers,
              &KtxImporterTest::imageCubeMapMipmaps,

              &KtxImporterTest::image3D,
              &KtxImporterTest::image3DMipmaps,
              &KtxImporterTest::image3DLayers});

    addInstancedTests({&KtxImporterTest::image3DCompressed},
        Containers::arraySize(CompressedImage3DData));

    addTests({&KtxImporterTest::image3DCompressedMipmaps});

    addInstancedTests({&KtxImporterTest::forwardBasis},
        Containers::arraySize(ForwardBasisData));

    addInstancedTests({&KtxImporterTest::forwardBasisFormat},
        Containers::arraySize(ForwardBasisFormatData));

    addInstancedTests({&KtxImporterTest::forwardBasisInvalid},
        Containers::arraySize(ForwardBasisInvalidData));

    addTests({&KtxImporterTest::forwardBasisPluginNotFound});

    addInstancedTests({&KtxImporterTest::keyValueDataEmpty},
        Containers::arraySize(QuietData));

    /* MSVC needs explicit type due to default template args */
    addInstancedTests<KtxImporterTest>({
        &KtxImporterTest::keyValueDataInvalid,
        &KtxImporterTest::keyValueDataInvalid<ImporterFlag::Quiet>},
        Containers::arraySize(InvalidKeyValueData));

    /* MSVC needs explicit type due to default template args */
    addInstancedTests<KtxImporterTest>({
        &KtxImporterTest::keyValueDataInvalidIgnored,
        &KtxImporterTest::keyValueDataInvalidIgnored<ImporterFlag::Quiet>},
        Containers::arraySize(IgnoredInvalidKeyValueData));

    /* MSVC needs explicit type due to default template args */
    addInstancedTests<KtxImporterTest>({
        &KtxImporterTest::orientationInvalid,
        &KtxImporterTest::orientationInvalid<ImporterFlag::Quiet>},
        Containers::arraySize(InvalidOrientationData));

    addInstancedTests({&KtxImporterTest::orientationFlip},
        Containers::arraySize(FlipData));

    addInstancedTests({&KtxImporterTest::orientationFlipCompressed},
        Containers::arraySize(FlipCompressedData));

    addInstancedTests({&KtxImporterTest::orientationFlipCompressed3D},
        Containers::arraySize(FlipCompressed3DData));

    addInstancedTests({&KtxImporterTest::swizzle},
        Containers::arraySize(SwizzleData));

    addTests({&KtxImporterTest::swizzleMultipleBytes,
              &KtxImporterTest::swizzleIdentity,
              &KtxImporterTest::swizzleUnsupported,
              &KtxImporterTest::swizzleCompressed});

    addInstancedTests({&KtxImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&KtxImporterTest::openTwice,
              &KtxImporterTest::openNormalAfterBasis,
              &KtxImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef KTXIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(KTXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    CORRADE_INTERNAL_ASSERT_OUTPUT(_managerWithBasisImporter.load(KTXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef BASISIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_managerWithBasisImporter.load(BASISIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef BCDECIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(BCDECIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef ETCDECIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(ETCDECIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void KtxImporterTest::openShort() {
    auto&& data = ShortData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");

    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgb.ktx2"));
    CORRADE_VERIFY(fileData);
    CORRADE_COMPARE_AS(data.length, fileData->size(), TestSuite::Compare::Less);

    Containers::String out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openData(fileData->prefix(data.length)));
    CORRADE_COMPARE(out, Utility::format("Trade::KtxImporter::openData(): {}\n", data.message));
}

void KtxImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->addFlags(data.flags);

    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file));
    CORRADE_COMPARE_AS(data.offset, fileData->size(), TestSuite::Compare::Less);

    (*fileData)[data.offset] = data.value;

    Containers::String out;
    {
        Error redirectError{&out};
        Warning redirectWarning{&out};
        CORRADE_VERIFY(!importer->openData(*fileData));
    }
    /* Sometimes there's also a warning, in that case take the message
       verbatim */
    if(Containers::StringView{data.message}.hasSuffix('\n'))
        CORRADE_COMPARE_AS(out,
            data.message,
            TestSuite::Compare::String);
    else
        CORRADE_COMPARE_AS(out,
            Utility::format("Trade::KtxImporter::openData(): {}\n", data.message),
            TestSuite::Compare::String);
}

void KtxImporterTest::invalidVersion() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");

    Containers::String out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "version1.ktx")));
    CORRADE_COMPARE(out, "Trade::KtxImporter::openData(): unsupported KTX version, expected 20 but got 11\n");
}

void KtxImporterTest::invalidFormat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");

    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgb.ktx2"));
    CORRADE_VERIFY(fileData);
    CORRADE_COMPARE_AS(fileData->size(), sizeof(Implementation::KtxHeader), TestSuite::Compare::GreaterOrEqual);

    Implementation::KtxHeader& header = *reinterpret_cast<Implementation::KtxHeader*>(fileData->data());

    /* Selected unsupported formats. Implementation::VkFormat only contains
       swizzled 8-bit formats so we have to define our own.
       Taken from magnum/src/MagnumExternal/Vulkan/flextVk.h
       (commit 9d4a8b49943a084cff64550792bb2eba223e0e03) */
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
        VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG = 1000054006
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

    for(UnsignedInt i = 0; i != Containers::arraySize(formats); ++i) {
        CORRADE_ITERATION(i);
        header.vkFormat = Utility::Endianness::littleEndian(formats[i]);

        Containers::String out;
        Error redirectError{&out};
        CORRADE_VERIFY(!importer->openData(*fileData));
        CORRADE_COMPARE(out, Utility::format("Trade::KtxImporter::openData(): unsupported format {}\n", UnsignedInt(formats[i])));
    }
}

#ifdef MAGNUM_BUILD_DEPRECATED
void KtxImporterTest::texture() {
    auto&& data = TextureData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file)));

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
        Containers::Optional<Trade::TextureData> texture = importer->texture(i);
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
        case TextureType::Texture1D:
            dimensions = 1;
            break;
        case TextureType::Texture1DArray:
        case TextureType::Texture2D:
            dimensions = 2;
            break;
        case TextureType::Texture2DArray:
        case TextureType::Texture3D:
        case TextureType::CubeMap:
        case TextureType::CubeMapArray:
            dimensions = 3;
            break;
        default: CORRADE_INTERNAL_ASSERT_UNREACHABLE();
    }
    CORRADE_COMPARE(counts[dimensions - 1], total);
}
#endif

void KtxImporterTest::imageRgba() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgba.ktx2")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 4);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(PatternRgba2DData), TestSuite::Compare::Container);
}

void KtxImporterTest::imageRgb32U() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgb32.ktx2")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->format(), PixelFormat::RGB32UI);
    CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 4);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    /* Output of PVRTexTool with format conversion. This is PatternRgbData[0],
       but each byte extended to uint by just repeating the byte 4 times. */
    constexpr UnsignedInt Half = 0x7f7f7f7f;
    constexpr Math::Color3<UnsignedInt> content[4*3]{
        {~0u,   0,   0}, {~0u, ~0u, ~0u}, {   0, 0,    0}, {   0, ~0u,    0},
        {~0u, ~0u, ~0u}, {~0u,   0,   0}, {   0, 0,    0}, {   0, ~0u,    0},
        {  0,   0, ~0u}, {  0, ~0u,   0}, {Half, 0, Half}, {Half,   0, Half}
    };

    CORRADE_COMPARE_AS(Containers::arrayCast<const Math::Color3<UnsignedInt>>(image->data()),
        Containers::arrayView(content), TestSuite::Compare::Container);
}

void KtxImporterTest::imageRgb32F() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgbf32.ktx2")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->format(), PixelFormat::RGB32F);
    CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 4);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    /* Output of PVRTexTool with format conversion. This is PatternRgbData[0],
       but each byte mapped to the range 0.0 - 1.0. */
    constexpr Float Half = 127.0f/255.0f;
    constexpr Math::Color3<Float> content[4*3]{
        {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {Half, 0.0f, Half}, {Half, 0.0f, Half}
    };

    CORRADE_COMPARE_AS(Containers::arrayCast<const Math::Color3<Float>>(image->data()),
        Containers::arrayView(content), TestSuite::Compare::Container);
}

void KtxImporterTest::imageDepthStencil() {
    auto&& data = DepthStencilImageData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file)));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->format(), data.format);
    CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 4);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    CORRADE_COMPARE_AS(image->data(), data.data, TestSuite::Compare::Container);
}

void KtxImporterTest::image1D() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "1d.ktx2")));

    CORRADE_COMPARE(importer->image1DCount(), 1);
    CORRADE_COMPARE(importer->image1DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData1D> image = importer->image1D(0);
    CORRADE_VERIFY(image);

    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags1D{});
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
    CORRADE_COMPARE(image->size(), (Math::Vector<1, Int>{4}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 4);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(PatternRgb1DData[0]), TestSuite::Compare::Container);
}

void KtxImporterTest::image1DMipmaps() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "1d-mipmaps.ktx2")));

    Containers::ArrayView<const Color3ub> mip0 = Containers::arrayView(PatternRgb1DData[0]);
    const Color3ub mip1[2]{0xffffff_rgb, 0x007f7f_rgb};
    const Color3ub mip2[1]{0x000000_rgb};
    const Containers::ArrayView<const Color3ub> mipViews[3]{mip0, mip1, mip2};

    CORRADE_COMPARE(importer->image1DCount(), 1);
    CORRADE_COMPARE(importer->image1DLevelCount(0), Containers::arraySize(mipViews));

    Math::Vector<1, Int> mipSize{4};
    for(UnsignedInt i = 0; i != importer->image1DLevelCount(0); ++i) {
        CORRADE_ITERATION(i);

        Containers::Optional<Trade::ImageData1D> image = importer->image1D(0, i);
        CORRADE_VERIFY(image);

        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlags1D{});
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), mipSize);

        const PixelStorage storage = image->storage();
        /* Alignment is 4 when row length is a multiple of 4 */
        const Int alignment = ((mipSize[0]*image->pixelSize())%4 == 0) ? 4 : 1;
        CORRADE_COMPARE(storage.alignment(), alignment);
        CORRADE_COMPARE(storage.rowLength(), 0);
        CORRADE_COMPARE(storage.imageHeight(), 0);
        CORRADE_COMPARE(storage.skip(), Vector3i{});

        CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(mipViews[i]), TestSuite::Compare::Container);

        mipSize = Math::max(mipSize >> 1, 1);
    }
}

void KtxImporterTest::image1DLayers() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "1d-layers.ktx2")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlag2D::Array);
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 4);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(PatternRgb1DData), TestSuite::Compare::Container);
}

void KtxImporterTest::image1DCompressed() {
    auto&& data = CompressedImage1DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file)));

    CORRADE_COMPARE(importer->image1DCount(), 1);
    CORRADE_COMPARE(importer->image1DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData1D> image = importer->image1D(0);
    CORRADE_VERIFY(image);

    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags1D{});
    CORRADE_COMPARE(image->compressedFormat(), data.format);
    CORRADE_COMPARE(image->size(), data.size);

    const CompressedPixelStorage storage = image->compressedStorage();
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    /* The compressed data is the output of PVRTexTool, nothing hand-crafted.
       Use --save-diagnostic to extract them if they're missing or wrong. The
       same files are re-used in the tests for KtxImageConverter as input data. */
    /** @todo remove this once CompressedImage etc. tests for data size on its
        own */
    const Vector3i blockSize = compressedPixelFormatBlockSize(data.format);
    const Vector3i blockCount = (Vector3i::pad(data.size, 1) + (blockSize - Vector3i{1}))/blockSize;
    CORRADE_COMPARE(image->data().size(), blockCount.product()*compressedPixelFormatBlockDataSize(data.format));
    CORRADE_COMPARE_AS(Containers::StringView{image->data()},
        Utility::Path::join(KTXIMPORTER_TEST_DIR, Utility::Path::splitExtension(data.file).first() + ".bin"),
        TestSuite::Compare::StringToFile);
}

void KtxImporterTest::image1DCompressedMipmaps() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "1d-compressed-mipmaps.ktx2")));

    CORRADE_COMPARE(importer->image1DCount(), 1);
    CORRADE_COMPARE(importer->image1DLevelCount(0), 3);

    Math::Vector<1, Int> mipSize{7};
    for(UnsignedInt i = 0; i != importer->image1DLevelCount(0); ++i) {
        CORRADE_ITERATION(i);

        Containers::Optional<Trade::ImageData1D> image = importer->image1D(0, i);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlags1D{});
        CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGB8Srgb);
        CORRADE_COMPARE(image->size(), mipSize);

        /** @todo remove this once CompressedImage etc. tests for data size on
            its own */
        const Vector3i blockSize = compressedPixelFormatBlockSize(image->compressedFormat());
        const Vector3i blockCount = (Vector3i::pad(mipSize, 1) + (blockSize - Vector3i{1}))/blockSize;
        CORRADE_COMPARE(image->data().size(), blockCount.product()*compressedPixelFormatBlockDataSize(image->compressedFormat()));
        /* This is suboptimal because when generating ground-truth data with
           --save-diagnostic the test needs to be run 4 times to save all mips.
           But hopefully this won't really be necessary. */
        CORRADE_COMPARE_AS(Containers::StringView{image->data()},
            Utility::Path::join(KTXIMPORTER_TEST_DIR, Utility::format("1d-compressed-mipmaps-mip{}.bin", i)),
            TestSuite::Compare::StringToFile);

        mipSize = Math::max(mipSize >> 1, 1);
    }
}

void KtxImporterTest::image2D() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgb.ktx2")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 4);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(PatternRgbData[0]), TestSuite::Compare::Container);
}

void KtxImporterTest::image2DMipmaps() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-mipmaps.ktx2")));

    /* Is there a nicer way to get a flat view for a multi-dimensional array? */
    const auto mip0 = Containers::arrayCast<const Color3ub>(PatternRgbData[0]);
    const Color3ub mip1[2]{0xffffff_rgb, 0x007f7f_rgb};
    const Color3ub mip2[1]{0x000000_rgb};
    const Containers::ArrayView<const Color3ub> mipViews[3]{mip0, mip1, mip2};

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), Containers::arraySize(mipViews));

    Vector2i mipSize{4, 3};
    for(UnsignedInt i = 0; i != importer->image2DLevelCount(0); ++i) {
        CORRADE_ITERATION(i);

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, i);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlags2D{});
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), mipSize);

        const PixelStorage storage = image->storage();
        /* Alignment is 4 when row length is a multiple of 4 */
        const Int alignment = ((mipSize.x()*image->pixelSize())%4 == 0) ? 4 : 1;
        CORRADE_COMPARE(storage.alignment(), alignment);
        CORRADE_COMPARE(storage.rowLength(), 0);
        CORRADE_COMPARE(storage.imageHeight(), 0);
        CORRADE_COMPARE(storage.skip(), Vector3i{});

        CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(mipViews[i]), TestSuite::Compare::Container);

        mipSize = Math::max(mipSize >> 1, 1);
    }
}

void KtxImporterTest::image2DMipmapsIncomplete() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-mipmaps-incomplete.ktx2")));

    const auto mip0 = Containers::arrayCast<const Color3ub>(PatternRgbData[0]);
    const Color3ub mip1[2]{0xffffff_rgb, 0x007f7f_rgb};
    const Containers::ArrayView<const Color3ub> mipViews[2]{mip0, mip1};

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), Containers::arraySize(mipViews));

    Vector2i mipSize{4, 3};
    for(UnsignedInt i = 0; i != importer->image2DLevelCount(0); ++i) {
        CORRADE_ITERATION(i);

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, i);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlags2D{});
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), mipSize);
        CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(mipViews[i]), TestSuite::Compare::Container);

        mipSize = Math::max(mipSize >> 1, 1);
    }
}

void KtxImporterTest::image2DLayers() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-layers.ktx2")));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlag3D::Array);
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
    CORRADE_COMPARE(image->size(), (Vector3i{4, 3, 3}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 4);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(PatternRgbData), TestSuite::Compare::Container);
}

void KtxImporterTest::image2DMipmapsAndLayers() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-mipmaps-and-layers.ktx2")));

    const auto mip0 = Containers::arrayCast<const Color3ub>(PatternRgbData);
    /* Mip data generated by PVRTexTool since it doesn't allow specifying our
       own mip data. toktx doesn't seem to support array textures at all, so
       this is our best option. Colors were extracted with an external viewer. */
    const Color3ub mip1[2*1*3]{
        0x0000ff_rgb, 0x7f007f_rgb,
        0x0000ff_rgb, 0x7f007f_rgb,
        0x000000_rgb, 0x000000_rgb
    };
    const Color3ub mip2[1*1*3]{
        0x0000ff_rgb,
        0x0000ff_rgb,
        0x000000_rgb
    };
    const Containers::ArrayView<const Color3ub> mipViews[3]{mip0, mip1, mip2};

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), Containers::arraySize(mipViews));

    Vector2i mipSize{4, 3};
    for(UnsignedInt i = 0; i != importer->image3DLevelCount(0); ++i) {
        CORRADE_ITERATION(i);

        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, i);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlag3D::Array);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), (Vector3i{mipSize, 3}));

        const PixelStorage storage = image->storage();
        /* Alignment is 4 when row length is a multiple of 4 */
        const Int alignment = ((mipSize.x()*image->pixelSize())%4 == 0) ? 4 : 1;
        CORRADE_COMPARE(storage.alignment(), alignment);
        CORRADE_COMPARE(storage.rowLength(), 0);
        CORRADE_COMPARE(storage.imageHeight(), 0);
        CORRADE_COMPARE(storage.skip(), Vector3i{});

        CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(mipViews[i]), TestSuite::Compare::Container);

        mipSize = Math::max(mipSize >> 1, 1);
    }
}

void KtxImporterTest::image2DCompressed() {
    auto&& data = CompressedImage2DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    /* Assume Y up orientation to get the data exactly as in the file without
       any warnings */
    importer->configuration().setValue("assumeOrientation", "ruo");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file)));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlag2D{});
    CORRADE_COMPARE(image->compressedFormat(), data.format);
    CORRADE_COMPARE(image->size(), data.size);

    const CompressedPixelStorage storage = image->compressedStorage();
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    /** @todo remove this once CompressedImage etc. tests for data size on its
        own */
    const Vector3i blockSize = compressedPixelFormatBlockSize(data.format);
    const Vector3i blockCount = (Vector3i::pad(data.size, 1) + (blockSize - Vector3i{1}))/blockSize;
    CORRADE_COMPARE(image->data().size(), blockCount.product()*compressedPixelFormatBlockDataSize(data.format));
    CORRADE_COMPARE_AS(Containers::StringView{image->data()},
        Utility::Path::join(KTXIMPORTER_TEST_DIR, Utility::Path::splitExtension(data.file).first() + ".bin"),
        TestSuite::Compare::StringToFile);
}

void KtxImporterTest::image2DCompressedMipmaps() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    /* Assume Y up orientation to get the data exactly as in the file without
       any warnings */
    importer->configuration().setValue("assumeOrientation", "ruo");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-compressed-mipmaps.ktx2")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 4);

    Vector2i mipSize{9, 10};
    for(UnsignedInt i = 0; i != importer->image2DLevelCount(0); ++i) {
        CORRADE_ITERATION(i);

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0, i);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlag2D{});
        CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGB8Srgb);
        CORRADE_COMPARE(image->size(), mipSize);

        /** @todo remove this once CompressedImage etc. tests for data size on
            its own */
        const Vector3i blockSize = compressedPixelFormatBlockSize(image->compressedFormat());
        const Vector3i blockCount = (Vector3i::pad(mipSize, 1) + (blockSize - Vector3i{1}))/blockSize;
        CORRADE_COMPARE(image->data().size(), blockCount.product()*compressedPixelFormatBlockDataSize(image->compressedFormat()));
        CORRADE_COMPARE_AS(Containers::StringView{image->data()},
            Utility::Path::join(KTXIMPORTER_TEST_DIR, Utility::format("2d-compressed-mipmaps-mip{}.bin", i)),
            TestSuite::Compare::StringToFile);

        mipSize = Math::max(mipSize >> 1, 1);
    }
}

void KtxImporterTest::image2DCompressedLayers() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    /* Assume Y up orientation to get the data exactly as in the file without
       any warnings */
    importer->configuration().setValue("assumeOrientation", "ruo");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-compressed-layers.ktx2")));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlag3D::Array);
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGB8Srgb);
    CORRADE_COMPARE(image->size(), (Vector3i{9, 10, 2}));

    /** @todo remove this once CompressedImage etc. tests for data size on its
        own */
    const Vector3i blockSize = compressedPixelFormatBlockSize(image->compressedFormat());
    const Vector3i blockCount = (Vector3i::pad(image->size(), 1) + (blockSize - Vector3i{1}))/blockSize;
    CORRADE_COMPARE(image->data().size(), blockCount.product()*compressedPixelFormatBlockDataSize(image->compressedFormat()));
    CORRADE_COMPARE_AS(Containers::StringView{image->data()},
        Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-compressed-layers.bin"),
        TestSuite::Compare::StringToFile);
}

/* Origin bottom-left. There's some weird color shift happening in the test
   files, probably the sampling in PVRTexTool. Non-white pixels in the original
   files are multiples of 0x101010. */
const Color3ub FacesRgbData[2][6][2][2]{
    /* cube+x.png */
    {{{0xffffff_rgb, 0x0d0d0d_rgb},
      {0x0d0d0d_rgb, 0x0d0d0d_rgb}},
    /* cube-x.png */
     {{0xffffff_rgb, 0x222222_rgb},
      {0x222222_rgb, 0x222222_rgb}},
    /* cube+y.png */
     {{0xffffff_rgb, 0x323232_rgb},
      {0x323232_rgb, 0x323232_rgb}},
    /* cube-y.png */
     {{0xffffff_rgb, 0x404040_rgb},
      {0x404040_rgb, 0x404040_rgb}},
    /* cube+z.png */
     {{0xffffff_rgb, 0x4f4f4f_rgb},
      {0x4f4f4f_rgb, 0x4f4f4f_rgb}},
    /* cube-z.png */
     {{0xffffff_rgb, 0x606060_rgb},
      {0x606060_rgb, 0x606060_rgb}}},

    /* cube+z.png */
    {{{0xffffff_rgb, 0x4f4f4f_rgb},
      {0x4f4f4f_rgb, 0x4f4f4f_rgb}},
    /* cube-z.png */
     {{0xffffff_rgb, 0x606060_rgb},
      {0x606060_rgb, 0x606060_rgb}},
    /* cube+x.png */
     {{0xffffff_rgb, 0x0d0d0d_rgb},
      {0x0d0d0d_rgb, 0x0d0d0d_rgb}},
    /* cube-x.png */
     {{0xffffff_rgb, 0x222222_rgb},
      {0x222222_rgb, 0x222222_rgb}},
    /* cube+y.png */
     {{0xffffff_rgb, 0x323232_rgb},
      {0x323232_rgb, 0x323232_rgb}},
    /* cube-y.png */
     {{0xffffff_rgb, 0x404040_rgb},
      {0x404040_rgb, 0x404040_rgb}}}
};

void KtxImporterTest::imageCubeMapIncomplete() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->setFlags(data.flags);

    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "cubemap.ktx2"));
    CORRADE_VERIFY(fileData);
    CORRADE_COMPARE_AS(fileData->size(),
        sizeof(Implementation::KtxHeader),
        TestSuite::Compare::GreaterOrEqual);

    /* All 6 bits set, should still emit a warning because the check only happens
       when face count is not 6 */
    const char cubemapData[1]{0x3f};
    /* Not a string, so no terminating 0 */
    Containers::Array<char> keyValueData = createKeyValueData("KTXcubemapIncomplete"_s, Containers::arrayView(cubemapData));
    patchKeyValueData(keyValueData, *fileData);

    Implementation::KtxHeader& header = *reinterpret_cast<Implementation::KtxHeader*>(fileData->data());
    header.layerCount = Utility::Endianness::littleEndian(6u);
    header.faceCount = Utility::Endianness::littleEndian(1u);

    Containers::String out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(importer->openData(*fileData));
    if(data.quiet)
        CORRADE_COMPARE(out, "");
    else
        CORRADE_COMPARE(out,
            "Trade::KtxImporter::openData(): missing or invalid orientation, assuming right, down. Set the assumeOrientation option to suppress this warning.\n"
            "Trade::KtxImporter::openData(): image contains incomplete cube map faces, importing faces as array layers\n");

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), 1);

    #ifdef MAGNUM_BUILD_DEPRECATED
    Containers::Optional<Trade::TextureData> texture = importer->texture(0);
    CORRADE_VERIFY(texture);
    CORRADE_COMPARE(texture->type(), TextureType::Texture2DArray);
    #endif

    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    /* Not CubeMap, because the file is marked as an incomplete cube map */
    CORRADE_COMPARE(image->flags(), ImageFlag3D::Array);
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
    CORRADE_COMPARE(image->size(), (Vector3i{2, 2, 6}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 1);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(FacesRgbData[0]), TestSuite::Compare::Container);
}

void KtxImporterTest::imageCubeMap() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "cubemap.ktx2")));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlag3D::CubeMap);
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
    CORRADE_COMPARE(image->size(), (Vector3i{2, 2, 6}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 1);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(FacesRgbData[0]), TestSuite::Compare::Container);
}

void KtxImporterTest::imageCubeMapMipmaps() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "cubemap-mipmaps.ktx2")));

    const auto mip0 = Containers::arrayCast<const Color3ub>(FacesRgbData[0]);
    const Color3ub mip1[1*1*6]{
        FacesRgbData[0][0][1][0],
        FacesRgbData[0][1][1][0],
        FacesRgbData[0][2][1][0],
        FacesRgbData[0][3][1][0],
        FacesRgbData[0][4][1][0],
        FacesRgbData[0][5][1][0]
    };
    const Containers::ArrayView<const Color3ub> mipViews[2]{mip0, mip1};

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), Containers::arraySize(mipViews));

    Vector2i mipSize{2, 2};
    for(UnsignedInt i = 0; i != importer->image3DLevelCount(0); ++i) {
        CORRADE_ITERATION(i);

        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, i);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlag3D::CubeMap);
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), (Vector3i{mipSize, 6}));
        CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(mipViews[i]), TestSuite::Compare::Container);

        mipSize = Math::max(mipSize >> 1, 1);
    }
}

void KtxImporterTest::imageCubeMapLayers() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "cubemap-layers.ktx2")));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), 1);

    constexpr UnsignedInt NumLayers = 2;

    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlag3D::CubeMap|ImageFlag3D::Array);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
    CORRADE_COMPARE(image->size(), (Vector3i{2, 2, NumLayers*6}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 1);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    const UnsignedInt faceSize = image->data().size()/NumLayers;

    for(UnsignedInt i = 0; i != NumLayers; ++i) {
        CORRADE_ITERATION(i);
        CORRADE_COMPARE_AS(image->data().sliceSize(i*faceSize, faceSize), Containers::arrayCast<const char>(FacesRgbData[i]), TestSuite::Compare::Container);
    }
}

void KtxImporterTest::image3D() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d.ktx2")));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags3D{});
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
    CORRADE_COMPARE(image->size(), (Vector3i{4, 3, 3}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 4);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    /* Same expected data as image2DLayers but the input images were created
       with reversed slice order to account for the z-flip on import from rdi
       to ruo */
    CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(PatternRgbData), TestSuite::Compare::Container);
}

void KtxImporterTest::image3DMipmaps() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d-mipmaps.ktx2")));

    const auto mip0 = Containers::arrayCast<const Color3ub>(PatternRgbData);
    const Color3ub mip1[2]{0xffffff_rgb, 0x007f7f_rgb};
    const Color3ub mip2[1]{0x000000_rgb};
    const Containers::ArrayView<const Color3ub> mipViews[3]{mip0, mip1, mip2};

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), Containers::arraySize(mipViews));

    Vector3i mipSize{4, 3, 3};
    for(UnsignedInt i = 0; i != importer->image3DLevelCount(0); ++i) {
        CORRADE_ITERATION(i);

        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, i);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->flags(), ImageFlags3D{});
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

        CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(mipViews[i]), TestSuite::Compare::Container);

        mipSize = Math::max(mipSize >> 1, 1);
    }
}

void KtxImporterTest::image3DLayers() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d-layers.ktx2")));

    const auto layer0 = Containers::arrayCast<const Color3ub>(PatternRgbData);
    /* Pattern, black, black */
    Color3ub layer1Data[3][3][4]{};
    Utility::copy(Containers::arrayView(PatternRgbData[0]), layer1Data[0]);
    const auto layer1 = Containers::arrayCast<const Color3ub>(layer1Data);

    const Containers::ArrayView<const Color3ub> imageViews[2]{layer0, layer1};

    CORRADE_COMPARE(importer->image3DCount(), Containers::arraySize(imageViews));

    for(UnsignedInt i = 0; i != importer->image3DCount(); ++i) {
        CORRADE_ITERATION(i);

        CORRADE_COMPARE(importer->image3DLevelCount(i), 1);
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(i);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(!image->isCompressed());
        /* Not an Array -- it'd have to be a 4D image. Instead it's multiple
           images */
        CORRADE_COMPARE(image->flags(), ImageFlags3D{});
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), (Vector3i{4, 3, 3}));

        const PixelStorage storage = image->storage();
        CORRADE_COMPARE(storage.alignment(), 4);
        CORRADE_COMPARE(storage.rowLength(), 0);
        CORRADE_COMPARE(storage.imageHeight(), 0);
        CORRADE_COMPARE(storage.skip(), Vector3i{});

        CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(imageViews[i]), TestSuite::Compare::Container);
    }
}

void KtxImporterTest::image3DCompressed() {
    auto&& data = CompressedImage3DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    /* Assume Y up orientation to get the data exactly as in the file without
       any warnings */
    importer->configuration().setValue("assumeOrientation", "ruo");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file)));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags3D{});
    CORRADE_COMPARE(image->compressedFormat(), data.format);
    CORRADE_COMPARE(image->size(), data.size);

    const CompressedPixelStorage storage = image->compressedStorage();
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    /** @todo remove this once CompressedImage etc. tests for data size on its
        own */
    const Vector3i blockSize = compressedPixelFormatBlockSize(data.format);
    const Vector3i blockCount = (data.size + (blockSize - Vector3i{1}))/blockSize;
    CORRADE_COMPARE(image->data().size(), blockCount.product()*compressedPixelFormatBlockDataSize(data.format));
    CORRADE_COMPARE_AS(Containers::StringView{image->data()},
        Utility::Path::join(KTXIMPORTER_TEST_DIR, Utility::Path::splitExtension(data.file).first() + ".bin"),
        TestSuite::Compare::StringToFile);
}

void KtxImporterTest::image3DCompressedMipmaps() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    /* Assume Y up orientation to get the data exactly as in the file without
       any warnings */
    importer->configuration().setValue("assumeOrientation", "ruo");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d-compressed-mipmaps.ktx2")));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), 4);

    Vector3i mipSize{9, 10, 5};
    for(UnsignedInt i = 0; i != importer->image3DLevelCount(0); ++i) {
        CORRADE_ITERATION(i);

        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0, i);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlags3D{});
        CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGB8Srgb);
        CORRADE_COMPARE(image->size(), mipSize);

        /** @todo remove this once CompressedImage etc. tests for data size on
            its own */
        const Vector3i blockSize = compressedPixelFormatBlockSize(image->compressedFormat());
        const Vector3i blockCount = (mipSize + (blockSize - Vector3i{1}))/blockSize;
        CORRADE_COMPARE(image->data().size(), blockCount.product()*compressedPixelFormatBlockDataSize(image->compressedFormat()));
        /* Compressed .bin data is manually generated in generate.sh, don't
           need to save it like the 1D/2D files */
        /** @todo Compare::DataToFile */
        Containers::Optional<Containers::Array<char>> data = Utility::Path::read(
            Utility::Path::join(KTXIMPORTER_TEST_DIR, Utility::format("3d-compressed-mipmaps-mip{}.bin", i)));
        CORRADE_VERIFY(data);
        CORRADE_COMPARE_AS(image->data(), *data, TestSuite::Compare::Container);

        mipSize = Math::max(mipSize >> 1, 1);
    }
}

void KtxImporterTest::forwardBasis() {
    auto&& data = ForwardBasisData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_managerWithBasisImporter.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _managerWithBasisImporter.instantiate("KtxImporter");

    /* Making sure that importer flags and config options are propagated */
    importer->setFlags(data.flags);
    importer->configuration().group("basis")->setValue("format", "Etc2RGBA");

    Containers::String out;
    {
        Debug redirectDebug{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, data.file)));
    }
    CORRADE_COMPARE(out, data.verboseMessage);

    /* Basis has no 1D image support, only 2D or 2D array */
    const UnsignedInt dimensions = data.size[2] == 0 ? 2 : 3;
    CORRADE_VERIFY(data.size[1]);

    /* All of these should forward to the correct instance and not crash */
    CORRADE_COMPARE(importer->image1DCount(), 0);
    CORRADE_COMPARE(importer->image2DCount(), dimensions == 2 ? data.images : 0);
    CORRADE_COMPARE(importer->image3DCount(), dimensions == 3 ? data.images : 0);
    #ifdef MAGNUM_BUILD_DEPRECATED
    CORRADE_COMPARE(importer->textureCount(), data.images);
    #endif

    for(UnsignedInt i = 0; i != data.images; ++i) {
        #ifdef MAGNUM_BUILD_DEPRECATED
        Containers::Optional<Trade::TextureData> texture = importer->texture(i);
        CORRADE_VERIFY(texture);
        CORRADE_COMPARE(texture->type(), data.type);
        #endif

        if(dimensions == 2) {
            CORRADE_COMPARE(importer->image2DLevelCount(i), 1);
            Containers::Optional<Trade::ImageData2D> image = importer->image2D(i);
            CORRADE_VERIFY(image);
            CORRADE_VERIFY(image->isCompressed());
            CORRADE_COMPARE(image->flags(), ImageFlag2D(UnsignedShort(data.imageFlags)));
            CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGBA8Srgb);
            CORRADE_COMPARE(Vector3i::pad(image->size()), data.size);
        } else if(dimensions == 3) {
            CORRADE_COMPARE(importer->image3DLevelCount(i), 1);
            Containers::Optional<Trade::ImageData3D> image = importer->image3D(i);
            CORRADE_VERIFY(image);
            CORRADE_VERIFY(image->isCompressed());
            CORRADE_COMPARE(image->flags(), data.imageFlags);
            CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGBA8Srgb);
            CORRADE_COMPARE(Vector3i::pad(image->size()), data.size);
        } else
            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
    }

    /* Verify that closing works as intended as well */
    importer->close();
    CORRADE_VERIFY(!importer->isOpened());
}

void KtxImporterTest::forwardBasisFormat() {
    auto&& data = ForwardBasisFormatData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_managerWithBasisImporter.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    /* Make sure to reset the globally set format back to not affect other
       tests. Do it in a scope guard so it's done even on test failure. */
    Containers::ScopeGuard resetGlobalFormat{_managerWithBasisImporter.metadata("BasisImporter"), [](PluginManager::PluginMetadata* metadata) {
        metadata->configuration().setValue("format", "");
    }};

    Containers::Pointer<AbstractImporter> importer = _managerWithBasisImporter.instantiate("KtxImporter");
    importer->addFlags(data.flags);

    if(data.ktxFormat)
        importer->configuration().group("basis")->setValue("format", data.ktxFormat);
    if(data.basisFormat)
        _managerWithBasisImporter.metadata("BasisImporter")->configuration().setValue("format", data.basisFormat);

    Containers::String out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba.ktx2")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(out, data.expectedWarning);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->compressedFormat(), data.expectedFormat);
    CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
}

void KtxImporterTest::forwardBasisInvalid() {
    auto&& data = ForwardBasisInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(data.requiresBasisImporter &&
       _managerWithBasisImporter.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _managerWithBasisImporter.instantiate("KtxImporter");

    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(BASISIMPORTER_TEST_DIR, data.file));
    CORRADE_VERIFY(fileData);
    CORRADE_COMPARE_AS(fileData->size(), data.offset, TestSuite::Compare::Greater);
    (*fileData)[data.offset] = data.value;

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(*fileData));
    CORRADE_COMPARE(out, data.message);
}

void KtxImporterTest::forwardBasisPluginNotFound() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");

    /* Happens on builds with static plugins. Can't really do much there. */
    if(_manager.loadState("BasisImporter") != PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin loaded, cannot test");

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba.ktx2")));
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    CORRADE_COMPARE(out,
        "PluginManager::Manager::load(): plugin BasisImporter is not static and was not found in nonexistent\n"
        "Trade::KtxImporter::openData(): can't forward a Basis Universal image to BasisImporter\n");
    #else
    CORRADE_COMPARE(out,
        "PluginManager::Manager::load(): plugin BasisImporter was not found\n"
        "Trade::KtxImporter::openData(): can't forward a Basis Universal image to BasisImporter\n");
    #endif
}

void KtxImporterTest::keyValueDataEmpty() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->addFlags(data.flags);

    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgb.ktx2"));
    CORRADE_VERIFY(fileData);
    CORRADE_COMPARE_AS(fileData->size(),
        sizeof(Implementation::KtxHeader),
        TestSuite::Compare::GreaterOrEqual);

    Implementation::KtxHeader& header = *reinterpret_cast<Implementation::KtxHeader*>(fileData->data());
    header.kvdByteLength = Utility::Endianness::littleEndian(0u);

    Containers::String out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openData(*fileData));
    }
    /* No warning besides missing orientation */
    if(data.quiet)
        CORRADE_COMPARE(out, "");
    else
        CORRADE_COMPARE(out, "Trade::KtxImporter::openData(): missing or invalid orientation, assuming right, down. Set the assumeOrientation option to suppress this warning.\n");
}

template<ImporterFlag flag> void KtxImporterTest::keyValueDataInvalid() {
    auto&& data = InvalidKeyValueData[testCaseInstanceId()];
    setTestCaseDescription(data.name);
    if(flag == ImporterFlag::Quiet)
        setTestCaseTemplateName("ImporterFlag::Quiet");

    /* Invalid key/value data that might hint at a broken file so the importer
       should warn and try to continue the import */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->setFlags(flag);

    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgb.ktx2"));
    CORRADE_VERIFY(fileData);

    patchKeyValueData(data.data, *fileData);

    Containers::String out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openData(*fileData));
    }
    /* Import succeeds with a warning */
    if(flag == ImporterFlag::Quiet)
        CORRADE_COMPARE(out, "");
    else
        CORRADE_COMPARE(out, Utility::format(
            "Trade::KtxImporter::openData(): {}\n"
            "Trade::KtxImporter::openData(): missing or invalid orientation, assuming right, down. Set the assumeOrientation option to suppress this warning.\n",
        data.message));
}

template<ImporterFlag flag> void KtxImporterTest::keyValueDataInvalidIgnored() {
    auto&& data = IgnoredInvalidKeyValueData[testCaseInstanceId()];
    setTestCaseDescription(data.name);
    if(flag == ImporterFlag::Quiet)
        setTestCaseTemplateName("ImporterFlag::Quiet");

    /* Invalid (according to the spec) key/value data that can just be
       ignored without warning because it doesn't affect the import */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->setFlags(flag);

    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgb.ktx2"));
    CORRADE_VERIFY(fileData);

    patchKeyValueData(data.data, *fileData);

    Containers::String out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openData(*fileData));
    }
    /* No warning besides missing orientation */
    if(flag == ImporterFlag::Quiet)
        CORRADE_COMPARE(out, "");
    else
        CORRADE_COMPARE(out, "Trade::KtxImporter::openData(): missing or invalid orientation, assuming right, down. Set the assumeOrientation option to suppress this warning.\n");
}

template<ImporterFlag flag> void KtxImporterTest::orientationInvalid() {
    auto&& data = InvalidOrientationData[testCaseInstanceId()];
    setTestCaseDescription(data.name);
    if(flag == ImporterFlag::Quiet)
        setTestCaseTemplateName("ImporterFlag::Quiet");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->setFlags(flag);

    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file));
    CORRADE_VERIFY(fileData);

    /* These two should have equivalent result except for the warning */
    if(data.assume)
        importer->configuration().setValue("assumeOrientation", data.orientation);
    else
        patchKeyValueData(createKeyValueData("KTXorientation"_s, data.orientation), *fileData);

    Containers::String out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openData(*fileData));
    }
    /* GCC 4.8 "cannot initialize" if this is constexpr due to the function
       being a template */
    Containers::StringView orientations[]{"right"_s, "down"_s, "forward"_s};
    const Containers::String orientationString = ", "_s.join(Containers::arrayView(orientations).prefix(data.dimensions));
    if(flag == ImporterFlag::Quiet)
        CORRADE_COMPARE(out, "");
    else if(data.assume)
        CORRADE_COMPARE(out, Utility::format("Trade::KtxImporter::openData(): invalid assumeOrientation option, falling back to {}\n", orientationString));
    else
        CORRADE_COMPARE(out, Utility::format("Trade::KtxImporter::openData(): missing or invalid orientation, assuming {}. Set the assumeOrientation option to suppress this warning.\n", orientationString));
}

void KtxImporterTest::orientationFlip() {
    auto&& data = FlipData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->addFlags(data.flags);
    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file));
    CORRADE_VERIFY(fileData);

    /* These two should achieve equivalent result */
    if(data.assume)
        importer->configuration().setValue("assumeOrientation", data.orientation);
    else
        patchKeyValueData(createKeyValueData("KTXorientation"_s, data.orientation), *fileData);

    Containers::String out;
    {
        Debug redirectOutput{&out};
        CORRADE_VERIFY(importer->openData(*fileData));
    }

    const Vector3i size = Math::max(data.size, 1);
    const Int dimensions = Math::min(data.size, 1).sum();
    Containers::Array<char> imageData;
    switch(dimensions) {
        case 1: {
            Containers::Optional<Trade::ImageData1D> image = importer->image1D(0);
            CORRADE_VERIFY(image);
            imageData = Containers::Array<char>{InPlaceInit, image->data()};
            break;
        }
        case 2: {
            Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
            CORRADE_VERIFY(image);
            imageData = Containers::Array<char>{InPlaceInit, image->data()};
            break;
        }
        case 3: {
            Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
            CORRADE_VERIFY(image);
            imageData = Containers::Array<char>{InPlaceInit, image->data()};
            break;
        }
        default: CORRADE_INTERNAL_ASSERT_UNREACHABLE();
    }

    Containers::StridedArrayView4D<const char> src{imageData, {
        std::size_t(size.z()),
        std::size_t(size.y()),
        std::size_t(size.x()),
        pixelFormatSize(data.format)
    }};

    Containers::Array<char> flippedData{imageData.size()};
    Containers::StridedArrayView4D<char> dst{flippedData, src.size()};

    if(data.flipped[2]) src = src.flipped<0>();
    if(data.flipped[1]) src = src.flipped<1>();
    if(data.flipped[0]) src = src.flipped<2>();

    Utility::copy(src, dst);

    CORRADE_COMPARE_AS(data.data, flippedData, TestSuite::Compare::Container);
    if(!data.message)
        CORRADE_COMPARE(out, "");
    else
        CORRADE_COMPARE(out, data.message);
}

void KtxImporterTest::orientationFlipCompressed() {
    auto&& data = FlipCompressedData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* This won't flip the image and won't produce any warning either */
    Containers::Pointer<AbstractImporter> importerNotFlipped = _manager.instantiate("KtxImporter");
    importerNotFlipped->configuration().setValue("assumeOrientation", "ruo");
    CORRADE_VERIFY(importerNotFlipped->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.filename)));
    Containers::Optional<ImageData2D> imageNotFlipped = importerNotFlipped->image2D(0);
    CORRADE_VERIFY(imageNotFlipped);

    /* This will either flip the image (with a verbose message) or print a
       warning */
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->addFlags(data.flags);
    if(data.assumeOrientation)
        importer->configuration().setValue("assumeOrientation", data.assumeOrientation);
    Containers::Optional<ImageData2D> image;
    Containers::String out;
    {
        Debug redirectOutput{&out};
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.filename)));
        image = importer->image2D(0);
        CORRADE_VERIFY(image);
    }
    if(data.message)
        CORRADE_COMPARE(out, data.message);
    else
        CORRADE_COMPARE(out, "");

    /* The images, once decoded, should then be flipped compared to each other,
       or if flip was not made or not possible, identical */

    Containers::StringView decoderName;
    if(Containers::StringView{data.name}.hasPrefix("BC"))
        decoderName = "BcDecImageConverter";
    else if(Containers::StringView{data.name}.hasPrefix("ETC") ||
            Containers::StringView{data.name}.hasPrefix("EAC"))
        decoderName = "EtcDecImageConverter";
    else CORRADE_SKIP("No decoder for" << image->compressedFormat() << Debug::nospace << ", cannot test decoded image equality.");

    /* Catch also ABI and interface mismatch errors */
    if(!(_converterManager.load(decoderName) & PluginManager::LoadState::Loaded))
        CORRADE_SKIP(decoderName << "plugin can't be loaded, cannot test decoded image equality.");

    Containers::Pointer<Trade::AbstractImageConverter> decoder = _converterManager.loadAndInstantiate(decoderName);
    /** @todo here it might start failing for incomplete blocks at some point
        due to the pixels being shifted, fix that by expanding the image size
        to full blocks before decoding like in the 3D case */
    Containers::Optional<Trade::ImageData2D> decodedNotFlipped = decoder->convert(*imageNotFlipped);
    Containers::Optional<Trade::ImageData2D> decoded = decoder->convert(*image);
    CORRADE_VERIFY(decodedNotFlipped);
    CORRADE_VERIFY(decoded);

    if(!data.flippedY) CORRADE_COMPARE_AS(*decoded,
        *decodedNotFlipped,
        DebugTools::CompareImage);
    else if(decoded->format() == PixelFormat::RGBA8Unorm ||
            decoded->format() == PixelFormat::RGBA8Srgb)
        CORRADE_COMPARE_AS(decoded->pixels<Vector4ub>().flipped<0>(),
            *decodedNotFlipped,
            DebugTools::CompareImage);
    else if(decoded->format() == PixelFormat::RG8Unorm)
        CORRADE_COMPARE_AS(decoded->pixels<Vector2ub>().flipped<0>(),
            *decodedNotFlipped,
            DebugTools::CompareImage);
    else if(decoded->format() == PixelFormat::RG8Snorm)
        CORRADE_COMPARE_AS(decoded->pixels<Vector2b>().flipped<0>(),
            *decodedNotFlipped,
            DebugTools::CompareImage);
    else if(decoded->format() == PixelFormat::R8Unorm)
        CORRADE_COMPARE_AS(decoded->pixels<UnsignedByte>().flipped<0>(),
            *decodedNotFlipped,
            DebugTools::CompareImage);
    else CORRADE_FAIL("Unexpected format" << decoded->format());
}

void KtxImporterTest::orientationFlipCompressed3D() {
    auto&& data = FlipCompressed3DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* This won't flip the image and won't produce any warning either */
    Containers::Pointer<AbstractImporter> importerNotFlipped = _manager.instantiate("KtxImporter");
    importerNotFlipped->configuration().setValue("assumeOrientation", "ruo");
    CORRADE_VERIFY(importerNotFlipped->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.filename)));
    Containers::Optional<ImageData3D> imageNotFlipped = importerNotFlipped->image3D(0);
    CORRADE_VERIFY(imageNotFlipped);

    /* This will either flip the image (with a verbose message) or print a
       warning */
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->addFlags(data.flags);
    if(data.assumeOrientation)
        importer->configuration().setValue("assumeOrientation", data.assumeOrientation);
    Containers::Optional<ImageData3D> image;
    Containers::String out;
    {
        Debug redirectOutput{&out};
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.filename)));
        image = importer->image3D(0);
        CORRADE_VERIFY(image);
    }
    if(data.message)
        CORRADE_COMPARE(out, data.message);
    else
        CORRADE_COMPARE(out, "");

    /* The images, once decoded, should then be flipped compared to each other,
       or if flip was not made or not possible, identical */

    Containers::StringView decoderName;
    if(Containers::StringView{data.name}.hasPrefix("BC"))
        decoderName = "BcDecImageConverter";
    else if(Containers::StringView{data.name}.hasPrefix("ETC") ||
            Containers::StringView{data.name}.hasPrefix("EAC"))
        decoderName = "EtcDecImageConverter";
    else CORRADE_SKIP("No decoder for" << image->compressedFormat() << Debug::nospace << ", cannot test decoded image equality.");

    /* Catch also ABI and interface mismatch errors */
    if(!(_converterManager.load(decoderName) & PluginManager::LoadState::Loaded))
        CORRADE_SKIP(decoderName << "plugin can't be loaded, cannot test decoded image equality.");

    /* The converter doesn't support 3D images yet (waiting for CompressedImage
       APIs to get more usable), manually convert each slice */
    Containers::Pointer<Trade::AbstractImageConverter> decoder = _converterManager.loadAndInstantiate(decoderName);
    CORRADE_COMPARE(imageNotFlipped->size(), (Vector3i{5, 5, 3}));
    const UnsignedInt blockDataSize = compressedPixelFormatBlockDataSize(imageNotFlipped->compressedFormat());
    Containers::Optional<Trade::ImageData2D> decodedNotFlipped[]{
        decoder->convert(CompressedImageView2D{
            imageNotFlipped->compressedFormat(), {8, 8},
            imageNotFlipped->data().prefix(2*2*blockDataSize)}),
        decoder->convert(CompressedImageView2D{
            imageNotFlipped->compressedFormat(), {8, 8},
            imageNotFlipped->data().sliceSize(1*2*2*blockDataSize,
                                              2*2*blockDataSize)}),
        decoder->convert(CompressedImageView2D{
            imageNotFlipped->compressedFormat(), {8, 8},
            imageNotFlipped->data().sliceSize(2*2*2*blockDataSize,
                                              2*2*blockDataSize)}),
    };
    Containers::Optional<Trade::ImageData2D> decoded[]{
        decoder->convert(CompressedImageView2D{
            image->compressedFormat(), {8, 8},
            image->data().prefix(2*2*blockDataSize)}),
        decoder->convert(CompressedImageView2D{
            image->compressedFormat(), {8, 8},
            image->data().sliceSize(1*2*2*blockDataSize,
                                    2*2*blockDataSize)}),
        decoder->convert(CompressedImageView2D{
            image->compressedFormat(), {8, 8},
            image->data().sliceSize(2*2*2*blockDataSize,
                                    2*2*blockDataSize)}),
    };

    Vector3i zOrder = data.flippedZ ? Vector3i{2, 1, 0} : Vector3i{0, 1, 2};
    for(Int i: {0, 1, 2}) {
        CORRADE_VERIFY(decodedNotFlipped[i]);
        CORRADE_VERIFY(decoded[zOrder[i]]);

        if(!data.flippedY) CORRADE_COMPARE_AS(*decoded[zOrder[i]],
            *decodedNotFlipped[i],
            DebugTools::CompareImage);
        else if(decoded[zOrder[i]]->format() == PixelFormat::RGBA8Unorm)
            CORRADE_COMPARE_AS(decoded[zOrder[i]]->pixels<Vector4ub>().flipped<0>(),
                *decodedNotFlipped[i],
                DebugTools::CompareImage);
        else CORRADE_FAIL("Unexpected format" << decoded[zOrder[i]]->format());
    }
}

void KtxImporterTest::swizzle() {
    auto&& data = SwizzleData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->addFlags(ImporterFlag::Verbose);

    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file));
    CORRADE_VERIFY(fileData);
    CORRADE_COMPARE_AS(fileData->size(), sizeof(Implementation::KtxHeader),
        TestSuite::Compare::Greater);

    /* toktx lets us swizzle the input data, but doesn't turn the format into
       a swizzled one. Patch the header manually. */
    if(data.vkFormat != Implementation::VK_FORMAT_UNDEFINED) {
        auto& header = *reinterpret_cast<Implementation::KtxHeader*>(fileData->data());
        header.vkFormat = Utility::Endianness::littleEndian(data.vkFormat);
    }

    Containers::String out;
    {
        Debug redirectDebug{&out};
        CORRADE_VERIFY(importer->openData(*fileData));
    }

    Containers::String expectedMessage = "Trade::KtxImporter::openData(): image will be flipped along Y\n";
    if(data.message)
        expectedMessage = expectedMessage + Utility::format("Trade::KtxImporter::openData(): {}\n", data.message);
    CORRADE_COMPARE(out, expectedMessage);

    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->format(), data.format);
    CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));
    CORRADE_COMPARE_AS(image->data(), data.data, TestSuite::Compare::Container);
}

void KtxImporterTest::swizzleMultipleBytes() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->addFlags(ImporterFlag::Verbose);

    Containers::String out;
    {
        Debug redirectDebug{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "bgr-swizzle-bgr-16bit.ktx2")));
    }

    CORRADE_COMPARE(out,
        "Trade::KtxImporter::openData(): image will be flipped along Y\n"
        "Trade::KtxImporter::openData(): format requires conversion from BGR to RGB\n");

    /* For some reason a 16-bit PNG sent through toktx ends up with 8-bit
       channels duplicated to 16 bits instead of being remapped. Not sure if
       this is a bug in GIMP or toktx, although the PNG shows correctly in
       several viewers so probably the latter. PVRTexTool does the same thing,
       see imageRgb32U(). This is PatternRgbData[0], but each byte extended to
       unsigned short by just repeating the byte twice. */
    constexpr UnsignedShort Half = 0x7f7f;
    constexpr Math::Color3<UnsignedShort> content[4*3]{
        {0xffff,      0,      0}, {0xffff, 0xffff, 0xffff}, {   0, 0,    0}, {   0, 0xffff,    0},
        {0xffff, 0xffff, 0xffff}, {0xffff,      0,      0}, {   0, 0,    0}, {   0, 0xffff,    0},
        {     0,      0, 0xffff}, {     0, 0xffff,      0}, {Half, 0, Half}, {Half,      0, Half}
    };

    CORRADE_COMPARE(importer->image2DCount(), 1);
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->format(), PixelFormat::RGB16Unorm);
    CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));
    CORRADE_COMPARE_AS(Containers::arrayCast<const Math::Color3<UnsignedShort>>(image->data()),
        Containers::arrayView(content), TestSuite::Compare::Container);
}

void KtxImporterTest::swizzleIdentity() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    importer->addFlags(ImporterFlag::Verbose);

    Containers::String out;
    Debug redirectError{&out};

    /* RGB1 swizzle. This also checks that the correct prefix based on channel
       count is used, since swizzle is always a constant length 4 in the
       key/value data. */
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "swizzle-identity.ktx2")));
    /* No message about format requiring conversion */
    CORRADE_COMPARE(out, "Trade::KtxImporter::openData(): image will be flipped along Y\n");
}

void KtxImporterTest::swizzleUnsupported() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");

    Containers::String out;
    Error redirectError{&out};

    /* Only identity (RG?B?A?), BGR and BGRA swizzle supported. This is the same
       swizzle string as in swizzle-identity.ktx2, but this file is RGBA instead
       of RGB, so the 1 shouldn't be ignored. */
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "swizzle-unsupported.ktx2")));
    CORRADE_COMPARE(out, "Trade::KtxImporter::openData(): unsupported channel mapping rgb1\n");
}

void KtxImporterTest::swizzleCompressed() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");

    Containers::Optional<Containers::Array<char>> fileData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-compressed-bc1.ktx2"));
    CORRADE_VERIFY(fileData);
    patchKeyValueData(createKeyValueData("KTXswizzle"_s, "bgra"_s), *fileData);

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(*fileData));
    CORRADE_COMPARE(out, "Trade::KtxImporter::openData(): unsupported channel mapping bgra\n");
}

void KtxImporterTest::openMemory() {
    /* Same as imageRgba() except that it uses openData() & openMemory()
       instead of openFile() to test data copying on import */

    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgba.ktx2"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*importer, *memory));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));

    const PixelStorage storage = image->storage();
    CORRADE_COMPARE(storage.alignment(), 4);
    CORRADE_COMPARE(storage.rowLength(), 0);
    CORRADE_COMPARE(storage.imageHeight(), 0);
    CORRADE_COMPARE(storage.skip(), Vector3i{});

    CORRADE_COMPARE_AS(image->data(), Containers::arrayCast<const char>(PatternRgba2DData), TestSuite::Compare::Container);
}

void KtxImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgb.ktx2")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    #ifdef MAGNUM_BUILD_DEPRECATED
    CORRADE_COMPARE(importer->textureCount(), 1);
    #endif

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgb.ktx2")));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    #ifdef MAGNUM_BUILD_DEPRECATED
    CORRADE_COMPARE(importer->textureCount(), 1);
    #endif

    /* Shouldn't crash, leak or anything */
}

void KtxImporterTest::openNormalAfterBasis() {
    if(_managerWithBasisImporter.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _managerWithBasisImporter.instantiate("KtxImporter");

    importer->configuration().group("basis")->setValue("format", "Etc2RGBA");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba.ktx2")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlags2D{});
        CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGBA8Srgb);
        CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
    }

    /* Loading a normal KTX afterwards should work */

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgb.ktx2")));

    CORRADE_COMPARE(importer->image2DCount(), 1);
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlags2D{});
        CORRADE_COMPARE(image->format(), PixelFormat::RGB8Srgb);
        CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));
    }
}

void KtxImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgb.ktx2")));

    /* Verify that everything is working the same way on second use */
    {
        Containers::Optional<ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));
    } {
        Containers::Optional<ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{4, 3}));
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::KtxImporterTest)
