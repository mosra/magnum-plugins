/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
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

#include <algorithm> /* std::find() */
#include <sstream>
#include <unordered_map>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/FormatStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>

#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "MagnumPlugins/KtxImporter/KtxHeader.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct KtxImageConverterTest: TestSuite::Tester {
    explicit KtxImageConverterTest();

    void supportedFormat();
    void supportedCompressedFormat();
    void unsupportedCompressedFormat();
    void implementationSpecificFormat();
    void implementationSpecificCompressedFormat();

    void dataFormatDescriptor();
    void dataFormatDescriptorCompressed();

    /* Non-default compressed pixel storage is currently not supported.
       It's firing an internal assert, so we're not testing that. */
    void pixelStorage();

    void tooManyLevels();
    void levelWrongSize();

    void convert1D();
    void convert1DMipmaps();
    void convert1DCompressed();
    void convert1DCompressedMipmaps();

    void convert2D();
    void convert2DMipmaps();
    /* Should be enough to only test this for one type */
    void convert2DMipmapsIncomplete();
    void convert2DCompressed();
    void convert2DCompressedMipmaps();

    void convert3D();
    void convert3DMipmaps();
    void convert3DCompressed();
    void convert3DCompressedMipmaps();

    /** @todo Add tests for cube and layered (and combined) images once the
        converter supports those */

    void convertFormats();

    void pvrtcRgb();

    void configurationOrientation();
    void configurationOrientationLessDimensions();
    void configurationOrientationEmpty();
    void configurationOrientationInvalid();
    void configurationSwizzle();
    void configurationSwizzleEmpty();
    void configurationSwizzleInvalid();
    void configurationWriterName();
    void configurationWriterNameEmpty();

    void configurationEmpty();
    void configurationSorted();

    void convertTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};

    Containers::Array<char> dfdData;
    std::unordered_map<Implementation::VkFormat, Containers::ArrayView<const char>> dfdMap;
};

using namespace Containers::Literals;
using namespace Math::Literals;

/* Origin top-left-back */
const Color3ub PatternRgbData[3][3][4]{
    /* black.png */
    {{0x000000_rgb, 0x000000_rgb, 0x000000_rgb, 0x000000_rgb},
     {0x000000_rgb, 0x000000_rgb, 0x000000_rgb, 0x000000_rgb},
     {0x000000_rgb, 0x000000_rgb, 0x000000_rgb, 0x000000_rgb}},
    /* pattern.png */
    {{0x0000ff_rgb, 0x00ff00_rgb, 0x7f007f_rgb, 0x7f007f_rgb},
     {0xffffff_rgb, 0xff0000_rgb, 0x000000_rgb, 0x00ff00_rgb},
     {0xff0000_rgb, 0xffffff_rgb, 0x000000_rgb, 0x00ff00_rgb}},
    /* pattern.png */
    {{0x0000ff_rgb, 0x00ff00_rgb, 0x7f007f_rgb, 0x7f007f_rgb},
     {0xffffff_rgb, 0xff0000_rgb, 0x000000_rgb, 0x00ff00_rgb},
     {0xff0000_rgb, 0xffffff_rgb, 0x000000_rgb, 0x00ff00_rgb}}
};

/* Output of PVRTexTool with format conversion. This is PatternRgbData[2],
    but each byte extended to uint by just repeating the byte 4 times. */
constexpr UnsignedInt HalfU = 0x7f7f7f7f;
constexpr UnsignedInt FullU = 0xffffffff;
constexpr Math::Color3<UnsignedInt> PatternRgb32UIData[4*3]{
    {    0,     0, FullU}, {    0, FullU,     0}, {HalfU, 0, HalfU}, {HalfU,     0, HalfU},
    {FullU, FullU, FullU}, {FullU,     0,     0}, {    0, 0,     0}, {    0, FullU,     0},
    {FullU,     0,     0}, {FullU, FullU, FullU}, {    0, 0,     0}, {    0, FullU,     0}
};

/* Output of PVRTexTool with format conversion. This is PatternRgbData[2],
    but each byte mapped to the range 0.0 - 1.0. */
