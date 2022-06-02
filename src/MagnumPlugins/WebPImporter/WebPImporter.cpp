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

#include <webp/types.h>
#include <webp/decode.h>
#include <webp/mux_types.h>

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

namespace Magnum { namespace Trade {

WebPImporter::WebPImporter() = default;

WebPImporter::WebPImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

WebPImporter::~WebPImporter() = default;

ImporterFeatures WebPImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool WebPImporter::doIsOpened() const { return _in; }

void WebPImporter::doClose() { _in = nullptr; }

void WebPImporter::doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) {
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

const char* vp8StatusCodeString(VP8StatusCode status) {
    switch(status) {
        case VP8_STATUS_OUT_OF_MEMORY: return "out of memory";
        case VP8_STATUS_INVALID_PARAM: return "invalid parameter";
        case VP8_STATUS_BITSTREAM_ERROR: return "bitstream error";
        case VP8_STATUS_UNSUPPORTED_FEATURE: return "unsupported feature";
        case VP8_STATUS_SUSPENDED: return "process suspended";
        case VP8_STATUS_USER_ABORT: return "process aborted";
        case VP8_STATUS_NOT_ENOUGH_DATA: return "not enough data";
        case VP8_STATUS_OK: ;
    }
    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
}

UnsignedInt WebPImporter::doImage2DCount() const { return 1; }

Containers::Optional<ImageData2D> WebPImporter::doImage2D(UnsignedInt, UnsignedInt) {

    /* Create the structure that contains the WebP image data. */
    WebPData image;
    image.bytes = reinterpret_cast<uint8_t*>(_in.data());
    image.size = _in.size();

    /* Verify the file is a WebP image file */
    const bool isWebPImage = WebPGetInfo(image.bytes, image.size, nullptr, nullptr);
    if(!isWebPImage) {
        Error() << "Trade::WebPImporter::image2D(): not a webp image";
        return {};
    }

    /* Structures for bitstream features and decoder configuration */
    WebPDecoderConfig config;
    WebPBitstreamFeatures bitstream = config.input;
    CORRADE_INTERNAL_ASSERT_OUTPUT(WebPInitDecoderConfig(&config));

    /* Reading the file information into config.input */
    const VP8StatusCode status = WebPGetFeatures(image.bytes, image.size, &bitstream);
    if(status != VP8_STATUS_OK) {
        Error err;
        err << "Trade::WebPImporter::image2D(): webp image features not found:" << vp8StatusCodeString(status);
        return {};
    }

    /* Filtering animated webp files, they are subject to a different decoding process defined in demux library */
    if(bitstream.format == 0) {
        Error{} << "Trade::WebPImporter::image2D(): animated webp images aren't supported";
        return {};
    }

    /* Image size */
    const Vector2i size(bitstream.width, bitstream.height);

    /* Channel number and pixel format (always 8-bit per channel) determined by alpha transparency,
       independent of the image quality (lossy or lossless) */
    int channels = 3;
    PixelFormat pixelFormat = PixelFormat::RGB8Unorm;
    WEBP_CSP_MODE colourDepth = MODE_RGB;

    if(bitstream.has_alpha) {
        channels = 4;
        pixelFormat = PixelFormat::RGBA8Unorm;
        colourDepth = MODE_RGBA;
    }

    /* Structure and configuration for decoding */
    WebPDecBuffer& outputBuffer = config.output;
    const std::size_t stride = 4*((size.x()*channels + 3)/4);
    outputBuffer.u.RGBA.size = stride*size.y();
    outputBuffer.u.RGBA.stride = stride;
    outputBuffer.colorspace = colourDepth;

    /* Create external memory pointed by outputBuffer buffer */
    Containers::Array<char> outData{NoInit, outputBuffer.u.RGBA.size};
    auto rgbaBuffer = reinterpret_cast<uint8_t*>(outData.data());
    outputBuffer.u.RGBA.rgba = rgbaBuffer;
    outputBuffer.is_external_memory = 1;

    /* Decompression of the image */
    const VP8StatusCode decodeStatus = WebPDecode(image.bytes, image.size, &config);
    if(decodeStatus != VP8_STATUS_OK) {
        Error err;
        err << "Trade::WebPImporter::image2D(): decoding error:" << vp8StatusCodeString(decodeStatus);
        return {}; ;
    }

    return Trade::ImageData2D{pixelFormat, size, std::move(outData)};
}
}}

CORRADE_PLUGIN_REGISTER(WebPImporter, Magnum::Trade::WebPImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.5")
