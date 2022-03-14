#ifndef Magnum_Trade_BasisImporter_h
#define Magnum_Trade_BasisImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019, 2021 Jonathan Hale <squareys@googlemail.com>
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
@m_keywords{BasisImporterBc4R BasisImporterBc5RG BasisImporterBc7RGBA}
@m_keywords{BasisImporterPvrtc1RGB4bpp BasisImporterPvrtc1RGBA4bpp}
@m_keywords{BasisImporterAstc4x4RGBA BasisImporterRGBA8}

Supports [Basis Universal](https://github.com/binomialLLC/basis_universal)
compressed images (`*.basis` or `*.ktx2`) by parsing and transcoding files into
an explicitly specified GPU format (see @ref Trade-BasisImporter-target-format).
You can use @ref BasisImageConverter to transcode images into this format.

This plugin provides `BasisImporterEacR`, `BasisImporterEacRG`,
`BasisImporterEtc1RGB`, `BasisImporterEtc2RGBA`, `BasisImporterBc1RGB`,
`BasisImporterBc3RGBA`, `BasisImporterBc4R`, `BasisImporterBc5RG`,
`BasisImporterBc7RGBA`, `BasisImporterPvrtc1RGB4bpp`,
`BasisImporterPvrtc1RGBA4bpp`, `BasisImporterAstc4x4RGBA`, `BasisImporterRGBA8`.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [Basis Universal GPU Texture Codec](https://github.com/BinomialLLC/basis_universal)
    library, licensed under @m_class{m-label m-success} **Apache-2.0**
    ([license text](https://opensource.org/licenses/Apache-2.0),
    [choosealicense.com](https://choosealicense.com/licenses/apache-2.0/)). It
    requires attribution for public use.

@section Trade-BasisImporter-usage Usage

This plugin depends on the @ref Trade and [Basis Universal](https://github.com/binomialLLC/basis_universal)
libraries and is built if `WITH_BASISIMPORTER` is enabled when building Magnum
Plugins. To use as a dynamic plugin, load @cpp "BasisImporter" @ce via
@ref Corrade::PluginManager::Manager. Current version of the plugin is tested
against the [`v1_15_update2` tag](https://github.com/BinomialLLC/basis_universal/tree/v1_15_update2),
but could possibly compile against newer versions as well.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins](https://github.com/mosra/magnum-plugins) and
[basis-universal](https://github.com/BinomialLLC/basis_universal) repositories
and do the following:

@code{.cmake}
set(BASIS_UNIVERSAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/basis-universal)
set(WITH_BASISIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::BasisImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindBasisUniversal.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindBasisUniversal.cmake)
into your `modules/` directory, request the `BasisImporter` component of the
`MagnumPlugins` package  and link to the `MagnumPlugins::BasisImporter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED BasisImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::BasisImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-BasisImporter-behavior Behavior and limitations

@subsection Trade-BasisImporter-behavior-types Image types

You can import all image types supported by `basisu`: (layered) 2D images,
(layered) cube maps, 3D images and videos. They can in turn all have multiple
mip levels. The image type can be determined from @ref texture() and
@ref TextureData::type().

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

@m_class{m-block m-warning}

@par Y-flipping
    While all importers for uncompressed image data are performing a Y-flip on
    import to have the origin at the bottom (as expected by OpenGL), it's a
    non-trivial operation with compressed images. In case of Basis, you can
    pass a `-y_flip` flag to the `basisu` tool to Y-flip the image
    * *during encoding*, however right now there's no way do so on import. To
    inform the user, the importer checks for the Y-flip flag in the file and if
    it's not there, prints a warning about the data having wrong orientation.
@par
    To account for this on the application side for files that you don't have
    control over, flip texture coordinates of the mesh or patch texture data
    loading in the shader.

@section Trade-BasisImporter-configuration Plugin-specific configuration

Basis allows configuration of the format of loaded compressed data.
The full form of the configuration is shown below:

@snippet MagnumPlugins/BasisImporter/BasisImporter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.

@subsection Trade-BasisImporter-target-format Target format

Basis is a compressed format that is *transcoded* into a compressed GPU format.
With @ref BasisImporter, this format can be chosen in different ways:

@snippet BasisImporter.cpp target-format-suffix

The list of valid suffixes is equivalent to enum value names in
@ref TargetFormat. If you want to be able to change the target format
dynamically, you may want to set the `format` configuration of the plugin, as
shown below. If you instantiate this class directly without a plugin manager,
you may also use @ref setTargetFormat().

@snippet BasisImporter.cpp target-format-config

There are many options and you should generally be striving for the
highest-quality format available on a given platform. A detailed description of
the choices can be found in the [Basis Universal Wiki](https://github.com/BinomialLLC/basis_universal/wiki/How-to-Deploy-ETC1S-Texture-Content-Using-Basis-Universal).
As an example, the following code is a decision making used by
@ref magnum-player "magnum-player" based on availability of corresponding
OpenGL, OpenGL ES and WebGL extensions, in its full ugly glory:

@snippet BasisImporter.cpp gl-extension-checks

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
         * @brief Type to transcode to
         *
         * If the image does not contain an alpha channel and the target format
         * has it, alpha will be set to opaque. Conversely, for output formats
         * without alpha the channel will be dropped.
         * @see @ref Trade-BasisImporter-target-format, @ref setTargetFormat()
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
        };

        /**
         * @brief Initialize Basis transcoder
         *
         * If the class is instantiated directly (not through a plugin
         * manager), this function has to be called explicitly before using
         * any instance.
         */
        static void initialize();

        /** @brief Default constructor */
        explicit BasisImporter();

        /** @brief Plugin manager constructor */
        explicit BasisImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~BasisImporter();

        /** @brief Target format */
        TargetFormat targetFormat() const;

        /**
        * @brief Set the target format
        *
        * See @ref Trade-BasisImporter-target-format for more information.
        */
        void setTargetFormat(TargetFormat format);

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

        MAGNUM_BASISIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_BASISIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
