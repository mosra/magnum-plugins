#ifndef Magnum_Trade_BasisImporter_h
#define Magnum_Trade_BasisImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019, 2021 Jonathan Hale <squareys@googlemail.com>

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

@m_keywords{BasisImporterEacR BasisImporterEacRG BasisImporterEtc1RGB} @m_keywords{BasisImporterEtc2RGBA BasisImporterBc1RGB BasisImporterBc3RGBA} @m_keywords{BasisImporterBc4R BasisImporterBc5RG BasisImporterBc7RGB} @m_keywords{BasisImporterBc7RGBA BasisImporterPvrtc1RGB4bpp}
@m_keywords{BasisImporterPvrtc1RGBA4bpp BasisImporterAstc4x4RGBA}
@m_keywords{BasisImporterRGBA8}

Supports [Basis Universal](https://github.com/binomialLLC/basis_universal)
(`*.basis`) compressed images by parsing and transcoding files into an
explicitly specified GPU format (see @ref Trade-BasisImporter-target-format).
You can use @ref BasisImageConverter to transcode images into this format.

This plugin provides `BasisImporterEacR`, `BasisImporterEacRG`,
`BasisImporterEtc1RGB`, `BasisImporterEtc2RGBA`, `BasisImporterBc1RGB`,
`BasisImporterBc3RGBA`, `BasisImporterBc4R`, `BasisImporterBc5RG`,
`BasisImporterBc7RGB`, `BasisImporterBc7RGBA`, `BasisImporterPvrtc1RGB4bpp`,
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
@ref Corrade::PluginManager::Manager.

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

@section Trade-BasisImporter-configuration Plugin-specific configuration

Basis allows configuration of the format of loaded compressed data.
The full form of the configuration is shown below:

@snippet MagnumPlugins/BasisImporter/BasisImporter.conf configuration_

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

There's many options and you should be generally striving for highest-quality
format available on given platform. Detailed description of the choices is
in [Basis Universal README](https://github.com/BinomialLLC/basis_universal#how-to-use-the-system).
As an example, the following code is a decision making used by
@ref magnum-player "magnum-player" based on availability of corresponding
OpenGL, OpenGL ES and WebGL extensions, in its full ugly glory:

@snippet BasisImporter.cpp gl-extension-checks

<b></b>

@m_class{m-block m-warning}

@par Y-flipping
    While all importers for uncompressed image data are doing an Y-flip on
    import to have origin at the bottom (as expected by OpenGL), it's a
    non-trivial operation with compressed images. In case of Basis, you can
    pass a `-y_flip` flag to the `basisu` tool to Y-flip the image
    * *during encoding*, however right now there's no way do so on import. To
    inform the user, the importer checks for the Y-flip flag in the file and if
    it's not there, prints a warning about the data having wrong orientation.
@par
    To account for this on the application side for files that you don't have a
    control of, flip texture coordinates of the mesh or patch texture data
    loading in the shader.
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
             * ETC1 RGB. Loaded as @ref CompressedPixelFormat::Etc2RGB8Unorm
             * (which ETC1 is a subset of). If the image contains an alpha
             * channel, it will be dropped since ETC1 alone doesn't support
             * alpha.
             */
            Etc1RGB = 0,

            /**
             * ETC2 RGBA. Loaded as @ref CompressedPixelFormat::Etc2RGBA8Unorm.
             */
            Etc2RGBA = 1,

            /**
             * BC1 RGB. Loaded as @ref CompressedPixelFormat::Bc1RGBUnorm.
             * Punchthrough alpha mode of @ref CompressedPixelFormat::Bc1RGBAUnorm
             * is not supported.
             */
            Bc1RGB = 2,

            /**
             * BC2 RGBA. Loaded as @ref CompressedPixelFormat::Bc3RGBAUnorm.
             */
            Bc3RGBA = 3,

            /** BC4 R. Loaded as @ref CompressedPixelFormat::Bc4RUnorm. */
            Bc4R = 4,

            /** BC5 RG. Loaded as @ref CompressedPixelFormat::Bc5RGUnorm. */
            Bc5RG = 5,

            /**
             * BC7 RGB (mode 6). Loaded as
             * @ref CompressedPixelFormat::Bc7RGBAUnorm, but with alpha values
             * set to opaque.
             */
            Bc7RGB = 6,

            /**
             * BC7 RGBA (mode 5). Loaded as
             * @ref CompressedPixelFormat::Bc7RGBAUnorm.
             */
            Bc7RGBA = 7,

            /**
             * PVRTC1 RGB 4 bpp. Loaded as
             * @ref CompressedPixelFormat::PvrtcRGB4bppUnorm.
             */
            PvrtcRGB4bpp = 8,

            /**
             * PVRTC1 RGBA 4 bpp. Loaded as
             * @ref CompressedPixelFormat::PvrtcRGBA4bppUnorm.
             */
            PvrtcRGBA4bpp = 9,

            /**
             * ASTC 4x4 RGBA. Loaded as
             * @ref CompressedPixelFormat::Astc4x4RGBAUnorm.
             */
            Astc4x4RGBA = 10,

            /* ATC (11, 12) skipped as Magnum doesn't have enums for those */

            /**
             * Uncompressed 32-bit RGBA. Loaded as
             * @ref PixelFormat::RGBA8Unorm. If no concrete format is
             * specified, the importer will fall back to this.
             */
            RGBA8 = 13,

            /* RGB565 / BGR565 / RGBA4444 (14, 15, 16) skipped as Magnum
               doesn't have generic enums for those */

            /* FXT1 (17), PVRTC2 RGB / RGBA (18, 19) skipped as Magnum doesn't
               have enums for those */

            /**
             * EAC unsigned red component. Loaded as
             * @ref CompressedPixelFormat::EacR11Unorm.
             */
            EacR = 20,

            /**
             * EAC unsigned red and green component. Loaded as
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
        explicit BasisImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

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
        struct State;

        MAGNUM_BASISIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_BASISIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_BASISIMPORTER_LOCAL void doClose() override;
        MAGNUM_BASISIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_BASISIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_BASISIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_BASISIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        Containers::Pointer<State> _state;
};

}}

#endif