constexpr Float HalfF = 127.0f / 255.0f;
constexpr Math::Color3<Float> PatternRgb32FData[4*3]{
    {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {HalfF, 0.0f, HalfF}, {HalfF, 0.0f, HalfF},
    {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f,  0.0f,  0.0f}, {0.0f,  1.0f,  0.0f},
    {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f,  0.0f,  0.0f}, {0.0f,  1.0f,  0.0f}
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

const char* WriterToktx = "toktx v4.0.0~6 / libktx v4.0.0~5";
const char* WriterPVRTexTool = "PVRTexLib v5.1.0";

const struct {
    const char* name;
    const Vector3i size;
    const char* message;
} TooManyLevelsData[]{
    {"1D", {1, 0, 0}, "there can be only 1 levels with base image size Vector(1) but got 2"},
    {"2D", {1, 1, 0}, "there can be only 1 levels with base image size Vector(1, 1) but got 2"},
    {"3D", {1, 1, 1}, "there can be only 1 levels with base image size Vector(1, 1, 1) but got 2"}
};

const struct {
    const char* name;
    const Vector3i sizes[2];
    const char* message;
} LevelWrongSizeData[]{
    {"1D", {{4, 0, 0}, {3, 0, 0}}, "expected size Vector(2) for level 1 but got Vector(3)"},
    {"2D", {{4, 5, 0}, {2, 1, 0}}, "expected size Vector(2, 2) for level 1 but got Vector(2, 1)"},
    {"3D", {{4, 5, 3}, {2, 2, 2}}, "expected size Vector(2, 2, 1) for level 1 but got Vector(2, 2, 2)"}
};

const struct {
    const char* name;
    const char* file;
    const CompressedPixelFormat format;
    const Math::Vector<1, Int> size;
} Convert1DCompressedData[]{
    {"BC1", "1d-compressed-bc1.ktx2", CompressedPixelFormat::Bc1RGBASrgb, {4}},
    {"ETC2", "1d-compressed-etc2.ktx2", CompressedPixelFormat::Etc2RGB8Srgb, {7}}
};

const struct {
    const char* name;
    const char* file;
    const CompressedPixelFormat format;
    const Vector2i size;
} Convert2DCompressedData[]{
    {"PVRTC", "2d-compressed-pvrtc.ktx2", CompressedPixelFormat::PvrtcRGBA4bppSrgb, {8, 8}},
    {"BC1", "2d-compressed-bc1.ktx2", CompressedPixelFormat::Bc1RGBASrgb, {8, 8}},
    {"BC3", "2d-compressed-bc3.ktx2", CompressedPixelFormat::Bc3RGBASrgb, {8, 8}},
    {"ETC2", "2d-compressed-etc2.ktx2", CompressedPixelFormat::Etc2RGB8Srgb, {9, 10}},
    {"ASTC", "2d-compressed-astc.ktx2", CompressedPixelFormat::Astc12x10RGBASrgb, {9, 10}}
};

const struct {
    const char* name;
    const char* file;
    const char* orientation;
    const char* writer;
    const PixelFormat format;
    const Containers::ArrayView<const char> data;
    bool save; /** @todo dafuq, replace all with Compare::DataToFile */
} ConvertFormatsData[]{
    {"RGB32UI", "2d-rgb32.ktx2", "rd", WriterPVRTexTool, PixelFormat::RGB32UI,
        Containers::arrayCast<const char>(PatternRgb32UIData), false},
    {"RGB32F", "2d-rgbf32.ktx2", "rd", WriterPVRTexTool, PixelFormat::RGB32F,
        Containers::arrayCast<const char>(PatternRgb32FData), false},
    /* These are saved as test files for KtxImporterTest */
    {"Stencil8UI", "2d-s8.ktx2", nullptr, nullptr, PixelFormat::Stencil8UI,
        Containers::arrayCast<const char>(PatternStencil8UIData), true},
    {"Depth16Unorm", "2d-d16.ktx2", nullptr, nullptr, PixelFormat::Depth16Unorm,
        Containers::arrayCast<const char>(PatternDepth16UnormData), true},
    {"Depth24UnormStencil8UI", "2d-d24s8.ktx2", nullptr, nullptr, PixelFormat::Depth24UnormStencil8UI,
        Containers::arrayCast<const char>(PatternDepth24UnormStencil8UIData), true},
    {"Depth32FStencil8UI", "2d-d32fs8.ktx2", nullptr, nullptr, PixelFormat::Depth32FStencil8UI,
        Containers::arrayCast<const char>(PatternDepth32FStencil8UIData), true}
};

const struct {
    const char* name;
    const CompressedPixelFormat inputFormat;
    const CompressedPixelFormat outputFormat;
} PvrtcRgbData[]{
    {"2bppUnorm", CompressedPixelFormat::PvrtcRGB2bppUnorm, CompressedPixelFormat::PvrtcRGBA2bppUnorm},
    {"2bppSrgb", CompressedPixelFormat::PvrtcRGB2bppSrgb, CompressedPixelFormat::PvrtcRGBA2bppSrgb},
    {"4bppUnorm", CompressedPixelFormat::PvrtcRGB4bppUnorm, CompressedPixelFormat::PvrtcRGBA4bppUnorm},
    {"4bppSrgb", CompressedPixelFormat::PvrtcRGB4bppSrgb, CompressedPixelFormat::PvrtcRGBA4bppSrgb},
};

const struct {
    const char* name;
    const char* value;
    const char* message;
} InvalidOrientationData[]{
    {"too short", "r", "invalid orientation string, expected at least 3 characters but got r"},
    {"invalid character", "xxx", "invalid character in orientation, expected r or l but got x"},
    {"invalid order", "rid", "invalid character in orientation, expected d or u but got i"},
};

const struct {
    const char* name;
    const char* value;
    const char* message;
} InvalidSwizzleData[]{
    {"too short", "r", "invalid swizzle length, expected 4 but got 1"},
    {"invalid characters", "rxba", "invalid characters in swizzle rxba"},
    {"invalid characters", "1012", "invalid characters in swizzle 1012"}
};

Containers::Array<char> readDataFormatDescriptor(Containers::ArrayView<const char> fileData) {
    CORRADE_INTERNAL_ASSERT(fileData.size() >= sizeof(Implementation::KtxHeader));
    const Implementation::KtxHeader& header = *reinterpret_cast<const Implementation::KtxHeader*>(fileData.data());

    const UnsignedInt offset = Utility::Endianness::littleEndian(header.dfdByteOffset);
    const UnsignedInt length = Utility::Endianness::littleEndian(header.dfdByteLength);
    Containers::Array<char> data{ValueInit, length};
    Utility::copy(fileData.exceptPrefix(offset).prefix(length), data);

    return data;
}

Containers::String readKeyValueData(Containers::ArrayView<const char> fileData) {
    CORRADE_INTERNAL_ASSERT(fileData.size() >= sizeof(Implementation::KtxHeader));
    const Implementation::KtxHeader& header = *reinterpret_cast<const Implementation::KtxHeader*>(fileData.data());

    const UnsignedInt offset = Utility::Endianness::littleEndian(header.kvdByteOffset);
    const UnsignedInt length = Utility::Endianness::littleEndian(header.kvdByteLength);
    Containers::String data{ValueInit, length};
    Utility::copy(fileData.exceptPrefix(offset).prefix(length), data);

    return data;
}

KtxImageConverterTest::KtxImageConverterTest() {
    addTests({&KtxImageConverterTest::supportedFormat,
              &KtxImageConverterTest::supportedCompressedFormat,
              &KtxImageConverterTest::unsupportedCompressedFormat,
              &KtxImageConverterTest::implementationSpecificFormat,
              &KtxImageConverterTest::implementationSpecificCompressedFormat,

              &KtxImageConverterTest::dataFormatDescriptor,
              &KtxImageConverterTest::dataFormatDescriptorCompressed,

              &KtxImageConverterTest::pixelStorage});

    addInstancedTests({&KtxImageConverterTest::tooManyLevels},
        Containers::arraySize(TooManyLevelsData));

    addInstancedTests({&KtxImageConverterTest::levelWrongSize},
        Containers::arraySize(LevelWrongSizeData));

    addTests({&KtxImageConverterTest::convert1D,
              &KtxImageConverterTest::convert1DMipmaps});

    addInstancedTests({&KtxImageConverterTest::convert1DCompressed},
        Containers::arraySize(Convert1DCompressedData));

    addTests({&KtxImageConverterTest::convert1DCompressedMipmaps,
              &KtxImageConverterTest::convert2D,
              &KtxImageConverterTest::convert2DMipmaps,
              &KtxImageConverterTest::convert2DMipmapsIncomplete});

    addInstancedTests({&KtxImageConverterTest::convert2DCompressed},
        Containers::arraySize(Convert2DCompressedData));

    addTests({&KtxImageConverterTest::convert2DCompressedMipmaps,
              &KtxImageConverterTest::convert3D,
              &KtxImageConverterTest::convert3DMipmaps,
              &KtxImageConverterTest::convert3DCompressed,
              &KtxImageConverterTest::convert3DCompressedMipmaps});

    addInstancedTests({&KtxImageConverterTest::convertFormats},
        Containers::arraySize(ConvertFormatsData));

    addInstancedTests({&KtxImageConverterTest::pvrtcRgb},
        Containers::arraySize(PvrtcRgbData));

    addTests({&KtxImageConverterTest::configurationOrientation,
              &KtxImageConverterTest::configurationOrientationLessDimensions,
              &KtxImageConverterTest::configurationOrientationEmpty});

    addInstancedTests({&KtxImageConverterTest::configurationOrientationInvalid},
        Containers::arraySize(InvalidOrientationData));

    addTests({&KtxImageConverterTest::configurationSwizzle,
              &KtxImageConverterTest::configurationSwizzleEmpty});

    addInstancedTests({&KtxImageConverterTest::configurationSwizzleInvalid},
        Containers::arraySize(InvalidSwizzleData));

    addTests({&KtxImageConverterTest::configurationWriterName,
              &KtxImageConverterTest::configurationWriterNameEmpty,
              &KtxImageConverterTest::configurationEmpty,
              &KtxImageConverterTest::configurationSorted,

              &KtxImageConverterTest::convertTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef KTXIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(KTXIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Optional plugins that don't have to be here */
    #ifdef KTXIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(KTXIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif

    /* Extract VkFormat and DFD content from merged DFD file */
    dfdData = *CORRADE_INTERNAL_ASSERT_EXPRESSION(Utility::Path::read(Utility::Path::join(KTXIMAGECONVERTER_TEST_DIR, "dfd-data.bin")));
    CORRADE_INTERNAL_ASSERT(!dfdData.isEmpty());
    CORRADE_INTERNAL_ASSERT(dfdData.size()%4 == 0);
    std::size_t offset = 0;
    while(offset < dfdData.size()) {
        /* Each entry is a VkFormat, followed directly by the DFD. The first
           uint32_t of the DFD is its size. */
        const Implementation::VkFormat format = *reinterpret_cast<Implementation::VkFormat*>(dfdData.data() + offset);
        offset += sizeof(format);
        const UnsignedInt size = *reinterpret_cast<UnsignedInt*>(dfdData.data() + offset);
        CORRADE_INTERNAL_ASSERT(size > 0);
        CORRADE_INTERNAL_ASSERT(size%4 == 0);
        dfdMap.emplace(format, dfdData.exceptPrefix(offset).prefix(size));
        offset += size;
    }
    CORRADE_INTERNAL_ASSERT(offset == dfdData.size());
}

void KtxImageConverterTest::supportedFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[32]{};

    /* All the formats in PixelFormat are supported */
    /** @todo This needs to be extended when new formats are added to
        PixelFormat. In dataFormatDescriptor() as well. Ideally Magnum itself
        should provide some kind of a "pixel format count" constant. */
    constexpr PixelFormat start = PixelFormat::R8Unorm;
    constexpr PixelFormat end = PixelFormat::Depth32FStencil8UI;

    for(UnsignedInt format = UnsignedInt(start); format <= UnsignedInt(end); ++format) {
        CORRADE_ITERATION(format);
        CORRADE_INTERNAL_ASSERT(Containers::arraySize(bytes) >= pixelSize(PixelFormat(format)));
        CORRADE_VERIFY(converter->convertToData(ImageView2D{PixelFormat(format), {1, 1}, bytes}));
    }
}

const CompressedPixelFormat UnsupportedCompressedFormats[]{
    /* Vulkan has no support (core or extension) for 3D ASTC formats.
       KTX supports them, but through an unreleased extension. */
    CompressedPixelFormat::Astc3x3x3RGBAUnorm,
    CompressedPixelFormat::Astc3x3x3RGBASrgb,
    CompressedPixelFormat::Astc3x3x3RGBAF,
    CompressedPixelFormat::Astc4x3x3RGBAUnorm,
    CompressedPixelFormat::Astc4x3x3RGBASrgb,
    CompressedPixelFormat::Astc4x3x3RGBAF,
    CompressedPixelFormat::Astc4x4x3RGBAUnorm,
    CompressedPixelFormat::Astc4x4x3RGBASrgb,
    CompressedPixelFormat::Astc4x4x3RGBAF,
    CompressedPixelFormat::Astc4x4x4RGBAUnorm,
    CompressedPixelFormat::Astc4x4x4RGBASrgb,
    CompressedPixelFormat::Astc4x4x4RGBAF,
    CompressedPixelFormat::Astc5x4x4RGBAUnorm,
    CompressedPixelFormat::Astc5x4x4RGBASrgb,
    CompressedPixelFormat::Astc5x4x4RGBAF,
    CompressedPixelFormat::Astc5x5x4RGBAUnorm,
    CompressedPixelFormat::Astc5x5x4RGBASrgb,
    CompressedPixelFormat::Astc5x5x4RGBAF,
    CompressedPixelFormat::Astc5x5x5RGBAUnorm,
    CompressedPixelFormat::Astc5x5x5RGBASrgb,
    CompressedPixelFormat::Astc5x5x5RGBAF,
    CompressedPixelFormat::Astc6x5x5RGBAUnorm,
    CompressedPixelFormat::Astc6x5x5RGBASrgb,
    CompressedPixelFormat::Astc6x5x5RGBAF,
    CompressedPixelFormat::Astc6x6x5RGBAUnorm,
    CompressedPixelFormat::Astc6x6x5RGBASrgb,
    CompressedPixelFormat::Astc6x6x5RGBAF,
    CompressedPixelFormat::Astc6x6x6RGBAUnorm,
    CompressedPixelFormat::Astc6x6x6RGBASrgb,
    CompressedPixelFormat::Astc6x6x6RGBAF
};

void KtxImageConverterTest::supportedCompressedFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[32]{};

    /** @todo This needs to be extended when new formats are added to
        CompressedPixelFormat. In dataFormatDescriptorCompressed() as well.
        Ideally Magnum itself should provide some kind of a "pixel format
        count" constant. */
    constexpr CompressedPixelFormat start = CompressedPixelFormat::Bc1RGBUnorm;
    constexpr CompressedPixelFormat end = CompressedPixelFormat::PvrtcRGBA4bppSrgb;

    for(UnsignedInt format = UnsignedInt(start); format <= UnsignedInt(end); ++format) {
        if(std::find(std::begin(UnsupportedCompressedFormats), std::end(UnsupportedCompressedFormats),
            CompressedPixelFormat(format)) == std::end(UnsupportedCompressedFormats))
        {
            CORRADE_ITERATION(format);
            CORRADE_INTERNAL_ASSERT(Containers::arraySize(bytes) >= compressedBlockDataSize(CompressedPixelFormat(format)));
            CORRADE_VERIFY(converter->convertToData(CompressedImageView2D{CompressedPixelFormat(format), {1, 1}, bytes}));
        }
    }
}

void KtxImageConverterTest::unsupportedCompressedFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[32]{};

    for(CompressedPixelFormat format: UnsupportedCompressedFormats) {
        CORRADE_ITERATION(format);
        CORRADE_INTERNAL_ASSERT(Containers::arraySize(bytes) >= compressedBlockDataSize(CompressedPixelFormat(format)));

        CORRADE_VERIFY(!converter->convertToData(CompressedImageView2D{format, {1, 1}, bytes}));

        /* Not testing the output message so that it shows up as a friendly
           nagging reminder to add support for these formats */
    }
}

void KtxImageConverterTest::implementationSpecificFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[1]{};

    std::ostringstream out;
    Error redirectError{&out};

    PixelStorage storage;
    storage.setAlignment(1);
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{storage, 0, 0, 1, {1, 1}, bytes}));
    CORRADE_COMPARE(out.str(),
        "Trade::KtxImageConverter::convertToData(): implementation-specific formats are not supported\n");
}

