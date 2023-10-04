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

#include "FreeTypeFont.h"

#include <algorithm> /* std::transform(), std::sort(), std::unique() */
#include <tuple> /* std::tie() :( */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/PluginManager/AbstractManager.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Unicode.h>
#include <Magnum/Image.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Text/AbstractGlyphCache.h>

namespace Magnum { namespace Text {

namespace {

class FreeTypeLayouter: public AbstractLayouter {
    public:
        explicit FreeTypeLayouter(FT_Face font, const AbstractGlyphCache& cache, const Float fontSize, const Float textSize, Containers::Array<FT_UInt>&& glyphs);

    private:
        Containers::Triple<Range2D, Range2D, Vector2> doRenderGlyph(const UnsignedInt i) override;

        FT_Face _font;
        const AbstractGlyphCache& _cache;
        const Float _fontSize, _layoutSize;
        const Containers::Array<FT_UInt> _glyphs;
};

}

#if defined(CORRADE_BUILD_MULTITHREADED) && !defined(CORRADE_TARGET_WINDOWS)
CORRADE_THREAD_LOCAL
#endif
FT_Library FreeTypeFont::library = nullptr;

void FreeTypeFont::initialize() {
    CORRADE_INTERNAL_ASSERT(!library);
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Init_FreeType(&library) == 0);
}

void FreeTypeFont::finalize() {
    CORRADE_INTERNAL_ASSERT(library);
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Done_FreeType(library) == 0);
    library = nullptr;
}

FreeTypeFont::FreeTypeFont(): ftFont(nullptr) {}

FreeTypeFont::FreeTypeFont(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractFont{manager, plugin}, ftFont(nullptr) {}

FreeTypeFont::~FreeTypeFont() { close(); }

FontFeatures FreeTypeFont::doFeatures() const { return FontFeature::OpenData; }

bool FreeTypeFont::doIsOpened() const { return ftFont; }

auto FreeTypeFont::doOpenData(const Containers::ArrayView<const char> data, const Float size) -> Properties {
    /* We need to preserve the data for whole FT_Face lifetime */
    _data = Containers::Array<unsigned char>{NoInit, data.size()};
    Utility::copy(Containers::arrayCast<const unsigned char>(data), _data);

    CORRADE_ASSERT(library, "Text::FreeTypeFont::openSingleData(): initialize() was not called", {});
    /** @todo ability to specify different font in TTC collection */
    if(FT_Error error = FT_New_Memory_Face(library, _data.begin(), _data.size(), 0, &ftFont)) {
        Error{} << "Text::FreeTypeFont::openData(): failed to open the font:" << error;
        return {};
    }
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Set_Char_Size(ftFont, 0, size*64, 0, 0) == 0);
    return {size,
            ftFont->size->metrics.ascender/64.0f,
            ftFont->size->metrics.descender/64.0f,
            ftFont->size->metrics.height/64.0f,
            UnsignedInt(ftFont->num_glyphs)};
}

void FreeTypeFont::doClose() {
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Done_Face(ftFont) == 0);
    _data = nullptr;
    ftFont = nullptr;
}

UnsignedInt FreeTypeFont::doGlyphId(const char32_t character) {
    return FT_Get_Char_Index(ftFont, character);
}

Vector2 FreeTypeFont::doGlyphSize(const UnsignedInt glyph) {
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(ftFont, glyph, FT_LOAD_DEFAULT) == 0);
    return Vector2{Float(ftFont->glyph->metrics.width),
                   Float(ftFont->glyph->metrics.height)}/64.0f;
}

Vector2 FreeTypeFont::doGlyphAdvance(const UnsignedInt glyph) {
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(ftFont, glyph, FT_LOAD_DEFAULT) == 0);
    return Vector2(ftFont->glyph->advance.x, ftFont->glyph->advance.y)/64.0f;
}

