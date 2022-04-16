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
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <sstream>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Image.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Swizzle.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/TextureData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct BasisImageConverterTest: TestSuite::Tester {
    explicit BasisImageConverterTest();

    void wrongFormat();
    void unknownOutputFormatData();
    void unknownOutputFormatFile();
    void invalidSwizzle();
    void tooManyLevels();
    void levelWrongSize();
    void processError();

    void configPerceptual();
    void configMipGen();

    void convert2DR();
    void convert2DRg();
    void convert2DRgb();
    void convert2DRgba();
    void convert2DMipmaps();

    void convertUastcPatchAwaySrgb();

    void convert2DArray();
    void convert2DArrayOneLayer();
    void convert2DArrayMipmaps();

    void convertToFile2D();
    void convertToFile3D();

    void threads();
    void ktx();
    void swizzle();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

using namespace Containers::Literals;

constexpr struct {
    const char* name;
    const Vector3i size;
    const char* message;
} TooManyLevelsData[]{
    {"2D", {1, 1, 0}, "there can be only 1 levels with base image size Vector(1, 1) but got 2"},
    /* 3D images are treated like and exported as 2D array images */
    {"2D array", {1, 1, 2}, "there can be only 1 levels with base image size Vector(1, 1, 2) but got 2"}
};

constexpr struct {
    const char* name;
    const Vector3i sizes[2];
    const char* message;
} LevelWrongSizeData[]{
    {"2D", {{4, 5, 0}, {2, 1, 0}}, "expected size Vector(2, 2) for level 1 but got Vector(2, 1)"},
    /* 3D images are treated like and exported as 2D array images */
    {"2D array", {{4, 5, 3}, {2, 2, 1}}, "expected size Vector(2, 2, 3) for level 1 but got Vector(2, 2, 1)"}
};

enum TransferFunction: std::size_t {
    Linear,
    Srgb
};

constexpr PixelFormat TransferFunctionFormats[2][4]{
    {PixelFormat::R8Unorm, PixelFormat::RG8Unorm, PixelFormat::RGB8Unorm, PixelFormat::RGBA8Unorm},
    {PixelFormat::R8Srgb, PixelFormat::RG8Srgb, PixelFormat::RGB8Srgb, PixelFormat::RGBA8Srgb}
};

constexpr struct {
    const char* name;
    bool uastc;
    const TransferFunction transferFunction;
} EncodingFormatTransferFunctionData[]{
    {"Unorm ETC1S", false, TransferFunction::Linear},
    {"Unorm UASTC", true, TransferFunction::Linear},
    {"Srgb ETC1S", false, TransferFunction::Srgb},
    {"Srgb UASTC", true, TransferFunction::Srgb}
};

const struct {
    const char* name;
    const char* pluginName;
    PixelFormat format;
    const char* perceptual;
    bool uastc;
    bool verbose;
    PixelFormat expected;
    const char* message;
} ConvertUastcPatchAwaySrgbData[]{
    /* Basis container has sRGB always set for UASTC, needs patching away if
       linear is desired either via a format or via a flag */
    {"Unorm ETC1S Basis", "BasisImageConverter",
        PixelFormat::RGBA8Unorm, "", false, true, PixelFormat::RGBA8Unorm,
        ""},
    {"Unorm ETC1S Basis, perceptual=true", "BasisImageConverter",
        PixelFormat::RGBA8Unorm, "true", false, true, PixelFormat::RGBA8Srgb,
        ""},
    {"Unorm UASTC Basis, quiet", "BasisImageConverter",
        PixelFormat::RGBA8Unorm, "", true, false, PixelFormat::RGBA8Unorm,
        ""},
    {"Unorm UASTC Basis", "BasisImageConverter",
        PixelFormat::RGBA8Unorm, "", true, true, PixelFormat::RGBA8Unorm,
        "Trade::BasisImageConverter::convertToData(): patching away an incorrect sRGB flag in the output Basis file\n"},
    {"Unorm UASTC Basis, perceptual=true", "BasisImageConverter",
        PixelFormat::RGBA8Unorm, "true", true, true, PixelFormat::RGBA8Srgb,
        ""},
    {"Srgb ETC1S Basis", "BasisImageConverter",
        PixelFormat::RGBA8Srgb, "", false, true, PixelFormat::RGBA8Srgb,
        ""},
    {"Srgb ETC1S Basis, perceptual=false", "BasisImageConverter",
        PixelFormat::RGBA8Srgb, "false", false, true, PixelFormat::RGBA8Unorm,
        ""},
    {"Srgb UASTC Basis", "BasisImageConverter",
        PixelFormat::RGBA8Srgb, "", true, true, PixelFormat::RGBA8Srgb,
        ""},
    {"Srgb UASTC Basis, perceptual=false", "BasisImageConverter",
        PixelFormat::RGBA8Srgb, "false", true, true, PixelFormat::RGBA8Unorm,
        "Trade::BasisImageConverter::convertToData(): patching away an incorrect sRGB flag in the output Basis file\n"},

    /* The KTX container doesn't suffer from this problem */
    {"Unorm UASTC KTX2", "BasisKtxImageConverter",
        PixelFormat::RGBA8Unorm, "", true, true, PixelFormat::RGBA8Unorm,
        ""},
    {"Srgb UASTC KTX2, perceptual=false", "BasisKtxImageConverter",
        PixelFormat::RGBA8Srgb, "false", true, true, PixelFormat::RGBA8Unorm,
        ""},
};

constexpr struct {
    const char* name;
    const char* pluginName;
} Convert2DArrayOneLayerData[]{
    {"Basis", "BasisImageConverter"},
    {"KTX2", "BasisKtxImageConverter"}
};

constexpr const char* BasisFileMagic = "sB";
constexpr const char* KtxFileMagic = "\xabKTX";

constexpr struct {
    const char* name;
    const char* pluginName;
    const char* filename;
    const char* prefix;
} ConvertToFileData[]{
    {"Basis", "BasisImageConverter", "image.basis", BasisFileMagic},
    {"KTX2", "BasisImageConverter", "image.ktx2", KtxFileMagic},
    {"KTX2 with explicit plugin name", "BasisKtxImageConverter", "image.foo", KtxFileMagic}
};

constexpr struct {
    const char* name;
    const char* threads;
} ThreadsData[]{
    {"", nullptr},
    {"2 threads", "2"},
    {"all threads", "0"}
};

constexpr struct {
    const char* name;
    const bool yFlip;
} FlippedData[]{
    {"y-flip", true},
    {"no y-flip", false}
};

constexpr struct {
    const char* name;
    const PixelFormat format;
    const Color4ub input;
    const char* swizzle;
    const Color4ub output;
} SwizzleData[] {
    {"R implicit", PixelFormat::R8Unorm, Color4ub{128, 0, 0}, "", Color4ub{128, 128, 128}},
    {"R none", PixelFormat::R8Unorm, Color4ub{128, 0, 0}, "rgba", Color4ub{128, 0, 0}},
    {"RG implicit", PixelFormat::RG8Unorm, Color4ub{64, 128, 0}, "", Color4ub{64, 64, 64, 128}},
    {"RG none", PixelFormat::RG8Unorm, Color4ub{64, 128, 0}, "rgba", Color4ub{64, 128, 0}}
};

BasisImageConverterTest::BasisImageConverterTest() {
    addTests({&BasisImageConverterTest::wrongFormat,
              &BasisImageConverterTest::unknownOutputFormatData,
              &BasisImageConverterTest::unknownOutputFormatFile,
              &BasisImageConverterTest::invalidSwizzle});

    addInstancedTests({&BasisImageConverterTest::tooManyLevels},
        Containers::arraySize(TooManyLevelsData));

    addInstancedTests({&BasisImageConverterTest::levelWrongSize},
        Containers::arraySize(LevelWrongSizeData));

    addTests({&BasisImageConverterTest::processError,

              &BasisImageConverterTest::configPerceptual,
              &BasisImageConverterTest::configMipGen});

    addInstancedTests({&BasisImageConverterTest::convert2DR,
                       &BasisImageConverterTest::convert2DRg,
                       &BasisImageConverterTest::convert2DRgb,
                       &BasisImageConverterTest::convert2DRgba},
        Containers::arraySize(EncodingFormatTransferFunctionData));

    addInstancedTests({&BasisImageConverterTest::convertUastcPatchAwaySrgb},
        Containers::arraySize(ConvertUastcPatchAwaySrgbData));

    addTests({&BasisImageConverterTest::convert2DMipmaps,

              &BasisImageConverterTest::convert2DArray});

    addInstancedTests({&BasisImageConverterTest::convert2DArrayOneLayer},
        Containers::arraySize(Convert2DArrayOneLayerData));

    addTests({&BasisImageConverterTest::convert2DArrayMipmaps});

    /* Just testing that image levels and file type get forwarded to
       doConvertToData(), anything else is tested in convert*() */
    addInstancedTests({&BasisImageConverterTest::convertToFile2D,
                       &BasisImageConverterTest::convertToFile3D},
        Containers::arraySize(ConvertToFileData));

    addInstancedTests({&BasisImageConverterTest::threads},
        Containers::arraySize(ThreadsData));

    addInstancedTests({&BasisImageConverterTest::ktx},
        Containers::arraySize(FlippedData));

    addInstancedTests({&BasisImageConverterTest::swizzle},
        Containers::arraySize(SwizzleData));

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
    #ifdef BASISIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(BASISIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef BASISIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(BASISIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif

    /* Create the output directory if it doesn't exist yet */
    CORRADE_INTERNAL_ASSERT_OUTPUT(Utility::Path::make(BASISIMAGECONVERTER_TEST_OUTPUT_DIR));
}

void BasisImageConverterTest::wrongFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");

    const char data[8]{};
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RG32F, {1, 1}, data}));
    CORRADE_COMPARE(out.str(), "Trade::BasisImageConverter::convertToData(): unsupported format PixelFormat::RG32F\n");
}

