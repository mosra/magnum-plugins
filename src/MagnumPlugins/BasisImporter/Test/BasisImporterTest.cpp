/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019 Jonathan Hale <squareys@googlemail.com>
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
    FROM, OUT OF OR IN CONNETCION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <sstream>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#ifdef MAGNUM_BUILD_DEPRECATED
#include <Magnum/Trade/TextureData.h>
#endif

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct BasisImporterTest: TestSuite::Tester {
    explicit BasisImporterTest();

    void empty();

    void invalidHeader();
    void invalidFile();
    void fileTooShort();

    void unconfigured();
    void invalidConfiguredFormat();
    void unsupportedFormat();
    void transcodingFailure();
    void nonBasisKtx();

    #ifdef MAGNUM_BUILD_DEPRECATED
    void texture();
    #endif

    void rgbUncompressed();
    void rgbUncompressedLinear();
    void rgbaUncompressed();
    void rgbaUncompressedUastc();
    void rgbaUncompressedMultipleImages();

    void rgb();
    void rgba();
    void linear();

    void array2D();
    void array2DMipmaps();
    void video();
    void image3D();
    void image3DMipmaps();
    void cubeMap();
    void cubeMapArray();

    void videoSeeking();
    void videoVerbose();

    void flipUncompressed();
    void flipUncompressed3D();
    void flip();
    void flip3D();

    void openMemory();
    void openSameTwice();
    void openDifferent();
    void importMultipleFormats();

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
    /* For testing Y-flip of block-compressed formats */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};
};

using namespace Containers::Literals;

constexpr struct {
    const char* name;
    const char* extension;
    /* To avoid introducing circular bugs, the ground truth input files are
       produced by `basisu` itself. THE DAMN THING however still doesn't write
       proper KTXorientation, which means it'll print a warning by default:
        https://github.com/BinomialLLC/basis_universal/issues/258
       The data *is* encoded with Y up, so we just set assumeYUp to silence
       the warning in this case. */
    bool hasMissingOrientationMetadata;
} FileTypeData[] {
    {"Basis", ".basis", false},
    {"KTX2", ".ktx2", true}
};

constexpr struct {
    const char* name;
    const Containers::ArrayView<const char> data;
    const char* message;
} InvalidHeaderData[] {
    /* GCC 4.8 needs the explicit cast :( */
    {"Invalid", Containers::arrayView("NotAValidFile"), "invalid basis header"},
    {"Invalid basis header", Containers::arrayView("sB\xff\xff"), "invalid basis header"},
    {"Invalid KTX2 identifier", Containers::arrayView("\xabKTX 30\xbb\r\n\x1a\n"), "invalid basis header"},
    {"Invalid KTX2 header", Containers::arrayView("\xabKTX 20\xbb\r\n\x1a\n\xff\xff\xff\xff"), "invalid KTX2 header, or not Basis compressed; might want to use KtxImporter instead"}
};

constexpr struct {
    const char* name;
    const char* file;
    std::size_t offset;
    const char value;
    const char* message;
} InvalidFileData[] {
    /* Change ktx2_etc1s_global_data_header::m_endpoint_count */
    {"Corrupt KTX2 supercompression data", "rgb.ktx2", 184, 0x00, "bad KTX2 file"},
    /* Changing anything in basis_file_header after m_header_crc16 makes the
       CRC16 check fail. Only the header checksum is currently checked, not the
       data checksum, so patching slice metadata still works. */
    {"Invalid header CRC16", "rgb.basis", 23, 0x7f, "bad basis file"},
    /* Change basis_slice_desc::m_orig_width of slice 0 */
    {"Mismatching array sizes", "rgba-array.basis", 82, 0x7f, "image slices have mismatching size, expected 127 by 27 but got 63 by 27"},
    /* Change basis_slice_desc::m_orig_width of slice 0 */
    {"Mismatching video sizes", "rgba-video.basis", 82, 0x7f, "image slices have mismatching size, expected 127 by 27 but got 63 by 27"},
    /* We can't patch m_tex_type in the header because of the CRC check, so we
       need dedicated files for the cube map checks */
    {"Invalid face count", "invalid-cube-face-count.basis", ~std::size_t(0), 0, "cube map face count must be a multiple of 6 but got 3"},
    {"Face not square", "invalid-cube-face-size.basis", ~std::size_t(0), 0, "cube map must be square but got 15 by 6"}
};

constexpr struct {
    const char* name;
    const char* file;
    const std::size_t size;
    const char* message;
} FileTooShortData[] {
    {"Basis", "rgb.basis", 64, "invalid basis header"},
    {"KTX2", "rgb.ktx2", 64, "invalid KTX2 header, or not Basis compressed; might want to use KtxImporter instead"}
};

const struct {
    const char* name;
    ImporterFlags flags;
    bool quiet;
} QuietData[]{
    {"", {}, false},
    {"quiet", ImporterFlag::Quiet, true}
};

#ifdef MAGNUM_BUILD_DEPRECATED
const struct {
    const char* name;
    const char* fileBase;
    TextureType type;
    Containers::Optional<TextureType> xfailType;
    const char* warning[2]; /* Basis, then KTX */
} TextureData[]{
    {"2D", "rgb", TextureType::Texture2D, {}, {
        "",
        "", /* rgb.ktx2 has the orientation metadata patched in */
    }},
    {"2D array", "rgba-array", TextureType::Texture2DArray, {}, {
        "",
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
    }},
    {"Cube map", "rgba-cubemap", TextureType::CubeMap, {}, {
        "",
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
    }},
    {"Cube map array", "rgba-cubemap-array", TextureType::CubeMapArray, {}, {
        "",
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
    }},
    /* The Basis metadata say it's a 3D texture, but it's actually a 2D array
       because the mip levels don't shrink along Z. The texture type is thus
       patched on import with a warning. With the KTX2 output `-tex_type 3d`
       marks the file as 2D array (and not 3D) so there's nothing to patch or
       warn about. */
    {"3D", "rgba-3d", TextureType::Texture3D, TextureType::Texture2DArray, {
        "Trade::BasisImporter::openData(): importing 3D texture as a 2D array texture\n",
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
    }},
    /* Same as above */
    {"3D mipmaps", "rgba-3d-mips", TextureType::Texture3D, TextureType::Texture2DArray, {
        "Trade::BasisImporter::openData(): importing 3D texture as a 2D array texture\n",
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
    }},
    {"Video", "rgba-video", TextureType::Texture2D, {}, {
        "",
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
    }}
};
#endif

constexpr struct {
    const char* fileBase;
    const char* fileBaseAlpha;
    const char* fileBaseLinear;
    const Vector2i expectedSize;
    const char* suffix;
    const CompressedPixelFormat expectedFormat;
    const CompressedPixelFormat expectedLinearFormat;
} FormatData[] {
    {"rgb", "rgba", "rgb-linear", {63, 27},
        "Etc1RGB",
        CompressedPixelFormat::Etc2RGB8Srgb,
        CompressedPixelFormat::Etc2RGB8Unorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
        "Etc2RGBA",
        CompressedPixelFormat::Etc2RGBA8Srgb,
        CompressedPixelFormat::Etc2RGBA8Unorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
        "Bc1RGB",
        CompressedPixelFormat::Bc1RGBSrgb,
        CompressedPixelFormat::Bc1RGBUnorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
        "Bc3RGBA",
        CompressedPixelFormat::Bc3RGBASrgb,
        CompressedPixelFormat::Bc3RGBAUnorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
        "Bc4R",
        CompressedPixelFormat::Bc4RUnorm,
        CompressedPixelFormat::Bc4RUnorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
        "Bc5RG",
        CompressedPixelFormat::Bc5RGUnorm,
        CompressedPixelFormat::Bc5RGUnorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
        "Bc7RGBA",
        CompressedPixelFormat::Bc7RGBASrgb,
        CompressedPixelFormat::Bc7RGBAUnorm},
    {"rgb-pow2", "rgba-pow2", "rgb-linear-pow2", {64, 32},
        "PvrtcRGB4bpp",
        CompressedPixelFormat::PvrtcRGB4bppSrgb,
        CompressedPixelFormat::PvrtcRGB4bppUnorm},
    {"rgb-pow2", "rgba-pow2", "rgb-linear-pow2", {64, 32},
        "PvrtcRGBA4bpp",
        CompressedPixelFormat::PvrtcRGBA4bppSrgb,
        CompressedPixelFormat::PvrtcRGBA4bppUnorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
        "Astc4x4RGBA",
        CompressedPixelFormat::Astc4x4RGBASrgb,
        CompressedPixelFormat::Astc4x4RGBAUnorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
        "EacR",
        CompressedPixelFormat::EacR11Unorm,
        CompressedPixelFormat::EacR11Unorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
        "EacRG",
        CompressedPixelFormat::EacRG11Unorm,
        CompressedPixelFormat::EacRG11Unorm}
};

