/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
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
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/TextureData.h>

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

    void texture();

    void rgbUncompressed();
    void rgbUncompressedNoFlip();
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

    void openMemory();
    void openSameTwice();
    void openDifferent();
    void importMultipleFormats();

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

using namespace Containers::Literals;

constexpr struct {
    const char* name;
    const char* extension;
} FileTypeData[] {
    {"Basis", ".basis"},
    {"KTX2", ".ktx2"}
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
    {"Invalid KTX2 header", Containers::arrayView("\xabKTX 20\xbb\r\n\x1a\n\xff\xff\xff\xff"), "invalid KTX2 header, or not Basis compressed"}
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
    {"KTX2", "rgb.ktx2", 64, "invalid KTX2 header, or not Basis compressed"}
};

const struct {
    const char* name;
    const char* fileBase;
    const TextureType type;
} TextureData[]{
    {"2D", "rgb", TextureType::Texture2D},
    {"2D array", "rgba-array", TextureType::Texture2DArray},
    {"Cube map", "rgba-cubemap", TextureType::CubeMap},
    {"Cube map array", "rgba-cubemap-array", TextureType::CubeMapArray},
    {"3D", "rgba-3d", TextureType::Texture3D},
    {"3D mipmaps", "rgba-3d-mips", TextureType::Texture3D},
    {"Video", "rgba-video", TextureType::Texture2D}
};

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
     "Etc1RGB", CompressedPixelFormat::Etc2RGB8Srgb, CompressedPixelFormat::Etc2RGB8Unorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
     "Etc2RGBA", CompressedPixelFormat::Etc2RGBA8Srgb, CompressedPixelFormat::Etc2RGBA8Unorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
     "Bc1RGB", CompressedPixelFormat::Bc1RGBSrgb, CompressedPixelFormat::Bc1RGBUnorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
     "Bc3RGBA", CompressedPixelFormat::Bc3RGBASrgb, CompressedPixelFormat::Bc3RGBAUnorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
     "Bc4R", CompressedPixelFormat::Bc4RUnorm, CompressedPixelFormat::Bc4RUnorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
     "Bc5RG", CompressedPixelFormat::Bc5RGUnorm, CompressedPixelFormat::Bc5RGUnorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
     "Bc7RGBA", CompressedPixelFormat::Bc7RGBASrgb, CompressedPixelFormat::Bc7RGBAUnorm},
    {"rgb-pow2", "rgba-pow2", "rgb-linear-pow2", {64, 32},
     "PvrtcRGB4bpp", CompressedPixelFormat::PvrtcRGB4bppSrgb, CompressedPixelFormat::PvrtcRGB4bppUnorm},
    {"rgb-pow2", "rgba-pow2", "rgb-linear-pow2", {64, 32},
     "PvrtcRGBA4bpp", CompressedPixelFormat::PvrtcRGBA4bppSrgb, CompressedPixelFormat::PvrtcRGBA4bppUnorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
     "Astc4x4RGBA", CompressedPixelFormat::Astc4x4RGBASrgb, CompressedPixelFormat::Astc4x4RGBAUnorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
     "EacR", CompressedPixelFormat::EacR11Unorm, CompressedPixelFormat::EacR11Unorm},
    {"rgb", "rgba", "rgb-linear", {63, 27},
     "EacRG", CompressedPixelFormat::EacRG11Unorm, CompressedPixelFormat::EacRG11Unorm}
};

