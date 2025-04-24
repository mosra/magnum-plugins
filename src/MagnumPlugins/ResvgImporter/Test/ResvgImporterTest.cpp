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
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/String.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include <resvg.h> /* RESVG_MAJOR_VERSION, RESVG_MINOR_VERSION */

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

using namespace Containers::Literals;

struct ResvgImporterTest: TestSuite::Tester {
    explicit ResvgImporterTest();

    void invalidData();
    void invalidFile();
    void tooLarge();

    void load();
    void loadPremultipliedLinear();
    void loadInvalidAlphaMode();
    void externalImage();
    void externalImageNotFound();
    void externalImageFromData();
    /* There are also external fonts, but given how external image loading
       "works" I don't even want to try. */

    void openTwice();
    void importTwice();

    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

const struct {
    const char* name;
    Containers::StringView data;
    const char* message;
} InvalidDataData[]{
    {"empty", ""_s, "parsing failed"},
    {"invalid UTF-8", "<\xde\xad\xbe\xef"_s, "not an UTF-8 string"},
    {"invalid GZip", "\x1f\x8b"_s, "malformed GZip"},
    {"invalid SVG size", "<svg version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\" width=\"0\"/>"_s, "invalid SVG size"},
    {"parsing failed", "</svg>"_s, "parsing failed"},
};

const struct {
    const char* name;
    const char* filename;
    const char* message;
} InvalidFileData[]{
    {"empty", "empty.svg", "Trade::ResvgImporter::openData(): parsing failed\n"},
    {"non-existent", "nonexistent.svg", "\nTrade::AbstractImporter::openFile(): cannot open file {}\n"}
};

const struct {
    const char* name;
    Containers::Optional<Float> dpi;
    const char* filename;
    const char* expected;
    Float maxThreshold, meanThreshold;
} LoadData[]{
    /* Saved from Inkscape as "Optimized SVG". It's a 32x24 file but it's
       important that it also has the scale set to 1, so the coordinates are in
       pixels as well. Otherwise it generates random rounding errors. */
    {"*.svg", {}, "file.svg", "file.png", 12.75f, 0.233f},
    /* The above file, imported and saved as "Plain SVGZ". It's larger because
       I suspect the contents aren't optimized in any way, but I want to ensure
       that the usual SVGZ files get opened so I don't fake this with applying
       gzip on the above directly. */
    {"*.svgz", {}, "file.svgz", "file.png", 12.75f, 0.233f},
    {"*.svg, 48 DPI", 48.0f, "file.svg", "file-48dpi.png", 4.0f, 0.1993f},
    /* Verifies that the DPI is queried from the config as a float, and that a
       proper rounding happens on the size. If it does, the file is 45 px wide,
       if not, only 44. */
    {"*.svg, 133.6 DPI", 133.6f, "file.svg", "file-133dpi.png", 111.25f, 1.333f},
};

const struct {
    const char* name;
    const char* filename;
} ExternalImageData[]{
    /* A 3x2 file made in Inkscape with rgb.png imported and scaled over the
       whole canvas. Again it's important to have the document scale set to 1
       to avoid rounding errors. Saved as "Optimized SVG" with "embed images"
       enabled. */
    {"embedded in SVG", "external-embedded.svg"},
    /* Like above, but saved with "embed images" disabled. I.e., save this one
       first and only then make the above out of it. */
    {"external", "external.svg"},
};

ResvgImporterTest::ResvgImporterTest() {
    addInstancedTests({&ResvgImporterTest::invalidData},
        Containers::arraySize(InvalidDataData));

    addInstancedTests({&ResvgImporterTest::invalidFile},
        Containers::arraySize(InvalidFileData));

    addTests({&ResvgImporterTest::tooLarge});

    addInstancedTests({&ResvgImporterTest::load},
        Containers::arraySize(LoadData));

    addTests({&ResvgImporterTest::loadPremultipliedLinear,
              &ResvgImporterTest::loadInvalidAlphaMode});

    addInstancedTests({&ResvgImporterTest::externalImage},
        Containers::arraySize(ExternalImageData));

    addTests({&ResvgImporterTest::externalImageNotFound,
              &ResvgImporterTest::externalImageFromData,

              &ResvgImporterTest::openTwice,
              &ResvgImporterTest::importTwice});

    /* Pull in the AnyImageImporter dependency for image comparison */
    _manager.load("AnyImageImporter");
    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    _manager.setPluginDirectory({});
    #endif
    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef RESVGIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(RESVGIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

using namespace Math::Literals;

void ResvgImporterTest::invalidData() {
    auto&& data = InvalidDataData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("ResvgImporter");

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(data.data));
    CORRADE_COMPARE(out, Utility::format("Trade::ResvgImporter::openData(): {}\n", data.message));
}

void ResvgImporterTest::invalidFile() {
    auto&& data = InvalidFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("ResvgImporter");

    /* Just to verify that file opening failure is correctly propagated and
       cleaned up as well -- the doOpenFile() needs to be there to save the
       path for external file loading */

    Containers::String filename = Utility::Path::join(RESVGIMPORTER_TEST_DIR, data.filename);

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(filename));
    /* There can be an error line from Utility::Path before */
    CORRADE_COMPARE_AS(out,
        Utility::format(data.message, filename),
        TestSuite::Compare::StringHasSuffix);
}

void ResvgImporterTest::tooLarge() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("ResvgImporter");

