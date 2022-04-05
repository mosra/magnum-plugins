/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2021 Pablo Escobar <mail@rvrs.in>

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

#include "KtxImageConverter.h"

#include <string>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/EndiannessBatch.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector3.h>

#include "MagnumPlugins/KtxImporter/KtxHeader.h"

namespace Magnum { namespace Trade {

namespace {

/* Most of the following code is needed for generating a Data Format Descriptor
   embedded in the file. Because Magnum itself doesn't care about the DFD and
   relies just on the format enum, the ideal way would be to have a
   pre-generated DFD blob for each format and then just copy that. However:

    -   There is no upstream-maintained way to generate such data. The KDF spec
        has a format.json (https://github.com/KhronosGroup/KTX-Specification/blob/master/formats.json)
        but that doesn't contain any information on how to fill the DFD. Then
        there's Khronos' own dfdutils (https://github.com/KhronosGroup/KTX-Software/tree/master/lib/dfdutils)
        but that generates headers through Perl scripts (a hell for long-term
        maintenance hell), and the headers need the original VkFormat enum to
        be defined. Finally, there's a testbidirectionalmapping.c that can be
        patched and abused to populate the DFD information, however we can't
        rely on it being useful long-term either.
    -   The generated DFD data for all formats are over 12 kB, which could
        become a deal breaker for embedding KtxImageConverter in Emscripten
        apps -- the same done with code should *hopefully* be smaller.

   Thus to ensure we're always able to add new formats, the information gets
   populated manually, using information directly from the KDF spec:
    https://www.khronos.org/registry/DataFormat/specs/1.3/dataformat.1.3.html#CompressedFormatModels
   On the other hand, to minimize the chance of error, the tests verify against ground-truth DFDs created using the testbidirectionalmapping.c utility.
   Worst case, if that one changes and we can't generate new formats with it
   anymore, we only would have certain DFDs untested, but it won't block us
   from adding new formats. */

/* Overloaded functions to use different pixel formats in templated code */
bool isFormatImplementationSpecific(PixelFormat format) {
    return isPixelFormatImplementationSpecific(format);
}

bool isFormatImplementationSpecific(CompressedPixelFormat format) {
    return isCompressedPixelFormatImplementationSpecific(format);
}

typedef Containers::Pair<Implementation::VkFormat, Implementation::VkFormatSuffix> FormatPair;

FormatPair vulkanFormat(PixelFormat format) {
    switch(format) {
        #define _c(vulkan, magnum, type) case PixelFormat::magnum: \
            return {vulkan, Implementation::VkFormatSuffix::type};
        #include "MagnumPlugins/KtxImporter/formatMapping.hpp"
        #undef _c
        default:
            return {{}, {}};
    }
}

FormatPair vulkanFormat(CompressedPixelFormat format) {
    /* In Vulkan there is no distinction between RGB and RGBA PVRTC:
       https://github.com/KhronosGroup/Vulkan-Docs/issues/512#issuecomment-307768667
       compressedFormatMapping.hpp (generated from Vk::PixelFormat) contains
       the RGBA variants, so we manually alias them here. We can't do this
       inside compressedFormatMapping.hpp because both Magnum and Vulkan
       formats must be unique for switch cases. */
    switch(format) {
        case CompressedPixelFormat::PvrtcRGB2bppUnorm:
            format = CompressedPixelFormat::PvrtcRGBA2bppUnorm;
            break;
        case CompressedPixelFormat::PvrtcRGB2bppSrgb:
            format = CompressedPixelFormat::PvrtcRGBA2bppSrgb;
            break;
        case CompressedPixelFormat::PvrtcRGB4bppUnorm:
            format = CompressedPixelFormat::PvrtcRGBA4bppUnorm;
            break;
        case CompressedPixelFormat::PvrtcRGB4bppSrgb:
            format = CompressedPixelFormat::PvrtcRGBA4bppSrgb;
            break;
        default:
            break;
    }

    switch(format) {
        #define _c(vulkan, magnum, type) case CompressedPixelFormat::magnum: \
            return {vulkan, Implementation::VkFormatSuffix::type};
        #include "MagnumPlugins/KtxImporter/compressedFormatMapping.hpp"
        #undef _c
        default:
            return {{}, {}};
    }
}

Vector3i formatUnitSize(PixelFormat) {
    return {1, 1, 1};
}

Vector3i formatUnitSize(CompressedPixelFormat format) {
    return compressedBlockSize(format);
}

UnsignedInt formatUnitDataSize(PixelFormat format) {
    return pixelSize(format);
}

UnsignedInt formatUnitDataSize(CompressedPixelFormat format) {
    return compressedBlockDataSize(format);
}

UnsignedByte formatTypeSize(PixelFormat format) {
    switch(format) {
        case PixelFormat::R8Unorm:
        case PixelFormat::RG8Unorm:
        case PixelFormat::RGB8Unorm:
        case PixelFormat::RGBA8Unorm:
        case PixelFormat::R8Snorm:
        case PixelFormat::RG8Snorm:
        case PixelFormat::RGB8Snorm:
        case PixelFormat::RGBA8Snorm:
        case PixelFormat::R8Srgb:
        case PixelFormat::RG8Srgb:
        case PixelFormat::RGB8Srgb:
        case PixelFormat::RGBA8Srgb:
        case PixelFormat::R8UI:
        case PixelFormat::RG8UI:
        case PixelFormat::RGB8UI:
        case PixelFormat::RGBA8UI:
        case PixelFormat::R8I:
        case PixelFormat::RG8I:
        case PixelFormat::RGB8I:
        case PixelFormat::RGBA8I:
        case PixelFormat::Stencil8UI:
            return 1;
        case PixelFormat::R16Unorm:
        case PixelFormat::RG16Unorm:
        case PixelFormat::RGB16Unorm:
        case PixelFormat::RGBA16Unorm:
        case PixelFormat::R16Snorm:
        case PixelFormat::RG16Snorm:
        case PixelFormat::RGB16Snorm:
        case PixelFormat::RGBA16Snorm:
        case PixelFormat::R16UI:
        case PixelFormat::RG16UI:
        case PixelFormat::RGB16UI:
        case PixelFormat::RGBA16UI:
        case PixelFormat::R16I:
        case PixelFormat::RG16I:
        case PixelFormat::RGB16I:
        case PixelFormat::RGBA16I:
        case PixelFormat::R16F:
        case PixelFormat::RG16F:
        case PixelFormat::RGB16F:
        case PixelFormat::RGBA16F:
        case PixelFormat::Depth16Unorm:
        case PixelFormat::Depth16UnormStencil8UI:
            return 2;
        case PixelFormat::R32UI:
        case PixelFormat::RG32UI:
        case PixelFormat::RGB32UI:
        case PixelFormat::RGBA32UI:
        case PixelFormat::R32I:
        case PixelFormat::RG32I:
        case PixelFormat::RGB32I:
        case PixelFormat::RGBA32I:
        case PixelFormat::R32F:
        case PixelFormat::RG32F:
        case PixelFormat::RGB32F:
        case PixelFormat::RGBA32F:
        case PixelFormat::Depth24Unorm:
        case PixelFormat::Depth32F:
        case PixelFormat::Depth24UnormStencil8UI:
        case PixelFormat::Depth32FStencil8UI:
            return 4;
    }

    CORRADE_ASSERT_UNREACHABLE("componentSize(): unsupported format" << format, {}); /* LCOV_EXCL_LINE */
}

UnsignedByte formatTypeSize(CompressedPixelFormat) {
    return 1;
}

struct SampleData {
    UnsignedShort bitOffset;
    UnsignedShort bitLength;
    Implementation::KdfBasicBlockSample::ChannelId id;
    /* For pixel formats where not all channels share the same suffix (only
       combined depth + stencil for now) we have to specify it manually */
    /** @todo Is there a good way to automate this in formatMapping.hpp? */
    Implementation::VkFormatSuffix suffix;
};

Containers::Pair<Implementation::KdfBasicBlockHeader::ColorModel, Containers::ArrayView<const SampleData>> samples(PixelFormat format) {
    constexpr auto ColorModel = Implementation::KdfBasicBlockHeader::ColorModel::Rgbsda;

    /* We later multiply the offset and length by the type size. This works as
       long as the channels are all the same size. If PixelFormat ever supports
       formats like R10G10B10A2 this needs to be changed. For depth formats
       this assumption already doesn't hold, so we have to specialize and
       later code needs to make sure to not multiply by the type size. */
    static constexpr SampleData SamplesRgba[]{
        {0, 8, Implementation::KdfBasicBlockSample::ChannelId::Red, Implementation::VkFormatSuffix{}},
        {8, 8, Implementation::KdfBasicBlockSample::ChannelId::Green, Implementation::VkFormatSuffix{}},
        {16, 8, Implementation::KdfBasicBlockSample::ChannelId::Blue, Implementation::VkFormatSuffix{}},
        {24, 8, Implementation::KdfBasicBlockSample::ChannelId::Alpha, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesDepth16Stencil[]{
        {0, 16, Implementation::KdfBasicBlockSample::ChannelId::Depth, Implementation::VkFormatSuffix::UNORM},
        {16, 8, Implementation::KdfBasicBlockSample::ChannelId::Stencil, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesDepth24Stencil[]{
        {0, 24, Implementation::KdfBasicBlockSample::ChannelId::Depth, Implementation::VkFormatSuffix::UNORM},
        {24, 8, Implementation::KdfBasicBlockSample::ChannelId::Stencil, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesDepth32FStencil[]{
        {0, 32, Implementation::KdfBasicBlockSample::ChannelId::Depth, Implementation::VkFormatSuffix::SFLOAT},
        {32, 8, Implementation::KdfBasicBlockSample::ChannelId::Stencil, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesStencil[]{
        {0, 8, Implementation::KdfBasicBlockSample::ChannelId::Stencil, Implementation::VkFormatSuffix{}}
    };

    switch(format) {
        case PixelFormat::Stencil8UI:
            return {ColorModel, SamplesStencil};
        case PixelFormat::Depth16Unorm:
            return {ColorModel, Containers::arrayView(SamplesDepth16Stencil).prefix(1)};
        case PixelFormat::Depth16UnormStencil8UI:
            return {ColorModel, SamplesDepth16Stencil};
        case PixelFormat::Depth24Unorm:
            return {ColorModel, Containers::arrayView(SamplesDepth24Stencil).prefix(1)};
        case PixelFormat::Depth24UnormStencil8UI:
            return {ColorModel, SamplesDepth24Stencil};
        case PixelFormat::Depth32F:
            return {ColorModel, Containers::arrayView(SamplesDepth32FStencil).prefix(1)};
        case PixelFormat::Depth32FStencil8UI:
            return {ColorModel, SamplesDepth32FStencil};
        default: {
            const UnsignedInt size = pixelSize(format);
            const UnsignedInt typeSize = formatTypeSize(format);
            CORRADE_INTERNAL_ASSERT(size%typeSize == 0);
            const UnsignedInt numChannels = size/typeSize;
            return {ColorModel, Containers::arrayView(SamplesRgba).prefix(numChannels)};
        }
    }
}

Containers::Pair<Implementation::KdfBasicBlockHeader::ColorModel, Containers::ArrayView<const SampleData>> samples(CompressedPixelFormat format) {
    static constexpr SampleData SamplesBc1[]{
        {0, 64, Implementation::KdfBasicBlockSample::ChannelId::Color, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesBc1AlphaPunchThrough[]{
        {0, 64, Implementation::KdfBasicBlockSample::ChannelId::Bc1Alpha, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesBc2And3[]{
        {0,  64, Implementation::KdfBasicBlockSample::ChannelId::Alpha, Implementation::VkFormatSuffix{}},
        {64, 64, Implementation::KdfBasicBlockSample::ChannelId::Color, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesBc4[]{
        {0, 64, Implementation::KdfBasicBlockSample::ChannelId::Color, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesBc4Signed[]{
        {0, 64, Implementation::KdfBasicBlockSample::ChannelId::Color, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesBc5[]{
        {0,  64, Implementation::KdfBasicBlockSample::ChannelId::Red, Implementation::VkFormatSuffix{}},
        {64, 64, Implementation::KdfBasicBlockSample::ChannelId::Green, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesBc5Signed[]{
        {0,  64, Implementation::KdfBasicBlockSample::ChannelId::Red, Implementation::VkFormatSuffix{}},
        {64, 64, Implementation::KdfBasicBlockSample::ChannelId::Green, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesBc6h[]{
        {0, 128, Implementation::KdfBasicBlockSample::ChannelId::Color, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesBc6hSigned[]{
        {0, 128, Implementation::KdfBasicBlockSample::ChannelId::Color, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesBc7[]{
        {0, 128, Implementation::KdfBasicBlockSample::ChannelId::Color, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesEacR11[]{
        {0, 64, Implementation::KdfBasicBlockSample::ChannelId::Red, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesEacR11Signed[]{
        {0, 64, Implementation::KdfBasicBlockSample::ChannelId::Red, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesEacRG11[]{
        {0,  64, Implementation::KdfBasicBlockSample::ChannelId::Red, Implementation::VkFormatSuffix{}},
        {64, 64, Implementation::KdfBasicBlockSample::ChannelId::Green, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesEacRG11Signed[]{
        {0,  64, Implementation::KdfBasicBlockSample::ChannelId::Red, Implementation::VkFormatSuffix{}},
        {64, 64, Implementation::KdfBasicBlockSample::ChannelId::Green, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesEtc2[]{
        {0, 64, Implementation::KdfBasicBlockSample::ChannelId::Etc2Color, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesEtc2AlphaPunchThrough[]{
        /* Both samples have the same offset, the KDF spec wants it that way.
           BC1 indicates punch-through alpha with a different channel id,
           but ETC2 is special. */
        {0, 64, Implementation::KdfBasicBlockSample::ChannelId::Etc2Color, Implementation::VkFormatSuffix{}},
        {0, 64, Implementation::KdfBasicBlockSample::ChannelId::Alpha, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesEtc2Alpha[]{
        {0,  64, Implementation::KdfBasicBlockSample::ChannelId::Alpha, Implementation::VkFormatSuffix{}},
        {64, 64, Implementation::KdfBasicBlockSample::ChannelId::Etc2Color, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesAstc[]{
        {0, 128, Implementation::KdfBasicBlockSample::ChannelId::Color, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesAstcHdr[]{
        {0, 128, Implementation::KdfBasicBlockSample::ChannelId::Color, Implementation::VkFormatSuffix{}}
    };
    static constexpr SampleData SamplesPvrtc[]{
        {0, 64, Implementation::KdfBasicBlockSample::ChannelId::Color, Implementation::VkFormatSuffix{}}
    };

    switch(format) {
        case CompressedPixelFormat::Bc1RGBUnorm:
        case CompressedPixelFormat::Bc1RGBSrgb:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Bc1, SamplesBc1};
        case CompressedPixelFormat::Bc1RGBAUnorm:
        case CompressedPixelFormat::Bc1RGBASrgb:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Bc1, SamplesBc1AlphaPunchThrough};
        case CompressedPixelFormat::Bc2RGBAUnorm:
        case CompressedPixelFormat::Bc2RGBASrgb:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Bc2, SamplesBc2And3};
        case CompressedPixelFormat::Bc3RGBAUnorm:
        case CompressedPixelFormat::Bc3RGBASrgb:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Bc3, SamplesBc2And3};
        case CompressedPixelFormat::Bc4RUnorm:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Bc4, SamplesBc4};
        case CompressedPixelFormat::Bc4RSnorm:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Bc4, SamplesBc4Signed};
        case CompressedPixelFormat::Bc5RGUnorm:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Bc5, SamplesBc5};
        case CompressedPixelFormat::Bc5RGSnorm:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Bc5, SamplesBc5Signed};
        case CompressedPixelFormat::Bc6hRGBUfloat:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Bc6h, SamplesBc6h};
        case CompressedPixelFormat::Bc6hRGBSfloat:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Bc6h, SamplesBc6hSigned};
        case CompressedPixelFormat::Bc7RGBAUnorm:
        case CompressedPixelFormat::Bc7RGBASrgb:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Bc7, SamplesBc7};
        case CompressedPixelFormat::EacR11Unorm:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Etc2, SamplesEacR11};
        case CompressedPixelFormat::EacR11Snorm:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Etc2, SamplesEacR11Signed};
        case CompressedPixelFormat::EacRG11Unorm:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Etc2, SamplesEacRG11};
        case CompressedPixelFormat::EacRG11Snorm:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Etc2, SamplesEacRG11Signed};
        case CompressedPixelFormat::Etc2RGB8Unorm:
        case CompressedPixelFormat::Etc2RGB8Srgb:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Etc2, SamplesEtc2};
        case CompressedPixelFormat::Etc2RGB8A1Unorm:
        case CompressedPixelFormat::Etc2RGB8A1Srgb:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Etc2, SamplesEtc2AlphaPunchThrough};
        case CompressedPixelFormat::Etc2RGBA8Unorm:
        case CompressedPixelFormat::Etc2RGBA8Srgb:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Etc2, SamplesEtc2Alpha};
        case CompressedPixelFormat::Astc4x4RGBAUnorm:
        case CompressedPixelFormat::Astc4x4RGBASrgb:
        case CompressedPixelFormat::Astc5x4RGBAUnorm:
        case CompressedPixelFormat::Astc5x4RGBASrgb:
        case CompressedPixelFormat::Astc5x5RGBAUnorm:
        case CompressedPixelFormat::Astc5x5RGBASrgb:
        case CompressedPixelFormat::Astc6x5RGBAUnorm:
        case CompressedPixelFormat::Astc6x5RGBASrgb:
        case CompressedPixelFormat::Astc6x6RGBAUnorm:
        case CompressedPixelFormat::Astc6x6RGBASrgb:
        case CompressedPixelFormat::Astc8x5RGBAUnorm:
        case CompressedPixelFormat::Astc8x5RGBASrgb:
        case CompressedPixelFormat::Astc8x6RGBAUnorm:
        case CompressedPixelFormat::Astc8x6RGBASrgb:
        case CompressedPixelFormat::Astc8x8RGBAUnorm:
        case CompressedPixelFormat::Astc8x8RGBASrgb:
        case CompressedPixelFormat::Astc10x5RGBAUnorm:
        case CompressedPixelFormat::Astc10x5RGBASrgb:
        case CompressedPixelFormat::Astc10x6RGBAUnorm:
        case CompressedPixelFormat::Astc10x6RGBASrgb:
        case CompressedPixelFormat::Astc10x8RGBAUnorm:
        case CompressedPixelFormat::Astc10x8RGBASrgb:
        case CompressedPixelFormat::Astc10x10RGBAUnorm:
        case CompressedPixelFormat::Astc10x10RGBASrgb:
        case CompressedPixelFormat::Astc12x10RGBAUnorm:
        case CompressedPixelFormat::Astc12x10RGBASrgb:
        case CompressedPixelFormat::Astc12x12RGBAUnorm:
        case CompressedPixelFormat::Astc12x12RGBASrgb:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Astc, SamplesAstc};
        case CompressedPixelFormat::Astc4x4RGBAF:
        case CompressedPixelFormat::Astc5x4RGBAF:
        case CompressedPixelFormat::Astc5x5RGBAF:
        case CompressedPixelFormat::Astc6x5RGBAF:
        case CompressedPixelFormat::Astc6x6RGBAF:
        case CompressedPixelFormat::Astc8x5RGBAF:
        case CompressedPixelFormat::Astc8x6RGBAF:
        case CompressedPixelFormat::Astc8x8RGBAF:
        case CompressedPixelFormat::Astc10x5RGBAF:
        case CompressedPixelFormat::Astc10x6RGBAF:
        case CompressedPixelFormat::Astc10x8RGBAF:
        case CompressedPixelFormat::Astc10x10RGBAF:
        case CompressedPixelFormat::Astc12x10RGBAF:
        case CompressedPixelFormat::Astc12x12RGBAF:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Astc, SamplesAstcHdr};
        /* 3D ASTC formats are not exposed in Vulkan */
        case CompressedPixelFormat::PvrtcRGB2bppUnorm:
        case CompressedPixelFormat::PvrtcRGB2bppSrgb:
        case CompressedPixelFormat::PvrtcRGBA2bppUnorm:
        case CompressedPixelFormat::PvrtcRGBA2bppSrgb:
        case CompressedPixelFormat::PvrtcRGB4bppUnorm:
        case CompressedPixelFormat::PvrtcRGB4bppSrgb:
        case CompressedPixelFormat::PvrtcRGBA4bppUnorm:
        case CompressedPixelFormat::PvrtcRGBA4bppSrgb:
            return {Implementation::KdfBasicBlockHeader::ColorModel::Pvrtc, SamplesPvrtc};
        default:
            /* Default switch to suppress warnings about unhandled 3D ASTC formats */
            break; /* LCOV_EXCL_LINE */
    }

    CORRADE_ASSERT_UNREACHABLE("samples(): unsupported format" << format, {}); /* LCOV_EXCL_LINE */
}

UnsignedByte channelFormat(Implementation::VkFormatSuffix suffix) {
    switch(suffix) {
        case Implementation::VkFormatSuffix::UNORM:
            return {};
        case Implementation::VkFormatSuffix::SNORM:
            return Implementation::KdfBasicBlockSample::ChannelFormat::Signed;
        case Implementation::VkFormatSuffix::UINT:
            return {};
        case Implementation::VkFormatSuffix::SINT:
            return Implementation::KdfBasicBlockSample::ChannelFormat::Signed;
        case Implementation::VkFormatSuffix::UFLOAT:
            return Implementation::KdfBasicBlockSample::ChannelFormat::Float;
        case Implementation::VkFormatSuffix::SFLOAT:
            return Implementation::KdfBasicBlockSample::ChannelFormat::Float |
                Implementation::KdfBasicBlockSample::ChannelFormat::Signed;
        case Implementation::VkFormatSuffix::SRGB:
            return {};
    }

    CORRADE_ASSERT_UNREACHABLE("channelFormat(): invalid format suffix" << UnsignedInt(suffix), {}); /* LCOV_EXCL_LINE */
}

Containers::Pair<UnsignedInt, UnsignedInt> channelMapping(Implementation::VkFormatSuffix suffix, UnsignedInt bitLength, bool isCompressed) {
    /* sampleLower and sampleUpper define how to interpret the range of values
       found in a channel.
       samplerLower = black value or -1 for signed values
       samplerUpper = white value or 1 for signed values

       There are a lot more weird subtleties for other color modes but this
       simple version is enough for our needs.

       Signed integer values are sign-extended. Floats need to be bitcast. */
    CORRADE_INTERNAL_ASSERT(bitLength <= 32);

    const UnsignedInt typeMask = ~0u >> (32 - bitLength);

    switch(suffix) {
        case Implementation::VkFormatSuffix::UNORM:
        case Implementation::VkFormatSuffix::SRGB:
            return {0u, typeMask};
        case Implementation::VkFormatSuffix::SNORM: {
            /* Remove sign bit to get largest positive value. If we flip the
               bits of that, we get the sign-extended smallest negative value. */
            const UnsignedInt positiveTypeMask = typeMask >> 1;
            /* Uncompressed formats need -MAX (= MIN + 1) for symmetry around 0
               but block-compressed formats need INT32_MIN according to the
               KDF spec. */
            return {~positiveTypeMask + UnsignedInt(!isCompressed), positiveTypeMask};
        }
        case Implementation::VkFormatSuffix::UINT:
            return {0u, 1u};
        case Implementation::VkFormatSuffix::SINT:
            return {~0u, 1u};
        case Implementation::VkFormatSuffix::UFLOAT:
            return {Utility::bitCast<UnsignedInt>(0.0f), Utility::bitCast<UnsignedInt>(1.0f)};
        case Implementation::VkFormatSuffix::SFLOAT:
            return {Utility::bitCast<UnsignedInt>(-1.0f), Utility::bitCast<UnsignedInt>(1.0f)};
    }

    CORRADE_ASSERT_UNREACHABLE("channelMapping(): invalid format suffix" << UnsignedInt(suffix), {}); /* LCOV_EXCL_LINE */
}

template<typename Format> Containers::Array<char> fillDataFormatDescriptor(Format format, Implementation::VkFormatSuffix suffix) {
    const auto sampleData = samples(format);
    CORRADE_INTERNAL_ASSERT(!sampleData.second().isEmpty());

    /* Calculate total size. Header + one sample block per channel. */
    const std::size_t dfdSamplesSize = sampleData.second().size()*sizeof(Implementation::KdfBasicBlockSample);
    const std::size_t dfdBlockSize = sizeof(Implementation::KdfBasicBlockHeader) + dfdSamplesSize;
    const std::size_t dfdSize = sizeof(UnsignedInt) + dfdBlockSize;
    CORRADE_INTERNAL_ASSERT(dfdSize%4 == 0);

    Containers::Array<char> data{ValueInit, dfdSize};
    std::size_t offset = 0;

    UnsignedInt& length = *reinterpret_cast<UnsignedInt*>(data.exceptPrefix(offset).data());
    offset += sizeof(length);

    length = dfdSize;

    /* Basic block header */
    Implementation::KdfBasicBlockHeader& header = *reinterpret_cast<Implementation::KdfBasicBlockHeader*>(data.exceptPrefix(offset).data());
    offset += sizeof(header);

    header.vendorId = Implementation::KdfBasicBlockHeader::VendorId::Khronos;
    header.descriptorType = Implementation::KdfBasicBlockHeader::DescriptorType::Basic;
    header.versionNumber = Implementation::KdfBasicBlockHeader::VersionNumber::Kdf1_3;
    header.descriptorBlockSize = dfdBlockSize;

    header.colorModel = sampleData.first();
    header.colorPrimaries = Implementation::KdfBasicBlockHeader::ColorPrimaries::Srgb;
    header.transferFunction = suffix == Implementation::VkFormatSuffix::SRGB ?
        Implementation::KdfBasicBlockHeader::TransferFunction::Srgb :
        Implementation::KdfBasicBlockHeader::TransferFunction::Linear;
    /** @todo Do we ever have premultiplied alpha? */

    const Vector3i unitSize = formatUnitSize(format);
    const UnsignedInt unitDataSize = formatUnitDataSize(format);

    /* Value of texelBlockDimension is saved as one less than the actual size.
       The intent is to allow 256 but it's a wonderful bug source. */
    for(UnsignedInt i = 0; i != unitSize.Size; ++i) {
        if(unitSize[i] > 1)
            header.texelBlockDimension[i] = unitSize[i] - 1;
    }

    /* Sample blocks, one per channel */
    auto samples = Containers::arrayCast<Implementation::KdfBasicBlockSample>(data.exceptPrefix(offset));
    offset += dfdSamplesSize;

    constexpr bool isCompressedFormat = std::is_same<Format, CompressedPixelFormat>::value;
    const bool isDepthStencil = !isCompressedFormat &&
        sampleData.second().front().id != Implementation::KdfBasicBlockSample::ChannelId::Red;

    const UnsignedByte typeSize = formatTypeSize(format);
    /* Compressed integer formats must use 32-bit lower/upper */
    const UnsignedByte mappingBitLength = (isCompressedFormat ? sizeof(UnsignedInt) : typeSize)*8;
    /* @todo BC6h has unsigned floats, but the spec says to use a sampleLower
             of -1.0. The signed channel format flag is still set, however.
             See https://github.com/KhronosGroup/DataFormat/issues/16 */
    const auto lowerUpper = channelMapping(suffix, mappingBitLength, isCompressedFormat);
    const UnsignedByte formatFlags = channelFormat(suffix);
    /* For non-compressed RGBA channels, we get the 1-byte channel data
       and then multiply by the actual typeSize in the loop below */
    const UnsignedByte bitRangeMultiplier = isDepthStencil ? 1 : typeSize;

    UnsignedShort extent = 0;
    for(UnsignedInt i = 0; i != samples.size(); ++i) {
        const auto& sampleContent = sampleData.second()[i];
        auto& sample = samples[i];
        /* Value of bitLength is saved as one less than the actual size */
        sample.bitOffset = sampleContent.bitOffset*bitRangeMultiplier;
        sample.bitLength = sampleContent.bitLength*bitRangeMultiplier - 1;

        /* Some channels have custom suffixes, can't use data calculated
           from the main suffix */
        UnsignedByte sampleFormatFlags;
        Containers::Pair<UnsignedInt, UnsignedInt> sampleLowerUpper;
        if(sampleContent.suffix != Implementation::VkFormatSuffix{}) {
            CORRADE_INTERNAL_ASSERT(!isCompressedFormat);
            sampleFormatFlags = channelFormat(sampleContent.suffix);
            sampleLowerUpper = channelMapping(sampleContent.suffix, sample.bitLength + 1, isCompressedFormat);
        } else {
            sampleFormatFlags = formatFlags;
            sampleLowerUpper = lowerUpper;
        }

        sample.channelType = sampleContent.id | sampleFormatFlags;
        sample.lower = sampleLowerUpper.first();
        sample.upper = sampleLowerUpper.second();

        /* The linear format flag should only be set when the transfer function
           is non-linear */
        if(header.transferFunction != Implementation::KdfBasicBlockHeader::TransferFunction::Linear &&
           sampleContent.id == Implementation::KdfBasicBlockSample::ChannelId::Alpha)
        {
           sample.channelType |= Implementation::KdfBasicBlockSample::ChannelFormat::Linear;
        }

        extent = Math::max<UnsignedShort>(sample.bitOffset + sample.bitLength + 1, extent);

        Utility::Endianness::littleEndianInPlace(sample.bitOffset,
            sample.lower, sample.upper);
    }

    /* Make sure channel bit ranges returned by samples() are plausible.
       Can't use equals because some formats have channels smaller than the
       pixel size (mainly the combined depth formats). */
    CORRADE_INTERNAL_ASSERT(extent%8 == 0);
    CORRADE_INTERNAL_ASSERT(extent <= unitDataSize*8);

    /* The byte count is the actual occupied number of bytes. For most formats
       this is equal to unitDataSize, but for some formats with different-sized
       channels it can be less (e.g. Depth16UnormStencil8UI). Depth24Unorm is
       an odd exception because as far as Vulkan is concerned, it's a packed
       type (_PACK32), so the byte count is 4, not 3. The check below works
       because Depth24Unorm is the only single-channel format where
       extent/8 < unitDataSize. */
    if(samples.size() > 1)
        header.bytesPlane[0] = extent/8;
    else
        header.bytesPlane[0] = unitDataSize;

    CORRADE_INTERNAL_ASSERT(offset == dfdSize);

    Utility::Endianness::littleEndianInPlace(length);
    Utility::Endianness::littleEndianInPlace(header.vendorId, header.descriptorType,
        header.versionNumber, header.descriptorBlockSize);

    return data;
}

UnsignedInt leastCommonMultiple(UnsignedInt a, UnsignedInt b) {
    const UnsignedInt product = a*b;

    /* Greatest common divisor */
    while(b != 0) {
        const UnsignedInt t = a%b;
        a = b;
        b = t;
    }
    const UnsignedInt gcd = a;

    return product/gcd;
}

template<UnsignedInt dimensions> void copyPixels(const BasicImageView<dimensions>& image, Containers::ArrayView<char> pixels) {
    /* Copy the pixels into output, dropping padding (if any) */
    const Containers::StridedArrayView<dimensions + 1, const char> srcPixels = image.pixels();
    Utility::copy(srcPixels, Containers::StridedArrayView<dimensions + 1, char>{pixels, srcPixels.size()});
}

template<UnsignedInt dimensions> void copyPixels(const BasicCompressedImageView<dimensions>& image, Containers::ArrayView<char> pixels) {
    /** @todo Support CompressedPixelStorage::skip */
    CORRADE_ASSERT(image.storage() == CompressedPixelStorage{}, "Trade::KtxImageConverter::convertToData(): non-default compressed storage is not supported", );
    Utility::copy(image.data().prefix(pixels.size()), pixels);
}

template<std::size_t Size> struct TypeForSize {};
template<> struct TypeForSize<1> { typedef UnsignedByte  Type; };
template<> struct TypeForSize<2> { typedef UnsignedShort Type; };
template<> struct TypeForSize<4> { typedef UnsignedInt   Type; };
template<> struct TypeForSize<8> { typedef UnsignedLong  Type; };

void endianSwap(Containers::ArrayView<char> data, UnsignedInt typeSize) {
    switch(typeSize) {
        case 1:
            /* Single-byte or block-compressed format, nothing to do */
            return;
        case 2:
            Utility::Endianness::littleEndianInPlace(Containers::arrayCast<TypeForSize<2>::Type>(data));
            return;
        case 4:
            Utility::Endianness::littleEndianInPlace(Containers::arrayCast<TypeForSize<4>::Type>(data));
            return;
        case 8:
            Utility::Endianness::littleEndianInPlace(Containers::arrayCast<TypeForSize<8>::Type>(data));
            return;
    }

    CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

using namespace Containers::Literals;

/* Having this inside convertLevels() leads to errors with GCC 4.8:
   "cannot initialize aggregate [...] with a compound literal" */
constexpr Containers::StringView ValidOrientations[3]{"rl"_s, "du"_s, "io"_s};

/* Using a template template parameter to deduce the image dimensions while
   matching both ImageView and CompressedImageView. Matching on the ImageView
   typedefs doesn't work, so we need the extra parameter of BasicImageView. */
template<UnsignedInt dimensions, template<UnsignedInt, typename> class View> Containers::Optional<Containers::Array<char>> convertLevels(Containers::ArrayView<const View<dimensions, const char>> imageLevels, const Utility::ConfigurationGroup& configuration) {
    const auto format = imageLevels.front().format();
    if(isFormatImplementationSpecific(format)) {
        Error{} << "Trade::KtxImageConverter::convertToData(): implementation-specific formats are not supported";
        return {};
    }

    const auto vkFormat = vulkanFormat(format);
    if(vkFormat.first() == Implementation::VK_FORMAT_UNDEFINED) {
        Error{} << "Trade::KtxImageConverter::convertToData(): unsupported format" << format;
        return {};
    }

    const Containers::Array<char> dataFormatDescriptor = fillDataFormatDescriptor(format, vkFormat.second());

    /* Fill key/value data. Values can be any byte-string but we only write
       constant text strings. Keys must be sorted alphabetically.
       Entries with an empty value won't be written. */

    const auto orientation = configuration.value<Containers::StringView>("orientation");
    const auto swizzle = configuration.value<Containers::StringView>("swizzle");
    const auto writerName = configuration.value<Containers::StringView>("writerName");

    if(orientation) {
        if(orientation.size() < dimensions) {
            Error{} << "Trade::KtxImageConverter::convertToData(): invalid orientation string, expected at least" <<
                dimensions << "characters but got" << orientation;
            return {};
        }

        for(UnsignedByte i = 0; i != dimensions; ++i) {
            if(!ValidOrientations[i].contains(orientation[i])) {
                /* Error{} prints char as int value so use StringViews to get
                   text output */
                Error{} << "Trade::KtxImageConverter::convertToData(): invalid character in orientation, expected" <<
                    ValidOrientations[i].prefix(1) << "or" << ValidOrientations[i].exceptPrefix(1) <<
                    "but got" << Containers::StringView{orientation}.exceptPrefix(i).prefix(1);
                return {};
            }
        }
    }

    if(swizzle && swizzle.size() != 4) {
        Error{} << "Trade::KtxImageConverter::convertToData(): invalid swizzle length, expected 4 but got" << swizzle.size();
        return {};
    }

    /** @todo clean up once StringView has findAnyNotOf() or some such */
    if(std::string{swizzle}.find_first_not_of("rgba01") != std::string::npos) {
        Error{} << "Trade::KtxImageConverter::convertToData(): invalid characters in swizzle" << swizzle;
        return {};
    }

    const Containers::Pair<Containers::StringView, Containers::StringView> keyValueMap[]{
        Containers::pair("KTXorientation"_s, Containers::StringView{orientation}.prefix(Math::min(size_t(dimensions), orientation.size()))),
        Containers::pair("KTXswizzle"_s, Containers::StringView{swizzle}),
        Containers::pair("KTXwriter"_s, Containers::StringView{writerName})
    };

    /* Calculate size */
    std::size_t kvdSize = 0;
    for(const auto& entry: keyValueMap) {
        CORRADE_INTERNAL_ASSERT(!entry.first().isEmpty());
        if(!entry.second().isEmpty()) {
            const UnsignedInt length = entry.first().size() + 1 + entry.second().size() + 1;
            kvdSize += sizeof(length) + (length + 3)/4*4;
        }
    }
    CORRADE_INTERNAL_ASSERT(kvdSize%4 == 0);

    /* Pack. We assume that values are text strings, no endian-swapping needed. */
    std::size_t kvdOffset = 0;
    Containers::Array<char> keyValueData{ValueInit, kvdSize};
    for(const auto& entry: keyValueMap) {
        if(!entry.second().isEmpty()) {
            const auto key = entry.first();
            const auto value = entry.second();
            const UnsignedInt length = key.size() + 1 + value.size() + 1;
            *reinterpret_cast<UnsignedInt*>(keyValueData.exceptPrefix(kvdOffset).data()) = length;
            Utility::Endianness::littleEndianInPlace(length);
            kvdOffset += sizeof(length);
            Utility::copy(key, keyValueData.exceptPrefix(kvdOffset).prefix(key.size()));
            kvdOffset += entry.first().size() + 1;
            Utility::copy(value, keyValueData.exceptPrefix(kvdOffset).prefix(value.size()));
            kvdOffset += entry.second().size() + 1;
            kvdOffset = (kvdOffset + 3)/4*4;
        }
    }
    CORRADE_INTERNAL_ASSERT(kvdOffset == kvdSize);

    /* Fill level index */
    const Math::Vector<dimensions, Int> size = imageLevels.front().size();

    const UnsignedInt numMipmaps = Math::min<UnsignedInt>(imageLevels.size(), Math::log2(size.max()) + 1);
    if(imageLevels.size() > numMipmaps) {
        Error{} << "Trade::KtxImageConverter::convertToData(): there can be only" << numMipmaps <<
            "levels with base image size" << imageLevels[0].size() << "but got" << imageLevels.size();
        return {};
    }

    Containers::Array<Implementation::KtxLevel> levelIndex{numMipmaps};

    const std::size_t levelIndexSize = numMipmaps*sizeof(Implementation::KtxLevel);
    std::size_t levelOffset = sizeof(Implementation::KtxHeader) + levelIndexSize +
        dataFormatDescriptor.size() + keyValueData.size();

    /* A "unit" is either a pixel or a block in a compressed format */
    const Vector3i unitSize = formatUnitSize(format);
    const UnsignedInt unitDataSize = formatUnitDataSize(format);

    for(UnsignedInt i = 0; i != levelIndex.size(); ++i) {
        /* Mip levels are required to be stored from smallest to largest for
           efficient streaming */
        const UnsignedInt mip = levelIndex.size() - 1 - i;
        const Math::Vector<dimensions, Int> mipSize = Math::max(size >> mip, 1);

        const auto& image = imageLevels[mip];

        if(image.size() != mipSize) {
            Error{} << "Trade::KtxImageConverter::convertToData(): expected "
                "size" << mipSize << "for level" << mip << "but got" << image.size();
            return {};
        }

        /* Offset needs to be aligned to the least common multiple of the
           texel/block size and 4. Not needed with supercompression. */
        const std::size_t alignment = leastCommonMultiple(unitDataSize, 4);
        levelOffset = (levelOffset + alignment - 1)/alignment*alignment;

        const Vector3i unitCount = (Vector3i::pad(mipSize, 1) + unitSize - Vector3i{1})/unitSize;
        const std::size_t levelSize = unitDataSize*unitCount.product();

        levelIndex[mip].byteOffset = levelOffset;
        levelIndex[mip].byteLength = levelSize;
        levelIndex[mip].uncompressedByteLength = levelSize;

        levelOffset += levelSize;
    }

    const std::size_t dataSize = levelOffset;
    Containers::Array<char> data{ValueInit, dataSize};

    std::size_t offset = 0;

    /* Fill header */
    auto& header = *reinterpret_cast<Implementation::KtxHeader*>(data.data());
    offset += sizeof(header);
    Utility::copy(Containers::arrayView(Implementation::KtxFileIdentifier), Containers::arrayView(header.identifier));

    header.vkFormat = vkFormat.first();
    header.typeSize = formatTypeSize(format);
    header.imageSize = Vector3ui{Vector3i::pad(size, 0u)};
    /** @todo Handle different image types (cube and/or array) once this can be
              queried from images */
    header.layerCount = 0;
    header.faceCount = 1;
    header.levelCount = levelIndex.size();
    header.supercompressionScheme = Implementation::SuperCompressionScheme::None;

    for(UnsignedInt i = 0; i != levelIndex.size(); ++i) {
        const Implementation::KtxLevel& level = levelIndex[i];
        const auto& image = imageLevels[i];
        const auto pixels = data.exceptPrefix(level.byteOffset).prefix(level.byteLength);
        copyPixels(image, pixels);

        endianSwap(pixels, header.typeSize);

        Utility::Endianness::littleEndianInPlace(
            level.byteOffset, level.byteLength,
            level.uncompressedByteLength);
    }

    Utility::copy(Containers::arrayCast<const char>(levelIndex), data.exceptPrefix(offset).prefix(levelIndexSize));
    offset += levelIndexSize;

    header.dfdByteOffset = offset;
    header.dfdByteLength = dataFormatDescriptor.size();
    offset += header.dfdByteLength;

    Utility::copy(dataFormatDescriptor, data.exceptPrefix(header.dfdByteOffset).prefix(header.dfdByteLength));

    if(!keyValueData.isEmpty()) {
        header.kvdByteOffset = offset;
        header.kvdByteLength = keyValueData.size();

        Utility::copy(keyValueData, data.exceptPrefix(header.kvdByteOffset).prefix(header.kvdByteLength));
    }

    /* Endian-swap once we're done using the header data */
    Utility::Endianness::littleEndianInPlace(
        header.vkFormat, header.typeSize,
        header.imageSize[0], header.imageSize[1], header.imageSize[2],
        header.layerCount, header.faceCount, header.levelCount,
        header.supercompressionScheme,
        header.dfdByteOffset, header.dfdByteLength,
        header.kvdByteOffset, header.kvdByteLength);

    /* GCC 4.8 and Clang 3.8 need extra help here */
    return Containers::optional(std::move(data));
}

}

KtxImageConverter::KtxImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures KtxImageConverter::doFeatures() const {
    return ImageConverterFeature::ConvertLevels1DToData |
        ImageConverterFeature::ConvertLevels2DToData |
        ImageConverterFeature::ConvertLevels3DToData |
        ImageConverterFeature::ConvertCompressedLevels1DToData |
        ImageConverterFeature::ConvertCompressedLevels2DToData |
        ImageConverterFeature::ConvertCompressedLevels3DToData;
}

Containers::Optional<Containers::Array<char>> KtxImageConverter::doConvertToData(Containers::ArrayView<const ImageView1D> imageLevels) {
    return convertLevels(imageLevels, configuration());
}

Containers::Optional<Containers::Array<char>> KtxImageConverter::doConvertToData(Containers::ArrayView<const ImageView2D> imageLevels) {
    return convertLevels(imageLevels, configuration());
}

Containers::Optional<Containers::Array<char>> KtxImageConverter::doConvertToData(Containers::ArrayView<const ImageView3D> imageLevels) {
    return convertLevels(imageLevels, configuration());
}

Containers::Optional<Containers::Array<char>> KtxImageConverter::doConvertToData(Containers::ArrayView<const CompressedImageView1D> imageLevels) {
    return convertLevels(imageLevels, configuration());
}

Containers::Optional<Containers::Array<char>> KtxImageConverter::doConvertToData(Containers::ArrayView<const CompressedImageView2D> imageLevels) {
    return convertLevels(imageLevels, configuration());
}

Containers::Optional<Containers::Array<char>> KtxImageConverter::doConvertToData(Containers::ArrayView<const CompressedImageView3D> imageLevels) {
    return convertLevels(imageLevels, configuration());
}

}}

CORRADE_PLUGIN_REGISTER(KtxImageConverter, Magnum::Trade::KtxImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3.2")
