#ifndef Magnum_Trade_BasisImageConverter_h
#define Magnum_Trade_BasisImageConverter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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
 * @m_since_{plugins,2019,10}
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
@brief Basis Universal image converter plugin
@m_since_{plugins,2019,10}

Creates [Basis Universal](https://github.com/binomialLLC/basis_universal)
(`*.basis`) files from images with format @ref PixelFormat::R8Unorm,
@ref PixelFormat::RG8Unorm, @ref PixelFormat::RGB8Unorm or
@ref PixelFormat::RGBA8Unorm. Use @ref BasisImporter to import images in this
format.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [Basis Universal GPU Texture Codec](https://github.com/BinomialLLC/basis_universal)
    library, licensed under @m_class{m-label m-success} **Apache-2.0**
    ([license text](https://opensource.org/licenses/Apache-2.0),
    [choosealicense.com](https://choosealicense.com/licenses/apache-2.0/)). It
    requires attribution for public use.

@section Trade-BasisImageConverter-usage Usage

This plugin depends on the @ref Trade and [Basis Universal](https://github.com/binomialLLC/basis_universal)
libraries and is built if `WITH_BASISIMAGECONVERTER` is enabled when building
Magnum Plugins. To use as a dynamic plugin, load @cpp "BasisImageConverter" @ce
via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins](https://github.com/mosra/magnum-plugins) and
[basis-universal](https://github.com/BinomialLLC/basis_universal) repositories
and do the following:

@code{.cmake}
set(BASIS_UNIVERSAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/basis-universal)
set(WITH_BASISIMAGECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::BasisImageConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindBasisUniversal.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindBasisUniversal.cmake)
into your `modules/` directory, request the `BasisImageConverter` component of
the `MagnumPlugins` package and link to the `MagnumPlugins::BasisImageConverter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED BasisImageConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::BasisImageConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-BasisImageConverter-behavior Behavior and limitations

@subsection Trade-BasisImageConverter-behavior-multiple-images Multiple images in one file

Due to limitations in the @ref AbstractImageConverter API, it's currently not
possible to create a Basis file containing multiple images --- you'd need to
use the upstream `basisu` tool for that instead.

While it's possible to tell Basis to create mip levels from the top-level image
using the @cb{.ini} mip_gen @ce @ref Trade-BasisImageConverter-configuration "configuration option",
due to the same plugin API limitations it's not possible to supply the mip
levels directly.

@subsection Trade-BasisImageConverter-behavior-loading Loading the plugin fails undefined symbol: pthread_create

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

@section Trade-BasisImageConverter-configuration Plugin-specific configuration

Basis compression can be configured to produce better quality or reduce
encoding time. Configuration options are equivalent to options of the `basisu`
tool. The full form of the configuration is shown below:

@snippet MagnumPlugins/BasisImageConverter/BasisImageConverter.conf configuration_

*/
class MAGNUM_BASISIMAGECONVERTER_EXPORT BasisImageConverter: public AbstractImageConverter {
    public:
        /** @brief Default constructor */
        explicit BasisImageConverter();

        /** @brief Plugin manager constructor */
        explicit BasisImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_BASISIMAGECONVERTER_LOCAL ImageConverterFeatures doFeatures() const override;
        MAGNUM_BASISIMAGECONVERTER_LOCAL Containers::Array<char> doConvertToData(const ImageView2D& image) override;
};

}}

#endif
