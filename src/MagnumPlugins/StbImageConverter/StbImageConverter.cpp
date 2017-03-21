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

#include "StbImageConverter.h"

#include <algorithm>
#include <Corrade/Containers/Array.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ASSERT CORRADE_INTERNAL_ASSERT
/* Not defining malloc/free, because there's no equivalent for realloc in C++ */
#include "stb_image_write.h"

namespace Magnum { namespace Trade {

StbImageConverter::StbImageConverter(Format format): _format{format} {
    /* Passing an invalid Format enum is user error, we'll assert on that in
       the exportToData() function */
}

StbImageConverter::StbImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImageConverter{manager, plugin} {
    if(plugin == "StbBmpImageConverter" || plugin == "BmpImageConverter")
        _format = Format::Bmp;
    else if(plugin == "StbHdrImageConverter" || plugin == "HdrImageConverter")
        _format = Format::Hdr;
    else if(plugin == "StbPngImageConverter" || plugin == "PngImageConverter")
        _format = Format::Png;
    else if(plugin == "StbTgaImageConverter" || plugin == "TgaImageConverter")
        _format = Format::Tga;
    else
        _format = {}; /* Runtime error in doExportToData() */
}

auto StbImageConverter::doFeatures() const -> Features { return Feature::ConvertData; }

Containers::Array<char> StbImageConverter::doExportToData(const ImageView2D& image) {
    if(!Int(_format)) {
        Error() << "Trade::StbImageConverter::exportToData(): cannot determine output format (plugin loaded as" << plugin() << Error::nospace << ")";
        return nullptr;
    }

    #ifndef MAGNUM_TARGET_GLES
    if(image.storage().swapBytes()) {
        Error() << "Trade::StbImageConverter::exportToData(): pixel byte swap is not supported";
        return nullptr;
    }
    #endif

    if((_format == Format::Bmp || _format == Format::Png || _format == Format::Tga) &&
        image.type() != PixelType::UnsignedByte)
    {
        Error() << "Trade::StbImageConverter::exportToData():" << image.type() << "is not supported for BMP/PNG/TGA format";
        return nullptr;
    }

    if(_format == Format::Hdr && image.type() != PixelType::Float) {
        Error() << "Trade::StbImageConverter::exportToData():" << image.type() << "is not supported for HDR format";
        return nullptr;
    }

    Int components;
    switch(image.format()) {
        #if !(defined(MAGNUM_TARGET_WEBGL) && defined(MAGNUM_TARGET_GLES2))
        case PixelFormat::Red:
        #endif
        #ifdef MAGNUM_TARGET_GLES2
        case PixelFormat::Luminance:
        #endif
            components = 1;
            break;
        #if !(defined(MAGNUM_TARGET_WEBGL) && defined(MAGNUM_TARGET_GLES2))
        case PixelFormat::RG:
        #endif
        #ifdef MAGNUM_TARGET_GLES2
        case PixelFormat::LuminanceAlpha:
        #endif
            components = 2;
            break;
        case PixelFormat::RGB: components = 3; break;
        case PixelFormat::RGBA: components = 4; break;
        default:
            Error() << "Trade::StbImageConverter::exportToData(): unsupported pixel format" << image.format();
            return nullptr;
    }

    /* Data properties */
    Math::Vector2<std::size_t> offset, dataSize;
    std::tie(offset, dataSize, std::ignore) = image.dataProperties();

    if(_format != Format::Png && dataSize.x() != image.pixelSize()*image.size().x()) {
        Error() << "Trade::StbImageConverter::exportToData(): data must be tightly packed for all formats except PNG";
        return nullptr;
    }

    /* Reverse rows in image data */
    Containers::Array<unsigned char> reversedData{image.data().size()};
    for(Int y = 0; y != image.size().y(); ++y) {
        std::copy(image.data<unsigned char>() + offset.sum() + y*dataSize.x(), image.data<unsigned char>() + offset.sum() + (y + 1)*dataSize.x(), reversedData + (image.size().y() - y - 1)*dataSize.x());
    }

    std::string data;
    const auto writeFunc = [](void* context, void* data, int size) {
        static_cast<std::string*>(context)->append(static_cast<char*>(data), size);
    };

    if(_format == Format::Bmp) {
        if(!stbi_write_bmp_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reversedData)) {
            Error() << "Trade::StbImageConverter::exportToData(): error while writing BMP file";
            return nullptr;
        }
    } else if(_format == Format::Hdr) {
        if(!stbi_write_hdr_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reinterpret_cast<float*>(reversedData.begin()))) {
            Error() << "Trade::StbImageConverter::exportToData(): error while writing HDR file";
            return nullptr;
        }
    } else if(_format == Format::Png) {
        if(!stbi_write_png_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reversedData, dataSize.x())) {
            Error() << "Trade::StbImageConverter::exportToData(): error while writing PNG file";
            return nullptr;
        }
    } else if(_format == Format::Tga) {
        if(!stbi_write_tga_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reversedData)) {
            Error() << "Trade::StbImageConverter::exportToData(): error while writing TGA file";
            return nullptr;
        }
    } else {
        CORRADE_ASSERT(false, "Trade::StbImageConverter::exportToData(): invalid format" << Int(_format), nullptr);
    }

    /* Copy the data into array (I would *love* to have a detach() function on
       std::string) */
    Containers::Array<char> fileData{data.size()};
    std::copy(data.begin(), data.end(), fileData.begin());
    return fileData;
}

}}
