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

ImageConverterFeatures StbDxtImageConverter::doFeatures() const {
    return ImageConverterFeature::Convert2D|ImageConverterFeature::Convert3D;
}

namespace {

Containers::Optional<ImageData3D> convertInternal(const ImageView3D& image, Utility::ConfigurationGroup& configuration) {
    const Int flags = configuration.value<bool>("highQuality") ? STB_DXT_HIGHQUAL : STB_DXT_NORMAL;

    /* Decide on the output format */
    CompressedPixelFormat outputFormat;
    switch(image.format()) {
        case PixelFormat::RGB8Unorm:
            outputFormat = CompressedPixelFormat::Bc1RGBUnorm;
            break;
        case PixelFormat::RGB8Srgb:
            outputFormat = CompressedPixelFormat::Bc1RGBSrgb;
            break;
        case PixelFormat::RGBA8Unorm:
            outputFormat = CompressedPixelFormat::Bc3RGBAUnorm;
            break;
        case PixelFormat::RGBA8Srgb:
            outputFormat = CompressedPixelFormat::Bc3RGBASrgb;
            break;
        /** @todo BC4/5 (single/two channel) -- template the loop? */
        default:
            Error{} << "Trade::StbDxtImageConverter::convert(): unsupported format" << image.format();
            return {};
    }
    const UnsignedInt inputChannelCount = pixelFormatChannelCount(image.format());
    const bool srgb = isPixelFormatSrgb(image.format());
    bool alpha = inputChannelCount == 4;

    /* If the alpha option is set, override the default. Input channel count
       stays the same, of course. */
    if(configuration.value<Containers::StringView>("alpha")) {
        if(configuration.value<bool>("alpha")) {
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

    if(!(image.size().xy() % 4).isZero()) {
        Error{} << "Trade::StbDxtImageConverter::convert(): expected size to be divisible by 4, got" << image.size().xy();
        return {};
    }

    const Containers::StridedArrayView4D<const UnsignedByte> input = Containers::arrayCast<const UnsignedByte>(image.pixels());

    const std::size_t outputBlockSize = alpha ? 16 : 8;

    /** @todo use blocks() once the compressed image APIs are done */
    Containers::Array<char> outputData{NoInit, std::size_t(image.size().product()*outputBlockSize/16)};
    const Containers::StridedArrayView4D<UnsignedByte> output{
        Containers::arrayCast<UnsignedByte>(outputData),
        {std::size_t(image.size().z()),
         std::size_t(image.size().y()/4),
         std::size_t(image.size().x()/4),
         outputBlockSize}
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
    for(std::size_t z = 0, zMax = input.size()[0]; z != zMax; ++z) {
        const Containers::StridedArrayView3D<const UnsignedByte> inputLayer = input[z];
        const Containers::StridedArrayView3D<UnsignedByte> outputLayer = output[z];
        for(std::size_t y = 0, yMax = input.size()[1]/4; y < yMax; ++y) {
            const Containers::StridedArrayView2D<UnsignedByte> outputRow = outputLayer[y];
            for(std::size_t x = 0, xMax = input.size()[2]/4; x < xMax; ++x) {
                /* If the alpha is missing, it'll copy only the RGB values into
                   the destination */
                Utility::copy(inputLayer.slice({4*y, 4*x, 0}, {4*y + 4, 4*x + 4, inputChannelCount}), inputBlock);

                /* Compress the block */
                stb_compress_dxt_block(&outputRow[x][0], inputBlockData, alpha, flags);
            }
        }
    }

    return ImageData3D{outputFormat, image.size(), std::move(outputData), image.flags()};
}

}

Containers::Optional<ImageData2D> StbDxtImageConverter::doConvert(const ImageView2D& image) {
    if(image.flags() & ImageFlag2D::Array) {
        Error{} << "Trade::StbDxtImageConverter::convert(): 1D array images are not supported";
        return {};
    }

    Containers::Optional<ImageData3D> out = convertInternal(image, configuration());
    if(!out) return {};

    CORRADE_INTERNAL_ASSERT(out->size().z() == 1);
    const Vector2i size = out->size().xy();
    return ImageData2D{out->compressedFormat(), size, out->release(), image.flags()};
}

Containers::Optional<ImageData3D> StbDxtImageConverter::doConvert(const ImageView3D& image) {
    return convertInternal(image, configuration());
}

}}

CORRADE_PLUGIN_REGISTER(StbDxtImageConverter, Magnum::Trade::StbDxtImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3.3")
