/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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

#include <cstring>
#include <algorithm>
#include <png.h>
/*
    The <csetjmp> header has to be included *after* png.h, otherwise older
    versions of libpng (i.e., one used on Travis 16.04 images), complain that

        __pngconf.h__ in libpng already includes setjmp.h
        __dont__ include it again.

    New versions don't have that anymore: https://github.com/glennrp/libpng/commit/6c2e919c7eb736d230581a4c925fa67bd901fcf8
*/
#include <csetjmp>
#include <Corrade/Containers/Array.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

namespace Magnum { namespace Trade {

PngImageConverter::PngImageConverter() = default;

PngImageConverter::PngImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures PngImageConverter::doFeatures() const { return ImageConverterFeature::Convert2DToData; }

Containers::Array<char> PngImageConverter::doConvertToData(const ImageView2D& image) {
    CORRADE_ASSERT(std::strcmp(PNG_LIBPNG_VER_STRING, png_libpng_ver) == 0,
        "Trade::PngImageConverter::convertToData(): libpng version mismatch, got" << png_libpng_ver << "but expected" << PNG_LIBPNG_VER_STRING, nullptr);

    Int bitDepth;
    Int colorType;
    switch(image.format()) {
        case PixelFormat::R8Unorm:
            bitDepth = 8;
            colorType = PNG_COLOR_TYPE_GRAY;
            break;
        case PixelFormat::R16Unorm:
            bitDepth = 16;
            colorType = PNG_COLOR_TYPE_GRAY;
            break;
        case PixelFormat::RG8Unorm:
            bitDepth = 8;
            colorType = PNG_COLOR_TYPE_GRAY_ALPHA;
            break;
        case PixelFormat::RG16Unorm:
            bitDepth = 16;
            colorType = PNG_COLOR_TYPE_GRAY_ALPHA;
            break;
        case PixelFormat::RGB8Unorm:
            bitDepth = 8;
            colorType = PNG_COLOR_TYPE_RGB;
            break;
        case PixelFormat::RGB16Unorm:
            bitDepth = 16;
            colorType = PNG_COLOR_TYPE_RGB;
            break;
        case PixelFormat::RGBA8Unorm:
            bitDepth = 8;
            colorType = PNG_COLOR_TYPE_RGBA;
            break;
        case PixelFormat::RGBA16Unorm:
            bitDepth = 16;
            colorType = PNG_COLOR_TYPE_RGBA;
            break;
        default:
            Error() << "Trade::PngImageConverter::convertToData(): unsupported pixel format" << image.format();
            return nullptr;
    }

    png_structp file = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    CORRADE_INTERNAL_ASSERT(file);
    png_infop info = png_create_info_struct(file);
    CORRADE_INTERNAL_ASSERT(info);
    std::string output;

    /* Error handling routine. Since we're replacing the png_default_error()
       function, we need to call std::longjmp() ourselves -- otherwise the
       default error handling with stderr printing kicks in. */
    if(setjmp(png_jmpbuf(file))) {
        png_destroy_write_struct(&file, &info);
        return nullptr;
    }
    png_set_error_fn(file, nullptr, [](const png_structp file, const png_const_charp message) {
        Error{} << "Trade::PngImageConverter::convertToData(): error:" << message;
        std::longjmp(png_jmpbuf(file), 1);
    }, [](png_structp, const png_const_charp message) {
        Warning{} << "Trade::PngImageConverter::convertToData(): warning:" << message;
    });

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

    /* Get data properties and calculate the initial slice based on subimage
       offset */
    const std::pair<Math::Vector2<std::size_t>, Math::Vector2<std::size_t>> dataProperties = image.dataProperties();
    auto data = Containers::arrayCast<const unsigned char>(image.data())
        .suffix(dataProperties.first.sum());

    /* Write rows in reverse order, properly take stride into account */
    if(bitDepth == 8) {
        for(Int y = 0; y != image.size().y(); ++y)
            png_write_row(file, const_cast<unsigned char*>(data.suffix((image.size().y() - y - 1)*dataProperties.second.x()).data()));

    /* For 16 bit depth we need to swap to big endian */
    } else if(bitDepth == 16) {
        #ifndef CORRADE_TARGET_BIG_ENDIAN
        png_set_swap(file);
        #endif
        for(Int y = 0; y != image.size().y(); ++y) {
            png_write_row(file, const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data.suffix((image.size().y() - y - 1)*dataProperties.second.x()).data())));
        }
    }

    png_write_end(file, nullptr);
    png_destroy_write_struct(&file, &info);

    /* Copy the string into the output array (I would kill for having std::string::release()) */
    Containers::Array<char> fileData{NoInit, output.size()};
    std::copy(output.begin(), output.end(), fileData.data());
    return fileData;
}

}}

CORRADE_PLUGIN_REGISTER(PngImageConverter, Magnum::Trade::PngImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3")
