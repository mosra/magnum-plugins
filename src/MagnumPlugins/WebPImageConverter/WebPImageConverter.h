#ifndef Magnum_Trade_WebPImageConverter_h
#define Magnum_Trade_WebPImageConverter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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
 * @brief Class @ref Magnum::Trade::WebPImageConverter
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/WebPImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_WEBPIMAGECONVERTER_BUILD_STATIC
    #ifdef WebPImageConverter_EXPORTS
        #define MAGNUM_WEBPIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_WEBPIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_WEBPIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_WEBPIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_WEBPIMAGECONVERTER_EXPORT
#define MAGNUM_WEBPIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief WEBP image converter plugin

Creates Portable Network Graphics (`*.webp`) files from images with format
@ref PixelFormat::R8Unorm / @ref PixelFormat::R16Unorm,
@ref PixelFormat::RG8Unorm / @ref PixelFormat::RG16Unorm,
@ref PixelFormat::RGB8Unorm / @ref PixelFormat::RGB16Unorm or
@ref PixelFormat::RGBA8Unorm / @ref PixelFormat::RGBA16Unorm. You can use
@ref WebPImporter to import images in this format.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [libwebp](https://chromium.googlesource.com/webm/libwebp/) library,
    released under the @m_class{m-label m-success} **BSD 3-clause** license as
    part of the WebM project ([license text](https://www.webmproject.org/license/software/),
    [choosealicense.com](https://choosealicense.com/licenses/bsd-3-clause/)).
    It requires attribution for public use.

@section Trade-WebPImageConverter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    via the base @ref AbstractImageConverter interface. See its documentation
    for introduction and usage examples.

This plugin depends on the @ref Trade and [libwebp](https://chromium.googlesource.com/webm/libwebp/)
libraries and is built if `MAGNUM_WITH_WEBPIMPORTER` is enabled when building
Magnum Plugins. To use as a dynamic plugin, load @cpp "WebPImageConverter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using libwebp itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(MAGNUM_WITH_WEBPIMAGECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::WebPImageConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindWebP.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindWebP.cmake)
into your `modules/` directory, request the `WebPImageConverter` component of
the `MagnumPlugins` package and link to the `MagnumPlugins::WebPImageConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED WebPImageConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::WebPImageConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-WebPImageConverter-behavior Behavior and limitations

The PNG file format doesn't have a way to distinguish between 2D and 1D array
images. If an image has @ref ImageFlag2D::Array set, a warning is printed and
the file is saved as a regular 2D image.
*/
class MAGNUM_WEBPIMAGECONVERTER_EXPORT WebPImageConverter: public AbstractImageConverter {
    public:
        /** @brief Default constructor */
        explicit WebPImageConverter();

        /** @brief Plugin manager constructor */
        explicit WebPImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

    private:
        MAGNUM_WEBPIMAGECONVERTER_LOCAL ImageConverterFeatures doFeatures() const override;
        MAGNUM_WEBPIMAGECONVERTER_LOCAL Containers::String doExtension() const override;
        MAGNUM_WEBPIMAGECONVERTER_LOCAL Containers::String doMimeType() const override;

        MAGNUM_WEBPIMAGECONVERTER_LOCAL Containers::Optional<Containers::Array<char>> doConvertToData(const ImageView2D& image) override;
};

}}

#endif
