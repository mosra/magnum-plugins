#ifndef Magnum_Trade_PngImporter_h
#define Magnum_Trade_PngImporter_h
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
 * @brief Class @ref Magnum::Trade::PngImporter
 */

#include <Corrade/Containers/Array.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/PngImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_PNGIMPORTER_BUILD_STATIC
    #ifdef PngImporter_EXPORTS
        #define MAGNUM_PNGIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_PNGIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_PNGIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_PNGIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_PNGIMPORTER_EXPORT
#define MAGNUM_PNGIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief PNG importer plugin

Supports Portable Network Graphics (`*.png`) grayscale, grayscale+alpha, RGB or
RGBA images with 8 and 16 bits per channel. Palleted images and images with
transparency mask are automatically converted to G(A) / RGB(A). You can use
@ref PngImageConverter to encode images into this format.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [libPNG](http://www.libpng.org/pub/png/libpng.html) library, released under
    the @m_class{m-label m-success} **libPNG** license
    ([license text](http://libpng.org/pub/png/src/libpng-LICENSE.txt)). It
    requires attribution for public use.

@section Trade-PngImporter-usage Usage

This plugin depends on the @ref Trade and [libPNG](http://www.libpng.org/pub/png/libpng.html)
libraries and is built if `WITH_PNGIMPORTER` is enabled when building Magnum
Plugins. To use as a dynamic plugin, load @cpp "PngImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using libPNG itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(WITH_PNGIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::PngImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `PngImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::PngImporter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED PngImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::PngImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-PngImporter-behavior Behavior and limitations

Grayscale images are imported as @ref PixelFormat::R8Unorm /
@ref PixelFormat::R16Unorm, grayscale + alpha as @ref PixelFormat::RG8Unorm /
@ref PixelFormat::RG16Unorm, RGB as @ref PixelFormat::RGB8Unorm /
@ref PixelFormat::RGB16Unorm and RGBA as @ref PixelFormat::RGBA8Unorm /
@ref PixelFormat::RGBA16Unorm. All imported images use default
@ref PixelStorage parameters.

@subsection Trade-PngImporter-behavior-cgbi Apple CgBI PNGs

CgBI is a proprietary Apple-specific extension to PNG
([details here](http://iphonedevwiki.net/index.php/CgBI_file_format)). Stock
`libPNG` doesn't support these, you might want to use some fork that supports
it, for example https://github.com/jongwook/libpng. Or use @ref StbImageImporter,
which has the support.

The test for this plugin contains a file that can be used for verifying CgBI
support.
*/
class MAGNUM_PNGIMPORTER_EXPORT PngImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit PngImporter();

        /** @brief Plugin manager constructor */
        explicit PngImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~PngImporter();

    private:
        MAGNUM_PNGIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_PNGIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_PNGIMPORTER_LOCAL void doClose() override;
        MAGNUM_PNGIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_PNGIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_PNGIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        Containers::Array<unsigned char> _in;
};

}}

#endif
