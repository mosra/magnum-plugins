#ifndef Magnum_Trade_MiniExrImageConverter_h
#define Magnum_Trade_MiniExrImageConverter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
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
 * @brief Class @ref Magnum::Trade::MiniExrImageConverter
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/MiniExrImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_MINIEXRIMAGECONVERTER_BUILD_STATIC
    #ifdef MiniExrImageConverter_EXPORTS
        #define MAGNUM_MINIEXRIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_MINIEXRIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_MINIEXRIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_MINIEXRIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_MINIEXRIMAGECONVERTER_EXPORT
#define MAGNUM_MINIEXRIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief OpenEXR image converter plugin using miniexr

@m_keywords{OpenExrImageConverter}

Supports images with format @ref PixelFormat::RGB16F or
@ref PixelFormat::RGBA16F using the [miniexr](https://github.com/aras-p/miniexr)
library.

This plugin depends on the @ref Trade library and is built if
`WITH_MINIEXRIMAGECONVERTER` is enabled when building Magnum Plugins. To use as
a dynamic plugin, you need to load the @cpp "MiniExrImageConverter" @ce plugin
from `MAGNUM_PLUGINS_IMAGECONVERTER_DIR`. To use as a static plugin or as a
dependency of another plugin with CMake, you need to request the
`MiniExrImageConverter` component of the `MagnumPlugins` package and link to
the `MagnumPlugins::MiniExrImageConverter` target. See @ref building-plugins,
@ref cmake-plugins and @ref plugins for more information.

This plugins provides `OpenExrImageConverter` plugin, but note that this plugin
generates only uncompressed files and the performance might be worse than when
using plugin dedicated for given format.

@m_class{m-block m-primary}

@thirdparty This plugin makes use of the
    [miniexr](https://github.com/aras-p/miniexr) library by Aras Pranckevičius, released into the @m_class{m-label m-primary} **public domain**
    ([choosealicense.com](https://choosealicense.com/licenses/unlicense/)).
*/
class MAGNUM_MINIEXRIMAGECONVERTER_EXPORT MiniExrImageConverter: public AbstractImageConverter {
    public:
        /** @brief Default constructor */
        explicit MiniExrImageConverter();

        /** @brief Plugin manager constructor */
        explicit MiniExrImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_MINIEXRIMAGECONVERTER_LOCAL Features doFeatures() const override;
        MAGNUM_MINIEXRIMAGECONVERTER_LOCAL Containers::Array<char> doExportToData(const ImageView2D& image) override;
};

}}

#endif
