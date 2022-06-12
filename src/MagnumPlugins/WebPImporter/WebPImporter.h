#ifndef Magnum_Trade_WebPImporter_h
#define Magnum_Trade_WebPImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2022 Melike Batihan <melikebatihan@gmail.com>

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
 * @brief Class @ref Magnum::Trade::WebPImporter
 * @m_since_latest_{plugins}
 */

#include <Corrade/Containers/Array.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/WebPImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_WEBPIMPORTER_BUILD_STATIC
    #ifdef WebPImporter_EXPORTS
        #define MAGNUM_WEBPIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_WEBPIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_WEBPIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_WEBPIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_WEBPIMPORTER_EXPORT
#define MAGNUM_WEBPIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief WebP importer plugin
@m_since_latest_{plugins}

Supports [WebP](https://en.wikipedia.org/wiki/WebP) (`*.webp`) RGB and RGBA
images.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [libwebp](https://chromium.googlesource.com/webm/libwebp/) library,
    released under the @m_class{m-label m-success} **BSD 3-clause** license as
    part of the WebM project ([license text](https://www.webmproject.org/license/software/),
    [choosealicense.com](https://choosealicense.com/licenses/bsd-3-clause/)).
    It requires attribution for public use.

@section Trade-WebPImporter-usage Usage

This plugin depends on the @ref Trade and [libwebp](https://chromium.googlesource.com/webm/libwebp/)
libraries and is built if `MAGNUM_WITH_WEBPIMPORTER` is enabled when building
Magnum Plugins. To use as a dynamic plugin, load @cpp "WebPImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using libwebp itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(MAGNUM_WITH_WEBPIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::WebPImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindWebP.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindWebP.cmake)
into your `modules/` directory, request the `WebPImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::WebPImporter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED WebPImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::WebPImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-WebPImporter-behavior Behavior and limitations

WebP only supports 24-bit RGB color with an optional 8-bit alpha channel, which
is imported as either @ref PixelFormat::RGB8Unorm or
@ref PixelFormat::RGBA8Unorm. It doesn't have a special colorspace for
grayscale, those are encoded the same way as RGB.

The importer doesn't support decoding of animated WebP files.
*/
class MAGNUM_WEBPIMPORTER_EXPORT WebPImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit WebPImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~WebPImporter();

    private:
        MAGNUM_WEBPIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_WEBPIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_WEBPIMPORTER_LOCAL void doClose() override;
        MAGNUM_WEBPIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;

        MAGNUM_WEBPIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_WEBPIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        Containers::Array<char> _in;
};

}}

#endif
