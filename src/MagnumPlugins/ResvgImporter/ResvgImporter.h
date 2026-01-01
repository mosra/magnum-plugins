#ifndef Magnum_Trade_ResvgImporter_h
#define Magnum_Trade_ResvgImporter_h
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
 * @brief Class @ref Magnum::Trade::ResvgImporter
 * @m_since_latest_{plugins}
 */

#include <Corrade/Containers/Pointer.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/ResvgImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_RESVGIMPORTER_BUILD_STATIC
    #ifdef ResvgImporter_EXPORTS
        #define MAGNUM_RESVGIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_RESVGIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_RESVGIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_RESVGIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_RESVGIMPORTER_EXPORT
#define MAGNUM_RESVGIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief SVG importer plugin using resvg
@m_since_latest_{plugins}

@m_keywords{SvgImporter}

Rasterizes Scalable Vector Graphics (`*.svg`) using the
[resvg](https://github.com/linebender/resvg) library. Supports also gzipped SVG
files (`*.svgz`).

This plugin provides the `SvgImporter` plugin.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [resvg](https://github.com/linebender/resvg) library originally by
    [Yevhenii Reizner](https://github.com/RazrFalcon),
    released under either an @m_class{m-label m-success} **Apache-2.0** license
    ([license text](https://github.com/linebender/resvg/blob/main/LICENSE-APACHE),
    [choosealicense.com](https://choosealicense.com/licenses/apache-2.0/)) or
    @m_class{m-label m-success} **MIT** ([license text](https://github.com/linebender/resvg/blob/main/LICENSE-MIT), [choosealicense.com](https://choosealicense.com/licenses/mit/)).
    It requires attribution for public use.

@section Trade-ResvgImporter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    through the base @ref AbstractImporter interface. See its documentation for
    introduction and usage examples.

This plugin depends on the @ref Trade library and is built if
`MAGNUM_WITH_RESVGIMPORTER` is enabled when building Magnum Plugins. To use
as a dynamic plugin, load @cpp "ResvgImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. The resvg library is written in Rust and cannot be added as a CMake
subproject so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary. Note that in order to
build against it, it has to be a library including the `resvg.h` C header, not
just the self-contained executable alone that's provided in the official
releases.

@code{.cmake}
set(MAGNUM_WITH_RESVGIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::ResvgImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `ResvgImporter` component of the
`MagnumPlugins` package in CMake and link to the `MagnumPlugins::ResvgImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED ResvgImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::ResvgImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-ResvgImporter-behavior Behavior and limitations

The output is always @ref PixelFormat::RGBA8Unorm and the rasterized size by
default corresponds to what's specified in @cb{.cml} <svg viewBox="..."> @ce.
Use the @cb{.ini} dpi @ce @ref Trade-ResvgImporter-configuration "configuration option"
to rasterize at a different size.

The resvg library only provides very limited error reporting in case an import
fails, so the plugin will almost always print a generic error message. It can
only log to standard error output in a way that cannot be disabled again
afterwards and thus the logging is never enabled by the plugin.

@subsection Trade-ResvgImporter-behavior-external-data External data references

Resvg supports external @cb{.xml} <image> @ce references, but for them to work
the file has to be opened through @ref openFile(). If the referenced file isn't
found, if a SVG with an external reference is parsed via @ref openData() or any
other error occurs, the resvg library silently ignores it without giving any
error or warning, and the output can also contain random pixel values in that
case.

The library also claims to support text rendering, but such feature so far
wasn't explicitly tested in the plugin implementation.

@section Trade-ResvgImporter-configuration Plugin-specific configuration

It's possible to tune various import options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/ResvgImporter/ResvgImporter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_RESVGIMPORTER_EXPORT ResvgImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit ResvgImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~ResvgImporter();

    private:
        MAGNUM_RESVGIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_RESVGIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_RESVGIMPORTER_LOCAL void doClose() override;
        MAGNUM_RESVGIMPORTER_LOCAL void doOpenFile(Containers::StringView filename) override;
        MAGNUM_RESVGIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;

        MAGNUM_RESVGIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_RESVGIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
