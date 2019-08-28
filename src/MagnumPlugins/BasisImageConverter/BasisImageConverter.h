#ifndef Magnum_Trade_BasisImageConverter_h
#define Magnum_Trade_BasisImageConverter_h
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
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/** @file
 * @brief Class @ref Magnum::Trade::BasisImageConverter
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/BasisImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_BASISIMAGECONVERTER_BUILD_STATIC
    #ifdef BasisImageConverter_EXPORTS
        #define MAGNUM_BASISIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_BASISIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_BASISIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_BASISIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_BASISIMAGECONVERTER_EXPORT
#define MAGNUM_BASISIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief BASIS image converter plugin

Creates [Basis Universal](https://github.com/binomialLLC/basis_universal)
(`*.basis`) files from images with format @ref PixelFormat::R8Unorm,
@ref PixelFormat::RG8Unorm, @ref PixelFormat::RGB8Unorm or
@ref PixelFormat::RGBA8Unorm.

This plugin depends on the @ref Trade and [Basis Universal](https://github.com/binomialLLC/basis_universal)
and is built if `WITH_BASISIMAGECONVERTER` is enabled when building Magnum
Plugins. To use as a dynamic plugin, you need to load the
@cpp "BasisImageConverter" @ce plugin from `MAGNUM_PLUGINS_IMAGECONVERTER_DIR`.
To use as a static plugin or as a dependency of another plugin with CMake, you
need to request the `BasisImageConverter` component of the `MagnumPlugins`
package and link to the `MagnumPlugins::BasisImageConverter` target. See
@ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

@section Trade-BasisImporter-configuration Plugin-specific configuration

Basis compression can be configured to produce better quality or reduce runtime.
Configuration options expose parameters of the `basisu` tool and match their
naming. The full form of the configuration is shown below:

@snippet MagnumPlugins/BasisImporter/BasisImporter.conf configuration

@subsection Trade-BasisImporter-magnum-configuration Magnum-specific configuration

The following options can be configured in addition to the ones that are
exposed `basisu` parameters.

-   `threads`: configure the number of threads basis should use during compression, `0`
       defaults to the value returned by @ref std::thread::hardware_concurrency().

-   `enable_debug_printf`: enable verbose logging in basis

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [Basis Universal GPU Texture Codec](https://github.com/BinomialLLC/basis_universal)
    library, licensed under @m_class{m-label m-info} **Apache-2.0**
    ([license text](https://opensource.org/licenses/Apache-2.0),
    [choosealicense.com](https://choosealicense.com/licenses/apache-2.0/)).
*/
class MAGNUM_BASISIMAGECONVERTER_EXPORT BasisImageConverter: public AbstractImageConverter {
    public:
        /** @brief Default constructor */
        explicit BasisImageConverter();

        /** @brief Plugin manager constructor */
        explicit BasisImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_BASISIMAGECONVERTER_LOCAL Features doFeatures() const override;
        MAGNUM_BASISIMAGECONVERTER_LOCAL Containers::Array<char> doExportToData(const ImageView2D& image) override;
};

}}

#endif
