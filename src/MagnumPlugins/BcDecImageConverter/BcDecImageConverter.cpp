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

#include "BcDecImageConverter.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

#define BCDEC_IMPLEMENTATION
#include "bcdec.h"

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

BcDecImageConverter::BcDecImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures BcDecImageConverter::doFeatures() const { return ImageConverterFeature::ConvertCompressed2D; }

namespace {

template<void(*decodeBlock)(const void*, void*, int)> void decodeBlocks(const Containers::StridedArrayView2D<const char>& src, const Containers::StridedArrayView2D<char>& dst) {
    const std::size_t yBlocks = src.size()[0];
    const std::size_t xBlocks = src.size()[1];
    CORRADE_INTERNAL_ASSERT(dst.size()[0] == yBlocks*4 &&
                            dst.size()[1] == xBlocks*4);
    const std::size_t dstRowStride = dst.stride()[0];
    for(std::size_t y = 0; y != yBlocks; ++y)
        for(std::size_t x = 0; x != xBlocks; ++x)
            decodeBlock(&src[{y, x}], &dst[{y*4, x*4}], dstRowStride);
}

/* To make bcdec_bc6h_float() / bcdec_bc6h_half() the same signature as the
   others to use in decodeBlocks() above */
template<void(*decodeBlock)(const void*, void*, int, int), bool isSigned, int typeSize> inline void decodeBc6hBlock(const void* src, void* dst, int rowStride) {
    /* The destinationPitch is apparently not bytes but rather depending on
       type size, so here it has to be divided by 2 or 4 */
    decodeBlock(src, dst, rowStride/typeSize, isSigned);
}

}

