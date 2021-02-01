#ifndef Magnum_Trade_StbImageImporter_h
#define Magnum_Trade_StbImageImporter_h
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
 * @brief Class @ref Magnum::Trade::StbImageImporter
 */

#include <Corrade/Containers/Pointer.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/StbImageImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_STBIMAGEIMPORTER_BUILD_STATIC
    #ifdef StbImageImporter_EXPORTS
        #define MAGNUM_STBIMAGEIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_STBIMAGEIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_STBIMAGEIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_STBIMAGEIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_STBIMAGEIMPORTER_EXPORT
#define MAGNUM_STBIMAGEIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief Image importer plugin using stb_image

@m_keywords{BmpImporter GifImporter HdrImporter JpegImporter PgmImporter}
@m_keywords{PicImporter PngImporter PpmImporter PsdImporter TgaImporter}

Supports the following formats using the
[stb_image](https://github.com/nothings/stb) library:

-   Windows Bitmap (`*.bmp`), @ref Trade-StbImageImporter-behavior-bmp "details"
-   Graphics Interchange Format (`*.gif`) including animations, @ref Trade-StbImageImporter-behavior-animated-gifs "details"
-   Radiance HDR (`*.hdr`)
-   JPEG (`*.jpg`, `*.jpe`, `*.jpeg`), @ref Trade-StbImageImporter-behavior-arithmetic-jpeg "details"
-   Portable Graymap (`*.pgm`)
-   Softimage PIC (`*.pic`)
-   Portable Network Graphics (`*.png`), @ref Trade-StbImageImporter-behavior-png "details"
-   Portable Pixmap (`*.ppm`)
-   Adobe Photoshop (`*.psd`), @ref Trade-StbImageImporter-behavior-psd "details"
-   Truevision TGA (`*.tga`, `*.vda`, `*.icb`, `*.vst`)

Creates RGB, RGBA, grayscale or grayscale + alpha images with 8, 16 or float 32
bits per channel. Palleted images are automatically converted to RGB(A).

This plugin provides the `BmpImporter`, `GifImporter`, `HdrImporter`,
`JpegImporter`, `PgmImporter`, `PicImporter`, `PngImporter`, `PpmImporter`,
`PsdImporter` and `TgaImporter` plugins, but note that this plugin doesn't have
complete support for all format quirks and the performance might be worse than
when using a plugin dedicated for given format.

@m_class{m-block m-primary}

@thirdparty This plugin makes use of the
    [stb_image](https://github.com/nothings/stb) library by Sean Barrett,
    released into the @m_class{m-label m-primary} **public domain**
    ([license text](https://github.com/nothings/stb/blob/e6afb9cbae4064da8c3e69af3ff5c4629579c1d2/stb_image.h#L7444-L7460),
    [choosealicense.com](https://choosealicense.com/licenses/unlicense/)),
    or alternatively under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/nothings/stb/blob/e6afb9cbae4064da8c3e69af3ff5c4629579c1d2/stb_image.h#L7426-L7442),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).

@section Trade-StbImageImporter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_STBIMAGEIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "StbImageImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_STBIMAGEIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::StbImageImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `StbImageImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::StbImageImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED StbImageImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::StbImageImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-StbImageImporter-behavior Behavior and limitations

LDR images are imported as @ref PixelFormat::RGB8Unorm /
@ref PixelFormat::RGB16Unorm, @ref PixelFormat::RGBA8Unorm /
@ref PixelFormat::RGBA16Unorm, @ref PixelFormat::R8Unorm /
@ref PixelFormat::R16Unorm for grayscale or @ref PixelFormat::RG8Unorm /
@ref PixelFormat::RG16Unorm for grayscale + alpha, HDR images as
@ref PixelFormat::RGB32F, @ref PixelFormat::RGBA32F, @ref PixelFormat::R32F or
@ref PixelFormat::RG32F. Certain formats support only some channel counts (for
example Radiance HDR can only be three-component), however it's possible to
override the desired channel count using the @cb{.ini} forceChannelCount @ce
@ref Trade-StbImageImporter-configuration "configuration option".

Images are imported with default @ref PixelStorage parameters except for
alignment, which may be changed to @cpp 1 @ce if the data require it.

The importer is thread-safe if Corrade and Magnum is compiled with
@ref CORRADE_BUILD_MULTITHREADED enabled.

@subsection Trade-StbImageImporter-behavior-bmp BMP support

1bpp and RLE files are not supported.

@subsection Trade-StbImageImporter-behavior-animated-gifs Animated GIFs

In case the file is an animated GIF, the importer will report frame count in
@ref image2DCount() and you can then import each frame separately.
Additionally, for the lack of better APIs at the moment, frame delays are
exposed through @ref importerState() as an array of @ref Magnum::Int "Int",
where each entry is number of milliseconds to wait before advancing to the next
frame. Example usage:

@snippet StbImageImporter.cpp gif-delays

Note that the support for GIF transitions is currently incomplete, see
[nothings/stb#683](https://github.com/nothings/stb/pull/683) for details.

@subsection Trade-StbImageImporter-behavior-arithmetic-jpeg Arithmetic JPEG decoding

[Arithmetic coding](https://en.wikipedia.org/wiki/Arithmetic_coding) is not
implemented in stb_image, use @ref JpegImporter together with libjpeg-turbo or
mozjpeg instead.

@subsection Trade-StbImageImporter-behavior-png PNG files

Both 8- and 16-bit images are supported, lower bit counts and paletted images
are expanded to 8-bit.

CgBI is a proprietary Apple-specific extension to PNG
([details here](http://iphonedevwiki.net/index.php/CgBI_file_format)). The
importer detects those files and converts BGRA channels back to RGBA.

@subsection Trade-StbImageImporter-behavior-psd PSD files

Both 8- and 16-bit images are supported. Only the composited view, there's no
way to import individual layers.

@section Trade-StbImageImporter-configuration Plugin-specific configuration

For some formats, it's possible to tune various output options through
@ref configuration(). See below for all options and their default values:

@snippet MagnumPlugins/StbImageImporter/StbImageImporter.conf config

@todo Enable ARM NEON when I'm able to test that
*/
class MAGNUM_STBIMAGEIMPORTER_EXPORT StbImageImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit StbImageImporter();

        /** @brief Plugin manager constructor */
        explicit StbImageImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~StbImageImporter();

    private:
        MAGNUM_STBIMAGEIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_STBIMAGEIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_STBIMAGEIMPORTER_LOCAL void doClose() override;
        MAGNUM_STBIMAGEIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_STBIMAGEIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_STBIMAGEIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_STBIMAGEIMPORTER_LOCAL const void* doImporterState() const override;

        struct State;
        Containers::Pointer<State> _in;
};

}}

#endif
