#ifndef Magnum_Text_HarfBuzzFont_h
#define Magnum_Text_HarfBuzzFont_h
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
 * @brief Class @ref Magnum::Text::HarfBuzzFont
 */

#include "MagnumPlugins/FreeTypeFont/FreeTypeFont.h"

#include "MagnumPlugins/HarfBuzzFont/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_HARFBUZZFONT_BUILD_STATIC
    #ifdef HarfBuzzFont_EXPORTS
        #define MAGNUM_HARFBUZZFONT_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_HARFBUZZFONT_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_HARFBUZZFONT_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_HARFBUZZFONT_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_HARFBUZZFONT_EXPORT
#define MAGNUM_HARFBUZZFONT_LOCAL
#endif

#ifndef DOXYGEN_GENERATING_OUTPUT
struct hb_font_t;
#endif

namespace Magnum { namespace Text {

/**
@brief HarfBuzz font plugin

@m_keywords{TrueTypeFont OpenTypeFont}

Improves @ref FreeTypeFont with [HarfBuzz](http://www.freedesktop.org/wiki/Software/HarfBuzz)
text layouting capabilities, such as kerning, ligatures etc.

This plugin provides the `TrueTypeFont` and `OpenTypeFont` plugins.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the [HarfBuzz](http://harfbuzz.org/)
    library, licensed under @m_class{m-label m-success} **MIT**
    ([license text](https://raw.githubusercontent.com/behdad/harfbuzz/master/COPYING),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)). It
    requires attribution for public use. In turn it depends on @ref FreeTypeFont
    and thus FreeType, see its documentation for further license info.

@section Text-HarfBuzzFont-usage Usage

This plugin depends on the @ref Text and
[HarfBuzz](http://www.freedesktop.org/wiki/Software/HarfBuzz) libraries and the
@ref FreeTypeFont plugin. It is built if `WITH_HARFBUZZFONT` is enabled when
building Magnum Plugins. To use as a dynamic plugin, load @cpp "HarfBuzzFont" @ce
via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using HarfBuzz itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(WITH_HARFBUZZFONT ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::HarfBuzzFont)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindHarfBuzz.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindHarfBuzz.cmake)
into your `modules/` directory, request the `HarfBuzzFont` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::HarfBuzzFont` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED HarfBuzzFont)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::HarfBuzzFont)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class MAGNUM_HARFBUZZFONT_EXPORT HarfBuzzFont: public FreeTypeFont {
    public:
        /**
         * @brief Initialize the HarfBuzz library
         *
         * Empty in order to avoid @ref FreeTypeFont::initialize() being called
         * again when initializing this plugin.
         */
        static void initialize() {}

        /**
         * @brief Finalize the HarfBuzz library
         *
         * Empty in order to avoid @ref FreeTypeFont::finalize() being called
         * again when initializing this plugin.
         */
        static void finalize() {}

        /** @brief Default constructor */
        explicit HarfBuzzFont();

        /** @brief Plugin manager constructor */
        explicit HarfBuzzFont(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~HarfBuzzFont();

    private:
        MAGNUM_HARFBUZZFONT_LOCAL FontFeatures doFeatures() const override;
        MAGNUM_HARFBUZZFONT_LOCAL bool doIsOpened() const override;
        MAGNUM_HARFBUZZFONT_LOCAL Metrics doOpenData(Containers::ArrayView<const char> data, Float size) override;
        MAGNUM_HARFBUZZFONT_LOCAL void doClose() override;
        MAGNUM_HARFBUZZFONT_LOCAL Containers::Pointer<AbstractLayouter> doLayout(const AbstractGlyphCache& cache, Float size, const std::string& text) override;

        hb_font_t* hbFont;
};

}}

#endif