void BasisImageConverterTest::unknownOutputFormatData() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");

    /* The converter defaults to .basis output, conversion should succeed */

    const char data[4]{};
    Containers::Optional<Containers::Array<char>> converted = converter->convertToData(ImageView2D{PixelFormat::RGB8Unorm, {1, 1}, data});
    CORRADE_VERIFY(converted);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*converted));
}

void BasisImageConverterTest::unknownOutputFormatFile() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");

    /* The converter defaults to .basis output, conversion should succeed */

    const char data[4]{};
    const ImageView2D image{PixelFormat::RGB8Unorm, {1, 1}, data};
    Containers::String filename = Utility::Path::join(BASISIMAGECONVERTER_TEST_OUTPUT_DIR, "file.foo");
    CORRADE_VERIFY(converter->convertToFile(image, filename));

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openFile(filename));
}

void BasisImageConverterTest::invalidSwizzle() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");

    const char data[8]{};
    std::ostringstream out;
    Error redirectError{&out};

    converter->configuration().setValue("swizzle", "gbgbg");
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, data}));

    converter->configuration().setValue("swizzle", "xaaa");
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RGBA8Unorm, {1, 1}, data}));

    CORRADE_COMPARE(out.str(),
        "Trade::BasisImageConverter::convertToData(): invalid swizzle length, expected 4 but got 5\n"
        "Trade::BasisImageConverter::convertToData(): invalid characters in swizzle xaaa\n");
}

