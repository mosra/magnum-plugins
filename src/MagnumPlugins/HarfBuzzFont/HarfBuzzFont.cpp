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

#include "HarfBuzzFont.h"

#include <hb-ft.h>
#include <Corrade/PluginManager/AbstractManager.h>
#include <Magnum/Text/AbstractGlyphCache.h>

namespace Magnum { namespace Text {

namespace {

class HarfBuzzLayouter: public AbstractLayouter {
    public:
        explicit HarfBuzzLayouter(const AbstractGlyphCache& cache, Float fontSize, Float textSize, hb_buffer_t* buffer, hb_glyph_info_t* glyphInfo, hb_glyph_position_t* glyphPositions, UnsignedInt glyphCount);

        ~HarfBuzzLayouter();

    private:
        std::tuple<Range2D, Range2D, Vector2> doRenderGlyph(UnsignedInt i) override;

        const AbstractGlyphCache& cache;
        const Float fontSize, textSize;
        hb_buffer_t* const buffer;
        hb_glyph_info_t* const glyphInfo;
        hb_glyph_position_t* const glyphPositions;
};

}

HarfBuzzFont::HarfBuzzFont(): hbFont(nullptr) {}

HarfBuzzFont::HarfBuzzFont(PluginManager::AbstractManager& manager, const std::string& plugin): FreeTypeFont{manager, plugin}, hbFont(nullptr) {}

HarfBuzzFont::~HarfBuzzFont() { close(); }

FontFeatures HarfBuzzFont::doFeatures() const { return FontFeature::OpenData; }

bool HarfBuzzFont::doIsOpened() const {
    CORRADE_INTERNAL_ASSERT(FreeTypeFont::doIsOpened() == !!hbFont);
    return FreeTypeFont::doIsOpened();
}

auto HarfBuzzFont::doOpenData(const Containers::ArrayView<const char> data, const Float size) -> Metrics {
    /* Open FreeType font */
    auto ret = FreeTypeFont::doOpenData(data, size);

    /* Create Harfbuzz font */
    if(FreeTypeFont::doIsOpened()) hbFont = hb_ft_font_create(ftFont, nullptr);

    return ret;
}

void HarfBuzzFont::doClose() {
    hb_font_destroy(hbFont);
    hbFont = nullptr;
    FreeTypeFont::doClose();
}

Containers::Pointer<AbstractLayouter> HarfBuzzFont::doLayout(const AbstractGlyphCache& cache, const Float size, const std::string& text) {
    /* Prepare HarfBuzz buffer */
    hb_buffer_t* const buffer = hb_buffer_create();
    hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buffer, hb_language_from_string("en", 2));

    /* Layout the text */
    hb_buffer_add_utf8(buffer, text.data(), text.size(), 0, -1);
    hb_shape(hbFont, buffer, nullptr, 0);

    UnsignedInt glyphCount;
    hb_glyph_info_t* const glyphInfo = hb_buffer_get_glyph_infos(buffer, &glyphCount);
    hb_glyph_position_t* const glyphPositions = hb_buffer_get_glyph_positions(buffer, &glyphCount);

    return Containers::pointer(new HarfBuzzLayouter(cache, this->size(), size, buffer, glyphInfo, glyphPositions, glyphCount));
}

namespace {

HarfBuzzLayouter::HarfBuzzLayouter(const AbstractGlyphCache& cache, const Float fontSize, const Float textSize, hb_buffer_t* const buffer, hb_glyph_info_t* const glyphInfo, hb_glyph_position_t* const glyphPositions, const UnsignedInt glyphCount): AbstractLayouter(glyphCount), cache(cache), fontSize(fontSize), textSize(textSize), buffer(buffer), glyphInfo(glyphInfo), glyphPositions(glyphPositions) {}

HarfBuzzLayouter::~HarfBuzzLayouter() {
    /* Destroy HarfBuzz buffer */
    hb_buffer_destroy(buffer);
}

std::tuple<Range2D, Range2D, Vector2> HarfBuzzLayouter::doRenderGlyph(const UnsignedInt i) {
    /* Position of the texture in the resulting glyph, texture coordinates */
    Vector2i position;
    Range2Di rectangle;
    std::tie(position, rectangle) = cache[glyphInfo[i].codepoint];

    /* Normalized texture coordinates */
    const auto textureCoordinates = Range2D(rectangle).scaled(1.0f/Vector2(cache.textureSize()));

    /* Glyph offset in normalized coordinates */
    const Vector2 offset = Vector2(glyphPositions[i].x_offset,
                                   glyphPositions[i].y_offset)/64.0f;

    /* Quad rectangle, computed from glyph offset and texture rectangle,
       denormalized to requested text size */
    const auto quadRectangle = Range2D(Range2Di::fromSize(position, rectangle.size()))
        .translated(offset).scaled(Vector2(textSize/fontSize));

    /* Glyph advance, denormalized to requested text size */
    const Vector2 advance = Vector2(glyphPositions[i].x_advance,
                                    glyphPositions[i].y_advance)*(textSize/(64.0f*fontSize));

    return std::make_tuple(quadRectangle, textureCoordinates, advance);
}

}

}}

CORRADE_PLUGIN_REGISTER(HarfBuzzFont, Magnum::Text::HarfBuzzFont,
    "cz.mosra.magnum.Text.AbstractFont/0.3")
