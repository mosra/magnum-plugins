#ifndef Magnum_Trade_DdsImporter_h
#define Magnum_Trade_DdsImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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

-   DDS uncompressed RGB, RGBA, BGR, BGRA, grayscale as
    @ref PixelFormat::RGB8Unorm, @ref PixelFormat::RGBA8Unorm or
    @ref PixelFormat::R8Unorm, with component swizzling as necessary
-   DDS compressed DXT1, DXT3, DXT5 as @ref CompressedPixelFormat::Bc1RGBAUnorm,
    @ref CompressedPixelFormat::Bc2RGBAUnorm and
    @ref CompressedPixelFormat::Bc3RGBAUnorm, respectively
-   DDS DXT10 with the following DXGI formats:
    -   `R8_TYPELESS`, `R8G8_TYPELESS`, `R8G8B8A8_TYPELESS` as
        @ref PixelFormat::R8UI and its two-/four-component equivalents (no
        special handling)
    -   `R8_UINT`, `R8G8_UINT`, `R8G8B8A8_UINT` as @ref PixelFormat::R8UI and
        its two-/four-component equivalents
    -   `R8_INT`, `R8G8_INT`, `R8G8B8A8_INT` as @ref PixelFormat::R8I and its
        two-/four-component equivalents
    -   `R8_UNORM`, `R8G8_UNORM`, `R8G8B8A8_UNORM` as @ref PixelFormat::R8Unorm
        and its two-/four-component equivalents
    -   `A8_UNORM` as @ref PixelFormat::R8Unorm (no special handling)
    -   `R8G8B8A8_UNORM_SRGB` as @ref PixelFormat::RGBA8Unorm (no special
        handling)
    -   `R8_SNORM`, `R8G8_SNORM`, `R8G8B8A8_SNORM` as
        @ref PixelFormat::R8Snorm and its two-/four-component equivalents
    -   `R16_TYPELESS`, `R16G16_TYPELESS`, `R16G16B16A16_TYPELESS` as
        @ref PixelFormat::R16UI and its two-/four-component equivalents (no
        special handling)
    -   `R16_UINT`, `R16G16_UINT`, `R16G16B16A16_UINT` as
        @ref PixelFormat::R16UI and its two-/four-component equivalents
    -   `R16_INT`, `R16G16_INT`, `R16G16B16A16_INT` as @ref PixelFormat::R16I
        and its two-/four-component equivalents
    -   `R16_FLOAT`, `R16G16_FLOAT`, `R16G16B16A16_FLOAT` as
        @ref PixelFormat::R16F and its two-/four-component equivalents
    -   `R16_UNORM`, `R16G16_UNORM`, `R16G16B16A16_UNORM` as
        @ref PixelFormat::R16Unorm and its two-/four-component equivalents
    -   `R16_SNORM`, `R16G16_SNORM`, `R16G16B16A16_SNORM` as
        @ref PixelFormat::R16Snorm and its two-/four-component equivalents
    -   `R32_TYPELESS`, `R32G32_TYPELESS`, `R32G32B32_TYPELESS`,
        `R32G32B32A32_TYPELESS` as  @ref PixelFormat::R32UI and its
        two-/three-/four-component equivalents (no special handling)
    -   `R32_UINT`, `R32G32_UINT`, `R32G32B32_UINT`, `R32G32B32A32_UINT` as
        @ref PixelFormat::R32UI and its two-/three-/four-component equivalents
    -   `R32_INT`, `R32G32_INT`, `R32G32B32_INT`, `R32G32B32A32_INT` as
        @ref PixelFormat::R32I  and its two-/three-/four-component equivalents
    -   `R32_FLOAT`, `R32G32_FLOAT`, `R32G32B32_FLOAT`, `R32G32B32A32_FLOAT` as
        @ref PixelFormat::R32F and its two-/three-/four-component equivalents

The importer recognizes @ref ImporterFlag::Verbose, printing additional info
when the flag is enabled.

BC6h, BC7 and other compressed formats are currently not imported correctly.
*/
class MAGNUM_DDSIMPORTER_EXPORT DdsImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit DdsImporter();

        /** @brief Plugin manager constructor */
        explicit DdsImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~DdsImporter();

    private:
        MAGNUM_DDSIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_DDSIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_DDSIMPORTER_LOCAL void doClose() override;
        MAGNUM_DDSIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

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