void BasisImageConverterTest::tooManyLevels() {
    auto&& data = TooManyLevelsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");

    const UnsignedByte bytes[8]{};
    CORRADE_INTERNAL_ASSERT(Math::max(Vector3ui{data.size}, 1u).product() <= Containers::arraySize(bytes));

    const UnsignedInt dimensions = Math::min(Vector3ui{data.size}, 1u).sum();

    std::ostringstream out;
    Error redirectError{&out};
    if(dimensions == 1) {
        CORRADE_VERIFY(!converter->convertToData({
            ImageView1D{PixelFormat::R8Unorm, data.size.x(), bytes},
            ImageView1D{PixelFormat::R8Unorm, data.size.x(), bytes}
        }));
    } else if(dimensions == 2) {
        CORRADE_VERIFY(!converter->convertToData({
            ImageView2D{PixelFormat::R8Unorm, data.size.xy(), bytes},
            ImageView2D{PixelFormat::R8Unorm, data.size.xy(), bytes}
        }));
    } else if(dimensions == 3) {
        CORRADE_VERIFY(!converter->convertToData({
            ImageView3D{PixelFormat::R8Unorm, data.size, bytes},
            ImageView3D{PixelFormat::R8Unorm, data.size, bytes}
        }));
    }

    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::BasisImageConverter::convertToData(): {}\n", data.message));
}

void BasisImageConverterTest::levelWrongSize() {
    auto&& data = LevelWrongSizeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");

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

    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::BasisImageConverter::convertToData(): {}\n", data.message));
}

void BasisImageConverterTest::processError() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    converter->configuration().setValue("max_endpoint_clusters",
        16128 /* basisu_frontend::cMaxEndpointClusters */ + 1);

    const char bytes[4]{};
    ImageView2D image{PixelFormat::RGBA8Unorm, Vector2i{1}, bytes};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(image));
    CORRADE_COMPARE(out.str(),
        "Trade::BasisImageConverter::convertToData(): frontend processing failed\n");
}

void BasisImageConverterTest::configPerceptual() {
    const char bytes[4]{};
    ImageView2D originalImage{PixelFormat::RGBA8Unorm, Vector2i{1}, bytes};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    /* Empty by default */
    CORRADE_COMPARE(converter->configuration().value("perceptual"), "");

    Containers::Optional<Containers::Array<char>> compressedDataAutomatic = converter->convertToData(originalImage);
    CORRADE_VERIFY(compressedDataAutomatic);

    converter->configuration().setValue("perceptual", true);

    Containers::Optional<Containers::Array<char>> compressedDataOverridden = converter->convertToData(originalImage);
    CORRADE_VERIFY(compressedDataOverridden);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");

    /* Empty perceptual config means to use the image format to determine if
       the output data should be sRGB */
    CORRADE_VERIFY(importer->openData(*compressedDataAutomatic));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);

    /* Perceptual true/false overrides the input format and forces sRGB on/off */
    CORRADE_VERIFY(importer->openData(*compressedDataOverridden));
    image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Srgb);
}

