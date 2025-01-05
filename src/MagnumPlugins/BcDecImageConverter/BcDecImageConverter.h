#ifndef Magnum_Trade_BcDecImageConverter_h
#define Magnum_Trade_BcDecImageConverter_h
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
 * @brief Class @ref Magnum::Trade::BcDecImageConverter
 * @m_since_latest_{plugins}
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/BcDecImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_BCDECIMAGECONVERTER_BUILD_STATIC
    #ifdef BcDecImageConverter_EXPORTS
        #define MAGNUM_BCDECIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_BCDECIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_BCDECIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_BCDECIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_BCDECIMAGECONVERTER_EXPORT
#define MAGNUM_BCDECIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief BCn-compressed image decoding using bcdec
@m_since_latest_{plugins}

Decodes BC1, BC2, BC3, BC4, BC5, BC6H and BC7 blocks to uncompressed RGBA using
the [bcdec](https://github.com/iOrange/bcdec) library. See also the
@ref EtcDecImageConverter plugin for decoding ETC and EAC images.

@m_class{m-block m-primary}

@thirdparty This plugin makes use of the
    [bcdec](https://github.com/iOrange/bcdec) library by Sergii Kudlai,
    released into the @m_class{m-label m-primary} **public domain**
    ([license text](https://github.com/iOrange/bcdec/blob/026acf98ea271045cb10713daa96ba98528badb7/LICENSE#L27-L52),
    [choosealicense.com](https://choosealicense.com/licenses/unlicense/)), or
    alternatively under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/iOrange/bcdec/blob/026acf98ea271045cb10713daa96ba98528badb7/LICENSE#L4-L24),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).

@section Trade-BcDecImageConverter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    via the base @ref AbstractImageConverter interface. See its documentation
    for introduction and usage examples.

This plugin depends on the @ref Trade library and is built if
`MAGNUM_WITH_BCDECIMAGECONVERTER` is enabled when building Magnum Plugins. To
use as a dynamic plugin, load @cpp "BcDecImageConverter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(MAGNUM_WITH_BCDECIMAGECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::BcDecImageConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `BcDecImageConverter` component
of the `MagnumPlugins` package and link to the
`MagnumPlugins::BcDecImageConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED BcDecImageConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::BcDecImageConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

@section Trade-BcDecImageConverter-behavior Behavior and limitations

The following formats are supported:

-   @ref CompressedPixelFormat::Bc1RGBUnorm,
    @relativeref{CompressedPixelFormat,Bc1RGBAUnorm},
    @relativeref{CompressedPixelFormat,Bc2RGBAUnorm},
    @relativeref{CompressedPixelFormat,Bc3RGBAUnorm} and
    @relativeref{CompressedPixelFormat,Bc7RGBAUnorm} is decoded to
    @ref PixelFormat::RGBA8Unorm
-   @ref CompressedPixelFormat::Bc1RGBSrgb,
    @relativeref{CompressedPixelFormat,Bc1RGBASrgb},
    @relativeref{CompressedPixelFormat,Bc2RGBASrgb},
    @relativeref{CompressedPixelFormat,Bc3RGBASrgb} and
    @relativeref{CompressedPixelFormat,Bc7RGBASrgb} is decoded to
    @ref PixelFormat::RGBA8Srgb
-   @ref CompressedPixelFormat::Bc4RUnorm /
    @relativeref{CompressedPixelFormat,Bc4RSnorm} is decoded to
    @ref PixelFormat::R8Unorm / @relativeref{PixelFormat,R8Snorm}
-   @ref CompressedPixelFormat::Bc5RGUnorm /
    @relativeref{CompressedPixelFormat,Bc5RGSnorm} is decoded to
    @ref PixelFormat::RG8Unorm / @relativeref{PixelFormat,RG8Snorm}
-   @ref CompressedPixelFormat::Bc6hRGBUfloat /
    @relativeref{CompressedPixelFormat,Bc6hRGBSfloat} is decoded to
    @ref PixelFormat::RGB16F by default, and to @ref PixelFormat::RGB32F if the
    @cb{.ini} bc6hToFloat @ce @ref Trade-BcDecImageConverter-configuration "configuration option"
    is enabled

The output image always has data for whole 4x4 blocks, if the actual size isn't
whole blocks, @ref PixelStorage::setRowLength() is set to treat the extra
pixels at the end of each row as padding. Non-default @ref CompressedPixelStorage
isn't supported in input images.

Only 2D image conversion is supported at the moment. Image flags, if any, are
passed through unchanged.

@section Trade-BcDecImageConverter-configuration Plugin-specific configuration

It's possible to tune various conversion options through @ref configuration().
See below for all options and their default values:

@snippet MagnumPlugins/BcDecImageConverter/BcDecImageConverter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_BCDECIMAGECONVERTER_EXPORT BcDecImageConverter: public AbstractImageConverter {
    public:
        /** @brief Plugin manager constructor */
        explicit BcDecImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

    private:
        MAGNUM_BCDECIMAGECONVERTER_LOCAL ImageConverterFeatures doFeatures() const override;

        MAGNUM_BCDECIMAGECONVERTER_LOCAL Containers::Optional<ImageData2D> doConvert(const CompressedImageView2D& image) override;
};

}}

#endif
