/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/Containers/String.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/FormatStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

using namespace Containers::Literals;

struct AstcImporterTest: TestSuite::Tester {
    explicit AstcImporterTest();

    void empty2D();
    void empty3D();
    void emptyOneDimensionZero2D();
    void emptyOneDimensionZero2DArray();
    void emptyOneDimensionZero2DArrayNoLayers();
    void emptyOneDimensionZero3D();

    void invalid();
    void invalidFormatConfiguration();

    void twoDimensions();
    void twoDimensionsIncompleteBlocks();
    void twoDimensionsArrayIncompleteBlocks();
    void threeDimensions();

    void fileTooLong2D();
    void fileTooLong3D();

    void openMemory();
    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

const struct {
    const char* name;
    Containers::StringView data;
    const char* message;
} InvalidData[]{
    {"header too short",
        "\x13\xAB\xA1\x5Cxyzxxxyyyzz"_s,
        "file header too short, expected at least 16 bytes but got 15"},
    {"bad magic",
        "\x1E\xAB\xA1\x5Cxyzxxxyyyzzz"_s,
        "invalid file magic 0x5CA1AB1E"}, /* should be 13, not 1E, somehow */
    {"all zeros magic",
        "\x00\x00\x00\x00xyzxxxyyyzzz"_s,
        "invalid file magic 0x00000000"},
    {"invalid 2D block size",
        "\x13\xAB\xA1\x5C" "\x4\x5\x1" "xxxyyyzzz"_s,
        "invalid block size {4, 5, 1}"},
    {"invalid 3D block size",
        "\x13\xAB\xA1\x5C" "\x3\x3\x4" "xxxyyyzzz"_s,
        "invalid block size {3, 3, 4}"},
    {"file too short with complete 2D blocks", /* 1x3x2 blocks */
        "\x13\xAB\xA1\x5C" "\x6\x5\x1" "\x6\0\0" "\xc\0\0" "\x2\0\0"
        "0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcde"_s,
        "file too short, expected 112 bytes but got 111"},
    {"file too short with complete 3D blocks", /* 1x3x2 blocks */
        "\x13\xAB\xA1\x5C" "\x6\x5\x5" "\x6\0\0" "\xe\0\0" "\xa\0\0"
        "0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcde"_s,
        "file too short, expected 112 bytes but got 111"},
    {"file too short with incomplete 2D blocks", /* 1x2x4 blocks */
        "\x13\xAB\xA1\x5C" "\xa\x8\x1" "\x9\0\0" "\xf\0\0" "\x4\0\0"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde"_s,
        "file too short, expected 144 bytes but got 143"},
    {"file too short with incomplete 3D blocks", /* 4x1x2 blocks */
        "\x13\xAB\xA1\x5C" "\x3\x3\x3" "\xb\0\0" "\x2\0\0" "\x5\0\0"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde"_s,
        "file too short, expected 144 bytes but got 143"}
};

const struct {
    const char* name;
    const char* format;
    Containers::Optional<bool> assumeYUpZBackward;
    CompressedPixelFormat expectedFormat2D, expectedFormat3D;
    const char* message;
} FormatData[]{
    {"", nullptr, {},
        CompressedPixelFormat::Astc8x8RGBAUnorm,
        CompressedPixelFormat::Astc3x3x3RGBAUnorm,
        "Trade::AstcImporter::openData(): image is assumed to be encoded with Y down and Z forward, imported data will have wrong orientation. Enable assumeYUpZBackward to suppress this warning.\n"},
    {"assume Y up and Z backward", nullptr, true,
        CompressedPixelFormat::Astc8x8RGBAUnorm,
        CompressedPixelFormat::Astc3x3x3RGBAUnorm,
        ""},
    {"sRGB", "srgb", {},
        CompressedPixelFormat::Astc8x8RGBASrgb,
        CompressedPixelFormat::Astc3x3x3RGBASrgb,
        "Trade::AstcImporter::openData(): image is assumed to be encoded with Y down and Z forward, imported data will have wrong orientation. Enable assumeYUpZBackward to suppress this warning.\n"},
    {"float", "float", {},
        CompressedPixelFormat::Astc8x8RGBAF,
        CompressedPixelFormat::Astc3x3x3RGBAF,
        "Trade::AstcImporter::openData(): image is assumed to be encoded with Y down and Z forward, imported data will have wrong orientation. Enable assumeYUpZBackward to suppress this warning.\n"},
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

AstcImporterTest::AstcImporterTest() {
    addTests({&AstcImporterTest::empty2D,
              &AstcImporterTest::empty3D,
              &AstcImporterTest::emptyOneDimensionZero2D,
              &AstcImporterTest::emptyOneDimensionZero2DArray,
              &AstcImporterTest::emptyOneDimensionZero2DArrayNoLayers,
              &AstcImporterTest::emptyOneDimensionZero3D});

    addInstancedTests({&AstcImporterTest::invalid},
        Containers::arraySize(InvalidData));

    addTests({&AstcImporterTest::invalidFormatConfiguration});

    addInstancedTests({&AstcImporterTest::twoDimensions},
        Containers::arraySize(FormatData));

    addTests({&AstcImporterTest::twoDimensionsIncompleteBlocks,
              &AstcImporterTest::twoDimensionsArrayIncompleteBlocks});

    addInstancedTests({&AstcImporterTest::threeDimensions},
        Containers::arraySize(FormatData));

    addTests({&AstcImporterTest::fileTooLong2D,
              &AstcImporterTest::fileTooLong3D});

    addInstancedTests({&AstcImporterTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&AstcImporterTest::openTwice,
              &AstcImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef ASTCIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(ASTCIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

using namespace Math::Literals;

void AstcImporterTest::empty2D() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");

    CORRADE_VERIFY(importer->openData(
        "\x13\xAB\xA1\x5C" "\xc\xa\x1"
        "\0\0\0" "\0\0\0" "\0\0\0"_s));
    CORRADE_COMPARE(importer->image1DCount(), 0);
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc12x10RGBAUnorm);
    CORRADE_COMPARE(image->size(), Vector2i{});
}

void AstcImporterTest::empty3D() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");

    CORRADE_VERIFY(importer->openData(
        "\x13\xAB\xA1\x5C" "\x5\x4\x4"
        "\0\0\0" "\0\0\0" "\0\0\0"_s));
    CORRADE_COMPARE(importer->image1DCount(), 0);
    CORRADE_COMPARE(importer->image2DCount(), 0);
    CORRADE_COMPARE(importer->image3DCount(), 1);

    Containers::Optional<ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags3D{});
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc5x4x4RGBAUnorm);
    CORRADE_COMPARE(image->size(), Vector3i{});
}

void AstcImporterTest::emptyOneDimensionZero2D() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");

