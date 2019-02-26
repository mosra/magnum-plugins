#ifndef Magnum_Trade_DevIlImageImporter_h
#define Magnum_Trade_DevIlImageImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2016 Alice Margatroid <loveoverwhelming@gmail.com>

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
 * @brief Class @ref Magnum::Trade::DevIlImageImporter
 */

#include <Corrade/Containers/Array.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/DevIlImageImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_DEVILIMAGEIMPORTER_BUILD_STATIC
    #ifdef DevIlImageImporter_EXPORTS
        #define MAGNUM_DEVILIMAGEIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_DEVILIMAGEIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_DEVILIMAGEIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_DEVILIMAGEIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_DEVILIMAGEIMPORTER_EXPORT
#define MAGNUM_DEVILIMAGEIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief DevIL Image importer plugin

@m_keywords{BmpImporter DdsImporter OpenExrImporter GifImporter HdrImporter}
@m_keywords{JpegImporter Jpeg2000Importer MngImporter PcxImporter PbmImporter}
@m_keywords{PgmImporter PicImporter PngImporter PnmImporter PpmImporter}
@m_keywords{PsdImporter SgiImporter TgaImporter TiffImporter}

Supports a large variety of image file types using the
[DevIL](http://openil.sourceforge.net/) library, including (note that the list
is incomplete):

-   Windows Bitmap (`*.bmp`)
-   Dr. Halo (`*.cut`)
-   Multi-PCX (`*.dcx`)
-   Dicom (`*.dicom`, `*.dcm`)
-   DirectDraw Surface (`*.dds`)
-   OpenEXR (`*.exr`)
-   Flexible Image Transport System (`*.fits`, `*.fit`)
-   Heavy Metal: FAKK 2 (`*.ftx`)
-   Graphics Interchange Format (`*.gif`)
-   Radiance HDR (`*.hdr`)
-   Macintosh icon (`*.icns`)
-   Windows icon/cursor (`*.ico`, `*.cur`)
-   Interchange File Format (`*.iff`)
-   Infinity Ward Image (`*.iwi`)
-   JPEG (`*.jpg`, `*.jpe`, `*.jpeg`)
-   JPEG 2000 (`*.jp2`)
-   Interlaced Bitmap (`*.lbm`)
-   Homeworld texture (`*.lif`)
-   Half-Life Model (`*.mdl`)
-   Multiple-image Network Graphics (`*.mng`)
-   MPEG-1 Audio Layer 3 (`*.mp3`)
-   PaintShop Pro Palette (`*.pal`)
-   Halo Palette (`*.pal`)
-   Kodak PhotoCD (`*.pcd`)
-   ZSoft PCX (`*.pcx`)
-   Softimage PIC (`*.pic`)
-   Portable Network Graphics (`*.png`)
-   Portable Anymap (`*.pbm`, `*.pgm`, `*.ppm`, `*.pnm`)
-   Alias PIX (`*.pix`)
-   Adobe Photoshop (`*.psd`)
-   PaintShop Pro (`*.psp`)
-   Pixar (`*.pxr`)
-   Raw data (`*.raw`)
-   Homeworld 2 Texture (`*.rot`)
-   Silicon Graphics (`*.sgi`, `*.bw`, `*.rgb`, `*.rgba`)
-   Creative Assembly Texture (`*.texture`)
-   Truevision TGA (`*.tga`, `*.vda`, `*.icb`, `*.vst`)
-   Tagged Image File Format (`*.tif`, `*.tiff`)
-   Gamecube Texture (`*.tpl`)
-   Unreal Texture (`*.utx`)
-   Quake 2 Texture (`*.wal`)
-   Valve Texture Format (`*.vtf`)
-   HD Photo (`*.wdp`, `*.hdp`)
-   X Pixel Map (`*.xpm`)
-   Doom graphics

This plugin depends on the @ref Trade and [DevIL](http://openil.sourceforge.net/)
libraries and is built if `WITH_DEVILIMAGEIMPORTER` is enabled when building
Magnum Plugins. To use as a dynamic plugin, you need to load the
@cpp "DevIlImageImporter" @ce plugin from `MAGNUM_PLUGINS_IMPORTER_DIR`. To use
as a static plugin or as a dependency of another plugin with CMake, you need to
request the `DevIlImageImporter` component of the `MagnumPlugins` package in
CMake and link to the `MagnumPlugins::DevIlImageImporter` target. See
@ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

This plugins provides `BmpImporter`, `DdsImporter`, `OpenExrImporter`,
`GifImporter`, `HdrImporter`, `JpegImporter`, `Jpeg2000Importer`,
`MngImporter`, `PcxImporter`, `PbmImporter`, `PgmImporter`, `PicImporter`,
`PngImporter`, `PnmImporter`, `PpmImporter`, `PsdImporter`, `SgiImporter`,
`TgaImporter` and `TiffImporter` plugins.

@m_class{m-block m-warning}

@thirdparty This plugin makes use of the [DevIL](http://openil.sourceforge.net/)
    library, licensed under @m_class{m-label m-warning} **LGPLv2.1**
    ([license text](http://openil.sourceforge.net/lgpl.txt),
    [choosealicense.com](https://choosealicense.com/licenses/lgpl-2.1/)). It
    requires attribution and either dynamic linking or source disclosure for
    public use.

The images are imported as @ref PixelFormat::R8Unorm,
@ref PixelFormat::RG8Unorm, @ref PixelFormat::RGB8Unorm and
@ref PixelFormat::RGBA8Unorm. BGR/BGRA will be converted to
@ref PixelFormat::RGB8Unorm / @ref PixelFormat::RGBA8Unorm, all other formats
to @ref PixelFormat::RGBA8Unorm. Images are imported with default
@ref PixelStorage parameters except for alignment, which may be changed to `1`
if the data require it.
*/
class MAGNUM_DEVILIMAGEIMPORTER_EXPORT DevIlImageImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit DevIlImageImporter();

        /** @brief Plugin manager constructor */
        explicit DevIlImageImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~DevIlImageImporter();

    private:
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL Features doFeatures() const override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL void doClose() override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_DEVILIMAGEIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id) override;

        Containers::Array<unsigned char> _in;
};

}}

#endif
