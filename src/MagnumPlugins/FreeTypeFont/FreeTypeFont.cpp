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
#include <ft2build.h>
#include FT_FREETYPE_H
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
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
#include <Magnum/TextureTools/Atlas.h>

namespace Magnum { namespace Text {

namespace {

class FreeTypeLayouter: public AbstractLayouter {
    public:
        explicit FreeTypeLayouter(FT_Face font, const AbstractGlyphCache& cache, UnsignedInt fontId, Float fontSize, Float textSize, Containers::Array<FT_UInt>&& glyphs);

    private:
        Containers::Triple<Range2D, Range2D, Vector2> doRenderGlyph(const UnsignedInt i) override;

        FT_Face _font;
        const AbstractGlyphCache& _cache;
        const UnsignedInt _fontId;
        const Float _fontSize, _layoutSize;
        const Containers::Array<FT_UInt> _glyphs;
};

}

#if defined(CORRADE_BUILD_MULTITHREADED) && !defined(CORRADE_TARGET_WINDOWS)
CORRADE_THREAD_LOCAL
#endif
FT_Library FreeTypeFont::_library = nullptr;

void FreeTypeFont::initialize() {
    CORRADE_INTERNAL_ASSERT(!_library);
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Init_FreeType(&_library) == 0);
}

void FreeTypeFont::finalize() {
    CORRADE_INTERNAL_ASSERT(_library);
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Done_FreeType(_library) == 0);
    _library = nullptr;
}

FreeTypeFont::FreeTypeFont(): _ftFont(nullptr) {}

FreeTypeFont::FreeTypeFont(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractFont{manager, plugin}, _ftFont(nullptr) {}

FreeTypeFont::~FreeTypeFont() { close(); }

FontFeatures FreeTypeFont::doFeatures() const { return FontFeature::OpenData; }

bool FreeTypeFont::doIsOpened() const { return _ftFont; }

auto FreeTypeFont::doOpenData(const Containers::ArrayView<const char> data, const Float size) -> Properties {
    /* We need to preserve the data for whole FT_Face lifetime */
    _data = Containers::Array<unsigned char>{NoInit, data.size()};
    Utility::copy(Containers::arrayCast<const unsigned char>(data), _data);

    CORRADE_ASSERT(_library, "Text::FreeTypeFont::openSingleData(): initialize() was not called", {});
    /** @todo ability to specify different font in TTC collection */
    if(FT_Error error = FT_New_Memory_Face(_library, _data.begin(), _data.size(), 0, &_ftFont)) {
        Error{} << "Text::FreeTypeFont::openData(): failed to open the font:" << error;
        return {};
    }
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Set_Char_Size(_ftFont, 0, size*64, 0, 0) == 0);
    return {size,
            _ftFont->size->metrics.ascender/64.0f,
            _ftFont->size->metrics.descender/64.0f,
            _ftFont->size->metrics.height/64.0f,
            UnsignedInt(_ftFont->num_glyphs)};
}

void FreeTypeFont::doClose() {
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Done_Face(_ftFont) == 0);
    _data = nullptr;
    _ftFont = nullptr;
}

UnsignedInt FreeTypeFont::doGlyphId(const char32_t character) {
    return FT_Get_Char_Index(_ftFont, character);
}

Vector2 FreeTypeFont::doGlyphSize(const UnsignedInt glyph) {
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(_ftFont, glyph, FT_LOAD_DEFAULT) == 0);
    return Vector2{Float(_ftFont->glyph->metrics.width),
                   Float(_ftFont->glyph->metrics.height)}/64.0f;
}

Vector2 FreeTypeFont::doGlyphAdvance(const UnsignedInt glyph) {
    CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(_ftFont, glyph, FT_LOAD_DEFAULT) == 0);
    return Vector2(_ftFont->glyph->advance.x, _ftFont->glyph->advance.y)/64.0f;
}