void BasisImageConverterTest::configMipGen() {
    const char bytes[16*16*4]{};
    ImageView2D originalLevel0{PixelFormat::RGBA8Unorm, Vector2i{16}, bytes};
    ImageView2D originalLevel1{PixelFormat::RGBA8Unorm, Vector2i{8}, bytes};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    /* Empty by default */
    CORRADE_COMPARE(converter->configuration().value<bool>("mip_gen"), false);
    converter->configuration().setValue("mip_gen", "");

    Containers::Optional<Containers::Array<char>> compressedDataGenerated = converter->convertToData({originalLevel0});
    CORRADE_VERIFY(compressedDataGenerated);

    Containers::Optional<Containers::Array<char>> compressedDataProvided = converter->convertToData({originalLevel0, originalLevel1});
    CORRADE_VERIFY(compressedDataProvided);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");

    /* Empty mip_gen config means to use the level count to determine if mip
       levels should be generated */
    CORRADE_VERIFY(importer->openData(*compressedDataGenerated));
    CORRADE_COMPARE(importer->image2DLevelCount(0), 5);

    CORRADE_VERIFY(importer->openData(*compressedDataProvided));
    CORRADE_COMPARE(importer->image2DLevelCount(0), 2);
}

template<typename SourceType, typename DestinationType = SourceType, UnsignedInt dimensions>
Image<dimensions> copyImageWithSkip(const BasicImageView<dimensions>& image, Math::Vector<dimensions, Int> skip, PixelFormat format = PixelFormat{}) {
    const Math::Vector<dimensions, Int> size = image.size();
    if(format == PixelFormat{})
        format = image.format();
    /* Width includes row alignment to 4 bytes */
    const UnsignedInt formatSize = pixelSize(format);
    const UnsignedInt widthWithSkip = ((size[0] + skip[0])*DestinationType::Size + 3)/formatSize*formatSize;
    const UnsignedInt dataSize = widthWithSkip*(size + skip).product()/(size[0] + skip[0]);
    Image<dimensions> imageWithSkip{PixelStorage{}.setSkip(Vector3i::pad(skip)), format,
        size, Containers::Array<char>{ValueInit, dataSize}};
    Utility::copy(Containers::arrayCast<const DestinationType>(
        image.template pixels<SourceType>()),
        imageWithSkip.template pixels<DestinationType>());
    return imageWithSkip;
}

void BasisImageConverterTest::convert2DR() {
    auto&& data = EncodingFormatTransferFunctionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png")));
    Containers::Optional<Trade::ImageData2D> originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly. During copy, we only use R channel to retrieve an
       R8 image. */
    const Image2D imageWithSkip = copyImageWithSkip<Color3ub, Math::Vector<1, UnsignedByte>>(
        ImageView2D(*originalImage), {7, 8}, TransferFunctionFormats[data.transferFunction][0]);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    if(data.uastc) converter->configuration().setValue("uastc", true);
    else CORRADE_VERIFY(!converter->configuration().value<bool>("uastc"));
    Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), TransferFunctionFormats[data.transferFunction][3]);

    /* CompareImage doesn't support Srgb formats, so we need to create a view
       on the original image, but with a Unorm format */
    const ImageView2D imageViewUnorm{imageWithSkip.storage(),
        TransferFunctionFormats[TransferFunction::Linear][0], imageWithSkip.size(), imageWithSkip.data()};
    /* Basis can only load RGBA8 uncompressed data, which corresponds to RRR1
       from our R8 image data. We chose the red channel from the imported image
       to compare to our original data. */
    CORRADE_COMPARE_WITH(
        (Containers::arrayCast<2, const UnsignedByte>(image->pixels().prefix(
            {std::size_t(image->size()[1]), std::size_t(image->size()[0]), 1}))),
        imageViewUnorm,
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImage{21.0f, 0.968f}));
}

void BasisImageConverterTest::convert2DRg() {
    auto&& data = EncodingFormatTransferFunctionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png")));
    Containers::Optional<Trade::ImageData2D> originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly. During copy, we only use R and G channels to
       retrieve an RG8 image. */
    const Image2D imageWithSkip = copyImageWithSkip<Color3ub, Vector2ub>(
        ImageView2D(*originalImage), {7, 8}, TransferFunctionFormats[data.transferFunction][1]);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    if(data.uastc) converter->configuration().setValue("uastc", true);
    else CORRADE_VERIFY(!converter->configuration().value<bool>("uastc"));
    Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), TransferFunctionFormats[data.transferFunction][3]);

    /* CompareImage doesn't support Srgb formats, so we need to create a view
       on the original image, but with a Unorm format */
    const ImageView2D imageViewUnorm{imageWithSkip.storage(),
        TransferFunctionFormats[TransferFunction::Linear][1], imageWithSkip.size(), imageWithSkip.data()};
    /* Basis can only load RGBA8 uncompressed data, which corresponds to RRRG
       from our RG8 image data. We chose the B and A channels from the imported
       image to compare to our original data. */
    CORRADE_COMPARE_WITH(
        (Containers::arrayCast<2, const Math::Vector2<UnsignedByte>>(image->pixels().exceptPrefix({0, 0, 2}))),
        imageViewUnorm,
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImage{22.0f, 1.039f}));
}

