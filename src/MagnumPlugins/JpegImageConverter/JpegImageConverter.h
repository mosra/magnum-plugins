#ifndef Magnum_Trade_JpegImageConverter_h
#define Magnum_Trade_JpegImageConverter_h
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
 * @brief Class @ref Magnum::Trade::JpegImageConverter
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/JpegImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_JPEGIMAGECONVERTER_BUILD_STATIC
    #if defined(JpegImageConverter_EXPORTS) || defined(JpegImageConverterObjects_EXPORTS)
        #define MAGNUM_JPEGIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_JPEGIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_JPEGIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_JPEGIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_JPEGIMAGECONVERTER_EXPORT
#define MAGNUM_JPEGIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief JPEG image converter plugin

Creates JPEG (`*.jpg`, `*.jpe`, `*.jpeg`) files from images with format
@ref PixelFormat::R8Unorm or @ref PixelFormat::RGB8Unorm. Images in
@ref PixelFormat::RGBA8Unorm are supported only if you use libJPEG Turbo
instead of vanilla libJPEG and the alpha channel gets ignored (with a warning
printed to the console). @ref PixelFormat::RG8Unorm can't be easily supported,
see @ref StbImageConverter for an alternative with a possibility to export RG
images as a grayscale JPEG. You can use @ref JpegImporter to import images in
this format.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the [libJPEG](http://ijg.org/) library,
    released under a custom @m_class{m-label m-success} **Libjpeg license**
    ([license text](https://jpegclub.org/reference/libjpeg-license/)). It
    requires attribution for public use. Note that this plugin can be built
    against any other compatible and possibly differently-licensed libJPEG
    implementation as well.

@section Trade-JpegImageConverter-usage Usage

This plugin depends on the @ref Trade and [libJPEG](http://libjpeg.sourceforge.net/)
libraries and is built if `WITH_JPEGIMAGECONVERTER` is enabled when building
Magnum Plugins. To use as a dynamic plugin, load @cpp "JpegImageConverter" @ce
via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using libJPEG itself as a CMake subproject isn't tested at the
moment, so you need to provide it as a system dependency and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(WITH_JPEGIMAGECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::JpegImageConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `JpegImageConverter` component of
the `MagnumPlugins` package and link to the `MagnumPlugins::JpegImageConverter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED JpegImageConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::JpegImageConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-JpegImageConverter-behavior Behavior and limitations

@subsection Trade-JpegImageConverter-behavior-implementations libJPEG implementations

While some systems (such as macOS) still ship only with the vanilla libJPEG,
you can get a much better performance and/or quality/size ratios by using other
implementations:

-   [libjpeg-turbo](https://libjpeg-turbo.org/), optimized for compression and
    decompression speed, though not necessarily the best quality/size ratio
-   [MozJPEG](https://github.com/mozilla/mozjpeg), optimized for quality/size
    ratio, though generally much slower than libjpeg-turbo

@subsection Trade-JpegImageConverter-behavior-arithmetic-coding Arithmetic JPEG encoding

Libjpeg has a switch to enable [arithmetic coding](https://en.wikipedia.org/wiki/Arithmetic_coding)
instead of Huffman, however it's currently not exposed in the plugin.

@section Trade-JpegImageConverter-configuration Plugin-specific config

It's possible to tune various output options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/JpegImageConverter/JpegImageConverter.conf config

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_JPEGIMAGECONVERTER_EXPORT JpegImageConverter: public AbstractImageConverter {
    public:
        /** @brief Default constructor */
        explicit JpegImageConverter();

        /** @brief Plugin manager constructor */
        explicit JpegImageConverter(PluginManager::AbstractManager& manager, std::string plugin);

    private:
        MAGNUM_JPEGIMAGECONVERTER_LOCAL ImageConverterFeatures doFeatures() const override;
        MAGNUM_JPEGIMAGECONVERTER_LOCAL Containers::Array<char> doConvertToData(const ImageView2D& image) override;
};

}}

#endif