void KtxImageConverterTest::implementationSpecificCompressedFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[1]{};

    std::ostringstream out;
    Error redirectError{&out};

    CompressedPixelStorage storage;
    CORRADE_VERIFY(!converter->convertToData(CompressedImageView2D{storage, 0, {1, 1}, bytes}));
    CORRADE_COMPARE(out.str(),
        "Trade::KtxImageConverter::convertToData(): implementation-specific formats are not supported\n");
}

void KtxImageConverterTest::dataFormatDescriptor() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[32]{};

    constexpr PixelFormat start = PixelFormat::R8Unorm;
    constexpr PixelFormat end = PixelFormat::Depth32FStencil8UI;

    for(UnsignedInt format = UnsignedInt(start); format <= UnsignedInt(end); ++format) {
        CORRADE_ITERATION(format);
        CORRADE_INTERNAL_ASSERT(Containers::arraySize(bytes) >= pixelSize(PixelFormat(format)));
        Containers::Optional<Containers::Array<char>> output = converter->convertToData(ImageView2D{PixelFormat(format), {1, 1}, bytes});
        CORRADE_VERIFY(output);

        const Implementation::KtxHeader& header = *reinterpret_cast<const Implementation::KtxHeader*>(output->data());
        const Implementation::VkFormat vkFormat = Utility::Endianness::littleEndian(header.vkFormat);

        Containers::Array<char> dfd = readDataFormatDescriptor(*output);
        CORRADE_COMPARE(dfdMap.count(vkFormat), 1);
        CORRADE_COMPARE_AS(dfd, dfdMap[vkFormat], TestSuite::Compare::Container);
    }
}

