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

#include "PngImporter.h"

#include <cstring> /* std::strcmp(), std::memcpy() */
#include <png.h>
/*
    The <csetjmp> header has to be included *after* png.h, otherwise older
    versions of libpng (i.e., one used on Travis 16.04 images), complain that

        __pngconf.h__ in libpng already includes setjmp.h
        __dont__ include it again.

    New versions don't have that anymore: https://github.com/glennrp/libpng/commit/6c2e919c7eb736d230581a4c925fa67bd901fcf8
*/
#include <csetjmp> /* setjmp(), libpng why are you still insane */
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Trade/ImageData.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

PngImporter::PngImporter() = default;

PngImporter::PngImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

PngImporter::~PngImporter() = default;

ImporterFeatures PngImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool PngImporter::doIsOpened() const { return _in; }

void PngImporter::doClose() { _in = nullptr; }

void PngImporter::doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) {
    /* Because here we're copying the data and using the _in to check if file
       is opened, having them nullptr would mean openData() would fail without
       any error message. It's not possible to do this check on the importer
       side, because empty file is valid in some formats (OBJ or glTF). We also
       can't do the full import here because then doImage2D() would need to
       copy the imported data instead anyway (and the uncompressed size is much
       larger). This way it'll also work nicely with a future openMemory(). */
    if(data.isEmpty()) {
        Error{} << "Trade::PngImporter::openData(): the file is empty";
        return;
    }

    /* Take over the existing array or copy the data if we can't */
    if(dataFlags & (DataFlag::Owned|DataFlag::ExternallyOwned)) {
        _in = std::move(data);
    } else {
        _in = Containers::Array<char>{NoInit, data.size()};
        Utility::copy(data, _in);
    }
}

UnsignedInt PngImporter::doImage2DCount() const { return 1; }

Containers::Optional<ImageData2D> PngImporter::doImage2D(UnsignedInt, UnsignedInt) {
    /* Structures for reading the file. To avoid leaks, everything that needs
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
        png_destroy_read_struct(&state->file, &state->info, nullptr);
    }};
    Containers::Array<png_bytep> rows;
    Containers::Array<char> data;
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

    /* Create the read structure. Directly set up also error/warning
       callbacks because if done here and not in a subsequent
       png_set_error_fn() call, it'll gracefully handle also libpng version
       mismatch errors. */
    png.file = png_create_read_struct(PNG_LIBPNG_VER_STRING, &jmpbuf, [](const png_structp file, const png_const_charp message) {
        Error{} << "Trade::PngImporter::image2D(): error:" << message;
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
           That may cause great confusion ("why image2D() returns a NullOpt if
           it was just a warning??"), so I'm detecting that, annotating it as
           an error instead, and jumping to the error return so we don't end up
           with a nullptr `png.file` below.

           Unfortunately this case is annoyingly hard to test automatically, so
           the following branch is uncovered. */
        if(Containers::StringView{message}.hasPrefix("Application built with libpng-"_s)) {
            Error{} << "Trade::PngImporter::image2D(): error:" << message;
            std::longjmp(*static_cast<std::jmp_buf*>(png_get_error_ptr(file)), 1);
        } else
            Warning{} << "Trade::PngImporter::image2D(): warning:" << message;
    });
    /* If png_create_read_struct() failed with an error or "failed with a
       warning" due to the version mismatch), in both cases it longjmp()'d and
       already returned above. So if we get to this point, the file pointer
       should always be there. */
    CORRADE_INTERNAL_ASSERT(png.file);

    // FFS
    if(setjmp(jmpbuf))
        return {};

    png.info = png_create_info_struct(png.file);
    CORRADE_INTERNAL_ASSERT(png.info);

    /* Set functions for reading */
    Containers::ArrayView<char> input = _in;
    png_set_read_fn(png.file, &input, [](const png_structp file, const png_bytep data, const png_size_t length) {
        auto&& input = *reinterpret_cast<Containers::ArrayView<char>*>(png_get_io_ptr(file));
        if(input.size() < length) png_error(file, "file too short");
        std::memcpy(data, input.begin(), length);
        input = input.exceptPrefix(length);
    });

    /* Read file information */
    png_read_info(png.file, png.info);

    /* Image size */
    const Vector2i size(png_get_image_width(png.file, png.info), png_get_image_height(png.file, png.info));

    /* Image channels and bit depth */
    png_uint_32 bits = png_get_bit_depth(png.file, png.info);
    png_uint_32 channels = png_get_channels(png.file, png.info);
    png_uint_32 colorType = png_get_color_type(png.file, png.info);

    /* Check image format, convert if necessary */
    switch(colorType) {
        /* Types that can be used without conversion */
        case PNG_COLOR_TYPE_GRAY:
            CORRADE_INTERNAL_ASSERT(channels == 1);

            /* Convert to 8-bit */
            if(bits < 8) {
                png_set_expand_gray_1_2_4_to_8(png.file);
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
            png_set_palette_to_rgb(png.file);
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
    if(png_get_valid(png.file, png.info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png.file);
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

    /** @todo there's an option to convert alpha to premultiplied on load:
        https://github.com/glennrp/libpng/blob/a37d4836519517bdce6cb9d956092321eca3e73b/png.h#L1096-L1135
        but there doesn't seem to be a way to check if the PNG file is already
        premultiplied (which is disallowed by the PNG spec, but ... tools)
        or do detection based on what tool exported the image? such as blender
        producing premultiplied PNGs https://developer.blender.org/T24764 */

    /* Initialize data array, align rows to four bytes */
    CORRADE_INTERNAL_ASSERT(bits >= 8);
    const std::size_t stride = ((size.x()*channels*bits/8 + 3)/4)*4;
    data = Containers::Array<char>{stride*std::size_t(size.y())};

    /* Endianness correction for 16 bit depth */
    #ifndef CORRADE_TARGET_BIG_ENDIAN
    if(bits == 16) png_set_swap(png.file);
    #endif

    /* Read image row by row */
    rows = Containers::Array<png_bytep>{std::size_t(size.y())};
    for(Int i = 0; i != size.y(); ++i)
        rows[i] = reinterpret_cast<unsigned char*>(data.data()) + (size.y() - i - 1)*stride;
    png_read_image(png.file, rows);

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
    "cz.mosra.magnum.Trade.AbstractImporter/0.5")
