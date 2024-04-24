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

#include <ft2build.h>
#include FT_FREETYPE_H
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/PluginManager/AbstractManager.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Unicode.h>
#include <Magnum/Image.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Text/AbstractGlyphCache.h>
#include <Magnum/Text/AbstractShaper.h>
#include <Magnum/TextureTools/Atlas.h>

namespace Magnum { namespace Text {

#ifndef FT_CONFIG_OPTION_ERROR_STRINGS
namespace {

const char* ftErrorString(const FT_Error error) {
    switch(error) {
        #define FT_ERRORDEF_(name, index, string) case FT_Err_ ## name: return string;
        #define FT_NOERRORDEF_ FT_ERRORDEF_
        #include <freetype/fterrdef.h>
        #undef FT_NOERRORDEF_
        #undef FT_ERRORDEF_
    }

    return {};
}

}
#endif

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
        Error e;
        e << "Text::FreeTypeFont::openData(): failed to open the font:";
        if(const char* string =
            /* FreeType can be compiled without error strings, in which case
               the returned string will be always null. In that case we do our
               own string conversion. */
            #ifndef FT_CONFIG_OPTION_ERROR_STRINGS
            ftErrorString(error)
            #else
            FT_Error_String(error)
            #endif
        )
            e << string;
        /* Even if it *is* compiled with error strings, there still can be
           garbage error codes, for which it should return at least the
           number. */
        else
            e << error;
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

void FreeTypeFont::doGlyphIdsInto(const Containers::StridedArrayView1D<const char32_t>& characters, const Containers::StridedArrayView1D<UnsignedInt>& glyphs) {
    for(std::size_t i = 0; i != characters.size(); ++i)
        glyphs[i] = FT_Get_Char_Index(_ftFont, characters[i]);
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

bool FreeTypeFont::doFillGlyphCache(AbstractGlyphCache& cache, const Containers::StridedArrayView1D<const UnsignedInt>& glyphIndices) {
    if(cache.format() != PixelFormat::R8Unorm) {
        Error{} << "Text::FreeTypeFont::fillGlyphCache(): expected a" << PixelFormat::R8Unorm << "glyph cache but got" << cache.format();
        return {};
    }

    /* Register this font, if not in the cache yet */
    Containers::Optional<UnsignedInt> fontId = cache.findFont(*this);
    if(!fontId)
        fontId = cache.addFont(_ftFont->num_glyphs, this);

    /* Get sizes of all glyphs to pack into the cache */
    struct Glyph {
        Vector2i size;
        Vector3i offset;
    };
    Containers::Array<Glyph> glyphs{NoInit, glyphIndices.size()};
    for(std::size_t i = 0; i != glyphIndices.size(); ++i) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(_ftFont, glyphIndices[i], FT_LOAD_DEFAULT) == 0);
        glyphs[i].size = Vector2i{Int(_ftFont->glyph->metrics.width), Int(_ftFont->glyph->metrics.height)}/64;
    }

    /* Pack the cache */
    const Vector3i cacheFilledSize = cache.atlas().filledSize();
    if(!cache.atlas().add(
        stridedArrayView(glyphs).slice(&Glyph::size),
        stridedArrayView(glyphs).slice(&Glyph::offset)))
    {
        /* Calculate the total area for a more useful report */
        std::size_t totalArea = 0;
        for(const Glyph& glyph: glyphs)
            totalArea += glyph.size.product();

        Error{} << "Text::FreeTypeFont::fillGlyphCache(): cannot fit" << glyphs.size() << "glyphs with a total area of" << totalArea << "pixels into a cache of size" << cache.size() << "and" << cacheFilledSize << "filled so far";
        return {};
    }

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

    return true;
}

Containers::Pointer<AbstractShaper> FreeTypeFont::doCreateShaper() {
    struct Shaper: AbstractShaper {
        using AbstractShaper::AbstractShaper;

        UnsignedInt doShape(const Containers::StringView textFull, const UnsignedInt begin, const UnsignedInt end, Containers::ArrayView<const FeatureRange>) override {
            const FT_Face ftFont = static_cast<const FreeTypeFont&>(font())._ftFont;
            const Containers::StringView text = textFull.slice(begin, end == ~UnsignedInt{} ? textFull.size() : end);

            /* Get glyph codes from characters */
            arrayResize(_glyphs, 0);
            arrayReserve(_glyphs, text.size());
            for(std::size_t i = 0; i != text.size(); ) {
                const Containers::Pair<char32_t, std::size_t> codepointNext = Utility::Unicode::nextChar(text, i);
                arrayAppend(_glyphs, FT_Get_Char_Index(ftFont, codepointNext.first()));
                i = codepointNext.second();
            }

            return _glyphs.size();
        }

        void doGlyphIdsInto(const Containers::StridedArrayView1D<UnsignedInt>& ids) const override {
            Utility::copy(_glyphs, ids);
        }
        void doGlyphOffsetsAdvancesInto(const Containers::StridedArrayView1D<Vector2>& offsets, const Containers::StridedArrayView1D<Vector2>& advances) const override {
            const FT_Face ftFont = static_cast<const FreeTypeFont&>(font())._ftFont;

            for(std::size_t i = 0; i != _glyphs.size(); ++i) {
                /* There's no glyph offsets in addition to advances */
                /** @todo there's horiBearingX and horiBearingY in
                    FT_Glyph_Metrics, isn't that the offset? */
                offsets[i] = {};

                /* Load the glyph to get its advance */
                CORRADE_INTERNAL_ASSERT_OUTPUT(FT_Load_Glyph(ftFont, _glyphs[i], FT_LOAD_DEFAULT) == 0);
                const FT_GlyphSlot slot = ftFont->glyph;
                advances[i] = Vector2{Float(slot->advance.x),
                                      Float(slot->advance.y)}/64.0f;
            }
        }

        Containers::Array<UnsignedInt> _glyphs;
    };

    return Containers::pointer<Shaper>(*this);
}

}}

CORRADE_PLUGIN_REGISTER(FreeTypeFont, Magnum::Text::FreeTypeFont,
    MAGNUM_TEXT_ABSTRACTFONT_PLUGIN_INTERFACE)
