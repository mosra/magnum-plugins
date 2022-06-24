/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2022 Melike Batihan <melikebatihan@gmail.com>

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

#include "WebPImporter.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

#include <webp/types.h>
#include <webp/decode.h>
#include <webp/mux_types.h>

namespace Magnum { namespace Trade {

WebPImporter::WebPImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

WebPImporter::~WebPImporter() = default;

ImporterFeatures WebPImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool WebPImporter::doIsOpened() const { return _in; }

void WebPImporter::doClose() { _in = nullptr; }

void WebPImporter::doOpenData(Containers::Array<char>&& data, const DataFlags dataFlags) {
    /* Because here we're copying the data and using the _in to check if file
       is opened, having them nullptr would mean openData() would fail without
       any error message. It's not possible to do this check on the importer
       side, because empty file is valid in some formats (OBJ or glTF). We also
       can't do the full import here because then doImage2D() would need to
       copy the imported data instead anyway (and the uncompressed size is much
       larger). This way it'll also work nicely with a future openMemory(). */
    if(data.isEmpty()) {
        Error{} << "Trade::WebPImporter::openData(): the file is empty";
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

namespace {

const char* vp8StatusCodeString(const VP8StatusCode status) {
    switch(status) {
        /* LCOV_EXCL_START */
        case VP8_STATUS_OUT_OF_MEMORY: return "out of memory";
        case VP8_STATUS_INVALID_PARAM: return "invalid parameter";
        case VP8_STATUS_BITSTREAM_ERROR: return "bitstream error";
        case VP8_STATUS_UNSUPPORTED_FEATURE: return "unsupported feature";
        case VP8_STATUS_SUSPENDED: return "process suspended";
        case VP8_STATUS_USER_ABORT: return "process aborted";
        case VP8_STATUS_NOT_ENOUGH_DATA: return "not enough data";
        case VP8_STATUS_OK: ;
        /* LCOV_EXCL_STOP */
    }

    CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

}

UnsignedInt WebPImporter::doImage2DCount() const { return 1; }

Containers::Optional<ImageData2D> WebPImporter::doImage2D(UnsignedInt, UnsignedInt) {
    /* Decoder configuration */
    WebPDecoderConfig config;
    CORRADE_INTERNAL_ASSERT_OUTPUT(WebPInitDecoderConfig(&config));
    config.options.flip = true;

    /* Reading the file information into config.input. This also verifies the
       file is actually a WebP file. */
    WebPBitstreamFeatures bitstream;
    const VP8StatusCode status = WebPGetFeatures(reinterpret_cast<std::uint8_t*>(_in.data()), _in.size(), &bitstream);
    if(status != VP8_STATUS_OK) {
        Error err;
        err << "Trade::WebPImporter::image2D(): WebP image features not found:" << vp8StatusCodeString(status);
        return {};
    }

    /* Filtering animated WebP files, they are subject to a different decoding
       process defined in demux library */
    if(bitstream.format == 0) {
        Error{} << "Trade::WebPImporter::image2D(): animated WebP images aren't supported";
        return {};
    }

    /* Channel number and pixel format (always 8-bit per channel) determined by
       alpha transparency. No special handling for lossy vs lossless files. */
    Int channels = 3;
    PixelFormat pixelFormat = PixelFormat::RGB8Unorm;
    WEBP_CSP_MODE colourDepth = MODE_RGB;
    if(bitstream.has_alpha) {
        channels = 4;
        pixelFormat = PixelFormat::RGBA8Unorm;
        colourDepth = MODE_RGBA;
    }

    /* Structure and configuration for decoding */
    WebPDecBuffer& outputBuffer = config.output;
    const std::size_t stride = 4*((bitstream.width*channels + 3)/4);
    outputBuffer.u.RGBA.size = stride*bitstream.height;
    outputBuffer.u.RGBA.stride = stride;
    outputBuffer.colorspace = colourDepth;

    /* Create external memory pointed by outputBuffer buffer */
    Containers::Array<char> outData{NoInit, outputBuffer.u.RGBA.size};
    outputBuffer.u.RGBA.rgba = reinterpret_cast<std::uint8_t*>(outData.data());
    outputBuffer.is_external_memory = 1;

    /* Decompression of the image */
    const VP8StatusCode decodeStatus = WebPDecode(reinterpret_cast<std::uint8_t*>(_in.data()), _in.size(), &config);
    if(decodeStatus != VP8_STATUS_OK) {
        Error err;
        err << "Trade::WebPImporter::image2D(): decoding error:" << vp8StatusCodeString(decodeStatus);
        return {};
    }

    return Trade::ImageData2D{pixelFormat, {bitstream.width, bitstream.height}, std::move(outData)};
}

}}

CORRADE_PLUGIN_REGISTER(WebPImporter, Magnum::Trade::WebPImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.5")
