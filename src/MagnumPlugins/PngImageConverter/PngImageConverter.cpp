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

#include "PngImageConverter.h"

#include <algorithm>
#include <png.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/Endianness.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

namespace Magnum { namespace Trade {

PngImageConverter::PngImageConverter() = default;

PngImageConverter::PngImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImageConverter{manager, plugin} {}

auto PngImageConverter::doFeatures() const -> Features { return Feature::ConvertData; }

Containers::Array<char> PngImageConverter::doExportToData(const ImageView2D& image) {
    CORRADE_ASSERT(std::strcmp(PNG_LIBPNG_VER_STRING, png_libpng_ver) == 0,
        "Trade::PngImporter::image2D(): libpng version mismatch, got" << png_libpng_ver << "but expected" << PNG_LIBPNG_VER_STRING, nullptr);

    #ifndef MAGNUM_TARGET_GLES
    if(image.storage().swapBytes()) {
        Error() << "Trade::PngImageConverter::exportToData(): pixel byte swap is not supported";
        return nullptr;
    }
    #endif

    Int bitDepth;
    switch(image.type()) {
        case PixelType::UnsignedByte:
            bitDepth = 8;
            break;
        case PixelType::UnsignedShort:
            bitDepth = 16;
            break;
        default:
            Error() << "Trade::PngImageConverter::exportToData(): unsupported pixel type" << image.type();
            return nullptr;
    }

    Int colorType;
    switch(image.format()) {
        #if !(defined(MAGNUM_TARGET_WEBGL) && defined(MAGNUM_TARGET_GLES2))
        case PixelFormat::Red:
        #endif
        #ifdef MAGNUM_TARGET_GLES2
        case PixelFormat::Luminance:
        #endif
            colorType = PNG_COLOR_TYPE_GRAY;
            break;
        #if !(defined(MAGNUM_TARGET_WEBGL) && defined(MAGNUM_TARGET_GLES2))
        case PixelFormat::RG:
        #endif
        #ifdef MAGNUM_TARGET_GLES2
        case PixelFormat::LuminanceAlpha:
        #endif
            colorType = PNG_COLOR_TYPE_GRAY_ALPHA;
            break;
        case PixelFormat::RGB:
            colorType = PNG_COLOR_TYPE_RGB;
            break;
        case PixelFormat::RGBA:
            colorType = PNG_COLOR_TYPE_RGBA;
            break;
        default:
            Error() << "Trade::PngImageConverter::exportToData(): unsupported pixel format" << image.format();
            return nullptr;
    }

    png_structp file = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    CORRADE_INTERNAL_ASSERT(file);
    png_infop info = png_create_info_struct(file);
    CORRADE_INTERNAL_ASSERT(info);
    std::string output;

    /* Error handling routine */
    /** @todo Get rid of setjmp (won't work everywhere) */
    if(setjmp(png_jmpbuf(file))) {
        Error() << "Trade::PngImageConverter::image2D(): error while reading PNG file";

        png_destroy_write_struct(&file, &info);
        return nullptr;
    }

    png_set_write_fn(file, &output, [](png_structp file, png_bytep data, png_size_t length){
        auto&& output = *reinterpret_cast<std::string*>(png_get_io_ptr(file));
        std::size_t oldSize = output.size();
        output.resize(output.size() + length);
        std::copy_n(data, length, &output[oldSize]);
    }, [](png_structp){});

    /* Write header */
    png_set_IHDR(file, info, image.size().x(), image.size().y(),
        bitDepth, colorType, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(file, info);

    /* Data properties */
    Math::Vector2<std::size_t> offset, dataSize;
    std::tie(offset, dataSize, std::ignore) = image.dataProperties();

    /* Write rows in reverse order, properly take data properties into account */
    if(bitDepth == 8) {
        for(Int y = 0; y != image.size().y(); ++y)
            png_write_row(file, const_cast<unsigned char*>(image.data<unsigned char>()) + offset.sum() + (image.size().y() - y - 1)*dataSize.x());

    /* For 16 bit depth we need to swap to big endian */
    } else if(bitDepth == 16) {
        const std::size_t rowSize = image.pixelSize()*image.size().x()/2;
        Containers::Array<UnsignedShort> row{rowSize};
        for(Int y = 0; y != image.size().y(); ++y) {
            std::copy_n(image.data<UnsignedShort>() + (offset.sum() + (image.size().y() - y - 1)*dataSize.x())/2, rowSize, row.data());
            for(UnsignedShort& i: row)
                Utility::Endianness::bigEndianInPlace(i);
            png_write_row(file, reinterpret_cast<unsigned char*>(row.data()));
        }
    }

    png_write_end(file, nullptr);
    png_destroy_write_struct(&file, &info);

    /* Copy the string into the output array (I would kill for having std::string::release()) */
    Containers::Array<char> fileData{output.size()};
    std::copy(output.begin(), output.end(), fileData.data());
    return fileData;
}

}}
