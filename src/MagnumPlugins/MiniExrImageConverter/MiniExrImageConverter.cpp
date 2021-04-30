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

#include "MiniExrImageConverter.h"

#include <algorithm>
#include <Corrade/Containers/Array.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++11-narrowing"
#endif
#include "miniexr.h"
#ifdef __clang__
#pragma GCC diagnostic pop
#endif

namespace Magnum { namespace Trade {

MiniExrImageConverter::MiniExrImageConverter() = default;

MiniExrImageConverter::MiniExrImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures MiniExrImageConverter::doFeatures() const { return ImageConverterFeature::Convert2DToData; }

Containers::Array<char> MiniExrImageConverter::doConvertToData(const ImageView2D& image) {
    Int components;
    switch(image.format()) {
        case PixelFormat::RGB16F: components = 3; break;
        case PixelFormat::RGBA16F: components = 4; break;
        default:
            Error() << "Trade::MiniExrImageConverter::convertToData(): unsupported pixel format" << image.format();
            return nullptr;
    }

    /* Get data properties and calculate the initial slice based on subimage
       offset */
    const std::pair<Math::Vector2<std::size_t>, Math::Vector2<std::size_t>> dataProperties = image.dataProperties();
    Containers::ArrayView<const char> inputData = image.data().suffix(dataProperties.first.sum());

    /* Do Y-flip and tight packing of image data */
    const std::size_t rowSize = image.size().x()*image.pixelSize();
    const std::size_t rowStride = dataProperties.second.x();
    const std::size_t packedDataSize = rowSize*image.size().y();
    Containers::Array<char> reversedPackedData{NoInit, packedDataSize};
    for(std::int_fast32_t y = 0; y != image.size().y(); ++y)
        std::copy_n(inputData.suffix((image.size().y() - y - 1)*rowStride).data(), rowSize, reversedPackedData + y*rowSize);

    std::size_t size;
    unsigned char* const data = miniexr_write(image.size().x(), image.size().y(), components, reversedPackedData, &size);
    CORRADE_INTERNAL_ASSERT(data);

    /* miniexr uses malloc to allocate and since we can't use custom deleters,
       copy the result into a new-allocated array instead */
    Containers::Array<char> fileData{NoInit, size};
    std::copy_n(data, size, fileData.begin());
    std::free(data);

    return fileData;
}

}}

CORRADE_PLUGIN_REGISTER(MiniExrImageConverter, Magnum::Trade::MiniExrImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3")