void BasisImageConverterTest::convert2DRgb() {
    auto&& data = EncodingFormatTransferFunctionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png")));
    Containers::Optional<Trade::ImageData2D> originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly */
    const Image2D imageWithSkip = copyImageWithSkip<Color3ub>(
        ImageView2D(*originalImage), {7, 8}, TransferFunctionFormats[data.transferFunction][2]);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    if(data.uastc) converter->configuration().setValue("uastc", true);
    else CORRADE_VERIFY(!converter->configuration().value<bool>("uastc"));
    Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), TransferFunctionFormats[data.transferFunction][3]);

    CORRADE_COMPARE_WITH(Containers::arrayCast<const Color3ub>(image->pixels<Color4ub>()),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgb-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 61.0f, 6.622f}));
}

void BasisImageConverterTest::convert2DRgba() {
    auto&& data = EncodingFormatTransferFunctionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png")));
    Containers::Optional<Trade::ImageData2D> originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly */
    const Image2D imageWithSkip = copyImageWithSkip<Color4ub>(
        ImageView2D(*originalImage), {7, 8}, TransferFunctionFormats[data.transferFunction][3]);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    if(data.uastc) converter->configuration().setValue("uastc", true);
    else CORRADE_VERIFY(!converter->configuration().value<bool>("uastc"));
    Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), TransferFunctionFormats[data.transferFunction][3]);

    CORRADE_COMPARE_WITH(image->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 97.25f, 8.547f}));
}

void BasisImageConverterTest::convertUastcPatchAwaySrgb() {
    auto&& data = ConvertUastcPatchAwaySrgbData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate(data.pluginName);
    if(data.verbose) converter->addFlags(ImageConverterFlag::Verbose);
    converter->configuration().setValue("uastc", data.uastc);
    converter->configuration().setValue("perceptual", data.perceptual);

    const char imageData[4*4*4]{};

    std::ostringstream out;
    Containers::Optional<Containers::Array<char>> compressedData;
    {
        Debug redirectOutput{&out};
        compressedData = converter->convertToData(ImageView2D{data.format, {4, 4}, Containers::arrayView(imageData)});
        CORRADE_VERIFY(compressedData);
    }

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*compressedData));
    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(!image->isCompressed());
    CORRADE_COMPARE(image->format(), data.expected);
    CORRADE_COMPARE(out.str(), data.message);
}

void BasisImageConverterTest::convert2DMipmaps() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");

    struct Level {
        const char* file;
        Containers::Optional<Image2D> imageWithSkip;
        Containers::Optional<ImageData2D> result;
    } levels[3] {
        {"rgba-63x27.png", {}, {}},
        {"rgba-31x13.png", {}, {}},
        {"rgba-15x6.png", {}, {}}
    };

    for(Level& level: levels) {
        CORRADE_ITERATION(level.file);
        CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, level.file)));
        Containers::Optional<Trade::ImageData2D> originalImage = pngImporter->image2D(0);
        CORRADE_VERIFY(originalImage);
        /* Use the original images and add a skip to ensure the converter reads
           the image data properly */
        level.imageWithSkip = copyImageWithSkip<Color4ub>(ImageView2D(*originalImage), {7, 8});
    }

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");

    /* Off by default */
    CORRADE_COMPARE(converter->configuration().value<bool>("mip_gen"), false);
    /* Making sure that providing custom levels turns off automatic mip level
       generation. We only provide an incomplete mip chain so we can tell if
       basis generated any extra levels beyond that. */
    converter->configuration().setValue("mip_gen", true);

    std::ostringstream out;
    Warning redirectWarning{&out};

    Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData({*levels[0].imageWithSkip, *levels[1].imageWithSkip, *levels[2].imageWithSkip});
    CORRADE_VERIFY(compressedData);
    CORRADE_COMPARE(out.str(), "Trade::BasisImageConverter::convertToData(): found user-supplied mip levels, ignoring mip_gen config value\n");

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*compressedData));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), Containers::arraySize(levels));

    for(std::size_t i = 0; i != Containers::arraySize(levels); ++i) {
        CORRADE_ITERATION("level" << i);
        levels[i].result = importer->image2D(0, i);
        CORRADE_VERIFY(levels[i].result);
    }

    CORRADE_COMPARE_WITH(*levels[0].result,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 97.25f, 7.882f}));
    CORRADE_COMPARE_WITH(*levels[1].result,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-31x13.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 81.0f, 14.33f}));
    CORRADE_COMPARE_WITH(*levels[2].result,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-15x6.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 76.25f, 24.5f}));
}

ImageView2D imageViewSlice(const ImageView3D& image, Int slice) {
    CORRADE_INTERNAL_ASSERT(image.storage().skip() == Vector3i{});
    return ImageView2D{image.storage(), image.format(), image.size().xy(),
        Containers::arrayView(image.pixels()[slice].data(), image.pixels().stride()[0])};
}

