#ifndef Magnum_Trade_JpegImporter_h
#define Magnum_Trade_JpegImporter_h
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
 * @brief Class @ref Magnum::Trade::JpegImporter
 */

#include <Corrade/Containers/Array.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/JpegImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_JPEGIMPORTER_BUILD_STATIC
    #ifdef JpegImporter_EXPORTS
        #define MAGNUM_JPEGIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_JPEGIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_JPEGIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_JPEGIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_JPEGIMPORTER_EXPORT
#define MAGNUM_JPEGIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief JPEG importer plugin

Supports RGB or grayscale images with 8 bits per channel. The images are
imported with @ref PixelFormat::RGB8Unorm or @ref PixelFormat::R8Unorm. All
imported images use default @ref PixelStorage parameters.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the [libJPEG](http://ijg.org/) library,
    released under a custom @m_class{m-label m-success} **Libjpeg license**
    ([license text](https://jpegclub.org/reference/libjpeg-license/)). It
    requires attribution for public use. Note that this plugin can be built
    against any other compatible and possibly differently-licensed libJPEG
    implementation as well.

@section Trade-JpegImporter-usage Usage

This plugin depends on the @ref Trade and [libJPEG](http://libjpeg.sourceforge.net/) libraries and is built if `WITH_JPEGIMPORTER` is enabled when building Magnum
Plugins. To use as a dynamic plugin, load @cpp "JpegImporter" @ce
via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using libJPEG itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(WITH_JPEGIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::JpegImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `JpegImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::JpegImporter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED JpegImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::JpegImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-JpegImporter-implementations libJPEG implementations

While some systems (such as macOS) still ship only with the vanilla libJPEG,
you can get a much better decoding performance by using
[libjpeg-turbo](https://libjpeg-turbo.org/).
*/
class MAGNUM_JPEGIMPORTER_EXPORT JpegImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit JpegImporter();

        /** @brief Plugin manager constructor */
        explicit JpegImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~JpegImporter();

    private:
        MAGNUM_JPEGIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_JPEGIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_JPEGIMPORTER_LOCAL void doClose() override;
        MAGNUM_JPEGIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_JPEGIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_JPEGIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        Containers::Array<unsigned char> _in;
};

}}

#endif
