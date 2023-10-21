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
#include <Corrade/Utility/Endianness.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Text/AbstractGlyphCache.h>
#include <Magnum/Text/AbstractShaper.h>
#include <Magnum/Text/Direction.h>
#include <Magnum/Text/Script.h>

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

        bool doSetScript(const Script script) override {
            /* Mildly crazy code to achieve the following:
                -   Script values not supported by given HarfBuzz version
                    result in HB_SCRIPT_INVALID being used, and this function
                    returning false.
                -   Script values supported by HarfBuzz are simply passed
                    through, as the Script and hb_script_t values are matching
                    by design and thus no complicated lookup table is needed.
                -   As there's 160+ scripts, with the vast majority supported
                    by all HarfBuzz versions, going through a giant "whitelist"
                    switch every time would be unnecessary overhead. So instead
                    it's an inverse, listing only scripts that *aren't*
                    supported. Supported scripts will thus fall through all
                    cases and be passed through. It's very likely that a
                    HarfBuzz version with most scripts is used, which means the
                    switch has very few cases. This also allows to pass
                    yet-unknown Script values to HarfBuzz, for example when
                    there's a new version out and
                -   As the switch lists just a very few cases and not all, it
                    needs to have the fallthrough in a `default` case instead
                    of outside of the switch to avoid compiler warnings about
                    100+ unhandled values.
                -   In case a HarfBuzz version that supports all scripts is
                    used, there would be no `case` left for the `return false`
                    codepath. It's thus `#if`'d away if a version for which
                    everything from scriptMapping.h is supported.
                -   If `default` would be last and the `#if` would be compiled
                    away, it could lead to accidents where newly added values
                    from scriptMapping.h fall through to the default case if
                    the HB_VERSION_ATLEAST() isn't updated appropriately:

                        switch(script) {
                            case Script::SomeNewScript:
                                // return true; compiled out
                            default:
                                _script = HB_SCRIPT_INVALID;
                                return false;
                        }

                    To prevent that, the `default` case is first. Accidental
                    omission of the `return true` will then lead to a compiler
                    error, either due to a `case` having no body, or due to
                    a codepath not having a `return`. Interestingly, it *does
                    not* cause the `default` to be picked always. */
            switch(script) {
                default:
                    /* HB_TAG() is unfortunately Endian-dependent, producing
                       e.g. ntaL instead of Latn on Little-Endian. I couldn't
                       find any documentation or a bug report on why this
                       differs from what OpenType fonts actually have (where
                       it's Big-Endian always, i.e. Latn), apart from one
                       "oops" in an old commit:
                        https://github.com/harfbuzz/harfbuzz/commit/fcd6f5326166e993b8f5222efbaffe916da98f0a */
                    _script = hb_script_t(Utility::Endianness::bigEndian(script));
                    return true;
                #define _c(name, hb) case Script::name:
                #define _c_include_supported 0
                #include "scriptMapping.h"
                #undef _c_include_supported
                #undef _c
                    /* This version needs to be kept in sync with the last
                       version in scriptMapping.h */
                    #if !HB_VERSION_ATLEAST(5, 2, 0)
                    _script = HB_SCRIPT_INVALID;
                    return false;
                    #endif
            }
        }

        bool doSetLanguage(const Containers::StringView language) override {
            _language = language ? hb_language_from_string(language.data(), language.size()) : HB_LANGUAGE_INVALID;
            /* There's not really a way to know whether given language is
               supported, as there's too many. Just say yes every time. */
            return true;
        }

        bool doSetDirection(ShapeDirection direction) override {
            switch(direction) {
                case ShapeDirection::LeftToRight:
                    _direction = HB_DIRECTION_LTR;
                    return true;
                case ShapeDirection::RightToLeft:
                    _direction = HB_DIRECTION_RTL;
                    return true;
                case ShapeDirection::TopToBottom:
                    _direction = HB_DIRECTION_TTB;
                    return true;
                case ShapeDirection::BottomToTop:
                    _direction = HB_DIRECTION_BTT;
                    return true;
                case ShapeDirection::Unspecified:
                    _direction = HB_DIRECTION_INVALID;
                    return true;
            }

            CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }

        UnsignedInt doShape(const Containers::StringView text, const UnsignedInt begin, const UnsignedInt end, const Containers::ArrayView<const FeatureRange> features) override {
            /* If shaping was performed already, the buffer type is
               HB_BUFFER_CONTENT_TYPE_GLYPHS, need to reset it to accept
               Unicode input again. */
            hb_buffer_reset(_buffer);

            /* Set direction, script and language, add the text */
            hb_buffer_set_script(_buffer, _script);
            hb_buffer_set_language(_buffer, _language);
            hb_buffer_set_direction(_buffer, _direction);
            hb_buffer_add_utf8(_buffer, text.data(), text.size(), begin, end - begin);

            /* If any of the properties were unspecified, try to guess them
               from the passed text */
            if(_script == HB_SCRIPT_INVALID ||
               _language == HB_LANGUAGE_INVALID ||
               _direction == HB_DIRECTION_INVALID)
                hb_buffer_guess_segment_properties(_buffer);

            /* Allocate a temporary array for hb_feature_t entries. The
               FeatureRange has the same layout, but unfortunately like with
               script values, the feature tags are endian-dependent in HarfBuzz
               so we have to modify them. Sigh. */
            /** @todo use some stack allocator or DynamicArray when that's a
                thing to avoid the allocation */
            Containers::Array<hb_feature_t> hbFeatures{NoInit, features.size()};
            for(std::size_t i = 0; i != features.size(); ++i) {
                hbFeatures[i].tag = UnsignedInt(Utility::Endianness::bigEndian(features[i].feature()));
                hbFeatures[i].value = features[i].value();
                hbFeatures[i].start = features[i].begin();
                hbFeatures[i].end = features[i].end();
            }

            hb_shape(static_cast<const HarfBuzzFont&>(font())._hbFont, _buffer, hbFeatures.data(), hbFeatures.size());

            return hb_buffer_get_length(_buffer);
        }

        Script doScript() const override {
            hb_segment_properties_t props;
            hb_buffer_get_segment_properties(_buffer, &props);
            return Script(Utility::Endianness::bigEndian(props.script));
        }

        Containers::StringView doLanguage() const override {
            hb_segment_properties_t props;
            hb_buffer_get_segment_properties(_buffer, &props);
            /* The string most probably isn't global as
               hb_buffer_set_language() accepts any string, but it should
               definitely stay in scope for at least as long as HarfBuzz is
               alive so we don't need to keep a local copy */
            return hb_language_to_string(props.language);
        }

        ShapeDirection doDirection() const override {
            hb_segment_properties_t props;
            hb_buffer_get_segment_properties(_buffer, &props);

            switch(props.direction) {
                case HB_DIRECTION_LTR: return ShapeDirection::LeftToRight;
                case HB_DIRECTION_RTL: return ShapeDirection::RightToLeft;
                case HB_DIRECTION_TTB: return ShapeDirection::TopToBottom;
                case HB_DIRECTION_BTT: return ShapeDirection::BottomToTop;
                case HB_DIRECTION_INVALID: return ShapeDirection::Unspecified;
            }

            CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }

        void doGlyphIdsInto(const Containers::StridedArrayView1D<UnsignedInt>& ids) const override {
            UnsignedInt glyphCount;
            hb_glyph_info_t* const glyphInfos = hb_buffer_get_glyph_infos(_buffer, &glyphCount);
            CORRADE_INTERNAL_ASSERT(glyphCount == this->glyphCount());

            for(std::size_t i = 0; i != glyphCount; ++i)
                ids[i] = glyphInfos[i].codepoint;
        }
        void doGlyphOffsetsAdvancesInto(const Containers::StridedArrayView1D<Vector2>& offsets, const Containers::StridedArrayView1D<Vector2>& advances) const override {
            UnsignedInt glyphCount;
            hb_glyph_position_t* const glyphPositions = hb_buffer_get_glyph_positions(_buffer, &glyphCount);
            CORRADE_INTERNAL_ASSERT(glyphCount == this->glyphCount());

            for(std::size_t i = 0; i != glyphCount; ++i) {
                offsets[i] = Vector2{Float(glyphPositions[i].x_offset),
                                     Float(glyphPositions[i].y_offset)}/64.0f;
                advances[i] = Vector2{Float(glyphPositions[i].x_advance),
                                      Float(glyphPositions[i].y_advance)}/64.0f;
            }
        }

        hb_buffer_t* _buffer;
        /* These are stored because they're re-set to the buffer before each
           shaping, to ensure autodetection for unspecified properties every
           time instead of the previously autodetected value staying for
           following runs */
        hb_script_t _script = HB_SCRIPT_INVALID;
        hb_language_t _language = HB_LANGUAGE_INVALID;
        hb_direction_t _direction = HB_DIRECTION_INVALID;
    };

    return Containers::pointer<Shaper>(*this);
}

}}

CORRADE_PLUGIN_REGISTER(HarfBuzzFont, Magnum::Text::HarfBuzzFont,
    MAGNUM_TEXT_ABSTRACTFONT_PLUGIN_INTERFACE)