Containers::Optional<ImageData2D> BcDecImageConverter::doConvert(const CompressedImageView2D& image) {
    const bool bc6hToFloat = configuration().value<bool>("bc6hToFloat");

    /* Decide on target pixel format */
    PixelFormat format;
    switch(image.format()) {
        case CompressedPixelFormat::Bc1RGBUnorm:
        case CompressedPixelFormat::Bc1RGBAUnorm:
        case CompressedPixelFormat::Bc2RGBAUnorm:
        case CompressedPixelFormat::Bc3RGBAUnorm:
        case CompressedPixelFormat::Bc7RGBAUnorm:
            format = PixelFormat::RGBA8Unorm;
            break;
        case CompressedPixelFormat::Bc1RGBSrgb:
        case CompressedPixelFormat::Bc1RGBASrgb:
        case CompressedPixelFormat::Bc2RGBASrgb:
        case CompressedPixelFormat::Bc3RGBASrgb:
        case CompressedPixelFormat::Bc7RGBASrgb:
            format = PixelFormat::RGBA8Srgb;
            break;
        case CompressedPixelFormat::Bc4RUnorm:
            format = PixelFormat::R8Unorm;
            break;
        case CompressedPixelFormat::Bc4RSnorm:
            format = PixelFormat::R8Snorm;
            break;
        case CompressedPixelFormat::Bc5RGUnorm:
            format = PixelFormat::RG8Unorm;
            break;
        case CompressedPixelFormat::Bc5RGSnorm:
            format = PixelFormat::RG8Snorm;
            break;
        case CompressedPixelFormat::Bc6hRGBUfloat:
        case CompressedPixelFormat::Bc6hRGBSfloat:
            format = bc6hToFloat ?
                PixelFormat::RGB32F :
                PixelFormat::RGB16F;
            break;
        default:
            Error{} << "Trade::BcDecImageConverter::convert(): unsupported format" << image.format();
            return {};
    }

    /* Block size is 4x4 in all cases */
    /** @todo clean up once the block size is stored directly in the image */
    constexpr Vector2i blockSize{4};
    CORRADE_INTERNAL_ASSERT(compressedPixelFormatBlockSize(image.format()) == (Vector3i{blockSize, 1}));

    /* Allocate output data. For simplicity make them contain the full 4x4
       blocks with an appropriate row length set. That way, if the actual used
       size isn't whole blocks, the extra unused pixels at the end of each row
       and at/or the end of the image are treated as padding without having
       to do a lot of special casing in the decoding loop. */
    const Vector2i blockCount = ((image.size() + blockSize - Vector2i{1})/blockSize);
    const Vector2i sizeInWholeBlocks = blockSize*blockCount;
    const UnsignedInt pixelSize = pixelFormatSize(format);
    Trade::ImageData2D out{
        /* Since it's always 4-pixel-wide blocks, the alignment can stay at the
           default of 4 */
        PixelStorage{}.setRowLength(sizeInWholeBlocks.x()),
        format,
        image.size(),
        Containers::Array<char>{NoInit, std::size_t(pixelSize*sizeInWholeBlocks.product())},
        image.flags()};

    /* Build the source block view and destination pixel view */
    /** @todo clean up and remove the error once there's a blocks() accessor */
    if(image.storage() != CompressedPixelStorage{}) {
        Error{} << "Trade::BcDecImageConverter::convert(): non-default compressed storage is not supported";
        return {};
    }
    const UnsignedInt blockDataSize = compressedPixelFormatBlockDataSize(image.format());
    const Containers::StridedArrayView2D<const char> src{
        image.data(),
        {std::size_t(blockCount.y()), std::size_t(blockCount.x())},
        {std::ptrdiff_t(blockCount.x()*blockDataSize), std::ptrdiff_t(blockDataSize)}
    };
    /* Can't use pixels() here because the pixel view may not be whole
       blocks */
    const Containers::StridedArrayView2D<char> dst{
        out.mutableData(),
        {std::size_t(sizeInWholeBlocks.y()),
         std::size_t(sizeInWholeBlocks.x())},
        {std::ptrdiff_t(sizeInWholeBlocks.x()*pixelSize),
         std::ptrdiff_t(pixelSize)}
    };

    /* Decode block-by-block */
    switch(image.format()) {
        case CompressedPixelFormat::Bc1RGBUnorm:
        case CompressedPixelFormat::Bc1RGBAUnorm:
        case CompressedPixelFormat::Bc1RGBSrgb:
        case CompressedPixelFormat::Bc1RGBASrgb:
            decodeBlocks<bcdec_bc1>(src, dst);
            break;
        case CompressedPixelFormat::Bc2RGBAUnorm:
        case CompressedPixelFormat::Bc2RGBASrgb:
            decodeBlocks<bcdec_bc2>(src, dst);
            break;
        case CompressedPixelFormat::Bc3RGBAUnorm:
        case CompressedPixelFormat::Bc3RGBASrgb:
            decodeBlocks<bcdec_bc3>(src, dst);
            break;
        case CompressedPixelFormat::Bc4RUnorm:
        case CompressedPixelFormat::Bc4RSnorm:
            decodeBlocks<bcdec_bc4>(src, dst);
            break;
        case CompressedPixelFormat::Bc5RGUnorm:
        case CompressedPixelFormat::Bc5RGSnorm:
            decodeBlocks<bcdec_bc5>(src, dst);
            break;
        case CompressedPixelFormat::Bc6hRGBUfloat:
            bc6hToFloat ?
                decodeBlocks<decodeBc6hBlock<bcdec_bc6h_float, false, 4>>(src, dst) :
                decodeBlocks<decodeBc6hBlock<bcdec_bc6h_half, false, 2>>(src, dst);
            break;
        case CompressedPixelFormat::Bc6hRGBSfloat:
            bc6hToFloat ?
                decodeBlocks<decodeBc6hBlock<bcdec_bc6h_float, true, 4>>(src, dst) :
                decodeBlocks<decodeBc6hBlock<bcdec_bc6h_half, true, 2>>(src, dst);
            break;
        case CompressedPixelFormat::Bc7RGBAUnorm:
        case CompressedPixelFormat::Bc7RGBASrgb:
            decodeBlocks<bcdec_bc7>(src, dst);
            break;
        /* Unsupported formats already handled above */
        default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }

    /* GCC 4.8 needs extra help here */
    return Containers::optional(std::move(out));
}

}}

CORRADE_PLUGIN_REGISTER(BcDecImageConverter, Magnum::Trade::BcDecImageConverter,
    MAGNUM_TRADE_ABSTRACTIMAGECONVERTER_PLUGIN_INTERFACE)
