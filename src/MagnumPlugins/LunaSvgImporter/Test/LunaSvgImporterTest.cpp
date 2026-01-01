/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
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

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

using namespace Containers::Literals;

struct LunaSvgImporterTest: TestSuite::Tester {
    explicit LunaSvgImporterTest();

    void invalid();
    void svgz();

    void load();
    void loadPremultipliedLinear();
    void loadInvalidAlphaMode();
    void externalImageEmbedded();
    void externalImage();

    void openTwice();
    void importTwice();

    /* Needs to load AnyImageImporter from a system-wide location */
    PluginManager::Manager<AbstractImporter> _manager;
};

const struct {
    const char* name;
    Containers::Optional<Float> dpi;
    const char* expected;
    Float maxThreshold, meanThreshold;
} LoadData[]{
    {"*.svg", {}, "file.png", 8.75f, 0.138f},
    {"*.svg, 48 DPI", 48.0f, "file-48dpi.png", 3.75f, 0.127f},
    {"*.svg, 133.6 DPI", 133.6f, "file-133dpi.png", 128.25f, 1.4999f},
};

LunaSvgImporterTest::LunaSvgImporterTest() {
    addTests({&LunaSvgImporterTest::invalid});

    addInstancedTests({&LunaSvgImporterTest::load},
        Containers::arraySize(LoadData));

    addTests({&LunaSvgImporterTest::loadPremultipliedLinear,
              &LunaSvgImporterTest::loadInvalidAlphaMode,
              &LunaSvgImporterTest::svgz,

              &LunaSvgImporterTest::externalImageEmbedded,
              &LunaSvgImporterTest::externalImage,

              &LunaSvgImporterTest::openTwice,
              &LunaSvgImporterTest::importTwice});

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
    #ifdef LUNASVGIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(LUNASVGIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

using namespace Math::Literals;

void LunaSvgImporterTest::invalid() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("LunaSvgImporter");

    /* There's no error reporting, so the message is always the same */

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openData(""));
    CORRADE_COMPARE(out, "Trade::LunaSvgImporter::openData(): parsing failed\n");
}

void LunaSvgImporterTest::svgz() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("LunaSvgImporter");

    /* Just to verify that svgz files aren't supported */

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "file.svgz")));
    CORRADE_COMPARE(out, "Trade::LunaSvgImporter::openData(): parsing failed\n");
}

void LunaSvgImporterTest::load() {
    auto&& data = LoadData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("LunaSvgImporter");
    if(data.dpi)
        importer->configuration().setValue("dpi", *data.dpi);

    /* Input file same as in ResvgImporterTest, see that test for details. The
       thresholds are different here and closer to the Inkscape-produced ground
       truth. */
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "file.svg")));

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
           semi-transparent circle. LunaSvg produces premultiplied output so it's
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

void LunaSvgImporterTest::loadPremultipliedLinear() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("LunaSvgImporter");

    /* Like load(), but disabling undoing of the alpha premultiplication and
       comparing that to manually (wrongly) premultiplied ground truth image */

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
        (DebugTools::CompareImage{2.75f, 0.069f}));
}

void LunaSvgImporterTest::loadInvalidAlphaMode() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("LunaSvgImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "file.svg")));

    /* This value is supported by PngImporter but not this plugin, as it'd have
       to do the unpremultiplication and then a correct sRGB-aware
       premultiplication. Better to offload that to a reusable utility. */
    importer->configuration().setValue("alphaMode", "premultiplied");

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->image2D(0));
    CORRADE_COMPARE(out, "Trade::LunaSvgImporter::image2D(): expected alphaMode to be either empty or premultipliedLinear but got premultiplied\n");
}

void LunaSvgImporterTest::externalImageEmbedded() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("LunaSvgImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "external-embedded.svg")));

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

       Compared to ResvgImporter, the file matches the input *exactly*, which
       is nice. */
    CORRADE_COMPARE_WITH(image->mutablePixels<Color4ub>().slice(&Color4ub::rgb),
        Utility::Path::join(RESVGIMPORTER_TEST_DIR, "rgb.png"),
        DebugTools::CompareImageToFile{_manager});
}

void LunaSvgImporterTest::externalImage() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("LunaSvgImporter");

    {
        CORRADE_EXPECT_FAIL("LunaSVG doesn't fail with an error if an unknown file is referenced, silently ignores the reference instead.");
        CORRADE_VERIFY(!importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "external.svg")));
    }

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);

    /* Like with ResvgImporter, the output is sometimes all #00000000 and
       sometimes all #000000ff, so I have to check just for the RGB part. I
       wonder how it happens that a C++ code and Rust code suffers from the
       same bug. */
    const char zeros[3*2*4]{};
    CORRADE_COMPARE_AS(image->mutablePixels<Color4ub>().slice(&Color4ub::rgb),
        (ImageView2D{PixelFormat::RGB8Unorm, {3, 2}, zeros}),
        DebugTools::CompareImage);
}

void LunaSvgImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("LunaSvgImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "file.svg")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(RESVGIMPORTER_TEST_DIR, "file.svg")));

    /* Shouldn't crash, leak or anything */
}

void LunaSvgImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("LunaSvgImporter");
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

CORRADE_TEST_MAIN(Magnum::Trade::Test::LunaSvgImporterTest)
