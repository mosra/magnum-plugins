/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
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

#include "StbTrueTypeFont.h"

#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/PluginManager/AbstractManager.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Unicode.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Text/AbstractGlyphCache.h>
#include <Magnum/Text/AbstractShaper.h>
#include <Magnum/TextureTools/Atlas.h>

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

#ifdef MAGNUM_BUILD_DEPRECATED
StbTrueTypeFont::StbTrueTypeFont() = default; /* LCOV_EXCL_LINE */
#endif

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

    if(!stbtt_InitFont(&font->info, font->data, offset)) {
        Error{} << "Text::StbTrueTypeFont::openData(): font initialization failed";
        return {};
    }

    /* All right, let's move in */
    _font = Utility::move(font);

    /* Set font size, 1 px = 0.75 pt
       (https://www.w3.org/TR/CSS21/syndata.html#x39) */
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

void StbTrueTypeFont::doGlyphIdsInto(const Containers::StridedArrayView1D<const char32_t>& characters, const Containers::StridedArrayView1D<UnsignedInt>& glyphs) {
    for(std::size_t i = 0; i != characters.size(); ++i)
        glyphs[i] = stbtt_FindGlyphIndex(&_font->info, characters[i]);
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

bool StbTrueTypeFont::doFillGlyphCache(AbstractGlyphCache& cache, const Containers::StridedArrayView1D<const UnsignedInt>& glyphIndices) {
    if(cache.format() != PixelFormat::R8Unorm) {
        Error{} << "Text::StbTrueTypeFont::fillGlyphCache(): expected a" << PixelFormat::R8Unorm << "glyph cache but got" << cache.format();
        return {};
    }

    /* Register this font, if not in the cache yet */
    Containers::Optional<UnsignedInt> fontId = cache.findFont(*this);
    if(!fontId)
        fontId = cache.addFont(_font->info.numGlyphs, this);

    /* Get sizes of all glyphs to pack into the cache */
    struct Glyph {
        Vector2i size;
        Vector3i offset;
    };
    Containers::Array<Glyph> glyphs{NoInit, glyphIndices.size()};
    for(std::size_t i = 0; i != glyphIndices.size(); ++i) {
        Range2Di box;
        stbtt_GetGlyphBitmapBox(&_font->info, glyphIndices[i], _font->scale, _font->scale, &box.min().x(), &box.min().y(), &box.max().x(), &box.max().y());
        glyphs[i].size = box.size();
    }

    /* Pack the cache */
    const Vector3i cacheFilledSize = cache.atlas().filledSize();
    const Containers::Optional<Range3Di> flushRange = cache.atlas().add(
        stridedArrayView(glyphs).slice(&Glyph::size),
        stridedArrayView(glyphs).slice(&Glyph::offset));
    if(!flushRange) {
        /* Calculate the total area for a more useful report */
        std::size_t totalArea = 0;
        for(const Glyph& glyph: glyphs)
            totalArea += glyph.size.product();

        Error{} << "Text::StbTrueTypeFont::fillGlyphCache(): cannot fit" << glyphs.size() << "glyphs with a total area of" << totalArea << "pixels into a cache of size" << cache.size() << "and" << cacheFilledSize << "filled so far";
        return {};
    }

    /* Memory for stb_truetype to render into.  We need to flip Y, so it can't
       be rendered directly into the glyph cache memory. */
    Range2Di maxBox;
    stbtt_GetFontBoundingBox(&_font->info, &maxBox.min().x(), &maxBox.min().y(), &maxBox.max().x(), &maxBox.max().y());
    Containers::Array<char> srcData{NoInit, std::size_t(maxBox.size().product())};
    const Containers::StridedArrayView2D<const char> src{srcData, {
        std::size_t(maxBox.sizeY()),
        std::size_t(maxBox.sizeX())
    }};

    /* Render all glyphs to the atlas and create a glyph map */
    const Containers::StridedArrayView3D<char> dst = cache.image().pixels<char>();
    for(std::size_t i = 0; i != glyphs.size(); ++i) {
        /* Render the glyph */
        Range2Di box;
        stbtt_GetGlyphBitmapBox(&_font->info, glyphIndices[i], _font->scale, _font->scale, &box.min().x(), &box.min().y(), &box.max().x(), &box.max().y());
        stbtt_MakeGlyphBitmap(&_font->info, reinterpret_cast<unsigned char*>(srcData.data()), maxBox.sizeX(), maxBox.sizeY(), maxBox.sizeX(), _font->scale, _font->scale, glyphIndices[i]);

        /* Copy the rendered glyph Y-flipped to the destination image */
        const Containers::Size2D glyphSize{std::size_t(glyphs[i].size.y()),
                                           std::size_t(glyphs[i].size.x())};
        Utility::copy(
            src
                .prefix(glyphSize).flipped<0>(),
            dst[glyphs[i].offset.z()]
                .sliceSize({std::size_t(glyphs[i].offset.y()),
                            std::size_t(glyphs[i].offset.x())}, glyphSize));

        /* Insert glyph parameters into the cache */
        cache.addGlyph(*fontId, glyphIndices[i],
            Vector2i{box.min().x(), -box.max().y()},
            glyphs[i].offset.z(),
            Range2Di::fromSize(glyphs[i].offset.xy(), glyphs[i].size));
    }

    /* Flush the updated cache image */
    cache.flushImage(*flushRange);

    return true;
}

Containers::Pointer<AbstractShaper> StbTrueTypeFont::doCreateShaper() {
    struct Shaper: AbstractShaper {
        using AbstractShaper::AbstractShaper;

        UnsignedInt doShape(const Containers::StringView textFull, const UnsignedInt begin, const UnsignedInt end, Containers::ArrayView<const FeatureRange>) override {
            const stbtt_fontinfo stbFont = static_cast<const StbTrueTypeFont&>(font())._font->info;
            const Containers::StringView text = textFull.slice(begin, end == ~UnsignedInt{} ? textFull.size() : end);

            /* Get glyph codes from characters */
            arrayResize(_glyphs, 0);
            arrayReserve(_glyphs, text.size());
            for(std::size_t i = 0; i != text.size(); ) {
                const Containers::Pair<char32_t, std::size_t> codepointNext = Utility::Unicode::nextChar(text, i);
                arrayAppend(_glyphs, InPlaceInit,
                    UnsignedInt(stbtt_FindGlyphIndex(&stbFont, codepointNext.first())),
                    begin + UnsignedInt(i));
                i = codepointNext.second();
            }

            return _glyphs.size();
        }

        void doGlyphIdsInto(const Containers::StridedArrayView1D<UnsignedInt>& ids) const override {
            Utility::copy(stridedArrayView(_glyphs).slice(&Containers::Pair<UnsignedInt, UnsignedInt>::first), ids);
        }
        void doGlyphOffsetsAdvancesInto(const Containers::StridedArrayView1D<Vector2>& offsets, const Containers::StridedArrayView1D<Vector2>& advances) const override {
            const Font& fontData = *static_cast<const StbTrueTypeFont&>(font())._font;

            for(std::size_t i = 0; i != _glyphs.size(); ++i) {
                /* There's no glyph offsets in addition to advances. The last
                   argument of stbtt_GetGlyphHMetrics() is leftSideBearing, but
                   that, once rounded, is returned from
                   stbtt_GetGlyphBitmapBox() that's then saved into the glyph
                   cache in doFillGlyphCache() above. */
                offsets[i] = {};

                /* Get glyph advance, scale it to actual used font size */
                Int advance;
                stbtt_GetGlyphHMetrics(&fontData.info, _glyphs[i].first(), &advance, nullptr);
                advances[i] = {advance*fontData.scale, 0.0f};
            }
        }
        void doGlyphClustersInto(const Containers::StridedArrayView1D<UnsignedInt>& clusters) const override {
            Utility::copy(stridedArrayView(_glyphs).slice(&Containers::Pair<UnsignedInt, UnsignedInt>::second), clusters);
        }

        Containers::Array<Containers::Pair<UnsignedInt, UnsignedInt>> _glyphs;
    };

    return Containers::pointer<Shaper>(*this);
}

}}

CORRADE_PLUGIN_REGISTER(StbTrueTypeFont, Magnum::Text::StbTrueTypeFont,
    MAGNUM_TEXT_ABSTRACTFONT_PLUGIN_INTERFACE)
