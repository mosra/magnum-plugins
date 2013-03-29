#ifndef Magnum_Text_HarfBuzzFont_HarfBuzzFont_h
#define Magnum_Text_HarfBuzzFont_HarfBuzzFont_h
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

/** @file
 * @brief Class Magnum::Text::HarfBuzzFont::HarfBuzzFont
 */

#include "FreeTypeFont/FreeTypeFont.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
struct hb_font_t;
#endif

namespace Magnum { namespace Text { namespace HarfBuzzFont {

/**
@brief HarfBuzz font

Improves FreeTypeFont with [HarfBuzz](http://www.freedesktop.org/wiki/Software/HarfBuzz)
text layouting capabilities, such as kerning, ligatures etc. See FreeTypeFont
class documentation for more information about usage.
*/
class HarfBuzzFont: public FreeTypeFont::FreeTypeFont {
    public:
        /** @brief Default constructor */
        explicit HarfBuzzFont();

        /** @brief Plugin manager constructor */
        explicit HarfBuzzFont(Corrade::PluginManager::AbstractPluginManager* manager, std::string plugin);

        ~HarfBuzzFont();

        bool open(const std::string& filename, Float size) override;
        bool open(const unsigned char* data, std::size_t dataSize, Float size) override;
        void close() override;
        AbstractLayouter* layout(const GlyphCache* const cache, const Float size, const std::string& text) override;

    private:
        void MAGNUM_TEXT_LOCAL finishConstruction();

        hb_font_t* hbFont;
};

}}}

#endif
