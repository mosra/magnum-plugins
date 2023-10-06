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
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Range.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Text/AbstractFont.h>
#include <Magnum/Text/AbstractGlyphCache.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Text { namespace Test { namespace {

struct FreeTypeFontTest: TestSuite::Tester {
    explicit FreeTypeFontTest();

    void empty();
    void invalid();

    void properties();
    void layout();
    void layoutNoGlyphsInCache();
    void layoutNoFontInCache();
    void layoutArrayCache();

    void fillGlyphCache();
    void fillGlyphCacheIncremental();
    void fillGlyphCacheArray();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractFont> _manager{"nonexistent"};

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<Trade::AbstractImporter> _importerManager;
};

const struct {
    const char* name;
    const char* string;
} LayoutData[]{
    {"", "Wave"},
    {"UTF-8", "Wavě"},
};

const struct {
    const char* name;
    const char* characters;
} FillGlyphCacheData[]{
    {"",
        /* Including also UTF-8 characters to be sure they're handled
           properly */
        "abcdefghijklmnopqrstuvwxyzěšč"},
    {"shuffled order",
        "mvxěipbryzdhfnqlčjšswutokeacg"},
    {"duplicates",
        "mvexěipbbrzzyčbjzdgšhhfnqljswutokeakcg"},
    {"characters not in font",
        /* ☃ */
        "abcdefghijkl\xe2\x98\x83mnopqrstuvwxyzěšč"},
};

FreeTypeFontTest::FreeTypeFontTest() {
    addTests({&FreeTypeFontTest::empty,
              &FreeTypeFontTest::invalid,

              &FreeTypeFontTest::properties});

    addInstancedTests({&FreeTypeFontTest::layout},
        Containers::arraySize(LayoutData));

    addTests({&FreeTypeFontTest::layoutNoGlyphsInCache,
              &FreeTypeFontTest::layoutNoFontInCache,
              &FreeTypeFontTest::layoutArrayCache});

    addInstancedTests({&FreeTypeFontTest::fillGlyphCache},
        Containers::arraySize(FillGlyphCacheData));

    addTests({&FreeTypeFontTest::fillGlyphCacheIncremental,
              &FreeTypeFontTest::fillGlyphCacheArray});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef FREETYPEFONT_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(FREETYPEFONT_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Pull in the AnyImageImporter dependency for image comparison */
    _importerManager.load("AnyImageImporter");
    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    _importerManager.setPluginDirectory({});
    #endif
    /* Load StbImageImporter from the build tree, if defined. Otherwise it's
       static and already loaded. */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void FreeTypeFontTest::empty() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("FreeTypeFont");

    std::ostringstream out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!font->openData({&a, 0}, 16.0f));
    CORRADE_COMPARE(out.str(), "Text::FreeTypeFont::openData(): failed to open the font: 6\n");
}

void FreeTypeFontTest::invalid() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("FreeTypeFont");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!font->openData("Oxygen.ttf", 16.0f));
    CORRADE_COMPARE(out.str(), "Text::FreeTypeFont::openData(): failed to open the font: 85\n");
}

void FreeTypeFontTest::properties() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("FreeTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));
    CORRADE_COMPARE(font->size(), 16.0f);
    CORRADE_COMPARE(font->ascent(), 15.0f);
    CORRADE_COMPARE(font->descent(), -4.0f);
    CORRADE_COMPARE(font->lineHeight(), 19.0f);
    CORRADE_COMPARE(font->glyphCount(), 671);
    CORRADE_COMPARE(font->glyphId(U'W'), 58);
    CORRADE_COMPARE(font->glyphSize(58), Vector2(18.0f, 12.0f));
    CORRADE_COMPARE(font->glyphAdvance(58), Vector2(17.0f, 0.0f));
}

