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

#include "SpngImporter.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

#include "spng.h"

namespace Magnum { namespace Trade {

SpngImporter::SpngImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin) : AbstractImporter{manager, plugin} {}

SpngImporter::~SpngImporter() = default;

ImporterFeatures SpngImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool SpngImporter::doIsOpened() const { return !!_in; }

void SpngImporter::doClose() { _in = nullptr; }

void SpngImporter::doOpenData(Containers::Array<char>&& data, const DataFlags dataFlags) {
    /* Because here we're copying the data and using the _in to check if file
       is opened, having them nullptr would mean openData() would fail without
       any error message. It's not possible to do this check on the importer
       side, because empty file is valid in some formats (OBJ or glTF). We also
       can't do the full import here because then doImage2D() would need to
       copy the imported data instead anyway (and the uncompressed size is much
       larger). This way it'll also work nicely with a future openMemory(). */
    if(data.isEmpty()) {
        Error{} << "Trade::SpngImporter::openData(): the file is empty";
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

UnsignedInt SpngImporter::doImage2DCount() const { return 1; }

Containers::Optional<ImageData2D> SpngImporter::doImage2D(UnsignedInt, UnsignedInt) {
    /* Create a decoder context */
    spng_ctx* const ctx = spng_ctx_new(0);
    Containers::ScopeGuard ctxGuard{ctx, spng_ctx_free};
    CORRADE_INTERNAL_ASSERT(ctx);

    /* Set an input buffer. Error reporting is largely undocumented, but in the
       source it fails only due to programmer error, not due to bad data. */
    CORRADE_INTERNAL_ASSERT_OUTPUT(spng_set_png_buffer(ctx, _in, _in.size()) == SPNG_OK);

    /* Get image header */
    spng_ihdr ihdr;
    if(const int error = spng_get_ihdr(ctx, &ihdr)) {
        Error{} << "Trade::SpngImporter::image2D(): failed to read the header:" << spng_strerror(error);
        return {};
    }

    /* If the tRNS chunk is present, patch the color type so the alpha gets
       used in the pixel format below. */
    spng_color_type colorType = spng_color_type(ihdr.color_type);
    bool hasTrns = false;
    {
        spng_trns trns;
        const int error = spng_get_trns(ctx, &trns);
        if(error == 0) {
            hasTrns = true;
            switch(colorType) {
                case SPNG_COLOR_TYPE_GRAYSCALE:
                    colorType = SPNG_COLOR_TYPE_GRAYSCALE_ALPHA;
                    break;
                case SPNG_COLOR_TYPE_INDEXED:
                case SPNG_COLOR_TYPE_TRUECOLOR:
                    colorType = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
                    break;
                case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
                case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
                    /* The output already has alpha, nothing to do */
                    /** @todo untested -- is this even a valid scenario? */
                    break;
            }
        } else if(error != SPNG_ECHUNKAVAIL) {
            Error{} << "Trade::SpngImporter::image2D(): failed to get the tRNS chunk:" << spng_strerror(error);
            return {};
        }
    }

    /* Decide on the pixel format */
    spng_format spngFormat{};
    PixelFormat format{};
    std::size_t pixelSize{};
    /* 1, 2, 4 and 8 bits, expanded to 8 */
    if(ihdr.bit_depth <= 8) switch(colorType) {
        case SPNG_COLOR_TYPE_GRAYSCALE:
            spngFormat = SPNG_FMT_G8;
            format = PixelFormat::R8Unorm;
            pixelSize = 1;
            break;
        case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
            /* Only expansion of 1/2/4/8-bit gray channel + optional tRNS is
               implemented, not gray+alpha:
                https://github.com/randy408/libspng/issues/74
               Same error is for 16-bit formats, but there we use the
               "passthrough" (SPNG_FMT_PNG) so it doesn't get hit:
                https://github.com/randy408/libspng/blob/ea6ca5bc18246a338a40b8ae0a55f77928442e28/spng/spng.c#L642-L647 */
            /** @todo revisit once implemented */
            if(hasTrns) {
                spngFormat = SPNG_FMT_GA8;
                format = PixelFormat::RG8Unorm;
                pixelSize = 2;
            } else {
                spngFormat = SPNG_FMT_RGBA8;
                format = PixelFormat::RGBA8Unorm;
                pixelSize = 4;
            }
            break;
        case SPNG_COLOR_TYPE_INDEXED:
        case SPNG_COLOR_TYPE_TRUECOLOR:
            spngFormat = SPNG_FMT_RGB8;
            format = PixelFormat::RGB8Unorm;
            pixelSize = 3;
            break;
        case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
            spngFormat = SPNG_FMT_RGBA8;
            format = PixelFormat::RGBA8Unorm;
            pixelSize = 4;
            break;
    /* 16 bits */
    } else if(ihdr.bit_depth == 16) {
        /* There's no SPNG_FMT_G16 / SPNG_FMT_RGB16, but as far as I understand
           the comment at https://github.com/randy408/libspng/issues/243, the
           16-bit formats need no conversion and so SPNG_FMT_PNG stands for
           "just pass the data through" */
        /** @todo using SPNG_FMT_PNG will cause tRNS to get ignored, create a
            test file and fix: https://github.com/randy408/libspng/blob/ea6ca5bc18246a338a40b8ae0a55f77928442e28/spng/spng.c#L3737-L3743 */
        spngFormat = SPNG_FMT_PNG;
        switch(colorType) {
            case SPNG_COLOR_TYPE_GRAYSCALE:
                format = PixelFormat::R16Unorm;
                pixelSize = 2;
                break;
            case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
                format = PixelFormat::RG16Unorm;
                pixelSize = 4;
                break;
            case SPNG_COLOR_TYPE_TRUECOLOR:
                format = PixelFormat::RGB16Unorm;
                pixelSize = 6;
                break;
            case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
                format = PixelFormat::RGBA16Unorm;
                pixelSize = 8;
                break;
            /* Palleted images are always 8-bit */
            /* LCOV_EXCL_START */
            case SPNG_COLOR_TYPE_INDEXED:
                CORRADE_INTERNAL_ASSERT_UNREACHABLE();
            /* LCOV_EXCL_STOP */
        }
    /* There isn't any other bit depth */
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    /* Not using default cases in the above switches to get notified about new
       enum values, nevertheless should check that they actually got written */
    CORRADE_INTERNAL_ASSERT(spngFormat && UnsignedInt(format) && pixelSize);

    /* Allocate output data with rows aligned to 4 bytes */
    const std::size_t stride = 4*((pixelSize*ihdr.width + 3)/4);
    Containers::Array<char> out{NoInit, stride*ihdr.height};

    /* Begin progressive decoding. Enable tRNS decoding always, it'll be
       ignored if no tRNS chunk was present. */
    if(const int error = spng_decode_image(ctx, nullptr, 0, spngFormat, SPNG_DECODE_TRNS|SPNG_DECODE_PROGRESSIVE)) {
        Error{} << "Trade::SpngImporter::image2D(): failed to start decoding:" << spng_strerror(error);
        return {};
    }

    /* Decode row-by-row, in reverse order */
    /** @todo might want to use https://github.com/randy408/libspng/pull/166
        instead of progressive decoding and StridedArrayView slicing, if it
        gets merged; OTOH unless there's an explicit way to align rows to
        four bytes we still need to do this :/ */
    const auto rows = Containers::StridedArrayView2D<char>{out, {ihdr.height, stride}}.flipped<0>();
    for(;;) {
        /* Again, the error state documentation is lacking, but looking at the
           source this one can fail only due to a programmer error, or with
           SPNG_EOI -- but that we handle in spng_decode_row() below, so it
           shouldn't get here after that. */
        spng_row_info rowInfo;
        CORRADE_INTERNAL_DEBUG_ASSERT_OUTPUT(spng_get_row_info(ctx, &rowInfo) == 0);

        const Containers::StridedArrayView1D<char> row = rows[rowInfo.row_num];
        if(const int error = spng_decode_row(ctx, row.data(), row.size())) {
            if(error == SPNG_EOI)
                break;
            Error{} << "Trade::SpngImporter::image2D(): failed to decode a row:" << spng_strerror(error);
            return {};
        }
    }

    /* With progressive decoding, libspng doesn't fail if the data is
       incomplete, which is a bit unfortunate, however we need to use
       progressive decoding for Y flip and 4-byte padding :(
        https://libspng.org/docs/decode/#error-handling
       Possibly related: https://github.com/randy408/libspng/issues/119 */

    return ImageData2D{format, Vector2i{Int(ihdr.width), Int(ihdr.height)}, std::move(out)};
}

}}

CORRADE_PLUGIN_REGISTER(SpngImporter, Magnum::Trade::SpngImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.5.1")
