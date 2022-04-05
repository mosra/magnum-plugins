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

#include "StbImageConverter.h"

#include <algorithm> /* std::copy() */ /** @todo remove */
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Path.h>
#include <Corrade/Utility/String.h>

#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ASSERT CORRADE_INTERNAL_ASSERT
/* Not defining malloc/free, because there's no equivalent for realloc in C++ */
#include "stb_image_write.h"

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

StbImageConverter::StbImageConverter(Format format): _format{format} {
    /* Passing an invalid Format enum is user error, we'll assert on that in
       the convertToData() function */

    /** @todo horrible workaround, fix this properly */
    configuration().setValue("jpegQuality", 0.8f);
}

StbImageConverter::StbImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImageConverter{manager, plugin} {
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

Containers::Optional<Containers::Array<char>> StbImageConverter::doConvertToData(const ImageView2D& image) {
    if(_format == Format{}) {
        Error{} << "Trade::StbImageConverter::convertToData(): cannot determine output format (plugin loaded as" << plugin() << Error::nospace << ", use one of the Stb{Bmp,Hdr,Jpeg,Png,Tga}ImageConverter aliases)";
        return {};
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
                return {};
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
                return {};
        }
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    /* Get data properties and calculate the initial slice based on subimage
       offset */
    const std::pair<Math::Vector2<std::size_t>, Math::Vector2<std::size_t>> dataProperties = image.dataProperties();
    auto inputData = Containers::arrayCast<const unsigned char>(image.data())
        .exceptPrefix(dataProperties.first.sum());

    /* Reverse rows in image data. There is stbi_flip_vertically_on_write() but
       can't use that because the input image might be sparse (having padded
       rows, for example). The copy makes the data tightly packed. */
    Containers::Array<unsigned char> reversedData{NoInit, image.pixelSize()*image.size().product()};
    std::size_t outputStride = image.pixelSize()*image.size().x();
    for(Int y = 0; y != image.size().y(); ++y) {
        auto row = inputData.exceptPrefix(y*dataProperties.second.x()).prefix(outputStride);
        std::copy(row.begin(), row.end(), reversedData.exceptPrefix((image.size().y() - y - 1)*outputStride).begin());
    }

    std::string data;
    const auto writeFunc = [](void* context, void* data, int size) {
        static_cast<std::string*>(context)->append(static_cast<char*>(data), size);
    };

    /* All these functions can only fail if the size is zero/negative, if the
       data pointer is null or if allocation fails. Except for the allocation
       failure (which isn't really recoverable as the whole OS is a mess at
       that point anyway) all of them are checked by AbstractImageConverter
       already so it's fine to just assert here. */
    if(_format == Format::Bmp) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(stbi_write_bmp_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reversedData));
    } else if(_format == Format::Jpeg) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(stbi_write_jpg_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reversedData, Int(configuration().value<Float>("jpegQuality")*100.0f)));
    } else if(_format == Format::Hdr) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(stbi_write_hdr_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reinterpret_cast<float*>(reversedData.begin())));
    } else if(_format == Format::Png) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(stbi_write_png_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reversedData, 0));
    } else if(_format == Format::Tga) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(stbi_write_tga_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reversedData));
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    /* Copy the data into array (I would *love* to have a detach() function on
       std::string) */
    Containers::Array<char> fileData{NoInit, data.size()};
    std::copy(data.begin(), data.end(), fileData.begin());

    /* GCC 4.8 and Clang 3.8 need extra help here */
    return Containers::optional(std::move(fileData));
}

bool StbImageConverter::doConvertToFile(const ImageView2D& image, const Containers::StringView filename) {
    /** @todo once Directory is std::string-free, use splitExtension() */
    const Containers::String normalized = Utility::String::lowercase(filename);

    /* Save the previous format to restore it back after, detect the format
       from extension if it's not supplied explicitly */
    const Format previousFormat = _format;
    if(_format == Format{}) {
        if(normalized.hasSuffix(".bmp"_s))
            _format = Format::Bmp;
        else if(normalized.hasSuffix(".hdr"_s))
            _format = Format::Hdr;
        else if(normalized.hasSuffix(".jpg"_s) ||
                normalized.hasSuffix(".jpeg"_s) ||
                normalized.hasSuffix(".jpe"_s))
            _format = Format::Jpeg;
        else if(normalized.hasSuffix(".png"_s))
            _format = Format::Png;
        else if(normalized.hasSuffix(".tga"_s) ||
                normalized.hasSuffix(".vda"_s) ||
                normalized.hasSuffix(".icb"_s) ||
                normalized.hasSuffix( ".vst"_s))
            _format = Format::Tga;
        else {
            Error{} << "Trade::StbImageConverter::convertToFile(): cannot determine output format for" << Utility::Path::split(filename).second() << "(plugin loaded as" << plugin() << Error::nospace << ", use one of the Stb{Bmp,Hdr,Jpeg,Png,Tga}ImageConverter aliases or a corresponding file extension)";
            return false;
        }
    }

    /* Delegate to the base implementation which calls doConvertToData() */
    const bool out = AbstractImageConverter::doConvertToFile(image, filename);

    /* Restore the previous format and return the result */
    _format = previousFormat;
    return out;
}

}}

CORRADE_PLUGIN_REGISTER(StbImageConverter, Magnum::Trade::StbImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3.2")