void FreeTypeFontTest::layout() {
    auto&& data = LayoutData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractFont> font = _manager.instantiate("FreeTypeFont");
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

    /* 'W' */
    CORRADE_COMPARE(layouter->renderGlyph(0, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{{0.78125f, 1.0625f}, {1.28125f, 4.8125f}},
                         Range2D{{0, 0.03125f}, {0.0625f, 0.5f}}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.53125f, 0.0f));

    /* 'a' (not in cache) */
    CORRADE_COMPARE(layouter->renderGlyph(1, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.25f, 0.0f));

    /* 'v' (not in cache) */
    CORRADE_COMPARE(layouter->renderGlyph(2, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.25f, 0.0f));

    /* 'e' or 'ě' */
    CORRADE_COMPARE(layouter->renderGlyph(3, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{{0.78125f, 0.375f}, {2.28125f, 1.25f}},
                         Range2D{{0.0625f, 0.015625f}, {0.25f, 0.125f}}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.28125f, 0.0f));
}

void FreeTypeFontTest::layoutNoGlyphsInCache() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("FreeTypeFont");
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
    CORRADE_COMPARE(cursorPosition, Vector2(0.53125f, 0.0f));

    CORRADE_COMPARE(layouter->renderGlyph(1, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.25f, 0.0f));

    CORRADE_COMPARE(layouter->renderGlyph(2, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.25f, 0.0f));

    CORRADE_COMPARE(layouter->renderGlyph(3, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.28125f, 0.0f));
}

void FreeTypeFontTest::layoutNoFontInCache() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("FreeTypeFont");
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
    CORRADE_COMPARE(out.str(), "Text::FreeTypeFont::layout(): font not found among 1 fonts in passed glyph cache\n");
}

void FreeTypeFontTest::layoutArrayCache() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("FreeTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct: AbstractGlyphCache {
        using AbstractGlyphCache::AbstractGlyphCache;

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i&, const ImageView2D&) override {}
    } cache{PixelFormat::R8Unorm, {256, 128, 3}};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!font->layout(cache, 0.5f, "Wave"));
    CORRADE_COMPARE(out.str(), "Text::FreeTypeFont::layout(): array glyph caches are not supported\n");
}

void FreeTypeFontTest::fillGlyphCache() {
    auto&& data = FillGlyphCacheData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Ideally this would be tested at least partially without the image, but
       adding extra logic for that would risk that the image might accidentally
       not get checked at all */
    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractFont> font = _manager.instantiate("FreeTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct GlyphCache: AbstractGlyphCache {
        explicit GlyphCache(PluginManager::Manager<Trade::AbstractImporter>& importerManager, PixelFormat format, const Vector2i& size): AbstractGlyphCache{format, size}, importerManager(importerManager) {}

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i& offset, const ImageView2D& image) override {
            /* The passed image is just the filled subset, compare the whole
               thing for more predictable results */
            CORRADE_COMPARE(offset, Vector2i{});
            CORRADE_COMPARE(image.size(), (Vector2i{64, 46}));
            CORRADE_COMPARE_WITH(this->image().pixels<UnsignedByte>()[0],
                Utility::Path::join(FREETYPEFONT_TEST_DIR, "glyph-cache.png"),
                DebugTools::CompareImageToFile{importerManager});
            called = true;
        }

        bool called = false;
        PluginManager::Manager<Trade::AbstractImporter>& importerManager;
    } cache{_importerManager, PixelFormat::R8Unorm, Vector2i{64}};

    /* Should call doSetImage() above, which then performs image comparison */
    font->fillGlyphCache(cache, data.characters);
    CORRADE_VERIFY(cache.called);

    /* The font should associate itself with the cache */
    CORRADE_COMPARE(cache.fontCount(), 1);
    CORRADE_COMPARE(cache.findFont(font.get()), 0);

    /* 26 ASCII characters, 3 UTF-8 ones + one "not found" glyph, and one
       invalid glyph from the cache itself. The count should be the same in all
       cases as the input is deduplicated and characters not present in the
       font get substituted for glyph 0. */
    CORRADE_COMPARE(cache.glyphCount(), 26 + 3 + 1 + 1);

    /* Check positions of a few select glyphs. They should all retain the same
       position regardless of how the input is shuffled. */

    /* Invalid glyph in the cache is deliberately not changed as that'd cause
       a mess if multiple fonts would each want to set its own */
    CORRADE_COMPARE(cache.glyph(0), Containers::triple(
        Vector2i{},
        0,
        Range2Di{}));
    /* Invalid glyph */
    CORRADE_COMPARE(cache.glyph(0, 0), Containers::triple(
        Vector2i{},
        0,
        Range2Di{{59, 26}, {64, 37}}));
    /* Above the baseline */
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('k')), Containers::triple(
        Vector2i{},
        0,
        Range2Di{{29, 14}, {37, 27}}));
    /* Below the baseline */
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('g')), Containers::triple(
        Vector2i{0, -4},
        0,
        Range2Di{{48, 0}, {57, 13}}));
    /* UTF-8 */
    UnsignedInt sId = font->glyphId(
        /* MSVC (but not clang-cl) doesn't support UTF-8 in char32_t literals
           but it does it regular strings. Still a problem in MSVC 2022, what a
           trash fire, can't you just give up on those codepage insanities
           already, ffs?! */
        #if defined(CORRADE_TARGET_MSVC) && !defined(CORRADE_TARGET_CLANG)
        U'\u0161'
        #else
        U'š'
        #endif
    );
    CORRADE_COMPARE(cache.glyph(0, sId), Containers::triple(
        Vector2i{},
        0,
        Range2Di{{22, 0}, {30, 14}}));
}

