/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013 Vladimír Vondruš <mosra@centrum.cz>

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

#include "FreeTypeFont.h"

#include <algorithm>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <Utility/Unicode.h>
#include <Image.h>
#include <ImageFormat.h>
#include <Text/GlyphCache.h>

namespace Magnum { namespace Text { namespace FreeTypeFont {

namespace {

class FreeTypeLayouter: public AbstractLayouter {
    public:
        explicit FreeTypeLayouter(FT_Face ftFont, const GlyphCache* const cache, const Float fontSize, const Float textSize, const std::string& text);

        std::tuple<Rectangle, Rectangle, Vector2> renderGlyph(const Vector2& cursorPosition, const UnsignedInt i) override;

    private:
        FT_Face font;
        const GlyphCache* const cache;
        const Float fontSize, textSize;
        std::vector<FT_UInt> glyphs;
};

}

FT_Library FreeTypeFont::library = nullptr;

void FreeTypeFont::initialize() {
    if(library) return;

    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Init_FreeType(&library) == 0);
}

void FreeTypeFont::finalize() {
    if(!library) return;

    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Done_FreeType(library) == 0);
    library = nullptr;
}

FreeTypeFont::FreeTypeFont(): ftFont(nullptr) {}

FreeTypeFont::FreeTypeFont(PluginManager::AbstractManager* manager, std::string plugin): AbstractFont(manager, std::move(plugin)), ftFont(nullptr) {}

FreeTypeFont::~FreeTypeFont() { close(); }

bool FreeTypeFont::open(const std::string& filename, Float size) {
    close();

    CORRADE_ASSERT(library, "Text::FreeTypeFont::FreeTypeFont::open(): initialize() was not called", false);
    if(FT_New_Face(library, filename.c_str(), 0, &ftFont) != 0) return false;
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Set_Char_Size(ftFont, 0, size*64, 100, 100) == 0);
    _size = size;
    return true;
}

bool FreeTypeFont::open(const unsigned char* data, std::size_t dataSize, Float size) {
    close();

    CORRADE_ASSERT(library, "Text::FreeTypeFont::FreeTypeFont::open(): initialize() was not called", false);
    if(FT_New_Memory_Face(library, data, dataSize, 0, &ftFont) != 0) return false;
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Set_Char_Size(ftFont, 0, size*64, 100, 100) == 0);
    _size = size;
    return true;
}

void FreeTypeFont::FreeTypeFont::close() {
    if(!ftFont) return;
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Done_Face(ftFont) == 0);
    ftFont = nullptr;
    _size = 0.0f;
}

void FreeTypeFont::createGlyphCache(GlyphCache* const cache, const std::string& characters) {
    CORRADE_ASSERT(ftFont, "Text::FreeTypeFont::FreeTypeFont::createGlyphCache(): no font opened", );

    /** @bug Crash when atlas is too small */

    /* Get glyph codes from characters */
    std::vector<FT_UInt> charIndices;
    charIndices.reserve(characters.size()+1);
    charIndices.push_back(0);
    for(std::size_t i = 0; i != characters.size(); ) {
        UnsignedInt codepoint;
        std::tie(codepoint, i) = Utility::Unicode::nextChar(characters, i);
        charIndices.push_back(FT_Get_Char_Index(ftFont, codepoint));
    }

    /* Remove duplicates (e.g. uppercase and lowercase mapped to same glyph) */
    std::sort(charIndices.begin(), charIndices.end());
    charIndices.erase(std::unique(charIndices.begin(), charIndices.end()), charIndices.end());

    /* Sizes of all characters */
    std::vector<Vector2i> charSizes;
    charSizes.reserve(charIndices.size());
    for(FT_UInt c: charIndices) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(ftFont, c, FT_LOAD_DEFAULT) == 0);
        charSizes.push_back(Vector2i(ftFont->glyph->metrics.width, ftFont->glyph->metrics.height)/64);
    }

    /* Create texture atlas */
    const std::vector<Rectanglei> charPositions = cache->reserve(charSizes);

    /* Render all characters to the atlas and create character map */
    unsigned char* pixmap = new unsigned char[cache->textureSize().product()]();
    Image2D image(cache->textureSize(), ImageFormat::Red, ImageType::UnsignedByte, pixmap);
    for(std::size_t i = 0; i != charPositions.size(); ++i) {
        /* Load and render glyph */
        /** @todo B&W only if radius != 0 */
        FT_GlyphSlot glyph = ftFont->glyph;
        CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(ftFont, charIndices[i], FT_LOAD_DEFAULT) == 0);
        CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL) == 0);

        /* Copy rendered bitmap to texture image */
        const FT_Bitmap& bitmap = glyph->bitmap;
        CORRADE_INTERNAL_ASSERT(std::abs(bitmap.width-charPositions[i].width()) <= 2);
        CORRADE_INTERNAL_ASSERT(std::abs(bitmap.rows-charPositions[i].height()) <= 2);
        for(Int yin = 0, yout = charPositions[i].bottom(), ymax = bitmap.rows; yin != ymax; ++yin, ++yout)
            for(Int xin = 0, xout = charPositions[i].left(), xmax = bitmap.width; xin != xmax; ++xin, ++xout)
                pixmap[yout*cache->textureSize().x() + xout] = bitmap.buffer[(bitmap.rows-yin-1)*bitmap.width + xin];

        /* Insert glyph parameters into cache */
        cache->insert(charIndices[i],
            Vector2i(glyph->bitmap_left, glyph->bitmap_top-charPositions[i].height()),
            charPositions[i]);
    }

    /* Set cache image */
    cache->setImage({}, &image);
}

