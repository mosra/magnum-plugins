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
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Configuration.h>
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
        _in = Utility::move(data);
    } else {
        _in = Containers::Array<char>{NoInit, data.size()};
        Utility::copy(data, _in);
    }
}

UnsignedInt PngImporter::doImage2DCount() const { return 1; }

Containers::Optional<ImageData2D> PngImporter::doImage2D(UnsignedInt, UnsignedInt) {
    /* Structures for reading the file */
    png_structp file = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    /** @todo this will assert if the PNG major/minor version doesn't match,
        with "libpng warning: Application built with libpng-1.7.0 but running
        with 1.6.38" being printed to stdout, the proper fix is to set error
        callbacks directly in the png_create_read_struct() call */
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
    }, flags() & ImporterFlag::Quiet ?
        /* MSVC w/o /permissive- "cannot convert <lambda> to <lambda>", ffs.
           Casting just one of the two is enough. */
        #ifndef CORRADE_MSVC_COMPATIBILITY
        [](png_structp, png_const_charp) {}
        #else
        static_cast<void(*)(png_structp, png_const_charp)>([](png_structp, png_const_charp) {})
        #endif
        : [](png_structp, const png_const_charp message) {
            Warning{} << "Trade::PngImporter::image2D(): warning:" << message;
        }
    );

    /* Set functions for reading */
    Containers::ArrayView<char> input = _in;
    png_set_read_fn(file, &input, [](const png_structp file, const png_bytep data, const png_size_t length) {
        auto&& input = *reinterpret_cast<Containers::ArrayView<char>*>(png_get_io_ptr(file));
        if(input.size() < length) png_error(file, "file too short");
        std::memcpy(data, input.begin(), length);
        input = input.exceptPrefix(length);
    });

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

    /* Premultiply alpha, if desired */
    if(const Containers::StringView alphaMode = configuration().value<Containers::StringView>("alphaMode")) {
        if(alphaMode != "premultiplied"_s) {
            Error{} << "Trade::PngImporter::image2D(): expected alphaMode to be either empty or premultiplied but got" << alphaMode;
            return {};
        }

        /* Everything would be *a lot* simpler if there was a direct option to
           JUST GIVE ME THE PIXEL VALUES PREMULTIPLIED, SATISFYING WHATEVER
           GAMMA VALUE WAS THERE, TRUST ME, I KNOW WHAT TO DO WITH THE OUTPUT.
           But that's not the case, it seems -- there's a complicated
           png_set_gamma() API to make the imported values match the screen
           gamma or whatnot (who needs that nowadays, with various GPU
           operations and compositors being in the way?!), and then a
           png_set_alpha_mode() which seems to be meant to replace
           png_set_gamma(), but seems like both of them have to be used in
           order to satisfy all of below:

            1.  If the file doesn't contain an alpha channel or the alpha
                channel is opaque, the imported values should be the same
                regardless of whether I touched any of png_set_alpha_mode() /
                png_set_gamma().
            2.  If the file contains an alpha channel and I don't enable
                premultiplication, the imported values shouldn't get changed in
                any way compared to just a raw import without
                png_set_alpha_mode() / png_set_gamma(), or compared to
                stb_image, libspng etc. This is achieved by simply not touching
                those functions in that case (which I assume also avoids any
                potential overhead of thise)
            3.  If the file contains an alpha channel and there's a gAMA chunk
                that says it's sRGB, premultiply the alpha accordingly, i.e.
                first perform a sRGB decode, then scale, then encode back. This
                is verified by manually scaling the sRGB-decoded pixel values
                in PngImporterTest::rgba().
            4.  If the file contains an alpha channel and there's a gAMA chunk
                that says it's linear, premultiply the alpha without a sRGB
                decode. This is again verified manually in
                PngImporterTest::rgba().
            5.  If the file contains an alpha channel and there's no gAMA
                chunk, continue as if there was a gAMA chunk saying it's sRGB
                (which is what PNG_DEFAULT_sRGB alone was meant to handle, I
                think?)

           The code below is the only combination I found that passes all
           tests. It's possible that there's a more efficient way to do that
           (scaling with an arbitrary gamma value sounds anything but
           performant), I just didn't find it in a reasonable timespan. */
        png_set_alpha_mode(file, PNG_ALPHA_PREMULTIPLIED, PNG_DEFAULT_sRGB);
        Double gamma;
        if(png_get_gAMA(file, info, &gamma) && Math::equal(gamma, 1.0)) {
            png_set_gamma(file, PNG_GAMMA_LINEAR, PNG_GAMMA_LINEAR);
        } else {
            /** @todo some constants for this?! */
            png_set_gamma(file, 1.0/0.45455, 0.45455);
        }

        /** @todo there doesn't seem to be a metadata specifying if the PNG
            file is already premultiplied (apart from the Apple CgBI
            extension), however some tools do that ... do detection based on
            what tool exported the image? such as blender producing
            premultiplied PNGs https://developer.blender.org/T24764 */
    }

    /* Enable 8-to-16 or 16-to-8 conversion if desired */
    if(const Int forceBitDepth = configuration().value<Int>("forceBitDepth")) {
        if(forceBitDepth == 8 && bits != 8) {
            CORRADE_INTERNAL_ASSERT(bits == 16);
            if(flags() & ImporterFlag::Verbose)
                Debug{} << "Trade::PngImporter::image2D(): stripping" << bits << Debug::nospace << "-bit channels to 8-bit";
            png_set_scale_16(file);
            bits = 8;
        } else if(forceBitDepth == 16 && bits != 16) {
            CORRADE_INTERNAL_ASSERT(bits == 8);
            if(flags() & ImporterFlag::Verbose)
                Debug{} << "Trade::PngImporter::image2D(): expanding" << bits << Debug::nospace << "-bit channels to 16-bit";
            png_set_expand_16(file);
            bits = 16;
        } else if(forceBitDepth != 8 && forceBitDepth != 16) {
            Error{} << "Trade::PngImporter::image2D(): expected forceBitDepth to be 0, 8 or 16 but got" << configuration().value<Containers::StringView>("forceBitDepth");
            return {};
        }
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
    return Trade::ImageData2D{format, size, Utility::move(data)};
}

}}

CORRADE_PLUGIN_REGISTER(PngImporter, Magnum::Trade::PngImporter,
    MAGNUM_TRADE_ABSTRACTIMPORTER_PLUGIN_INTERFACE)
