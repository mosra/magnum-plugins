#ifndef Magnum_Trade_AvifImporter_h
#define Magnum_Trade_AvifImporter_h
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
 * @brief Class @ref Magnum::Trade::AvifImporter
 * @m_since_latest_{plugins}
 */

#include <Corrade/Containers/Array.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/AvifImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_AVIFIMPORTER_BUILD_STATIC
    #ifdef AvifImporter_EXPORTS
        #define MAGNUM_AVIFIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_AVIFIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_AVIFIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_AVIFIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_AVIFIMPORTER_EXPORT
#define MAGNUM_AVIFIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief AVIF importer plugin
@m_since_latest_{plugins}

Imports [AV1 Image File Format](https://en.wikipedia.org/wiki/AVIF) (`*.avif`)
images using the [libavif](https://github.com/AOMediaCodec/libavif) library.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [libavif](https://github.com/AOMediaCodec/libavif) library, released under
    the @m_class{m-label m-success} **BSD 2-clause** license
    ([license text](https://github.com/AOMediaCodec/libavif/blob/main/LICENSE),
    [choosealicense.com](https://choosealicense.com/licenses/bsd-2-clause/)).
    It requires attribution for public use.

@section Trade-AvifImporter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    through the base @ref AbstractImporter interface. See its documentation for
    introduction and usage examples.

This plugin depends on the @ref Trade and [libavif](https://github.com/AOMediaCodec/libavif)
libraries and is built if `MAGNUM_WITH_AVIFIMPORTER` is enabled when building
Magnum Plugins. To use as a dynamic plugin, load @cpp "AvifImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using libavif itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(MAGNUM_WITH_AVIFIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::AvifImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `AvifImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::AvifImporter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED AvifImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::AvifImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-AvifImporter-behavior Behavior and limitations

Supports grayscale, grayscale+alpha, RGB and RGBA images with 8, 10 and 12 bits
per channel. 10- and 12-bit images are expanded to 16 bits.

RGB images are imported as @ref PixelFormat::RGB8Unorm /
@ref PixelFormat::RGB16Unorm and RGBA as @ref PixelFormat::RGBA8Unorm /
@ref PixelFormat::RGBA16Unorm. Since version 1.3.0, libavif supports converting
grayscale images as well, which are then imported as @ref PixelFormat::R8Unorm
/ @ref PixelFormat::R16Unorm and grayscale + alpha as
@ref PixelFormat::RG8Unorm / @ref PixelFormat::RG16Unorm. On older versions
they're imported as RGB and RGBA, respectively. All imported images use default
@ref PixelStorage parameters.

The importer always imports only the first image in the file, animation- and
timing-related data are ignored.
*/
class MAGNUM_AVIFIMPORTER_EXPORT AvifImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit AvifImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~AvifImporter();

    private:
        MAGNUM_AVIFIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_AVIFIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_AVIFIMPORTER_LOCAL void doClose() override;
        MAGNUM_AVIFIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;

        MAGNUM_AVIFIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_AVIFIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        Containers::Array<char> _in;
};

}}

#endif
