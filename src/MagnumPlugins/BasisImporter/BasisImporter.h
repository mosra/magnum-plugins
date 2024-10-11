#ifndef Magnum_Trade_BasisImporter_h
#define Magnum_Trade_BasisImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019, 2021 Jonathan Hale <squareys@googlemail.com>
    Copyright © 2021, 2024 Pablo Escobar <mail@rvrs.in>

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
    FROM, OUT OF OR IN CONNETCION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/** @file
* @brief Class @ref Magnum::Trade::BasisImporter
* @m_since_{plugins,2019,10}
*/

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/BasisImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_BASISIMPORTER_BUILD_STATIC
#ifdef BasisImporter_EXPORTS
    #define MAGNUM_BASISIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
#else
    #define MAGNUM_BASISIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
#endif
#else
#define MAGNUM_BASISIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_BASISIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_BASISIMPORTER_EXPORT
#define MAGNUM_BASISIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief Basis Universal importer plugin
@m_since_{plugins,2019,10}

@m_keywords{BasisImporterEacR BasisImporterEacRG BasisImporterEtc1RGB}
@m_keywords{BasisImporterEtc2RGBA BasisImporterBc1RGB BasisImporterBc3RGBA}
@m_keywords{BasisImporterBc4R BasisImporterBc5RG BasisImporterBc6hRGB}
@m_keywords{BasisImporterBc7RGBA BasisImporterPvrtc1RGB4bpp}
@m_keywords{BasisImporterPvrtc1RGBA4bpp BasisImporterAstc4x4RGBA}
@m_keywords{BasisImporterAstc4x4RGBAF BasisImporterRGBA8 BasisImporterRGB16F}
@m_keywords{BasisImporterRGBA16F}

