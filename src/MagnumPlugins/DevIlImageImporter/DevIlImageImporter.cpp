/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2016 Alice Margatroid <loveoverwhelming@gmail.com>

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

#include "DevIlImageImporter.h"

#include <algorithm>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

#include <IL/il.h>
#include <IL/ilu.h>

#ifdef MAGNUM_TARGET_GLES2
#include <Magnum/Context.h>
#include <Magnum/Extensions.h>
#endif

namespace Magnum { namespace Trade {

DevIlImageImporter::DevIlImageImporter() = default;

DevIlImageImporter::DevIlImageImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

DevIlImageImporter::~DevIlImageImporter() { close(); }

auto DevIlImageImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool DevIlImageImporter::doIsOpened() const { return _in; }

void DevIlImageImporter::doClose() { _in = nullptr; }

void DevIlImageImporter::doOpenData(const Containers::ArrayView<const char> data) {
    _in = Containers::Array<unsigned char>(data.size());
    std::copy(data.begin(), data.end(), _in.begin());
}

UnsignedInt DevIlImageImporter::doImage2DCount() const { return 1; }

std::optional<ImageData2D> DevIlImageImporter::doImage2D(UnsignedInt) {
    ILuint imgID = 0;
    ilGenImages(1, &imgID);
    ilBindImage(imgID);

    ILboolean success = ilLoadL(IL_TYPE_UNKNOWN, _in, _in.size());
    if(success == IL_FALSE) {
        Error() << "Trade::DevIlImageImporter::image2D(): cannot open the image:" << ilGetError();
        return std::nullopt;
    }

    ILubyte *data = ilGetData();

    Vector2i size;
    size.x() = ilGetInteger(IL_IMAGE_WIDTH);
    size.y() = ilGetInteger(IL_IMAGE_HEIGHT);

    ILint ilFormat = ilGetInteger(IL_IMAGE_FORMAT);
    int components = 0;

    bool rgba_needed = false;
    PixelFormat format;
    switch(ilFormat) {
        /* Grayscale */
        case IL_LUMINANCE:
            #ifndef MAGNUM_TARGET_GLES2
            format = PixelFormat::Red;
            #elif !defined(MAGNUM_TARGET_WEBGL)
            format = Context::hasCurrent() && Context::current().isExtensionSupported<Extensions::GL::EXT::texture_rg>() ?
                PixelFormat::Red : PixelFormat::Luminance;
            #else
            format = PixelFormat::Luminance;
            #endif

            components = 1;
            break;

        /* Grayscale + alpha */
        case IL_LUMINANCE_ALPHA:
            #ifndef MAGNUM_TARGET_GLES2
            format = PixelFormat::RG;
            #elif !defined(MAGNUM_TARGET_WEBGL)
            format = Context::hasCurrent() && Context::current().isExtensionSupported<Extensions::GL::EXT::texture_rg>() ?
                PixelFormat::RG : PixelFormat::LuminanceAlpha;
            #else
            format = PixelFormat::LuminanceAlpha;
            #endif

            components = 2;
            break;

        /* BGR */
        case IL_BGR:
            #ifndef MAGNUM_TARGET_GLES
            format = PixelFormat::BGR;
            #else
            rgba_needed = true;
            #endif

            components = 3;
            break;

        /* BGRA */
        case IL_BGRA:
            #ifndef MAGNUM_TARGET_GLES
            format = PixelFormat::BGRA;
            #else
            rgba_needed = true;
            #endif

            components = 4;
            break;

        /* RGB */
        case IL_RGB:
            format = PixelFormat::RGB;

            components = 3;
            break;

        /* RGBA */
        case IL_RGBA:
            format = PixelFormat::RGBA;

            components = 4;
            break;

        /* Convert to RGBA */
        default: rgba_needed = true;
    }

    /* If the format isn't one we recognize, convert to RGBA */
    if(rgba_needed) {
        success = ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);
        if(success == IL_FALSE) {
            Error() << "Trade::DevIlImageImporter::image2D(): cannot convert image: " << ilGetError();
            return std::nullopt;
        }

        format = PixelFormat::RGBA;
        components = 4;
    }

    /* Flip the image to match OpenGL's conventions */
    /** @todo use our own routine to avoid linking to ILU */
    ILinfo ImageInfo;
    iluGetImageInfo(&ImageInfo);
    if(ImageInfo.Origin == IL_ORIGIN_UPPER_LEFT)
        iluFlipImage();

    /* Copy the data into array that is owned by us and not by IL */
    Containers::Array<char> imageData{std::size_t(size.product()*components)};
    std::copy_n(reinterpret_cast<char*>(data), imageData.size(), imageData.begin());

    /* Release the texture back to DevIL */
    ilDeleteImages(1, &imgID);

    /* Adjust pixel storage if row size is not four byte aligned */
    PixelStorage storage;
    if((size.x()*components)%4 != 0)
        storage.setAlignment(1);

    return Trade::ImageData2D{storage, format, PixelType::UnsignedByte, size, std::move(imageData)};
}

}}
