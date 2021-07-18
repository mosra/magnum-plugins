#ifndef Magnum_Trade_OpenExrImageConverter_h
#define Magnum_Trade_OpenExrImageConverter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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
 * @brief Class @ref Magnum::Trade::OpenExrImageConverter
 * @m_since_latest_{plugins}
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/OpenExrImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_OPENEXRIMAGECONVERTER_BUILD_STATIC
    #ifdef OpenExrImageConverter_EXPORTS
        #define MAGNUM_OPENEXRIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_OPENEXRIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_OPENEXRIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_OPENEXRIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_OPENEXRIMAGECONVERTER_EXPORT
#define MAGNUM_OPENEXRIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief OpenEXR image converter plugin
@m_since_latest_{plugins}

Creates OpenEXR (`*.exr`) files from images with
@ref PixelFormat::R16F / @relativeref{PixelFormat,RG16F} /
@relativeref{PixelFormat,RGB16F} / @relativeref{PixelFormat,RGBA16F},
@ref PixelFormat::R32F / @relativeref{PixelFormat,RG32F} /
@relativeref{PixelFormat,RGB32F} / @relativeref{PixelFormat,RGBA32F} or
@ref PixelFormat::R32UI / @relativeref{PixelFormat,RG32UI} /
@relativeref{PixelFormat,RGB32UI} / @relativeref{PixelFormat,RGBA32UI} and
@ref PixelFormat::Depth32F. You can use @ref OpenExrImporter to import images
in this format.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the [OpenEXR](https://www.openexr.com)
    library, licensed under @m_class{m-label m-success} **BSD 3-clause**
    ([license text](https://github.com/AcademySoftwareFoundation/openexr/blob/master/LICENSE.md),
    [choosealicense.com](https://choosealicense.com/licenses/bsd-3-clause/)).
    It requires attribution for public use.

@section Trade-OpenExrImageConverter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_OPENEXRIMAGECONVERTER` is enabled when building Magnum Plugins. To use as
a dynamic plugin, load @cpp "OpenExrImageConverter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins](https://github.com/mosra/magnum-plugins) and
[openexr](https://github.com/AcademySoftwareFoundation/openexr) repositories
(pin OpenEXR at `v3.0.1` at least) and do the following. OpenEXR depends on
zlib and Imath, however it's capable of fetching those dependencies on its own
so bundling them isn't necessary. If you want to use system-installed OpenEXR,
omit the  first part and point `CMAKE_PREFIX_PATH` to its installation dir if
necessary.

@code{.cmake}
# Disable unneeded functionality
set(PYILMBASE_ENABLE OFF CACHE BOOL "" FORCE)
set(IMATH_INSTALL_PKG_CONFIG OFF CACHE BOOL "" FORCE)
set(IMATH_INSTALL_SYM_LINK OFF CACHE BOOL "" FORCE)
set(OPENEXR_INSTALL OFF CACHE BOOL "" FORCE)
set(OPENEXR_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(OPENEXR_INSTALL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(OPENEXR_INSTALL_PKG_CONFIG OFF CACHE BOOL "" FORCE)
set(OPENEXR_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
set(OPENEXR_BUILD_UTILS OFF CACHE BOOL "" FORCE)
# Otherwise OpenEXR uses C++14, and before OpenEXR 3.0.2 also forces C++14 on
# all libraries that link to it.
set(OPENEXR_CXX_STANDARD 11 CACHE STRING "" FORCE)
# OpenEXR implicitly bundles Imath. However, without this only the first CMake
# run will pass and subsequent runs will fail.
set(CMAKE_DISABLE_FIND_PACKAGE_Imath ON)
# These variables may be used by other projects, so ensure they're reset back
# to their original values after. OpenEXR forces CMAKE_DEBUG_POSTFIX to _d,
# which isn't desired outside of that library.
set(_PREV_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(_PREV_BUILD_TESTING ${BUILD_TESTING})
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "" FORCE)
add_subdirectory(openexr EXCLUDE_FROM_ALL)
set(BUILD_SHARED_LIBS ${_PREV_BUILD_SHARED_LIBS})
set(BUILD_TESTING ${_PREV_BUILD_TESTING})
unset(CMAKE_DEBUG_POSTFIX CACHE)

set(WITH_OPENEXRIMAGECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::OpenExrImageConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindOpenEXR.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindOpenEXR.cmake)
into your `modules/` directory, request the `OpenExrImageConverter` component
of the `MagnumPlugins` package and link to the
`MagnumPlugins::OpenExrImageConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED OpenExrImageConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::OpenExrImageConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-OpenExrImageConverter-behavior Behavior and limitations

@subsection Trade-OpenExrImageConverter-behavior-channel-mapping Channel mapping

Images with @ref PixelFormat::R16F / @relativeref{PixelFormat,RG16F} /
@relativeref{PixelFormat,RGB16F} / @relativeref{PixelFormat,RGBA16F},
@ref PixelFormat::R32F / @relativeref{PixelFormat,RG32F} /
@relativeref{PixelFormat,RGB32F} / @relativeref{PixelFormat,RGBA32F} or
@ref PixelFormat::R32UI / @relativeref{PixelFormat,RG32UI} /
@relativeref{PixelFormat,RGB32UI} / @relativeref{PixelFormat,RGBA32UI} are
implicitly written to channels named `R`, `G`, `B` and `A`; images with
@ref PixelFormat::Depth32F are implicitly written to a `Z` channel.

If the default behavior is not sufficient, custom channel mapping can be
supplied @ref Trade-OpenExrImageConverter-configuration "in the configuration".

@subsection Trade-OpenExrImageConverter-behavior-multilayer-multipart Multilayer and multipart images

Channels can be prefixed with a custom layer name by specifying the
@cb{.ini} layer @ce @ref Trade-OpenExrImporter-configuration "configuration option". Combining multiple layers into a single image isn't supported right
now, writing multiple images into a multipart file is not supported either.

@subsection Trade-OpenExrImageConverter-behavior-deep Cube map, environment map and deep images

Creating deep images is not supported right now, cube map and environment map
images can be only written from a two-dimensional input, however there's
currently no way to mark them properly in the metadata.

@section Trade-OpenExrImageConverter-configuration Plugin-specific configuration

It's possible to tune various options mainly for channel mapping through
@ref configuration(). See below for all options and their default values:

@snippet MagnumPlugins/OpenExrImageConverter/OpenExrImageConverter.conf config

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_OPENEXRIMAGECONVERTER_EXPORT OpenExrImageConverter: public AbstractImageConverter {
    public:
        /** @brief Plugin manager constructor */
        explicit OpenExrImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_OPENEXRIMAGECONVERTER_LOCAL ImageConverterFeatures doFeatures() const override;
        MAGNUM_OPENEXRIMAGECONVERTER_LOCAL Containers::Array<char> doConvertToData(const ImageView2D& image) override;
};

}}

#endif
