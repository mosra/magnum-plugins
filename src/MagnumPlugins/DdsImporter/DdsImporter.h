#ifndef Magnum_Trade_DdsImporter_h
#define Magnum_Trade_DdsImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
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

Supports DirectDraw Surface images (`*.dds`).

@section Trade-DdsImporter-usage Usage

This plugin depends on the @ref Trade library and is built if
`MAGNUM_WITH_DDSIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "DdsImporter" @ce via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(MAGNUM_WITH_DDSIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::DdsImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `DdsImporter` component of the
`MagnumPlugins` package in CMake and link to the `MagnumPlugins::DdsImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED DdsImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::DdsImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-DdsImporter-behavior Behavior and limitations

@m_class{m-block m-warning}

@par Imported image orientation
    Unlike @ref Trade-KtxImporter-behavior "KTX" or
    @ref Trade-BasisImporter-behavior "Basis", the file format doesn't contain
    any orientation metadata, and so it's assumed to follow the Vulkan/D3D
    coordinate system with Y down and (for 3D textures) Z forward. Uncompressed
    images will be flipped on import to Y up and (for 3D textures) Z backward.
    Because flipping block-compressed data is nontrivial, compressed images
    will not be flipped on import, instead a message will be printed to
    @relativeref{Magnum,Warning} and the data will be passed through unchanged.
    Set the @cb{.ini} assumeYUpZBackward @ce
    @ref Trade-DdsImporter-configuration "configuration option" to assume the
    OpenGL coordinate system, perform no flipping of uncompressed data and
    silence the warning for compressed data.

The importer recognizes @ref ImporterFlag::Verbose, printing additional info
when the flag is enabled.

@subsection Trade-DdsImporter-behavior-types Image types

All image types supported by DDS are imported, including 1D, 1D array, 2D, 2D
array, cube maps, cube map arrays and 3D images. They can, in turn, all have
multiple mip levels. The images are annotated with @ref ImageFlag2D::Array,
@ref ImageFlag3D::Array and @ref ImageFlag3D::CubeMap as appropriate.
Furthermore, for backward compatibility, if @ref MAGNUM_BUILD_DEPRECATED is
enabled, the image type can also be determined from @ref texture() and
@ref TextureData::type().

The importer exposes always exactly one image. For layered images and (layered)
cube maps, the array layers and faces are exposed as an additional image
dimension. 1D array textures import @ref ImageData2D with n y-slices, 2D array
textures import @ref ImageData3D with n z-slices and (layered) cube maps import
@ref ImageData3D with 6*n z-slices.

@subsection Trade-DdsImporter-behavior-multilevel Multilevel images

Files with multiple mip levels are imported with the largest level first, with
the size of each following level divided by 2, rounded down. Mip chains can be
incomplete, ie. they don't have to extend all the way down to a level of size
1x1.

@subsection Trade-DdsImporter-behavior-cube Cube maps

Cube map faces are imported in the order +X, -X, +Y, -Y, +Z, -Z. Layered cube
maps are stored as multiple sets of faces, ie. all faces +X through -Z for the
first layer, then all faces of the second layer, etc.

Incomplete cube maps, which are possible in legacy DDS files without the DXT10
header, are imported as a 2D array image, but information about which faces it
contains isn't preserved.

@subsection Trade-DdsImporter-behavior-format Legacy DDS format support

The following formats are supported for legacy DDS files without the DXT10
header:

-   Uncompressed RGB, RGBA / RGBX, BGR, BGRA / BGRX, R are imported as
    @ref PixelFormat::RGB8Unorm, @ref PixelFormat::RGBA8Unorm or
    @ref PixelFormat::R8Unorm, with component swizzling as necessary. The
    `X` variant is not treated in any special way --- alpha channel gets
    whatever data is there.
-   Compressed FourCC `DXT1`, `DXT3`, `DXT5`, `ATI1` (BC4), `ATI2` (BC5),
    `BC4S` and `BC5S` formats are imported as
    @ref CompressedPixelFormat::Bc1RGBAUnorm,
    @relativeref{CompressedPixelFormat,Bc2RGBAUnorm},
    @relativeref{CompressedPixelFormat,Bc3RGBAUnorm},
    @relativeref{CompressedPixelFormat,Bc4RUnorm},
    @relativeref{CompressedPixelFormat,Bc5RGUnorm},
    @relativeref{CompressedPixelFormat,Bc4RSnorm} and
    @relativeref{CompressedPixelFormat,Bc5RGSnorm} respectively

While the legacy header support arbitrary other swizzles, anything except BGR
and BGRA will fail to import. Other FourCC codes except the ones listed above
are not supported.

@subsection Trade-DdsImporter-behavior-format-dxt10 DXT10 format support

