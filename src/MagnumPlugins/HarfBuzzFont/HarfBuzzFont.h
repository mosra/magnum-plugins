#ifndef Magnum_Text_HarfBuzzFont_h
#define Magnum_Text_HarfBuzzFont_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015
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

/** @file
 * @brief Class @ref Magnum::Text::HarfBuzzFont
 */

#include "MagnumPlugins/FreeTypeFont/FreeTypeFont.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
struct hb_font_t;
#endif

namespace Magnum { namespace Text {

/**
@brief HarfBuzz font plugin

Improves @ref FreeTypeFont with [HarfBuzz](http://www.freedesktop.org/wiki/Software/HarfBuzz)
text layouting capabilities, such as kerning, ligatures etc.

This plugin depends on **HarfBuzz** library and @ref FreeTypeFont plugin. It
is built if `WITH_HARFBUZZFONT` is enabled when building Magnum Plugins. To
use dynamic plugin, you need to load `HarfBuzzFont` plugin from
`MAGNUM_PLUGINS_FONT_DIR`. To use static plugin, you need to request
`HarfBuzzFont` component of `MagnumPlugins` package in CMake and link to
`${MAGNUMPLUGINS_HARFBUZZFONT_LIBRARIES}`. To use this as a dependency of
another plugin, you additionally need to add
`${MAGNUMPLUGINS_HARFBUZZFONT_INCLUDE_DIRS}` to include path. See
@ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class HarfBuzzFont: public FreeTypeFont {
    public:
        /** @brief Default constructor */
        explicit HarfBuzzFont();

        /** @brief Plugin manager constructor */
        explicit HarfBuzzFont(PluginManager::AbstractManager& manager, std::string plugin);

        ~HarfBuzzFont();

    private:
        Features doFeatures() const override;
        bool doIsOpened() const override;
        std::pair<Float, Float> doOpenSingleData(Containers::ArrayView<const char> data, Float size) override;
        void doClose() override;
        std::unique_ptr<AbstractLayouter> doLayout(const GlyphCache& cache, Float size, const std::string& text) override;

        hb_font_t* hbFont;
};

}}

#endif
