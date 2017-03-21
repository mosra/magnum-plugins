#ifndef Magnum_Trade_PngImporter_h
#define Magnum_Trade_PngImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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
 * @brief Class @ref Magnum::Trade::PngImporter
 */

#include <Corrade/Containers/Array.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/PngImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_PNGIMPORTER_BUILD_STATIC
    #if defined(PngImporter_EXPORTS) || defined(PngImporterObjects_EXPORTS)
        #define MAGNUM_PNGIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_PNGIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_PNGIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_PNGIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief PNG importer plugin

Supports Portable Network Graphics (`*.png`) RGB, RGBA or grayscale images with
8 and 16 bits per channel. Palleted images and images with transparency mask
are automatically converted to RGB(A).

This plugin depends on **libPNG** library and is built if `WITH_PNGIMPORTER`
is enabled when building Magnum Plugins. To use dynamic plugin, you need to
load `PngImporter` plugin from `MAGNUM_PLUGINS_IMPORTER_DIR`. To use static
plugin, you need to request `PngImporter` component of `MagnumPlugins`
package in CMake and link to `MagnumPlugins::PngImporter` target. See
@ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

The images are imported with @ref PixelType::UnsignedByte / @ref PixelType::UnsignedShort
and @ref PixelFormat::RGB, @ref PixelFormat::RGBA or @ref PixelFormat::Red,
respectively. Grayscale images require extension @extension{ARB,texture_rg}.
All imported images use default @ref PixelStorage parameters.

In OpenGL ES 2.0, if @extension{EXT,texture_rg} is not supported and in
WebGL 1.0, grayscale images use @ref PixelFormat::Luminance instead of
@ref PixelFormat::Red.
*/
class MAGNUM_PNGIMPORTER_EXPORT PngImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit PngImporter();

        /** @brief Plugin manager constructor */
        explicit PngImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~PngImporter();

    private:
        MAGNUM_PNGIMPORTER_LOCAL Features doFeatures() const override;
        MAGNUM_PNGIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_PNGIMPORTER_LOCAL void doClose() override;
        MAGNUM_PNGIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_PNGIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_PNGIMPORTER_LOCAL std::optional<ImageData2D> doImage2D(UnsignedInt id) override;

        Containers::Array<unsigned char> _in;
};

}}

#endif
