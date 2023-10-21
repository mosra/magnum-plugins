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
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Text/AbstractFont.h>
#include <Magnum/Text/AbstractGlyphCache.h>
#include <Magnum/Text/AbstractShaper.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Text { namespace Test { namespace {

struct StbTrueTypeFontTest: TestSuite::Tester {
    explicit StbTrueTypeFontTest();

    void empty();
    void invalid();

    void properties();

    void shape();
    void shapeEmpty();
    void shaperReuse();

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
    UnsignedInt eGlyphId;
    UnsignedInt begin, end;
} ShapeData[]{
    {"", "Wave", 72, 0, ~UnsignedInt{}},
    {"substring", "haWavefefe", 72, 2, 6},
    {"UTF-8", "Wavě", 220, 0, ~UnsignedInt{}},
    {"UTF-8 substring", "haWavěfefe", 220, 2, 7},
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

StbTrueTypeFontTest::StbTrueTypeFontTest() {
    addTests({&StbTrueTypeFontTest::empty,
              &StbTrueTypeFontTest::invalid,

              &StbTrueTypeFontTest::properties});

    addInstancedTests({&StbTrueTypeFontTest::shape},
        Containers::arraySize(ShapeData));

    addTests({&StbTrueTypeFontTest::shapeEmpty,
              &StbTrueTypeFontTest::shaperReuse});

    addInstancedTests({&StbTrueTypeFontTest::fillGlyphCache},
        Containers::arraySize(FillGlyphCacheData));

    addTests({&StbTrueTypeFontTest::fillGlyphCacheIncremental,
              &StbTrueTypeFontTest::fillGlyphCacheArray});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STBTRUETYPEFONT_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBTRUETYPEFONT_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
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

void StbTrueTypeFontTest::empty() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");

    std::ostringstream out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!font->openData({&a, 0}, 16.0f));
    CORRADE_COMPARE(out.str(), "Text::StbTrueTypeFont::openData(): the file is empty\n");
}

void StbTrueTypeFontTest::invalid() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!font->openData("Oxygen.ttf", 16.0f));
    CORRADE_COMPARE(out.str(), "Text::StbTrueTypeFont::openData(): can't get offset of the first font\n");
}

void StbTrueTypeFontTest::properties() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    CORRADE_COMPARE(font->size(), 16.0f);
    CORRADE_COMPARE(font->glyphCount(), 671);
    CORRADE_COMPARE(font->glyphId(U'W'), 58);

    /* Compared to FreeType, StbTrueType has slightly larger glyphs which makes
       the test values quite different but the actual visual output isn't that
       different. I suppose this is due to a lack of hinting in the
       implementation. Best visible it is in the glyph cache output -- the
       characters look mostly the same but occupy more space. */

    CORRADE_COMPARE(font->ascent(), 17.0112f);
    CORRADE_COMPARE(font->descent(), -4.32215f);
    CORRADE_COMPARE(font->lineHeight(), 21.3333f);
    CORRADE_COMPARE(font->glyphSize(58), Vector2(21.0f, 14.0f));
    CORRADE_COMPARE(font->glyphAdvance(58), Vector2(19.0694f, 0.0f));
}

void StbTrueTypeFontTest::shape() {
    auto&& data = ShapeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    CORRADE_COMPARE(shaper->shape(data.string, data.begin, data.end), 4);

    UnsignedInt ids[4];
    Vector2 offsets[4];
    Vector2 advances[4];
    shaper->glyphIdsInto(ids);
    shaper->glyphOffsetsAdvancesInto(offsets, advances);
    CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
        58u,            /* 'W' */
        68u,            /* 'a' */
        89u,            /* 'v' */
        data.eGlyphId   /* 'e' or 'ě' */
    }), TestSuite::Compare::Container);
    /* There are no glyph-specific offsets here */
    CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
        {}, {}, {}, {}
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
        {19.0694f, 0.0f},
        {9.45861f, 0.0f},
        {9.27069f, 0.0f},
        {9.55705f, 0.0f}
    }), TestSuite::Compare::Container);
}

void StbTrueTypeFontTest::shapeEmpty() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    /* Shouldn't crash or do anything rogue */
    CORRADE_COMPARE(shaper->shape("Wave", 2, 2), 0);
}

void StbTrueTypeFontTest::shaperReuse() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    /* Empty text */
    {
        CORRADE_COMPARE(shaper->shape("Wave", 2, 2), 0);

    /* Short text. Empty shape shouldn't have caused any broken state. */
    } {
        CORRADE_COMPARE(shaper->shape("We"), 2);
        UnsignedInt ids[2];
        Vector2 offsets[2];
        Vector2 advances[2];
        shaper->glyphIdsInto(ids);
        shaper->glyphOffsetsAdvancesInto(offsets, advances);
        CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
            58u, /* 'W' */
            72u  /* 'e' */
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
            {}, {},
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
            {19.0694f, 0.0f},
            {9.55705f, 0.0f}
        }), TestSuite::Compare::Container);

    /* Long text, same as in shape(), should enlarge the array for it */
    } {
        CORRADE_COMPARE(shaper->shape("Wave"), 4);
        UnsignedInt ids[4];
        Vector2 offsets[4];
        Vector2 advances[4];
        shaper->glyphIdsInto(ids);
        shaper->glyphOffsetsAdvancesInto(offsets, advances);
        CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
            58u, /* 'W' */
            68u, /* 'a' */
            89u, /* 'v' */
            72u  /* 'e' or 'ě' */
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
            {}, {}, {}, {}
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
            {19.0694f, 0.0f},
            {9.45861f, 0.0f},
            {9.27069f, 0.0f},
            {9.55705f, 0.0f}
        }), TestSuite::Compare::Container);

    /* Short text again, should not leave the extra glyphs there */
    } {
        CORRADE_COMPARE(shaper->shape("a"), 1);
        UnsignedInt ids[1];
        Vector2 offsets[1];
        Vector2 advances[1];
        shaper->glyphIdsInto(ids);
        shaper->glyphOffsetsAdvancesInto(offsets, advances);
        CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
            68u,
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
            {},
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
            {9.45861f, 0.0f}
        }), TestSuite::Compare::Container);
    }
}

