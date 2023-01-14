/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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
#include <algorithm> /* std::copy() */ /** @todo remove */
#include <string> /** @todo replace with a growable array */
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
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ScopeGuard.h>
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
    if(image.flags() & ImageFlag2D::Array) {
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

    /* Structures for writing the file. To avoid leaks, everything that needs
       to be destructed when exiting the function has to be defined *before*
       the setjmp() call below. */
    struct PngState {
        png_structp file;
        png_infop info;
    } png{};
    /** @todo a capturing ScopeGuard would be nicer :( */
    Containers::ScopeGuard pngStateGuard{&png, [](PngState* state) {
        /* Although undocumented, this function gracefully handles the case
           when the file/info is null, no need to check that explicitly. */
        png_destroy_write_struct(&state->file, &state->info);
    }};
    std::string output;
    /* For exiting out of error callbacks via longjmp. Can't use the builtin
       png_jmpbuf() because the errors can happen inside
       png_create_read_struct(), thus even before the png.file gets filled, and
       thus even before we can call setjmp(png_jmpbuf(png.file)). Yet the
       official documentation says absolutely nothing about handling such case,
       F.F.S. */
    std::jmp_buf jmpbuf;
    /* We may arrive here from the longjmp from the error callback */
    if(setjmp(jmpbuf))
        return {};

    /* Create the write structure. Directly set up also error/warning
       callbacks because if done here and not in a subsequent
       png_set_error_fn() call, it'll gracefully handle also libpng version
       mismatch errors. */
    png.file = png_create_write_struct(PNG_LIBPNG_VER_STRING, &jmpbuf, [](const png_structp file, const png_const_charp message) {
        Error{} << "Trade::PngImageConverter::convertToData(): error:" << message;
        /* Since we're replacing the png_default_error() function, we need to
           call std::longjmp() ourselves -- otherwise the default error
           handling with stderr printing kicks in. See above for why
           png_jmpbuf() can't be used. */
        std::longjmp(*static_cast<std::jmp_buf*>(png_get_error_ptr(file)), 1);
    }, [](const png_structp file, const png_const_charp message) {
        /* If there's a mismatch in the passed PNG_LIBPNG_VER_STRING major or
           minor version (but not patch version), png_create_read_struct()
           returns a nullptr. However, such a *fatal* error is treated as a
           warning by the library, directed to the warning callback:
             https://github.com/glennrp/libpng/blob/07b8803110da160b158ebfef872627da6c85cbdf/png.c#L219-L240
           That may cause great confusion ("why convertToData() returns a
           NullOpt if it was just a warning??"), so I'm detecting that,
           annotating it as an error instead, and jumping to the error return
           so we don't end up with a nullptr `png.file` below.

           Unfortunately this case is annoyingly hard to test automatically, so
           the following branch is uncovered. */
        if(Containers::StringView{message}.hasPrefix("Application built with libpng-"_s)) {
            Error{} << "Trade::PngImageConverter::convertToData(): error:" << message;
            std::longjmp(*static_cast<std::jmp_buf*>(png_get_error_ptr(file)), 1);
        } else
            Warning{} << "Trade::PngImageConverter::convertToData(): warning:" << message;
    });
    /* If png_create_write_struct() failed, png.file is a nullptr. Apart from
       that we may arrive here from the longjmp from the error callback. */
    if(!png.file || setjmp(png_jmpbuf(png.file)))
        return {};

    png.info = png_create_info_struct(png.file);
    CORRADE_INTERNAL_ASSERT(png.info);

    /* Set functions for writing */
    png_set_write_fn(png.file, &output, [](png_structp file, png_bytep data, png_size_t length){
        auto&& output = *reinterpret_cast<std::string*>(png_get_io_ptr(file));
        std::size_t oldSize = output.size();
        output.resize(output.size() + length);
        std::copy_n(data, length, &output[oldSize]);
    }, [](png_structp){});

    /* Write header */
    png_set_IHDR(png.file, png.info, image.size().x(), image.size().y(),
        bitDepth, colorType, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png.file, png.info);

    /* Get data properties and calculate the initial slice based on subimage
       offset */
    const std::pair<Math::Vector2<std::size_t>, Math::Vector2<std::size_t>> dataProperties = image.dataProperties();
    auto data = Containers::arrayCast<const unsigned char>(image.data())
        .exceptPrefix(dataProperties.first.sum());

    /* Write rows in reverse order, properly take stride into account */
    if(bitDepth == 8) {
        for(Int y = 0; y != image.size().y(); ++y)
            png_write_row(png.file, const_cast<unsigned char*>(data.exceptPrefix((image.size().y() - y - 1)*dataProperties.second.x()).data()));

    /* For 16 bit depth we need to swap to big endian */
    } else if(bitDepth == 16) {
        #ifndef CORRADE_TARGET_BIG_ENDIAN
        png_set_swap(png.file);
        #endif
        for(Int y = 0; y != image.size().y(); ++y) {
            png_write_row(png.file, const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data.exceptPrefix((image.size().y() - y - 1)*dataProperties.second.x()).data())));
        }
    }

    png_write_end(png.file, nullptr);

    /* Copy the string into the output array (I would kill for having std::string::release()) */
    Containers::Array<char> fileData{NoInit, output.size()};
    std::copy(output.begin(), output.end(), fileData.data());

    /* GCC 4.8 needs extra help here */
    return Containers::optional(std::move(fileData));
}

}}

CORRADE_PLUGIN_REGISTER(PngImageConverter, Magnum::Trade::PngImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3.3")