void KtxImageConverterTest::dataFormatDescriptorCompressed() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[32]{};

    constexpr CompressedPixelFormat start = CompressedPixelFormat::Bc1RGBUnorm;
    constexpr CompressedPixelFormat end = CompressedPixelFormat::PvrtcRGBA4bppSrgb;

    for(UnsignedInt format = UnsignedInt(start); format <= UnsignedInt(end); ++format) {
        if(std::find(std::begin(UnsupportedCompressedFormats), std::end(UnsupportedCompressedFormats),
            CompressedPixelFormat(format)) == std::end(UnsupportedCompressedFormats))
        {
            CORRADE_ITERATION(format);
            CORRADE_INTERNAL_ASSERT(Containers::arraySize(bytes) >= compressedBlockDataSize(CompressedPixelFormat(format)));
            Containers::Optional<Containers::Array<char>> output = converter->convertToData(CompressedImageView2D{CompressedPixelFormat(format), {1, 1}, bytes});
            CORRADE_VERIFY(output);

            const Implementation::KtxHeader& header = *reinterpret_cast<const Implementation::KtxHeader*>(output->data());
            const Implementation::VkFormat vkFormat = Utility::Endianness::littleEndian(header.vkFormat);

            Containers::Array<char> dfd = readDataFormatDescriptor(*output);
            CORRADE_COMPARE(dfdMap.count(vkFormat), 1);
            CORRADE_COMPARE_AS(dfd, dfdMap[vkFormat], TestSuite::Compare::Container);
        }
    }
}