void StbTrueTypeFontTest::fillGlyphCache() {
    auto&& data = FillGlyphCacheData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Ideally this would be tested at least partially without the image, but
       adding extra logic for that would risk that the image might accidentally
       not get checked at all */
    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct GlyphCache: AbstractGlyphCache {
        explicit GlyphCache(PluginManager::Manager<Trade::AbstractImporter>& importerManager, PixelFormat format, const Vector2i& size): AbstractGlyphCache{format, size}, importerManager(importerManager) {}

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i& offset, const ImageView2D& image) override {
            /* The passed image is just the filled subset, compare the whole
               thing for more predictable results */
            CORRADE_COMPARE(offset, Vector2i{});
            CORRADE_COMPARE(image.size(), (Vector2i{64, 63}));
            CORRADE_COMPARE_WITH(this->image().pixels<UnsignedByte>()[0],
                Utility::Path::join(STBTRUETYPEFONT_TEST_DIR, "glyph-cache.png"),
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
       position regardless of how the input is shuffled. Again, this is
       different from FreeType, most probably due to stb_truetype not
       implementing hinting. */

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
        Range2Di{{56, 31}, {62, 44}}));
    /* Above the baseline */
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('k')), Containers::triple(
        Vector2i{1, 0},
        0,
        Range2Di{{13, 16}, {22, 30}}));
    /* Below the baseline */
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('g')), Containers::triple(
        Vector2i{0, -5},
        0,
        Range2Di{{4, 0}, {14, 16}}));
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
        Vector2i{0, -1},
        0,
        Range2Di{{52, 0}, {60, 16}}));
}

void StbTrueTypeFontTest::fillGlyphCacheIncremental() {
    /* Ideally this would be tested at least partially without the image, but
       adding extra logic for that would risk that the image might accidentally
       not get checked at all */
    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct GlyphCache: AbstractGlyphCache {
        explicit GlyphCache(PluginManager::Manager<Trade::AbstractImporter>& importerManager, PixelFormat format, const Vector2i& size): AbstractGlyphCache{format, size}, importerManager(importerManager) {}

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i& offset, const ImageView2D& image) override {
            /* The passed image is just the filled subset, compare the whole
               thing for more predictable results */
            if(called == 0) {
                CORRADE_COMPARE(offset, Vector2i{});
                CORRADE_COMPARE(image.size(), (Vector2i{64, 45}));
            } else if(called == 1) {
                CORRADE_COMPARE(offset, (Vector2i{0, 30}));
                CORRADE_COMPARE(image.size(), (Vector2i{60, 33}));
                CORRADE_COMPARE_WITH(this->image().pixels<UnsignedByte>()[0],
                    Utility::Path::join(STBTRUETYPEFONT_TEST_DIR, "glyph-cache.png"),
                    DebugTools::CompareImageToFile{importerManager});
            } else CORRADE_FAIL("This shouldn't get called more than twice");
            ++called;
        }

        Int called = 0;
        PluginManager::Manager<Trade::AbstractImporter>& importerManager;
    } cache{_importerManager, PixelFormat::R8Unorm, Vector2i{64}};

    /* First call with the bottom half of the glyph cache until the invalid
       glyph */
    font->fillGlyphCache(cache, "jgpqčěšdbylhktfi");
    CORRADE_COMPARE(cache.called, 1);

    /* The font should associate itself with the cache now */
    CORRADE_COMPARE(cache.fontCount(), 1);
    CORRADE_COMPARE(cache.findFont(font.get()), 0);

    /* 17 characters + one global invalid glyph */
    CORRADE_COMPARE(cache.glyphCount(), 17 + 1);

    /* Second call with the rest */
    font->fillGlyphCache(cache, "oacesmnuwvxzr");
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
        Range2Di{{56, 31}, {62, 44}}));
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('k')), Containers::triple(
        Vector2i{1, 0},
        0,
        Range2Di{{13, 16}, {22, 30}}));
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('g')), Containers::triple(
        Vector2i{0, -5},
        0,
        Range2Di{{4, 0}, {14, 16}}));
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
        Vector2i{0, -1},
        0,
        Range2Di{{52, 0}, {60, 16}}));
}

void StbTrueTypeFontTest::fillGlyphCacheArray() {
    /* Ideally this would be tested at least partially without the image, but
       adding extra logic for that would risk that the image might accidentally
       not get checked at all */
    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
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
                Utility::Path::join(STBTRUETYPEFONT_TEST_DIR, "glyph-cache-array0.png"),
                DebugTools::CompareImageToFile{importerManager});
            CORRADE_COMPARE_WITH(this->image().pixels<UnsignedByte>()[1],
                Utility::Path::join(STBTRUETYPEFONT_TEST_DIR, "glyph-cache-array1.png"),
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
        Range2Di{{4, 34}, {10, 47}}));
    /* First layer */
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('g')), Containers::triple(
        Vector2i{0, -5},
        0,
        Range2Di{{4, 0}, {14, 16}}));
    /* Second layer */
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('n')), Containers::triple(
        Vector2i{0, 0},
        1,
        Range2Di{{23, 12}, {33, 23}}));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Text::Test::StbTrueTypeFontTest)
