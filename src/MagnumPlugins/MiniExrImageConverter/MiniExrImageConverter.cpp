/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
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

auto MiniExrImageConverter::doFeatures() const -> Features { return Feature::ConvertData; }

Containers::Array<char> MiniExrImageConverter::doExportToData(const ImageView2D& image) {
    Int components;
    switch(image.format()) {
        case PixelFormat::RGB16F: components = 3; break;
        case PixelFormat::RGBA16F: components = 4; break;
        default:
            Error() << "Trade::MiniExrImageConverter::exportToData(): unsupported pixel format" << image.format();
            return nullptr;
    }

    /* Data properties */
    const std::pair<Math::Vector2<std::size_t>, Math::Vector2<std::size_t>> dataProperties = image.dataProperties();

    /* Image data pointer including skip */
    const char* imageData = image.data() + dataProperties.first.sum();

    /* Do Y-flip and tight packing of image data */
    const std::size_t rowSize = image.size().x()*image.pixelSize();
    const std::size_t rowStride = dataProperties.second.x();
    const std::size_t packedDataSize = rowSize*image.size().y();
    Containers::Array<char> reversedPackedData{packedDataSize};
    for(std::int_fast32_t y = 0; y != image.size().y(); ++y)
        std::copy_n(imageData + (image.size().y() - y - 1)*rowStride, rowSize, reversedPackedData + y*rowSize);

    std::size_t size;
    unsigned char* const data = miniexr_write(image.size().x(), image.size().y(), components, reversedPackedData, &size);
    CORRADE_INTERNAL_ASSERT(data);

    /* Wrap the data in an array with custom deleter (we can't use delete[]) */
    Containers::Array<char> fileData{reinterpret_cast<char*>(data), std::size_t(size),
        [](char* data, std::size_t) { std::free(data); }};

    return fileData;
}

}}

CORRADE_PLUGIN_REGISTER(MiniExrImageConverter, Magnum::Trade::MiniExrImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.2.1")
