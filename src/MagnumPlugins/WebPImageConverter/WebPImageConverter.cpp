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

#include <algorithm> /* std::copy() */ /** @todo remove */
#include <webp/types.h>
#include <webp/encode.h>
#include <webp/mux_types.h>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/ScopeGuard.h>

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

    // Set up WebP configuration for encoding
    WebPConfig config;
    if(!WebPConfigInit(&config)) {
        Error() << "Trade::WebPImageConverter::convertToData(): couldn't initialize config: version mismatch";
        return {};
    }

    UnsignedInt channelCount;
    switch(image.format()) {
        case PixelFormat::RGB8Unorm:
            channelCount = 3;
            config.alpha_quality = 0;
            break;
        case PixelFormat::RGBA8Unorm:
            channelCount = 4;
            config.alpha_quality = 100;
            break;
        default:
            Error{} << "Trade::WebPImageConverter::convertToData(): unsupported pixel format";
            return {};
    }

    // Flip the order of the pixel rows.
    const Containers::StridedArrayView3D<const char> pixels = image.pixels();
    const Containers::StridedArrayView3D<const char> pixelsFlipped = pixels.flipped<0>();
    auto inputData = reinterpret_cast<uint8_t*>(const_cast<void*>(pixelsFlipped.data()));

    config.lossless = 1;
    config.exact = 1;
    config.quality = 100;
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
    pic.use_argb = 1;
    pic.width = image.size().x();
    pic.height = image.size().y();
    pic.writer = WebPMemoryWrite;
    pic.custom_ptr = &writer;

    if (!WebPPictureAlloc(&pic)) {
        Error() << "Memory error: couldn't allocate memory buffer\n";
        return {};
    }

    const int success = (channelCount == 4) ? WebPPictureImportRGBA(&pic, inputData, -pixels.stride()[0])
                                             : WebPPictureImportRGB(&pic, inputData, -pixels.stride()[0]);
    if(!success) {
        Error() << "Buffer error: picture too big, %d\n" << vp8EncodingStatus(pic.error_code);
        return {};
    }

    // Encode image to WebP format.
    int result = WebPEncode(&config, &pic);
    if (!result) {
        Containers::ScopeGuard e{&writer, [](WebPMemoryWriter *w) {
            WebPMemoryWriterClear(w);
        }};
        Error() << "Encoding error:" << vp8EncodingStatus(pic.error_code);
        return {};
    }

    Containers::Array<char> fileData{NoInit, writer.size};
    std::copy(writer.mem, writer.mem+writer.size, fileData.begin());

    Containers::ScopeGuard e{&pic, [](WebPPicture *p) {
        WebPPictureFree(p);
    }};

    return Containers::optional(std::move(fileData));
}

}}

CORRADE_PLUGIN_REGISTER(WebPImageConverter, Magnum::Trade::WebPImageConverter,
    MAGNUM_TRADE_ABSTRACTIMAGECONVERTER_PLUGIN_INTERFACE)
