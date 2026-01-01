#ifndef Magnum_Text_HarfBuzzFont_h
#define Magnum_Text_HarfBuzzFont_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
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

Improves @ref FreeTypeFont with text shaping capabilities provided by the
[HarfBuzz](https://harfbuzz.github.io) library.

This plugin provides the `TrueTypeFont` and `OpenTypeFont` plugins.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the [HarfBuzz](https://harfbuzz.github.io)
    library, licensed under @m_class{m-label m-success} **MIT**
    ([license text](https://raw.githubusercontent.com/harfbuzz/harfbuzz/main/COPYING),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)). It
    requires attribution for public use. In turn it depends on @ref FreeTypeFont
    and thus FreeType, see its documentation for further license info.

@section Text-HarfBuzzFont-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    through the base @ref AbstractFont interface. See its documentation for
    introduction and usage examples.

This plugin depends on the @ref Text and [HarfBuzz](https://harfbuzz.github.io)
libraries and the @ref FreeTypeFont plugin. It is built if
`MAGNUM_WITH_HARFBUZZFONT` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "HarfBuzzFont" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using HarfBuzz itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(MAGNUM_WITH_HARFBUZZFONT ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::HarfBuzzFont)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `HarfBuzzFont` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::HarfBuzzFont` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED HarfBuzzFont)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::HarfBuzzFont)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins amd
@ref file-formats for more information.

@section Text-HarfBuzzFont-behavior Behavior and limitations

This plugin implements @ref AbstractShaper::setScript(),
@relativeref{AbstractShaper,setLanguage()} and
@relativeref{AbstractShaper,setDirection()}. With HarfBuzz 5.2+ all
@ref Script values are supported, with older versions the function will return
@cpp false @ce if given script value is not supported. See documentation of the
@m_class{m-doc-external} [hb_script_t](https://harfbuzz.github.io/harfbuzz-hb-common.html#hb-script-t)
enum for more information about which scripts are supported on which version.

If @ref AbstractShaper::setScript(), @relativeref{AbstractShaper,setLanguage()}
or @relativeref{AbstractShaper,setDirection()} isn't called or it's explicitly
set to unspecified values, the library will attempt to guess the properties.
Script and direction is guessed from the input text passed to
@ref AbstractShaper::shape(), the language HarfBuzz currently detects from
system locale. The actual properties used for shaping are then exposed through
@ref AbstractShaper::script(), @relativeref{AbstractShaper,language()} and
@relativeref{AbstractShaper,direction()}.

Reusing an @ref AbstractShaper instance that has the script, language or
direction left unspecified will attempt to guess these properties anew for
every new shaped text. On the other hand, if a concrete script, language or
direction value is set, it stays used for subsequent shaped text.

@subsection Text-HarfBuzzFont-behavior-features Typographic features

HarfBuzz [enables the following features by default](https://harfbuzz.github.io/shaping-opentype-features.html):

-   @ref Feature::AboveBaseForms
-   @ref Feature::BelowBaseForms
-   @ref Feature::GlyphCompositionDecomposition
-   @ref Feature::LocalizedForms
-   @ref Feature::MarkPositioning
-   @ref Feature::MarkToMarkPositioning
-   @ref Feature::RequiredLigatures

Additionally, for @ref ShapeDirection::LeftToRight or
@ref ShapeDirection::RightToLeft, HarfBuzz enables the following by default:

-   @ref Feature::ContextualAlternates
-   @ref Feature::ContextualLigatures
-   @ref Feature::CursivePositioning
-   @ref Feature::Distances
-   @ref Feature::Kerning
-   @ref Feature::StandardLigatures
-   @ref Feature::RequiredContextualAlternates

Additionally, when HarfBuzz encounters a fraction slash (`⁄`, U+2044), it looks
backward and forward for decimal digits, enables @ref Feature::Numerators on
the sequence before the slash, @ref Feature::Denominators on the sequence after
the slash and @ref Feature::Fractions on the whole sequence including the
slash.

For @ref ShapeDirection::TopToBottom or @ref ShapeDirection::BottomToTop,
HarfBuzz enables @ref Feature::VerticalWriting by default.

For @ref Script::Hangul, HarfBuzz disables @ref Feature::ContextualAlternates
by default. For Indic scripts and @ref Script::Khmer, HarfBuzz disables
@ref Feature::StandardLigatures by default.
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

        #ifdef MAGNUM_BUILD_DEPRECATED
        /**
         * @brief Default constructor
         * @m_deprecated_since_latest Direct plugin instantiation isn't a
         *      supported use case anymore, instantiate through the plugin
         *      manager instead.
         */
        CORRADE_DEPRECATED("instantiate through the plugin manager instead") explicit HarfBuzzFont();
        #endif

        /** @brief Plugin manager constructor */
        explicit HarfBuzzFont(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~HarfBuzzFont();

    private:
        MAGNUM_HARFBUZZFONT_LOCAL FontFeatures doFeatures() const override;
        MAGNUM_HARFBUZZFONT_LOCAL bool doIsOpened() const override;
        MAGNUM_HARFBUZZFONT_LOCAL Properties doOpenData(Containers::ArrayView<const char> data, Float size) override;
        MAGNUM_HARFBUZZFONT_LOCAL void doClose() override;
        MAGNUM_HARFBUZZFONT_LOCAL Containers::Pointer<AbstractShaper> doCreateShaper() override;

        hb_font_t* _hbFont;
};

}}

#endif