const struct {
    const char* name;
    const char* file;
} VideoSeekingData[]{
    {"Basis ETC1S", "rgba-video.basis"},
    {"KTX2 ETC1S", "rgba-video.ktx2"},
    {"Basis UASTC", "rgba-video-uastc.basis"},
    {"KTX2 UASTC", "rgba-video-uastc.ktx2"}
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

    addTests({&BasisImporterTest::unconfigured,
              &BasisImporterTest::invalidConfiguredFormat,
              &BasisImporterTest::unsupportedFormat,
              &BasisImporterTest::transcodingFailure,
              &BasisImporterTest::nonBasisKtx});

    addInstancedTests({&BasisImporterTest::texture},
                      Containers::arraySize(TextureData));

    addInstancedTests({&BasisImporterTest::rgbUncompressed,
                       &BasisImporterTest::rgbUncompressedNoFlip,
                       &BasisImporterTest::rgbUncompressedLinear,
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
                       &BasisImporterTest::video,
                       &BasisImporterTest::image3D,
                       &BasisImporterTest::image3DMipmaps,
                       &BasisImporterTest::cubeMap,
                       &BasisImporterTest::cubeMapArray},
                      Containers::arraySize(FileTypeData));

    addInstancedTests({&BasisImporterTest::videoSeeking},
                      Containers::arraySize(VideoSeekingData));

    addTests({&BasisImporterTest::videoVerbose});

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
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");

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

    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): no format to transcode to was specified, falling back to uncompressed RGBA8. To get rid of this warning either load the plugin via one of its BasisImporterEtc1RGB, ... aliases, or explicitly set the format option in plugin configuration.\n");

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

    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): invalid transcoding target format Banana, expected to be one of EacR, EacRG, Etc1RGB, Etc2RGBA, Bc1RGB, Bc3RGBA, Bc4R, Bc5RG, Bc7RGBA, Pvrtc1RGB4bpp, Pvrtc1RGBA4bpp, Astc4x4RGBA, RGBA8\n");
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
    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::openData(): invalid KTX2 header, or not Basis compressed\n");
}

void BasisImporterTest::texture() {
    auto&& data = TextureData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporter");

    for(const auto& fileType: FileTypeData) {
        CORRADE_ITERATION(fileType.name);

        const bool isKtx2 = std::string{fileType.name} == "KTX2";
        const bool is3D = data.type == TextureType::Texture3D;
        /* basisu saves volume textures as KTX2 2D arrays, and we import 3D
           basis files as 2D arrays, too */
        const TextureType realType = is3D ? TextureType::Texture2DArray : data.type;

        std::ostringstream out;
        Warning redirectWarning{&out};

        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, Containers::StringView{data.fileBase} + fileType.extension)));
        if(!isKtx2 && is3D)
            CORRADE_COMPARE(out.str(), "Trade::BasisImporter::openData(): importing 3D texture as a 2D array texture\n");
        else
            CORRADE_COMPARE(out.str(), "");

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
            CORRADE_COMPARE(texture->type(), realType);
        }

        UnsignedInt dimensions;
        switch(realType) {
            case TextureType::Texture2D:
                dimensions = 2;
                break;
            case TextureType::Texture2DArray:
            case TextureType::CubeMap:
            case TextureType::CubeMapArray:
                dimensions = 3;
                break;
            /* No 1D/3D (array) allowed */
            default: CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        }
        CORRADE_COMPARE(counts[dimensions - 1], total);
    }
}

void BasisImporterTest::rgbUncompressed() {
    auto&& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_COMPARE(importer->configuration().value<std::string>("format"),
        "RGBA8");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
        "rgb"_s + data.extension)));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<Trade::ImageData2D> image;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    /* There should be no Y-flip warning as the image is pre-flipped */
    CORRADE_COMPARE(out.str(), "");
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

void BasisImporterTest::rgbUncompressedNoFlip() {
    auto&& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
        "rgb-noflip"_s + data.extension)));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<Trade::ImageData2D> image;
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        image = importer->image2D(0);
    }
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(out.str(), "Trade::BasisImporter::image2D(): the image was not encoded Y-flipped, imported data will have wrong orientation\n");
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
    CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    CORRADE_COMPARE_WITH(Containers::arrayCast<const Color3ub>(image->pixels<Color4ub>().flipped<0>()),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 51.334f, 8.643f}));
}