Imports [Basis Universal](https://github.com/binomialLLC/basis_universal)
compressed images (`*.basis` or `*.ktx2`) by parsing and transcoding files into
an explicitly specified GPU format (see @ref Trade-BasisImporter-target-format).
You can use @ref BasisImageConverter to transcode images into this format.

This plugin provides `BasisImporterEacR`, `BasisImporterEacRG`,
`BasisImporterEtc1RGB`, `BasisImporterEtc2RGBA`, `BasisImporterBc1RGB`,
`BasisImporterBc3RGBA`, `BasisImporterBc4R`, `BasisImporterBc5RG`,
`BasisImporterBc6hRGB`,, `BasisImporterBc7RGBA`, `BasisImporterPvrtc1RGB4bpp`,
`BasisImporterPvrtc1RGBA4bpp`, `BasisImporterAstc4x4RGBA`,
`BasisImporterAstc4x4RGBAF`, `BasisImporterRGBA8`, `BasisImporterRGB16F`,
`BasisImporterRGBA16F`.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [Basis Universal GPU Texture Codec](https://github.com/BinomialLLC/basis_universal)
    library, licensed under @m_class{m-label m-success} **Apache-2.0**
    ([license text](https://opensource.org/licenses/Apache-2.0),
    [choosealicense.com](https://choosealicense.com/licenses/apache-2.0/)). It
    requires attribution for public use.

@section Trade-BasisImporter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    through the base @ref AbstractImporter interface. See its documentation for
    introduction and usage examples.

This plugin depends on the @ref Trade and [Basis Universal](https://github.com/binomialLLC/basis_universal)
libraries and is built if `MAGNUM_WITH_BASISIMPORTER` is enabled when building
Magnum Plugins. To use as a dynamic plugin, load @cpp "BasisImporter" @ce via
@ref Corrade::PluginManager::Manager. Current version of the plugin is tested
against the [`v1_50_0_2`](https://github.com/BinomialLLC/basis_universal/tree/v1_50_0_2)
`1.16.4` and `v1_15_update2` tags, but could possibly compile against newer
versions as well.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins](https://github.com/mosra/magnum-plugins),
[zstd](https://github.com/facebook/zstd) and
[basis-universal](https://github.com/BinomialLLC/basis_universal) repositories
and do the following. Basis uses Zstd for KTX2 images, instead of bundling you
can depend on an externally-installed zstd package, in which case omit the
first part of the setup below. If Zstd isn't bundled and isn't found
externally, Basis will be compiled without Zstd support. See
@ref Trade-BasisImporter-binary-size below for additional options to control
what gets built.

@code{.cmake}
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
# Create a static library so the plugin is self-contained
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
# Basis doesn't use any multithreading in zstd, this prevents a need to link to
# pthread on Linux
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
# Don't build Zstd tests if enable_testing() was called in parent project
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(zstd/build/cmake EXCLUDE_FROM_ALL)

set(BASIS_UNIVERSAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/basis-universal)
set(MAGNUM_WITH_BASISIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::BasisImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake,
request the `BasisImporter` component of the `MagnumPlugins` package  and link
to the `MagnumPlugins::BasisImporter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED BasisImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::BasisImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-BasisImporter-behavior Behavior and limitations

@m_class{m-block m-warning}

@par Imported image orientation
    Both the Basis and the KTX2 file format can contain orientation metadata.
    If the orientation doesn't match Y up, the plugin will attempt to Y-flip
    the data on import. In case of the `basisu` tool you can pass `-y_flip` to
    encode the image with Y up but note the orientation
    [isn't correctly written to the file with a KTX2 output](https://github.com/BinomialLLC/basis_universal/issues/258).
    To hint about possible orientation issues, the plugin prints a message to @relativeref{Magnum,Warning} if the orientation metadata are not present in
    a KTX2 file. The @ref BasisImageConverter encodes Y up files by default and
    writes the metadata correctly for both Basis and KTX2 files.
@par
    Y-flipping block-compressed data is non-trivial and so far is implemented
    only for BC1, BC2, BC3, BC4 and BC5 formats with APIs from
    @ref Magnum/Math/ColorBatch.h. Other compressed formats will print a
    warning and the data will not be flipped. A warning also gets printed in
    case the flip is performed on an image whose height isn't whole blocks, as
    that causes the data to be shifted.
@par
    You can also set the @cb{.ini} assumeYUp @ce
    @ref Trade-BasisImporter-configuration "configuration option" to ignore
    any metadata, assume the Y up orientation and perform no flipping.

The importer recognizes @ref ImporterFlag::Verbose, printing additional info
when the flag is enabled. @ref ImporterFlag::Quiet is recognized as well and
causes all import warnings to be suppressed.

@subsection Trade-BasisImporter-behavior-types Image types

You can import all image types supported by `basisu`: (layered) 2D images,
(layered) cube maps, 3D images and videos. They can in turn all have multiple
mip levels. The images are annotated with @ref ImageFlag3D::Array and
@ref ImageFlag3D::CubeMap as appropriate. Furthermore, for backward
compatibility, if @ref MAGNUM_BUILD_DEPRECATED is enabled, the image type can
also be determined from @ref texture() and @ref TextureData::type().

For layered 2D images and (layered) cube maps, the array layers and faces are
exposed as an additional image dimension. @ref image3D() will return an
@ref ImageData3D with n z-slices, or 6*n z-slices for cube maps.

All 3D images will be imported as 2D array textures with as many layers as
depth slices. This unifies the behaviour with Basis compressed KTX2 files that
don't support 3D images in the first place, and avoids confusing behaviour with
mip levels which are always 2-dimensional in Basis compressed images.

Video files will be imported as multiple 2D images with the same size and level
count. Due to the way video is encoded by Basis Universal, seeking to arbitrary
frames is not allowed. If you call @ref image2D() with non-sequential frame
indices and that frame is not an I-frame, it will print an error and fail.
Restarting from frame 0 is always allowed.

@subsection Trade-BasisImporter-behavior-multilevel Multilevel images

Files with multiple mip levels are imported with the largest level first, with
the size of each following level divided by 2, rounded down. Mip chains can be
incomplete, ie. they don't have to extend all the way down to a level of size
1x1.

Because mip levels in `.basis` files are always 2-dimensional, they wouldn't
halve correctly in the z-dimension for 3D images. If a 3D image with mip levels
is detected, it gets imported as a layered 2D image instead, along with a
warning being printed.

@subsection Trade-BasisImporter-behavior-cube Cube maps

Cube map faces are imported in the order +X, -X, +Y, -Y, +Z, -Z as seen from a
left-handed coordinate system (+X is right, +Y is up, +Z is forward). Layered
cube maps are stored as multiple sets of faces, ie. all faces +X through -Z for
the first layer, then all faces of the second layer, etc.

@subsection Trade-BasisImporter-behavior-ktx KTX2 files

Basis Universal supports only the Basis-encoded subset of the KTX2 format. It
treats non-Basis-encoded KTX2 files the same way as broken KTX2 files, and so
the plugin cannot robustly proxy the loading to @ref KtxImporter in that case.
Instead, if you're dealing with generic KTX2 files, you're encouraged to use
@ref KtxImporter directly --- it will then delegate to @ref BasisImporter for
Basis-encoded files.

@section Trade-BasisImporter-configuration Plugin-specific configuration

Basis allows configuration of the format of loaded compressed data.
The full form of the configuration is shown below:

@snippet MagnumPlugins/BasisImporter/BasisImporter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.

@subsection Trade-BasisImporter-target-format Target format

Basis is a compressed format that is *transcoded* into a compressed GPU format.
When loading an HDR image, it will be transcoded to an HDR target format.
Conversely, non-HDR images can only be transcoded to non-HDR formats.
With @ref BasisImporter, these formats can be chosen in different ways:

@snippet BasisImporter.cpp target-format-suffix

The list of valid suffixes is equivalent to enum value names in
@ref TargetFormat. If you want to be able to change the target format
dynamically, set the @cb{.ini} format @ce and @cb{.ini} formatHdr @ce
@ref Trade-BasisImporter-configuration "configuration options".

@snippet BasisImporter.cpp target-format-config

There are many options and you should generally be striving for the
highest-quality format available on a given platform. A detailed description of
the choices can be found in the [Basis Universal Wiki](https://github.com/BinomialLLC/basis_universal/wiki/How-to-Deploy-ETC1S-Texture-Content-Using-Basis-Universal).
As an example, the following code is a decision making used by
@ref magnum-player "magnum-player" based on availability of corresponding
OpenGL, OpenGL ES and WebGL extensions, in its full ugly glory:

@m_class{m-console-wrap}

@snippet BasisImporter.cpp gl-extension-checks

Selecting an HDR format is a bit simpler:

@snippet BasisImporter.cpp gl-extension-checks-hdr

@subsection Trade-BasisImporter-binary-size Reducing binary size

To reduce the binary size of the transcoder, Basis Universal supports a set of
preprocessor defines to turn off unneeded features. The Basis Universal Wiki
lists macros to disable specific [target formats](https://github.com/BinomialLLC/basis_universal/wiki/How-to-Use-and-Configure-the-Transcoder#shrinking-the-transcoders-compiled-size)
as well as [KTX2 support including the Zstd dependency](https://github.com/BinomialLLC/basis_universal/wiki/How-to-Use-and-Configure-the-Transcoder#disabling-ktx2-or-zstandard-usage).
If you're building it from source with `BASIS_UNIVERSAL_DIR` set, add the
desired defines before adding `magnum-plugins` as a subfolder:

@code{.cmake}
add_definitions(
    -DBASISD_SUPPORT_BC7=0
    -DBASISD_SUPPORT_KTX2=0)

# ...
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)
@endcode
*/
class MAGNUM_BASISIMPORTER_EXPORT BasisImporter: public AbstractImporter {
    public:
        /**
         * @brief Format to transcode to
         *
         * Exposed for documentation purposes only. Pick the format either by
         * loading the plugin under one of the above-listed aliases with the
         * values as suffix, or by setting the @cb{.ini} format @ce and
         * @cb{.ini} formatHdr @ce @ref Trade-BasisImporter-configuration "configuration options".
         *
         * If the image does not contain an alpha channel and the target format
         * has it, alpha will be set to opaque. Conversely, for output formats
         * without alpha the channel will be dropped.
         * @see @ref Trade-BasisImporter-target-format
         */
        enum class TargetFormat: UnsignedInt {
            /* ID kept the same as in Basis itself to make the mapping easy */

            /**
             * ETC1 RGB. Loaded as @ref CompressedPixelFormat::Etc2RGB8Unorm/
             * @ref CompressedPixelFormat::Etc2RGB8Srgb (which ETC1 is a
             * subset of). If the image contains an alpha channel, it will be
             * dropped since ETC1 alone doesn't support alpha.
             */
            Etc1RGB = 0,

            /**
             * ETC2 RGBA. Loaded as @ref CompressedPixelFormat::Etc2RGBA8Unorm/
             * @ref CompressedPixelFormat::Etc2RGBA8Srgb.
             */
            Etc2RGBA = 1,

            /**
             * BC1 RGB. Loaded as @ref CompressedPixelFormat::Bc1RGBUnorm/
             * @ref CompressedPixelFormat::Bc1RGBSrgb. Punchthrough alpha mode
             * of BC1 RGBA is not supported.
             */
            Bc1RGB = 2,

            /**
             * BC3 RGBA. Loaded as @ref CompressedPixelFormat::Bc3RGBAUnorm/
             * @ref CompressedPixelFormat::Bc3RGBASrgb.
             */
            Bc3RGBA = 3,

            /** BC4 R. Loaded as @ref CompressedPixelFormat::Bc4RUnorm. */
            Bc4R = 4,

            /**
             * BC5 RG. Taken from the input red and alpha channel. Loaded as
             * @ref CompressedPixelFormat::Bc5RGUnorm.
             */
            Bc5RG = 5,

            /* Bc7RGB (=6) used to be the mode 6 transcoder which went away
               when UASTC was added. The old mode 5 transcoder (=7) is called
               BC7_ALT in the transcoder and is only kept around for backward
               compatibility but treated exactly the same as BC7_RGBA (=6). */

            /**
             * BC7 RGBA. If no alpha is present, it's set to opaque. Loaded as
             * @ref CompressedPixelFormat::Bc7RGBAUnorm/
             * @ref CompressedPixelFormat::Bc7RGBASrgb.
             */
            Bc7RGBA = 6,

            /**
             * PVRTC1 RGB 4 bpp. Loaded as
             * @ref CompressedPixelFormat::PvrtcRGB4bppUnorm/
             * @ref CompressedPixelFormat::PvrtcRGB4bppSrgb.
             */
            PvrtcRGB4bpp = 8,

            /**
             * PVRTC1 RGBA 4 bpp. Loaded as
             * @ref CompressedPixelFormat::PvrtcRGBA4bppUnorm/
             * @ref CompressedPixelFormat::PvrtcRGBA4bppSrgb.
             */
            PvrtcRGBA4bpp = 9,

            /**
             * ASTC 4x4 RGBA. Loaded as
             * @ref CompressedPixelFormat::Astc4x4RGBAUnorm/
             * @ref CompressedPixelFormat::Astc4x4RGBASrgb.
             */
            Astc4x4RGBA = 10,

            /* ATC (11, 12) skipped as Magnum doesn't have enums for those */

            /**
             * Uncompressed 32-bit RGBA. Loaded as
             * @ref PixelFormat::RGBA8Unorm / @ref PixelFormat::RGBA8Srgb. If
             * no concrete format is specified, the importer will fall back to
             * this.
             */
            RGBA8 = 13,

            /* RGB565 / BGR565 / RGBA4444 (14, 15, 16) skipped as Magnum
               doesn't have generic enums for those */

            /* FXT1 (17), PVRTC2 RGB / RGBA (18, 19) skipped as Magnum doesn't
               have enums for those */

            /**
             * EAC R. Loaded as @ref CompressedPixelFormat::EacR11Unorm.
             */
            EacR = 20,

            /**
             * EAC RG. Taken from the input red and alpha channel. Loaded as
             * @ref CompressedPixelFormat::EacRG11Unorm.
             */
            EacRG = 21,

            /**
             * BC6 RGB unsigned HDR. Loaded as
             * @ref CompressedPixelFormat::Bc6hRGBUfloat.
             */
            Bc6hRGB = 22,

            /**
             * ASTC 4x4 RGBA HDR. The alpha channel is always set to opaque.
             * Loaded as @ref CompressedPixelFormat::Astc4x4RGBAF.
             */
            Astc4x4RGBAF = 23,

            /**
             * Uncompressed half float RGB HDR. Loaded as
             * @ref PixelFormat::RGB16F.
             */
            RGB16F = 24,

            /**
             * Uncompressed half float RGBA HDR. The alpha channel is always
             * set to opaque. Loaded as @ref PixelFormat::RGBA16F.
             */
            RGBA16F = 25,
        };

        /** @brief Initialize Basis transcoder */
        static void initialize();

        #ifdef MAGNUM_BUILD_DEPRECATED
        /**
         * @brief Default constructor
         * @m_deprecated_since_latest Direct plugin instantiation isn't a
         *      supported use case anymore, instantiate through the plugin
         *      manager instead.
         */
        CORRADE_DEPRECATED("instantiate through the plugin manager instead") explicit BasisImporter();
        #endif

        /** @brief Plugin manager constructor */
        explicit BasisImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~BasisImporter();

        #ifdef MAGNUM_BUILD_DEPRECATED
        /**
         * @brief Target format
         * @m_deprecated_since_latest Direct plugin instantiation isn't a
         *      supported use case anymore and thus it isn't possible to query
         *      the current format with an API. Either check the
         *      `format` @ref Trade-BasisImporter-configuration "configuration option"
         *      or compare the name under which was the plugin loaded in
         *      @ref plugin() against the aliases listed above.
         */
        CORRADE_DEPRECATED("instantiate through the plugin manager instead") TargetFormat targetFormat() const;

        /**
         * @brief Set the target format
         * @m_deprecated_since_latest Direct plugin instantiation isn't a
         *      supported use case anymore and thus it isn't possible to query
         *      the current format with an API. Set the
         *      `format` @ref Trade-BasisImporter-configuration "configuration option"
         *      instead.
        */
        CORRADE_DEPRECATED("instantiate through the plugin manager instead") void setTargetFormat(TargetFormat format);
        #endif

    private:
        MAGNUM_BASISIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_BASISIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_BASISIMPORTER_LOCAL void doClose() override;
        MAGNUM_BASISIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;

        template<UnsignedInt dimensions> MAGNUM_BASISIMPORTER_LOCAL Containers::Optional<ImageData<dimensions>> doImage(UnsignedInt id, UnsignedInt level);

        MAGNUM_BASISIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_BASISIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_BASISIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_BASISIMPORTER_LOCAL UnsignedInt doImage3DCount() const override;
        MAGNUM_BASISIMPORTER_LOCAL UnsignedInt doImage3DLevelCount(UnsignedInt id) override;
        MAGNUM_BASISIMPORTER_LOCAL Containers::Optional<ImageData3D> doImage3D(UnsignedInt id, UnsignedInt level) override;

        #ifdef MAGNUM_BUILD_DEPRECATED
        MAGNUM_BASISIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_BASISIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;
        #endif

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
