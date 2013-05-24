#ifndef Magnum_Text_FreeTypeFont_FreeTypeFont_h
#define Magnum_Text_FreeTypeFont_FreeTypeFont_h
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
 * @brief Class Magnum::Text::FreeTypeFont::FreeTypeFontRenderer, Magnum::Text::FreeTypeFont::FreeTypeFont
 */

#include <Text/AbstractFont.h>

#ifndef DOXYGEN_GENERATING_OUTPUT
struct FT_LibraryRec_;
typedef FT_LibraryRec_* FT_Library;
struct FT_FaceRec_;
typedef FT_FaceRec_*  FT_Face;

#ifdef _WIN32
    #ifdef FreeTypeFont_EXPORTS
        #define MAGNUM_FREETYPEFONT_EXPORT __declspec(dllexport)
    #else
        #define MAGNUM_FREETYPEFONT_EXPORT __declspec(dllimport)
    #endif
    #define MAGNUM_FREETYPEFONT_LOCAL
#else
    #define MAGNUM_FREETYPEFONT_EXPORT __attribute__ ((visibility ("default")))
    #define MAGNUM_FREETYPEFONT_LOCAL __attribute__ ((visibility ("hidden")))
#endif
#endif

namespace Magnum { namespace Text { namespace FreeTypeFont {

/**
@brief FreeType font

The font can be created either from file or from memory location of format
supported by [FreeType](http://www.freetype.org/) library.
*/
class MAGNUM_FREETYPEFONT_EXPORT FreeTypeFont: public AbstractFont {
    public:
        /** @brief Initialize FreeType library */
        static void initialize();

        /** @brief Finalize FreeType library */
        static void finalize();

        /** @brief Default constructor */
        explicit FreeTypeFont();

        /** @brief Plugin manager constructor */
        explicit FreeTypeFont(PluginManager::AbstractManager* manager, std::string plugin);

        ~FreeTypeFont();

        bool open(const std::string& filename, Float size) override;
        bool open(const unsigned char* data, std::size_t dataSize, Float size) override;
        void close() override;
        void createGlyphCache(GlyphCache* const cache, const std::string& characters) override;
        AbstractLayouter* layout(const GlyphCache* const cache, const Float size, const std::string& text) override;

    #ifdef DOXYGEN_GENERATING_OUTPUT
    private:
    #else
    protected:
    #endif
        static MAGNUM_FREETYPEFONT_LOCAL FT_Library library;

        FT_Face ftFont;
};

}}}

#endif