void KtxImageConverterTest::pixelStorage() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    constexpr UnsignedByte bytes[4*3]{
        0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 10, 11
    };

    PixelStorage storage;
    storage.setAlignment(4);
    storage.setSkip({1, 1, 0});

    const ImageView2D inputImage{storage, PixelFormat::R8UI, {2, 2}, Containers::arrayView(bytes)};
    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImage);
    CORRADE_VERIFY(output);

    if(_importerManager.loadState("KtxImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openData(*output));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE_AS(image->data(), Containers::arrayView<char>({5, 6, 9, 10}), TestSuite::Compare::Container);
}

void KtxImageConverterTest::tooManyLevels() {
    auto&& data = TooManyLevelsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[4]{};
    CORRADE_INTERNAL_ASSERT(Math::max(Vector3ui{data.size}, 1u).product()*4u <= Containers::arraySize(bytes));

    const UnsignedInt dimensions = Math::min(Vector3ui{data.size}, 1u).sum();

    std::ostringstream out;
    Error redirectError{&out};
    if(dimensions == 1) {
        CORRADE_VERIFY(!converter->convertToData({
            ImageView1D{PixelFormat::RGBA8Unorm, data.size.x(), bytes},
            ImageView1D{PixelFormat::RGBA8Unorm, data.size.x(), bytes}
        }));
    } else if(dimensions == 2) {
        CORRADE_VERIFY(!converter->convertToData({
            ImageView2D{PixelFormat::RGBA8Unorm, data.size.xy(), bytes},
            ImageView2D{PixelFormat::RGBA8Unorm, data.size.xy(), bytes}
        }));
    } else if(dimensions == 3) {
        CORRADE_VERIFY(!converter->convertToData({
            ImageView3D{PixelFormat::RGBA8Unorm, data.size, bytes},
            ImageView3D{PixelFormat::RGBA8Unorm, data.size, bytes}
        }));
    }

    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::KtxImageConverter::convertToData(): {}\n", data.message));
}

void KtxImageConverterTest::levelWrongSize() {
    auto&& data = LevelWrongSizeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[256]{};
    CORRADE_INTERNAL_ASSERT(Math::max(Vector3ui{data.sizes[0]}, 1u).product()*4u <= Containers::arraySize(bytes));

    const UnsignedInt dimensions = Math::min(Vector3ui{data.sizes[0]}, 1u).sum();

    std::ostringstream out;
    Error redirectError{&out};
    if(dimensions == 1) {
        CORRADE_VERIFY(!converter->convertToData({
            ImageView1D{PixelFormat::RGBA8Unorm, data.sizes[0].x(), bytes},
            ImageView1D{PixelFormat::RGBA8Unorm, data.sizes[1].x(), bytes}
        }));
    } else if(dimensions == 2) {
        CORRADE_VERIFY(!converter->convertToData({
            ImageView2D{PixelFormat::RGBA8Unorm, data.sizes[0].xy(), bytes},
            ImageView2D{PixelFormat::RGBA8Unorm, data.sizes[1].xy(), bytes}
        }));
    } else if(dimensions == 3) {
        CORRADE_VERIFY(!converter->convertToData({
            ImageView3D{PixelFormat::RGBA8Unorm, data.sizes[0], bytes},
            ImageView3D{PixelFormat::RGBA8Unorm, data.sizes[1], bytes}
        }));
    }

    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::KtxImageConverter::convertToData(): {}\n", data.message));
}

void KtxImageConverterTest::convert1D() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    /* toktx writes no orientation for 1D files */
    converter->configuration().removeValue("orientation");
    converter->configuration().setValue("writerName", WriterToktx);

    const Color3ub data[4]{
        0xff0000_rgb, 0xffffff_rgb, 0x000000_rgb, 0x007f7f_rgb
    };
    PixelStorage storage;
    storage.setAlignment(1);
    const ImageView1D inputImage{storage, PixelFormat::RGB8Srgb, {4}, data};
    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImage);
    CORRADE_VERIFY(output);

    /* Compare against 'ground truth' output generated by toktx/PVRTexTool */
    /** @todo Compare::DataToFile */
    Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "1d.ktx2"));
    CORRADE_VERIFY(expected);
    CORRADE_COMPARE_AS(*output, *expected, TestSuite::Compare::Container);
}

void KtxImageConverterTest::convert1DMipmaps() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    converter->configuration().removeValue("orientation");
    converter->configuration().setValue("writerName", WriterToktx);

    constexpr Math::Vector<1, Int> size{4};
    const Color3ub mip0[4]{0xff0000_rgb, 0xffffff_rgb, 0x000000_rgb, 0x007f7f_rgb};
    const Color3ub mip1[2]{0xffffff_rgb, 0x007f7f_rgb};
    const Color3ub mip2[1]{0x000000_rgb};

    PixelStorage storage;
    storage.setAlignment(1);
    const ImageView1D inputImages[3]{
        ImageView1D{storage, PixelFormat::RGB8Srgb, Math::max(size >> 0, 1), mip0},
        ImageView1D{storage, PixelFormat::RGB8Srgb, Math::max(size >> 1, 1), mip1},
        ImageView1D{storage, PixelFormat::RGB8Srgb, Math::max(size >> 2, 1), mip2}
    };

    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImages);
    CORRADE_VERIFY(output);

    /** @todo Compare::DataToFile */
    Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "1d-mipmaps.ktx2"));
    CORRADE_VERIFY(expected);
    CORRADE_COMPARE_AS(*output, *expected, TestSuite::Compare::Container);
}

void KtxImageConverterTest::convert1DCompressed() {
    auto&& data = Convert1DCompressedData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    converter->configuration().setValue("orientation", "r");
    converter->configuration().setValue("writerName", WriterPVRTexTool);

    Containers::Optional<Containers::Array<char>> blockData = Utility::Path::read(
        Utility::Path::join(KTXIMPORTER_TEST_DIR, Utility::Path::splitExtension(data.file).first() + ".bin"));
    CORRADE_VERIFY(blockData);
    const CompressedImageView1D inputImage{data.format, data.size, *blockData};

    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImage);
    CORRADE_VERIFY(output);

    /** @todo Compare::DataToFile */
    Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file));
    CORRADE_VERIFY(expected);
    CORRADE_COMPARE_AS(*output, *expected, TestSuite::Compare::Container);
}

void KtxImageConverterTest::convert1DCompressedMipmaps() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    converter->configuration().setValue("orientation", "r");
    converter->configuration().setValue("writerName", WriterPVRTexTool);

    constexpr Math::Vector<1, Int> size{7};
    Containers::Optional<Containers::Array<char>> mip0 = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "1d-compressed-mipmaps-mip0.bin"));
    Containers::Optional<Containers::Array<char>> mip1 = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "1d-compressed-mipmaps-mip1.bin"));
    Containers::Optional<Containers::Array<char>> mip2 = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "1d-compressed-mipmaps-mip2.bin"));
    CORRADE_VERIFY(mip0);
    CORRADE_VERIFY(mip1);
    CORRADE_VERIFY(mip2);

    const CompressedImageView1D inputImages[3]{
        CompressedImageView1D{CompressedPixelFormat::Etc2RGB8Srgb, Math::max(size >> 0, 1), *mip0},
        CompressedImageView1D{CompressedPixelFormat::Etc2RGB8Srgb, Math::max(size >> 1, 1), *mip1},
        CompressedImageView1D{CompressedPixelFormat::Etc2RGB8Srgb, Math::max(size >> 2, 1), *mip2}
    };

    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImages);
    CORRADE_VERIFY(output);

    /** @todo Compare::DataToFile */
    Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "1d-compressed-mipmaps.ktx2"));
    CORRADE_VERIFY(expected);
    CORRADE_COMPARE_AS(*output, *expected, TestSuite::Compare::Container);
}

