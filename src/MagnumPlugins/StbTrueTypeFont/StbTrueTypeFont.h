#ifndef Magnum_Text_StbTrueTypeFont_h
#define Magnum_Text_StbTrueTypeFont_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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
 * @brief Class @ref Magnum::Text::StbTrueTypeFont
 */

#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/VisibilityMacros.h>
#include <Magnum/Text/AbstractFont.h>

#include "MagnumPlugins/StbTrueTypeFont/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_STBTRUETYPEFONT_BUILD_STATIC
    #if defined(StbTrueTypeFont_EXPORTS) || defined(StbTrueTypeFontObjects_EXPORTS)
        #define MAGNUM_STBTRUETYPEFONT_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_STBTRUETYPEFONT_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_STBTRUETYPEFONT_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_STBTRUETYPEFONT_LOCAL CORRADE_VISIBILITY_LOCAL
#endif

namespace Magnum { namespace Text {

/**
@brief TrueType font plugin using stb_freetype

The font can be created either from file or from memory location of format
supported by [StbTrueType](http://www.freetype.org/) library.

This plugin is built if `WITH_STBTRUETYPEFONT` is enabled when building Magnum
Plugins. To use dynamic plugin, you need to load `StbTrueTypeFont` plugin from
`MAGNUM_PLUGINS_FONT_DIR`. To use static plugin, you need to request
`StbTrueTypeFont` component of `MagnumPlugins` package in CMake and link to
`MagnumPlugins::StbTrueTypeFont` target.

This plugin provides `TrueTypeFont` plugin, but please note that this plugin
trades the simplicity and portability for various limitations, the most visible
being the lack of autohinting. That causes the rendered glyphs looking blurry
compared to @ref FreeTypeFont and because of that the font properties and sizes
don't exactly match properties of fonts opened with @ref FreeTypeFont using the
same size.

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class MAGNUM_STBTRUETYPEFONT_EXPORT StbTrueTypeFont: public AbstractFont {
    public:
        /** @brief Default constructor */
        explicit StbTrueTypeFont();

        /** @brief Plugin manager constructor */
        explicit StbTrueTypeFont(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~StbTrueTypeFont();

    private:
        struct Font;
        class Layouter;

        MAGNUM_STBTRUETYPEFONT_LOCAL Features doFeatures() const override;
        MAGNUM_STBTRUETYPEFONT_LOCAL bool doIsOpened() const override;
        MAGNUM_STBTRUETYPEFONT_LOCAL Metrics doOpenSingleData(Containers::ArrayView<const char> data, Float size) override;
        MAGNUM_STBTRUETYPEFONT_LOCAL void doClose() override;
        MAGNUM_STBTRUETYPEFONT_LOCAL UnsignedInt doGlyphId(char32_t character) override;
        MAGNUM_STBTRUETYPEFONT_LOCAL Vector2 doGlyphAdvance(UnsignedInt glyph) override;
        MAGNUM_STBTRUETYPEFONT_LOCAL void doFillGlyphCache(GlyphCache& cache, const std::u32string& characters) override;
        MAGNUM_STBTRUETYPEFONT_LOCAL std::unique_ptr<AbstractLayouter> doLayout(const GlyphCache& cache, Float size, const std::string& text) override;

        std::unique_ptr<Font> _font;
};

}}

#endif
