#ifndef Magnum_Trade_StbImageConverter_h
#define Magnum_Trade_StbImageConverter_h
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
 * @brief Class @ref Magnum::Trade::StbImageConverter
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/StbImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_STBIMAGECONVERTER_BUILD_STATIC
    #ifdef StbImageConverter_EXPORTS
        #define MAGNUM_STBIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_STBIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_STBIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_STBIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_STBIMAGECONVERTER_EXPORT
#define MAGNUM_STBIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief Image converter plugin using stb_image_write

@m_keywords{StbBmpImageConverter StbHdrImageConverter StbJpegImageConverter}
@m_keywords{StbPngImageConverter StbTgaImageConverter BmpImageConverter}
@m_keywords{HdrImageConverter JpegImageConverter PngImageConverter}
@m_keywords{TgaImageConverter}

Creates files in one of the following formats using the
[stb_image_write](https://github.com/nothings/stb) library:

-   Windows Bitmap (`*.bmp`) if the plugin was loaded as `StbBmpImageConverter`
    / `BmpImageConverter` or @ref Format::Bmp was passed to the constructor.
    Accepts @ref PixelFormat::R8Unorm, @ref PixelFormat::RG8Unorm,
    @ref PixelFormat::RGB8Unorm and @ref PixelFormat::RGBA8Unorm. Single- and
    two-channel inputs are converted to three-channel with the the first
    channel repeated three times (and second ignored), four-channel input
    loses alpha.
-   Radiance HDR (`*.hdr`) if the plugin was loaded as `StbHdrImageConverter`
    / `HdrImageConverter` or @ref Format::Hdr was passed to the constructor.
    Accepts @ref PixelFormat::R32F, @ref PixelFormat::RG32F,
    @ref PixelFormat::RGB32F or @ref PixelFormat::RGBA32F. R and RG inputs are
    converted to three-channel RRR with G ignored, four-channel input loses
    alpha.
-   JPEG (`*.jpg`, `*.jpe`, `*.jpeg`) if the plugin was loaded as
    `StbJpegImageConverter` / `JpegImageConverter` or @ref Format::Jpeg was
    passed to the constructor. Accepts @ref PixelFormat::R8Unorm,
    @ref PixelFormat::RG8Unorm, @ref PixelFormat::RGB8Unorm and
    @ref PixelFormat::RGBA8Unorm. Single- and two-channel inputs are converted
    to three-channel with the first channel repeated three times (and second
    ignored), four-channel input loses alpha.
-   Portable Network Graphics (`*.png`) if the plugin was loaded as
    `StbPngImageConverter` / `PngImageConverter` or @ref Format::Png was passed
    to the constructor. Accepts @ref PixelFormat::R8Unorm,
    @ref PixelFormat::RG8Unorm, @ref PixelFormat::RGB8Unorm and
    @ref PixelFormat::RGBA8Unorm, output has the same amount of channels as
    input.
-   Truevision TGA (`*.tga`, `*.vda`, `*.icb`, `*.vst`) if the plugin was
    loaded as `StbTgaImageConverter` / `TgaImageConverter` or @ref Format::Tga
    was passed to the constructor. Accepts @ref PixelFormat::R8Unorm,
    @ref PixelFormat::RG8Unorm, @ref PixelFormat::RGB8Unorm and
    @ref PixelFormat::RGBA8Unorm, output has the same amount of channels as
    input.

If the conversion results in lost channels (such as saving RGBA to a JPEG,
losing the alpha in process), a warning is printed to the output.

Besides `StbBmpImageConverter`, `StbHdrImageConverter`, `StbJpegImageConverter`,
`StbPngImageConverter` and `StbTgaImageConverter` aliases this plugin provides
also `BmpImageConverter`, `HdrImageConverter`, `JpegImageConverter`,
`PngImageConverter` and `TgaImageConverter` plugins, but note that this plugin
may generate slightly larger files and the performance might be worse than when
using plugins dedicated for given format.

@m_class{m-block m-primary}

@thirdparty This plugin makes use of the
    [stb_image_write](https://github.com/nothings/stb) library by Sean Barrett,
    released into the @m_class{m-label m-primary} **public domain**
    ([license text](https://github.com/nothings/stb/blob/e6afb9cbae4064da8c3e69af3ff5c4629579c1d2/stb_image_write.h#L1550-L1566),
    [choosealicense.com](https://choosealicense.com/licenses/unlicense/)),
    or alternatively under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/nothings/stb/blob/e6afb9cbae4064da8c3e69af3ff5c4629579c1d2/stb_image_write.h#L1532-L1548),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).

@section Trade-StbImageConverter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_STBIMAGECONVERTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load one of the @cpp "StbBmpImageConverter" @ce,
@cpp "StbHdrImageConverter" @ce, @cpp "StbPngImageConverter" @ce,
@cpp "StbTgaImageConverter" @ce plugins via @ref Corrade::PluginManager::Manager.
If you would load the plugin as just @cpp "StbImageConverter" @ce, it won't be
able to guess the desired output format.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_STBIMAGECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::StbImageConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `StbImageConverter` component of
the `MagnumPlugins` package and link to the `MagnumPlugins::StbImageConverter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED StbImageConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::StbImageConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-StbImageConverter-behavior Behavior and limitations

@subsection Trade-StbImageConverter-behavior-arithmetic-jpeg Arithmetic JPEG encoding

[Arithmetic coding](https://en.wikipedia.org/wiki/Arithmetic_coding) is
implemented neither in stb_image_write nor in stb_image.

@subsection Trade-StbImageConverter-behavior-16bit-png 16-bit PNGs

Conversion to 16-bit PNGs is not supported. Use @ref PngImageConverter instead
if you need to deal with 16-bit pixel formats.

@section Trade-StbImageConverter-configuration Plugin-specific configuration

For some formats, it's possible to tune various output options through
@ref configuration(). See below for all options and their default values:

@snippet MagnumPlugins/StbImageConverter/StbImageConverter.conf config
*/
class MAGNUM_STBIMAGECONVERTER_EXPORT StbImageConverter: public AbstractImageConverter {
    public:
        /**
         * @brief Output file format
         *
         * @see @ref StbImageConverter(Format)
         */
        enum class Format: Int {
            /* 0 used for invalid value */

            Bmp = 1,    /**< Output BMP images */
            Jpeg,       /**< Output JPEG images */
            Hdr,        /**< Output HDR images */
            Png,        /**< Output PNG images */
            Tga         /**< Output TGA images */
        };

        /**
         * @brief Default constructor
         *
         * The converter outputs files in format defined by @ref Format.
         */
        explicit StbImageConverter(Format format);

        /**
         * @brief Plugin manager constructor
         *
         * Outputs files in format based on which alias was used to load the
         * plugin.
         */
        explicit StbImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_STBIMAGECONVERTER_LOCAL ImageConverterFeatures doFeatures() const override;
        MAGNUM_STBIMAGECONVERTER_LOCAL Containers::Array<char> doConvertToData(const ImageView2D& image) override;

        Format _format;
};

}}

#endif
