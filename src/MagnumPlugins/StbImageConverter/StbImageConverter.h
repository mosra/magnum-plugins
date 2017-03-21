#ifndef Magnum_Trade_StbImageConverter_h
#define Magnum_Trade_StbImageConverter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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
 * @brief Class @ref Magnum::Trade::StbImageConverter
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/StbImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_STBIMAGECONVERTER_BUILD_STATIC
    #if defined(StbImageConverter_EXPORTS) || defined(StbImageConverterObjects_EXPORTS)
        #define MAGNUM_STBIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_STBIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_STBIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_STBIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief Image converter plugin using stb_image_write

Creates files in one of the following formats:

-   Windows Bitmap (`*.bmp`) if the plugin was loaded as `StbBmpImageConverter`
    / `BmpImageConverter` or @ref Format::Bmp was passed to the constructor
-   Radiance HDR (`*.hdr`) if the plugin was loaded as `StbHdrImageConverter`
    / `HdrImageConverter` or @ref Format::Hdr was passed to the constructor
-   Portable Network Graphics (`*.png`) if the plugin was loaded as
    `StbPngImageConverter` / `PngImageConverter` or @ref Format::Png was passed
    to the constructor
-   Truevision TGA (`*.tga`, `*.vda`, `*.icb`, `*.vst`) if the plugin was
    loaded as `StbTgaImageConverter` / `TgaImageConverter` or @ref Format::Tga
    was passed to the constructor

The images must have format @ref PixelFormat::Red, @ref PixelFormat::RG,
@ref PixelFormat::RGB or @ref PixelFormat::RGBA. For BMP, PNG and TGA the type
must be @ref PixelType::UnsignedByte, for HDR the type must be
@ref PixelType::Float. On OpenGL ES 2.0 and WebGL 1.0 accepts also
@ref PixelFormat::Luminance instead of @ref PixelFormat::Red and
@ref PixelFormat::LuminanceAlpha instead of @ref PixelFormat::RG. Does *not*
support non-default @ref PixelStorage::swapBytes() values, the image data must
be tightly packed (except for PNG output, which is able to handle custom row
strides).

This plugin is built if `WITH_STBIMAGECONVERTER` is enabled when building
Magnum Plugins. To use dynamic plugin, you need to load `*ImageConverter`
plugin from `MAGNUM_PLUGINS_IMAGECONVERTER_DIR`. To use static plugin, you need
to request `StbImageConverter` component of `MagnumPlugins` package in CMake
and link to `MagnumPlugins::StbImageConverter` target. Note that you need to
load one of the alias names instead of just `StbImageConverter` in order to
specify one of the output formats.

Besides `StbBmpImageConverter`, `StbHdrImageConverter`, `StbPngImageConverter`
and `StbTgaImageConverter` aliases this plugin provides also
`BmpImageConverter`, `HdrImageConverter`, `PngImageConverter` and
`TgaImageConverter` plugins, but note that this plugin may generate slightly
larger files and the performance might be worse than when using plugins
dedicated for given format.

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
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

            Bmp = 1,    /**< @brief Output BMP images */
            Hdr,        /**< @brief Output HDR images */
            Png,        /**< @brief Output PNG images */
            Tga         /**< @brief Output TGA images */
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
        MAGNUM_STBIMAGECONVERTER_LOCAL Features doFeatures() const override;
        MAGNUM_STBIMAGECONVERTER_LOCAL Containers::Array<char> doExportToData(const ImageView2D& image) override;

        Format _format;
};

}}

#endif
