#ifndef Magnum_Text_FreeTypeFont_h
#define Magnum_Text_FreeTypeFont_h
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
 * @brief Class @ref Magnum::Text::FreeTypeFont
 */

#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/VisibilityMacros.h>
#include <Magnum/Text/AbstractFont.h>

#include "MagnumPlugins/FreeTypeFont/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
struct FT_LibraryRec_;
typedef FT_LibraryRec_* FT_Library;
struct FT_FaceRec_;
typedef FT_FaceRec_*  FT_Face;

#ifndef MAGNUM_FREETYPEFONT_BUILD_STATIC
    #if defined(FreeTypeFont_EXPORTS) || defined(FreeTypeFontObjects_EXPORTS)
        #define MAGNUM_TEXT_FREETYPEFONT_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_TEXT_FREETYPEFONT_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_TEXT_FREETYPEFONT_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_TEXT_FREETYPEFONT_LOCAL CORRADE_VISIBILITY_LOCAL
#endif

namespace Magnum { namespace Text {

/**
@brief FreeType font plugin

The font can be created either from file or from memory location of format
supported by [FreeType](http://www.freetype.org/) library.

This plugin depends on **FreeType** library and is built if `WITH_FREETYPEFONT`
is enabled when building Magnum Plugins. To use dynamic plugin, you need to
load `FreeTypeFont` plugin from `MAGNUM_PLUGINS_FONT_DIR`. To use static
plugin, you need to request `FreeTypeFont` component of `MagnumPlugins`
package in CMake and link to `MagnumPlugins::FreeTypeFont` target.

This plugin provides `TrueTypeFont` plugin.

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class MAGNUM_TEXT_FREETYPEFONT_EXPORT FreeTypeFont: public AbstractFont {
    public:
        /** @brief Initialize FreeType library */
        static void initialize();

        /** @brief Finalize FreeType library */
        static void finalize();

        /** @brief Default constructor */
        explicit FreeTypeFont();

        /** @brief Plugin manager constructor */
        explicit FreeTypeFont(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~FreeTypeFont();

    #ifdef DOXYGEN_GENERATING_OUTPUT
    private:
    #else
    protected:
    #endif
        Containers::Array<unsigned char> _data;
        FT_Face ftFont;

        bool doIsOpened() const override;
        Metrics doOpenSingleData(Containers::ArrayView<const char> data, Float size) override;
        void doClose() override;

    private:
        static MAGNUM_TEXT_FREETYPEFONT_LOCAL FT_Library library;

        Features MAGNUM_TEXT_FREETYPEFONT_LOCAL doFeatures() const override;

        UnsignedInt doGlyphId(char32_t character) override;

        Vector2 doGlyphAdvance(UnsignedInt glyph) override;

        /** @todo Why this can't be defined as local? */
        void doFillGlyphCache(GlyphCache& cache, const std::u32string& characters) override;

        std::unique_ptr<AbstractLayouter> MAGNUM_TEXT_FREETYPEFONT_LOCAL doLayout(const GlyphCache& cache, Float size, const std::string& text) override;
};

}}

#endif
