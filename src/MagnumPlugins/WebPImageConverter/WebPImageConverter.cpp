/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2023 Melike Batihan <melikebatihan@gmail.com>

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

#include <webp/encode.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

WebPImageConverter::WebPImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures WebPImageConverter::doFeatures() const {
    return ImageConverterFeature::Convert2DToData;
}

Containers::String WebPImageConverter::doExtension() const { return "webp"_s; }

Containers::String WebPImageConverter::doMimeType() const {
    return "image/webp"_s;
}

namespace {

const char* errorString(const WebPEncodingError error) {
    switch(error) {
        /* LCOV_EXCL_START */
        case VP8_ENC_ERROR_OUT_OF_MEMORY:
            return "out of memory";
        case VP8_ENC_ERROR_NULL_PARAMETER:
            return "null parameter";
        case VP8_ENC_ERROR_BITSTREAM_OUT_OF_MEMORY:
            return "bitstream flushing error";
        case VP8_ENC_ERROR_INVALID_CONFIGURATION:
            return "invalid configuration";
        case VP8_ENC_ERROR_BAD_DIMENSION:
            return "invalid picture size";
        case VP8_ENC_ERROR_PARTITION0_OVERFLOW:
            return "partition bigger than 512k";
        case VP8_ENC_ERROR_PARTITION_OVERFLOW:
            return "partition bigger than 16M";
        case VP8_ENC_ERROR_BAD_WRITE:
            return "error while flushing bytes";
        case VP8_ENC_ERROR_FILE_TOO_BIG:
            return "file too big";
        case VP8_ENC_ERROR_USER_ABORT:
            return "process aborted by user";
        case VP8_ENC_ERROR_LAST:
            return "terminator error";
        case VP8_ENC_OK:
            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
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

    /* Set up initial config from a configuration preset */
    const Containers::StringView presetString = configuration().value<Containers::StringView>("preset");
    bool losslessPreset;
    WebPConfig config;
    if(presetString == "lossless"_s) {
        losslessPreset = true;
        /* While WebPConfigPreset() calls WebPConfigInit() internally,
           WebPConfigLosslessPreset() doesn't, causing the validation below
           to fail. Do that directly here as well. */
        if(!WebPConfigInit(&config) || !WebPConfigLosslessPreset(&config, configuration().value<Int>("lossless"))) {
            /** @todo this fails also due to a version mismatch, unfortunately
                there's no way to know what actually failed, and calling
                WebPConfigInit() once more above seems like an excessive amount
                of error checking */
            Error{} << "Trade::WebPImageConverter::convertToData(): cannot apply a lossless preset with level" << configuration().value<Int>("lossless");
            return {};
        }
    } else {
        losslessPreset = false;
        WebPPreset preset;
        if(presetString == "default"_s)
            preset = WEBP_PRESET_DEFAULT;
        else if(presetString == "picture"_s)
            preset = WEBP_PRESET_PICTURE;
        else if(presetString == "photo"_s)
            preset = WEBP_PRESET_PHOTO;
        else if(presetString == "drawing"_s)
            preset = WEBP_PRESET_DRAWING;
        else if(presetString == "icon"_s)
            preset = WEBP_PRESET_ICON;
        else if(presetString == "text"_s)
            preset = WEBP_PRESET_TEXT;
        else {
            Error{} << "Trade::WebPImageConverter::convertToData(): expected preset to be one of lossless, default, picture, photo, drawing, icon or text but got" << presetString;
            return {};
        }
        if(!WebPConfigPreset(&config, preset, configuration().value<Float>("lossy"))) {
            /** @todo this fails also due to a version mismatch, same as
                above */
            Error{} << "Trade::WebPImageConverter::convertToData(): cannot apply a" << presetString << "preset with quality" << configuration().value<Float>("lossy");
            return {};
        }
    }

    /* Decide how to import the image. If it has no alpha, implicitly set the
       quality to 0. It can still be overriden later. */
    int(*importer)(WebPPicture*, const std::uint8_t*, int);
    switch(image.format()) {
        case PixelFormat::RGB8Unorm:
            importer = WebPPictureImportRGB;
            config.alpha_quality = 0;
            break;
        case PixelFormat::RGBA8Unorm:
            importer = WebPPictureImportRGBA;
            break;
        default:
            Error{} << "Trade::WebPImageConverter::convertToData(): unsupported format" << image.format();
            return {};
    }

    /* Additional options */
    /* Transparent RGB values are implicitly preserved for lossless encoding,
       if not overriden */
    config.exact = configuration().value<Containers::StringView>("exactTransparentRgb") ?
        configuration().value<bool>("exactTransparentRgb") : losslessPreset;
    if(configuration().value<Containers::StringView>("alphaQuality"))
        config.alpha_quality = configuration().value<Int>("alphaQuality");
    if(!WebPValidateConfig(&config)) {
        /* Yeah, libwebp doesn't provide any better error handling than that.
           Expand when more options are added. */
        Error{} << "Trade::WebPImageConverter::convertToData(): option validation failed, check the alphaQuality configuration option";
        return {};
    }

    /* Set up a picture. This can only fail due to a version mismatch. Which
       would happen in WebPConfig*Preset() already, not here. */
    WebPPicture picture;
    CORRADE_INTERNAL_ASSERT_OUTPUT(WebPPictureInit(&picture));
    Containers::ScopeGuard pictureGuard{&picture, WebPPictureFree};
    picture.width = image.size().x();
    picture.height = image.size().y();
    /* ARGB is implicitly used for the lossless preset, if not overriden */
    picture.use_argb = configuration().value<Containers::StringView>("useArgb") ?
        configuration().value<bool>("useArgb") : losslessPreset;

    /* Write the output to a growable array. The WebP builtin WebPMemoryWriter
       does an awful malloc+memcpy+free every time without even trying to use
       realloc() so this is definitely better. */
    Containers::Array<char> data;
    picture.writer = [](const std::uint8_t* data, const std::size_t size, const WebPPicture* picture) {
        arrayAppend(*static_cast<Containers::Array<char>*>(picture->custom_ptr),
            {reinterpret_cast<const char*>(data), size});
        return 1;
    };
    picture.custom_ptr = &data;

    /* Import the RGB / RGBA image. Fortunately it allows a negative stride,
       so we don't need to manually Y-flip the data first. */
    const Containers::StridedArrayView3D<const char> pixels = image.pixels().flipped<0>();
    if(!importer(&picture, reinterpret_cast<const std::uint8_t*>(pixels.data()), pixels.stride()[0])) {
        /* This can happen due to a memory error but also for example when the
           stride is smaller than width. Yeah, sorry, no better error reporting
           than this. */
        Error{} << "Trade::WebPImageConverter::convertToData(): importing an image failed";
        return {};
    }

    if(!WebPEncode(&config, &picture)) {
        Error{} << "Trade::WebPImageConverter::convertToData(): encoding an image failed:" << errorString(picture.error_code);
        return {};
    }

    /* Convert the growable array back to a non-growable with the default
       deleter so we can return it */
    arrayShrink(data);

    /* GCC 4.8 needs extra help here */
    return Containers::optional(Utility::move(data));
}

}}

CORRADE_PLUGIN_REGISTER(WebPImageConverter, Magnum::Trade::WebPImageConverter,
    MAGNUM_TRADE_ABSTRACTIMAGECONVERTER_PLUGIN_INTERFACE)
