#ifndef Magnum_Trade_PngImageConverter_h
#define Magnum_Trade_PngImageConverter_h
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
 * @brief Class @ref Magnum::Trade::PngImageConverter
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/PngImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_PNGIMAGECONVERTER_BUILD_STATIC
    #ifdef PngImageConverter_EXPORTS
        #define MAGNUM_PNGIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_PNGIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_PNGIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_PNGIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_PNGIMAGECONVERTER_EXPORT
#define MAGNUM_PNGIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief PNG image converter plugin

Creates Portable Network Graphics (`*.png`) files from images with format
@ref PixelFormat::R8Unorm / @ref PixelFormat::R16Unorm,
@ref PixelFormat::RG8Unorm / @ref PixelFormat::RG16Unorm,
@ref PixelFormat::RGB8Unorm / @ref PixelFormat::RGB16Unorm or
@ref PixelFormat::RGBA8Unorm / @ref PixelFormat::RGBA16Unorm.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [libPNG](http://www.libpng.org/pub/png/libpng.html) library, released under
    the @m_class{m-label m-success} **libPNG** license
    ([license text](http://libpng.org/pub/png/src/libpng-LICENSE.txt)). It
    requires attribution for public use.

@section Trade-PngImageConverter-usage Usage

This plugin depends on the @ref Trade and [libPNG](http://www.libpng.org/pub/png/libpng.html)
libraries and is built if `WITH_PNGIMAGECONVERTER` is enabled when building
Magnum Plugins. To use as a dynamic plugin, load @cpp "PngImageConverter" @ce
via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using libPNG itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(WITH_PNGIMAGECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::PngImageConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `PngImageConverter` component of
the `MagnumPlugins` package and link to the `MagnumPlugins::PngImageConverter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED PngImageConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::PngImageConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.
*/
class MAGNUM_PNGIMAGECONVERTER_EXPORT PngImageConverter: public AbstractImageConverter {
    public:
        /** @brief Default constructor */
        explicit PngImageConverter();

        /** @brief Plugin manager constructor */
        explicit PngImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_PNGIMAGECONVERTER_LOCAL ImageConverterFeatures doFeatures() const override;
        MAGNUM_PNGIMAGECONVERTER_LOCAL Containers::Array<char> doExportToData(const ImageView2D& image) override;
};

}}

#endif
