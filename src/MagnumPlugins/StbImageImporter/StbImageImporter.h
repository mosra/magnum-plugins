#ifndef Magnum_Trade_StbImageImporter_h
#define Magnum_Trade_StbImageImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015
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
 * @brief Class @ref Magnum::Trade::StbImageImporter
 */

#include <Corrade/Containers/Array.h>
#include <Magnum/Trade/AbstractImporter.h>

namespace Magnum { namespace Trade {

/**
@brief Image importer plugin using stb_image

Supports the following formats:

-   BMP, only non-1bpp, no RLE
-   GIF
-   HDR
-   JPEG, except for arithmetic encoding
-   PGM
-   PIC
-   PNG
-   PPM
-   PSD, only composited view
-   TGA

Creates RGB, RGBA, grayscale or grayscale + alpha images with 8 bits per
channel. Palleted images are automatically converted to RGB(A).

This plugin is built if `WITH_STBIMAGEIMPORTER` is enabled when building
Magnum Plugins. To use dynamic plugin, you need to load `StbImageImporter`
plugin from `MAGNUM_PLUGINS_IMPORTER_DIR`. To use static plugin, you need to
request `StbImageImporter` component of `MagnumPlugins` package in CMake and
link to `${MAGNUMPLUGINS_STBIMAGEIMPORTER_LIBRARIES}`. To use this as a
dependency of another plugin, you additionally need to add
`${MAGNUMPLUGINS_STBIMAGEIMPORTER_INCLUDE_DIRS}` to include path.

This plugins provides `BmpImporter`, `GifImporter`, `HdrImporter`,
`JpegImporter`, `PgmImporter`, `PicImporter`, `PngImporter`, `PpmImporter`,
`PsdImporter` and `TgaImporter` plugins, but note that this plugin doesn't have
complete support for all format quirks and the performance might be worse than
when using plugin dedicated for given format.

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

The images are imported with @ref ColorType::UnsignedByte type and
@ref ColorFormat::RGB, @ref ColorFormat::RGBA, @ref ColorFormat::Red for
grayscale or @ref ColorFormat::RG for grayscale + alpha. Grayscale and
grayscale + alpha images require extension @extension{ARB,texture_rg}.

In OpenGL ES 2.0 if @es_extension{EXT,texture_rg} is not supported and in WebGL
1.0, grayscale images use @ref ColorFormat::Luminance instead of
@ref ColorFormat::Red and @ref ColorFormat::LuminanceAlpha instead of
@ref ColorFormat::RG.

@todo Properly support floating-point HDR images
@todo Enable ARM NEON when I'm able to test that
*/
class StbImageImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit StbImageImporter();

        /** @brief Plugin manager constructor */
        explicit StbImageImporter(PluginManager::AbstractManager& manager, std::string plugin);

        ~StbImageImporter();

    private:
        Features doFeatures() const override;
        bool doIsOpened() const override;
        void doClose() override;
        void doOpenData(Containers::ArrayView<const char> data) override;

        UnsignedInt doImage2DCount() const override;
        std::optional<ImageData2D> doImage2D(UnsignedInt id) override;

    private:
        Containers::Array<unsigned char> _in;
};

}}

#endif
