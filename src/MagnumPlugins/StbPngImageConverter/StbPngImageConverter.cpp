/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015
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

#include "StbPngImageConverter.h"

#include <Corrade/Containers/Array.h>

#include "Magnum/ColorFormat.h"
#include "Magnum/Image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace Magnum { namespace Trade {

StbPngImageConverter::StbPngImageConverter() = default;

StbPngImageConverter::StbPngImageConverter(PluginManager::AbstractManager& manager, std::string plugin): AbstractImageConverter(manager, std::move(plugin)) {}

auto StbPngImageConverter::doFeatures() const -> Features { return Feature::ConvertData; }

Containers::Array<char> StbPngImageConverter::doExportToData(const ImageReference2D& image) const {
    if(image.type() != ColorType::UnsignedByte) {
        Error() << "Trade::StbPngImageConverter::exportToData(): unsupported color type" << image.type();
        return nullptr;
    }

    Int components;
    switch(image.format()) {
        case ColorFormat::Red: components = 1; break;
        case ColorFormat::RG: components = 2; break;
        case ColorFormat::RGB: components = 3; break;
        case ColorFormat::RGBA: components = 4; break;
        default:
            Error() << "Trade::StbPngImageConverter::exportToData(): unsupported color format" << image.format();
            return nullptr;
    }

    /* Reverse rows in image data */
    Containers::Array<unsigned char> reversedData{std::size_t(image.size().product()*components)};
    for(Int y = 0; y != image.size().y(); ++y) {
        const Int stride = image.size().x()*components;
        std::copy(image.data<unsigned char>() + y*stride, image.data<unsigned char>() + (y + 1)*stride, reversedData + (image.size().y() - y - 1)*stride);
    }

    Int size;
    unsigned char* const data = stbi_write_png_to_mem(reversedData, 0, image.size().x(), image.size().y(), components, &size);
    CORRADE_INTERNAL_ASSERT(data);

    /* Copy the data to a new[]-allocated array so we can delete[] it later,
       then delete the original data with free() */
    Containers::Array<char> fileData{std::size_t(size)};
    std::copy(data, data + size, fileData.begin());
    std::free(data);

    return fileData;
}

}}
