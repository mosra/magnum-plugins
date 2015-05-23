#ifndef Magnum_Trade_StbPngImageConverter_h
#define Magnum_Trade_StbPngImageConverter_h
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
 * @brief Class @ref Magnum::Trade::StbPngImageConverter
 */

#include "Magnum/Trade/AbstractImageConverter.h"

namespace Magnum { namespace Trade {

/**
@brief PNG image converter plugin using stb_image_write

Supports images with format @ref ColorFormat::Red, @ref ColorFormat::RG,
@ref ColorFormat::RGB or @ref ColorFormat::RGBA and type
@ref ColorType::UnsignedByte. On OpenGL ES 2.0 and WebGL 1.0 accepts also
@ref ColorFormat::Luminance instead of @ref ColorFormat::Red and
@ref ColorFormat::LuminanceAlpha instaed of @ref ColorFormat::RG.

This plugin is built if `WITH_STBPNGIMAGECONVERTER` is enabled when building
Magnum Plugins. To use dynamic plugin, you need to load `StbPngImageConverter`
plugin from `MAGNUM_PLUGINS_IMPORTER_DIR`. To use static plugin, you need to
request `StbPngImageConverter` component of `MagnumPlugins` package in CMake
and link to `${MAGNUMPLUGINS_STBPNGIMAGECONVERTER_LIBRARIES}`. To use this as a
dependency of another plugin, you additionally need to add
`${MAGNUMPLUGINS_STBPNGIMAGECONVERTER_INCLUDE_DIRS}` to include path.

This plugins provides `PngImageConverter` plugin, but note that this plugin
may generate slightly larger files and the performance might be worse than when
using plugin dedicated for given format.

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class StbPngImageConverter: public AbstractImageConverter {
    public:
        /** @brief Default constructor */
        explicit StbPngImageConverter();

        /** @brief Plugin manager constructor */
        explicit StbPngImageConverter(PluginManager::AbstractManager& manager, std::string plugin);

    private:
        Features doFeatures() const override;
        Containers::Array<char> doExportToData(const ImageReference2D& image) const override;
};

}}

#endif
