/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023 Vladimír Vondruš <mosra@centrum.cz>

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

#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Utility/Algorithms.h>
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
    if(plugin == "StbBmpImageConverter"_s || plugin == "BmpImageConverter"_s)
        _format = Format::Bmp;
    else if(plugin == "StbHdrImageConverter"_s || plugin == "HdrImageConverter"_s)
        _format = Format::Hdr;
    else if(plugin == "StbJpegImageConverter"_s || plugin == "JpegImageConverter"_s)
        _format = Format::Jpeg;
    else if(plugin == "StbPngImageConverter"_s || plugin == "PngImageConverter"_s)
        _format = Format::Png;
    else if(plugin == "StbTgaImageConverter"_s || plugin == "TgaImageConverter"_s)
        _format = Format::Tga;
    else
        _format = {}; /* Runtime error in doExportToData() */
}

ImageConverterFeatures StbImageConverter::doFeatures() const { return ImageConverterFeature::Convert2DToData; }

Containers::String StbImageConverter::doExtension() const {
    if(_format == Format::Bmp) return "bmp"_s;
    if(_format == Format::Hdr) return "hdr"_s;
    if(_format == Format::Jpeg) return "jpg"_s;
    if(_format == Format::Png) return "png"_s;
    if(_format == Format::Tga) return "tga"_s;

    return {};
}

Containers::String StbImageConverter::doMimeType() const {
    if(_format == Format::Bmp) return "image/bmp"_s;
    if(_format == Format::Hdr) return "image/vnd.radiance"_s;
    if(_format == Format::Jpeg) return "image/jpeg"_s;
    if(_format == Format::Png) return "image/png"_s;
    /* https://en.wikipedia.org/wiki/Truevision_TGA says there's no registered
       MIME type. It probably never will be. Using `file --mime-type` on a TGA
       file returns image/x-tga, so using that here as well. */
    if(_format == Format::Tga) return "image/x-tga"_s;

    return {};
}

Containers::Optional<Containers::Array<char>> StbImageConverter::doConvertToData(const ImageView2D& image) {
    if(_format == Format{}) {
        Error{} << "Trade::StbImageConverter::convertToData(): cannot determine output format (plugin loaded as" << plugin() << Error::nospace << ", use one of the Stb{Bmp,Hdr,Jpeg,Png,Tga}ImageConverter aliases)";
        return {};
    }

    /* Warn about lost metadata */
    if((image.flags() & ImageFlag2D::Array) && !(flags() & ImageConverterFlag::Quiet)) {
        Warning{} << "Trade::StbImageConverter::convertToData(): 1D array images are unrepresentable in any of the formats, saving as a regular 2D image";
    }

    Int components;
    if(_format == Format::Bmp || _format == Format::Jpeg || _format == Format::Png || _format == Format::Tga) {
        switch(image.format()) {
            case PixelFormat::R8Unorm:      components = 1; break;
            case PixelFormat::RG8Unorm:
                if((_format == Format::Bmp || _format == Format::Jpeg) && !(flags() & ImageConverterFlag::Quiet))
                    Warning{} << "Trade::StbImageConverter::convertToData(): ignoring green channel for BMP/JPEG output";
                components = 2;
                break;
            case PixelFormat::RGB8Unorm:    components = 3; break;
            case PixelFormat::RGBA8Unorm:
                if((_format == Format::Bmp || _format == Format::Jpeg) && !(flags() & ImageConverterFlag::Quiet))
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
                if(!(flags() & ImageConverterFlag::Quiet))
                    Warning{} << "Trade::StbImageConverter::convertToData(): ignoring green channel for HDR output";
                components = 2;
                break;
            case PixelFormat::RGB32F:       components = 3; break;
            case PixelFormat::RGBA32F:
                if(!(flags() & ImageConverterFlag::Quiet))
                    Warning{} << "Trade::StbImageConverter::convertToData(): ignoring alpha channel for HDR output";
                components = 4;
                break;
            default:
                Error() << "Trade::StbImageConverter::convertToData():" << image.format() << "is not supported for HDR output";
                return {};
        }
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    /* Copy image pixels to a tightly-packed array with rows reversed.
       Unfortunately there's no way to specify arbitrary strides, for Y
       flipping there's stbi_flip_vertically_on_write() but since we have to
       do a copy anyway we can flip during that as well. */
    Containers::Array<char> flippedPackedData{NoInit, image.pixelSize()*image.size().product()};
    Utility::copy(image.pixels().flipped<0>(), Containers::StridedArrayView3D<char>{flippedPackedData, {
        std::size_t(image.size().y()),
        std::size_t(image.size().x()),
        image.pixelSize(),
    }});

    Containers::Array<char> data;
    const auto writeFunc = [](void* context, void* data, int size) {
        arrayAppend(*static_cast<Containers::Array<char>*>(context), {static_cast<const char*>(data), std::size_t(size)});
    };

    /* All these functions can only fail if the size is zero/negative, if the
       data pointer is null or if allocation fails. Except for the allocation
       failure (which isn't really recoverable as the whole OS is a mess at
       that point anyway) all of them are checked by AbstractImageConverter
       already so it's fine to just assert here. */
    if(_format == Format::Bmp) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(stbi_write_bmp_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, flippedPackedData));
    } else if(_format == Format::Jpeg) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(stbi_write_jpg_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, flippedPackedData, Int(configuration().value<Float>("jpegQuality")*100.0f)));
    } else if(_format == Format::Hdr) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(stbi_write_hdr_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, reinterpret_cast<float*>(flippedPackedData.begin())));
    } else if(_format == Format::Png) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(stbi_write_png_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, flippedPackedData, 0));
    } else if(_format == Format::Tga) {
        CORRADE_INTERNAL_ASSERT_OUTPUT(stbi_write_tga_to_func(writeFunc, &data, image.size().x(), image.size().y(), components, flippedPackedData));
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */

    /* Convert the growable array back to a non-growable with the default
       deleter so we can return it */
    arrayShrink(data);

    /* GCC 4.8 needs extra help here */
    return Containers::optional(std::move(data));
}

bool StbImageConverter::doConvertToFile(const ImageView2D& image, const Containers::StringView filename) {
    /* We don't detect any double extensions yet, so we can normalize just the
       extension. In case we eventually might, it'd have to be split() instead
       to save at least by normalizing just the filename and not the path. */
    const Containers::String normalizedExtension = Utility::String::lowercase(Utility::Path::splitExtension(filename).second());

    /* Save the previous format to restore it back after, detect the format
       from extension if it's not supplied explicitly */
    const Format previousFormat = _format;
    if(_format == Format{}) {
        if(normalizedExtension == ".bmp"_s)
            _format = Format::Bmp;
        else if(normalizedExtension == ".hdr"_s)
            _format = Format::Hdr;
        else if(normalizedExtension == ".jpg"_s ||
                normalizedExtension == ".jpeg"_s ||
                normalizedExtension == ".jpe"_s)
            _format = Format::Jpeg;
        else if(normalizedExtension == ".png"_s)
            _format = Format::Png;
        else if(normalizedExtension == ".tga"_s ||
                normalizedExtension == ".vda"_s ||
                normalizedExtension == ".icb"_s ||
                normalizedExtension ==  ".vst"_s)
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
    MAGNUM_TRADE_ABSTRACTIMAGECONVERTER_PLUGIN_INTERFACE)
