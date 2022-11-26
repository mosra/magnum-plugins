#ifndef Magnum_Trade_StbResizeImageConverter_h
#define Magnum_Trade_StbResizeImageConverter_h
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
 * @brief Class @ref Magnum::Trade::StbResizeImageConverter
 * @m_since_latest_{plugins}
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/StbResizeImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_STBRESIZEIMAGECONVERTER_BUILD_STATIC
    #ifdef StbResizeImageConverter_EXPORTS
        #define MAGNUM_STBRESIZEIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_STBRESIZEIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_STBRESIZEIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_STBRESIZEIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_STBRESIZEIMAGECONVERTER_EXPORT
#define MAGNUM_STBRESIZEIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief Image resizing using stb_image_resize
@m_since_latest_{plugins}

Performs downsampling and upsampling of 8-bit, 16-bit and float 2D and 2D array
or cube images using the [stb_image_resize](https://github.com/nothings/stb)
library.

@m_class{m-block m-primary}

@thirdparty This plugin makes use of the
    [stb_image_resize](https://github.com/nothings/stb) library by Jorge L.
    Rodriguez and Sean Barrett, released into the
    @m_class{m-label m-primary} **public domain**
    ([license text](https://github.com/nothings/stb/blob/af1a5bc352164740c1cc1354942b1c6b72eacb8a/stb_image_resize.h#L2616-L2632),
    [choosealicense.com](https://choosealicense.com/licenses/unlicense/)),
    or alternatively under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/nothings/stb/blob/af1a5bc352164740c1cc1354942b1c6b72eacb8a/stb_image_resize.h#L2598-L2614),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).

@section Trade-StbResizeImageConverter-usage Usage

This plugin depends on the @ref Trade library and is built if
`MAGNUM_WITH_STBRESIZEIMAGECONVERTER` is enabled when building Magnum Plugins.
To use as a dynamic plugin, load @cpp "StbResizeImageConverter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(MAGNUM_WITH_STBRESIZEIMAGECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::StbResizeImageConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `StbResizeImageConverter` component
of the `MagnumPlugins` package and link to the
`MagnumPlugins::StbResizeImageConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED StbResizeImageConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::StbResizeImageConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

@section Trade-StbResizeImageConverter-behavior Behavior and limitations

Accepts @ref PixelFormat::RGBA8Unorm, @relativeref{PixelFormat,RGBA8Srgb},
@relativeref{PixelFormat,RGBA16Unorm}, @relativeref{PixelFormat,RGBA32F} and
their 1-, 2- and 3-component variants. In order to perform a conversion, you
have to set the @cb{.ini} size @ce @ref Trade-StbResizeImageConverter-configuration "configuration option".

Image flags are passed through unchanged. As the resizing operation operates in
two dimensions, the @cb{.ini} size @ce option always takes a 2D size. 1D images
and 1D array images (with @ref ImageFlag2D::Array set) are not supported, 3D
images are expected to have either @ref ImageFlag3D::Array nor
@ref ImageFlag3D::CubeMap set.

@section Trade-StbResizeImageConverter-configuration Plugin-specific configuration

Apart from the mandatory @cb{.ini} size @ce, other options can be set through
@ref configuration(). See below for all options and their default values:

@snippet MagnumPlugins/StbResizeImageConverter/StbResizeImageConverter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_STBRESIZEIMAGECONVERTER_EXPORT StbResizeImageConverter: public AbstractImageConverter {
    public:
        /** @brief Plugin manager constructor */
        explicit StbResizeImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

    private:
        MAGNUM_STBRESIZEIMAGECONVERTER_LOCAL ImageConverterFeatures doFeatures() const override;
        MAGNUM_STBRESIZEIMAGECONVERTER_LOCAL Containers::Optional<ImageData2D> doConvert(const ImageView2D& image) override;
        MAGNUM_STBRESIZEIMAGECONVERTER_LOCAL Containers::Optional<ImageData3D> doConvert(const ImageView3D& image) override;
};

}}

#endif
