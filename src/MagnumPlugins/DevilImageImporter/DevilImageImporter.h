#ifndef Magnum_Trade_DevilImageImporter_h
#define Magnum_Trade_DevilImageImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016
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
 * @brief Class @ref Magnum::Trade::DevilImageImporter
 */

#include <Corrade/Containers/Array.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/DevilImageImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_DEVILIMAGEIMPORTER_BUILD_STATIC
    #if defined(DevilImageImporter_EXPORTS) || defined(DevilImageImporterObjects_EXPORTS)
        #define MAGNUM_DEVILIMAGEIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_DEVILIMAGEIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_DEVILIMAGEIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_DEVILIMAGEIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief JPEG importer plugin

Supports RGB or grayscale images with 8 bits per channel.

This plugin depends on **libJPEG** library and is built if `WITH_DEVILIMAGEIMPORTER`
is enabled when building Magnum Plugins. To use dynamic plugin, you need to
load `DevilImageImporter` plugin from `MAGNUM_PLUGINS_IMPORTER_DIR`. To use static
plugin, you need to request `DevilImageImporter` component of `MagnumPlugins`
package in CMake and link to `MagnumPlugins::DevilImageImporter`. See
@ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

The images are imported with @ref PixelType::UnsignedByte and @ref PixelFormat::RGB
or @ref PixelFormat::Red, respectively. Grayscale images require extension
@extension{ARB,texture_rg}. All imported images use default @ref PixelStorage
parameters.

In OpenGL ES 2.0, if @es_extension{EXT,texture_rg} is not supported and in
WebGL 1.0, grayscale images use @ref PixelFormat::Luminance instead of
@ref PixelFormat::Red.
*/
class MAGNUM_DEVILIMAGEIMPORTER_EXPORT DevilImageImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit DevilImageImporter();

        /** @brief Plugin manager constructor */
        explicit DevilImageImporter(PluginManager::AbstractManager& manager, std::string plugin);

        ~DevilImageImporter();

    private:
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL Features doFeatures() const override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL void doClose() override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_DEVILIMAGEIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_DEVILIMAGEIMPORTER_LOCAL std::optional<ImageData2D> doImage2D(UnsignedInt id) override;

        Containers::Array<unsigned char> _in;
};

}}

#endif
