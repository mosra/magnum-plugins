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

#include "StbImageConverter.h"

#include <algorithm>
#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ASSERT CORRADE_INTERNAL_ASSERT
/* Not defining malloc/free, because there's no equivalent for realloc in C++ */
#include "stb_image_write.h"

namespace Magnum { namespace Trade {

StbImageConverter::StbImageConverter(Format format): _format{format} {
    /* Passing an invalid Format enum is user error, we'll assert on that in
       the convertToData() function */

    /** @todo horrible workaround, fix this properly */
    configuration().setValue("jpegQuality", 0.8f);
}

StbImageConverter::StbImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImageConverter{manager, plugin} {
    if(plugin == "StbBmpImageConverter" || plugin == "BmpImageConverter")
        _format = Format::Bmp;
    else if(plugin == "StbHdrImageConverter" || plugin == "HdrImageConverter")
        _format = Format::Hdr;
    else if(plugin == "StbJpegImageConverter" || plugin == "JpegImageConverter")
        _format = Format::Jpeg;
    else if(plugin == "StbPngImageConverter" || plugin == "PngImageConverter")
        _format = Format::Png;
    else if(plugin == "StbTgaImageConverter" || plugin == "TgaImageConverter")
        _format = Format::Tga;
    else
        _format = {}; /* Runtime error in doExportToData() */
}

ImageConverterFeatures StbImageConverter::doFeatures() const { return ImageConverterFeature::Convert2DToData; }

Containers::Array<char> StbImageConverter::doConvertToData(const ImageView2D& image) {
    if(!Int(_format)) {
        Error() << "Trade::StbImageConverter::convertToData(): cannot determine output format (plugin loaded as" << plugin() << Error::nospace << ")";
        return nullptr;
    }

    Int components;
    if(_format == Format::Bmp || _format == Format::Jpeg || _format == Format::Png || _format == Format::Tga) {
        switch(image.format()) {
            case PixelFormat::R8Unorm:      components = 1; break;
            case PixelFormat::RG8Unorm:
                if(_format == Format::Bmp || _format == Format::Jpeg)
                    Warning{} << "Trade::StbImageConverter::convertToData(): ignoring green channel for BMP/JPEG output";
                components = 2;
                break;
            case PixelFormat::RGB8Unorm:    components = 3; break;
            case PixelFormat::RGBA8Unorm:
                if(_format == Format::Bmp || _format == Format::Jpeg)
                    Warning{} << "Trade::StbImageConverter::convertToData(): ignoring alpha channel for BMP/JPEG output";
                components = 4;
                break;
            default:
                Error() << "Trade::StbImageConverter::convertToData():" << image.format() << "is not supported for BMP/JPEG/PNG/TGA output";
                return nullptr;
        }
    } else if(_format == Format::Hdr) {
        switch(image.format()) {
            case PixelFormat::R32F:         components = 1; break;
            case PixelFormat::RG32F:
                Warning{} << "Trade::StbImageConverter::convertToData(): ignoring green channel for HDR output";
                components = 2;
                break;
            case PixelFormat::RGB32F:       components = 3; break;
            case PixelFormat::RGBA32F:
                Warning{} << "Trade::StbImageConverter::convertToData(): ignoring alpha channel for HDR output";
                components = 4;
                break;
            default:
                Error() << "Trade::StbImageConverter::convertToData():" << image.format() << "is not supported for HDR output";
                return nullptr;
        }
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    /* Get data properties and calculate the initial slice based on subimage
       offset */
    const std::pair<Math::Vector2<std::size_t>, Math::Vector2<std::size_t>> dataProperties = image.dataProperties();
    auto inputData = Containers::arrayCast<const unsigned char>(image.data())
        .suffix(dataProperties.first.sum());

    /* Reverse rows in image data. There is stbi_flip_vertically_on_write() but
       can't use that because the input image might be sparse (having padded
       rows, for example). The copy makes the data tightly packed. */
    Containers::Array<unsigned char> reversedData{NoInit, image.pixelSize()*image.size().product()};
    std::size_t outputStride = image.pixelSize()*image.size().x();
    for(Int y = 0; y != image.size().y(); ++y) {
        auto row = inputData.suffix(y*dataProperties.second.x()).prefix(outputStride);
        std::copy(row.begin(), row.end(), reversedData.suffix((image.size().y() - y - 1)*outputStride).begin());
    }

    std::string data;
    const auto writeFunc = [](void* context, void* data, int size) {
        static_cast<std::string*>(context)->append(static_cast<char*>(data), size);
    };

    if(_format == Format::Bmp) {
        if(!stbi_write_bmp_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reversedData)) {
            Error() << "Trade::StbImageConverter::convertToData(): error while writing the BMP file";
            return nullptr;
        }
    } else if(_format == Format::Jpeg) {
        if(!stbi_write_jpg_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reversedData, Int(configuration().value<Float>("jpegQuality")*100.0f))) {
            Error() << "Trade::StbImageConverter::convertToData(): error while writing the JPEG file";
            return nullptr;
        }
    } else if(_format == Format::Hdr) {
        if(!stbi_write_hdr_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reinterpret_cast<float*>(reversedData.begin()))) {
            Error() << "Trade::StbImageConverter::convertToData(): error while writing the HDR file";
            return nullptr;
        }
    } else if(_format == Format::Png) {
        if(!stbi_write_png_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reversedData, 0)) {
            Error() << "Trade::StbImageConverter::convertToData(): error while writing the PNG file";
            return nullptr;
        }
    } else if(_format == Format::Tga) {
        if(!stbi_write_tga_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reversedData)) {
            Error() << "Trade::StbImageConverter::convertToData(): error while writing the TGA file";
            return nullptr;
        }
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    /* Copy the data into array (I would *love* to have a detach() function on
       std::string) */
    Containers::Array<char> fileData{NoInit, data.size()};
    std::copy(data.begin(), data.end(), fileData.begin());
    return fileData;
}

}}

CORRADE_PLUGIN_REGISTER(StbImageConverter, Magnum::Trade::StbImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3")
