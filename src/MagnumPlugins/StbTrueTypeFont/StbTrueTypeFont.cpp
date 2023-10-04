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

#include "StbTrueTypeFont.h"

#include <algorithm> /* std::transform(), std::sort(), std::unique() */
#include <tuple> /* std::tie() :( */
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

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#ifdef CORRADE_TARGET_MSVC
#include <cmath>
/* Using fabsf instead of fabs (double version) for 30% performance
   improvement on MSVC Debug builds. The others are not so significant. */
#define STBTT_ifloor(x) static_cast<int>(floorf(x))
#define STBTT_iceil(x) static_cast<int>(ceilf(x))
#define STBTT_sqrt(x) sqrtf(x)
#define STBTT_fmod(x, y) fmodf(x, y)
#define STBTT_pow(x,y) powf(x, y)
#define STBTT_cos(x) cosf(x)
#define STBTT_acos(x) acosf(x)
#define STBTT_fabs(x) fabsf(x)
#endif
#include "stb_truetype.h"

namespace Magnum { namespace Text {

struct StbTrueTypeFont::Font {
    Containers::Array<unsigned char> data;
    stbtt_fontinfo info;
    Float scale;
};

class StbTrueTypeFont::Layouter: public AbstractLayouter {
    public:
        explicit Layouter(Font& _font, const AbstractGlyphCache& cache, const Float fontSize, const Float textSize, std::vector<Int>&& glyphs);

    private:
        Containers::Triple<Range2D, Range2D, Vector2> doRenderGlyph(const UnsignedInt i) override;

