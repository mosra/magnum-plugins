/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Containers/StridedArrayView.h>
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

StbDxtImageConverter::StbDxtImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures StbDxtImageConverter::doFeatures() const { return ImageConverterFeature::Convert2D; }

Containers::Optional<ImageData2D> StbDxtImageConverter::doConvert(const ImageView2D& image) {
    const bool alpha = configuration().value<bool>("alpha");
    const Int flags = configuration().value<bool>("highQuality") ? STB_DXT_HIGHQUAL : STB_DXT_NORMAL;

    /* Decide on the output format */
    CompressedPixelFormat outputFormat;
    switch(image.format()) {
        case PixelFormat::RGBA8Unorm:
            outputFormat = alpha ?
                CompressedPixelFormat::Bc3RGBAUnorm :
                CompressedPixelFormat::Bc1RGBUnorm;
            break;
        case PixelFormat::RGBA8Srgb:
            outputFormat = alpha ?
                CompressedPixelFormat::Bc3RGBASrgb :
                CompressedPixelFormat::Bc1RGBSrgb;
            break;
        /** @todo BC4/5 (single/two channel) -- template the loop? */
        default:
            Error{} << "Trade::StbDxtImageConverter::convert(): unsupported format" << image.format();
            return {};
    }

    if(!(image.size() % 4).isZero()) {
        Error{} << "Trade::StbDxtImageConverter::convert(): expected size to be divisible by 4, got" << image.size();
        return {};
    }

    const Containers::StridedArrayView2D<const Color4ub> input = image.pixels<Color4ub>();

    const std::size_t outputBlockSize = alpha ? 16 : 8;

    /** @todo use blocks() once the compressed image APIs are done */
    Containers::Array<char> outputData{NoInit, std::size_t(image.size().product()*outputBlockSize/16)};
    const Containers::StridedArrayView2D<UnsignedByte> output{
        Containers::arrayCast<UnsignedByte>(outputData),
        {std::size_t(image.size().y()/4), std::size_t(image.size().x()/4)},
        {std::ptrdiff_t(image.size().x()*outputBlockSize/4), outputBlockSize}
    };

    /* Go through all blocks in the input file and compress them */
    for(std::size_t y = 0, yMax = image.size().y()/4; y < yMax; ++y) {
        for(std::size_t x = 0, xMax = image.size().x()/4; x < xMax; ++x) {
            /* Gather pixels in a block together to a linear array */
            Containers::StaticArray<16, Color4ub> inputBlockData{NoInit};
            std::size_t i = 0;
            for(const Containers::StridedArrayView1D<const Color4ub> inputBlockRow: input.slice({4*y, 4*x}, {4*y + 4, 4*x + 4})) {
                for(const Color4ub inputBlockPixel: inputBlockRow)
                    inputBlockData[i++] = inputBlockPixel;
            }
            CORRADE_INTERNAL_ASSERT(i == 16);

            /* Compress the block */
            stb_compress_dxt_block(&output[y][x], reinterpret_cast<const UnsignedByte*>(inputBlockData.data()), alpha, flags);
        }
    }

    return ImageData2D{outputFormat, image.size(), std::move(outputData)};
}

}}

CORRADE_PLUGIN_REGISTER(StbDxtImageConverter, Magnum::Trade::StbDxtImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3")