void FreeTypeFont::doFillGlyphCache(AbstractGlyphCache& cache, const Containers::ArrayView<const char32_t> characters) {
    /** @bug Crash when atlas is too small */

    /* Get glyph codes from characters */
    Containers::Array<FT_UInt> glyphIndices{NoInit, characters.size() + 1};
    glyphIndices[0] = 0;
    for(std::size_t i = 0; i != characters.size(); ++i)
        glyphIndices[i + 1] = FT_Get_Char_Index(ftFont, characters[i]);

    /* Remove duplicates (e.g. uppercase and lowercase mapped to same glyph) */
    /** @todo deduplicate via a BitArray instead */
    std::sort(glyphIndices.begin(), glyphIndices.end());
    const std::size_t uniqueCount = std::unique(glyphIndices.begin(), glyphIndices.end()) - glyphIndices.begin();

    /* Sizes of all glyphs */
    std::vector<Vector2i> glyphSizes(uniqueCount);
    for(std::size_t i = 0; i != uniqueCount; ++i) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(ftFont, glyphIndices[i], FT_LOAD_DEFAULT) == 0);
            glyphSizes[i] = Vector2i{Int(ftFont->glyph->metrics.width), Int(ftFont->glyph->metrics.height)}/64;
    }

    /* Create texture atlas */
    const std::vector<Range2Di> glyphPositions = cache.reserve(glyphSizes);

    /* Render all glyphs to the atlas and create a glyph map */
    Containers::Array<char> pixmap{ValueInit, std::size_t(cache.textureSize().product())};
    for(std::size_t i = 0; i != glyphPositions.size(); ++i) {
        /* Load and render glyph */
        /** @todo B&W only if radius != 0 */
        FT_GlyphSlot glyph = ftFont->glyph;
        CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(ftFont, glyphIndices[i], FT_LOAD_DEFAULT) == 0);
        CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL) == 0);

        /* Copy rendered bitmap to texture image */
        const FT_Bitmap& bitmap = glyph->bitmap;
        CORRADE_INTERNAL_ASSERT(std::abs(Int(bitmap.width)-glyphPositions[i].sizeX()) <= 2);
        CORRADE_INTERNAL_ASSERT(std::abs(Int(bitmap.rows)-glyphPositions[i].sizeY()) <= 2);
        for(Int yin = 0, yout = glyphPositions[i].bottom(), ymax = bitmap.rows; yin != ymax; ++yin, ++yout)
            for(Int xin = 0, xout = glyphPositions[i].left(), xmax = bitmap.width; xin != xmax; ++xin, ++xout)
                pixmap[yout*cache.textureSize().x() + xout] = bitmap.buffer[(bitmap.rows-yin-1)*bitmap.width + xin];

        /* Insert glyph parameters into the cache */
        cache.insert(glyphIndices[i],
            Vector2i{glyph->bitmap_left, glyph->bitmap_top- glyphPositions[i].sizeY()},
            glyphPositions[i]);
    }

    /* Set cache image */
    Image2D image(PixelFormat::R8Unorm, cache.textureSize(), Utility::move(pixmap));
    cache.setImage({}, image);
}

Containers::Pointer<AbstractLayouter> FreeTypeFont::doLayout(const AbstractGlyphCache& cache, const Float size, const Containers::StringView text) {
    /* Get glyph codes from characters */
    Containers::Array<UnsignedInt> glyphs{NoInit, text.size()};
    for(std::size_t i = 0; i != text.size(); ) {
        const Containers::Pair<char32_t, std::size_t> codepointNext = Utility::Unicode::nextChar(text, i);
        glyphs[i] = FT_Get_Char_Index(ftFont, codepointNext.first());
        i = codepointNext.second();
    }

    return Containers::pointer<FreeTypeLayouter>(ftFont, cache, this->size(), size, Utility::move(glyphs));
}

namespace {

FreeTypeLayouter::FreeTypeLayouter(FT_Face font, const AbstractGlyphCache& cache, const Float fontSize, const Float layoutSize, Containers::Array<FT_UInt>&& glyphs): AbstractLayouter{UnsignedInt(glyphs.size())}, _font{font}, _cache(cache), _fontSize{fontSize}, _layoutSize{layoutSize}, _glyphs{Utility::move(glyphs)} {}

Containers::Triple<Range2D, Range2D, Vector2> FreeTypeLayouter::doRenderGlyph(const UnsignedInt i) {
    /* Position of the texture in the resulting glyph, texture coordinates */
    Vector2i position;
    Range2Di rectangle;
    std::tie(position, rectangle) = _cache[_glyphs[i]];

    /* Normalized texture coordinates */
    const auto textureCoordinates = Range2D{rectangle}.scaled(1.0f/Vector2{_cache.textureSize()});

    /* Quad rectangle, computed from texture rectangle, denormalized to
       requested text size */
    const auto quadRectangle = Range2D(Range2Di::fromSize(position, rectangle.size())).scaled(Vector2{_layoutSize/_fontSize});

    /* Load glyph */
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(_font, _glyphs[i], FT_LOAD_DEFAULT) == 0);
    const FT_GlyphSlot slot = _font->glyph;

    /* Glyph advance, denormalized to requested text size */
    const Vector2 advance = Vector2{Float(slot->advance.x), Float(slot->advance.y)}*(_layoutSize/(64.0f*_fontSize));

    return {quadRectangle, textureCoordinates, advance};
}

}

}}

CORRADE_PLUGIN_REGISTER(FreeTypeFont, Magnum::Text::FreeTypeFont,
    MAGNUM_TEXT_ABSTRACTFONT_PLUGIN_INTERFACE)
