/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/DebugStl.h>
#include <Magnum/Text/AbstractFont.h>
#include <Magnum/Text/AbstractGlyphCache.h>

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
};

StbTrueTypeFontTest::StbTrueTypeFontTest() {
    addTests({&StbTrueTypeFontTest::empty,
              &StbTrueTypeFontTest::invalid,

              &StbTrueTypeFontTest::properties,
              &StbTrueTypeFontTest::layout,
              &StbTrueTypeFontTest::fillGlyphCache});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STBTRUETYPEFONT_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBTRUETYPEFONT_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
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
    CORRADE_VERIFY(font->openFile(TTF_FILE, 16.0f));

    CORRADE_COMPARE(font->size(), 16.0f);
    CORRADE_COMPARE(font->glyphId(U'W'), 58);

    {
        CORRADE_EXPECT_FAIL("Font properties don't match FreeType with the same font size.");
        CORRADE_COMPARE(font->ascent(), 15.0f);
        CORRADE_COMPARE(font->descent(), -4.0f);
        CORRADE_COMPARE(font->lineHeight(), 19.0f);
        CORRADE_COMPARE(font->glyphAdvance(58), Vector2(17.0f, 0.0f));
    } {
        /* Test that we are at least consistently wrong */
        CORRADE_COMPARE(font->ascent(), 17.0112f);
        CORRADE_COMPARE(font->descent(), -4.32215f);
        CORRADE_COMPARE(font->lineHeight(), 21.3333f);
        CORRADE_COMPARE(font->glyphAdvance(58), Vector2(19.0694f, 0.0f));
    }
}

struct DummyGlyphCache: AbstractGlyphCache {
    using AbstractGlyphCache::AbstractGlyphCache;

    GlyphCacheFeatures doFeatures() const override { return {}; }
    void doSetImage(const Vector2i&, const ImageView2D&) override {}
};

void StbTrueTypeFontTest::layout() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(TTF_FILE, 16.0f));

    /* Fill the cache with some fake glyphs */
    DummyGlyphCache cache{Vector2i{256}};
    cache.insert(font->glyphId(U'W'), {25, 34}, {{0, 8}, {16, 128}});
    cache.insert(font->glyphId(U'e'), {25, 12}, {{16, 4}, {64, 32}});

    Containers::Pointer<AbstractLayouter> layouter = font->layout(cache, 0.5f, "Wave");
    CORRADE_VERIFY(layouter);
    CORRADE_COMPARE(layouter->glyphCount(), 4);

    Vector2 cursorPosition;
    Range2D rectangle, position, textureCoordinates;

    /* 'W' */
    std::tie(position, textureCoordinates) = layouter->renderGlyph(0, cursorPosition = {}, rectangle);
    CORRADE_COMPARE(position, Range2D({0.78125f, 1.0625f}, {1.28125f, 4.8125f}));
    CORRADE_COMPARE(textureCoordinates, Range2D({0, 0.03125f}, {0.0625f, 0.5f}));
    {
        CORRADE_EXPECT_FAIL("Font properties don't match FreeType with the same font size.");
        CORRADE_COMPARE(cursorPosition, Vector2(0.53125f, 0.0f));
    } {
        /* Test that we are at least consistently wrong */
        CORRADE_COMPARE(cursorPosition, Vector2(0.595917f, 0.0f));
    }

    /* 'a' (not in cache) */
    std::tie(position, textureCoordinates) = layouter->renderGlyph(1, cursorPosition = {}, rectangle);
    CORRADE_COMPARE(position, Range2D());
    CORRADE_COMPARE(textureCoordinates, Range2D());
    {
        CORRADE_EXPECT_FAIL("Font properties don't match FreeType with the same font size.");
        CORRADE_COMPARE(cursorPosition, Vector2(0.25f, 0.0f));
    } {
        /* Test that we are at least consistently wrong */
        CORRADE_COMPARE(cursorPosition, Vector2(0.295582f, 0.0f));
    }

    /* 'v' (not in cache) */
    std::tie(position, textureCoordinates) = layouter->renderGlyph(2, cursorPosition = {}, rectangle);
    CORRADE_COMPARE(position, Range2D());
    CORRADE_COMPARE(textureCoordinates, Range2D());
    {
        CORRADE_EXPECT_FAIL("Font properties don't match FreeType with the same font size.");
        CORRADE_COMPARE(cursorPosition, Vector2(0.25f, 0.0f));
    } {
        /* Test that we are at least consistently wrong */
        CORRADE_COMPARE(cursorPosition, Vector2(0.289709f, 0.0f));
    }

    /* 'e' */
    std::tie(position, textureCoordinates) = layouter->renderGlyph(3, cursorPosition = {}, rectangle);
    CORRADE_COMPARE(position, Range2D({0.78125f, 0.375f}, {2.28125f, 1.25f}));
    CORRADE_COMPARE(textureCoordinates, Range2D({0.0625f, 0.015625f}, {0.25f, 0.125f}));
    {
        CORRADE_EXPECT_FAIL("Font properties don't match FreeType with the same font size.");
        CORRADE_COMPARE(cursorPosition, Vector2(0.28125f, 0.0f));
    } {
        /* Test that we are at least consistently wrong */
        CORRADE_COMPARE(cursorPosition, Vector2(0.298658f, 0.0f));
    }
}

void StbTrueTypeFontTest::fillGlyphCache() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(TTF_FILE, 16.0f));

    /* Just testing that nothing crashes, asserts or errors */
    DummyGlyphCache cache{Vector2i{256}};
    font->fillGlyphCache(cache, "abcdefghijklmnopqrstuvwxyz");

    /* All characters + one "not found" glyph */
    CORRADE_COMPARE(cache.glyphCount(), 27);

    /** @todo properly test contents */
}

}}}}

CORRADE_TEST_MAIN(Magnum::Text::Test::StbTrueTypeFontTest)