const struct {
    const char* name;
    const char* filename;
    ImporterFlags flags;
    Containers::Optional<bool> assumeYUp;
    PixelFormat expectedFormat;
    bool flipped, flippedToPng;
    const char* message;
} FlipUncompressedData[] {
    {"Y down",
        "rgb-noflip.ktx2", {}, {},
        PixelFormat::RGBA8Srgb, true, false,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"},
    {"Y down, verbose",
        "rgb-noflip.ktx2", ImporterFlag::Verbose, {},
        PixelFormat::RGBA8Srgb, true, false,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::openData(): image will be flipped along Y\n"},
    {"Y down, quiet",
        "rgb-noflip.ktx2", ImporterFlag::Quiet, {},
        PixelFormat::RGBA8Srgb, true, false, nullptr},
    {"Y down, assume Y up, verbose",
        "rgb-noflip.ktx2", {}, true,
        PixelFormat::RGBA8Srgb, false, true, nullptr},
    {"Y up, verbose",
        "rgb.basis", ImporterFlag::Verbose, {},
        PixelFormat::RGBA8Srgb, false, false, nullptr},
    {"Y up metadata missing, assume Y up, verbose",
        /* rgb.ktx2 has the KTXorientation metadata patched in */
        "rgb-linear.ktx2", ImporterFlag::Verbose, true,
        PixelFormat::RGBA8Unorm, false, false, nullptr},
};

const struct {
    TestSuite::TestCaseDescriptionSourceLocation name;
    const char* filename;
    ImporterFlags flags;
    Containers::Optional<bool> assumeYUp;
    const char* format;
    CompressedPixelFormat expectedFormat;
    bool flipped;
    const char* message;
} FlipData[] {
    {"ETC1 RGB, flip not implemented",
        "rgb-noflip.ktx2", {}, {},
        "Etc1RGB", CompressedPixelFormat::Etc2RGB8Srgb, false,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::image2D(): Y flip is not yet implemented for CompressedPixelFormat::Etc2RGB8Srgb, imported data will have wrong orientation. Enable assumeYUp to suppress this warning.\n"},
    {"ETC1 RGB, flip not implemented, verbose",
        "rgb-noflip.ktx2", ImporterFlag::Verbose, {},
        "Etc1RGB", CompressedPixelFormat::Etc2RGB8Srgb, false,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        /* The Y flip note is printed even if it's impossible to do after,
           since the target format can be different for each image2D() call */
        "Trade::BasisImporter::openData(): image will be flipped along Y\n"
        "Trade::BasisImporter::image2D(): Y flip is not yet implemented for CompressedPixelFormat::Etc2RGB8Srgb, imported data will have wrong orientation. Enable assumeYUp to suppress this warning.\n"},
    {"ETC1 RGB, flip not implemented, quiet",
        "rgb-noflip.ktx2", ImporterFlag::Quiet, {},
        "Etc1RGB", CompressedPixelFormat::Etc2RGB8Srgb, false,
        nullptr},
    {"ETC1 RGB, Y up, verbose",
        "rgb-linear.basis", ImporterFlag::Verbose, {},
        "Etc1RGB", CompressedPixelFormat::Etc2RGB8Unorm, false,
        nullptr},
    {"ETC1 RGB, assume Y up, verbose",
        "rgb-noflip.ktx2", ImporterFlag::Verbose, true,
        "Etc1RGB", CompressedPixelFormat::Etc2RGB8Srgb, false,
        nullptr},
    {"ETC2 RGBA, flip not implemented",
        "rgb-noflip.ktx2", {}, {},
        "Etc2RGBA", CompressedPixelFormat::Etc2RGBA8Srgb, false,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::image2D(): Y flip is not yet implemented for CompressedPixelFormat::Etc2RGBA8Srgb, imported data will have wrong orientation. Enable assumeYUp to suppress this warning.\n"},
    {"EAC R, flip not implemented",
        "rgb-noflip.ktx2", {}, {},
        "EacR", CompressedPixelFormat::EacR11Unorm, false,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::image2D(): Y flip is not yet implemented for CompressedPixelFormat::EacR11Unorm, imported data will have wrong orientation. Enable assumeYUp to suppress this warning.\n"},
    {"EAC RG, flip not implemented",
        "rgb-noflip.ktx2", {}, {},
        "EacRG", CompressedPixelFormat::EacRG11Unorm, false,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::image2D(): Y flip is not yet implemented for CompressedPixelFormat::EacRG11Unorm, imported data will have wrong orientation. Enable assumeYUp to suppress this warning.\n"},
    {"BC1",
        "rgb-noflip-pow2.ktx2", {}, {},
        "Bc1RGB", CompressedPixelFormat::Bc1RGBSrgb, true,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"},
    {"BC1, verbose",
        "rgb-noflip-pow2.ktx2", ImporterFlag::Verbose, {},
        "Bc1RGB", CompressedPixelFormat::Bc1RGBSrgb, true,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::openData(): image will be flipped along Y\n"},
    {"BC1, incomplete blocks",
        "rgb-noflip.ktx2", {}, {},
        "Bc1RGB", CompressedPixelFormat::Bc1RGBSrgb, true,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::image2D(): Y-flipping a compressed image that's not whole blocks, the result will be shifted by 1 pixels\n"},
    {"BC1, incomplete blocks, verbose",
        "rgb-noflip.ktx2", ImporterFlag::Verbose, {},
        "Bc1RGB", CompressedPixelFormat::Bc1RGBSrgb, true,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::openData(): image will be flipped along Y\n"
        "Trade::BasisImporter::image2D(): Y-flipping a compressed image that's not whole blocks, the result will be shifted by 1 pixels\n"},
    {"BC1, incomplete blocks, quiet",
        "rgb-noflip.ktx2", ImporterFlag::Quiet, {},
        "Bc1RGB", CompressedPixelFormat::Bc1RGBSrgb, true,
        nullptr},
    {"BC1, Y up, verbose",
        /* This file has Y up KTXorientation patched in */
        "rgb.ktx2", ImporterFlag::Verbose, {},
        "Bc1RGB", CompressedPixelFormat::Bc1RGBSrgb, false,
        nullptr},
    {"BC1, assume Y up, verbose",
        "rgb-linear-pow2.ktx2", ImporterFlag::Verbose, true,
        "Bc1RGB", CompressedPixelFormat::Bc1RGBUnorm, false,
        nullptr},
    {"BC1, assume Y down, verbose",
        "rgb-linear-pow2.ktx2", ImporterFlag::Verbose, false,
        "Bc1RGB", CompressedPixelFormat::Bc1RGBUnorm, true,
        "Trade::BasisImporter::openData(): image will be flipped along Y\n"},
    {"BC3",
        "rgb-noflip-pow2.ktx2", {}, {},
        "Bc3RGBA", CompressedPixelFormat::Bc3RGBASrgb, true,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"},
    {"BC4",
        "rgb-noflip-pow2.ktx2", {}, {},
        "Bc4R", CompressedPixelFormat::Bc4RUnorm, true,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"},
    {"BC5",
        "rgb-noflip-pow2.ktx2", {}, {},
        "Bc5RG", CompressedPixelFormat::Bc5RGUnorm, true,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"},
    /* BC7 is usually not compiled for Emscripten because it's _huge_ */
    #if !defined(BASISD_SUPPORT_BC7) || BASISD_SUPPORT_BC7 != 0
    {"BC7, flip not implemented",
        "rgb-noflip.ktx2", {}, {},
        "Bc7RGBA", CompressedPixelFormat::Bc7RGBASrgb, false,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::image2D(): Y flip is not yet implemented for CompressedPixelFormat::Bc7RGBASrgb, imported data will have wrong orientation. Enable assumeYUp to suppress this warning.\n"},
    #endif
    {"PVRTC RGB, flip not implemented",
        "rgb-noflip-pow2.ktx2", {}, {},
        "PvrtcRGB4bpp", CompressedPixelFormat::PvrtcRGB4bppSrgb, false,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::image2D(): Y flip is not yet implemented for CompressedPixelFormat::PvrtcRGB4bppSrgb, imported data will have wrong orientation. Enable assumeYUp to suppress this warning.\n"},
    {"PVRTC RGBA, flip not implemented",
        "rgb-noflip-pow2.ktx2", {}, {},
        "PvrtcRGBA4bpp", CompressedPixelFormat::PvrtcRGBA4bppSrgb, false,
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::image2D(): Y flip is not yet implemented for CompressedPixelFormat::PvrtcRGBA4bppSrgb, imported data will have wrong orientation. Enable assumeYUp to suppress this warning.\n"},
    {"ASTC 4x4, flip not implemented",
        "rgb-noflip.basis", {}, {},
        "Astc4x4RGBA", CompressedPixelFormat::Astc4x4RGBASrgb, false,
        "Trade::BasisImporter::image2D(): Y flip is not yet implemented for CompressedPixelFormat::Astc4x4RGBASrgb, imported data will have wrong orientation. Enable assumeYUp to suppress this warning.\n"},
};

const struct {
    const char* name;
    const char* extension;
    ImporterFlags flags;
    Containers::Optional<bool> assumeYUp;
    const char* message;
} Image3DData[]{
    /* In case of Basis, the image is marked as 3D. Which doesn't make sense
       as the mip levels don't shrink along Z, so we patch the type to say 2D
       array and print a warning. */
    {"Basis", ".basis", {}, {},
        "Trade::BasisImporter::openData(): importing 3D texture as a 2D array texture\n"},
    {"Basis", ".basis", ImporterFlag::Quiet, {},
        ""},
    /* On the other hand, for KTX2 files, the image is always marked as 2D
       array and never as 3D, so no warning here.

       But, there's a different warning, specific to KTX2 -- THE DAMN THING
       still doesn't write proper KTXorientation, so opening any KTX file
       produced by it (and not by our BasisImageConverter, which patches that
       in after) will warn no matter whether it was flipped or not:
       https://github.com/BinomialLLC/basis_universal/issues/258

       In other words, the data *does have* correct orientation, it's just not
       marked as such, so we have to force the orientation to not have the
       data flipped back. */
    {"KTX2", ".ktx2", {}, true,
        ""},
};

const struct {
    const char* name;
    const char* file;
    /* See FileTypeData.hasMissingOrientationMetadata for details */
    bool hasMissingOrientationMetadata;
} VideoSeekingData[]{
    {"Basis ETC1S", "rgba-video.basis", false},
    {"KTX2 ETC1S", "rgba-video.ktx2", true},
    {"Basis UASTC", "rgba-video-uastc.basis", false},
    {"KTX2 UASTC", "rgba-video-uastc.ktx2", true}
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

BasisImporterTest::BasisImporterTest() {
    addTests({&BasisImporterTest::empty});

    addInstancedTests({&BasisImporterTest::invalidHeader},
                      Containers::arraySize(InvalidHeaderData));

    addInstancedTests({&BasisImporterTest::invalidFile},
                      Containers::arraySize(InvalidFileData));

    addInstancedTests({&BasisImporterTest::fileTooShort},
                      Containers::arraySize(FileTooShortData));

    addInstancedTests({&BasisImporterTest::unconfigured},
        Containers::arraySize(QuietData));

    addTests({&BasisImporterTest::invalidConfiguredFormat,
              &BasisImporterTest::unsupportedFormat,
              &BasisImporterTest::transcodingFailure,
              &BasisImporterTest::nonBasisKtx});

    #ifdef MAGNUM_BUILD_DEPRECATED
    addInstancedTests({&BasisImporterTest::texture},
                      Containers::arraySize(TextureData));
    #endif

    addInstancedTests({&BasisImporterTest::rgbUncompressed},
        Containers::arraySize(FileTypeData));

    addInstancedTests({&BasisImporterTest::rgbUncompressedLinear,
                       &BasisImporterTest::rgbaUncompressed,
                       &BasisImporterTest::rgbaUncompressedUastc},
                      Containers::arraySize(FileTypeData));

    addTests({&BasisImporterTest::rgbaUncompressedMultipleImages});

    addInstancedTests({&BasisImporterTest::rgb,
                       &BasisImporterTest::rgba,
                       &BasisImporterTest::linear},
                      Containers::arraySize(FormatData));

    addInstancedTests({&BasisImporterTest::array2D,
                       &BasisImporterTest::array2DMipmaps,
                       &BasisImporterTest::video},
                      Containers::arraySize(FileTypeData));

    addInstancedTests({&BasisImporterTest::image3D,
                       &BasisImporterTest::image3DMipmaps},
        Containers::arraySize(Image3DData));

    addInstancedTests({&BasisImporterTest::cubeMap,
                       &BasisImporterTest::cubeMapArray},
                      Containers::arraySize(FileTypeData));

    addInstancedTests({&BasisImporterTest::videoSeeking},
                      Containers::arraySize(VideoSeekingData));

    addTests({&BasisImporterTest::videoVerbose});

    addInstancedTests({&BasisImporterTest::flipUncompressed},
        Containers::arraySize(FlipUncompressedData));

    addTests({&BasisImporterTest::flipUncompressed3D});

    addInstancedTests({&BasisImporterTest::flip},
        Containers::arraySize(FlipData));

    addTests({&BasisImporterTest::flip3D});

    addInstancedTests({&BasisImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&BasisImporterTest::openSameTwice,
              &BasisImporterTest::openDifferent,
              &BasisImporterTest::importMultipleFormats});

    /* Pull in the AnyImageImporter dependency for image comparison */
    _manager.load("AnyImageImporter");
    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    _manager.setPluginDirectory({});
    #endif
    /* Load StbImageImporter from the build tree, if defined. Otherwise it's
       static and already loaded. */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef BASISIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(BASISIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef BCDECIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(BCDECIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef ETCDECIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(ETCDECIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void BasisImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");

    std::ostringstream out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::openData(): the file is empty\n");
}

void BasisImporterTest::invalidHeader() {
    auto&& data = InvalidHeaderData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openData(data.data));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::BasisImporter::openData(): {}\n", data.message));
}

void BasisImporterTest::invalidFile() {
    auto&& data = InvalidFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");

    Containers::Optional<Containers::Array<char>> basisData = Utility::Path::read(Utility::Path::join(BASISIMPORTER_TEST_DIR, data.file));
    CORRADE_VERIFY(basisData);

    if(data.offset != ~std::size_t(0)) {
        CORRADE_VERIFY(data.offset < basisData->size());
        (*basisData)[data.offset] = data.value;
    }

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->openData(*basisData));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::BasisImporter::openData(): {}\n", data.message));
}

void BasisImporterTest::fileTooShort() {
    auto&& data = FileTooShortData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");

    Containers::Optional<Containers::Array<char>> basisData = Utility::Path::read(Utility::Path::join(BASISIMPORTER_TEST_DIR, data.file));
    CORRADE_VERIFY(basisData);

    std::ostringstream out;
    Error redirectError{&out};

    /* Shorten the data */
    CORRADE_COMPARE_AS(data.size, basisData->size(), TestSuite::Compare::Less);
    CORRADE_VERIFY(!importer->openData(basisData->prefix(data.size)));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::BasisImporter::openData(): {}\n", data.message));
}

void BasisImporterTest::unconfigured() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    importer->addFlags(data.flags);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));

    std::ostringstream out;
    Containers::Optional<Trade::ImageData2D> image;
    {
        Warning redirectWarning{&out};
        image = importer->image2D(0);
        /* It should warn just once, not spam every time */
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));

    if(data.quiet)
        CORRADE_COMPARE(out.str(), "");
    else
        CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): no format to transcode to was specified, falling back to uncompressed RGBA8. To get rid of this warning, either explicitly set the format option to one of Etc1RGB, Etc2RGBA, EacR, EacRG, Bc1RGB, Bc3RGBA, Bc4R, Bc5RG, Bc7RGBA, PvrtcRGB4bpp, PvrtcRGBA4bpp, Astc4x4RGBA or RGBA8, or load the plugin via one of its BasisImporterEtc1RGB, ... aliases.\n");

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    CORRADE_COMPARE_WITH(Containers::arrayCast<const Color3ub>(image->pixels<Color4ub>()),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 58.334f, 6.622f}));
}

void BasisImporterTest::invalidConfiguredFormat() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));

    std::ostringstream out;
    Error redirectError{&out};
    importer->configuration().setValue("format", "Banana");
    CORRADE_VERIFY(!importer->image2D(0));

    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): invalid transcoding target format Banana, expected to be one of Etc1RGB, Etc2RGBA, EacR, EacRG, Bc1RGB, Bc3RGBA, Bc4R, Bc5RG, Bc7RGBA, Pvrtc1RGB4bpp, Pvrtc1RGBA4bpp, Astc4x4RGBA or RGBA8\n");
}

void BasisImporterTest::unsupportedFormat() {
    #if !defined(BASISD_SUPPORT_BC7) || BASISD_SUPPORT_BC7
    /* BC7 is YUUGE and thus defined out on Emscripten. Skip the test if that's
       NOT the case. This assumes -DBASISD_SUPPORT_*=0 is supplied globally. */
    CORRADE_SKIP("BC7 is compiled into Basis, can't test unsupported target format error.");
    #endif

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));

    std::ostringstream out;
    Error redirectError{&out};
    importer->configuration().setValue("format", "Bc7RGBA");
    CORRADE_VERIFY(!importer->image2D(0));

    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): Basis Universal was compiled without support for Bc7RGBA\n");
}

void BasisImporterTest::transcodingFailure() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterPvrtcRGB4bpp");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));

    std::ostringstream out;
    Error redirectError{&out};

    /* PVRTC1 requires power of 2 image dimensions, but rgb.basis is 63x27,
       hence basis will fail during transcoding */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(!image);
    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): transcoding failed\n");
}

void BasisImporterTest::nonBasisKtx() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgba.ktx2")));
    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::openData(): invalid KTX2 header, or not Basis compressed; might want to use KtxImporter instead\n");
}

