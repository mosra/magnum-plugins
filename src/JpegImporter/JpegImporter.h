#ifndef Magnum_Trade_JpegImporter_h
#define Magnum_Trade_JpegImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013 Vladimír Vondruš <mosra@centrum.cz>

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
 * @brief Class Magnum::Trade::JpegImporter
 */

#include <Trade/AbstractImporter.h>

#ifndef DOXYGEN_GENERATING_OUTPUT
    #if defined(JpegImporter_EXPORTS) || defined(JpegImporterObjects_EXPORTS)
        #define MAGNUM_JPEGIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_JPEGIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
    #define MAGNUM_JPEGIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief JPEG importer

Supports RGB or grayscale images with 8 bits per channel.

The images are imported with @ref ImageType "ImageType::UnsignedByte" and
@ref ImageFormat "ImageFormat::RGB" or @ref ImageFormat "ImageFormat::Red"
respectively. Grayscale images require extension @extension{ARB,texture_rg}.

In OpenGL ES 2.0, if @es_extension{EXT,texture_rg} is not supported, grayscale
images use @ref ImageFormat "ImageFormat::Luminance" instead of
@ref ImageFormat "ImageFormat::Red".
*/
class MAGNUM_JPEGIMPORTER_EXPORT JpegImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit JpegImporter();

        /** @brief Plugin manager constructor */
        explicit JpegImporter(PluginManager::AbstractManager* manager, std::string plugin);

        ~JpegImporter();

    private:
        MAGNUM_JPEGIMPORTER_LOCAL Features doFeatures() const override;
        MAGNUM_JPEGIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_JPEGIMPORTER_LOCAL void doClose() override;
        MAGNUM_JPEGIMPORTER_LOCAL void doOpenData(Containers::ArrayReference<const unsigned char> data) override;

        MAGNUM_JPEGIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_JPEGIMPORTER_LOCAL ImageData2D* doImage2D(UnsignedInt id) override;

    private:
        Containers::Array<unsigned char>* _in;
};

}}

#endif