void FreeTypeFontTest::fillGlyphCacheIncremental() {
    /* Ideally this would be tested at least partially without the image, but
       adding extra logic for that would risk that the image might accidentally
       not get checked at all */
    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractFont> font = _manager.instantiate("FreeTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct GlyphCache: AbstractGlyphCache {
        explicit GlyphCache(PluginManager::Manager<Trade::AbstractImporter>& importerManager, PixelFormat format, const Vector2i& size): AbstractGlyphCache{format, size}, importerManager(importerManager) {}

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i& offset, const ImageView2D& image) override {
            /* The passed image is just the filled subset, compare the whole
               thing for more predictable results */
            if(called == 0) {
                CORRADE_COMPARE(offset, Vector2i{});
                CORRADE_COMPARE(image.size(), (Vector2i{64, 37}));
            } else if(called == 1) {
                CORRADE_COMPARE(offset, (Vector2i{0, 26}));
                CORRADE_COMPARE(image.size(), (Vector2i{61, 20}));
                CORRADE_COMPARE_WITH(this->image().pixels<UnsignedByte>()[0],
                    Utility::Path::join(FREETYPEFONT_TEST_DIR, "glyph-cache.png"),
                    DebugTools::CompareImageToFile{importerManager});
            } else CORRADE_FAIL("This shouldn't get called more than twice");
            ++called;
        }

        Int called = 0;
        PluginManager::Manager<Trade::AbstractImporter>& importerManager;
    } cache{_importerManager, PixelFormat::R8Unorm, Vector2i{64}};

    /* First call with the bottom half of the glyph cache until the invalid
       glyph */
    font->fillGlyphCache(cache, "jěčšbdghpqkylfti");
    CORRADE_COMPARE(cache.called, 1);

    /* The font should associate itself with the cache now */
    CORRADE_COMPARE(cache.fontCount(), 1);
    CORRADE_COMPARE(cache.findFont(font.get()), 0);

    /* 17 characters + one global invalid glyph */
    CORRADE_COMPARE(cache.glyphCount(), 17 + 1);

    /* Second call with the rest */
    font->fillGlyphCache(cache, "mwovenuacsxzr");
    CORRADE_COMPARE(cache.called, 2);

    /* The font should not be added again */
    CORRADE_COMPARE(cache.fontCount(), 1);

    /* There's now all glyphs like in fillGlyphCache() */
    CORRADE_COMPARE(cache.glyphCount(), 26 + 3 + 1 + 1);

    /* Positions of the glyphs should be the same as in fillGlyphCache() */
    CORRADE_COMPARE(cache.glyph(0), Containers::triple(
        Vector2i{},
        0,
        Range2Di{}));
    CORRADE_COMPARE(cache.glyph(0, 0), Containers::triple(
        Vector2i{},
        0,
        Range2Di{{59, 26}, {64, 37}}));
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('k')), Containers::triple(
        Vector2i{},
        0,
        Range2Di{{29, 14}, {37, 27}}));
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('g')), Containers::triple(
        Vector2i{0, -4},
        0,
        Range2Di{{48, 0}, {57, 13}}));
    UnsignedInt sId = font->glyphId(
        /* MSVC (but not clang-cl) doesn't support UTF-8 in char32_t literals
           but it does it regular strings. Still a problem in MSVC 2022, what a
           trash fire, can't you just give up on those codepage insanities
           already, ffs?! */
        #if defined(CORRADE_TARGET_MSVC) && !defined(CORRADE_TARGET_CLANG)
        U'\u0161'
        #else
        U'š'
        #endif
    );
    CORRADE_COMPARE(cache.glyph(0, sId), Containers::triple(
        Vector2i{},
        0,
        Range2Di{{22, 0}, {30, 14}}));
}