void BasisImporterTest::rgbUncompressedLinear() {
    auto&& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
        "rgb-linear"_s + data.extension)));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
        "rgba"_s + data.extension)));
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
        (DebugTools::CompareImageToFile{_manager, 94.3f, 8.039f}));
}

void BasisImporterTest::rgbaUncompressedUastc() {
    auto&& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
        "rgba-uastc"_s + data.extension)));
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

    #if defined(BASISD_SUPPORT_BC7) && !BASISD_SUPPORT_BC7
    /* BC7 is YUUGE and thus defined out on Emscripten. Skip the test if that's
       the case. This assumes -DBASISD_SUPPORT_*=0 is supplied globally. */
    if(formatData.expectedFormat == CompressedPixelFormat::Bc7RGBASrgb)
        CORRADE_SKIP("This format is not compiled into Basis.");
    #endif

    const std::string pluginName = "BasisImporter" + std::string(formatData.suffix);
    setTestCaseDescription(formatData.suffix);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate(pluginName);
    CORRADE_COMPARE(importer->configuration().value<std::string>("format"),
        formatData.suffix);

    for(const auto& fileType: FileTypeData) {
        CORRADE_ITERATION(fileType.name);

        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
            std::string{formatData.fileBase} + fileType.extension)));
        CORRADE_COMPARE(importer->image2DCount(), 1);

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(image->isCompressed());
        CORRADE_COMPARE(image->compressedFormat(), formatData.expectedFormat);
        CORRADE_COMPARE(image->size(), formatData.expectedSize);
        /** @todo remove this once CompressedImage etc. tests for data size on its
            own / we're able to decode the data ourselves */
        CORRADE_COMPARE(image->data().size(), compressedBlockDataSize(formatData.expectedFormat)*((image->size() + compressedBlockSize(formatData.expectedFormat).xy() - Vector2i{1})/compressedBlockSize(formatData.expectedFormat).xy()).product());
    }
}

void BasisImporterTest::rgba() {
    auto& formatData = FormatData[testCaseInstanceId()];

    #if defined(BASISD_SUPPORT_BC7) && !BASISD_SUPPORT_BC7
    /* BC7 is YUUGE and thus defined out on Emscripten. Skip the test if that's
       the case. This assumes -DBASISD_SUPPORT_*=0 is supplied globally. */
    if(formatData.expectedFormat == CompressedPixelFormat::Bc7RGBASrgb)
        CORRADE_SKIP("This format is not compiled into Basis.");
    #endif

    const std::string pluginName = "BasisImporter" + std::string(formatData.suffix);
    setTestCaseDescription(formatData.suffix);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate(pluginName);
    CORRADE_COMPARE(importer->configuration().value<std::string>("format"),
        formatData.suffix);

    for(const auto& fileType: FileTypeData) {
        CORRADE_ITERATION(fileType.name);

        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
            Containers::String{formatData.fileBaseAlpha} + fileType.extension)));
        CORRADE_COMPARE(importer->image2DCount(), 1);

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(image->isCompressed());
        CORRADE_COMPARE(image->compressedFormat(), formatData.expectedFormat);
        CORRADE_COMPARE(image->size(), formatData.expectedSize);
        /** @todo remove this once CompressedImage etc. tests for data size on its
            own / we're able to decode the data ourselves */
        CORRADE_COMPARE(image->data().size(), compressedBlockDataSize(formatData.expectedFormat)*((image->size() + compressedBlockSize(formatData.expectedFormat).xy() - Vector2i{1})/compressedBlockSize(formatData.expectedFormat).xy()).product());
    }
}

