/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
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

#include <cstdlib> /* std::free() */
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Utility/Algorithms.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

#ifdef CORRADE_TARGET_CLANG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++11-narrowing"
#endif
#include "miniexr.h"
#ifdef CORRADE_TARGET_CLANG
#pragma GCC diagnostic pop
#endif

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

#ifdef MAGNUM_BUILD_DEPRECATED
MiniExrImageConverter::MiniExrImageConverter() = default; /* LCOV_EXCL_LINE */
#endif

MiniExrImageConverter::MiniExrImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures MiniExrImageConverter::doFeatures() const { return ImageConverterFeature::Convert2DToData; }

Containers::String MiniExrImageConverter::doExtension() const { return "exr"_s; }

Containers::String MiniExrImageConverter::doMimeType() const {
    /* According to https://lists.gnu.org/archive/html/openexr-devel/2014-05/msg00014.html
       there's no registered MIME type, image/x-exr is what `file --mime-type`
       returns as well. */
    return "image/x-exr"_s;
}

Containers::Optional<Containers::Array<char>> MiniExrImageConverter::doConvertToData(const ImageView2D& image) {
    /* Warn about lost metadata */
    if((image.flags() & ImageFlag2D::Array) && !(flags() & ImageConverterFlag::Quiet)) {
        Warning{} << "Trade::MiniExrImageConverter::convertToData(): 1D array images are unrepresentable in OpenEXR, saving as a regular 2D image";
    }

    Int components;
    switch(image.format()) {
        case PixelFormat::RGB16F: components = 3; break;
        case PixelFormat::RGBA16F: components = 4; break;
        default:
            Error() << "Trade::MiniExrImageConverter::convertToData(): unsupported format" << image.format();
            return {};
    }

    /* Copy image pixels to a tightly-packed array with rows flipped.
       Unfortunately there's no way to specify arbitrary strides, for Y
       flipping there's stbi_flip_vertically_on_write() but since we have to
       do a copy anyway we can flip during that as well. */
    Containers::Array<char> flippedPackedData{NoInit, image.pixelSize()*image.size().product()};
    Utility::copy(image.pixels().flipped<0>(), Containers::StridedArrayView3D<char>{flippedPackedData, {
        std::size_t(image.size().y()),
        std::size_t(image.size().x()),
        image.pixelSize(),
    }});

    std::size_t size;
    unsigned char* const data = miniexr_write(image.size().x(), image.size().y(), components, flippedPackedData, &size);
    CORRADE_INTERNAL_ASSERT(data);

    /* miniexr uses malloc to allocate and since we can't use custom deleters,
       copy the result into a new-allocated array instead */
    Containers::Array<char> fileData{InPlaceInit, Containers::arrayView(reinterpret_cast<const char*>(data), size)};
    std::free(data);

    /* GCC 4.8 needs extra help here */
    return Containers::optional(Utility::move(fileData));
}

}}

CORRADE_PLUGIN_REGISTER(MiniExrImageConverter, Magnum::Trade::MiniExrImageConverter,
    MAGNUM_TRADE_ABSTRACTIMAGECONVERTER_PLUGIN_INTERFACE)
