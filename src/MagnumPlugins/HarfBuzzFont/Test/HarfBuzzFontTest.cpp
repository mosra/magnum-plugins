/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/Containers/String.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Text/AbstractFont.h>
#include <Magnum/Text/AbstractGlyphCache.h>
#include <hb.h>

#include "configure.h"

namespace Magnum { namespace Text { namespace Test { namespace {

struct HarfBuzzFontTest: TestSuite::Tester {
    explicit HarfBuzzFontTest();

    void layout();
    void layoutNoGlyphsInCache();
    void layoutNoFontInCache();
    void layoutArrayCache();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractFont> _manager{"nonexistent"};
};

const struct {
    const char* name;
    const char* string;
    Float advanceAfterV, advanceAfterVWeird;
} LayoutData[]{
    {"", "Wave", 0.25f, 0.249512f},
    {"UTF-8", "Wavě", 0.25293f, 0.25293f},
};

HarfBuzzFontTest::HarfBuzzFontTest() {
    addInstancedTests({&HarfBuzzFontTest::layout},
        Containers::arraySize(LayoutData));

    addTests({&HarfBuzzFontTest::layoutNoGlyphsInCache,
              &HarfBuzzFontTest::layoutNoFontInCache,
              &HarfBuzzFontTest::layoutArrayCache});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #if defined(FREETYPEFONT_PLUGIN_FILENAME) && defined(HARFBUZZFONT_PLUGIN_FILENAME)
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(FREETYPEFONT_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(HARFBUZZFONT_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void HarfBuzzFontTest::layout() {
    auto&& data = LayoutData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    /* Fill the cache with some fake glyphs */
    struct: AbstractGlyphCache {
        using AbstractGlyphCache::AbstractGlyphCache;

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i&, const ImageView2D&) override {}
    } cache{PixelFormat::R8Unorm, Vector2i{256}};
    UnsignedInt fontId = cache.addFont(font->glyphCount(), font.get());
    cache.addGlyph(fontId, font->glyphId(U'W'), {25, 34}, {{0, 8}, {16, 128}});
    cache.addGlyph(fontId, font->glyphId(U'e'), {25, 12}, {{16, 4}, {64, 32}});
    /* ě has deliberately the same glyph data as e */
    cache.addGlyph(fontId, font->glyphId(
        /* MSVC (but not clang-cl) doesn't support UTF-8 in char32_t literals
           but it does it regular strings. Still a problem in MSVC 2022, what a
           trash fire, can't you just give up on those codepage insanities
           already, ffs?! */
        #if defined(CORRADE_TARGET_MSVC) && !defined(CORRADE_TARGET_CLANG)
        U'\u011B'
        #else
        U'ě'
        #endif
    ), {25, 12}, {{16, 4}, {64, 32}});

    Containers::Pointer<AbstractLayouter> layouter = font->layout(cache, 0.5f, data.string);
    CORRADE_VERIFY(layouter);
    CORRADE_COMPARE(layouter->glyphCount(), 4);

    Vector2 cursorPosition;
    Range2D rectangle;

    /* Difference between this and FreeTypeFont should be _only_ in advances */

    /* 'W' */
    CORRADE_COMPARE(layouter->renderGlyph(0, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{{0.78125f, 1.0625f}, {1.28125f, 4.8125f}},
                         Range2D{{0, 0.03125f}, {0.0625f, 0.5f}}));
    /* Versions 3.3.0 and 3.3.1 reported {0.515625f, 0.0f} here, but the change
       is reverted in 3.3.2 again "as it proved problematic". */
    CORRADE_COMPARE(cursorPosition, Vector2(0.51123f, 0.0f));

    /* 'a' (not in cache) */
    CORRADE_COMPARE(layouter->renderGlyph(1, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.258301f, 0.0f));

    /* 'v' (not in cache). HarfBuzz before 1.7 and after 3.1 give 0.25,
       versions between the other; additionally vě has slightly different
       spacing than ve. */
    CORRADE_COMPARE(layouter->renderGlyph(2, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    if((HB_VERSION_MAJOR*100 + HB_VERSION_MINOR < 107) ||
       (HB_VERSION_MAJOR*100 + HB_VERSION_MINOR >= 301))
        CORRADE_COMPARE(cursorPosition, Vector2(data.advanceAfterV, 0.0f));
    else
        CORRADE_COMPARE(cursorPosition, Vector2(data.advanceAfterVWeird, 0.0f));

    /* 'e' or 'ě' */
    CORRADE_COMPARE(layouter->renderGlyph(3, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{{0.78125f, 0.375f}, {2.28125f, 1.25f}},
                         Range2D{{0.0625f, 0.015625f}, {0.25f, 0.125f}}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.260742f, 0.0f));
}

void HarfBuzzFontTest::layoutNoGlyphsInCache() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    /* Fill the cache with some fake glyphs */
    struct: AbstractGlyphCache {
        using AbstractGlyphCache::AbstractGlyphCache;

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i&, const ImageView2D&) override {}
    } cache{PixelFormat::R8Unorm, Vector2i{256}};

    /* Add a font that is associated with this one but fillGlyphCache() was
       actually not called for it */
    cache.addFont(font->glyphCount(), font.get());

    Containers::Pointer<AbstractLayouter> layouter = font->layout(cache, 0.5f, "Wave");
    CORRADE_VERIFY(layouter);
    CORRADE_COMPARE(layouter->glyphCount(), 4);

    Vector2 cursorPosition;
    Range2D rectangle;

    /* Compared to layout(), only the cursor position gets updated, everything
       else falls back to the invalid glyph */

    CORRADE_COMPARE(layouter->renderGlyph(0, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    /* Versions 3.3.0 and 3.3.1 reported {0.515625f, 0.0f} here, but the change
       is reverted in 3.3.2 again "as it proved problematic". */
    CORRADE_COMPARE(cursorPosition, Vector2(0.51123f, 0.0f));

    CORRADE_COMPARE(layouter->renderGlyph(1, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.258301f, 0.0f));

    CORRADE_COMPARE(layouter->renderGlyph(2, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    if((HB_VERSION_MAJOR*100 + HB_VERSION_MINOR < 107) ||
       (HB_VERSION_MAJOR*100 + HB_VERSION_MINOR >= 301))
        CORRADE_COMPARE(cursorPosition, Vector2(0.25f, 0.0f));
    else
        CORRADE_COMPARE(cursorPosition, Vector2(0.249512f, 0.0f));

    CORRADE_COMPARE(layouter->renderGlyph(3, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.260742f, 0.0f));
}

void HarfBuzzFontTest::layoutNoFontInCache() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct: AbstractGlyphCache {
        using AbstractGlyphCache::AbstractGlyphCache;

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i&, const ImageView2D&) override {}
    } cache{PixelFormat::R8Unorm, Vector2i{256}};

    /* Add a font that isn't associated with this one */
    cache.addFont(15);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!font->layout(cache, 0.5f, "Wave"));
    CORRADE_COMPARE(out.str(), "Text::HarfBuzzFont::layout(): font not found among 1 fonts in passed glyph cache\n");
}

void HarfBuzzFontTest::layoutArrayCache() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct: AbstractGlyphCache {
        using AbstractGlyphCache::AbstractGlyphCache;

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i&, const ImageView2D&) override {}
    } cache{PixelFormat::R8Unorm, {256, 128, 3}};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!font->layout(cache, 0.5f, "Wave"));
    CORRADE_COMPARE(out.str(), "Text::HarfBuzzFont::layout(): array glyph caches are not supported\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::Text::Test::HarfBuzzFontTest)