void FreeTypeFontTest::fillGlyphCacheArray() {
    /* Ideally this would be tested at least partially without the image, but
       adding extra logic for that would risk that the image might accidentally
       not get checked at all */
    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractFont> font = _manager.instantiate("FreeTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct GlyphCache: AbstractGlyphCache {
        explicit GlyphCache(PluginManager::Manager<Trade::AbstractImporter>& importerManager, PixelFormat format, const Vector3i& size): AbstractGlyphCache{format, size}, importerManager(importerManager) {}

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector3i& offset, const ImageView3D& image) override {
            /* The passed image is just the filled subset, compare the whole
               thing for more predictable results */
            CORRADE_COMPARE(offset, Vector3i{});
            CORRADE_COMPARE(image.size(), (Vector3i{48, 48, 2}));
            CORRADE_COMPARE_WITH(this->image().pixels<UnsignedByte>()[0],
                Utility::Path::join(FREETYPEFONT_TEST_DIR, "glyph-cache-array0.png"),
                DebugTools::CompareImageToFile{importerManager});
            CORRADE_COMPARE_WITH(this->image().pixels<UnsignedByte>()[1],
                Utility::Path::join(FREETYPEFONT_TEST_DIR, "glyph-cache-array1.png"),
                DebugTools::CompareImageToFile{importerManager});
            called = true;
        }

        bool called = false;
        PluginManager::Manager<Trade::AbstractImporter>& importerManager;
    } cache{_importerManager, PixelFormat::R8Unorm, {48, 48, 2}};

    /* Should call doSetImage() above, which then performs image comparison */
    font->fillGlyphCache(cache, "abcdefghijklmnopqrstuvwxyzěšč");
    CORRADE_VERIFY(cache.called);

    /* The font should associate itself with the cache */
    CORRADE_COMPARE(cache.fontCount(), 1);
    CORRADE_COMPARE(cache.findFont(font.get()), 0);

    /* Same as in fillGlyphCache() */
    CORRADE_COMPARE(cache.glyphCount(), 26 + 3 + 1 + 1);

    /* Positions are spread across two layers now */
    CORRADE_COMPARE(cache.glyph(0), Containers::triple(
        Vector2i{},
        0,
        Range2Di{}));
    /* Invalid glyph */
    CORRADE_COMPARE(cache.glyph(0, 0), Containers::triple(
        Vector2i{},
        0,
        Range2Di{{15, 27}, {20, 38}}));
    /* First layer */
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('g')), Containers::triple(
        Vector2i{0, -4},
        0,
        Range2Di{{39, 13}, {48, 26}}));
    /* Second layer */
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('n')), Containers::triple(
        Vector2i{0, 0},
        1,
        Range2Di{{0, 0}, {9, 9}}));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Text::Test::FreeTypeFontTest)
