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
libraries and is built if `WITH_BASISIMAGECONVERTER` is enabled when building
Magnum Plugins. To use as a dynamic plugin, you need to load the
@cpp "BasisImageConverter" @ce plugin from `MAGNUM_PLUGINS_IMAGECONVERTER_DIR`.
To use as a static plugin or as a dependency of another plugin with CMake, you
need to request the `BasisImageConverter` component of the `MagnumPlugins`
package and link to the `MagnumPlugins::BasisImageConverter` target. See
@ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

@section Trade-BasisImageConverter-configuration Plugin-specific configuration

Basis compression can be configured to produce better quality or reduce
encoding time. Configuration options are equivalent to options of the `basisu`
tool. The full form of the configuration is shown below:

@snippet MagnumPlugins/BasisImageConverter/BasisImageConverter.conf configuration_

<b></b>

@section Trade-BasisImageConverter-loading Loading the plugin fails undefined symbol: pthread_create

On Linux it may happen that loading the plugin will fail with
`undefined symbol: pthread_create`. The Basis encoder is optionally
multithreaded and while linking the dynamic plugin library to `pthread` would
resolve this particular error, the actual thread creation (if the
@cb{.conf} threads= @ce option is set to something else than `1`) later would
crash on a null function pointer call. Unfortunately there's no way to detect
this case at runtime and fail gracefully, so instead the plugin requires
* *the application* to link to `pthread` instead. With CMake it can be done
like this:

@code{.cmake}
find_package(Threads REQUIRED)
target_link_libraries(your-application PRIVATE Threads::Threads)
@endcode

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [Basis Universal GPU Texture Codec](https://github.com/BinomialLLC/basis_universal)
    library, licensed under @m_class{m-label m-success} **Apache-2.0**
    ([license text](https://opensource.org/licenses/Apache-2.0),
    [choosealicense.com](https://choosealicense.com/licenses/apache-2.0/)). It
    requires attribution for public use.
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
