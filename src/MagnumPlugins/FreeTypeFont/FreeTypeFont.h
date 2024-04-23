#ifndef Magnum_Text_FreeTypeFont_h
#define Magnum_Text_FreeTypeFont_h
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

/** @file
 * @brief Class @ref Magnum::Text::FreeTypeFont
 */

#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/Macros.h> /* CORRADE_THREAD_LOCAL */
#include <Corrade/Utility/VisibilityMacros.h>
#include <Magnum/Text/AbstractFont.h>

#include "MagnumPlugins/FreeTypeFont/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
struct FT_LibraryRec_;
typedef FT_LibraryRec_* FT_Library;
struct FT_FaceRec_;
typedef FT_FaceRec_*  FT_Face;

#ifndef MAGNUM_FREETYPEFONT_BUILD_STATIC
    #ifdef FreeTypeFont_EXPORTS
        #define MAGNUM_FREETYPEFONT_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_FREETYPEFONT_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_FREETYPEFONT_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_FREETYPEFONT_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_FREETYPEFONT_EXPORT
#define MAGNUM_FREETYPEFONT_LOCAL
#endif

namespace Magnum { namespace Text {

/**
@brief FreeType font plugin

@m_keywords{TrueTypeFont OpenTypeFont}

Uses the [FreeType](https://freetype.org) library to load fonts of the
following formats:

-   [OpenType](https://en.wikipedia.org/wiki/OpenType) (`*.otf`)
-   [TrueType](https://en.wikipedia.org/wiki/TrueType) (`*.ttf`)
-   [WOFF and WOFF2](https://en.wikipedia.org/wiki/Web_Open_Font_Format)
    (`*.woff`, `*.woff2`)
-   [Type 1](https://en.wikipedia.org/wiki/PostScript_fonts) (`*.pfb`, `*.pfm`,
    `*.afm`, `*.ofm`...)
-   Windows Font (`*.fon`, `*.fnt`)
-   [X11 PCF](https://en.wikipedia.org/wiki/Portable_Compiled_Format) (`*.pcf`)
-   [BDF](https://en.wikipedia.org/wiki/Glyph_Bitmap_Distribution_Format)
    (`*.bdf`)
-   CID, CFF

This plugin provides the `TrueTypeFont` and `OpenTypeFont` plugins.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the [FreeType](https://freetype.org/)
    library, released under the @m_class{m-label m-success} **FreeType license**
    ([license text](https://gitlab.freedesktop.org/freetype/freetype/-/blob/master/docs/FTL.TXT))
    or @m_class{m-label m-danger} **GPLv2**
    ([license text](https://gitlab.freedesktop.org/freetype/freetype/-/blob/master/docs/GPLv2.TXT),
    [choosealicense.com](https://choosealicense.com/licenses/gpl-2.0)). It
    requires attribution for public use.

@section Text-FreeTypeFont-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    through the base @ref AbstractFont interface. See its documentation for
    introduction and usage examples.

This plugin depends on the @ref Text and [FreeType](https://freetype.org/)
libraries and is built if `MAGNUM_WITH_FREETYPEFONT` is enabled when building
Magnum Plugins. To use as a dynamic plugin, load @cpp "FreeTypeFont" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using FreeType itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(MAGNUM_WITH_FREETYPEFONT ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::FreeTypeFont)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `FreeTypeFont` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::FreeTypeFont` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED FreeTypeFont)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::FreeTypeFont)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Text-FreeTypeFont-behavior Behavior and limitations

The @ref fillGlyphCache() function expects a @ref PixelFormat::R8Unorm glyph
cache. If the format doesn't match or the glyphs can't fit, it prints a message
to @relativeref{Magnum,Error} and returns @cpp false @ce.

The FreeType library alone doesn't provide any advanced shaping capabilities,
thus @ref AbstractShaper::setScript(),
@relativeref{AbstractShaper,setLanguage()} and
@relativeref{AbstractShaper,setDirection()} are a no-op and return
@cpp false @ce. You're encouraged to use the @ref HarfBuzzFont plugin if you
need these.

While FreeType provides access to font kerning tables, the plugin doesn't use
them at the moment. The feature list passed to @ref AbstractShaper::shape() is
ignored.
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
        explicit FreeTypeFont(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~FreeTypeFont();

    #ifdef DOXYGEN_GENERATING_OUTPUT
    private:
    #else
    protected:
    #endif
        Containers::Array<unsigned char> _data;
        FT_Face _ftFont;

        bool doIsOpened() const override;
        Properties doOpenData(Containers::ArrayView<const char> data, Float size) override;
        void doClose() override;

    private:
        static
        /** @todo Windows don't support dllexported thread-local symbols, work
            around that (access a local symbol through exported getter?) */
        #if defined(CORRADE_BUILD_MULTITHREADED) && !defined(CORRADE_TARGET_WINDOWS)
        CORRADE_THREAD_LOCAL
        #endif
        MAGNUM_FREETYPEFONT_LOCAL FT_Library _library;

        /* Only the interfaces that HarfBuzzFont replaces with its own can be
           MAGNUM_FREETYPEFONT_LOCAL here */

        FontFeatures MAGNUM_FREETYPEFONT_LOCAL doFeatures() const override;

        UnsignedInt doGlyphId(char32_t character) override;

        Vector2 doGlyphSize(UnsignedInt glyph) override;
        Vector2 doGlyphAdvance(UnsignedInt glyph) override;

        bool doFillGlyphCache(AbstractGlyphCache& cache, Containers::ArrayView<const char32_t> characters) override;

        MAGNUM_FREETYPEFONT_LOCAL Containers::Pointer<AbstractShaper> doCreateShaper() override;
};

}}

#endif
