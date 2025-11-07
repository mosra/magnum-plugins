/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
              Vladimír Vondruš <mosra@centrum.cz>

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

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

#include <png.h> /* PNG_WARNINGS_SUPPORTED */

namespace Magnum { namespace Trade { namespace Test { namespace {

struct PngImageConverterTest: TestSuite::Tester {
    explicit PngImageConverterTest();

    void wrongFormat();
    void conversionError();

    void rgb();
    void rgb16();

    void rgba();

    void grayscale();
    void grayscale16();

    void grayscaleAlpha();

    void unsupportedMetadata();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImageConverter> _converterManager{"nonexistent"};
    PluginManager::Manager<AbstractImporter> _importerManager{"nonexistent"};
};

using namespace Math::Literals;

const struct {
    const char* name;
    ImageConverterFlags flags;
    bool quiet;
} QuietData[]{
    {"", {}, false},
    {"quiet", ImageConverterFlag::Quiet, true}
};

const struct {
    const char* name;
    ImageConverterFlags converterFlags;
    ImageFlags2D imageFlags;
    const char* message;
} UnsupportedMetadataData[]{
    {"1D array", {}, ImageFlag2D::Array,
        "1D array images are unrepresentable in PNG, saving as a regular 2D image"},
    {"1D array, quiet", ImageConverterFlag::Quiet, ImageFlag2D::Array,
        nullptr},
};

PngImageConverterTest::PngImageConverterTest() {
    addTests({&PngImageConverterTest::wrongFormat});

    addInstancedTests({&PngImageConverterTest::conversionError},
        Containers::arraySize(QuietData));

    addTests({&PngImageConverterTest::rgb,
              &PngImageConverterTest::rgb16,

              &PngImageConverterTest::rgba,

              &PngImageConverterTest::grayscale,
              &PngImageConverterTest::grayscale16,

              &PngImageConverterTest::grayscaleAlpha});

    addInstancedTests({&PngImageConverterTest::unsupportedMetadata},
        Containers::arraySize(UnsupportedMetadataData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef PNGIMAGECONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(PNGIMAGECONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* The PngImporter is optional */
    #ifdef PNGIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(PNGIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void PngImageConverterTest::wrongFormat() {
    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");

    const char data[8]{};
    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::RG32F, {1, 1}, data}));
    CORRADE_COMPARE(out, "Trade::PngImageConverter::convertToData(): unsupported pixel format PixelFormat::RG32F\n");
}

void PngImageConverterTest::conversionError() {
    auto&& data = QuietData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Important: this also tests warning suppression! */

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");
    converter->addFlags(data.flags);

    /* Because zero-size images are disallowed by the base implementation
       already, we can't abuse that for checking conversion errors. PNG image
       width/height is limited to 31 bits, so let's pretend we have a 2 GB
       image. Hope this won't trigger sanitizers. */
    const char imageData[1]{};
    Containers::String out;
    Warning redirectWarning{&out};
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertToData(ImageView2D{PixelFormat::R8Unorm, {0x7fffffff, 1}, {imageData, 1u << 31}}));
    if(data.quiet)
        CORRADE_COMPARE(out,
            "Trade::PngImageConverter::convertToData(): error: Invalid IHDR data\n");
    else {
        const char* expected =
            /* The version in emscripten-ports for some reason has warnings
               disabled, making only the error printed, not the warning:
                https://github.com/emscripten-core/emscripten/blob/389b097d3c3e62ba1989593242496c232e922f0e/tools/ports/libpng/pnglibconf.h#L277 */
            #ifdef PNG_WARNINGS_SUPPORTED
            "Trade::PngImageConverter::convertToData(): warning: Image width exceeds user limit in IHDR\n"
            #endif
            "Trade::PngImageConverter::convertToData(): error: Invalid IHDR data\n";
        CORRADE_COMPARE(out, expected);
    }
}

void PngImageConverterTest::rgb() {
    const char original[]{
        /* Skip */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

        0, 0, 0, 1, 2, 3, 2, 3, 4, 0, 0, 0,
        0, 0, 0, 3, 4, 5, 4, 5, 6, 0, 0, 0,
        0, 0, 0, 5, 6, 7, 6, 7, 8, 0, 0, 0
    };

    const char expected[]{
        1, 2, 3, 2, 3, 4, 0, 0,
        3, 4, 5, 4, 5, 6, 0, 0,
        5, 6, 7, 6, 7, 8, 0, 0
    };

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");
    CORRADE_COMPARE(converter->extension(), "png");
    CORRADE_COMPARE(converter->mimeType(), "image/png");

    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{
        PixelStorage{}
            .setRowLength(3)
            .setSkip({1, 1, 0}),
        PixelFormat::RGB8Unorm, {2, 3}, original});
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(*data,
        Utility::Path::join(PNGIMAGECONVERTER_TEST_DIR, "rgb.png"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_AS(*converted,
        (ImageView2D{PixelFormat::RGB8Unorm, {2, 3}, expected}),
        DebugTools::CompareImage);
}

void PngImageConverterTest::rgb16() {
    const UnsignedShort original[]{
        /* Skip */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

        1000, 2000, 3000, 2000, 3000, 4000, 0, 0, 0, 0,
        3000, 4000, 5000, 4000, 5000, 6000, 0, 0, 0, 0,
        5000, 6000, 7000, 6000, 7000, 8000, 0, 0, 0, 0
    };

    const UnsignedShort expected[]{
        1000, 2000, 3000, 2000, 3000, 4000,
        3000, 4000, 5000, 4000, 5000, 6000,
        5000, 6000, 7000, 6000, 7000, 8000
    };

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{
        PixelStorage{}
            .setSkip({0, 1, 0})
            .setRowLength(3),
        PixelFormat::RGB16Unorm, {2, 3}, original});
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(*data,
        Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgb16.png"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_AS(*converted,
        (ImageView2D{PixelFormat::RGB16Unorm, {2, 3}, expected}),
        DebugTools::CompareImage);
}

void PngImageConverterTest::rgba() {
    const Color4ub original[]{
        /* Skip */
        {}, {}, {},

        /* Making sure the alpha is non-trivial, i.e. not all 00 or FF but also
           other values, to verify alpha premultiplication on import */
        0x6633ff99_rgba, 0xcc33ff00_rgba, 0x9933ff66_rgba,
        0x00ccff33_rgba, 0x336699ff_rgba, 0xff0033cc_rgba
    };

    const Color4ub expected[]{
        0x6633ff99_rgba, 0xcc33ff00_rgba, 0x9933ff66_rgba,
        0x00ccff33_rgba, 0x336699ff_rgba, 0xff0033cc_rgba
    };

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{
        PixelStorage{}
            .setSkip({0, 1, 0}),
        PixelFormat::RGBA8Unorm, {3, 2}, original});
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(*data,
        Utility::Path::join(PNGIMPORTER_TEST_DIR, "rgba.png"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_AS(*converted,
        (ImageView2D{PixelFormat::RGBA8Unorm, {3, 2}, expected}),
        DebugTools::CompareImage);
}

void PngImageConverterTest::grayscale() {
    const char original[]{
        /* Skip */
        0, 0, 0, 0,

        1, 2, 0, 0,
        3, 4, 0, 0,
        5, 6, 0, 0
    };

    const char expected[]{
        1, 2, 0, 0,
        3, 4, 0, 0,
        5, 6, 0, 0
    };

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{
        PixelStorage{}
            .setSkip({0, 1, 0}),
        PixelFormat::R8Unorm, {2, 3}, original});
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(*data,
        Utility::Path::join(PNGIMAGECONVERTER_TEST_DIR, "gray.png"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_AS(*converted,
        (ImageView2D{PixelFormat::R8Unorm, {2, 3}, expected}),
        DebugTools::CompareImage);
}

void PngImageConverterTest::grayscale16() {
    const UnsignedShort original[]{
        /* Skip */
        0, 0, 0, 0,

        1000, 2000, 0, 0,
        3000, 4000, 0, 0,
        5000, 6000, 0, 0
    };

    const UnsignedShort expected[]{
        1000, 2000,
        3000, 4000,
        5000, 6000
    };

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{
        PixelStorage{}
            .setSkip({0, 1, 0})
            .setRowLength(3),
        PixelFormat::R16Unorm, {2, 3}, original});
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(*data,
        Utility::Path::join(PNGIMPORTER_TEST_DIR, "gray16.png"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_AS(*converted,
        (ImageView2D{PixelFormat::R16Unorm, {2, 3}, expected}),
        DebugTools::CompareImage);
}

void PngImageConverterTest::grayscaleAlpha() {
    const Vector2ub original[]{
        /* Skip */
        {}, {}, {}, {},

        /* Making sure the alpha is non-trivial, i.e. not all 00 or FF but also
           other values, to verify alpha premultiplication on import */
        {0x66, 0x99}, {0xcc, 0x00}, {0x99, 0x66}, {},
        {0x00, 0x33}, {0x33, 0xff}, {0xff, 0xcc}, {}
    };

    const Vector2ub expected[]{
        {0x66, 0x99}, {0xcc, 0x00}, {0x99, 0x66}, {},
        {0x00, 0x33}, {0x33, 0xff}, {0xff, 0xcc}, {}
    };

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");
    Containers::Optional<Containers::Array<char>> data = converter->convertToData(ImageView2D{
        PixelStorage{}
            .setSkip({0, 1, 0}),
        PixelFormat::RG8Unorm, {3, 2}, original});
    CORRADE_VERIFY(data);
    CORRADE_COMPARE_AS(*data,
        Utility::Path::join(PNGIMPORTER_TEST_DIR, "ga.png"),
        TestSuite::Compare::StringToFile);

    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractImporter> importer = _importerManager.instantiate("PngImporter");
    CORRADE_VERIFY(importer->openData(*data));
    Containers::Optional<Trade::ImageData2D> converted = importer->image2D(0);
    CORRADE_VERIFY(converted);
    CORRADE_COMPARE_AS(*converted,
        (ImageView2D{PixelFormat::RG8Unorm, {3, 2}, expected}),
        DebugTools::CompareImage);
}

void PngImageConverterTest::unsupportedMetadata() {
    auto&& data = UnsupportedMetadataData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImageConverter> converter = _converterManager.instantiate("PngImageConverter");
    converter->addFlags(data.converterFlags);

    const char imageData[4]{};
    ImageView2D image{PixelFormat::RGBA8Unorm, {1, 1}, imageData, data.imageFlags};

    Containers::String out;
    Warning redirectWarning{&out};
    CORRADE_VERIFY(converter->convertToData(image));
    if(!data.message)
        CORRADE_COMPARE(out, "");
    else
        CORRADE_COMPARE(out, Utility::format("Trade::PngImageConverter::convertToData(): {}\n", data.message));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::PngImageConverterTest)