#ifdef MAGNUM_BUILD_DEPRECATED
void BasisImporterTest::texture() {
    auto&& data = TextureData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");

    for(std::size_t i = 0; i != Containers::arraySize(FileTypeData); ++i) {
        const auto fileType = FileTypeData[i];
        CORRADE_ITERATION(fileType.name);

        std::ostringstream out;
        {
            Warning redirectWarning{&out};
            CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, Containers::StringView{data.fileBase} + fileType.extension)));
        }
        /* Not testing ImporterFlag::Quiet here as this functionality is
           deprecated */
        CORRADE_COMPARE(out.str(), data.warning[i]);

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
            {
                CORRADE_EXPECT_FAIL_IF(data.xfailType,
                    "Basis-exported data don't match texture type, so it's patched by us on import.");
                CORRADE_COMPARE(texture->type(), data.type);
            }
            if(data.xfailType) CORRADE_COMPARE(texture->type(), data.xfailType);
        }

        UnsignedInt dimensions;
        switch(data.type) {
            case TextureType::Texture2D:
                dimensions = 2;
                break;
            case TextureType::Texture3D:
            case TextureType::Texture2DArray:
            case TextureType::CubeMap:
            case TextureType::CubeMapArray:
                dimensions = 3;
                break;
            /* No 1D array / 3D array allowed */
            default: CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }
        CORRADE_COMPARE(counts[dimensions - 1], total);
    }
}
#endif

