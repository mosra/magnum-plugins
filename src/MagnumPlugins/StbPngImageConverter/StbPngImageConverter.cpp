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

#include "StbPngImageConverter.h"

#include <Corrade/Containers/Array.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ASSERT CORRADE_INTERNAL_ASSERT
/* Not defining malloc/free, because there's no equivalent for realloc in C++ */
#include "stb_image_write.h"

namespace Magnum { namespace Trade {

StbPngImageConverter::StbPngImageConverter() = default;

StbPngImageConverter::StbPngImageConverter(PluginManager::AbstractManager& manager, std::string plugin): AbstractImageConverter(manager, std::move(plugin)) {}

auto StbPngImageConverter::doFeatures() const -> Features { return Feature::ConvertData; }

Containers::Array<char> StbPngImageConverter::doExportToData(const ImageView2D& image) {
    #ifndef MAGNUM_TARGET_GLES
    if(image.storage().swapBytes()) {
        Error() << "Trade::StbPngImageConverter::exportToData(): pixel byte swap is not supported";
        return nullptr;
    }
    #endif

    if(image.type() != PixelType::UnsignedByte) {
        Error() << "Trade::StbPngImageConverter::exportToData(): unsupported pixel type" << image.type();
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
            Error() << "Trade::StbPngImageConverter::exportToData(): unsupported pixel format" << image.format();
            return nullptr;
    }

    /* Data properties */
    std::size_t offset;
    Math::Vector2<std::size_t> dataSize;
    std::tie(offset, dataSize, std::ignore) = image.dataProperties();

    /* Reverse rows in image data */
    Containers::Array<unsigned char> reversedData{image.data().size()};
    for(Int y = 0; y != image.size().y(); ++y) {
        std::copy(image.data<unsigned char>() + offset + y*dataSize.x(), image.data<unsigned char>() + offset + (y + 1)*dataSize.x(), reversedData + (image.size().y() - y - 1)*dataSize.x());
    }

    Int size;
    unsigned char* const data = stbi_write_png_to_mem(reversedData, dataSize.x(), image.size().x(), image.size().y(), components, &size);
    CORRADE_INTERNAL_ASSERT(data);

    /* Wrap the data in an array with custom deleter (we can't use delete[]) */
    Containers::Array<char> fileData{reinterpret_cast<char*>(data), std::size_t(size),
        [](char* data, std::size_t) { std::free(data); }};

    return fileData;
}

}}
