#ifndef Magnum_Trade_OpenExrImporter_h
#define Magnum_Trade_OpenExrImporter_h
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
 * @brief Class @ref Magnum::Trade::OpenExrImporter
 * @m_since_latest_{plugins}
 */

#include <Corrade/Containers/Pointer.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/OpenExrImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_OPENEXRIMPORTER_BUILD_STATIC
    #ifdef OpenExrImporter_EXPORTS
        #define MAGNUM_OPENEXRIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_OPENEXRIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_OPENEXRIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_OPENEXRIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_OPENEXRIMPORTER_EXPORT
#define MAGNUM_OPENEXRIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief OpenEXR importer plugin
@m_since_latest_{plugins}

Supports single-part OpenEXR (`*.exr`) images with half-float, float and
unsigned integer channels. You can use @ref OpenExrImageConverter to encode
images into this format.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the [OpenEXR](https://www.openexr.com)
    library, licensed under @m_class{m-label m-success} **BSD 3-clause**
    ([license text](https://github.com/AcademySoftwareFoundation/openexr/blob/master/LICENSE.md),
    [choosealicense.com](https://choosealicense.com/licenses/bsd-3-clause/)).
    It requires attribution for public use.

@section Trade-OpenExrImporter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_OPENEXRIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "OpenExrImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using OpenEXR itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(WITH_OPENEXRIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::OpenExrImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindOpenEXR.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindOpenEXR.cmake)
into your `modules/` directory, request the `OpenExrImporter` component of the
`MagnumPlugins` package in CMake and link to the
`MagnumPlugins::OpenExrImporter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED OpenExrImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::OpenExrImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-OpenExrImporter-behavior Behavior and limitations

@subsection Trade-OpenExrImporter-behavior-channel-mapping Channel mapping

Images containing `R`, `G`, `B` or `A` channels are imported as
@ref PixelFormat::R16F / @relativeref{PixelFormat,RG16F} /
@relativeref{PixelFormat,RGB16F} / @relativeref{PixelFormat,RGBA16F},
@ref PixelFormat::R32F / @relativeref{PixelFormat,RG32F} /
@relativeref{PixelFormat,RGB32F} / @relativeref{PixelFormat,RGBA32F} or
@ref PixelFormat::R32UI / @relativeref{PixelFormat,RG32UI} /
@relativeref{PixelFormat,RGB32UI} / @relativeref{PixelFormat,RGBA32UI}, all
channels are expected to have the same type.

If neither of the color channels is present and a a `Z` channel is present
instead, the image is imported as @ref PixelFormat::Depth32F, expecting the
channel to be of a float type.

If a file doesn't match ither of the above, custom channel mapping can be
supplied @ref Trade-OpenExrImporter-configuration "in the configuration".

@subsection Trade-OpenExrImporter-behavior-data-layout Data alignment, display and data windows

Image rows are always aligned to four bytes.

OpenEXR image metadata contain a display and data window, which can be used for
annotating crop borders or specifying that the data is just a small portion of
a larger image. The importer ignores the display window and imports everything
that's inside the data window, without offseting it in any way.

@subsection Trade-OpenExrImporter-behavior-multilayer-multipart Multilayer and multipart images

Images with custom layers (for example with separate channels for a left and
right view) can be imported by specifying the @cb{.ini} layer @ce
@ref Trade-OpenExrImporter-configuration "configuration option".

Multipart images are not supported right now.

@subsection Trade-OpenExrImporter-behavior-deep Cube map, environment map and deep images

Deep images are not supported right now, cube map and environment map images
are currently imported as two-dimensional images without any indication of
image type.

@section Trade-OpenExrImporter-configuration Plugin-specific configuration

It's possible to tune various options mainly for channel mapping through
@ref configuration(). See below for all options and their default values:

@snippet MagnumPlugins/OpenExrImporter/OpenExrImporter.conf config
*/
class MAGNUM_OPENEXRIMPORTER_EXPORT OpenExrImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit OpenExrImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~OpenExrImporter();

    private:
        MAGNUM_OPENEXRIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_OPENEXRIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_OPENEXRIMPORTER_LOCAL void doClose() override;
        MAGNUM_OPENEXRIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_OPENEXRIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_OPENEXRIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