void BasisImporterTest::rgbUncompressed() {
    auto&& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_COMPARE(importer->configuration().value("format"), "RGBA8");
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
        "rgb"_s + data.extension)));
    }
    /* There should be no Y-flip warning as the image is pre-flipped (and
       KTXorientation is patched into rgb.ktx2, see convert.sh) */
    CORRADE_COMPARE(out.str(), "");
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    CORRADE_COMPARE_WITH(Containers::arrayCast<const Color3ub>(image->pixels<Color4ub>()),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 58.334f, 6.622f}));
}

void BasisImporterTest::rgbUncompressedLinear() {
    auto&& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    /* See the comment above this variable for more details. The
       rgb-linear.{basis,ktx2} are both encoded with Y up in convert.sh but
       the KTX files are missing the KTXorientation metadata, causing a
       warning. */
    if(data.hasMissingOrientationMetadata)
        importer->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
        "rgb-linear"_s + data.extension)));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
    CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    CORRADE_COMPARE_WITH(Containers::arrayCast<const Color3ub>(image->pixels<Color4ub>()),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 61.0f, 6.321f}));
}

void BasisImporterTest::rgbaUncompressed() {
    auto&& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    /* See the comment above this variable for more details. The
       rgba.{basis,ktx2} are both encoded with Y up in convert.sh but the KTX
       files are missing the KTXorientation metadata, causing a warning. */
    if(data.hasMissingOrientationMetadata)
        importer->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
        "rgba"_s + data.extension)));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    CORRADE_COMPARE_WITH(image->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 94.3f, 8.039f}));
}

void BasisImporterTest::rgbaUncompressedUastc() {
    auto&& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    /* See the comment above this variable for more details. The
       rgb-uastc.{basis,ktx2} are both encoded with Y up in convert.sh but
       the KTX files are missing the KTXorientation metadata, causing a
       warning. */
    if(data.hasMissingOrientationMetadata)
        importer->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
        "rgba-uastc"_s + data.extension)));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    CORRADE_COMPARE_WITH(image->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        /* There are insignificant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 4.75f, 0.576f}));
}

void BasisImporterTest::rgbaUncompressedMultipleImages() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
        "rgba-2images-mips.basis")));
    CORRADE_COMPARE(importer->image2DCount(), 2);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 3);
    CORRADE_COMPARE(importer->image2DLevelCount(1), 2);

    Containers::Optional<Trade::ImageData2D> image0 = importer->image2D(0);
    Containers::Optional<Trade::ImageData2D> image0l1 = importer->image2D(0, 1);
    Containers::Optional<Trade::ImageData2D> image0l2 = importer->image2D(0, 2);
    CORRADE_VERIFY(image0);
    CORRADE_VERIFY(image0l1);
    CORRADE_VERIFY(image0l2);

    Containers::Optional<Trade::ImageData2D> image1 = importer->image2D(1);
    Containers::Optional<Trade::ImageData2D> image1l1 = importer->image2D(1, 1);
    CORRADE_VERIFY(image1);
    CORRADE_VERIFY(image1l1);

    for(auto* image: {&*image0, &*image0l1, &*image0l2,
                      &*image1, &*image1l1}) {
        CORRADE_ITERATION(image->size());
        CORRADE_VERIFY(!image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlags2D{});
        CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    }

    CORRADE_COMPARE(image0->size(), (Vector2i{63, 27}));
    CORRADE_COMPARE(image0l1->size(), (Vector2i{31, 13}));
    CORRADE_COMPARE(image0l2->size(), (Vector2i{15, 6}));
    CORRADE_COMPARE(image1->size(), (Vector2i{13, 31}));
    CORRADE_COMPARE(image1l1->size(), (Vector2i{6, 15}));

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    /* There are moderately significant compression artifacts, larger than with
       one image alone as there's more to compress */
    CORRADE_COMPARE_WITH(image0->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        (DebugTools::CompareImageToFile{_manager, 96.0f, 8.11f}));
    CORRADE_COMPARE_WITH(image0l1->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-31x13.png"),
        (DebugTools::CompareImageToFile{_manager, 75.75f, 14.077f}));
    CORRADE_COMPARE_WITH(image0l2->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-15x6.png"),
        (DebugTools::CompareImageToFile{_manager, 65.0f, 23.487f}));
    CORRADE_COMPARE_WITH(image1->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-13x31.png"),
        (DebugTools::CompareImageToFile{_manager, 55.5f, 12.305f}));
    CORRADE_COMPARE_WITH(image1l1->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-6x15.png"),
        (DebugTools::CompareImageToFile{_manager, 68.25f, 21.792f}));
}

