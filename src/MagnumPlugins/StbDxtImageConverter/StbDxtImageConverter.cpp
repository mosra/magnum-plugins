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

#include "StbDxtImageConverter.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Magnum/Math/Color.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

#define STB_DXT_IMPLEMENTATION
/* LOL the thing doesn't #include <string.h> on its own, wtf */
#include <cstring>
#include "stb_dxt.h"

namespace Magnum { namespace Trade {

StbDxtImageConverter::StbDxtImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures StbDxtImageConverter::doFeatures() const { return ImageConverterFeature::Convert2D; }

Containers::Optional<ImageData2D> StbDxtImageConverter::doConvert(const ImageView2D& image) {
    const Int flags = configuration().value<bool>("highQuality") ? STB_DXT_HIGHQUAL : STB_DXT_NORMAL;

    /* Decide on the output format */
    /** @todo isPixelFormatSrgb(), pixelFormatChannelCount() helpers, and same
        for compressed pixel formats */
    std::size_t inputChannelCount;
    bool alpha, srgb;
    CompressedPixelFormat outputFormat;
    switch(image.format()) {
        case PixelFormat::RGB8Unorm:
            inputChannelCount = 3;
            alpha = false;
            srgb = false;
            outputFormat = CompressedPixelFormat::Bc1RGBUnorm;
            break;
        case PixelFormat::RGB8Srgb:
            inputChannelCount = 3;
            alpha = false;
            srgb = true;
            outputFormat = CompressedPixelFormat::Bc1RGBSrgb;
            break;
        case PixelFormat::RGBA8Unorm:
            inputChannelCount = 4;
            alpha = true;
            srgb = false;
            outputFormat = CompressedPixelFormat::Bc3RGBAUnorm;
            break;
        case PixelFormat::RGBA8Srgb:
            inputChannelCount = 4;
            alpha = true;
            srgb = true;
            outputFormat = CompressedPixelFormat::Bc3RGBASrgb;
            break;
        /** @todo BC4/5 (single/two channel) -- template the loop? */
        default:
            Error{} << "Trade::StbDxtImageConverter::convert(): unsupported format" << image.format();
            return {};
    }

    /* If the alpha option is set, override the default. Input channel count
       stays the same, of course. */
    if(configuration().value<Containers::StringView>("alpha")) {
        if(configuration().value<bool>("alpha")) {
            alpha = true;
            outputFormat = srgb ?
                CompressedPixelFormat::Bc3RGBASrgb :
                CompressedPixelFormat::Bc3RGBAUnorm;
        } else {
            alpha = false;
            outputFormat = srgb ?
                CompressedPixelFormat::Bc1RGBSrgb :
                CompressedPixelFormat::Bc1RGBUnorm;
        }
    }

    if(!(image.size() % 4).isZero()) {
        Error{} << "Trade::StbDxtImageConverter::convert(): expected size to be divisible by 4, got" << image.size();
        return {};
    }

    const Containers::StridedArrayView3D<const UnsignedByte> input = Containers::arrayCast<const UnsignedByte>(image.pixels());

    const std::size_t outputBlockSize = alpha ? 16 : 8;

    /** @todo use blocks() once the compressed image APIs are done */
    Containers::Array<char> outputData{NoInit, std::size_t(image.size().product()*outputBlockSize/16)};
    const Containers::StridedArrayView2D<UnsignedByte> output{
        Containers::arrayCast<UnsignedByte>(outputData),
        {std::size_t(image.size().y()/4), std::size_t(image.size().x()/4)},
        {std::ptrdiff_t(image.size().x()*outputBlockSize/4), outputBlockSize}
    };

    /* Prepare destination where to copy linearized input data. If the alpha is
       missing in the input, fill it to 255. */
    UnsignedByte inputBlockData[16*4];
    if(inputChannelCount == 3) {
        /* Utility::copy() would work but be a lot more painful in this case */
        for(std::size_t i = 0; i != sizeof(inputBlockData); i += 4)
            inputBlockData[i + 3] = 255;
    }
    const Containers::StridedArrayView3D<UnsignedByte> inputBlock{inputBlockData, {4, 4, inputChannelCount}, {4*4, 4, 1}};

    /* Go through all blocks in the input file, linearize and compress them */
    for(std::size_t y = 0, yMax = image.size().y()/4; y < yMax; ++y) {
        for(std::size_t x = 0, xMax = image.size().x()/4; x < xMax; ++x) {
            /* If the alpha is missing, it'll copy only the RGB values into the
               destination */
            Utility::copy(input.slice({4*y, 4*x, 0}, {4*y + 4, 4*x + 4, inputChannelCount}), inputBlock);

            /* Compress the block */
            stb_compress_dxt_block(&output[y][x], inputBlockData, alpha, flags);
        }
    }

    return ImageData2D{outputFormat, image.size(), std::move(outputData)};
}

}}

CORRADE_PLUGIN_REGISTER(StbDxtImageConverter, Magnum::Trade::StbDxtImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3.2")
