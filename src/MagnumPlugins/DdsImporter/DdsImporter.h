#ifndef Magnum_Trade_DdsImporter_h
#define Magnum_Trade_DdsImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2015 Jonathan Hale <squareys@googlemail.com>

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
 * @brief Class @ref Magnum::Trade::DdsImporter
 */

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/DdsImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_DDSIMPORTER_BUILD_STATIC
    #if defined(DdsImporter_EXPORTS) || defined(DdsImporterObjects_EXPORTS)
        #define MAGNUM_DDSIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_DDSIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_DDSIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_DDSIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief DDS image importer plugin

Supports DirectDraw Surface images (`*.dds`) in the following formats:

-   DDS uncompressed RGB, RGBA, BGR, BGRA, grayscale
-   DDS compressed DXT1, DXT3, DXT5
-   DDS DXT10 with the following DXGI formats (`TYPELESS` formats are loaded as
    either @ref PixelType::UnsignedByte, @ref PixelType::UnsignedShort or
    @ref PixelType::UnsignedInt):
    -   `R32G32B32A32_(TYPELESS|UINT|SINT|FLOAT)`
    -   `R32G32B32_(TYPELESS|UINT|SINT|FLOAT)`
    -   `R32G32_(TYPELESS|UINT|SINT|FLOAT)`
    -   `R32_(TYPELESS|UINT|SINT|FLOAT)`
    -   `D32_FLOAT`
    -   `R16G16B16A16_(TYPELESS|UINT|SINT|FLOAT|UNORM|SNORM)`
    -   `R16G16B16_(TYPELESS|UINT|SINT|FLOAT|UNORM|SNORM)`
    -   `R16G16_(TYPELESS|UINT|SINT|FLOAT|UNORM|SNORM)`
    -   `R16_(TYPELESS|UINT|SINT|FLOAT|UNORM|SNORM)`
    -   `D16_UNORM`
    -   `R8G8B8A8_(TYPELESS|UINT|SINT|UNORM|UNORM_SRGB|SNORM)` (Notion of sRGB
         is discarded)
    -   `R8G8B8_(TYPELESS|UINT|SINT|UNORM|SNORM)`
    -   `R8G8_(TYPELESS|UINT|SINT|UNORM|SNORM)`
    -   `R8_(TYPELESS|UINT|SINT|UNORM|SNORM)`
    -   `A8_UNORM` (Loaded as @ref PixelFormat::Red)

This plugin is built if `WITH_DDSIMPORTER` is enabled when building
Magnum Plugins. To use dynamic plugin, you need to load `DdsImporter`
plugin from `MAGNUM_PLUGINS_IMPORTER_DIR`. To use static plugin, you need to
request `DdsImporter` component of `MagnumPlugins` package in CMake and
link to `MagnumPlugins::DdsImporter` target. See @ref building-plugins,
@ref cmake-plugins and @ref plugins for more information.

The images are imported with @ref PixelType::UnsignedByte type and
@ref PixelFormat::RGB, @ref PixelFormat::RGBA, @ref PixelFormat::Red for
grayscale. BGR and BGRA images are converted to @ref PixelFormat::RGB,
@ref PixelFormat::RGBA respectively. If the image is compressed, they are
imported with @ref CompressedPixelFormat::RGBAS3tcDxt1,
@ref CompressedPixelFormat::RGBAS3tcDxt3 and @ref CompressedPixelFormat::RGBAS3tcDxt5.

In OpenGL ES 2.0 and WebGL 1.0, single- and two-component images use
@ref PixelFormat::Luminance and @ref PixelFormat::LuminanceAlpha instead
of @ref PixelFormat::Red / @ref PixelFormat::RG.

Note: Mipmaps are currently imported under separate image data ids. You may
access them via @ref image2D(UnsignedInt)/@ref image3D(UnsignedInt) which will
return the n-th mip, a bigger n indicating a smaller mip.
*/
class MAGNUM_DDSIMPORTER_EXPORT DdsImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit DdsImporter();

        /** @brief Plugin manager constructor */
        explicit DdsImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~DdsImporter();

    private:
        MAGNUM_DDSIMPORTER_LOCAL Features doFeatures() const override;
        MAGNUM_DDSIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_DDSIMPORTER_LOCAL void doClose() override;
        MAGNUM_DDSIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_DDSIMPORTER_LOCAL std::optional<ImageData2D> doImage2D(UnsignedInt id) override;

        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage3DCount() const override;
        MAGNUM_DDSIMPORTER_LOCAL std::optional<ImageData3D> doImage3D(UnsignedInt id) override;

    private:
        struct File;

        std::unique_ptr<File> _f;
};

}}

#endif