void BasisImageConverterTest::convert2DArray() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png")));
    Containers::Optional<Trade::ImageData2D> originalSlice = pngImporter->image2D(0);
    CORRADE_VERIFY(originalSlice);

    Containers::Array<char> data{NoInit, originalSlice->data().size()*3};
    MutableImageView3D originalImage{originalSlice->format(), {originalSlice->size(), 3}, data};
    Utility::copy(originalSlice->pixels(), originalImage.pixels()[0]);
    Containers::StridedArrayView3D<Color4ub> pixels = originalImage.pixels<Color4ub>();
    for(Int y = 0; y != originalImage.size().y(); ++y) {
        for(Int x = 0; x != originalImage.size().x(); ++x) {
            const Color4ub& original = pixels[0][y][x];
            pixels[1][y][x] = Color4ub{255} - original;
            pixels[2][y][x] = Math::gather<'b', 'r', 'a', 'g'>(original);
        }
    }

    /* Use the built image and add a skip to ensure the converter reads the
       image data properly */
    const Image3D imageWithSkip = copyImageWithSkip<Color4ub>(ImageView3D(originalImage), {7, 8, 5});

    std::ostringstream out;
    Warning redirectWarning{&out};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);
    CORRADE_COMPARE(out.str(), "Trade::BasisImageConverter::convertToData(): exporting 3D image as a 2D array image\n");

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*compressedData));
    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->textureCount(), 1);
    Containers::Optional<Trade::TextureData> texture = importer->texture(0);
    CORRADE_COMPARE(texture->type(), TextureType::Texture2DArray);
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);

    /* CompareImage only supports 2D images, compare each layer individually */
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[0], imageViewSlice(ImageView3D(originalImage), 0),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImage{97.25f, 7.89f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[1], imageViewSlice(ImageView3D(originalImage), 1),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImage{97.25f, 7.735f}));
    CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[2], imageViewSlice(ImageView3D(originalImage), 2),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImage{96.5f, 6.928f}));
}

void BasisImageConverterTest::convert2DArrayOneLayer() {
    auto&& data = Convert2DArrayOneLayerData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    /* For KTX2 files, basis_universal treats 2D array images with one layer
       as standard 2D images:
       https://github.com/BinomialLLC/basis_universal/blob/928a0caa3e5db2d4748bce6b23507757f9867d14/encoder/basisu_comp.cpp#L1809 */

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png")));
    Containers::Optional<Trade::ImageData2D> originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    std::ostringstream out;
    Warning redirectWarning{&out};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate(data.pluginName);
    Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData(ImageView3D{ImageView2D(*originalImage)});
    CORRADE_VERIFY(compressedData);
    CORRADE_COMPARE(out.str(), "Trade::BasisImageConverter::convertToData(): exporting 3D image as a 2D array image\n");

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*compressedData));
    CORRADE_COMPARE(importer->textureCount(), 1);
    {
        CORRADE_EXPECT_FAIL_IF(data.pluginName == "BasisKtxImageConverter"_s,
            "basis_universal exports KTX2 2D array images with a single layer as 2D images.");
        CORRADE_COMPARE(importer->image3DCount(), 1);
        Containers::Optional<Trade::TextureData> texture = importer->texture(0);
        CORRADE_COMPARE(texture->type(), TextureType::Texture2DArray);
    }
}