    CORRADE_VERIFY(importer->openData(
        "\x13\xAB\xA1\x5C" "\x6\x6\x1"
        "\0\0\0" "\x5\x3\x1" "\x1\0\0"_s));
    CORRADE_COMPARE(importer->image1DCount(), 0);
    CORRADE_COMPARE(importer->image2DCount(), 1);
    CORRADE_COMPARE(importer->image3DCount(), 0);

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc6x6RGBAUnorm);
    CORRADE_COMPARE(image->size(), (Vector2i{0, 66309}));
}

void AstcImporterTest::emptyOneDimensionZero2DArray() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");

    /* 2D format, but > 1 layer, so it's a 3D image */
    CORRADE_VERIFY(importer->openData(
        "\x13\xAB\xA1\x5C" "\x5\x5\x1"
        "\x5\x3\x1" "\0\0\0" "\x7\xb\x2"_s));
    CORRADE_COMPARE(importer->image1DCount(), 0);
    CORRADE_COMPARE(importer->image2DCount(), 0);
    CORRADE_COMPARE(importer->image3DCount(), 1);

    Containers::Optional<ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlag3D::Array);
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc5x5RGBAUnorm);
    CORRADE_COMPARE(image->size(), (Vector3i{66309, 0, 133895}));
}

void AstcImporterTest::emptyOneDimensionZero2DArrayNoLayers() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");

    /* 0 layers, so it can't be a 2D image either */
    CORRADE_VERIFY(importer->openData(
        "\x13\xAB\xA1\x5C" "\x5\x5\x1"
        "\x7\xb\x2" "\x5\x3\x1" "\0\0\0"_s));
    CORRADE_COMPARE(importer->image1DCount(), 0);
    CORRADE_COMPARE(importer->image2DCount(), 0);
    CORRADE_COMPARE(importer->image3DCount(), 1);

    Containers::Optional<ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlag3D::Array);
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc5x5RGBAUnorm);
    CORRADE_COMPARE(image->size(), (Vector3i{133895, 66309, 0}));
}

void AstcImporterTest::emptyOneDimensionZero3D() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");

    CORRADE_VERIFY(importer->openData(
        "\x13\xAB\xA1\x5C" "\x4\x4\x3"
        "\0\0\0" "\x7\xb\x2" "\x5\x3\x1"_s));
    CORRADE_COMPARE(importer->image1DCount(), 0);
    CORRADE_COMPARE(importer->image2DCount(), 0);
    CORRADE_COMPARE(importer->image3DCount(), 1);

    Containers::Optional<ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags3D{});
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc4x4x3RGBAUnorm);
    CORRADE_COMPARE(image->size(), (Vector3i{0, 133895, 66309}));
}