void KtxImageConverterTest::convert2D() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    converter->configuration().setValue("orientation", "rd");
    converter->configuration().setValue("writerName", WriterToktx);

    PixelStorage storage;
    storage.setAlignment(1);
    const ImageView2D inputImage{storage, PixelFormat::RGB8Srgb, {4, 3}, PatternRgbData[Containers::arraySize(PatternRgbData) - 1]};
    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImage);
    CORRADE_VERIFY(output);

    /** @todo Compare::DataToFile */
    Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-rgb.ktx2"));
    CORRADE_VERIFY(expected);
    CORRADE_COMPARE_AS(*output, *expected, TestSuite::Compare::Container);
}

void KtxImageConverterTest::convert2DMipmaps() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    converter->configuration().setValue("orientation", "rd");
    converter->configuration().setValue("writerName", WriterToktx);

    constexpr Vector2i size{4, 3};
    const auto mip0 = Containers::arrayCast<const Color3ub>(Containers::arrayView(
        PatternRgbData[Containers::arraySize(PatternRgbData) - 1]));
    const Color3ub mip1[2]{0xffffff_rgb, 0x007f7f_rgb};
    const Color3ub mip2[1]{0x000000_rgb};

    PixelStorage storage;
    storage.setAlignment(1);
    const ImageView2D inputImages[3]{
        ImageView2D{storage, PixelFormat::RGB8Srgb, Math::max(size >> 0, 1), mip0},
        ImageView2D{storage, PixelFormat::RGB8Srgb, Math::max(size >> 1, 1), mip1},
        ImageView2D{storage, PixelFormat::RGB8Srgb, Math::max(size >> 2, 1), mip2}
    };

    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImages);
    CORRADE_VERIFY(output);

    /** @todo Compare::DataToFile */
    Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-mipmaps.ktx2"));
    CORRADE_VERIFY(expected);
    CORRADE_COMPARE_AS(*output, *expected, TestSuite::Compare::Container);
}

void KtxImageConverterTest::convert2DMipmapsIncomplete() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    converter->configuration().setValue("orientation", "rd");
    converter->configuration().setValue("writerName", WriterToktx);

    constexpr Vector2i size{4, 3};
    const auto mip0 = Containers::arrayCast<const Color3ub>(Containers::arrayView(
        PatternRgbData[Containers::arraySize(PatternRgbData) - 1]));
    const Color3ub mip1[2]{0xffffff_rgb, 0x007f7f_rgb};

    PixelStorage storage;
    storage.setAlignment(1);
    const ImageView2D inputImages[2]{
        ImageView2D{storage, PixelFormat::RGB8Srgb, Math::max(size >> 0, 1), mip0},
        ImageView2D{storage, PixelFormat::RGB8Srgb, Math::max(size >> 1, 1), mip1}
    };

    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImages);
    CORRADE_VERIFY(output);

    /** @todo Compare::DataToFile */
    Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-mipmaps-incomplete.ktx2"));
    CORRADE_VERIFY(expected);
    CORRADE_COMPARE_AS(*output, *expected, TestSuite::Compare::Container);
}

void KtxImageConverterTest::convert2DCompressed() {
    auto&& data = Convert2DCompressedData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    converter->configuration().setValue("orientation", "rd");
    converter->configuration().setValue("writerName", WriterPVRTexTool);

    Containers::Optional<Containers::Array<char>> blockData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, Utility::Path::splitExtension(data.file).first() + ".bin"));
    CORRADE_VERIFY(blockData);
    const CompressedImageView2D inputImage{data.format, data.size, *blockData};

    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImage);
    CORRADE_VERIFY(output);

    /** @todo Compare::DataToFile */
    Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file));
    CORRADE_VERIFY(expected);
    CORRADE_COMPARE_AS(*output, *expected, TestSuite::Compare::Container);
}

void KtxImageConverterTest::convert2DCompressedMipmaps() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    converter->configuration().setValue("orientation", "rd");
    converter->configuration().setValue("writerName", WriterPVRTexTool);

    constexpr Vector2i size{9, 10};
    Containers::Optional<Containers::Array<char>> mip0 = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-compressed-mipmaps-mip0.bin"));
    Containers::Optional<Containers::Array<char>> mip1 = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-compressed-mipmaps-mip1.bin"));
    Containers::Optional<Containers::Array<char>> mip2 = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-compressed-mipmaps-mip2.bin"));
    Containers::Optional<Containers::Array<char>> mip3 = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-compressed-mipmaps-mip3.bin"));
    CORRADE_VERIFY(mip0);
    CORRADE_VERIFY(mip1);
    CORRADE_VERIFY(mip2);
    CORRADE_VERIFY(mip3);

    const CompressedImageView2D inputImages[4]{
        CompressedImageView2D{CompressedPixelFormat::Etc2RGB8Srgb, Math::max(size >> 0, 1), *mip0},
        CompressedImageView2D{CompressedPixelFormat::Etc2RGB8Srgb, Math::max(size >> 1, 1), *mip1},
        CompressedImageView2D{CompressedPixelFormat::Etc2RGB8Srgb, Math::max(size >> 2, 1), *mip2},
        CompressedImageView2D{CompressedPixelFormat::Etc2RGB8Srgb, Math::max(size >> 3, 1), *mip3}
    };

    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImages);
    CORRADE_VERIFY(output);

    /** @todo Compare::DataToFile */
    Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "2d-compressed-mipmaps.ktx2"));
    CORRADE_VERIFY(expected);
    CORRADE_COMPARE_AS(*output, *expected, TestSuite::Compare::Container);
}

void KtxImageConverterTest::convert3D() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    converter->configuration().setValue("orientation", "rdi");
    converter->configuration().setValue("writerName", WriterPVRTexTool);

    PixelStorage storage;
    storage.setAlignment(1);
    const ImageView3D inputImage{storage, PixelFormat::RGB8Srgb, {4, 3, 3}, PatternRgbData};
    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImage);
    CORRADE_VERIFY(output);

    /** @todo Compare::DataToFile */
    Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d.ktx2"));
    CORRADE_VERIFY(expected);
    CORRADE_COMPARE_AS(*output, *expected, TestSuite::Compare::Container);
}

