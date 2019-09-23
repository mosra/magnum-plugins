#ifndef Magnum_Trade_BasisImporter_h
#define Magnum_Trade_BasisImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019 Jonathan Hale <squareys@googlemail.com>

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
*/

#include <Corrade/Containers/Array.h>
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
@brief Type to transcode to
*/
enum class BasisTranscodingType : Int {
    /**
     * ETC1. Loaded as @ref CompressedPixelFormat::Etc2RGB8Unorm or
     * @ref CompressedPixelFormat::Etc2RGBA8Unorm if the image contains an
     * alpha channel.
     *
     * This format is loaded like @ref BasisTranscodingType::Etc2, prefer it
     * for potentially better quality.
     */
    Etc1 = 0,

    /**
     * ETC2. Loaded as @ref CompressedPixelFormat::Etc2RGB8Unorm or
     * @ref CompressedPixelFormat::Etc2RGBA8Unorm if the image contains an
     * alpha channel.
     */
    Etc2 = 1,

    /**
     * BC1. Loaded as @ref CompressedPixelFormat::Bc1RGBAUnorm or
     * @ref CompressedPixelFormat::Bc1RGBUnorm if the image contains an alpha
     * channel.
     */
    Bc1 = 2,

    /**
     * BC2. Loaded as @ref CompressedPixelFormat::Bc3RGBAUnorm. If the image
     * does not contain an alpha channel, alpha will be set to opaque.
     */
    Bc3 = 3,

    /**
     * BC4. Loaded as @ref CompressedPixelFormat::Bc4RUnorm.
     */
    Bc4 = 4,

    /**
     * BC5. Loaded as @ref CompressedPixelFormat::Bc5RGUnorm.
     */
    Bc5 = 5,

    /**
     * BC7 mode 6 (opaque only). Loaded as
     * @ref CompressedPixelFormat::Bc7RGBAUnorm, but with alpha values set to
     * opaque.
     */
    Bc7M6OpaqueOnly = 6,

    /**
     * PVRTC1 4 bpp (opaque only). Loaded as
     * @ref CompressedPixelFormat::PvrtcRGB4bppUnorm. If the image contains an
     * alpha channel, it will be dropped.
     */
    Pvrtc1_4OpaqueOnly = 7
};

/**
@brief Basis Universal importer plugin

@m_keywords{BasisImporterEtc1 BasisImporterEtc2 BasisImporterBc1 BasisImporterBc3}
@m_keywords{BasisImporterBc4 BasisImporterBc5 BasisImporterBc7M6OpaqueOnly}
@m_keywords{BasisImporterPvrtc1_4OpaqueOnly}

Supports [Basis Universal](https://github.com/binomialLLC/basis_universal)
(`*.basis`) compressed images by parsing and transcoding files into an
explicitly specified GPU format (see @ref Trade-BasisImporter-target-format).

This plugin depends on the @ref Trade and [Basis Universal](https://github.com/binomialLLC/basis_universal)
libraries and is built if `WITH_BASISIMPORTER` is enabled when building Magnum
Plugins. To use as a dynamic plugin, you need to load the
@cpp "BasisImporter" @ce plugin from `MAGNUM_PLUGINS_IMPORTER_DIR`. To use as a
static plugin or as a dependency of another plugin with CMake, you need to
request the `BasisImporter` component of the `MagnumPlugins` package and link
to the `MagnumPlugins::BasisImporter` target. See @ref building-plugins,
@ref plugins for more information.

This plugin provides `BasisImporterEtc1`, `BasisImporterEtc2`,
`BasisImporterBc1`, `BasisImporterBc3`, `BasisImporterBc4`, `BasisImporterBc5`,
`BasisImporterBc7M6OpaqueOnly`, `BasisImporterPvrtc1_4OpaqueOnly`.

@section Trade-BasisImporter-configuration Plugin-specific configuration

Basis allows configuration of the format of loaded compressed data.
The full form of the configuration is shown below:

@snippet MagnumPlugins/BasisImporter/BasisImporter.conf configuration_

@subsection Trade-BasisImporter-target-format Target format

Basis is a compressed format that is *transcoded* into a compressed GPU format.
With @ref BasisImporter, this format can be chosen in different ways:

@snippet BasisImporter.cpp target-format-suffix

The list of valid suffixes is equivalent to enum value names in
@ref BasisTranscodingType. If you want to be able to change the target format
dynamically, you may want to set the `format` configuration of the plugin:

@snippet BasisImporter.cpp target-format-config

If you instantiate this class directly without a plugin manager, you may also
use @ref setTargetFormat().

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [Basis Universal GPU Texture Codec](https://github.com/BinomialLLC/basis_universal)
    library, licensed under @m_class{m-label m-info} **Apache-2.0**
    ([license text](https://opensource.org/licenses/Apache-2.0),
    [choosealicense.com](https://choosealicense.com/licenses/apache-2.0/)).
*/
class MAGNUM_BASISIMPORTER_EXPORT BasisImporter: public AbstractImporter {
    public:
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
        BasisTranscodingType targetFormat() const;

        /**
        * @brief Set the target format
        *
        * See @ref Trade-BasisImporter-target-format for more information.
        */
        void setTargetFormat(BasisTranscodingType format);

    private:
        struct State;

        MAGNUM_BASISIMPORTER_LOCAL Features doFeatures() const override;
        MAGNUM_BASISIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_BASISIMPORTER_LOCAL void doClose() override;
        MAGNUM_BASISIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_BASISIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_BASISIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id) override;

        Containers::Array<unsigned char> _in;
        Containers::Pointer<State> _state;
};

}}

#endif