void AstcImporterTest::invalid() {
    auto&& data = InvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(data.data));
    CORRADE_COMPARE(out.str(), Utility::formatString("Trade::AstcImporter::openData(): {}\n", data.message));
}

void AstcImporterTest::invalidFormatConfiguration() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");
    importer->configuration().setValue("format", "sRGB");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(ASTCIMPORTER_TEST_DIR, "8x8.astc")));
    CORRADE_COMPARE(out.str(),
        "Trade::AstcImporter::openData(): invalid format sRGB, expected linear, srgb or float\n");
}

void AstcImporterTest::twoDimensions() {
    auto&& data = FormatData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");
    if(data.format)
        importer->configuration().setValue("format", data.format);
    else
        CORRADE_COMPARE(importer->configuration().value("format"), "linear");
    if(data.assumeYUpZBackward)
        importer->configuration().setValue("assumeYUpZBackward", *data.assumeYUpZBackward);
    else
        CORRADE_COMPARE(importer->configuration().value("assumeYUpZBackward"), "false");

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASTCIMPORTER_TEST_DIR, "8x8.astc")));
    }
    CORRADE_COMPARE(out.str(), data.message);
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->compressedFormat(), data.expectedFormat2D);
    CORRADE_COMPARE(image->size(), (Vector2i{64, 32}));
    CORRADE_COMPARE(image->data().size(), 8*4*128/8); /* 8x4 blocks */
    /* Verify just a small prefix and suffix to be sure the data got copied */
    CORRADE_COMPARE_AS(image->data().prefix(4), Containers::array({
        '\x9d', '\x84', '\x97', '\xa3',
    }), TestSuite::Compare::Container);
    /** @todo suffix() once it takes N last bytes */
    CORRADE_COMPARE_AS(image->data().exceptPrefix(image->data().size() - 4), Containers::array({
        '\xcc', '\x22', '\xdd', '\x33'
    }), TestSuite::Compare::Container);
}

void AstcImporterTest::twoDimensionsIncompleteBlocks() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASTCIMPORTER_TEST_DIR, "12x10-incomplete-blocks.astc")));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags2D{});
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc12x10RGBAUnorm);
    CORRADE_COMPARE(image->size(), (Vector2i{63, 27}));
    CORRADE_COMPARE(image->data().size(), 6*3*128/8); /* 6x3 blocks */
    /* Verify just a small prefix and suffix to be sure the data got copied */
    CORRADE_COMPARE_AS(image->data().prefix(4), Containers::array({
        '\xa5', '\x88', '\x86', '\x03',
    }), TestSuite::Compare::Container);
    /** @todo suffix() once it takes N last bytes */
    CORRADE_COMPARE_AS(image->data().exceptPrefix(image->data().size() - 4), Containers::array({
        '\x0c', '\xbd', '\xd0', '\x78'
    }), TestSuite::Compare::Container);
}

void AstcImporterTest::twoDimensionsArrayIncompleteBlocks() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASTCIMPORTER_TEST_DIR, "12x12-array-incomplete-blocks.astc")));
    CORRADE_COMPARE(importer->image3DCount(), 1);

    Containers::Optional<ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlag3D::Array);
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc12x12RGBAUnorm);
    CORRADE_COMPARE(image->size(), (Vector3i{27, 27, 2}));
    CORRADE_COMPARE(image->data().size(), 3*3*2*128/8); /* 3x3x2 blocks */
    /* Verify just a small prefix and suffix to be sure the data got copied */
    CORRADE_COMPARE_AS(image->data().prefix(4), Containers::array({
        '\xb1', '\xe8', '\xd3', '\x91',
    }), TestSuite::Compare::Container);
    /** @todo suffix() once it takes N last bytes */
    CORRADE_COMPARE_AS(image->data().exceptPrefix(image->data().size() - 4), Containers::array({
        '\x76', '\x7a', '\xfc', '\xad'
    }), TestSuite::Compare::Container);
}