void KtxImageConverterTest::convert3DMipmaps() {
    /* Neither toktx nor PVRTexTool can create mipmapped 3D textures. We use
       the converter to create our own test file for the importer and the
       converter ground truth. At the very least it catches unexpected changes.
       Save it by running the test with:
       --save-diagnostic [path/to/KtxImporter/Test] */

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    converter->configuration().setValue("orientation", "rdi");

    const Vector3i size{4, 3, 3};
    const auto mip0 = Containers::arrayCast<const Color3ub>(Containers::arrayView(PatternRgbData));
    const Color3ub mip1[2]{0xffffff_rgb, 0x007f7f_rgb};
    const Color3ub mip2[1]{0x000000_rgb};

    PixelStorage storage;
    storage.setAlignment(1);
    const ImageView3D inputImages[3]{
        ImageView3D{storage, PixelFormat::RGB8Srgb, Math::max(size >> 0, 1), mip0},
        ImageView3D{storage, PixelFormat::RGB8Srgb, Math::max(size >> 1, 1), mip1},
        ImageView3D{storage, PixelFormat::RGB8Srgb, Math::max(size >> 2, 1), mip2}
    };

    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImages);
    CORRADE_VERIFY(output);

    CORRADE_COMPARE_AS(Containers::StringView{*output},
        Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d-mipmaps.ktx2"),
        TestSuite::Compare::StringToFile);
}

void KtxImageConverterTest::convert3DCompressed() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    converter->configuration().setValue("orientation", "rdi");
    converter->configuration().setValue("writerName", WriterPVRTexTool);

    Containers::Optional<Containers::Array<char>> blockData = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d-compressed.bin"));
    CORRADE_VERIFY(blockData);
    const CompressedImageView3D inputImage{CompressedPixelFormat::Etc2RGB8Srgb, {9, 10, 3}, *blockData};

    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImage);
    CORRADE_VERIFY(output);

    /** @todo Compare::DataToFile */
    Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d-compressed.ktx2"));
    CORRADE_VERIFY(expected);
    CORRADE_COMPARE_AS(*output, *expected, TestSuite::Compare::Container);
}

void KtxImageConverterTest::convert3DCompressedMipmaps() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    converter->configuration().setValue("orientation", "rdi");

    /* Same as convert3DMipmaps, we generate this file here because none of the
       tools can do it. The other compressed .bin data is extracted from files
       created by toktx/PVRTexTool. In this case we handishly created data from
       existing 2D ETC2 data. Oh well, better than nothing until there's a
       better way to generate these images. */

    constexpr Vector3i size{9, 10, 5};
    Containers::Optional<Containers::Array<char>> mip0 = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d-compressed-mipmaps-mip0.bin"));
    Containers::Optional<Containers::Array<char>> mip1 = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d-compressed-mipmaps-mip1.bin"));
    Containers::Optional<Containers::Array<char>> mip2 = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d-compressed-mipmaps-mip2.bin"));
    Containers::Optional<Containers::Array<char>> mip3 = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d-compressed-mipmaps-mip3.bin"));
    CORRADE_VERIFY(mip0);
    CORRADE_VERIFY(mip1);
    CORRADE_VERIFY(mip2);
    CORRADE_VERIFY(mip3);

    const CompressedImageView3D inputImages[4]{
        CompressedImageView3D{CompressedPixelFormat::Etc2RGB8Srgb, Math::max(size >> 0, 1), *mip0},
        CompressedImageView3D{CompressedPixelFormat::Etc2RGB8Srgb, Math::max(size >> 1, 1), *mip1},
        CompressedImageView3D{CompressedPixelFormat::Etc2RGB8Srgb, Math::max(size >> 2, 1), *mip2},
        CompressedImageView3D{CompressedPixelFormat::Etc2RGB8Srgb, Math::max(size >> 3, 1), *mip3}
    };

    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImages);
    CORRADE_VERIFY(output);

    /** @todo Compare::DataToFile */
    Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d-compressed-mipmaps.ktx2"));
    CORRADE_COMPARE_AS(Containers::StringView{*output},
        Utility::Path::join(KTXIMPORTER_TEST_DIR, "3d-compressed-mipmaps.ktx2"),
        TestSuite::Compare::StringToFile);
}

void KtxImageConverterTest::convertFormats() {
    auto&& data = ConvertFormatsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    if(data.orientation)
        converter->configuration().setValue("orientation", data.orientation);
    if(data.writer)
        converter->configuration().setValue("writerName", data.writer);

    PixelStorage storage;
    storage.setAlignment(1);
    const ImageView2D inputImage{storage, data.format, {4, 3}, data.data};
    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImage);
    CORRADE_VERIFY(output);

    if(data.save) {
        CORRADE_COMPARE_AS(Containers::StringView{*output},
            Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file),
            TestSuite::Compare::StringToFile);
    } else {
        /** @todo Compare::DataToFile */
        Containers::Optional<Containers::Array<char>> expected = Utility::Path::read(Utility::Path::join(KTXIMPORTER_TEST_DIR, data.file));
        CORRADE_COMPARE_AS(*output, *expected, TestSuite::Compare::Container);
    }
}

void KtxImageConverterTest::pvrtcRgb() {
    auto&& data = PvrtcRgbData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[16]{};
    const UnsignedInt dataSize = compressedBlockDataSize(data.inputFormat);
    const Vector2i imageSize = {2, 2};
    CORRADE_INTERNAL_ASSERT(Containers::arraySize(bytes) >= dataSize);
    CORRADE_INTERNAL_ASSERT((Vector3i{imageSize, 1}) <= compressedBlockSize(data.inputFormat));

    const CompressedImageView2D inputImage{data.inputFormat, imageSize, Containers::arrayView(bytes).prefix(dataSize)};
    Containers::Optional<Containers::Array<char>> output = converter->convertToData(inputImage);
    CORRADE_VERIFY(output);

    if(_importerManager.loadState("KtxImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("KtxImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("KtxImporter");
    CORRADE_VERIFY(importer->openData(*output));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->compressedFormat(), data.outputFormat);
    CORRADE_COMPARE_AS(image->data(), inputImage.data(), TestSuite::Compare::Container);
}

void KtxImageConverterTest::configurationOrientation() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    /* Default value */
    CORRADE_COMPARE(converter->configuration().value("orientation"), "ruo");
    CORRADE_VERIFY(converter->configuration().setValue("orientation", "ldo"));

    const UnsignedByte bytes[4]{};
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView3D{PixelFormat::RGBA8Unorm, {1, 1, 1}, bytes});
    CORRADE_VERIFY(data);

    Containers::String keyValueData = readKeyValueData(*data);
    CORRADE_VERIFY(keyValueData.contains("KTXorientation\0ldo\0"_s));
}

