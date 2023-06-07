#ifndef Magnum_Trade_SpngImporter_h
#define Magnum_Trade_SpngImporter_h
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
 * @brief Class @ref Magnum::Trade::SpngImporter
 * @m_since_latest_{plugins}
 */

#include <Corrade/Containers/Array.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/SpngImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_SPNGIMPORTER_BUILD_STATIC
    #ifdef SpngImporter_EXPORTS
        #define MAGNUM_SPNGIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_SPNGIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_SPNGIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_SPNGIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_SPNGIMPORTER_EXPORT
#define MAGNUM_SPNGIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief PNG importer plugin using libspng
@m_since_latest_{plugins}

@m_keywords{PngImporter}

Imports Portable Network Graphics (`*.png`) images using the
[libspng](https://libspng.org) library. Supports grayscale, grayscale+alpha,
RGB and RGBA images with 1, 2, 4, 8 and 16 bits per channel. Palleted images and
images with transparency mask are automatically converted to G(A) / RGB(A).

This plugin provides the `PngImporter` plugin and can be considerably faster
than @ref PngImporter, especially when combined with [zlib-ng](https://github.com/zlib-ng/zlib-ng).

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [libspng](https://libspng.org) library by [\@randy408](https://github.com/randy408),
    released under @m_class{m-label m-success} **BSD 2-clause**
    ([license text](https://github.com/randy408/libspng/blob/master/LICENSE),
    [choosealicense.com](https://choosealicense.com/licenses/bsd-2-clause/)).
    It requires attribution for public use.

@section Trade-SpngImporter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    through the base @ref AbstractImporter interface. See its documentation for
    introduction and usage examples.

This plugin depends on the @ref Trade library and is built if
`MAGNUM_WITH_SPNGIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "SpngImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using libspng itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(MAGNUM_WITH_SPNGIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::SpngImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindSpng.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindSpng.cmake)
into your `modules/` directory, request the `SpngImporter` component of the
`MagnumPlugins` package in CMake and link to the `MagnumPlugins::SpngImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED SpngImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::SpngImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-SpngImporter-behavior Behavior and limitations

The supported format feature set is similar to @ref PngImporter --- grayscale images are imported as @ref PixelFormat::R8Unorm /
@ref PixelFormat::R16Unorm, RGB as @ref PixelFormat::RGB8Unorm /
@ref PixelFormat::RGB16Unorm and RGBA as @ref PixelFormat::RGBA8Unorm /
@ref PixelFormat::RGBA16Unorm. All imported images use default
@ref PixelStorage parameters. Palleted images and images with transparency mask
are automatically converted to G(A) / RGB(A).

The only exception is grayscale + alpha, which is improted as
@ref PixelFormat::RGBA8Unorm in the 8-bit case, as libspng implements bit depth
conversion only for single-, three- and four-channel color types. 16-bit
grayscale + alpha is imported as @ref PixelFormat::RG16Unorm as no conversion
needs to be performed there.

@subsection Trade-SpngImporter-behavior-incomplete-data Handling of incomplete and corrupted data

In order to support Y flipping and row alignment, the importer uses libspng's
progressive decoding which doesn't report errors in case of truncated files or
invalid data chunks. Import of such files will succeed, but the contents will
be incomplete. See [libpng documentation about error handling](https://libspng.org/docs/decode/#error-handling)
for more information.

@subsection Trade-SpngImporter-behavior-cgbi Apple CgBI PNGs

CgBI is a proprietary Apple-specific extension to PNG
([details here](http://iphonedevwiki.net/index.php/CgBI_file_format)),
unfortunately libspng [doesn't plan to support it](https://github.com/randy408/libspng/issues/16).
To import such files use either @ref StbImageImporter or
@ref Trade-PngImporter-behavior-cgbi "PngImporter with a patched libpng".
*/
class MAGNUM_SPNGIMPORTER_EXPORT SpngImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit SpngImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~SpngImporter();

    private:
        MAGNUM_SPNGIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_SPNGIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_SPNGIMPORTER_LOCAL void doClose() override;
        MAGNUM_SPNGIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;

        MAGNUM_SPNGIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_SPNGIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        Containers::Array<char> _in;
};

}}

#endif