void BasisImporterTest::linear() {
    auto& formatData = FormatData[testCaseInstanceId()];

    /* Test linear formats, sRGB was tested in rgb() */

    #if defined(BASISD_SUPPORT_BC7) && !BASISD_SUPPORT_BC7
    /* BC7 is YUUGE and thus defined out on Emscripten. Skip the test if that's
       the case. This assumes -DBASISD_SUPPORT_*=0 is supplied globally. */
    if(formatData.expectedLinearFormat == CompressedPixelFormat::Bc7RGBAUnorm)
        CORRADE_SKIP("This format is not compiled into Basis.");
    #endif

    const std::string pluginName = "BasisImporter" + std::string(formatData.suffix);
    setTestCaseDescription(formatData.suffix);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate(pluginName);
    CORRADE_COMPARE(importer->configuration().value<std::string>("format"),
        formatData.suffix);

    for(const auto& fileType: FileTypeData) {
        CORRADE_ITERATION(fileType.name);

        CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR,
            std::string{formatData.fileBaseLinear} + fileType.extension)));
        CORRADE_COMPARE(importer->image2DCount(), 1);

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_VERIFY(image->isCompressed());
        CORRADE_COMPARE(image->compressedFormat(), formatData.expectedLinearFormat);
        CORRADE_COMPARE(image->size(), formatData.expectedSize);
        /** @todo remove this once CompressedImage etc. tests for data size on its
            own / we're able to decode the data ourselves */
        CORRADE_COMPARE(image->data().size(), compressedBlockDataSize(formatData.expectedLinearFormat)*((image->size() + compressedBlockSize(formatData.expectedLinearFormat).xy() - Vector2i{1})/compressedBlockSize(formatData.expectedLinearFormat).xy()).product());
    }
}

void BasisImporterTest::array2D() {
    auto& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-array"_s + data.extension)));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-array-mips"_s + data.extension)));

    Containers::Optional<Trade::ImageData3D> levels[3];

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), Containers::arraySize(levels));

    for(std::size_t i = 0; i != Containers::arraySize(levels); ++i) {
        CORRADE_ITERATION(i);
        levels[i] = importer->image3D(0, i);
        CORRADE_VERIFY(levels[i]);

        CORRADE_VERIFY(!levels[i]->isCompressed());
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-video"_s + data.extension)));

    Containers::Optional<Trade::ImageData2D> frames[3];

    CORRADE_COMPARE(importer->image2DCount(), Containers::arraySize(frames));

    for(UnsignedInt i = 0; i != Containers::arraySize(frames); ++i) {
        frames[i] = importer->image2D(i);
        CORRADE_VERIFY(frames[i]);
        CORRADE_VERIFY(!frames[i]->isCompressed());
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
    auto& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-3d"_s + data.extension)));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
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
    auto& data = FileTypeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Data is identical to array2DMipmaps. Mip levels in basis are per 2D
       image, for 3D images they consequently don't halve in the z-dimension.
       The importer prints a warning (unless it's a KTX2 file, those don't
       indicate 3D images at all) and imports as Texture2DArray. The texture
       type is tested in texture(). */

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-3d-mips"_s + data.extension)));

    Containers::Optional<Trade::ImageData3D> levels[3];

    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), Containers::arraySize(levels));

    for(std::size_t i = 0; i != Containers::arraySize(levels); ++i) {
        CORRADE_ITERATION(i);
        levels[i] = importer->image3D(0, i);
        CORRADE_VERIFY(levels[i]);

        CORRADE_VERIFY(!levels[i]->isCompressed());
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-cubemap"_s + data.extension)));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-cubemap-array"_s + data.extension)));

    CORRADE_COMPARE(importer->image3DCount(), 1);
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
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

void BasisImporterTest::openMemory() {
    /* Same as rgbaUncompressed() except that it uses openData() & openMemory()
       instead of openFile() to test data copying on import */

    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer);
    CORRADE_COMPARE(importer->configuration().value<std::string>("format"),
        "RGBA8");
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
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb.basis")));

    /* Verify that everything is working properly with reused transcoder */
    {
        importer->configuration().setValue("format", "Etc2RGBA");

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Etc2RGBA8Srgb);
        CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
    } {
        importer->configuration().setValue("format", "Bc1RGB");

        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Bc1RGBSrgb);
        CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::BasisImporterTest)
