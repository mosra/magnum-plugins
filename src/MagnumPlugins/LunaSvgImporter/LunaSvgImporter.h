#ifndef Magnum_Trade_LunaSvgImporter_h
#define Magnum_Trade_LunaSvgImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
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
 * @brief Class @ref Magnum::Trade::LunaSvgImporter
 * @m_since_latest_{plugins}
 */

#include <Corrade/Containers/Pointer.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/LunaSvgImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_LUNASVGIMPORTER_BUILD_STATIC
    #ifdef LunaSvgImporter_EXPORTS
        #define MAGNUM_LUNASVGIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_LUNASVGIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_LUNASVGIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_LUNASVGIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_LUNASVGIMPORTER_EXPORT
#define MAGNUM_LUNASVGIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief SVG importer plugin using LunaSVG
@m_since_latest_{plugins}

@m_keywords{SvgImporter}

Rasterizes Scalable Vector Graphics (`*.svg`) using the
[LunaSVG](https://github.com/sammycage/lunasvg) library.

This plugin provides the `SvgImporter` plugin.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [LunaSVG](https://github.com/sammycage/plutosvg) library by
    [Samuel Ugochukwu](https://github.com/sammycage),
    released under @m_class{m-label m-success} **MIT** ([license text](https://github.com/sammycage/lunasvg/blob/master/LICENSE), [choosealicense.com](https://choosealicense.com/licenses/mit/)).
    It requires attribution for public use.

@section Trade-LunaSvgImporter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    through the base @ref AbstractImporter interface. See its documentation for
    introduction and usage examples.

This plugin depends on the @ref Trade library and is built if
`MAGNUM_WITH_LUNASVGIMPORTER` is enabled when building Magnum Plugins. To use
as a dynamic plugin, load @cpp "LunaSvgImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using LunaSVG itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(MAGNUM_WITH_LUNASVGIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::LunaSvgImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `LunaSvgImporter` component of the
`MagnumPlugins` package in CMake and link to the `MagnumPlugins::LunaSvgImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED LunaSvgImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::LunaSvgImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-LunaSvgImporter-behavior Behavior and limitations

The output is always @ref PixelFormat::RGBA8Unorm and the rasterized size by
default corresponds to what's specified in @cb{.cml} <svg viewBox="..."> @ce.
Use the @cb{.ini} dpi @ce @ref Trade-LunaSvgImporter-configuration "configuration option"
to rasterize at a different size.

The library doesn't provide any error status or message in case an import
fails, so the plugin will always print a generic error. Compared to
@ref ResvgImporter, @cb{.xml} <image> @ce elements are supported only if the
image file is embedded. External references are silently ignored without any
error or warning. SVGZ files are not supported, use the @ref ResvgImporter
plugin instead.

@section Trade-LunaSvgImporter-configuration Plugin-specific configuration

It's possible to tune various import options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/LunaSvgImporter/LunaSvgImporter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_LUNASVGIMPORTER_EXPORT LunaSvgImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit LunaSvgImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~LunaSvgImporter();

    private:
        MAGNUM_LUNASVGIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_LUNASVGIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_LUNASVGIMPORTER_LOCAL void doClose() override;
        MAGNUM_LUNASVGIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;

        MAGNUM_LUNASVGIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_LUNASVGIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