AbstractLayouter* FreeTypeFont::layout(const GlyphCache* const cache, const Float size, const std::string& text) {
    CORRADE_ASSERT(ftFont, "Text::FreeTypeFont::FreeTypeFont::layout(): no font opened", nullptr);

    return new FreeTypeLayouter(ftFont, cache, this->size(), size, text);
}

namespace {

FreeTypeLayouter::FreeTypeLayouter(FT_Face font, const GlyphCache* const cache, const Float fontSize, const Float textSize, const std::string& text): font(font), cache(cache), fontSize(fontSize), textSize(textSize) {
    /* Get glyph codes from characters */
    glyphs.reserve(text.size());
    for(std::size_t i = 0; i != text.size(); ) {
        UnsignedInt codepoint;
        std::tie(codepoint, i) = Utility::Unicode::nextChar(text, i);
        glyphs.push_back(FT_Get_Char_Index(font, codepoint));
    }
    _glyphCount = glyphs.size();
}

std::tuple<Rectangle, Rectangle, Vector2> FreeTypeLayouter::renderGlyph(const Vector2& cursorPosition, const UnsignedInt i) {
    /* Position of the texture in the resulting glyph, texture coordinates */
    Vector2i position;
    Rectanglei rectangle;
    std::tie(position, rectangle) = (*cache)[glyphs[i]];

    Rectangle texturePosition = Rectangle::fromSize(Vector2(position)/fontSize,
                                                    Vector2(rectangle.size())/fontSize);
    Rectangle textureCoordinates(Vector2(rectangle.bottomLeft())/cache->textureSize(),
                                 Vector2(rectangle.topRight())/cache->textureSize());

    /* Load glyph */
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(font, glyphs[i], FT_LOAD_DEFAULT) == 0);
    const FT_GlyphSlot  slot = font->glyph;
    Vector2 offset = Vector2(0, 0); /** @todo really? */
    Vector2 advance = Vector2(slot->advance.x, slot->advance.y)/(64*fontSize);

    /* Absolute quad position, composed from cursor position, glyph offset
        and texture position, denormalized to requested text size */
    Rectangle quadPosition = Rectangle::fromSize(
        (cursorPosition + offset + Vector2(texturePosition.left(), texturePosition.bottom()))*textSize,
        texturePosition.size()*textSize);

    return std::make_tuple(quadPosition, textureCoordinates, advance);
}

}

}}}

CORRADE_PLUGIN_REGISTER(FreeTypeFont, Magnum::Text::FreeTypeFont::FreeTypeFont,
    "cz.mosra.magnum.Text.AbstractFont/0.1")
