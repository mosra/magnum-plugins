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
#include <Containers/Array.h>
#include <Utility/Unicode.h>
#include <Context.h>
#include <Extensions.h>
#include <Image.h>
#include <ImageFormat.h>
#include <Text/GlyphCache.h>

namespace Magnum { namespace Text {

namespace {

class FreeTypeLayouter: public AbstractLayouter {
    public:
        explicit FreeTypeLayouter(FT_Face ftFont, const GlyphCache& cache, const Float fontSize, const Float textSize, const std::string& text);

        std::tuple<Rectangle, Rectangle, Vector2> renderGlyph(const UnsignedInt i) override;

    private:
        FT_Face font;
        const GlyphCache& cache;
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

auto FreeTypeFont::doFeatures() const -> Features { return Feature::OpenData; }

bool FreeTypeFont::doIsOpened() const { return ftFont; }

void FreeTypeFont::doOpenFile(const std::string& filename, const Float size) {
    CORRADE_ASSERT(library, "Text::FreeTypeFont::open(): initialize() was not called", );
    if(FT_New_Face(library, filename.c_str(), 0, &ftFont) != 0) return;
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Set_Char_Size(ftFont, 0, size*64, 100, 100) == 0);
    _size = size;
}

void FreeTypeFont::doOpenSingleData(const Containers::ArrayReference<const unsigned char> data, const Float size) {
    CORRADE_ASSERT(library, "Text::FreeTypeFont::open(): initialize() was not called", );
    if(FT_New_Memory_Face(library, data.begin(), data.size(), 0, &ftFont) != 0) return;
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Set_Char_Size(ftFont, 0, size*64, 100, 100) == 0);
    _size = size;
}

void FreeTypeFont::doClose() {
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Done_Face(ftFont) == 0);
    ftFont = nullptr;
    _size = 0.0f;
}

UnsignedInt FreeTypeFont::doGlyphId(const char32_t character) {
    return FT_Get_Char_Index(ftFont, character);
}

Vector2 FreeTypeFont::doGlyphAdvance(const UnsignedInt glyph) {
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(ftFont, glyph, FT_LOAD_DEFAULT) == 0);
    return Vector2(ftFont->glyph->advance.x, ftFont->glyph->advance.y)/64.0f;
}

#ifndef _WIN32
void FreeTypeFont::doFillGlyphCache(GlyphCache& cache, const std::u32string& characters)
#else
void FreeTypeFont::doFillGlyphCache(GlyphCache& cache, const std::vector<char32_t>& characters)
#endif
{
    /** @bug Crash when atlas is too small */

    /* Get glyph codes from characters */
    std::vector<FT_UInt> charIndices;
    charIndices.resize(characters.size()+1);
    charIndices[0] = 0;
    std::transform(characters.begin(), characters.end(), charIndices.begin()+1,
        [this](const char32_t c) { return FT_Get_Char_Index(ftFont, c); });

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
    const std::vector<Rectanglei> charPositions = cache.reserve(charSizes);

    /* Render all characters to the atlas and create character map */
    unsigned char* pixmap = new unsigned char[cache.textureSize().product()]();
    /** @todo Some better way for this */
    #ifndef MAGNUM_TARGET_GLES2
    Image2D image(ImageFormat::Red, ImageType::UnsignedByte, cache.textureSize(), pixmap);
    #else
    Image2D image(Context::current() && Context::current()->isExtensionSupported<Extensions::GL::EXT::texture_rg>() ?
        ImageFormat::Red : ImageFormat::Luminance, ImageType::UnsignedByte, cache.textureSize(), pixmap);
    #endif
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
                pixmap[yout*cache.textureSize().x() + xout] = bitmap.buffer[(bitmap.rows-yin-1)*bitmap.width + xin];

        /* Insert glyph parameters into cache */
        cache.insert(charIndices[i],
            Vector2i(glyph->bitmap_left, glyph->bitmap_top-charPositions[i].height()),
            charPositions[i]);
    }

    /* Set cache image */
    cache.setImage({}, image);
}

AbstractLayouter* FreeTypeFont::doLayout(const GlyphCache& cache, const Float size, const std::string& text) {
    return new FreeTypeLayouter(ftFont, cache, this->size(), size, text);
}

namespace {

FreeTypeLayouter::FreeTypeLayouter(FT_Face font, const GlyphCache& cache, const Float fontSize, const Float textSize, const std::string& text): font(font), cache(cache), fontSize(fontSize), textSize(textSize) {
    /* Get glyph codes from characters */
    glyphs.reserve(text.size());
    for(std::size_t i = 0; i != text.size(); ) {
        UnsignedInt codepoint;
        std::tie(codepoint, i) = Utility::Unicode::nextChar(text, i);
        glyphs.push_back(FT_Get_Char_Index(font, codepoint));
    }
    _glyphCount = glyphs.size();
}

std::tuple<Rectangle, Rectangle, Vector2> FreeTypeLayouter::renderGlyph(const UnsignedInt i) {
    /* Position of the texture in the resulting glyph, texture coordinates */
    Vector2i position;
    Rectanglei rectangle;
    std::tie(position, rectangle) = cache[glyphs[i]];

    const Rectangle texturePosition = Rectangle::fromSize(Vector2(position)/fontSize,
                                                          Vector2(rectangle.size())/fontSize);
    const Rectangle textureCoordinates(Vector2(rectangle.bottomLeft())/Vector2(cache.textureSize()),
                                       Vector2(rectangle.topRight())/Vector2(cache.textureSize()));

    /* Load glyph */
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(font, glyphs[i], FT_LOAD_DEFAULT) == 0);
    const FT_GlyphSlot slot = font->glyph;

    /* Absolute quad position, composed from cursor position, glyph offset
       and texture position, denormalized to requested text size */
    const Rectangle quadPosition = Rectangle::fromSize(
        Vector2(texturePosition.left(), texturePosition.bottom())*textSize,
        texturePosition.size()*textSize);

    /* Glyph advance, denormalized to requested text size */
    const Vector2 advance = Vector2(slot->advance.x, slot->advance.y)*textSize/(64*fontSize);

    return std::make_tuple(quadPosition, textureCoordinates, advance);
}

}

}}