void BasisImporterTest::rgb() {
    auto& formatData = FormatData[testCaseInstanceId()];
    setTestCaseDescription(formatData.suffix);

    #if defined(BASISD_SUPPORT_BC7) && !BASISD_SUPPORT_BC7
    /* BC7 is YUUGE and thus defined out on Emscripten. Skip the test if that's
       the case. This assumes -DBASISD_SUPPORT_*=0 is supplied globally. */
    if(formatData.expectedFormat == CompressedPixelFormat::Bc7RGBASrgb)
        CORRADE_SKIP("This format is not compiled into Basis.");
    #endif

    const Containers::String pluginName = "BasisImporter"_s + formatData.suffix;

    for(const auto& fileType: FileTypeData) {
        CORRADE_ITERATION(fileType.name);

        Containers::Pointer<AbstractImporter> importer = _manager.instantiate(pluginName);
        CORRADE_COMPARE(importer->configuration().value("format"), formatData.suffix);
        /* See the comment above this variable for more details. The files used
           by this function are all encoded with Y up in convert.sh, but
           except for rgb.ktx2 (which has it patched in) the KTX files are
           missing the KTXorientation metadata, causing a warning. */
        if(fileType.hasMissingOrientationMetadata && formatData.fileBase != "rgb"_s)
            importer->configuration().setValue("assumeYUp", true);

        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
            Containers::StringView{formatData.fileBase} + fileType.extension)));
        CORRADE_COMPARE(importer->image2DCount(), 1);

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlags2D{});
        CORRADE_COMPARE(image->compressedFormat(), formatData.expectedFormat);
        CORRADE_COMPARE(image->size(), formatData.expectedSize);
        /** @todo remove this once CompressedImage etc. tests for data size on
            its own / we're able to decode the data ourselves */
        CORRADE_COMPARE(image->data().size(), compressedPixelFormatBlockDataSize(formatData.expectedFormat)*((image->size() + compressedPixelFormatBlockSize(formatData.expectedFormat).xy() - Vector2i{1})/compressedPixelFormatBlockSize(formatData.expectedFormat).xy()).product());
    }
}

void BasisImporterTest::rgba() {
    auto& formatData = FormatData[testCaseInstanceId()];
    setTestCaseDescription(formatData.suffix);

    #if defined(BASISD_SUPPORT_BC7) && !BASISD_SUPPORT_BC7
    /* BC7 is YUUGE and thus defined out on Emscripten. Skip the test if that's
       the case. This assumes -DBASISD_SUPPORT_*=0 is supplied globally. */
    if(formatData.expectedFormat == CompressedPixelFormat::Bc7RGBASrgb)
        CORRADE_SKIP("This format is not compiled into Basis.");
    #endif

    const Containers::String pluginName = "BasisImporter"_s + formatData.suffix;

    for(const auto& fileType: FileTypeData) {
        CORRADE_ITERATION(fileType.name);

        Containers::Pointer<AbstractImporter> importer = _manager.instantiate(pluginName);
        CORRADE_COMPARE(importer->configuration().value("format"), formatData.suffix);
        /* See the comment above this variable for more details. The files used
           by this function are all encoded with Y up in convert.sh, but
           the KTX files are missing the KTXorientation metadata, causing a
           warning. */
        if(fileType.hasMissingOrientationMetadata)
            importer->configuration().setValue("assumeYUp", true);

        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
            Containers::String{formatData.fileBaseAlpha} + fileType.extension)));
        CORRADE_COMPARE(importer->image2DCount(), 1);

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlags2D{});
        CORRADE_COMPARE(image->compressedFormat(), formatData.expectedFormat);
        CORRADE_COMPARE(image->size(), formatData.expectedSize);
        /** @todo remove this once CompressedImage etc. tests for data size on
            its own / we're able to decode the data ourselves */
        CORRADE_COMPARE(image->data().size(), compressedPixelFormatBlockDataSize(formatData.expectedFormat)*((image->size() + compressedPixelFormatBlockSize(formatData.expectedFormat).xy() - Vector2i{1})/compressedPixelFormatBlockSize(formatData.expectedFormat).xy()).product());
    }
}

void BasisImporterTest::linear() {
    auto& formatData = FormatData[testCaseInstanceId()];
    setTestCaseDescription(formatData.suffix);

    /* Test linear formats, sRGB was tested in rgb() */

    #if defined(BASISD_SUPPORT_BC7) && !BASISD_SUPPORT_BC7
    /* BC7 is YUUGE and thus defined out on Emscripten. Skip the test if that's
       the case. This assumes -DBASISD_SUPPORT_*=0 is supplied globally. */
    if(formatData.expectedLinearFormat == CompressedPixelFormat::Bc7RGBAUnorm)
        CORRADE_SKIP("This format is not compiled into Basis.");
    #endif

    const Containers::String pluginName = "BasisImporter"_s + formatData.suffix;

    for(const auto& fileType: FileTypeData) {
        CORRADE_ITERATION(fileType.name);

        Containers::Pointer<AbstractImporter> importer = _manager.instantiate(pluginName);
        CORRADE_COMPARE(importer->configuration().value("format"), formatData.suffix);
        /* See the comment above this variable for more details. The files used
           by this function are all encoded with Y up in convert.sh, but the
           KTX files are missing the KTXorientation metadata, causing a
           warning. */
        if(fileType.hasMissingOrientationMetadata)
            importer->configuration().setValue("assumeYUp", true);

        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
            Containers::StringView{formatData.fileBaseLinear} + fileType.extension)));
        CORRADE_COMPARE(importer->image2DCount(), 1);

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(image->isCompressed());
        CORRADE_COMPARE(image->flags(), ImageFlags2D{});
        CORRADE_COMPARE(image->compressedFormat(), formatData.expectedLinearFormat);
        CORRADE_COMPARE(image->size(), formatData.expectedSize);
        /** @todo remove this once CompressedImage etc. tests for data size on
            its own / we're able to decode the data ourselves */
        CORRADE_COMPARE(image->data().size(), compressedPixelFormatBlockDataSize(formatData.expectedLinearFormat)*((image->size() + compressedPixelFormatBlockSize(formatData.expectedLinearFormat).xy() - Vector2i{1})/compressedPixelFormatBlockSize(formatData.expectedLinearFormat).xy()).product());
    }
}

void BasisImporterTest::array2D() {
    auto& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    /* See the comment above this variable for more details. The
       rgb-array.{basis,ktx2} are both encoded with Y up in convert.sh but the
       KTX files are missing the KTXorientation metadata, causing a warning. */
    if(data.hasMissingOrientationMetadata)
        importer->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-array"_s + data.extension)));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlag3D::Array);
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector3i{63, 27, 3}));

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    /* There are moderately significant compression artifacts */
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[0],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        (DebugTools::CompareImageToFile{_manager, 94.0f, 8.064f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[1],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27-slice1.png"),
        (DebugTools::CompareImageToFile{_manager, 74.0f, 6.481f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[2],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27-slice2.png"),
        (DebugTools::CompareImageToFile{_manager, 88.0f, 8.426f}));
}

void BasisImporterTest::array2DMipmaps() {
    auto& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    /* See the comment above this variable for more details. The
       rgba-array-mips.{basis,ktx2} are both encoded with Y up in convert.sh
       but the KTX files are missing the KTXorientation metadata, causing a
       warning. */
    if(data.hasMissingOrientationMetadata)
        importer->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-array-mips"_s + data.extension)));

    Containers::Optional<Trade::ImageData3D> levels[3];

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), Containers::arraySize(levels));

    for(std::size_t i = 0; i != Containers::arraySize(levels); ++i) {
        CORRADE_ITERATION(i);
        levels[i] = importer->image3D(0, i);
        CORRADE_VERIFY(levels[i]);

        CORRADE_VERIFY(!levels[i]->isCompressed());
        CORRADE_COMPARE(levels[i]->flags(), ImageFlag3D::Array);
        CORRADE_COMPARE(levels[i]->format(), PixelFormat::RGBA8Srgb);
        CORRADE_COMPARE(levels[i]->size(), (Vector3i{Vector2i{63, 27} >> i, 3}));
    }

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    /* There are moderately significant compression artifacts */
    CORRADE_COMPARE_WITH(levels[0]->pixels<Color4ub>()[0],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        (DebugTools::CompareImageToFile{_manager, 96.0f, 8.027f}));
    CORRADE_COMPARE_WITH(levels[0]->pixels<Color4ub>()[1],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27-slice1.png"),
        (DebugTools::CompareImageToFile{_manager, 74.0f, 6.482f}));
    CORRADE_COMPARE_WITH(levels[0]->pixels<Color4ub>()[2],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27-slice2.png"),
        (DebugTools::CompareImageToFile{_manager, 90.0f, 8.399f}));

    /* Only testing the first layer's mips so we don't need all the slices'
       mips as ground truth data, too */
    CORRADE_COMPARE_WITH(levels[1]->pixels<Color4ub>()[0],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-31x13.png"),
        (DebugTools::CompareImageToFile{_manager, 75.75f, 14.132f}));
    CORRADE_COMPARE_WITH(levels[2]->pixels<Color4ub>()[0],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-15x6.png"),
        (DebugTools::CompareImageToFile{_manager, 65.0f, 23.47f}));
}