void FreeTypeFont::doFillGlyphCache(AbstractGlyphCache& cache, const Containers::ArrayView<const char32_t> characters) {
    /** @todo fix the fillGlyphCache API to make it failable */
    CORRADE_INTERNAL_ASSERT(cache.format() == PixelFormat::R8Unorm);

    /* Register this font, if not in the cache yet */
    Containers::Optional<UnsignedInt> fontId = cache.findFont(this);
    const bool firstFill = !fontId;
    if(!fontId)
        fontId = cache.addFont(_ftFont->num_glyphs, this);

    /* Get glyph codes from characters. If this is the first fill, include also
       the invalid glyph. */
    /** @todo leave that on the user, maybe? or do it only in the convenience
        "characters" overload and not the "glyph IDs" one */
    Containers::Array<FT_UInt> glyphIndices{NoInit, characters.size() + (firstFill ? 1 : 0)};
    for(std::size_t i = 0; i != characters.size(); ++i)
        glyphIndices[i] = FT_Get_Char_Index(_ftFont, characters[i]);
    if(firstFill)
        glyphIndices.back() = 0;

    /* Remove duplicates (e.g. uppercase and lowercase mapped to same glyph) */
    /** @todo deduplicate via a BitArray instead */
    std::sort(glyphIndices.begin(), glyphIndices.end());
    const std::size_t uniqueCount = std::unique(glyphIndices.begin(), glyphIndices.end()) - glyphIndices.begin();

    /* Get sizes of all glyphs to pack into the cache */
    struct Glyph {
        Vector2i size;
        Vector3i offset;
    };
    Containers::Array<Glyph> glyphs{NoInit, uniqueCount};
    for(std::size_t i = 0; i != uniqueCount; ++i) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(_ftFont, glyphIndices[i], FT_LOAD_DEFAULT) == 0);
        glyphs[i].size = Vector2i{Int(_ftFont->glyph->metrics.width), Int(_ftFont->glyph->metrics.height)}/64;
    }

    /* Pack the cache */
    /** @todo fix the fillGlyphCache API to make it failable */
    CORRADE_INTERNAL_ASSERT_OUTPUT(cache.atlas().add(
        stridedArrayView(glyphs).slice(&Glyph::size),
        stridedArrayView(glyphs).slice(&Glyph::offset)));

    /* Render all glyphs to the atlas and create a glyph map */
    const Containers::StridedArrayView3D<char> dst = cache.image().pixels<char>();
    Range3Di flushRange;
    for(std::size_t i = 0; i != glyphs.size(); ++i) {
        /* Load and render the glyph */
        const FT_GlyphSlot glyph = _ftFont->glyph;
        /** @todo B&W only if radius != 0 */
        CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(_ftFont, glyphIndices[i], FT_LOAD_DEFAULT) == 0);
        CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL) == 0);

        /* Copy the rendered glyph Y-flipped to the destination image */
        const FT_Bitmap& bitmap = glyph->bitmap;
        const Containers::Size2D glyphSize{bitmap.rows, bitmap.width};
        Utility::copy(
            Containers::StridedArrayView2D<const char>{{reinterpret_cast<const char*>(bitmap.buffer), ~std::size_t{}}, glyphSize}.flipped<0>(),
            dst[glyphs[i].offset.z()]
                .sliceSize({std::size_t(glyphs[i].offset.y()),
                            std::size_t(glyphs[i].offset.x())}, glyphSize));

        /* Insert glyph parameters into the cache */
        cache.addGlyph(*fontId, glyphIndices[i],
            Vector2i{glyph->bitmap_left, glyph->bitmap_top - glyphs[i].size.y()},
            glyphs[i].offset.z(),
            Range2Di::fromSize(glyphs[i].offset.xy(), glyphs[i].size));

        /* Maintain an union spanning all added glyphs to flush */
        /** @todo might span too much if mutliple slices are covered in a
            disjoint fashion, what to do? */
        flushRange = Math::join(flushRange, Range3Di::fromSize(glyphs[i].offset, Vector3i{glyphs[i].size, 1}));
    }

    /* Flush the updated cache image */
    cache.flushImage(flushRange);
}

Containers::Pointer<AbstractLayouter> FreeTypeFont::doLayout(const AbstractGlyphCache& cache, const Float size, const Containers::StringView text) {
    /* Not yet, at least */
    if(cache.size().z() != 1) {
        Error{} << "Text::FreeTypeFont::layout(): array glyph caches are not supported";
        return {};
    }

    /* Find this font in the cache */
    Containers::Optional<UnsignedInt> fontId = cache.findFont(this);
    if(!fontId) {
        Error{} << "Text::FreeTypeFont::layout(): font not found among" << cache.fontCount() << "fonts in passed glyph cache";
        return {};
    }

    /* Get glyph codes from characters */
    Containers::Array<UnsignedInt> glyphs;
    arrayReserve(glyphs, text.size());
    for(std::size_t i = 0; i != text.size(); ) {
        const Containers::Pair<char32_t, std::size_t> codepointNext = Utility::Unicode::nextChar(text, i);
        arrayAppend(glyphs, FT_Get_Char_Index(_ftFont, codepointNext.first()));
        i = codepointNext.second();
    }

    return Containers::pointer<FreeTypeLayouter>(_ftFont, cache, *fontId, this->size(), size, Utility::move(glyphs));
}

namespace {

FreeTypeLayouter::FreeTypeLayouter(FT_Face font, const AbstractGlyphCache& cache, const UnsignedInt fontId, const Float fontSize, const Float layoutSize, Containers::Array<FT_UInt>&& glyphs): AbstractLayouter{UnsignedInt(glyphs.size())}, _font{font}, _cache(cache), _fontId{fontId}, _fontSize{fontSize}, _layoutSize{layoutSize}, _glyphs{Utility::move(glyphs)} {}

Containers::Triple<Range2D, Range2D, Vector2> FreeTypeLayouter::doRenderGlyph(const UnsignedInt i) {
    /* Offset of the glyph rectangle relative to the cursor, layer, texture
       coordinates. We checked that the glyph cache is 2D in doLayout() so the
       layer can be ignored. */
    const Containers::Triple<Vector2i, Int, Range2Di> glyph = _cache.glyph(_fontId, _glyphs[i]);
    CORRADE_INTERNAL_ASSERT(glyph.second() == 0);

    /* Normalized texture coordinates */
    const auto textureCoordinates = Range2D{glyph.third()}.scaled(1.0f/Vector2{_cache.size().xy()});

    /* Quad rectangle, computed from texture rectangle, denormalized to
       requested text size */
    const auto quadRectangle = Range2D{Range2Di::fromSize(glyph.first(), glyph.third().size())}.scaled(Vector2{_layoutSize/_fontSize});

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
