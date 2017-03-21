/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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

#include <Magnum/OpenGLTester.h>
#include <Magnum/Text/GlyphCache.h>

#include "MagnumPlugins/FreeTypeFont/FreeTypeFont.h"

#include "configure.h"

namespace Magnum { namespace Text { namespace Test {

struct FreeTypeFontGLTest: OpenGLTester {
    explicit FreeTypeFontGLTest();

    ~FreeTypeFontGLTest();

    void properties();
    void layout();
    void fillGlyphCache();
};

FreeTypeFontGLTest::FreeTypeFontGLTest() {
    addTests({&FreeTypeFontGLTest::properties,
              &FreeTypeFontGLTest::layout,
              &FreeTypeFontGLTest::fillGlyphCache});

    FreeTypeFont::initialize();
}

FreeTypeFontGLTest::~FreeTypeFontGLTest() {
    FreeTypeFont::finalize();
}

void FreeTypeFontGLTest::properties() {
    FreeTypeFont font;
    CORRADE_VERIFY(font.openFile(TTF_FILE, 16.0f));
    CORRADE_COMPARE(font.size(), 16.0f);
    CORRADE_COMPARE(font.ascent(), 15.0f);
    CORRADE_COMPARE(font.descent(), -4.0f);
    CORRADE_COMPARE(font.lineHeight(), 19.0f);
    CORRADE_COMPARE(font.glyphId(U'W'), 58);
    CORRADE_COMPARE(font.glyphAdvance(58), Vector2(17.0f, 0.0f));
}

void FreeTypeFontGLTest::layout() {
    FreeTypeFont font;
    CORRADE_VERIFY(font.openFile(TTF_FILE, 16.0f));

    /* Fill the cache with some fake glyphs */
    GlyphCache cache(Vector2i(256));
    cache.insert(font.glyphId(U'W'), {25, 34}, {{0, 8}, {16, 128}});
    cache.insert(font.glyphId(U'e'), {25, 12}, {{16, 4}, {64, 32}});

    std::unique_ptr<AbstractLayouter> layouter = font.layout(cache, 0.5f, "Wave");
    CORRADE_VERIFY(layouter);
    CORRADE_COMPARE(layouter->glyphCount(), 4);

    Vector2 cursorPosition;
    Range2D rectangle, position, textureCoordinates;

    /* 'W' */
    std::tie(position, textureCoordinates) = layouter->renderGlyph(0, cursorPosition = {}, rectangle);
    CORRADE_COMPARE(position, Range2D({0.78125f, 1.0625f}, {1.28125f, 4.8125f}));
    CORRADE_COMPARE(textureCoordinates, Range2D({0, 0.03125f}, {0.0625f, 0.5f}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.53125f, 0.0f));

    /* 'a' (not in cache) */
    std::tie(position, textureCoordinates) = layouter->renderGlyph(1, cursorPosition = {}, rectangle);
    CORRADE_COMPARE(position, Range2D());
    CORRADE_COMPARE(textureCoordinates, Range2D());
    CORRADE_COMPARE(cursorPosition, Vector2(0.25f, 0.0f));

    /* 'v' (not in cache) */
    std::tie(position, textureCoordinates) = layouter->renderGlyph(2, cursorPosition = {}, rectangle);
    CORRADE_COMPARE(position, Range2D());
    CORRADE_COMPARE(textureCoordinates, Range2D());
    CORRADE_COMPARE(cursorPosition, Vector2(0.25f, 0.0f));

    /* 'e' */
    std::tie(position, textureCoordinates) = layouter->renderGlyph(3, cursorPosition = {}, rectangle);
    CORRADE_COMPARE(position, Range2D({0.78125f, 0.375f}, {2.28125f, 1.25f}));
    CORRADE_COMPARE(textureCoordinates, Range2D({0.0625f, 0.015625f}, {0.25f, 0.125f}));
    CORRADE_COMPARE(cursorPosition, Vector2(0.28125f, 0.0f));
}

void FreeTypeFontGLTest::fillGlyphCache() {
    FreeTypeFont font;
    CORRADE_VERIFY(font.openFile(TTF_FILE, 16.0f));

    /* Just testing that nothing crashes, asserts or errors */
    GlyphCache cache{Vector2i{256}};
    font.fillGlyphCache(cache, "abcdefghijklmnopqrstuvwxyz");

    MAGNUM_VERIFY_NO_ERROR();

    /* All characters + one "not found" glyph */
    CORRADE_COMPARE(cache.glyphCount(), 27);

    /** @todo properly test contents */
}

}}}

CORRADE_TEST_MAIN(Magnum::Text::Test::FreeTypeFontGLTest)