void BasisImporterTest::video() {
    auto& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    /* See the comment above this variable for more details. The
       rgba-video.{basis,ktx2} are both encoded with Y up in convert.sh but the
       KTX files are missing the KTXorientation metadata, causing a warning. */
    if(data.hasMissingOrientationMetadata)
        importer->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-video"_s + data.extension)));

    Containers::Optional<Trade::ImageData2D> frames[3];

    CORRADE_COMPARE(importer->image2DCount(), Containers::arraySize(frames));

    for(UnsignedInt i = 0; i != Containers::arraySize(frames); ++i) {
        frames[i] = importer->image2D(i);
        CORRADE_VERIFY(frames[i]);
        CORRADE_VERIFY(!frames[i]->isCompressed());
        CORRADE_COMPARE(frames[i]->flags(), ImageFlags2D{});
        CORRADE_COMPARE(frames[i]->format(), PixelFormat::RGBA8Srgb);
        CORRADE_COMPARE(frames[i]->size(), (Vector2i{63, 27}));
    }

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    /* There are moderately significant compression artifacts */
    CORRADE_COMPARE_WITH(frames[0]->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        (DebugTools::CompareImageToFile{_manager, 96.25f, 8.198f}));
    CORRADE_COMPARE_WITH(frames[1]->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27-slice1.png"),
        (DebugTools::CompareImageToFile{_manager, 74.0f, 6.507f}));
    CORRADE_COMPARE_WITH(frames[2]->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27-slice2.png"),
        (DebugTools::CompareImageToFile{_manager, 76.0f, 8.311f}));
}

void BasisImporterTest::image3D() {
    auto& data = Image3DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    importer->addFlags(data.flags);
    if(data.assumeYUp)
        importer->configuration().setValue("assumeYUp", *data.assumeYUp);

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-3d"_s + data.extension)));
    }
    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(out.str(), data.message);

    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    /* 3D images actually behave like 2D arrays, and are just mislabeled */
    CORRADE_COMPARE(image->flags(), ImageFlag3D::Array);
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector3i{63, 27, 3}));

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    /* There are moderately significant compression artifacts */
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[0],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        (DebugTools::CompareImageToFile{_manager, 94.0f, 8.064f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[1],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27-slice1.png"),
        (DebugTools::CompareImageToFile{_manager, 74.0f, 6.481f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[2],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27-slice2.png"),
        (DebugTools::CompareImageToFile{_manager, 88.0f, 8.426f}));
}

void BasisImporterTest::image3DMipmaps() {
    /* Data is identical to array2DMipmaps. See Image3DData for an explanation
       of what is being tested here. */
    auto& data = Image3DData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    if(data.assumeYUp)
        importer->configuration().setValue("assumeYUp", *data.assumeYUp);
    importer->addFlags(data.flags);

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-3d-mips"_s + data.extension)));
    }
    CORRADE_COMPARE(out.str(), data.message);

    Containers::Optional<Trade::ImageData3D> levels[3];

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), Containers::arraySize(levels));

    for(std::size_t i = 0; i != Containers::arraySize(levels); ++i) {
        CORRADE_ITERATION(i);

        levels[i] = importer->image3D(0, i);
        CORRADE_VERIFY(levels[i]);
        CORRADE_VERIFY(!levels[i]->isCompressed());
        /* 3D images actually behave like 2D arrays, and are just mislabeled */
        CORRADE_COMPARE(levels[i]->flags(), ImageFlag3D::Array);
        CORRADE_COMPARE(levels[i]->format(), PixelFormat::RGBA8Srgb);
        CORRADE_COMPARE(levels[i]->size(), (Vector3i{Vector2i{63, 27} >> i, 3}));
    }

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    /* There are moderately significant compression artifacts */
    CORRADE_COMPARE_WITH(levels[0]->pixels<Color4ub>()[0],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        (DebugTools::CompareImageToFile{_manager, 96.0f, 8.027f}));
    CORRADE_COMPARE_WITH(levels[0]->pixels<Color4ub>()[1],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27-slice1.png"),
        (DebugTools::CompareImageToFile{_manager, 74.0f, 6.482f}));
    CORRADE_COMPARE_WITH(levels[0]->pixels<Color4ub>()[2],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27-slice2.png"),
        (DebugTools::CompareImageToFile{_manager, 90.0f, 8.399f}));

    /* Only testing the first layer's mips so we don't need all the slices'
       mips as ground truth data, too */
    CORRADE_COMPARE_WITH(levels[1]->pixels<Color4ub>()[0],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-31x13.png"),
        (DebugTools::CompareImageToFile{_manager, 75.75f, 14.132f}));
    CORRADE_COMPARE_WITH(levels[2]->pixels<Color4ub>()[0],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-15x6.png"),
        (DebugTools::CompareImageToFile{_manager, 65.0f, 23.47f}));
}

