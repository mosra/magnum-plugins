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
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/PluginManager/AbstractManager.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Text/AbstractGlyphCache.h>
#include <Magnum/Text/AbstractShaper.h>

namespace Magnum { namespace Text {

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

Containers::Pointer<AbstractShaper> HarfBuzzFont::doCreateShaper() {
    struct Shaper: AbstractShaper {
        explicit Shaper(AbstractFont& font): AbstractShaper{font} {
            _buffer = hb_buffer_create();
        }

        /* Probably don't need moves for anything here, so disable them as well
           instead of implementing them or risking that a buffer gets destroyed
           twice */
        Shaper(const Shaper&) = delete;
        Shaper(Shaper&&) = delete;

        ~Shaper() {
            hb_buffer_destroy(_buffer);
        }

        Shaper& operator=(const Shaper&) = delete;
        Shaper& operator=(Shaper&&) = delete;

        UnsignedInt doShape(const Containers::StringView text, const UnsignedInt begin, const UnsignedInt end, Containers::ArrayView<const FeatureRange>) override {
            /* If shaping was performed already, the buffer type is
               HB_BUFFER_CONTENT_TYPE_GLYPHS, need to reset it to accept
               Unicode input again. */
            hb_buffer_reset(_buffer);
            hb_buffer_set_direction(_buffer, HB_DIRECTION_LTR);
            hb_buffer_set_script(_buffer, HB_SCRIPT_LATIN);
            hb_buffer_set_language(_buffer, hb_language_from_string("en", 2));

            hb_buffer_add_utf8(_buffer, text.data(), text.size(), begin, end - begin);
            hb_shape(static_cast<const HarfBuzzFont&>(font())._hbFont, _buffer, nullptr, 0);

            return hb_buffer_get_length(_buffer);
        }

        void doGlyphsInto(const Containers::StridedArrayView1D<UnsignedInt>& ids, const Containers::StridedArrayView1D<Vector2>& offsets, const Containers::StridedArrayView1D<Vector2>& advances) const override {
            UnsignedInt glyphCount;
            hb_glyph_info_t* const glyphInfos = hb_buffer_get_glyph_infos(_buffer, &glyphCount);
            hb_glyph_position_t* const glyphPositions = hb_buffer_get_glyph_positions(_buffer, &glyphCount);
            CORRADE_INTERNAL_ASSERT(glyphCount == this->glyphCount());

            for(std::size_t i = 0; i != glyphCount; ++i) {
                ids[i] = glyphInfos[i].codepoint;
                offsets[i] = Vector2{Float(glyphPositions[i].x_offset),
                                     Float(glyphPositions[i].y_offset)}/64.0f;
                advances[i] = Vector2{Float(glyphPositions[i].x_advance),
                                      Float(glyphPositions[i].y_advance)}/64.0f;
            }
        }

        hb_buffer_t* _buffer;
    };

    return Containers::pointer<Shaper>(*this);
}

}}

CORRADE_PLUGIN_REGISTER(HarfBuzzFont, Magnum::Text::HarfBuzzFont,
    MAGNUM_TEXT_ABSTRACTFONT_PLUGIN_INTERFACE)
