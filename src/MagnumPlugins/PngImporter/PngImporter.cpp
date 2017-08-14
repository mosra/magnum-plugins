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

#include "PngImporter.h"

#include <algorithm>
#include <png.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Endianness.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Trade/ImageData.h>

#ifdef MAGNUM_TARGET_GLES2
#include <Magnum/Context.h>
#include <Magnum/Extensions.h>
#endif

namespace Magnum { namespace Trade {

PngImporter::PngImporter() = default;

PngImporter::PngImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

PngImporter::~PngImporter() = default;

auto PngImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool PngImporter::doIsOpened() const { return _in; }

void PngImporter::doClose() { _in = nullptr; }

void PngImporter::doOpenData(const Containers::ArrayView<const char> data) {
    _in = Containers::Array<unsigned char>(data.size());
    std::copy(data.begin(), data.end(), _in.begin());
}

UnsignedInt PngImporter::doImage2DCount() const { return 1; }

std::optional<ImageData2D> PngImporter::doImage2D(UnsignedInt) {
    CORRADE_ASSERT(std::strcmp(PNG_LIBPNG_VER_STRING, png_libpng_ver) == 0,
        "Trade::PngImporter::image2D(): libpng version mismatch, got" << png_libpng_ver << "but expected" << PNG_LIBPNG_VER_STRING, std::nullopt);

    /* Verify file signature */
    if(png_sig_cmp(_in, 0, Math::min<std::size_t>(8, _in.size())) != 0) {
        Error() << "Trade::PngImporter::image2D(): wrong file signature";
        return std::nullopt;
    }

    /* Structures for reading the file */
    png_structp file = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    CORRADE_INTERNAL_ASSERT(file);
    png_infop info = png_create_info_struct(file);
    CORRADE_INTERNAL_ASSERT(info);
    Containers::Array<png_bytep> rows;
    Containers::Array<char> data;

    /* Error handling routine */
    /** @todo Get rid of setjmp (won't work everywhere) */
    if(setjmp(png_jmpbuf(file))) {
        Error() << "Trade::PngImporter::image2D(): error while reading PNG file";

        png_destroy_read_struct(&file, &info, nullptr);
        return std::nullopt;
    }

    /* Input starts right after the header */
    Containers::ArrayView<unsigned char> input =_in.suffix(8);

    /* Set function for reading from std::istream */
    png_set_read_fn(file, &input, [](png_structp file, png_bytep data, png_size_t length) {
        auto&& input = *reinterpret_cast<Containers::ArrayView<unsigned char>*>(png_get_io_ptr(file));
        std::copy_n(input.begin(), Math::min(length, input.size()), data);
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
    const png_uint_32 colorType = png_get_color_type(file, info);

    /* Image format */
    PixelFormat format;
    switch(colorType) {
        /* Types that can be used without conversion */
        case PNG_COLOR_TYPE_GRAY:
            #ifndef MAGNUM_TARGET_GLES2
            format = PixelFormat::Red;
            #else
            format = Context::hasCurrent() && Context::current().isExtensionSupported<Extensions::GL::EXT::texture_rg>() ?
                PixelFormat::Red : PixelFormat::Luminance;
            #endif
            CORRADE_INTERNAL_ASSERT(channels == 1);

            /* Convert to 8-bit */
            if(bits < 8) {
                png_set_expand_gray_1_2_4_to_8(file);
                bits = 8;
            }

            break;

        case PNG_COLOR_TYPE_RGB:
            format = PixelFormat::RGB;
            CORRADE_INTERNAL_ASSERT(channels == 3);
            break;

        case PNG_COLOR_TYPE_RGBA:
            format = PixelFormat::RGBA;
            CORRADE_INTERNAL_ASSERT(channels == 4);
            break;

        /* Palette needs to be converted */
        case PNG_COLOR_TYPE_PALETTE:
            /** @todo test case for this */
            png_set_palette_to_rgb(file);
            format = PixelFormat::RGB;
            channels = 3;
            break;

        default:
            Error() << "Trade::PngImporter::image2D(): unsupported color type" << colorType;
            png_destroy_read_struct(&file, &info, nullptr);
            return std::nullopt;
    }

    /* Convert transparency mask to alpha */
    if(png_get_valid(file, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(file);
        channels += 1;
        CORRADE_INTERNAL_ASSERT(channels == 4);
    }

    /* Image type */
    PixelType type;
    switch(bits) {
        case 8:  type = PixelType::UnsignedByte;  break;
        case 16: type = PixelType::UnsignedShort; break;

        default:
            Error() << "Trade::PngImporter::image2D(): unsupported bit depth" << bits;
            png_destroy_read_struct(&file, &info, nullptr);
            return std::nullopt;
    }

    /* Initialize data array, align rows to four bytes */
    const std::size_t stride = ((size.x()*channels*bits/8 + 3)/4)*4;
    data = Containers::Array<char>{stride*std::size_t(size.y())};

    /* Read image row by row */
    rows = Containers::Array<png_bytep>{std::size_t(size.y())};
    for(Int i = 0; i != size.y(); ++i)
        rows[i] = reinterpret_cast<unsigned char*>(data.data()) + (size.y() - i - 1)*stride;
    png_read_image(file, rows);

    /* Cleanup */
    png_destroy_read_struct(&file, &info, nullptr);

    /* Endianness correction for 16 bit depth */
    if(type == PixelType::UnsignedShort) {
        Containers::ArrayView<UnsignedShort> data16{reinterpret_cast<UnsignedShort*>(data.data()), data.size()/2};
        for(UnsignedShort& i: data16)
            Utility::Endianness::bigEndianInPlace(i);
    }

    /* Always using the default 4-byte alignment */
    return Trade::ImageData2D{format, type, size, std::move(data)};
}

}}
