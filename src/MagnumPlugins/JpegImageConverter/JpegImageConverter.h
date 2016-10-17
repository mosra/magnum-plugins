#ifndef Magnum_Trade_JpegImageConverter_h
#define Magnum_Trade_JpegImageConverter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015
              Vladimír Vondruš <mosra@centrum.cz>

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
#endif

namespace Magnum { namespace Trade {

/**
@brief JPEG image converter plugin

Supports images with format @ref PixelFormat::Red or @ref PixelFormat::RGB and
type @ref PixelType::UnsignedByte. On OpenGL ES 2.0 and WebGL 1.0 accepts also
@ref PixelFormat::Luminance instead of @ref PixelFormat::Red. Does *not*
support non-default @ref PixelStorage::swapBytes() values.

This plugin depends on **libJPEG** library and is built if `WITH_JPEGIMAGECONVERTER`
is enabled when building Magnum Plugins. To use dynamic plugin, you need to load
`JpegImageConverter` plugin from `MAGNUM_PLUGINS_IMAGECONVERTER_DIR`. To use
static plugin, you need to request `JpegImageConverter` component of
`MagnumPlugins` package in CMake and link to `MagnumPlugins::JpegImageConverter`
target. See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class MAGNUM_JPEGIMAGECONVERTER_EXPORT JpegImageConverter: public AbstractImageConverter {
    public:
        /** @brief Default constructor */
        explicit JpegImageConverter();

        /** @brief Plugin manager constructor */
        explicit JpegImageConverter(PluginManager::AbstractManager& manager, std::string plugin);

    private:
        MAGNUM_JPEGIMAGECONVERTER_LOCAL Features doFeatures() const override;
        MAGNUM_JPEGIMAGECONVERTER_LOCAL Containers::Array<char> doExportToData(const ImageView2D& image) override;
};

}}

#endif
