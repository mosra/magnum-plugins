#ifndef Magnum_Trade_DdsImporter_h
#define Magnum_Trade_DdsImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
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
    #ifdef DdsImporter_EXPORTS
        #define MAGNUM_DDSIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_DDSIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_DDSIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_DDSIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_DDSIMPORTER_EXPORT
#define MAGNUM_DDSIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief DDS image importer plugin

Supports DirectDraw Surface images (`*.dds`) in the following formats:

-   DDS uncompressed RGB, RGBA, BGR, BGRA, grayscale as
    @ref PixelFormat::RGB8Unorm, @ref PixelFormat::RGBA8Unorm or
    @ref PixelFormat::R8Unorm, with component swizzling as necessary
-   DDS compressed DXT1, DXT3, DXT5 as @ref CompressedPixelFormat::Bc1RGBAUnorm,
    @ref CompressedPixelFormat::Bc2RGBAUnorm and
    @ref CompressedPixelFormat::Bc3RGBAUnorm, respectively
-   DDS DXT10 in formats that correspond to the
    @m_class{m-doc-external} [DXGI_FORMAT](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
    mapping documented in @ref Magnum::PixelFormat "PixelFormat" and
    @ref CompressedPixelFormat
    (searching for the DXGI format names works as well), with the following
    special cases:

    -   @m_class{m-doc-external} [DXGI_FORMAT_A8_UNORM](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
        is imported as @ref PixelFormat::R8Unorm
    -   @m_class{m-doc-external} [DXGI_FORMAT_B8G8R8A8_UNORM](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format) /
        @m_class{m-doc-external} [DXGI_FORMAT_B8G8R8X8_UNORM](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
        and @m_class{m-doc-external} [DXGI_FORMAT_B8G8R8A8_UNORM_SRGB](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format) /
        @m_class{m-doc-external} [DXGI_FORMAT_B8G8R8X8_UNORM_SRGB](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
        is imported as @ref PixelFormat::RGBA8Unorm and @ref PixelFormat::RGBA8Srgb
        with component swizlling, the `X` variant not being treated in any
        special way --- alpha channel gets whatever data are there
    -   All uncompressed `*_TYPELESS` formats are treated the same way as the
        `*_UI` alternatives
    -   BC1 -- BC5 and BC7 `*_TYPELESS` formats are treated the same way as their
        `*_UNORM` alternatives
    -   Depth/stencil formats, packed formats, (planar) YUV / YCbCr video formats,
    -   @m_class{m-doc-external} [DXGI_FORMAT_BC6H_TYPELESS](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
        is treated the same way as @m_class{m-doc-external} [DXGI_FORMAT_BC6H_UF16](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format),
        thus @ref CompressedPixelFormat::Bc6hRGBUfloat

    Depth/stencil formats, packed formats, (planar) YUV / YCbCr video formats,
    @m_class{m-doc-external} [DXGI_FORMAT_R1_UNORM](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
    and @m_class{m-doc-external} [R10G10B10_XR_BIAS_A2_UNORM](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
    are not supported.

This plugin depends on the @ref Trade library and is built if
`WITH_DDSIMPORTER` is enabled when building Magnum Plugins. To use as a dynamic
plugin, you need to load the @cpp "DdsImporter" @ce plugin from
`MAGNUM_PLUGINS_IMPORTER_DIR`. To use as a static plugin or as a dependency of
another plugin with CMake, you need to request the `DdsImporter` component of
the `MagnumPlugins` package in CMake and link to the
`MagnumPlugins::DdsImporter` target. See @ref building-plugins,
@ref cmake-plugins and @ref plugins for more information.

@note Mipmaps are currently imported under separate image data IDs. You may
    access them via @ref image2D(UnsignedInt) / @ref image3D(UnsignedInt) which
    will return the n-th mip, a bigger n indicating a smaller mip.
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
        MAGNUM_DDSIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id) override;

        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage3DCount() const override;
        MAGNUM_DDSIMPORTER_LOCAL Containers::Optional<ImageData3D> doImage3D(UnsignedInt id) override;

    private:
        struct File;
        Containers::Pointer<File> _f;
};

}}

#endif
