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
`WITH_DDSIMPORTER` is enabled when building Magnum Plugins. To use as a dynamic
plugin, load @cpp "DdsImporter" @ce via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_DDSIMPORTER ON CACHE BOOL "" FORCE)
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

Imports images in the following formats:

-   Uncompressed RGB, RGBA / RGBX, BGR, BGRA / BGRX, grayscale as
    @ref PixelFormat::RGB8Unorm, @ref PixelFormat::RGBA8Unorm or
    @ref PixelFormat::R8Unorm, with component swizzling as necessary. The `X`
    variant not being treated in any special way --- alpha channel gets
    whatever data is there.
-   Compressed DXT1, DXT3, DXT5, ATI1 (BC4), ATI2 (BC5), BC4S and BC5S as
    @ref CompressedPixelFormat::Bc1RGBAUnorm,
    @relativeref{CompressedPixelFormat,Bc2RGBAUnorm},
    @relativeref{CompressedPixelFormat,Bc3RGBAUnorm},
    @relativeref{CompressedPixelFormat,Bc4RUnorm},
    @relativeref{CompressedPixelFormat,Bc5RGUnorm},
    @relativeref{CompressedPixelFormat,Bc4RSnorm} and
    @relativeref{CompressedPixelFormat,Bc5RGSnorm} respectively
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

    Packed formats, (planar) YUV / YCbCr video formats,
    @m_class{m-doc-external} [DXGI_FORMAT_R1_UNORM](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
    and @m_class{m-doc-external} [R10G10B10_XR_BIAS_A2_UNORM](https://docs.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
    are not supported.

The importer recognizes @ref ImporterFlag::Verbose, printing additional info
when the flag is enabled.
*/
class MAGNUM_DDSIMPORTER_EXPORT DdsImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit DdsImporter();

        /** @brief Plugin manager constructor */
        explicit DdsImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~DdsImporter();

    private:
        MAGNUM_DDSIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_DDSIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_DDSIMPORTER_LOCAL void doClose() override;
        MAGNUM_DDSIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;

        template<UnsignedInt dimensions> MAGNUM_DDSIMPORTER_LOCAL ImageData<dimensions> doImage(const char* prefix, UnsignedInt id, UnsignedInt level);

        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_DDSIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage3DCount() const override;
        MAGNUM_DDSIMPORTER_LOCAL UnsignedInt doImage3DLevelCount(UnsignedInt id) override;
        MAGNUM_DDSIMPORTER_LOCAL Containers::Optional<ImageData3D> doImage3D(UnsignedInt id, UnsignedInt level) override;

    private:
        struct File;
        Containers::Pointer<File> _f;
};

}}

#endif
