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

#include "PngImporter.h"

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
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Trade/ImageData.h>

namespace Magnum { namespace Trade {

PngImporter::PngImporter() = default;

PngImporter::PngImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

PngImporter::~PngImporter() = default;

ImporterFeatures PngImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool PngImporter::doIsOpened() const { return _in; }

void PngImporter::doClose() { _in = nullptr; }

void PngImporter::doOpenData(const Containers::ArrayView<const char> data) {
    /* Because here we're copying the data and using the _in to check if file
       is opened, having them nullptr would mean openData() would fail without
       any error message. It's not possible to do this check on the importer
       side, because empty file is valid in some formats (OBJ or glTF). We also
       can't do the full import here because then doImage2D() would need to
       copy the imported data instead anyway (and the uncompressed size is much
       larger). This way it'll also work nicely with a future openMemory(). */
    if(data.empty()) {
        Error{} << "Trade::PngImporter::openData(): the file is empty";
        return;
    }

    _in = Containers::Array<unsigned char>(data.size());
    std::copy(data.begin(), data.end(), _in.begin());
}

UnsignedInt PngImporter::doImage2DCount() const { return 1; }

Containers::Optional<ImageData2D> PngImporter::doImage2D(UnsignedInt, UnsignedInt) {
    CORRADE_ASSERT(std::strcmp(PNG_LIBPNG_VER_STRING, png_libpng_ver) == 0,
        "Trade::PngImporter::image2D(): libpng version mismatch, got" << png_libpng_ver << "but expected" << PNG_LIBPNG_VER_STRING, Containers::NullOpt);

    /* Verify file signature */
    if(png_sig_cmp(_in, 0, Math::min(std::size_t(8), _in.size())) != 0) {
        Error() << "Trade::PngImporter::image2D(): wrong file signature";
        return Containers::NullOpt;
    }

    /* Structures for reading the file */
    png_structp file = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    CORRADE_INTERNAL_ASSERT(file);
    png_infop info = png_create_info_struct(file);
    CORRADE_INTERNAL_ASSERT(info);
    /** @todo a capturing ScopeGuard would be nicer :( */
    struct PngState {
        png_structp file;
        png_infop info;
    } pngState{file, info};
    Containers::ScopeGuard pngStateGuard{&pngState, [](PngState* state) {
        png_destroy_read_struct(&state->file, &state->info, nullptr);
    }};
    Containers::Array<png_bytep> rows;
    Containers::Array<char> data;

    /* Error handling routine. Since we're replacing the png_default_error()
       function, we need to call std::longjmp() ourselves -- otherwise the
       default error handling with stderr printing kicks in. */
    if(setjmp(png_jmpbuf(file))) return Containers::NullOpt;
    png_set_error_fn(file, nullptr, [](const png_structp file, const png_const_charp message) {
        Error{} << "Trade::PngImporter::image2D(): error:" << message;
        std::longjmp(png_jmpbuf(file), 1);
    }, [](png_structp, const png_const_charp message) {
        Warning{} << "Trade::PngImporter::image2D(): warning:" << message;
    });

    /* Input starts right after the header */
    if(_in.size() < 8) {
        Error{} << "Trade::PngImporter::image2D(): signature too short";
        return Containers::NullOpt;
    }
    Containers::ArrayView<unsigned char> input = _in.suffix(8);

    /* Set functions for reading */
    png_set_read_fn(file, &input, [](const png_structp file, const png_bytep data, const png_size_t length) {
        auto&& input = *reinterpret_cast<Containers::ArrayView<unsigned char>*>(png_get_io_ptr(file));
        if(input.size() < length) png_error(file, "file too short");
        std::copy_n(input.begin(), length, data);
        input = input.suffix(length);
    });

    /* The signature is already read */
    png_set_sig_bytes(file, 8);

    /* Read file information */
    png_read_info(file, info);

    /* Image size */
    const Vector2i size(png_get_image_width(file, info), png_get_image_height(file, info));

    /* Image channels and bit depth */
    png_uint_32 bits = png_get_bit_depth(file, info);
    png_uint_32 channels = png_get_channels(file, info);
    png_uint_32 colorType = png_get_color_type(file, info);

    /* Check image format, convert if necessary */
    switch(colorType) {
        /* Types that can be used without conversion */
        case PNG_COLOR_TYPE_GRAY:
            CORRADE_INTERNAL_ASSERT(channels == 1);

            /* Convert to 8-bit */
            if(bits < 8) {
                png_set_expand_gray_1_2_4_to_8(file);
                bits = 8;
            }

            break;

        case PNG_COLOR_TYPE_GRAY_ALPHA:
            CORRADE_INTERNAL_ASSERT(channels == 2);
            break;

        case PNG_COLOR_TYPE_RGB:
            CORRADE_INTERNAL_ASSERT(channels == 3);
            break;

        case PNG_COLOR_TYPE_RGBA:
            CORRADE_INTERNAL_ASSERT(channels == 4);
            break;

        /* Palette needs to be converted */
        case PNG_COLOR_TYPE_PALETTE:
            png_set_palette_to_rgb(file);
            /* png_get_bit_depth(file, info); would return the original value
               here (which can be < 8), expecting the png_set_*() function to
               give back 8-bit channels */
            bits = 8;
            colorType = PNG_COLOR_TYPE_RGB;
            channels = 3;
            break;

        /* LCOV_EXCL_START */
        /* We have covered all cases above, but just in case this happens,
           provide a clear message */
        default:
            CORRADE_ASSERT_UNREACHABLE("Trade::PngImporter::image2D(): unsupported color type" << colorType, );
        /* LCOV_EXCL_STOP */
    }

    /* Convert transparency mask to alpha */
    if(png_get_valid(file, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(file);
        channels += 1;
        if(channels == 2)
            colorType = PNG_COLOR_TYPE_GRAY_ALPHA;
        else if(channels == 4)
            colorType = PNG_COLOR_TYPE_RGBA;
        else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        /* png_get_bit_depth(file, info); would return the original value
           here (which can be < 8), expecting the png_set_*() function to give
           back 8-bit channels */
        bits = 8;
    }

    /* Initialize data array, align rows to four bytes */
    CORRADE_INTERNAL_ASSERT(bits >= 8);
    const std::size_t stride = ((size.x()*channels*bits/8 + 3)/4)*4;
    data = Containers::Array<char>{stride*std::size_t(size.y())};

    /* Endianness correction for 16 bit depth */
    #ifndef CORRADE_TARGET_BIG_ENDIAN
    if(bits == 16) png_set_swap(file);
    #endif

    /* Read image row by row */
    rows = Containers::Array<png_bytep>{std::size_t(size.y())};
    for(Int i = 0; i != size.y(); ++i)
        rows[i] = reinterpret_cast<unsigned char*>(data.data()) + (size.y() - i - 1)*stride;
    png_read_image(file, rows);

    /* 8-bit images */
    PixelFormat format;
    if(bits == 8) {
        switch(colorType) {
            case PNG_COLOR_TYPE_GRAY: format = PixelFormat::R8Unorm; break;
            case PNG_COLOR_TYPE_GRAY_ALPHA:
                                      format = PixelFormat::RG8Unorm; break;
            case PNG_COLOR_TYPE_RGB:  format = PixelFormat::RGB8Unorm; break;
            case PNG_COLOR_TYPE_RGBA: format = PixelFormat::RGBA8Unorm; break;
            default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }

    /* 16-bit images */
    } else if(bits == 16) {
        switch(colorType) {
            case PNG_COLOR_TYPE_GRAY: format = PixelFormat::R16Unorm; break;
            case PNG_COLOR_TYPE_GRAY_ALPHA:
                                      format = PixelFormat::RG16Unorm; break;
            case PNG_COLOR_TYPE_RGB:  format = PixelFormat::RGB16Unorm; break;
            case PNG_COLOR_TYPE_RGBA: format = PixelFormat::RGBA16Unorm; break;
            default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }

    /* https://en.wikipedia.org/wiki/Portable_Network_Graphics#Pixel_format
       Only 1, 2, 4, 8 or 16 bits per channel, we expand the 1/2/4 to 8 above */
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    /* Always using the default 4-byte alignment */
    return Trade::ImageData2D{format, size, std::move(data)};
}

}}

CORRADE_PLUGIN_REGISTER(PngImporter, Magnum::Trade::PngImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.3")
