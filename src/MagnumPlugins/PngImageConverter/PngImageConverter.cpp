/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023 Vladimír Vondruš <mosra@centrum.cz>

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

#include <png.h>
/*
    The <csetjmp> header has to be included *after* png.h, otherwise older
    versions of libpng (i.e., one used on Travis 16.04 images), complain that

        __pngconf.h__ in libpng already includes setjmp.h
        __dont__ include it again.

    New versions don't have that anymore: https://github.com/glennrp/libpng/commit/6c2e919c7eb736d230581a4c925fa67bd901fcf8
*/
#include <csetjmp>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/String.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

PngImageConverter::PngImageConverter() = default;

PngImageConverter::PngImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures PngImageConverter::doFeatures() const { return ImageConverterFeature::Convert2DToData; }

Containers::String PngImageConverter::doExtension() const { return "png"_s; }

Containers::String PngImageConverter::doMimeType() const {
    return "image/png"_s;
}

Containers::Optional<Containers::Array<char>> PngImageConverter::doConvertToData(const ImageView2D& image) {
    /* Warn about lost metadata */
    if((image.flags() & ImageFlag2D::Array) && !(flags() & ImageConverterFlag::Quiet)) {
        Warning{} << "Trade::PngImageConverter::convertToData(): 1D array images are unrepresentable in PNG, saving as a regular 2D image";
    }

    Int bitDepth;
    Int colorType;
    switch(image.format()) {
        /* LCOV_EXCL_START */
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
        /* LCOV_EXCL_STOP */
        default:
            Error() << "Trade::PngImageConverter::convertToData(): unsupported pixel format" << image.format();
            return {};
    }

    png_structp file = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    /** @todo this will assert if the PNG major/minor version doesn't match,
        with "libpng warning: Application built with libpng-1.7.0 but running
        with 1.6.38" being printed to stdout, the proper fix is to set error
        callbacks directly in the png_create_write_struct() call */
    CORRADE_INTERNAL_ASSERT(file);
    png_infop info = png_create_info_struct(file);
    CORRADE_INTERNAL_ASSERT(info);
    Containers::Array<char> output;

    /* Error handling routine. Since we're replacing the png_default_error()
       function, we need to call std::longjmp() ourselves -- otherwise the
       default error handling with stderr printing kicks in. */
    if(setjmp(png_jmpbuf(file))) {
        png_destroy_write_struct(&file, &info);
        return {};
    }
    png_set_error_fn(file, nullptr, [](const png_structp file, const png_const_charp message) {
        Error{} << "Trade::PngImageConverter::convertToData(): error:" << message;
        std::longjmp(png_jmpbuf(file), 1);
    }, flags() & ImageConverterFlag::Quiet ?
        /* MSVC w/o /permissive- "cannot convert <lambda> to <lambda>", ffs.
           Casting just one of the two is enough. */
        #ifndef CORRADE_MSVC_COMPATIBILITY
        [](png_structp, png_const_charp) {}
        #else
        static_cast<void(*)(png_structp, png_const_charp)>([](png_structp, png_const_charp) {})
        #endif
        : [](png_structp, const png_const_charp message) {
            Warning{} << "Trade::PngImageConverter::convertToData(): warning:" << message;
        }
    );

    png_set_write_fn(file, &output, [](png_structp file, png_bytep data, png_size_t length){
        auto& output = *reinterpret_cast<Containers::Array<char>*>(png_get_io_ptr(file));
        arrayAppend(output, {reinterpret_cast<const char*>(data), length});
    }, [](png_structp){});

    /* Write header */
    png_set_IHDR(file, info, image.size().x(), image.size().y(),
        bitDepth, colorType, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(file, info);

    /* For 16 bit depth we need to swap to big endian */
    if(bitDepth == 16) {
        #ifndef CORRADE_TARGET_BIG_ENDIAN
        png_set_swap(file);
        #endif
    } else CORRADE_INTERNAL_ASSERT(bitDepth == 8);

    /* Write rows in reverse order. While the rows may have some padding after,
       the actual pixels in the row should be contiguous so it should be safe
       to pass a pointer to the first byte of each. */
    const Containers::StridedArrayView3D<const char> pixelsFlipped = image.pixels().flipped<0>();
    CORRADE_INTERNAL_ASSERT(pixelsFlipped.isContiguous<1>());
    for(Int y = 0; y != image.size().y(); ++y)
        png_write_row(file, static_cast<unsigned char*>(const_cast<void*>(pixelsFlipped[y].data())));

    png_write_end(file, nullptr);
    png_destroy_write_struct(&file, &info);

    /* Convert the growable array back to a non-growable with the default
       deleter so we can return it */
    arrayShrink(output);

    /* GCC 4.8 needs extra help here */
    return Containers::optional(std::move(output));
}

}}

CORRADE_PLUGIN_REGISTER(PngImageConverter, Magnum::Trade::PngImageConverter,
    MAGNUM_TRADE_ABSTRACTIMAGECONVERTER_PLUGIN_INTERFACE)
