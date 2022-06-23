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
    return ImageConverterFeature::Convert2D;
}

Containers::Optional<ImageData2D> StbResizeImageConverter::doConvert(const ImageView2D& image) {
    /* Image has to be non-empty, otherwise we hit an assertion deep in the
       algorithm. Overriding STBIR_ASSERT() would help neither making the
       failure graceful nor having a human-readable message. */
    if(!image.size().product()) {
        Error{} << "Trade::StbResizeImageConverter::convert(): invalid input image size" << Debug::packed << image.size();
        return {};
    }

    /* Output size */
    if(!configuration().value<Containers::StringView>("size")) {
        Error{} << "Trade::StbResizeImageConverter::convert(): output size was not specified";
        return {};
    }
    const Vector2i size = configuration().value<Vector2i>("size");

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
    if(configuration().value<bool>("alphaPremultiplied"))
        flags |= STBIR_FLAG_ALPHA_PREMULTIPLIED;
    if(configuration().value<bool>("alphaUsesSrgb"))
        flags |= STBIR_FLAG_ALPHA_USES_COLORSPACE;

    /* Edge mode */
    const Containers::StringView edgeString = configuration().value<Containers::StringView>("edge");
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
    const Containers::StringView filterString = configuration().value<Containers::StringView>("filter");
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
    Trade::ImageData2D out{image.format(), size, Containers::Array<char>{NoInit, stride*size.y()}};

    const Containers::StridedArrayView3D<const char> srcPixels = image.pixels();
    const Containers::StridedArrayView3D<char> dstPixels = out.mutablePixels();
    /* Apart from wrong input (which we check here), the only way this function
       could fail is due to a memory allocation failure. Which is likely only
       when doing some really crazy upsample, and then it'd fail already when
       allocating the output image above. */
    CORRADE_INTERNAL_ASSERT_OUTPUT(stbir_resize(
        srcPixels.data(), srcPixels.size()[1], srcPixels.size()[0], srcPixels.stride()[0],
        dstPixels.data(), dstPixels.size()[1], dstPixels.size()[0], dstPixels.stride()[0],
        /** @todo option for separate horizontal and vertical filters */
        type, channelCount, alphaChannelIndex, flags, edge, edge, filter, filter, colorspace, nullptr));

    /* GCC 4.8 needs extra help here */
    return Containers::optional(std::move(out));
}

}}

CORRADE_PLUGIN_REGISTER(StbResizeImageConverter, Magnum::Trade::StbResizeImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3.2")
