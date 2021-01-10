#ifndef Magnum_Text_StbTrueTypeFont_h
#define Magnum_Text_StbTrueTypeFont_h
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

/** @file
 * @brief Class @ref Magnum::Text::StbTrueTypeFont
 */

#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/VisibilityMacros.h>
#include <Magnum/Text/AbstractFont.h>

#include "MagnumPlugins/StbTrueTypeFont/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_STBTRUETYPEFONT_BUILD_STATIC
    #ifdef StbTrueTypeFont_EXPORTS
        #define MAGNUM_STBTRUETYPEFONT_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_STBTRUETYPEFONT_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_STBTRUETYPEFONT_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_STBTRUETYPEFONT_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_STBTRUETYPEFONT_EXPORT
#define MAGNUM_STBTRUETYPEFONT_LOCAL
#endif

namespace Magnum { namespace Text {

/**
@brief TrueType font plugin using stb_truetype

@m_keywords{TrueTypeFont OpenTypeFont}

Supports opening fonts of the following formats either from file or from memory
location using the [stb_truetype](https://github.com/nothings/stb) library:

-   TrueType fonts (`*.ttf`)
-   OpenType / Type 2 fonts (`*.otf`)

This plugin provides the `TrueTypeFont` and `OpenTypeFont` plugins, but please
note that this plugin trades the simplicity and portability for various
limitations, the most visible being the lack of autohinting. That causes the
rendered glyphs looking blurry compared to for example @ref FreeTypeFont and
because of that the font properties and sizes don't exactly match properties of
fonts opened with @ref FreeTypeFont using the same size.

@m_class{m-block m-primary}

@thirdparty This plugin makes use of the
    [stb_truetype](https://github.com/nothings/stb) library by Sean Barrett,
    released into the @m_class{m-label m-primary} **public domain**
    ([license text](https://github.com/nothings/stb/blob/e6afb9cbae4064da8c3e69af3ff5c4629579c1d2/stb_truetype.h#L4835-L4851),
    [choosealicense.com](https://choosealicense.com/licenses/unlicense/)),
    or alternatively under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/nothings/stb/blob/master/stb_truetype.h#L4817-L4833),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).

@section Text-StbTrueTypeFont-usage Usage

This plugin depends on the @ref Text library and is built if `WITH_STBTRUETYPEFONT`
is enabled when building Magnum Plugins and depends on the @ref Text library.
To use as a dynamic plugin, load @cpp "StbTrueTypeFont" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_STBTRUETYPEFONT ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::StbTrueTypeFont)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `StbTrueTypeFont` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::StbTrueTypeFont`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED StbTrueTypeFont)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::StbTrueTypeFont)
@endcode

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

        MAGNUM_STBTRUETYPEFONT_LOCAL FontFeatures doFeatures() const override;
        MAGNUM_STBTRUETYPEFONT_LOCAL bool doIsOpened() const override;
        MAGNUM_STBTRUETYPEFONT_LOCAL Metrics doOpenData(Containers::ArrayView<const char> data, Float size) override;
        MAGNUM_STBTRUETYPEFONT_LOCAL void doClose() override;
        MAGNUM_STBTRUETYPEFONT_LOCAL UnsignedInt doGlyphId(char32_t character) override;
        MAGNUM_STBTRUETYPEFONT_LOCAL Vector2 doGlyphAdvance(UnsignedInt glyph) override;
        MAGNUM_STBTRUETYPEFONT_LOCAL void doFillGlyphCache(AbstractGlyphCache& cache, const std::u32string& characters) override;
        MAGNUM_STBTRUETYPEFONT_LOCAL Containers::Pointer<AbstractLayouter> doLayout(const AbstractGlyphCache& cache, Float size, const std::string& text) override;

        Containers::Pointer<Font> _font;
};

}}

#endif
