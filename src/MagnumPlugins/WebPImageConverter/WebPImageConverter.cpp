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

#include "WebPImageConverter.h"

//??
#include <iostream>
#include <cstring>

#include <cstring>
#include <algorithm> /* std::copy() */ /** @todo remove */
#include <string> /** @todo replace with a growable array */
#include <webp/types.h>
#include <webp/encode.h>
#include <webp/decode.h>
#include <webp/mux.h>
#include <webp/demux.h>
#include <webp/mux_types.h>

#include <csetjmp>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

WebPImageConverter::WebPImageConverter() = default;

WebPImageConverter::WebPImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures WebPImageConverter::doFeatures() const { return ImageConverterFeature::Convert2DToData; }

Containers::String WebPImageConverter::doExtension() const { return "webp"_s; }

Containers::String WebPImageConverter::doMimeType() const {
    return "image/webp"_s;
}

namespace {

    const char* vp8EncodingStatus(const WebPEncodingError status) {
        switch(status) {
            /* LCOV_EXCL_START */
            case VP8_ENC_ERROR_OUT_OF_MEMORY: return "out of memory";
            case VP8_ENC_ERROR_NULL_PARAMETER: return "null parameter";
            case VP8_ENC_ERROR_BITSTREAM_OUT_OF_MEMORY: return "bitstream flushing error";
            case VP8_ENC_ERROR_INVALID_CONFIGURATION: return "invalid configuration";
            case VP8_ENC_ERROR_BAD_DIMENSION: return "invalid picture size";
            case VP8_ENC_ERROR_PARTITION0_OVERFLOW: return "partition bigger than 512k";
            case VP8_ENC_ERROR_PARTITION_OVERFLOW: return "partition bigger than 16M";
            case VP8_ENC_ERROR_BAD_WRITE: return "error while flushing bytes";
            case VP8_ENC_ERROR_FILE_TOO_BIG: return "file too big";
            case VP8_ENC_ERROR_USER_ABORT: return "process aborted by user";
            case VP8_ENC_ERROR_LAST: return "terminator error";
            case VP8_ENC_OK: ;
                /* LCOV_EXCL_STOP */
        }

        CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }
}

Containers::Optional<Containers::Array<char>> WebPImageConverter::doConvertToData(const ImageView2D& image) {
    /* Warn about lost metadata */
    if((image.flags() & ImageFlag2D::Array) && !(flags() & ImageConverterFlag::Quiet)) {
        Warning{} << "Trade::WebPImageConverter::convertToData(): 1D array images are unrepresentable in WebP, saving as a regular 2D image";
    }

    const std::pair<Math::Vector2<std::size_t>, Math::Vector2<std::size_t>> dataProperties = image.dataProperties();
    auto inputData = Containers::arrayCast<const unsigned char>(image.data()).exceptPrefix(dataProperties.first.sum());

    int width = image.size().x();
    int height = image.size().y();
    UnsignedInt channel_count = pixelFormatChannelCount(image.format());
    const std::size_t stride = width * channel_count;  // in pixels units, not bytes.

    // Set up WebP configuration for encoding
    WebPConfig config;
    if(!WebPConfigInit(&config)) {
        Error() << "Trade::WebPImageConverter::convertToData(): couldn't initialize config: version mismatch";
        return {};
    }
    config.lossless = 1;
    config.exact = 1;
    config.quality = 100;
    if(channel_count ==4) config.alpha_quality = 100;
    config.method = 6;
    if (!WebPValidateConfig(&config)) {
        Error() << "Trade::WebPImageConverter::convertToData(): invalid configuration";
        return {};
    }

    // Set up writer method with a byte-output.
    WebPMemoryWriter writer;
    WebPMemoryWriterInit(&writer);

    // Store the input data in WebPPicture structure.
    WebPPicture pic;
    if (!WebPPictureInit(&pic)) {
        Error() << "Trade::WebPImageConverter::convertToData(): couldn't initialize picture structure: version mismatch";
        return {};
    }

    pic.use_argb = (channel_count == 4) ? 1 : 0;
    pic.width = width;
    pic.height = height;

    if(pic.width > WEBP_MAX_DIMENSION || pic.height > WEBP_MAX_DIMENSION) {
        if(flags() & ImageConverterFlag::Quiet)
            Error() << "Trade::WebPImageConverter::convertToData(): invalid WebPPicture size";
        else
            Warning{} << "Trade::WebPImageConverter::convertToData(): width or height of WebPPicture structure exceeds the maximum allowed:" << WEBP_MAX_DIMENSION;

        return {};
    }

    pic.writer = WebPMemoryWrite;
    pic.custom_ptr = &writer;

    int success = (channel_count == 4) ? WebPPictureImportRGBA(&pic, inputData.data(), stride)
           : WebPPictureImportRGB(&pic, inputData.data(), stride);
    if(!success) {
        Error() << "Buffer error: picture too big, %d\n" << vp8EncodingStatus(pic.error_code);
        return {};
    }

    // Encode image to WebP format.
    int result = WebPEncode(&config, &pic);
    if (!result) {
        WebPMemoryWriterClear(&writer);
        Error() << "Encoding error: %d\n" << vp8EncodingStatus(pic.error_code);
        return {};
    }

    auto output = writer.mem;
    Containers::Array<char> fileData{reinterpret_cast<char *>(output), writer.max_size};

    WebPPictureFree(&pic);

    /* GCC 4.8 needs extra help here */
    return Containers::optional(std::move(fileData));
}

}}

CORRADE_PLUGIN_REGISTER(WebPImageConverter, Magnum::Trade::WebPImageConverter,
    MAGNUM_TRADE_ABSTRACTIMAGECONVERTER_PLUGIN_INTERFACE)
