#ifndef Magnum_Trade_KtxImporter_h
#define Magnum_Trade_KtxImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2021 Pablo Escobar <mail@rvrs.in>

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
 * @brief Class @ref Magnum::Trade::KtxImporter
 * @m_since_latest_{plugins}
 */

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/KtxImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_KTXIMPORTER_BUILD_STATIC
    #ifdef KtxImporter_EXPORTS
        #define MAGNUM_KTXIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_KTXIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_KTXIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_KTXIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_KTXIMPORTER_EXPORT
#define MAGNUM_KTXIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief KTX2 image importer plugin
@m_since_latest_{plugins}

Supports Khronos Texture 2.0 images (`*.ktx2`). You can use
@ref KtxImageConverter to encode images into this format.

@section Trade-KtxImporter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_KTXIMPORTER` is enabled when building Magnum Plugins. To use as a dynamic
plugin, load @cpp "KtxImporter" @ce via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_KTXIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::KtxImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `KtxImporter` component of the
`MagnumPlugins` package in CMake and link to the `MagnumPlugins::KtxImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED KtxImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::KtxImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-KtxImporter-behavior Behavior and limitations

Imports images in the following formats:

-   KTX2 with all uncompressed Vulkan formats that have an equivalent in
    @ref PixelFormat, with component swizzling as necessary
-   KTX2 with most compressed Vulkan formats that have an equivalent in
    @ref CompressedPixelFormat. None of the 3D ASTC formats are supported.

With compressed pixel formats, the image will not be flipped if the Y- or
Z-axis orientation doesn't match the output orientation. The nontrivial amount
of work involved with flipping block-compressed data makes this unfeasible. The
import will succeed but a warning will be emitted.

The importer recognizes @ref ImporterFlag::Verbose, printing additional info
when the flag is enabled.

@subsection Trade-KtxImporter-behavior-types Image types

All image types supported by KTX2 are imported, including 1D, 2D, cube maps,
and 3D images. They can, in turn, all have multiple array layers as well as
multiple mip levels. The image type can be determined from @ref texture() and
@ref TextureData::type().

For layered images and (layered) cube maps, the array layers and faces are
exposed as an additional image dimension. 1D array textures import
@ref ImageData2D with n y-slices, 2D array textures import @ref ImageData3D
with n z-slices and (layered) cube maps import @ref ImageData3D with 6*n
z-slices. 3D array textures behave differently: because there is no
`ImageData4D`, each layer is imported as a separate @ref ImageData3D, with
@ref image3DCount() determining the number of layers.

@subsection Trade-KtxImporter-behavior-multilevel Multilevel images

Files with multiple mip levels are imported with the largest level first, with
the size of each following level divided by 2, rounded down. Mip chains can be
incomplete, ie. they don't have to extend all the way down to a level of size
1x1.

@subsection Trade-KtxImporter-behavior-cube Cube maps

Cube map faces are imported in the order +X, -X, +Y, -Y, +Z, -Z as seen from a
left-handed coordinate system (+X is right, +Y is up, +Z is forward). Layered
cube maps are stored as multiple sets of faces, ie. all faces +X through -Z for
the first layer, then all faces of the second layer, etc.

Incomplete cube maps (determined by the `KTXcubemapIncomplete` metadata entry)
are imported as a 2D array image, but information about which faces it contains
can't be imported.

@subsection Trade-KtxImporter-behavior-swizzle Swizzle support

Explicit swizzling via the KTXswizzle header entry supports BGR and BGRA. Any
other non-identity channel remapping is unsupported and results in an error.

For reasons similar to the restriction on axis-flips, compressed formats don't
support any swizzling, and the import fails if an image with a compressed
format contains a swizzle that isn't RGBA.

@subsection Trade-KtxImporter-behavior-basis Basis Universal compression

When the importer detects a Basis Universal compressed file, it will forward
the file to the @ref BasisImporter plugin, if available. Flags set via
@ref setFlags() and options in the @cb{.ini} [basis] @ce section of the
@ref Trade-KtxImporter-configuration "plugin configuration" are propagated to
@ref BasisImporter.

Calls to the @ref image1DCount() / @ref image2DCount() / @ref image3DCount(),
@ref image1DLevelCount() / @ref image2DLevelCount() / @ref image3DLevelCount(),
@ref image1D() / @ref image2D() / @ref image3D() and @ref textureCount() /
@ref texture() functions are then proxied to @ref BasisImporter. The
@ref close() function closes and discards the internally instantiated plugin;
@ref isOpened() works as usual.

@subsection Trade-KtxImporter-behavior-supercompression Supercompression

Importing files with [supercompression](https://www.khronos.org/registry/KTX/specs/2.0/ktxspec_v2.html#supercompressionSchemes)
is not supported. When @ref Trade-KtxImporter-behavior-basis "forwarding Basis Universal compressed files",
some supercompression schemes like BasisLZ and Zstandard can be handled by
@ref BasisImporter.

@section Trade-KtxImporter-configuration Plugin-specific configuration

For some formats, it's possible to tune various options through
@ref configuration(). See below for all options and their default values;
a subset of the option is the same as in @link BasisImporter @endlink:

@snippet MagnumPlugins/KtxImporter/KtxImporter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_KTXIMPORTER_EXPORT KtxImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit KtxImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~KtxImporter();

    private:
        MAGNUM_KTXIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_KTXIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_KTXIMPORTER_LOCAL void doClose() override;
        MAGNUM_KTXIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;

        template<UnsignedInt dimensions> MAGNUM_KTXIMPORTER_LOCAL ImageData<dimensions> doImage(UnsignedInt id, UnsignedInt level);

        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doImage1DCount() const override;
        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doImage1DLevelCount(UnsignedInt id) override;
        MAGNUM_KTXIMPORTER_LOCAL Containers::Optional<ImageData1D> doImage1D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_KTXIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doImage3DCount() const override;
        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doImage3DLevelCount(UnsignedInt id) override;
        MAGNUM_KTXIMPORTER_LOCAL Containers::Optional<ImageData3D> doImage3D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_KTXIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;

        struct File;
        Containers::Pointer<File> _f;
        Containers::Pointer<AbstractImporter> _basisImporter;
};

}}

#endif