void AstcImporterTest::threeDimensions() {
    auto&& data = FormatData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");
    if(data.format)
        importer->configuration().setValue("format", data.format);
    else
        CORRADE_COMPARE(importer->configuration().value("format"), "linear");
    if(data.assumeYUpZBackward)
        importer->configuration().setValue("assumeYUpZBackward", *data.assumeYUpZBackward);
    else
        CORRADE_COMPARE(importer->configuration().value("assumeYUpZBackward"), "false");

    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASTCIMPORTER_TEST_DIR, "3x3x3.astc")));
    }
    CORRADE_COMPARE(out.str(), data.message);
    CORRADE_COMPARE(importer->image3DCount(), 1);

    Containers::Optional<ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->flags(), ImageFlags3D{});
    CORRADE_COMPARE(image->compressedFormat(), data.expectedFormat3D);
    CORRADE_COMPARE(image->size(), (Vector3i{27, 27, 3}));
    CORRADE_COMPARE(image->data().size(), 9*9*1*128/8); /* 9x9x1 blocks */
    /* Verify just a small prefix and suffix to be sure the data got copied */
    CORRADE_COMPARE_AS(image->data().prefix(4), Containers::array({
        '\x06', '\x08', '\x80', '\x35',
    }), TestSuite::Compare::Container);
    /** @todo suffix() once it takes N last bytes */
    CORRADE_COMPARE_AS(image->data().exceptPrefix(image->data().size() - 4), Containers::array({
        '\xdf', '\x00', '\x40', '\x47'
    }), TestSuite::Compare::Container);
}

void AstcImporterTest::fileTooLong2D() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");
    /* Suppress the other warning so we have just the one we're looking for */
    importer->configuration().setValue("assumeYUpZBackward", true);

    /* Add some extra stuff at the end of the file */
    Containers::Optional<Containers::String> data = Utility::Path::readString(Utility::Path::join(ASTCIMPORTER_TEST_DIR, "8x8.astc"));
    CORRADE_VERIFY(data);
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openData(*data + "HAHA"));
    }
    CORRADE_COMPARE(out.str(), "Trade::AstcImporter::openData(): ignoring 4 extra bytes at the end of file\n");
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->data().size(), 8*4*128/8); /* 8x4 blocks */
    /* The extra data should not be present in the output, having the same
       suffix as in the twoDimensions() case */
    /** @todo suffix() once it takes N last bytes */
    CORRADE_COMPARE_AS(image->data().exceptPrefix(image->data().size() - 4), Containers::array({
        '\xcc', '\x22', '\xdd', '\x33'
    }), TestSuite::Compare::Container);
}

void AstcImporterTest::fileTooLong3D() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");
    /* Suppress the other warning so we have just the one we're looking for */
    importer->configuration().setValue("assumeYUpZBackward", true);

    /* Add some extra stuff at the end of the file */
    Containers::Optional<Containers::String> data = Utility::Path::readString(Utility::Path::join(ASTCIMPORTER_TEST_DIR, "3x3x3.astc"));
    CORRADE_VERIFY(data);
    std::ostringstream out;
    {
        Warning redirectWarning{&out};
        CORRADE_VERIFY(importer->openData(*data + "HAHA"));
    }
    CORRADE_COMPARE(out.str(), "Trade::AstcImporter::openData(): ignoring 4 extra bytes at the end of file\n");
    CORRADE_COMPARE(importer->image3DCount(), 1);

    Containers::Optional<ImageData3D> image = importer->image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->data().size(), 9*9*1*128/8); /* 9x9x1 blocks */
    /* The extra data should not be present in the output, having the same
       suffix as in the threeDimensions() case */
    /** @todo suffix() once it takes N last bytes */
    CORRADE_COMPARE_AS(image->data().exceptPrefix(image->data().size() - 4), Containers::array({
        '\xdf', '\x00', '\x40', '\x47'
    }), TestSuite::Compare::Container);
}

void AstcImporterTest::openMemory() {
    /* same as (a subset of) twoDimensions() except that it uses openData() &
       openMemory() instead of openFile() to test data copying on import */

    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");
    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(ASTCIMPORTER_TEST_DIR, "8x8.astc"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*importer, *memory));
    CORRADE_COMPARE(importer->image2DCount(), 1);

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->compressedFormat(), CompressedPixelFormat::Astc8x8RGBAUnorm);
    CORRADE_COMPARE(image->size(), (Vector2i{64, 32}));
    CORRADE_COMPARE(image->data()[1], '\x84');
}

void AstcImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASTCIMPORTER_TEST_DIR, "8x8.astc")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASTCIMPORTER_TEST_DIR, "8x8.astc")));

    /* Shouldn't crash, leak or anything */
}

void AstcImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("AstcImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(ASTCIMPORTER_TEST_DIR, "8x8.astc")));

    /* Verify that everything is working the same way on second use */
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{64, 32}));
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{64, 32}));
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::AstcImporterTest)