void BasisImageConverterTest::convert2DArrayMipmaps() {
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");

    struct Level {
        const char* file;
        Containers::Optional<Image3D> originalImage;
        Containers::Optional<Image3D> imageWithSkip;
        Containers::Optional<ImageData3D> result;
    } levels[3] {
        {"rgba-63x27.png", {}, {}, {}},
        {"rgba-31x13.png", {}, {}, {}},
        {"rgba-15x6.png", {}, {}, {}}
    };

    for(Level& level: levels) {
        CORRADE_ITERATION(level.file);
        CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, level.file)));
        Containers::Optional<Trade::ImageData2D> originalSlice = pngImporter->image2D(0);
        CORRADE_VERIFY(originalSlice);

        level.originalImage = Image3D{originalSlice->format(), {originalSlice->size(), 3},
            Containers::Array<char>{NoInit, originalSlice->data().size()*3}};
        Utility::copy(originalSlice->pixels(), level.originalImage->pixels()[0]);
        Containers::StridedArrayView3D<Color4ub> pixels = level.originalImage->pixels<Color4ub>();
        for(Int y = 0; y != level.originalImage->size().y(); ++y) {
            for(Int x = 0; x != level.originalImage->size().x(); ++x) {
                const Color4ub& original = pixels[0][y][x];
                pixels[1][y][x] = Color4ub{255} - original;
                pixels[2][y][x] = Math::gather<'b', 'r', 'a', 'g'>(original);
            }
        }

        /* Use the original images and add a skip to ensure the converter reads
           the image data properly */
        level.imageWithSkip = copyImageWithSkip<Color4ub>(ImageView3D(*level.originalImage), {7, 8, 5});
    }

    std::ostringstream out;
    Warning redirectWarning{&out};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData({*levels[0].imageWithSkip, *levels[1].imageWithSkip, *levels[2].imageWithSkip});
    CORRADE_VERIFY(compressedData);
    CORRADE_COMPARE(out.str(), "Trade::BasisImageConverter::convertToData(): exporting 3D image as a 2D array image\n");

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*compressedData));
    CORRADE_COMPARE(importer->image3DCount(), 1);
    CORRADE_COMPARE(importer->image3DLevelCount(0), Containers::arraySize(levels));
    CORRADE_COMPARE(importer->textureCount(), 1);
    Containers::Optional<Trade::TextureData> texture = importer->texture(0);
    CORRADE_COMPARE(texture->type(), TextureType::Texture2DArray);
    Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);

    for(std::size_t i = 0; i != Containers::arraySize(levels); ++i) {
        CORRADE_ITERATION("level" << i);
        levels[i].result = importer->image3D(0, i);
        CORRADE_VERIFY(levels[i].result);
    }

    /* CompareImage only supports 2D images, compare each layer individually */
    for(Int i = 0; i != levels[0].originalImage->size().z(); ++i) {
        CORRADE_ITERATION("level 0, layer" << i);
        CORRADE_COMPARE_WITH(levels[0].result->pixels<Color4ub>()[i],
            imageViewSlice(ImageView3D(*levels[0].originalImage), i),
            /* There are moderately significant compression artifacts */
            (DebugTools::CompareImage{97.25f, 7.914f}));
    }
    for(Int i = 0; i != levels[0].originalImage->size().z(); ++i) {
        CORRADE_ITERATION("level 1, layer" << i);
        CORRADE_COMPARE_WITH(levels[1].result->pixels<Color4ub>()[i],
            imageViewSlice(ImageView3D(*levels[1].originalImage), i),
            /* There are moderately significant compression artifacts */
            (DebugTools::CompareImage{87.0f, 14.453f}));
    }
    for(Int i = 0; i != levels[0].originalImage->size().z(); ++i) {
        CORRADE_ITERATION("level 2, layer" << i);
        CORRADE_COMPARE_WITH(levels[2].result->pixels<Color4ub>()[i],
            imageViewSlice(ImageView3D(*levels[2].originalImage), i),
            /* There are moderately significant compression artifacts */
            (DebugTools::CompareImage{80.5f, 23.878f}));
    }
}

void BasisImageConverterTest::convertToFile2D() {
    auto&& data = ConvertToFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png")));
    Containers::Optional<Trade::ImageData2D> originalLevel0 = pngImporter->image2D(0);
    CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-31x13.png")));
    Containers::Optional<Trade::ImageData2D> originalLevel1 = pngImporter->image2D(0);
    CORRADE_VERIFY(originalLevel0);
    CORRADE_VERIFY(originalLevel1);

    const ImageView2D originalLevels[2]{*originalLevel0, *originalLevel1};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate(data.pluginName);
    Containers::String filename = Utility::Path::join(BASISIMAGECONVERTER_TEST_OUTPUT_DIR, data.filename);
    CORRADE_VERIFY(converter->convertToFile(originalLevels, filename));

    /* Verify it's actually the right format */
    /** @todo some FileHasPrefix, and possibly optimization for binary data? */
    Containers::Optional<Containers::String> output = Utility::Path::readString(filename);
    CORRADE_VERIFY(output);
    CORRADE_COMPARE_AS(*output, data.prefix,
        TestSuite::Compare::StringHasPrefix);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openFile(filename));
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image2DLevelCount(0), 2);

    Containers::Optional<Trade::ImageData2D> level0 = importer->image2D(0, 0);
    Containers::Optional<Trade::ImageData2D> level1 = importer->image2D(0, 1);
    CORRADE_VERIFY(level0);
    CORRADE_VERIFY(level1);

    CORRADE_COMPARE_WITH(*level0,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 97.25f, 7.882f}));
    CORRADE_COMPARE_WITH(*level1,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-31x13.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 81.0f, 14.31f}));

    /* The format should get reset again after so convertToData() isn't left
       with some random format after */
    if(data.pluginName == "BasisImageConverter"_s) {
        Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData(originalLevels);
        CORRADE_VERIFY(compressedData);
        /* Not testing with Compare::StringHasPrefix because it would print the
           whole binary on error. Not wanted.. */
        CORRADE_VERIFY(Containers::StringView{*compressedData}.hasPrefix(BasisFileMagic));
    }
}

