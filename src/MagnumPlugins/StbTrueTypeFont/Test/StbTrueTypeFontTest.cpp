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
#include <Corrade/Utility/Unicode.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Text/AbstractFont.h>
#include <Magnum/Text/AbstractGlyphCache.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Text { namespace Test { namespace {

struct StbTrueTypeFontTest: TestSuite::Tester {
    explicit StbTrueTypeFontTest();

    void empty();
    void invalid();

    void properties();
    void layout();
    void fillGlyphCache();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractFont> _manager{"nonexistent"};

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<Trade::AbstractImporter> _importerManager;
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

              &StbTrueTypeFontTest::properties,
              &StbTrueTypeFontTest::layout});

    addInstancedTests({&StbTrueTypeFontTest::fillGlyphCache},
        Containers::arraySize(FillGlyphCacheData));

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

void StbTrueTypeFontTest::layout() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    /* Fill the cache with some fake glyphs */
    struct: AbstractGlyphCache {
        using AbstractGlyphCache::AbstractGlyphCache;

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i&, const ImageView2D&) override {}
    } cache{Vector2i{256}};
    cache.insert(font->glyphId(U'W'), {25, 34}, {{0, 8}, {16, 128}});
    cache.insert(font->glyphId(U'e'), {25, 12}, {{16, 4}, {64, 32}});

    Containers::Pointer<AbstractLayouter> layouter = font->layout(cache, 0.5f, "Wave");
    CORRADE_VERIFY(layouter);
    CORRADE_COMPARE(layouter->glyphCount(), 4);

    Vector2 cursorPosition;
    Range2D rectangle;

    /* Compared to FreeType, StbTrueType has slightly larger glyphs which makes
       the test values quite different but the actual visual output isn't that
       different. I suppose this is due to a lack of hinting in the
       implementation. Best visible it is in the glyph cache output -- the
       characters look mostly the same but occupy more space. */

    /* 'W' */
    CORRADE_COMPARE(layouter->renderGlyph(0, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{{0.78125f, 1.0625f}, {1.28125f, 4.8125f}},
                         Range2D{{0, 0.03125f}, {0.0625f, 0.5f}}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.595917f, 0.0f));

    /* 'a' (not in cache) */
    CORRADE_COMPARE(layouter->renderGlyph(1, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.295582f, 0.0f));

    /* 'v' (not in cache) */
    CORRADE_COMPARE(layouter->renderGlyph(2, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{}, Range2D{}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.289709f, 0.0f));

    /* 'e' */
    CORRADE_COMPARE(layouter->renderGlyph(3, cursorPosition = {}, rectangle),
        Containers::pair(Range2D{{0.78125f, 0.375f}, {2.28125f, 1.25f}},
                         Range2D{{0.0625f, 0.015625f}, {0.25f, 0.125f}}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.298658f, 0.0f));
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
        explicit GlyphCache(PluginManager::Manager<Trade::AbstractImporter>& importerManager, const Vector2i& size): AbstractGlyphCache{size}, importerManager(importerManager) {}

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i& offset, const ImageView2D& image) override {
            CORRADE_COMPARE(offset, Vector2i{});
            CORRADE_COMPARE_WITH(image,
                Utility::Path::join(STBTRUETYPEFONT_TEST_DIR, "glyph-cache.png"),
                DebugTools::CompareImageToFile{importerManager});
            called = true;
        }

        bool called = false;
        PluginManager::Manager<Trade::AbstractImporter>& importerManager;
    } cache{_importerManager, Vector2i{96}};

    /* Should call doSetImage() above, which then performs image comparison */
    font->fillGlyphCache(cache, data.characters);
    CORRADE_VERIFY(cache.called);

    /* 26 ASCII characters, 3 UTF-8 ones + one "not found" glyph. The could
       should be the same in all cases as the input is deduplicated and
       characters not present in the font get substituted for glyph 0. */
    CORRADE_COMPARE(cache.glyphCount(), 26 + 3 + 1);

    /* Check positions of a few select glyphs. They should all retain the same
       position regardless of how the input is shuffled. Again, this is
       different from FreeType, most probably due to stb_truetype not
       implementing hinting. */

    /* Invalid glyph */
    CORRADE_COMPARE(cache[0], std::make_pair(
        Vector2i{}, Range2Di{{}, {6, 13}}));
    /* Above the baseline */
    CORRADE_COMPARE(cache[font->glyphId('k')], std::make_pair(
        Vector2i{1, 0}, Range2Di{{75, 19}, {84, 33}}));
    /* Below the baseline */
    CORRADE_COMPARE(cache[font->glyphId('g')], std::make_pair(
        Vector2i{0, -5}, Range2Di{{15, 19}, {25, 35}}));
    /* UTF-8 */
    CORRADE_COMPARE(cache[font->glyphId(Utility::Unicode::nextChar("š", 0).first())], std::make_pair(
        Vector2i{0, -1}, Range2Di{{75, 76}, {83, 92}}));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Text::Test::StbTrueTypeFontTest)