    /* This is fine (100k elements) */
    CORRADE_VERIFY(importer->openData(Utility::format("<svg version=\"1.1\" viewBox=\"0 0 1 1\" xmlns=\"http://www.w3.org/2000/svg\">{}</svg>", "<g/>"_s*100000)));

    /* 1M not anymore. It should report RESVG_ERROR_ELEMENTS_LIMIT_REACHED
       according to the docs, but it reports the generic semi-useless
       RESVG_ERROR_PARSING_FAILED instead. */
    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(Utility::format("<svg version=\"1.1\" viewBox=\"0 0 1 1\" xmlns=\"http://www.w3.org/2000/svg\">{}</svg>", "<g/>"_s*1000000)));
    CORRADE_COMPARE(out, "Trade::ResvgImporter::openData(): parsing failed\n");
}

void ResvgImporterTest::load() {
    auto&& data = LoadData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("ResvgImporter");
    if(data.dpi)
        importer->configuration().setValue("dpi", *data.dpi);

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    CORRADE_COMPARE_WITH(*image,
        Utility::Path::join(RESVGIMPORTER_TEST_DIR, data.expected),
        (DebugTools::CompareImageToFile{_manager, data.maxThreshold, data.meanThreshold}));

    /* Verify what's easy to miss by just eyeballing, especially various flips
       or color channel swizzles. Only if the DPI override isn't set, otherwise
       the pixel coordinates would be different. */
    if(!data.dpi) {
        /* The format should be RGBA8Unorm (so, not sRGB just yet) */
        CORRADE_COMPARE(image->format(), PixelFormat::RGBA8Unorm);
        /* On the left is an opaque vertical green line (i.e., not a
           rectangle) */
        CORRADE_COMPARE(image->pixels<Color4ub>()[6][6], 0x3bd267_rgb);
        CORRADE_COMPARE(image->pixels<Color4ub>()[18][6], 0x3bd267_rgb);
        /* Bottom right (with Y up) should be a *non-premultiplied*
           semi-transparent circle. Resvg produces premultiplied output so it's
           undone in the code, causing off-by-one differences, so this checks
           with a delta. Is cast (not unpacked) to a floating-point type to
           allow the deltas to work properly. */
        CORRADE_COMPARE_WITH(Color4{image->pixels<Color4ub>()[8][24]},
            Color4{0x2f83cc66_rgba},
            TestSuite::Compare::around(Color4{1.0f}));
        /* The rest is transparent black */
        CORRADE_COMPARE(image->pixels<Color4ub>()[16][24], 0x00000000_rgba);
    }
}

void ResvgImporterTest::loadPremultipliedLinear() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("ResvgImporter");

    /* Like load(), but disabling undoing of the alpha premultiplication and
       comparing that to manually premultiplied ground truth image */

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "file.svg")));

    importer->configuration().setValue("alphaMode", "premultipliedLinear");
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    Containers::Pointer<AbstractImporter> pngImporter = _manager.instantiate("PngImporter");
    CORRADE_VERIFY(pngImporter->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "file.png")));

    Containers::Optional<Trade::ImageData2D> pngImage = pngImporter->image2D(0);
    CORRADE_VERIFY(pngImage);

    /* If this is commented out, the comparison passes for everything except
       the semi-transparent circle bottom right */
    for(Containers::StridedArrayView1D<Color4ub> row: pngImage->mutablePixels<Color4ub>())
        for(Color4ub& i: row)
            i = i.premultiplied();

    CORRADE_COMPARE_WITH(*image,
        *pngImage,
        (DebugTools::CompareImage{3.75f, 0.089f}));
}