        Font& _font;
        const AbstractGlyphCache& _cache;
        const Float _fontSize, _textSize;
        const std::vector<Int> _glyphs;
};

StbTrueTypeFont::StbTrueTypeFont() = default;

StbTrueTypeFont::StbTrueTypeFont(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractFont{manager, plugin} {}

StbTrueTypeFont::~StbTrueTypeFont() = default;

FontFeatures StbTrueTypeFont::doFeatures() const { return FontFeature::OpenData; }

bool StbTrueTypeFont::doIsOpened() const { return !!_font; }

auto StbTrueTypeFont::doOpenData(const Containers::ArrayView<const char> data, const Float size) -> Properties {
    /* stbtt_GetFontOffsetForIndex() fails hard when passed it an empty file
       (because of course it doesn't take a size, ffs), check explicitly */
    if(data.isEmpty()) {
        Error{} << "Text::StbTrueTypeFont::openData(): the file is empty";
        return {};
    }

    Containers::Pointer<Font> font{InPlaceInit};

    /* TrueType fonts are memory-mapped, thus we need to preserve the data for
       the whole plugin lifetime */
    font->data = Containers::Array<unsigned char>(NoInit, data.size());
    Utility::copy(Containers::arrayCast<const unsigned char>(data), font->data);

    /** @todo ability to specify different font index in TTC collection */
    const int offset = stbtt_GetFontOffsetForIndex(font->data, 0);
    if(offset < 0) {
        Error{} << "Text::StbTrueTypeFont::openData(): can't get offset of the first font";
        return {};
    }

    if(!stbtt_InitFont(&font->info, font->data, offset)) return {};

    /* All right, let's move in */
    _font = Utility::move(font);

    /* Set font size, 1 px = 0.75 pt (http://www.w3.org/TR/CSS21/syndata.html#x39) */
    _font->scale = stbtt_ScaleForPixelHeight(&_font->info, size/0.75f);

    /* Return font metrics */
    Int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&_font->info, &ascent, &descent, &lineGap);
    return {size,
            _font->scale*ascent,
            _font->scale*descent,
            _font->scale*(ascent - descent + lineGap),
            UnsignedInt(_font->info.numGlyphs)};
}

void StbTrueTypeFont::doClose() {
    _font = nullptr;
}

UnsignedInt StbTrueTypeFont::doGlyphId(const char32_t character) {
    return stbtt_FindGlyphIndex(&_font->info, character);
}

Vector2 StbTrueTypeFont::doGlyphSize(const UnsignedInt glyph) {
    Range2Di box;
    stbtt_GetGlyphBitmapBox(&_font->info, glyph, _font->scale, _font->scale, &box.min().x(), &box.min().y(), &box.max().x(), &box.max().y());
    return Vector2{box.size()};
}

Vector2 StbTrueTypeFont::doGlyphAdvance(const UnsignedInt glyph) {
    Int advance;
    stbtt_GetGlyphHMetrics(&_font->info, glyph, &advance, nullptr);
    return Vector2::xAxis(advance*_font->scale);
}

void StbTrueTypeFont::doFillGlyphCache(AbstractGlyphCache& cache, const Containers::ArrayView<const char32_t> characters) {
    /* Get glyph codes from characters */
    Containers::Array<Int> glyphIndices{NoInit, characters.size() + 1};
    glyphIndices[0] = 0;
    for(std::size_t i = 0; i != characters.size(); ++i)
        glyphIndices[i + 1] = stbtt_FindGlyphIndex(&_font->info, characters[i]);

    /* Remove duplicates (e.g. uppercase and lowercase mapped to same glyph) */
    /** @todo deduplicate via a BitArray instead */
    std::sort(glyphIndices.begin(), glyphIndices.end());
    const std::size_t uniqueCount = std::unique(glyphIndices.begin(), glyphIndices.end()) - glyphIndices.begin();

    /* Properties of all glyphs to reserve the cache */
    std::vector<Vector2i> glyphSizes(uniqueCount);
    for(std::size_t i = 0; i != uniqueCount; ++i) {
        Range2Di box;
        stbtt_GetGlyphBitmapBox(&_font->info, glyphIndices[i], _font->scale, _font->scale, &box.min().x(), &box.min().y(), &box.max().x(), &box.max().y());
        glyphSizes[i] = box.size();
    }

    /* Create texture atlas */
    /** @todo use Containers::Array for this */
    const std::vector<Range2Di> glyphPositions = cache.reserve(glyphSizes);

    /* Pixmap for a single glyph. We need to flip Y, so it can't be rendered
       directly into the texture. What a pity. */
    Range2Di maxBox;
    stbtt_GetFontBoundingBox(&_font->info, &maxBox.min().x(), &maxBox.min().y(), &maxBox.max().x(), &maxBox.max().y());
    Containers::Array<UnsignedByte> glyphPixmap{NoInit,
        std::size_t(maxBox.size().product())};

    /* Render all glyphs to the atlas and create a glyph map */
    Containers::Array<char> pixmap{ValueInit, std::size_t(cache.textureSize().product())};
    for(std::size_t i = 0; i != glyphPositions.size(); ++i) {
        /* Render glyph */
        Range2Di box;
        stbtt_GetGlyphBitmapBox(&_font->info, glyphIndices[i], _font->scale, _font->scale, &box.min().x(), &box.min().y(), &box.max().x(), &box.max().y());
        stbtt_MakeGlyphBitmap(&_font->info, glyphPixmap, maxBox.sizeX(), maxBox.sizeY(), maxBox.sizeX(), _font->scale, _font->scale, glyphIndices[i]);

        /* Copy rendered bitmap to texture image */
        for(Int yin = 0, yout = glyphPositions[i].bottom(), ymax = glyphSizes[i].y(); yin != ymax; ++yin, ++yout)
            for(Int xin = 0, xout = glyphPositions[i].left(), xmax = glyphSizes[i].x(); xin != xmax; ++xin, ++xout)
                pixmap[yout*cache.textureSize().x() + xout] = glyphPixmap[(glyphSizes[i].y() - yin - 1)*maxBox.sizeX() + xin];

        /* Insert glyph parameters into the cache */
        cache.insert(glyphIndices[i],
            Vector2i{box.min().x(), -box.max().y()},
            glyphPositions[i]);
    }

    /* Set cache image */
    Image2D image(PixelFormat::R8Unorm, cache.textureSize(), Utility::move(pixmap));
    cache.setImage({}, image);
}

Containers::Pointer<AbstractLayouter> StbTrueTypeFont::doLayout(const AbstractGlyphCache& cache, const Float size, const Containers::StringView text) {
    /* Get glyph codes from characters */
    std::vector<Int> glyphs;
    glyphs.reserve(text.size());
    for(std::size_t i = 0; i != text.size(); ) {
        const Containers::Pair<char32_t, std::size_t> codepointNext = Utility::Unicode::nextChar(text, i);
        glyphs.push_back(stbtt_FindGlyphIndex(&_font->info, codepointNext.first()));
        i = codepointNext.second();
    }

    return Containers::pointer(new Layouter{*_font, cache, this->size(), size, Utility::move(glyphs)});
}

StbTrueTypeFont::Layouter::Layouter(Font& font, const AbstractGlyphCache& cache, const Float fontSize, const Float textSize, std::vector<Int>&& glyphs): AbstractLayouter(glyphs.size()), _font(font), _cache(cache), _fontSize{fontSize}, _textSize{textSize}, _glyphs{Utility::move(glyphs)} {}

Containers::Triple<Range2D, Range2D, Vector2> StbTrueTypeFont::Layouter::doRenderGlyph(const UnsignedInt i) {
    /* Position of the texture in the resulting glyph, texture coordinates */
    Vector2i position;
    Range2Di rectangle;
    std::tie(position, rectangle) = _cache[_glyphs[i]];

    /* Normalized texture coordinates */
    const auto textureCoordinates = Range2D(rectangle).scaled(1.0f/Vector2(_cache.textureSize()));

    /* Quad rectangle, computed from texture rectangle, denormalized to
       requested text size */
    const auto quadRectangle = Range2D(Range2Di::fromSize(position, rectangle.size())).scaled(Vector2(_textSize/_fontSize));

    /* Glyph advance, denormalized to requested text size */
    Vector2i advance;
    stbtt_GetGlyphHMetrics(&_font.info, _glyphs[i], &advance.x(), nullptr);
    return {quadRectangle, textureCoordinates, Vector2(advance)*(_font.scale*_textSize/_fontSize)};
}

}}

CORRADE_PLUGIN_REGISTER(StbTrueTypeFont, Magnum::Text::StbTrueTypeFont,
    MAGNUM_TEXT_ABSTRACTFONT_PLUGIN_INTERFACE)
