/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
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

#include "AvifImporter.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

/* There's also avif_cxx.h but the only thing it does is defining a bunch of
   std::unique_ptr wrappers. Do I want that? Hell no. */
#include <avif/avif.h>

namespace Magnum { namespace Trade {

AvifImporter::AvifImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

AvifImporter::~AvifImporter() = default;

ImporterFeatures AvifImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool AvifImporter::doIsOpened() const { return _in; }

void AvifImporter::doClose() { _in = nullptr; }

void AvifImporter::doOpenData(Containers::Array<char>&& data, const DataFlags dataFlags) {
    /* Because here we're copying the data and using the _in to check if file
       is opened, having them nullptr would mean openData() would fail without
       any error message. It's not possible to do this check on the importer
       side, because empty file is valid in some formats (OBJ or glTF). We also
       can't do the full import here because then doImage2D() would need to
       copy the imported data instead anyway (and the uncompressed size is much
       larger). This way it'll also work nicely with a future openMemory(). */
    if(data.isEmpty()) {
        Error{} << "Trade::AvifImporter::openData(): the file is empty";
        return;
    }

    /* Take over the existing array or copy the data if we can't */
    if(dataFlags & (DataFlag::Owned|DataFlag::ExternallyOwned))
        _in = Utility::move(data);
    else
        _in = Containers::Array<char>{InPlaceInit, data};
}

UnsignedInt AvifImporter::doImage2DCount() const { return 1; }

Containers::Optional<ImageData2D> AvifImporter::doImage2D(UnsignedInt, UnsignedInt) {
    /* With 0 we can use if(avifResult error = avifWhatever) in all cases
       below */
    static_assert(AVIF_RESULT_OK == 0,
        "cannot use AVIF_RESULT_OK in shorthand branches");

    /* Set up the decoder. There's no clear reason this could fail apart from
       memory allocation failure, so just assert it didn't. */
    avifDecoder* const decoder = avifDecoderCreate();
    CORRADE_INTERNAL_ASSERT(decoder);
    const Containers::ScopeGuard decoderGuard{decoder, avifDecoderDestroy};

    /* This can only fail if data is null, which never happens, so just assert
       again. */
    CORRADE_INTERNAL_ASSERT_OUTPUT(avifDecoderSetIOMemory(decoder, reinterpret_cast<std::uint8_t*>(_in.data()), _in.size()) == AVIF_RESULT_OK);

    /* Parse the file header */
    if(const avifResult error = avifDecoderParse(decoder)) {
        Error err;
        err << "Trade::AvifImporter::image2D(): cannot parse file header:" << avifResultToString(error);
        /* The comment says this *might* be empty sometimes, so print it only
           if it's not */
        if(decoder->diag.error[0])
            err << Debug::nospace << ":" << decoder->diag.error;
        return {};
    }

    /* The file can have multiple images. It's not clearly mentioned in the
       documentation (er, header comments) that there's at least one always, so
       assert that just to be sure. */
    CORRADE_INTERNAL_ASSERT(decoder->imageCount >= 1);

    /* Decode the first image */
    if(const avifResult error = avifDecoderNextImage(decoder)) {
        Error err;
        err << "Trade::AvifImporter::image2D(): cannot decode the image:" << avifResultToString(error);
        /* The comment says this *might* be empty sometimes, so print it only
           if it's not */
        if(decoder->diag.error[0])
            err << Debug::nospace << ":" << decoder->diag.error;
        return {};
    }

    /* Set up image defaults and override them below if needed */
    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, decoder->image);
    const Vector2i size{Int(rgb.width), Int(rgb.height)};

    /* 10- and 12-bit images are occupying the low bits and are thus too dark
       when displayed. For those force the bit depth to 16 to have them use the
       whole range. Also, the input format can be just 8, 10 or 12, a value of
       16 is only available as an override. */
    if(rgb.depth == 10 || rgb.depth == 12)
        rgb.depth = 16;
    else CORRADE_INTERNAL_ASSERT(rgb.depth == 8);

    /* Override the format from the default RGBA if needed, and use the logic
       to decide on the target PixelFormat as well. If the original image has a
       YUV 4:0:0 format, it's monochrome. If the original image doesn't have an
       alpha plane, it's just R or RGB. */
    CORRADE_INTERNAL_ASSERT(rgb.format == AVIF_RGB_FORMAT_RGBA);
    PixelFormat format;
    UnsignedInt channelCount;
    #if AVIF_VERSION_MAJOR*100 + AVIF_VERSION_MINOR >= 103
    if(decoder->image->yuvFormat == AVIF_PIXEL_FORMAT_YUV400) {
        if(!decoder->image->alphaPlane) {
            rgb.format = AVIF_RGB_FORMAT_GRAY;
            format = rgb.depth == 8 ? PixelFormat::R8Unorm : PixelFormat::R16Unorm;
            channelCount = 1;
        } else {
            rgb.format = AVIF_RGB_FORMAT_GRAYA;
            format = rgb.depth == 8 ? PixelFormat::RG8Unorm : PixelFormat::RG16Unorm;
            channelCount = 2;
        }
    } else
    #endif
    {
        if(!decoder->image->alphaPlane) {
            rgb.format = AVIF_RGB_FORMAT_RGB;
            format = rgb.depth == 8 ? PixelFormat::RGB8Unorm : PixelFormat::RGB16Unorm;
            channelCount = 3;
        } else {
            rgb.format = AVIF_RGB_FORMAT_RGBA;
            format = rgb.depth == 8 ? PixelFormat::RGBA8Unorm : PixelFormat::RGBA16Unorm;
            channelCount = 4;
        }
    }

    /* Allocate our own image data so we don't need to copy them after just to
       be able to use the default deleter. */
    const std::size_t rowStride = 4*((rgb.width*channelCount*(rgb.depth/8) + 3)/4);
    Trade::ImageData2D image{format, size, Containers::Array<char>{NoInit, rowStride*rgb.height}};
    rgb.pixels = reinterpret_cast<std::uint8_t*>(image.mutableData().data());
    rgb.rowBytes = rowStride;

    /* Decode the image. In this case the only possible error is memory or
       thread allocation failure, which if happens isn't really recoverable
       anyway I guess. All other cases of error returns in the libavif code are
       because "something we delegated to didn't return OK for whatever reason"
       and those functions don't return OK mostly just if they don't have
       correct parameters passed. In other words, questionable coding practices
       and all those should have been assertions in the library itself. So,
       again, just assert again. */
    CORRADE_INTERNAL_ASSERT_OUTPUT(avifImageYUVToRGB(decoder->image, &rgb) == AVIF_RESULT_OK);

    /* Y-flip the image. There's no builtin functionality in libavif, so do it
       in-place on the imported data. */
    Utility::flipInPlace<0>(image.mutablePixels());

    /* Everything went well, return the image. No need to free the avifRGBImage
       in any way as we're allocating our own data. */
    return image;
}

}}

CORRADE_PLUGIN_REGISTER(AvifImporter, Magnum::Trade::AvifImporter,
    MAGNUM_TRADE_ABSTRACTIMPORTER_PLUGIN_INTERFACE)
