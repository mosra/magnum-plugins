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

#include "HarfBuzzFont.h"

#include <hb-ft.h>
#include <tuple> /* std::tie() :( */
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/PluginManager/AbstractManager.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Text/AbstractGlyphCache.h>

namespace Magnum { namespace Text {

namespace {

class HarfBuzzLayouter: public AbstractLayouter {
    public:
        explicit HarfBuzzLayouter(const AbstractGlyphCache& cache, Float fontSize, Float textSize, hb_buffer_t* buffer, hb_glyph_info_t* glyphInfo, hb_glyph_position_t* glyphPositions, UnsignedInt glyphCount);

        ~HarfBuzzLayouter();

    private:
        Containers::Triple<Range2D, Range2D, Vector2> doRenderGlyph(UnsignedInt i) override;

        const AbstractGlyphCache& _cache;
        const Float _fontSize, _textSize;
        hb_buffer_t* const _buffer;
        hb_glyph_info_t* const _glyphInfo;
        hb_glyph_position_t* const _glyphPositions;
};

}

HarfBuzzFont::HarfBuzzFont(): _hbFont(nullptr) {}

HarfBuzzFont::HarfBuzzFont(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): FreeTypeFont{manager, plugin}, _hbFont(nullptr) {}

HarfBuzzFont::~HarfBuzzFont() { close(); }

FontFeatures HarfBuzzFont::doFeatures() const { return FontFeature::OpenData; }

bool HarfBuzzFont::doIsOpened() const {
    CORRADE_INTERNAL_ASSERT(FreeTypeFont::doIsOpened() == !!_hbFont);
    return FreeTypeFont::doIsOpened();
}

auto HarfBuzzFont::doOpenData(const Containers::ArrayView<const char> data, const Float size) -> Properties {
    /* Open FreeType font */
    auto ret = FreeTypeFont::doOpenData(data, size);

    /* Create Harfbuzz font */
    if(FreeTypeFont::doIsOpened()) _hbFont = hb_ft_font_create(_ftFont, nullptr);

    return ret;
}

void HarfBuzzFont::doClose() {
    hb_font_destroy(_hbFont);
    _hbFont = nullptr;
    FreeTypeFont::doClose();
}

Containers::Pointer<AbstractLayouter> HarfBuzzFont::doLayout(const AbstractGlyphCache& cache, const Float size, const Containers::StringView text) {
    /* Prepare HarfBuzz buffer */
    hb_buffer_t* const buffer = hb_buffer_create();
    hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buffer, hb_language_from_string("en", 2));

    /* Layout the text */
    hb_buffer_add_utf8(buffer, text.data(), text.size(), 0, -1);
    hb_shape(_hbFont, buffer, nullptr, 0);

    UnsignedInt glyphCount;
    hb_glyph_info_t* const glyphInfo = hb_buffer_get_glyph_infos(buffer, &glyphCount);
    hb_glyph_position_t* const glyphPositions = hb_buffer_get_glyph_positions(buffer, &glyphCount);

    return Containers::pointer<HarfBuzzLayouter>(cache, this->size(), size, buffer, glyphInfo, glyphPositions, glyphCount);
}

namespace {

HarfBuzzLayouter::HarfBuzzLayouter(const AbstractGlyphCache& cache, const Float fontSize, const Float textSize, hb_buffer_t* const buffer, hb_glyph_info_t* const glyphInfo, hb_glyph_position_t* const glyphPositions, const UnsignedInt glyphCount): AbstractLayouter(glyphCount), _cache(cache), _fontSize(fontSize), _textSize(textSize), _buffer(buffer), _glyphInfo(glyphInfo), _glyphPositions(glyphPositions) {}

HarfBuzzLayouter::~HarfBuzzLayouter() {
    /* Destroy HarfBuzz buffer */
    hb_buffer_destroy(_buffer);
}

Containers::Triple<Range2D, Range2D, Vector2> HarfBuzzLayouter::doRenderGlyph(const UnsignedInt i) {
    /* Position of the texture in the resulting glyph, texture coordinates */
    Vector2i position;
    Range2Di rectangle;
    std::tie(position, rectangle) = _cache[_glyphInfo[i].codepoint];

    /* Normalized texture coordinates */
    const auto textureCoordinates = Range2D(rectangle).scaled(1.0f/Vector2(_cache.textureSize()));

    /* Glyph offset in normalized coordinates */
    const Vector2 offset = Vector2(_glyphPositions[i].x_offset,
                                   _glyphPositions[i].y_offset)/64.0f;

    /* Quad rectangle, computed from glyph offset and texture rectangle,
       denormalized to requested text size */
    const auto quadRectangle = Range2D(Range2Di::fromSize(position, rectangle.size()))
        .translated(offset).scaled(Vector2(_textSize/_fontSize));

    /* Glyph advance, denormalized to requested text size */
    const Vector2 advance = Vector2(_glyphPositions[i].x_advance,
                                    _glyphPositions[i].y_advance)*(_textSize/(64.0f*_fontSize));

    return {quadRectangle, textureCoordinates, advance};
}

}

}}

CORRADE_PLUGIN_REGISTER(HarfBuzzFont, Magnum::Text::HarfBuzzFont,
    MAGNUM_TEXT_ABSTRACTFONT_PLUGIN_INTERFACE)