Formats that correspond to the @m_class{m-doc-external} [DXGI_FORMAT](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
are supported for DDS files with the DXT10 header (`DX10` FourCC). The mapping
is documented in @ref Magnum::PixelFormat "PixelFormat" and
@ref CompressedPixelFormat (searching for the DXGI format names works as well),
with the following special cases:

-   @m_class{m-doc-external} [DXGI_FORMAT_A8_UNORM](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
    is imported as @ref PixelFormat::R8Unorm
-   @m_class{m-doc-external} [DXGI_FORMAT_B8G8R8A8_UNORM](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format) /
    @m_class{m-doc-external} [DXGI_FORMAT_B8G8R8X8_UNORM](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
    and @m_class{m-doc-external} [DXGI_FORMAT_B8G8R8A8_UNORM_SRGB](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format) /
    @m_class{m-doc-external} [DXGI_FORMAT_B8G8R8X8_UNORM_SRGB](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
    is imported as @ref PixelFormat::RGBA8Unorm and
    @ref PixelFormat::RGBA8Srgb with component swizlling, the `X` variant
    not being treated in any special way --- alpha channel gets whatever
    data is there
-   @m_class{m-doc-external} [DXGI_FORMAT_R32G8X24_TYPELESS](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format),
    @m_class{m-doc-external} [DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format) and
    @m_class{m-doc-external} [DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
    are all treated the same way as @m_class{m-doc-external} [DXGI_FORMAT_D32_FLOAT_S8X24_UINT](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format),
    thus @ref PixelFormat::Depth32FStencil8UI
-   @m_class{m-doc-external} [DXGI_FORMAT_R24G8_TYPELESS](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format),
    @m_class{m-doc-external} [DXGI_FORMAT_R24_UNORM_X8_TYPELESS](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format) and
    @m_class{m-doc-external} [DXGI_FORMAT_X24_TYPELESS_G8_UINT](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
    are all treated the same way as @m_class{m-doc-external} [DXGI_FORMAT_D24_UNORM_S8_UINT](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format),
    thus @ref PixelFormat::Depth24UnormStencil8UI
-   BC1 -- BC5 and BC7 `*_TYPELESS` formats are treated the same way as
    their `*_UNORM` alternatives
-   @m_class{m-doc-external} [DXGI_FORMAT_BC6H_TYPELESS](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
    is treated the same way as @m_class{m-doc-external} [DXGI_FORMAT_BC6H_UF16](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format),
    thus @ref CompressedPixelFormat::Bc6hRGBUfloat
-   All other uncompressed `*_TYPELESS` formats are treated the same way as
    the `*_UI` alternatives

Additionally, nonstandard 2D ASTC `DXGI_FORMAT_ASTC_4x4_TYPELESS`,
`DXGI_FORMAT_ASTC_4x4_UNORM`, `DXGI_FORMAT_ASTC_4x4_UNORM_SRGB` formats
including other sizes up to 12x12 are supported, mapping to
@ref CompressedPixelFormat::Astc4x4RGBAUnorm,
@relativeref{CompressedPixelFormat,Astc4x4RGBASrgb} and following variants.
A subset of those is used by [NVidia Texture Tools Exporter](https://developer.nvidia.com/nvidia-texture-tools-exporter),
historically the ASTC formats were meant to be included in Direct3D 11.3 but
were later pulled. While there are reserved enum values for float variants of
these formats as well, no tool is known to produce them. 3D ASTC formats have
no known mapping.

Packed formats, (planar) YUV / YCbCr video formats,
@m_class{m-doc-external} [DXGI_FORMAT_R1_UNORM](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
and @m_class{m-doc-external} [R10G10B10_XR_BIAS_A2_UNORM](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
are not supported.

@section Trade-DdsImporter-configuration Plugin-specific configuration

It's possible to tune various import options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/DdsImporter/DdsImporter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_DDSIMPORTER_EXPORT DdsImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit DdsImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~DdsImporter();

    private:
        MAGNUM_DDSIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_DDSIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_DDSIMPORTER_LOCAL void doClose() override;
        MAGNUM_DDSIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;

        template<UnsignedInt dimensions> MAGNUM_DDSIMPORTER_LOCAL ImageData<dimensions> doImage(UnsignedInt id, UnsignedInt level);

        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage1DCount() const override;
        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage1DLevelCount(UnsignedInt id) override;
        MAGNUM_DDSIMPORTER_LOCAL Containers::Optional<ImageData1D> doImage1D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_DDSIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage3DCount() const override;
        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage3DLevelCount(UnsignedInt id) override;
        MAGNUM_DDSIMPORTER_LOCAL Containers::Optional<ImageData3D> doImage3D(UnsignedInt id, UnsignedInt level) override;

        #ifdef MAGNUM_BUILD_DEPRECATED
        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_DDSIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;
        #endif

        struct File;
        Containers::Pointer<File> _f;
};

}}

#endif