void BasisImageConverterTest::convertToFile3D() {
    auto&& data = ConvertToFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png")));
    Containers::Optional<Trade::ImageData2D> originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    const ImageView3D originalImage3D{ImageView2D(*originalImage)};

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate(data.pluginName);
    Containers::String filename = Utility::Path::join(BASISIMAGECONVERTER_TEST_OUTPUT_DIR, data.filename);
    CORRADE_VERIFY(converter->convertToFile({originalImage3D}, filename));

    /* Verify it's actually the right format */
    /** @todo some FileHasPrefix, and possibly optimization for binary data? */
    Containers::Optional<Containers::String> output = Utility::Path::readString(filename);
    CORRADE_VERIFY(output);
    CORRADE_COMPARE_AS(*output, data.prefix,
        TestSuite::Compare::StringHasPrefix);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openFile(filename));

    const bool isKtx = Containers::StringView{data.prefix} == KtxFileMagic;
    {
        CORRADE_EXPECT_FAIL_IF(isKtx,
            "basis_universal exports KTX2 2D array images with a single layer as 2D images.");
        CORRADE_COMPARE(importer->image3DCount(), 1);
    }

    if(!isKtx) {
        Containers::Optional<Trade::ImageData3D> image = importer->image3D(0);
        CORRADE_VERIFY(image);

        CORRADE_COMPARE_WITH(image->pixels<Color4ub>()[0], *originalImage,
            /* There are moderately significant compression artifacts */
            (DebugTools::CompareImage{97.25f, 7.914f}));
    }

    /* The format should get reset again after so convertToData() isn't left
       with some random format after */
    if(data.pluginName == "BasisImageConverter"_s) {
        Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData({originalImage3D});
        CORRADE_VERIFY(compressedData);
        CORRADE_VERIFY(Containers::StringView{*compressedData}.hasPrefix(BasisFileMagic));
    }
}

void BasisImageConverterTest::threads() {
    auto&& data = ThreadsData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png")));
    Containers::Optional<Trade::ImageData2D> originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly */
    const Image2D imageWithSkip = copyImageWithSkip<Color4ub>(ImageView2D(*originalImage), {7, 8});

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    if(data.threads) converter->configuration().setValue("threads", data.threads);
    Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    CORRADE_COMPARE_WITH(image->pixels<Color4ub>(),
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 97.25f, 7.914f}));
}

void BasisImageConverterTest::ktx() {
    auto&& data = FlippedData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png")));
    Containers::Optional<Trade::ImageData2D> originalImage = pngImporter->image2D(0);
    CORRADE_VERIFY(originalImage);

    /* Use the original image and add a skip to ensure the converter reads the
       image data properly */
    const Image2D imageWithSkip = copyImageWithSkip<Color4ub>(ImageView2D(*originalImage), {7, 8});

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisKtxImageConverter");
    converter->configuration().setValue("create_ktx2_file", true);
    converter->configuration().setValue("y_flip", data.yFlip);
    const Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData(imageWithSkip);
    CORRADE_VERIFY(compressedData);
    const Containers::StringView compressedView{Containers::arrayView(*compressedData)};

    CORRADE_VERIFY(compressedView.hasPrefix(KtxFileMagic));

    char KTXorientation[] = "KTXorientation\0r?";
    KTXorientation[sizeof(KTXorientation) - 1] = data.yFlip ? 'u' : 'd';
    CORRADE_VERIFY(compressedView.contains(KTXorientation));

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*compressedData));
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    /* Basis can only load RGBA8 uncompressed data, which corresponds to RGB1
       from our RGB8 image data. */
    Containers::StridedArrayView2D<const Color4ub> pixels = image->pixels<Color4ub>();
    if(!data.yFlip) pixels = pixels.flipped<0>();
    CORRADE_COMPARE_WITH(pixels,
        Utility::Path::join(BASISIMPORTER_TEST_DIR, "rgba-63x27.png"),
        /* There are moderately significant compression artifacts */
        (DebugTools::CompareImageToFile{_manager, 97.25f, 9.398f}));
}

void BasisImageConverterTest::swizzle() {
    auto&& data = SwizzleData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("BasisImageConverter");
    /* Default is empty */
    CORRADE_COMPARE(converter->configuration().value("swizzle"), "");
    converter->configuration().setValue("swizzle", data.swizzle);

    const Color4ub pixel[1]{data.input};
    const ImageView2D originalImage{data.format, {1, 1}, Containers::arrayCast<const char>(pixel)};

    Containers::Optional<Containers::Array<char>> compressedData = converter->convertToData(originalImage);
    CORRADE_VERIFY(compressedData);

    if(_manager.loadState("BasisImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("BasisImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("BasisImporterRGBA8");
    CORRADE_VERIFY(importer->openData(*compressedData));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), (Vector2i{1, 1}));
    /* There are very minor compression artifacts */
    CORRADE_COMPARE_WITH(
        Vector4i{image->pixels<Vector4ub>()[0][0]},
        Vector4i{data.output},
        TestSuite::Compare::around(Vector4i{2}));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::BasisImageConverterTest)
