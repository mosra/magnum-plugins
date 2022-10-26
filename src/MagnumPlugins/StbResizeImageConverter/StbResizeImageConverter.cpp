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

#include "StbResizeImageConverter.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/ConfigurationValue.h>
#include <Magnum/Trade/ImageData.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_MAX_CHANNELS 4 /* 64 is the default, no need for that many */
#include "stb_image_resize.h"

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

StbResizeImageConverter::StbResizeImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures StbResizeImageConverter::doFeatures() const {
    return ImageConverterFeature::Convert2D|ImageConverterFeature::Convert3D;
}

namespace {

Containers::Optional<ImageData3D> convertInternal(const ImageView3D& image, Utility::ConfigurationGroup& configuration) {
    /* Image has to be non-empty, otherwise we hit an assertion deep in the
       algorithm. Overriding STBIR_ASSERT() would help neither making the
       failure graceful nor having a human-readable message. */
    if(!image.size().product()) {
        Error{} << "Trade::StbResizeImageConverter::convert(): invalid input image size" << Debug::packed << image.size().xy();
        return {};
    }

    /* Target output size. The final output size depends on wheter upscaling is
       disabled. */
    if(!configuration.value<Containers::StringView>("size")) {
        Error{} << "Trade::StbResizeImageConverter::convert(): output size was not specified";
        return {};
    }
    const Vector2i targetSize = configuration.value<Vector2i>("size");
    if(!targetSize.product()) {
        Error{} << "Trade::StbResizeImageConverter::convert(): invalid output image size" << Debug::packed << targetSize;
        return {};
    }

    /* Actual output size depending on whether upsampling is desired or not */
    const Vector2i size = configuration.value<bool>("upsample") ? targetSize : Vector2i{Math::min(targetSize, image.size().xy())};

    /* Data type and component count. Branching on isPixelFormatDepthOrStencil()
       to avoid having a dedicated error path for depth/stencil formats. */
    stbir_datatype type;
    switch(isPixelFormatDepthOrStencil(image.format()) ? image.format() : pixelFormatChannelFormat(image.format())) {
        case PixelFormat::R8Unorm:
        case PixelFormat::R8Srgb:
            type = STBIR_TYPE_UINT8;
            break;
        case PixelFormat::R16Unorm:
            type = STBIR_TYPE_UINT16;
            break;
        case PixelFormat::R32F:
            type = STBIR_TYPE_FLOAT;
            break;
        /** @todo STBIR_TYPE_UINT32 possibly for resampling depth? */
        default:
            Error{} << "Trade::StbResizeImageConverter::convert(): unsupported format" << image.format();
            return {};
    }
    const Int channelCount = pixelFormatChannelCount(image.format());
    const Int alphaChannelIndex = channelCount == 4 ? 3 : -1;
    const stbir_colorspace colorspace = isPixelFormatSrgb(image.format()) ? STBIR_COLORSPACE_SRGB : STBIR_COLORSPACE_LINEAR;

    /* Flags */
    Int flags{};
    if(configuration.value<bool>("alphaPremultiplied"))
        flags |= STBIR_FLAG_ALPHA_PREMULTIPLIED;
    if(configuration.value<bool>("alphaUsesSrgb"))
        flags |= STBIR_FLAG_ALPHA_USES_COLORSPACE;

    /* Edge mode */
    const Containers::StringView edgeString = configuration.value<Containers::StringView>("edge");
    stbir_edge edge;
    /* LCOV_EXCL_START, it makes no sense to test each and every */
    if(edgeString == "clamp"_s)
        edge = STBIR_EDGE_CLAMP;
    else if(edgeString == "reflect"_s)
        edge = STBIR_EDGE_REFLECT;
    else if(edgeString == "wrap"_s)
        edge = STBIR_EDGE_WRAP;
    else if(edgeString == "zero"_s)
        edge = STBIR_EDGE_ZERO;
    /* LCOV_EXCL_STOP */
    else {
        Error{} << "Trade::StbResizeImageConverter::convert(): expected edge mode to be one of clamp, reflect, wrap or zero, got" << edgeString;
        return {};
    }

    /* Filter */
    const Containers::StringView filterString = configuration.value<Containers::StringView>("filter");
    stbir_filter filter;
    /* LCOV_EXCL_START, it makes no sense to test each and every */
    if(!filterString)
        filter = STBIR_FILTER_DEFAULT;
    else if(filterString == "box"_s)
        filter = STBIR_FILTER_BOX;
    else if(filterString == "triangle"_s)
        filter = STBIR_FILTER_TRIANGLE;
    else if(filterString == "cubicspline"_s)
        filter = STBIR_FILTER_CUBICBSPLINE;
    else if(filterString == "catmullrom"_s)
        filter = STBIR_FILTER_CATMULLROM;
    else if(filterString == "mitchell"_s)
        filter = STBIR_FILTER_MITCHELL;
    /* LCOV_EXCL_STOP */
    else {
        Error{} << "Trade::StbResizeImageConverter::convert(): expected filter to be empty or one of box, triangle, cubicpline, catmullrom or mitchell, got" << filterString;
        return {};
    }

    /* Always align output rows at four bytes */
    const std::size_t stride = 4*((size.x()*image.pixelSize() + 3)/4);
    Trade::ImageData3D out{image.format(), {size, image.size().z()}, Containers::Array<char>{NoInit, stride*size.y()*image.size().z()}, image.flags()};

    const Containers::StridedArrayView4D<const char> srcPixels = image.pixels();
    const Containers::StridedArrayView4D<char> dstPixels = out.mutablePixels();

    /* If the target size is the same as the image size, or if upsampling is
       disabled and the image is smaller (or equal) in both dimensions, just
       copy the data over to avoid needless work and undesired artifacts */
    if(image.size().xy() == targetSize || (!configuration.value<bool>("upsample") && (image.size().xy() <= targetSize).all())) {
        Utility::copy(srcPixels, dstPixels);
        return Containers::optional(std::move(out));
    }

    /* Apart from wrong input (which we check here), the only way this function
       could fail is due to a memory allocation failure. Which is likely only
       when doing some really crazy upsample, and then it'd fail already when
       allocating the output image above. */
    for(std::size_t z = 0, zMax = image.size().z(); z != zMax; ++z) {
        const Containers::StridedArrayView3D<const char> srcPixelLayer = srcPixels[z];
        const Containers::StridedArrayView3D<char> dstPixelLayer = dstPixels[z];
        CORRADE_INTERNAL_ASSERT_OUTPUT(stbir_resize(
            srcPixelLayer.data(), srcPixelLayer.size()[1], srcPixelLayer.size()[0], srcPixelLayer.stride()[0],
            dstPixelLayer.data(), dstPixelLayer.size()[1], dstPixelLayer.size()[0], dstPixelLayer.stride()[0],
            /** @todo option for separate horizontal and vertical filters */
            type, channelCount, alphaChannelIndex, flags, edge, edge, filter, filter, colorspace, nullptr));
    }

    /* GCC 4.8 needs extra help here */
    return Containers::optional(std::move(out));
}

}

Containers::Optional<ImageData2D> StbResizeImageConverter::doConvert(const ImageView2D& image) {
    if(image.flags() & ImageFlag2D::Array) {
        /** @todo or take only the X size instead? then it would make sense to
            provide also a non-array 1D variant */
        Error{} << "Trade::StbResizeImageConverter::convert(): 1D array images are not supported";
        return {};
    }

    Containers::Optional<ImageData3D> out = convertInternal(image, configuration());
    if(!out) return {};

    CORRADE_INTERNAL_ASSERT(out->size().z() == 1);
    const Vector2i size = out->size().xy();
    return ImageData2D{out->format(), size, out->release(), ImageFlag2D(UnsignedShort(out->flags()))};
}

Containers::Optional<ImageData3D> StbResizeImageConverter::doConvert(const ImageView3D& image) {
    if(!(image.flags() & (ImageFlag3D::Array|ImageFlag3D::CubeMap))) {
        Error{} << "Trade::StbResizeImageConverter::convert(): 3D images are not supported";
        return {};
    }

    return convertInternal(image, configuration());
}

}}

CORRADE_PLUGIN_REGISTER(StbResizeImageConverter, Magnum::Trade::StbResizeImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3.3")