void ResvgImporterTest::loadInvalidAlphaMode() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("ResvgImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "file.svg")));

    /* This value is supported by PngImporter but not this plugin, as it'd have
       to do the unpremultiplication and then a correct sRGB-aware
       premultiplication. Better to offload that to a reusable utility. */
    importer->configuration().setValue("alphaMode", "premultiplied");

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out, "Trade::ResvgImporter::image2D(): expected alphaMode to be either empty or premultipliedLinear but got premultiplied\n");
}

void ResvgImporterTest::externalImage() {
    auto&& data = ExternalImageData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("ResvgImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, data.filename)));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    if(_manager.loadState("AnyImageImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("AnyImageImporter plugin not found, cannot test contents");
    if(_manager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test contents");

    /* This file is referenced by the SVG so the output should be the same as
       the referenced file. Well, except that the referenced file is just RGB
       so we compare only the first three channels. (And using mutablePixels()
       instead of pixels() because rgb() on const Color4 doesn't return a
       reference in order to be constexpr, sigh.)

       The file has image-rendering="pixelated" so the output should be exactly
       the same as the input. It's *claimed* to be supported since resvg 0.45:
        https://github.com/linebender/resvg/commit/fc4d020acc4101c7ddd236c51faae7dcddaf0176
       But there's no change compared to 0.35, so first testing with a delta
       that should pass, and then expecting an exact comparison to fail. */
    CORRADE_COMPARE_WITH(image->mutablePixels<Color4ub>().slice(&Color4ub::rgb),
        Utility::Path::join(RESVGIMPORTER_TEST_DIR, "rgb.png"),
        (DebugTools::CompareImageToFile{_manager, 8.67f, 6.8f}));
    {
        #if RESVG_MAJOR_VERSION*1000 + RESVG_MINOR_VERSION >= 45
        CORRADE_EXPECT_FAIL("Resvg 0.45 claims but doesn't actually support image-rendering=\"pixelated\".");
        #else
        CORRADE_EXPECT_FAIL("Resvg" << RESVG_VERSION << "doesn't support image-rendering=\"pixelated\".");
        #endif
        CORRADE_COMPARE_WITH(image->mutablePixels<Color4ub>().slice(&Color4ub::rgb),
            Utility::Path::join(RESVGIMPORTER_TEST_DIR, "rgb.png"),
            DebugTools::CompareImageToFile{_manager});
    }
}

void ResvgImporterTest::externalImageNotFound() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("ResvgImporter");

    /* Jesus, why did I think this would be easy. If the external-notfound.svg
       contains just the image */
    {
        CORRADE_EXPECT_FAIL("Resvg doesn't fail with an error if an unknown file is referenced, silently ignores the reference instead.");
        CORRADE_VERIFY(!importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "external-notfound.svg")));
    }

    /* So guess what, it just gives back an all-zero image */
    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    const char zeros[3*2*4]{};
    CORRADE_COMPARE_AS(*image,
        (ImageView2D{PixelFormat::RGBA8Unorm, {3, 2}, zeros}),
        DebugTools::CompareImage);
}

void ResvgImporterTest::externalImageFromData() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("ResvgImporter");

    Containers::Optional<Containers::Array<char>> data = Utility::Path::read(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "external.svg"));
    CORRADE_VERIFY(data);

    /* So, like, if it would fail, we could at least hint to users that SVGs
       with externally referenced files can't be loaded as data. But it doesn't
       so we can't. */
    {
        CORRADE_EXPECT_FAIL("Resvg doesn't fail with an error if a file is referenced but no path is set for it either.");
        CORRADE_VERIFY(!importer->openData(*data));
    }

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    /* So guess what! This time it gives back an all-#000000ff image. Not all
       zero but all black. Well, sometimes. Sometimes it's all zero as well, so
       I'm testing just the RGB values to not have this fail randomly. Is this
       the massively advertised Rust safety at play? */
    const char zeros[3*2*4]{};
    CORRADE_COMPARE_AS(image->mutablePixels<Color4ub>().slice(&Color4ub::rgb),
        (ImageView2D{PixelFormat::RGB8Unorm, {3, 2}, zeros}),
        DebugTools::CompareImage);
}

void ResvgImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("ResvgImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "file.svg")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "file.svg")));

    /* Shouldn't crash, leak or anything */
}

void ResvgImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("ResvgImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "file.svg")));

    /* Verify that everything is working the same way on second use */
    {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{32, 24}));
    } {
        Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
        CORRADE_VERIFY(image);
        CORRADE_COMPARE(image->size(), (Vector2i{32, 24}));
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::ResvgImporterTest)
