#ifndef Magnum_Trade_EtcDecImageConverter_h
#define Magnum_Trade_EtcDecImageConverter_h
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
 * @brief Class @ref Magnum::Trade::EtcDecImageConverter
 * @m_since_latest_{plugins}
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/EtcDecImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_ETCDECIMAGECONVERTER_BUILD_STATIC
    #ifdef EtcDecImageConverter_EXPORTS
        #define MAGNUM_ETCDECIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_ETCDECIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_ETCDECIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_ETCDECIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_ETCDECIMAGECONVERTER_EXPORT
#define MAGNUM_ETCDECIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief ETC/EAC-compressed image decoding using etcdec
@m_since_latest_{plugins}

Decodes ETC1, ETC2 and EAC blocks to uncompressed RGBA using the
[etcdec](https://github.com/iOrange/etcdec) library. See also the
@ref BcDecImageConverter plugin for decoding BCn images.

@m_class{m-block m-primary}

@thirdparty This plugin makes use of the
    [etcdec](https://github.com/iOrange/etcdec) library by Sergii Kudlai,
    released into the @m_class{m-label m-primary} **public domain**
    ([license text](https://github.com/iOrange/etcdec/blob/70772097f175b849633c4a9f6984ef3da4df8a66/LICENSE#L27-L52),
    [choosealicense.com](https://choosealicense.com/licenses/unlicense/)), or
    alternatively under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/iOrange/etcdec/blob/70772097f175b849633c4a9f6984ef3da4df8a66/LICENSE#L4-L24),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).

@section Trade-EtcDecImageConverter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    via the base @ref AbstractImageConverter interface. See its documentation
    for introduction and usage examples.

This plugin depends on the @ref Trade library and is built if
`MAGNUM_WITH_ETCDECIMAGECONVERTER` is enabled when building Magnum Plugins. To
use as a dynamic plugin, load @cpp "EtcDecImageConverter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(MAGNUM_WITH_ETCDECIMAGECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::EtcDecImageConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `EtcDecImageConverter` component
of the `MagnumPlugins` package and link to the
`MagnumPlugins::EtcDecImageConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED EtcDecImageConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::EtcDecImageConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

@section Trade-EtcDecImageConverter-behavior Behavior and limitations

The following formats are supported:

-   @ref CompressedPixelFormat::EacR11Unorm /
    @relativeref{CompressedPixelFormat,EacR11Snorm} is decoded to
    @ref PixelFormat::R16Unorm / @relativeref{PixelFormat,R16Snorm} by default,
    and to @ref PixelFormat::R32F if the @cb{.ini} eacToFloat @ce
    @ref Trade-EtcDecImageConverter-configuration "configuration option" is
    enabled
-   @ref CompressedPixelFormat::EacRG11Unorm /
    @relativeref{CompressedPixelFormat,EacRG11Snorm} is decoded to
    @ref PixelFormat::RG16Unorm / @relativeref{PixelFormat,RG16Snorm} by
    default, and to @ref PixelFormat::RG32F if the @cb{.ini} eacToFloat @ce
    configuration option is enabled
-   @ref CompressedPixelFormat::Etc2RGB8Unorm,
    @relativeref{CompressedPixelFormat,Etc2RGB8A1Unorm} and
    @relativeref{CompressedPixelFormat,Etc2RGBA8Unorm} is decoded to
    @ref PixelFormat::RGBA8Unorm
-   @ref CompressedPixelFormat::Etc2RGB8Srgb,
    @relativeref{CompressedPixelFormat,Etc2RGB8A1Srgb} and
    @relativeref{CompressedPixelFormat,Etc2RGBA8Srgb} is decoded to
    @ref PixelFormat::RGBA8Srgb

The output image always has data for whole 4x4 blocks, if the actual size isn't
whole blocks, @ref PixelStorage::setRowLength() is set to treat the extra
pixels at the end of each row as padding. Non-default @ref CompressedPixelStorage
isn't supported in input images.

Only 2D image conversion is supported at the moment. Image flags, if any, are
passed through unchanged.

@section Trade-EtcDecImageConverter-configuration Plugin-specific configuration

It's possible to tune various conversion options through @ref configuration().
See below for all options and their default values:

@snippet MagnumPlugins/EtcDecImageConverter/EtcDecImageConverter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_ETCDECIMAGECONVERTER_EXPORT EtcDecImageConverter: public AbstractImageConverter {
    public:
        /** @brief Plugin manager constructor */
        explicit EtcDecImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

    private:
        MAGNUM_ETCDECIMAGECONVERTER_LOCAL ImageConverterFeatures doFeatures() const override;

        MAGNUM_ETCDECIMAGECONVERTER_LOCAL Containers::Optional<ImageData2D> doConvert(const CompressedImageView2D& image) override;
};

}}

#endif
