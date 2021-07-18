#ifndef Magnum_Trade_DevIlImageImporter_h
#define Magnum_Trade_DevIlImageImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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
-   DirectDraw Surface (`*.dds`), @ref Trade-DevIlImageImporter-behavior-dds "details"
-   OpenEXR (`*.exr`), @ref Trade-DevIlImageImporter-behavior-exr "details"
-   Flexible Image Transport System (`*.fits`, `*.fit`)
-   Heavy Metal: FAKK 2 (`*.ftx`)
-   Graphics Interchange Format (`*.gif`), @ref Trade-DevIlImageImporter-behavior-animated-gifs "details"
-   Radiance HDR (`*.hdr`)
-   Macintosh icon (`*.icns`)
-   Windows icon/cursor (`*.ico`, `*.cur`), @ref Trade-DevIlImageImporter-behavior-ico "details"
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
-   Portable Network Graphics (`*.png`), @ref Trade-DevIlImageImporter-behavior-cgbi "details"
-   Portable Anymap (`*.pbm`, `*.pgm`, `*.ppm`, `*.pnm`)
-   Alias PIX (`*.pix`)
-   Adobe Photoshop (`*.psd`), @ref Trade-DevIlImageImporter-behavior-psd "details"
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

This plugins provides `BmpImporter`, `DdsImporter`, `OpenExrImporter`,
`GifImporter`, `HdrImporter`, `IcoImporter`, `JpegImporter`,
`Jpeg2000Importer`, `MngImporter`, `PcxImporter`, `PbmImporter`, `PgmImporter`,
`PicImporter`, `PngImporter`, `PnmImporter`, `PpmImporter`, `PsdImporter`,
`SgiImporter`, `TgaImporter` and `TiffImporter` plugins.

@m_class{m-block m-warning}

@thirdparty This plugin makes use of the [DevIL](http://openil.sourceforge.net/)
    library, licensed under @m_class{m-label m-warning} **LGPLv2.1**
    ([license text](http://openil.sourceforge.net/lgpl.txt),
    [choosealicense.com](https://choosealicense.com/licenses/lgpl-2.1/)). It
    requires attribution and either dynamic linking or source disclosure for
    public use.

@section Trade-DevIlImageImporter-usage Usage

This plugin depends on the @ref Trade and [DevIL](http://openil.sourceforge.net/)
libraries and is built if `WITH_DEVILIMAGEIMPORTER` is enabled when building
Magnum Plugins. To use as a dynamic plugin, load @cpp "DevIlImageImporter" @ce
via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using DevIL itself as a CMake subproject isn't tested at the moment,
so you need to provide it as a system dependency and point `CMAKE_PREFIX_PATH`
to its installation dir if necessary.

@code{.cmake}
set(WITH_TINYGLTFIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::DevIlImageImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindDevIL.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindDevIL.cmake)
into your `modules/` directory, request the `DevIlImageImporter` component of
the `MagnumPlugins`  package in CMake and link to the
`MagnumPlugins::DevIlImageImporter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED DevIlImageImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::DevIlImageImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-DevIlImageImporter-behavior Behavior and limitations

The images are imported as @ref PixelFormat::R8Unorm,
@ref PixelFormat::RG8Unorm, @ref PixelFormat::RGB8Unorm and
@ref PixelFormat::RGBA8Unorm. BGR/BGRA will be converted to
@ref PixelFormat::RGB8Unorm / @ref PixelFormat::RGBA8Unorm, all other formats
to @ref PixelFormat::RGBA8Unorm. Images are imported with default
@ref PixelStorage parameters except for alignment, which may be changed to `1`
if the data require it.

@subsection Trade-DevIlImageImporter-behavior-dds Compressed DDS files

DDS files with BCn compression are always decompressed to RGBA on input.

@subsection Trade-DevIlImageImporter-behavior-exr OpenEXR files

Although DevIL advertises OpenEXR support, when trying any file it either
complained about an invalid extension (@cpp 0x50b @ce) or, when specifying
@cb{.ini} type=0x0442 @ce in the
@ref Trade-DevIlImageImporter-configuration "configuration", failed with
an invalid enum error (@cpp 0x501 @ce). Since DevIL internally uses OpenEXR
anyway, you'll have a much better luck with @ref OpenExrImporter.

@subsection Trade-DevIlImageImporter-behavior-animated-gifs Animated GIFs

In case the file is an animated GIF, the importer will report frame count in
@ref image2DCount() and you can then import each frame separately.
Unfortunately, GIF transitions are not supported properly and some GIFs can
fail to load or crash the library. See @ref StbImageImporter for an
alternative.

@subsection Trade-DevIlImageImporter-behavior-ico ICO files

DevIL is able to load ICO files with embedded BMPs, unfortunately it crashes
for some ICOs with embedded PNGs and doesn't correctly import alpha for others
--- use @ref IcoImporter in that case instead. Note that for proper file format
auto-detection you either need to load them via @ref openFile() or set the
@cb{.ini} type @ce @ref Trade-DevIlImageImporter-configuration "configuration option" to
(hexadecimal) @cpp 0x0424 @ce. Compared to @ref IcoImporter the icon sizes are
reported as separate images instead of image levels.

@subsection Trade-DevIlImageImporter-behavior-cgbi Apple CgBI PNGs

CgBI is a proprietary Apple-specific extension to PNG
([details here](http://iphonedevwiki.net/index.php/CgBI_file_format)). DevIL
doesn't support these, use @ref StbImageImporter for loading these instead.

@subsection Trade-DevIlImageImporter-behavior-psd PSD files

Based on the source it seems that DevIL parses only the composited view,
importing individual layers is not possible.

@section Trade-DevIlImageImporter-configuration Plugin-specific config

It's possible to tune various output options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/DevIlImageImporter/DevIlImageImporter.conf config

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_DEVILIMAGEIMPORTER_EXPORT DevIlImageImporter: public AbstractImporter {
    public:
        /** @brief Initialize DevIL library */
        static void initialize();

        /** @brief Plugin manager constructor */
        explicit DevIlImageImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~DevIlImageImporter();

    private:
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL void doClose() override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL void doOpenFile(const std::string& filename) override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_DEVILIMAGEIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        UnsignedInt _image = 0;
};

}}

#endif