void BasisImporterTest::cubeMap() {
    auto& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    /* See the comment above this variable for more details. The
       rgba-cubemap.{basis,ktx2} are both encoded with Y up in convert.sh but
       the KTX files are missing the KTXorientation metadata, causing a
       warning. */
    if(data.hasMissingOrientationMetadata)
        importer->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-cubemap"_s + data.extension)));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlag3D::CubeMap);
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector3i{27, 27, 6}));

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    /* There are moderately significant compression artifacts */
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[0],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27.png"),
        (DebugTools::CompareImageToFile{_manager, 94.0f, 10.83f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[1],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27-slice1.png"),
        (DebugTools::CompareImageToFile{_manager, 74.0f, 6.972f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[2],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27-slice2.png"),
        (DebugTools::CompareImageToFile{_manager, 88.0f, 10.591f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[3],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27.png"),
        (DebugTools::CompareImageToFile{_manager, 94.0f, 10.83f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[4],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27-slice1.png"),
        (DebugTools::CompareImageToFile{_manager, 74.0f, 6.972f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[5],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27-slice2.png"),
        (DebugTools::CompareImageToFile{_manager, 88.0f, 10.591f}));
}

void BasisImporterTest::cubeMapArray() {
    auto& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    /* See the comment above this variable for more details. The
       rgba-cubemap-array.{basis,ktx2} are both encoded with Y up in convert.sh
       but the KTX files are missing the KTXorientation metadata, causing a
       warning. */
    if(data.hasMissingOrientationMetadata)
        importer->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-cubemap-array"_s + data.extension)));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlag3D::CubeMap|ImageFlag3D::Array);
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector3i{27, 27, 12}));

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    /* There are moderately significant compression artifacts */
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[0],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27.png"),
        (DebugTools::CompareImageToFile{_manager, 94.0f, 10.83f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[1],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27-slice1.png"),
        (DebugTools::CompareImageToFile{_manager, 74.0f, 6.972f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[2],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27-slice2.png"),
        (DebugTools::CompareImageToFile{_manager, 88.0f, 10.591f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[3],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27.png"),
        (DebugTools::CompareImageToFile{_manager, 94.0f, 10.83f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[4],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27-slice1.png"),
        (DebugTools::CompareImageToFile{_manager, 74.0f, 6.972f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[5],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27-slice2.png"),
        (DebugTools::CompareImageToFile{_manager, 88.0f, 10.591f}));

    /* Second layer, 2nd and 3rd face are switched */
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[6],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27.png"),
        (DebugTools::CompareImageToFile{_manager, 94.0f, 10.83f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[8],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27-slice1.png"),
        (DebugTools::CompareImageToFile{_manager, 74.0f, 6.972f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[7],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27-slice2.png"),
        (DebugTools::CompareImageToFile{_manager, 88.0f, 10.591f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[9],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27.png"),
        (DebugTools::CompareImageToFile{_manager, 94.0f, 10.83f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[10],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27-slice1.png"),
        (DebugTools::CompareImageToFile{_manager, 74.0f, 6.972f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[11],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-27x27-slice2.png"),
        (DebugTools::CompareImageToFile{_manager, 88.0f, 10.591f}));
}

void BasisImporterTest::videoSeeking() {
    auto& data = VideoSeekingData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    /* See the comment above this variable for more details. The files are all
       encoded with Y up in convert.sh but the KTX files are missing the
       KTXorientation metadata, causing a warning. */
    if(data.hasMissingOrientationMetadata)
        importer->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, data.file)));

    CORRADE_COMPARE(importer->image2DCount(), 3);

    std::ostringstream out;
    Error redirectError{&out};

    CORRADE_VERIFY(!importer->image2D(2));
    CORRADE_VERIFY(importer->image2D(0));
    CORRADE_VERIFY(!importer->image2D(2));
    CORRADE_VERIFY(importer->image2D(1));
    CORRADE_VERIFY(importer->image2D(2));
    CORRADE_VERIFY(importer->image2D(0));

    CORRADE_COMPARE(out.str(),
        "Trade::BasisImporter::image2D(): video frames must be transcoded sequentially, expected frame 0 but got 2\n"
        "Trade::BasisImporter::image2D(): video frames must be transcoded sequentially, expected frame 1 or 0 but got 2\n");
}

void BasisImporterTest::videoVerbose() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");

    std::ostringstream out;
    Debug redirectDebug{&out};

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-video.basis")));
    CORRADE_COMPARE(out.str(), "");

    importer->close();
    importer->setFlags(ImporterFlag::Verbose);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-video.basis")));
    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::openData(): file contains video frames, images must be transcoded sequentially\n");
}

void BasisImporterTest::flipUncompressed() {
    auto& data = FlipUncompressedData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* This won't flip the image and won't produce any warning either */
    Containers::Pointer<AbstractImporter> importerNotFlipped = _manager.instantiate("BasisImporter");
    importerNotFlipped->configuration().setValue("format", "RGBA8");
    importerNotFlipped->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importerNotFlipped->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, data.filename)));
    Containers::Optional<ImageData2D> imageNotFlipped = importerNotFlipped->image2D(0);
    CORRADE_VERIFY(imageNotFlipped);
    CORRADE_COMPARE(imageNotFlipped->format(), data.expectedFormat);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    importer->configuration().setValue("format", "RGBA8");
    importer->addFlags(data.flags);
    if(data.assumeYUp)
        importer->configuration().setValue("assumeYUp", *data.assumeYUp);

    Containers::Optional<ImageData2D> image;
    std::ostringstream out;
    {
        Debug redirectOutput{&out};
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, data.filename)));
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->format(), data.expectedFormat);
    if(data.message)
        CORRADE_COMPARE(out.str(), data.message);
    else
        CORRADE_COMPARE(out.str(), "");

    /* The images should then be flipped compared to each other, or if flip was
       not made, identical */
    if(!data.flipped) CORRADE_COMPARE_AS(*image,
        *imageNotFlipped,
        DebugTools::CompareImage);
    else CORRADE_COMPARE_AS(image->pixels<Vector4ub>().flipped<0>(),
        *imageNotFlipped,
        DebugTools::CompareImage);

    /* The image should also be relatively close to the original it was
       compressed from */
    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    /* As the PNGs are Y-flipped on import as well, the logic is different
       here compared to above. There are moderately significant compression
       artifacts; the alpha channel is all 1s. */
    /** @todo clean up once it's possible to specify image orientation on
        load with some importer flags */
    if(!data.flippedToPng) CORRADE_COMPARE_WITH(Containers::arrayCast<const Vector3ub>(image->pixels<Vector4ub>()),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"),
        (DebugTools::CompareImageToFile{_manager, 61.0f, 8.65f}));
    else CORRADE_COMPARE_WITH(Containers::arrayCast<const Vector3ub>(image->pixels<Vector4ub>().flipped<0>()),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"),
        (DebugTools::CompareImageToFile{_manager, 61.0f, 8.65f}));
}

void BasisImporterTest::flipUncompressed3D() {
    /* This won't flip the image and won't produce any warning either */
    Containers::Pointer<AbstractImporter> importerNotFlipped = _manager.instantiate("BasisImporter");
    importerNotFlipped->configuration().setValue("format", "RGBA8");
    importerNotFlipped->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importerNotFlipped->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-noflip-array.ktx2")));
    Containers::Optional<ImageData3D> imageNotFlipped = importerNotFlipped->image3D(0);
    CORRADE_VERIFY(imageNotFlipped);
    CORRADE_COMPARE(imageNotFlipped->format(), PixelFormat::RGBA8Srgb);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    importer->configuration().setValue("format", "RGBA8");
    importer->addFlags(ImporterFlag::Verbose);

    Containers::Optional<ImageData3D> image;
    std::ostringstream out;
    {
        Debug redirectOutput{&out};
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-noflip-array.ktx2")));
        image = importer->image3D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(out.str(),
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::openData(): image will be flipped along Y\n");

    /* The images should then be Y-flipped compared to each other, with Z slice
       order kept. Comparing slice by slice as CompareImage has no 3D support. */
    CORRADE_COMPARE(image->size(), (Vector3i{63, 27, 3}));
    CORRADE_COMPARE_AS(image->pixels<Vector4ub>()[0].flipped<0>(),
        (ImageView2D{imageNotFlipped->format(), {63, 27}, imageNotFlipped->data()}),
        DebugTools::CompareImage);
    CORRADE_COMPARE_AS(image->pixels<Vector4ub>()[1].flipped<0>(),
        (ImageView2D{imageNotFlipped->format(), {63, 27}, imageNotFlipped->data().exceptPrefix(4*63*27)}),
        DebugTools::CompareImage);
    CORRADE_COMPARE_AS(image->pixels<Vector4ub>()[2].flipped<0>(),
        (ImageView2D{imageNotFlipped->format(), {63, 27}, imageNotFlipped->data().exceptPrefix(2*4*63*27)}),
        DebugTools::CompareImage);

    /* The image should also be relatively close to the original it was
       compressed from */
    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    /* As the PNGs are Y-flipped on import as well, the imported image should
       simply match them. There are moderately significant compression
       artifacts. */
    /** @todo clean up once it's possible to specify image orientation on
        load with some importer flags */
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[0],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        (DebugTools::CompareImageToFile{_manager, 75.25f, 10.31f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[1],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27-slice1.png"),
        (DebugTools::CompareImageToFile{_manager, 58.0f, 8.30f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[2],
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27-slice2.png"),
        (DebugTools::CompareImageToFile{_manager, 85.5f, 10.02f}));
}

void BasisImporterTest::flip() {
    auto& data = FlipData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* This won't flip the image and won't produce any warning either */
    Containers::Pointer<AbstractImporter> importerNotFlipped = _manager.instantiate("BasisImporter");
    importerNotFlipped->configuration().setValue("format", data.format);
    importerNotFlipped->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importerNotFlipped->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, data.filename)));
    Containers::Optional<ImageData2D> imageNotFlipped = importerNotFlipped->image2D(0);
    CORRADE_VERIFY(imageNotFlipped);
    CORRADE_COMPARE(imageNotFlipped->compressedFormat(), data.expectedFormat);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    importer->configuration().setValue("format", data.format);
    importer->addFlags(data.flags);
    if(data.assumeYUp)
        importer->configuration().setValue("assumeYUp", *data.assumeYUp);

    Containers::Optional<ImageData2D> image;
    std::ostringstream out;
    {
        Debug redirectOutput{&out};
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, data.filename)));
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->compressedFormat(), data.expectedFormat);
    if(data.message)
        CORRADE_COMPARE(out.str(), data.message);
    else
        CORRADE_COMPARE(out.str(), "");

    /* The images, once decoded, should then be flipped compared to each other,
       or if flip was not possible, identical */

    Containers::StringView decoderName;
    if(Containers::StringView{data.format}.hasPrefix("Bc"))
        decoderName = "BcDecImageConverter";
    else if(Containers::StringView{data.format}.hasPrefix("Etc") ||
            Containers::StringView{data.format}.hasPrefix("Eac"))
        decoderName = "EtcDecImageConverter";
    else CORRADE_SKIP("No decoder for" << data.expectedFormat << Debug::nospace << ", cannot test decoded image equality.");

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

    /* The flipping is done on whole blocks, so in case of incomplete blocks
       the comparison would fail. Round the image size up to whole blocks --
       BcDec / EtcDec outputs are laid out like that internally already, it's
       just the size being smaller. */
    const Vector2i blockSize = compressedPixelFormatBlockSize(data.expectedFormat).xy();
    ImageView2D decodedNotFlippedWholeBlocks{decodedNotFlipped->format(),
        blockSize*((decodedNotFlipped->size() + blockSize - Vector2i{1})/blockSize),
        decodedNotFlipped->data()};
    ImageView2D decodedWholeBlocks{decoded->format(),
        blockSize*((decoded->size() + blockSize - Vector2i{1})/blockSize),
        decoded->data()};

    if(!data.flipped) CORRADE_COMPARE_AS(decodedWholeBlocks,
        decodedNotFlippedWholeBlocks,
        DebugTools::CompareImage);
    else if(decoded->format() == PixelFormat::RGBA8Unorm ||
            decoded->format() == PixelFormat::RGBA8Srgb)
        CORRADE_COMPARE_AS(decodedWholeBlocks.pixels<Vector4ub>().flipped<0>(),
            decodedNotFlippedWholeBlocks,
            DebugTools::CompareImage);
    else if(decoded->format() == PixelFormat::RG8Unorm)
        CORRADE_COMPARE_AS(decodedWholeBlocks.pixels<Vector2ub>().flipped<0>(),
            decodedNotFlippedWholeBlocks,
            DebugTools::CompareImage);
    else if(decoded->format() == PixelFormat::RG8Snorm)
        CORRADE_COMPARE_AS(decodedWholeBlocks.pixels<Vector2b>().flipped<0>(),
            decodedNotFlippedWholeBlocks,
            DebugTools::CompareImage);
    else if(decoded->format() == PixelFormat::R8Unorm)
        CORRADE_COMPARE_AS(decodedWholeBlocks.pixels<UnsignedByte>().flipped<0>(),
            decodedNotFlippedWholeBlocks,
            DebugTools::CompareImage);
    else CORRADE_FAIL("Unexpected format" << decoded->format());
}

void BasisImporterTest::flip3D() {
    /* This won't flip the image and won't produce any warning either */
    Containers::Pointer<AbstractImporter> importerNotFlipped = _manager.instantiate("BasisImporter");
    importerNotFlipped->configuration().setValue("format", "Bc1RGB");
    importerNotFlipped->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importerNotFlipped->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-noflip-array.ktx2")));
    Containers::Optional<ImageData3D> imageNotFlipped = importerNotFlipped->image3D(0);
    CORRADE_VERIFY(imageNotFlipped);
    CORRADE_COMPARE(imageNotFlipped->compressedFormat(), CompressedPixelFormat::Bc1RGBSrgb);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    importer->configuration().setValue("format", "Bc1RGB");
    importer->addFlags(ImporterFlag::Verbose);

    Containers::Optional<ImageData3D> image;
    std::ostringstream out;
    {
        Debug redirectOutput{&out};
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-noflip-array.ktx2")));
        image = importer->image3D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc1RGBSrgb);
    CORRADE_COMPARE(out.str(),
        "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.\n"
        "Trade::BasisImporter::openData(): image will be flipped along Y\n"
        "Trade::BasisImporter::image3D(): Y-flipping a compressed image that's not whole blocks, the result will be shifted by 1 pixels\n");

    /* Catch also ABI and interface mismatch errors */
    if(!(_converterManager.load("BcDecImageConverter") & PluginManager::LoadState::Loaded))
        CORRADE_SKIP("BcDecImageConverter plugin can't be loaded, cannot test decoded image equality.");

    /* The converter doesn't support 3D images yet (waiting for CompressedImage
       APIs to get more usable), manually convert each slice, padding it to
       whole blocks. The images should then be Y-flipped compared to each
       other, with Z slice order kept. */
    Containers::Pointer<Trade::AbstractImageConverter> decoder = _converterManager.loadAndInstantiate("BcDecImageConverter");
    CORRADE_COMPARE(imageNotFlipped->size(), (Vector3i{63, 27, 3}));
    CORRADE_COMPARE(image->size(), (Vector3i{63, 27, 3}));
    for(Int i: {0, 1, 2}) {
        CORRADE_ITERATION(i);

        Containers::Optional<Trade::ImageData2D> decodedNotFlipped = decoder->convert(CompressedImageView2D{
            imageNotFlipped->compressedFormat(), {64, 28},
            imageNotFlipped->data().sliceSize(i*16*7*8,
                                              16*7*8)});
        Containers::Optional<Trade::ImageData2D> decoded = decoder->convert(CompressedImageView2D{
            image->compressedFormat(), {64, 28},
            image->data().sliceSize(i*16*7*8,
                                    16*7*8)});
        CORRADE_VERIFY(decodedNotFlipped);
        CORRADE_VERIFY(decoded);

        CORRADE_COMPARE_AS(decoded->pixels<Vector4ub>().flipped<0>(),
            *decodedNotFlipped,
            DebugTools::CompareImage);
    }
}

void BasisImporterTest::openMemory() {
    /* Same as rgbaUncompressed() except that it uses openData() & openMemory()
       instead of openFile() to test data copying on import */

    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer);
    CORRADE_COMPARE(importer->configuration().value("format"), "RGBA8");
    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba.basis"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*importer, *memory));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    CORRADE_COMPARE_WITH(image->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 94.0f, 8.039f}));
}

void BasisImporterTest::openSameTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterEtc2RGBA");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));

    /* Shouldn't crash, leak or anything */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
}

void BasisImporterTest::openDifferent() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterEtc2RGBA");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-video.basis")));
    /* The rgba-cubemap-array.ktx2 is encoded with Y up in convert.sh but the
       KTX files are missing the KTXorientation metadata due to a basisu bug,
       causing a warning. See FileTypeData.hasMissingOrientationMetadata for
       details. */
    importer->configuration().setValue("assumeYUp", true);
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-cubemap-array.ktx2")));
    CORRADE_COMPARE(importer->image3DCount(), 1);

    /* Verify that everything is working properly with different files
       and transcoders. Shouldn't crash, leak or anything. */
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector3i{27, 27, 12}));
}

void BasisImporterTest::importMultipleFormats() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb-noflip.basis")));

    /* Verify that everything is working properly with reused transcoder */
    {
        std::ostringstream out;
        Containers::Optional<Trade::ImageData2D> image;
        {
            Warning redirectWarning{&out};
            image = importer->image2D(0);
        }
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
        CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
        CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): no format to transcode to was specified, falling back to uncompressed RGBA8. To get rid of this warning, either explicitly set the format option to one of Etc1RGB, Etc2RGBA, EacR, EacRG, Bc1RGB, Bc3RGBA, Bc4R, Bc5RG, Bc7RGBA, PvrtcRGB4bpp, PvrtcRGBA4bpp, Astc4x4RGBA or RGBA8, or load the plugin via one of its BasisImporterEtc1RGB, ... aliases.\n");

    /* Second time without a format change it shouldn't repeat the same
       warning */
    } {
        std::ostringstream out;
        Containers::Optional<Trade::ImageData2D> image;
        {
            Warning redirectWarning{&out};
            image = importer->image2D(0);
        }
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
        CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
        /* Warning printed above already, not printed second time */
        CORRADE_COMPARE(out.str(), "");

    /* Format changed, now it should print that it's not implemented */
    } {
        importer->configuration().setValue("format", "Etc2RGBA");

        std::ostringstream out;
        Containers::Optional<Trade::ImageData2D> image;
        {
            Warning redirectWarning{&out};
            image = importer->image2D(0);
        }
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGBA8Srgb);
        CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
        CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): Y flip is not yet implemented for CompressedPixelFormat::Etc2RGBA8Srgb, imported data will have wrong orientation. Enable assumeYUp to suppress this warning.\n");

    /* Second time it again shouldn't */
    } {
        std::ostringstream out;
        Containers::Optional<Trade::ImageData2D> image;
        {
            Warning redirectWarning{&out};
            image = importer->image2D(0);
        }
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGBA8Srgb);
        CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
        CORRADE_COMPARE(out.str(), "");

    /* For a new format it should again */
    } {
        importer->configuration().setValue("format", "Astc4x4RGBA");

        std::ostringstream out;
        Containers::Optional<Trade::ImageData2D> image;
        {
            Warning redirectWarning{&out};
            image = importer->image2D(0);
        }
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc4x4RGBASrgb);
        CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
        CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): Y flip is not yet implemented for CompressedPixelFormat::Astc4x4RGBASrgb, imported data will have wrong orientation. Enable assumeYUp to suppress this warning.\n");

    /* For an empty format it should again say that no format is set */
    } {
        importer->configuration().setValue("format", "");

        std::ostringstream out;
        Containers::Optional<Trade::ImageData2D> image;
        {
            Warning redirectWarning{&out};
            image = importer->image2D(0);
        }
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
        CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
        CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): no format to transcode to was specified, falling back to uncompressed RGBA8. To get rid of this warning, either explicitly set the format option to one of Etc1RGB, Etc2RGBA, EacR, EacRG, Bc1RGB, Bc3RGBA, Bc4R, Bc5RG, Bc7RGBA, PvrtcRGB4bpp, PvrtcRGBA4bpp, Astc4x4RGBA or RGBA8, or load the plugin via one of its BasisImporterEtc1RGB, ... aliases.\n");
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::BasisImporterTest)