void KtxImageConverterTest::configurationOrientationLessDimensions() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    /* Orientation string is shortened to the number of dimensions, extra characters are ignored */
    CORRADE_VERIFY(converter->configuration().setValue("orientation", "rdxxx"));

    const UnsignedByte bytes[4]{};
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, bytes});
    CORRADE_VERIFY(data);

    Containers::String keyValueData = readKeyValueData(*data);
    CORRADE_VERIFY(keyValueData.contains("KTXorientation\0rd\0"_s));
}

void KtxImageConverterTest::configurationOrientationEmpty() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    CORRADE_VERIFY(converter->configuration().setValue("orientation", ""));

    const UnsignedByte bytes[4]{};
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, bytes});
    CORRADE_VERIFY(data);

    /* Empty orientation isn't written to key/value data at all */
    Containers::String keyValueData = readKeyValueData(*data);
    CORRADE_VERIFY(!keyValueData.contains("KTXorientation"_s));
}

void KtxImageConverterTest::configurationOrientationInvalid() {
    auto&& data = InvalidOrientationData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    CORRADE_VERIFY(converter->configuration().setValue("orientation", data.value));

    std::ostringstream out;
    Error redirectError{&out};

    const UnsignedByte bytes[4]{};
    CORRADE_VERIFY(!converter->convertToData(ImageView3D{PixelFormat::RGBA8Unorm, {1, 1, 1}, bytes}));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::KtxImageConverter::convertToData(): {}\n", data.message));
}

void KtxImageConverterTest::configurationSwizzle() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    /* Default value */
    CORRADE_COMPARE(converter->configuration().value("swizzle"), "");
    CORRADE_VERIFY(converter->configuration().setValue("swizzle", "rgba"));

    const UnsignedByte bytes[4]{};
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, bytes});
    CORRADE_VERIFY(data);

    Containers::String keyValueData = readKeyValueData(*data);
    CORRADE_VERIFY(keyValueData.contains("KTXswizzle\0rgba\0"_s));
}

void KtxImageConverterTest::configurationSwizzleEmpty() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    /* Swizzle is empty by default, tested in configurationSwizzle() */

    const UnsignedByte bytes[4]{};
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, bytes});
    CORRADE_VERIFY(data);

    /* Empty swizzle isn't written to key/value data at all */
    Containers::String keyValueData = readKeyValueData(*data);
    CORRADE_VERIFY(!keyValueData.contains("KTXswizzle"_s));
}

void KtxImageConverterTest::configurationSwizzleInvalid() {
    auto&& data = InvalidSwizzleData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    CORRADE_VERIFY(converter->configuration().setValue("swizzle", data.value));

    std::ostringstream out;
    Error redirectError{&out};

    const UnsignedByte bytes[4]{};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, bytes}));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::KtxImageConverter::convertToData(): {}\n", data.message));
}

void KtxImageConverterTest::configurationWriterName() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    /* Default value */
    CORRADE_COMPARE(converter->configuration().value("writerName"), "Magnum KtxImageConverter");
    CORRADE_VERIFY(converter->configuration().setValue("writerName", "KtxImageConverterTest&$%1234@\x02\n\r\t\x15!"));

    const UnsignedByte bytes[4]{};
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, bytes});
    CORRADE_VERIFY(data);

    /* Writer doesn't have to be null-terminated, don't test for \0 */
    Containers::String keyValueData = readKeyValueData(*data);
    CORRADE_VERIFY(keyValueData.contains("KTXwriter\0KtxImageConverterTest&$%1234@\x02\n\r\t\x15!"_s));
}

void KtxImageConverterTest::configurationWriterNameEmpty() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    CORRADE_VERIFY(converter->configuration().setValue("writerName", ""));

    const UnsignedByte bytes[4]{};
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, bytes});
    CORRADE_VERIFY(data);

    /* Empty writer name isn't written to key/value data at all */
    Containers::String keyValueData = readKeyValueData(*data);
    CORRADE_VERIFY(!keyValueData.contains("KTXwriter"_s));
}

void KtxImageConverterTest::configurationEmpty() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    CORRADE_VERIFY(converter->configuration().removeValue("writerName"));
    CORRADE_VERIFY(converter->configuration().removeValue("swizzle"));
    CORRADE_VERIFY(converter->configuration().removeValue("orientation"));

    const UnsignedByte bytes[4]{};
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, bytes});
    CORRADE_VERIFY(data);

    /* Key/value data should not be written if it only contains empty values */

    const Implementation::KtxHeader& header = *reinterpret_cast<const Implementation::KtxHeader*>(data->data());
    CORRADE_COMPARE(header.kvdByteOffset, 0);
    CORRADE_COMPARE(header.kvdByteLength, 0);
}

void KtxImageConverterTest::configurationSorted() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");
    CORRADE_VERIFY(converter->configuration().setValue("writerName", "x"));
    CORRADE_VERIFY(converter->configuration().setValue("swizzle", "barg"));
    CORRADE_VERIFY(converter->configuration().setValue("orientation", "rd"));

    const UnsignedByte bytes[4]{};
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, bytes});
    CORRADE_VERIFY(data);

    Containers::String keyValueData = readKeyValueData(*data);
    Containers::StringView writerOffset = keyValueData.find("KTXwriter"_s);
    Containers::StringView swizzleOffset = keyValueData.find("KTXswizzle"_s);
    Containers::StringView orientationOffset = keyValueData.find("KTXorientation"_s);

    CORRADE_VERIFY(!writerOffset.isEmpty());
    CORRADE_VERIFY(!swizzleOffset.isEmpty());
    CORRADE_VERIFY(!orientationOffset.isEmpty());

    /* Entries are sorted alphabetically */
    CORRADE_VERIFY(orientationOffset.begin() < swizzleOffset.begin());
    CORRADE_VERIFY(swizzleOffset.begin() < writerOffset.begin());
}

void KtxImageConverterTest::convertTwice() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("KtxImageConverter");

    const UnsignedByte bytes[4]{};
    Containers::Optional<Containers::Array<char>> data1 = converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, bytes});
    CORRADE_VERIFY(data1);
    Containers::Optional<Containers::Array<char>> data2 = converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, bytes});
    CORRADE_VERIFY(data2);

    /* Shouldn't crash, output should be identical */
    CORRADE_COMPARE_AS(*data1, *data2, TestSuite::Compare::Container);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::KtxImageConverterTest)
