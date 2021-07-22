/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/EndiannessBatch.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Swizzle.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include "MagnumPlugins/KtxImporter/KtxHeader.h"

namespace Magnum { namespace Trade {

namespace {

typedef Containers::Pair<Implementation::VkFormat, Implementation::VkFormatSuffix> FormatPair;

FormatPair vulkanFormat(PixelFormat format) {
    switch(format) {
        #define _p(vulkan, magnum, type) case PixelFormat::magnum: \
            return {Implementation::VK_FORMAT_ ## vulkan, Implementation::VkFormatSuffix::type};
        #include "MagnumPlugins/KtxImporter/formatMapping.hpp"
        #undef _p
        default:
            return {{}, {}};
    }
}

FormatPair vulkanFormat(CompressedPixelFormat format) {
    switch(format) {
        #define _c(vulkan, magnum, type) case CompressedPixelFormat::magnum: \
            return {Implementation::VK_FORMAT_ ## vulkan, Implementation::VkFormatSuffix::type};
        #include "MagnumPlugins/KtxImporter/formatMapping.hpp"
        #undef _c
        default:
            return {{}, {}};
    }
}

UnsignedByte componentSize(PixelFormat format) {
    CORRADE_INTERNAL_ASSERT(!isPixelFormatImplementationSpecific(format));

    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic error "-Wswitch"
    #endif
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
    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif

    CORRADE_ASSERT_UNREACHABLE("componentSize(): invalid format" << format, {});
}

UnsignedByte componentSize(CompressedPixelFormat) {
    return 1;
}

UnsignedByte channelFormat(Implementation::VkFormatSuffix suffix, Implementation::KdfBasicBlockSample::ChannelId id) {
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
            return (id == Implementation::KdfBasicBlockSample::ChannelId::Alpha)
                ? Implementation::KdfBasicBlockSample::ChannelFormat::Linear
                : Implementation::KdfBasicBlockSample::ChannelFormat{};
    }

    CORRADE_ASSERT_UNREACHABLE("channelFormat(): invalid format suffix" << UnsignedInt(suffix), {});
}

Containers::Array2<UnsignedInt> channelMapping(Implementation::VkFormatSuffix suffix, UnsignedByte typeSize) {
    /* sampleLower and sampleUpper define how to interpret the range of values
       found in a channel.
       samplerLower = black value or -1 for signed values
       samplerUpper = white value or 1 for signed values

       There are a lot more weird subtleties for other color modes but this
       simple version is enough for our needs.

       Signed integer values are sign-extended. Floats need to be bitcast. */

    /** @todo If we support custom formats, this might require changes for
              typeSize == 8 because these values are only 32-bit. Currently,
              Magnum doesn't expose 64-bit formats. */
    CORRADE_INTERNAL_ASSERT(typeSize <= 4);

    const UnsignedInt typeMask = ~0u >> ((4 - typeSize) * 8);

    switch(suffix) {
        case Implementation::VkFormatSuffix::UNORM:
        case Implementation::VkFormatSuffix::SRGB:
            return {0u, typeMask};
        case Implementation::VkFormatSuffix::SNORM:
            {
            /* Remove sign bit to get largest positive value. If we flip the
               bits of that, we get the sign-extended lowest negative value. */
            const UnsignedInt positiveTypeMask = typeMask >> 1;
            return {~positiveTypeMask, positiveTypeMask};
            }
        case Implementation::VkFormatSuffix::UINT:
            return {0u, 1u};
        case Implementation::VkFormatSuffix::SINT:
            return {~0u, 1u};
        case Implementation::VkFormatSuffix::UFLOAT:
            return {Corrade::Utility::bitCast<UnsignedInt>(0.0f), Corrade::Utility::bitCast<UnsignedInt>(1.0f)};
        case Implementation::VkFormatSuffix::SFLOAT:
            return {Corrade::Utility::bitCast<UnsignedInt>(-1.0f), Corrade::Utility::bitCast<UnsignedInt>(1.0f)};
    }

    CORRADE_ASSERT_UNREACHABLE("channelLimits(): invalid format suffix" << UnsignedInt(suffix), {});
}

bool isSrgb(PixelFormat format) {
    CORRADE_INTERNAL_ASSERT(!isPixelFormatImplementationSpecific(format));

    switch(format) {
        case PixelFormat::R8Srgb:
        case PixelFormat::RG8Srgb:
        case PixelFormat::RGB8Srgb:
        case PixelFormat::RGBA8Srgb:
            return true;
        default:
            return false;
    }
}

bool isSrgb(CompressedPixelFormat format) {
    CORRADE_INTERNAL_ASSERT(!isCompressedPixelFormatImplementationSpecific(format));

    switch(format) {
        case CompressedPixelFormat::Bc1RGBSrgb:
        case CompressedPixelFormat::Bc1RGBASrgb:
        case CompressedPixelFormat::Bc2RGBASrgb:
        case CompressedPixelFormat::Bc3RGBASrgb:
        case CompressedPixelFormat::Bc7RGBASrgb:
        case CompressedPixelFormat::Etc2RGB8Srgb:
        case CompressedPixelFormat::Etc2RGB8A1Srgb:
        case CompressedPixelFormat::Etc2RGBA8Srgb:
        case CompressedPixelFormat::Astc4x4RGBASrgb:
        case CompressedPixelFormat::Astc5x4RGBASrgb:
        case CompressedPixelFormat::Astc5x5RGBASrgb:
        case CompressedPixelFormat::Astc6x5RGBASrgb:
        case CompressedPixelFormat::Astc6x6RGBASrgb:
        case CompressedPixelFormat::Astc8x5RGBASrgb:
        case CompressedPixelFormat::Astc8x6RGBASrgb:
        case CompressedPixelFormat::Astc8x8RGBASrgb:
        case CompressedPixelFormat::Astc10x5RGBASrgb:
        case CompressedPixelFormat::Astc10x6RGBASrgb:
        case CompressedPixelFormat::Astc10x8RGBASrgb:
        case CompressedPixelFormat::Astc10x10RGBASrgb:
        case CompressedPixelFormat::Astc12x10RGBASrgb:
        case CompressedPixelFormat::Astc12x12RGBASrgb:
        case CompressedPixelFormat::Astc3x3x3RGBASrgb:
        case CompressedPixelFormat::Astc4x3x3RGBASrgb:
        case CompressedPixelFormat::Astc4x4x3RGBASrgb:
        case CompressedPixelFormat::Astc4x4x4RGBASrgb:
        case CompressedPixelFormat::Astc5x4x4RGBASrgb:
        case CompressedPixelFormat::Astc5x5x4RGBASrgb:
        case CompressedPixelFormat::Astc5x5x5RGBASrgb:
        case CompressedPixelFormat::Astc6x5x5RGBASrgb:
        case CompressedPixelFormat::Astc6x6x5RGBASrgb:
        case CompressedPixelFormat::Astc6x6x6RGBASrgb:
        case CompressedPixelFormat::PvrtcRGB2bppSrgb:
        case CompressedPixelFormat::PvrtcRGBA2bppSrgb:
        case CompressedPixelFormat::PvrtcRGB4bppSrgb:
        case CompressedPixelFormat::PvrtcRGBA4bppSrgb:
            return true;
        default:
            return false;
    }
}

Containers::Array<char> fillDataFormatDescriptor(PixelFormat format, Implementation::VkFormatSuffix suffix) {
    const UnsignedInt texelSize = pixelSize(format);
    const UnsignedInt typeSize = componentSize(format);
    /** @todo numChannels will be wrong for block-compressed formats */
    const UnsignedInt numChannels = texelSize / typeSize;

    /* Calculate size */
    const std::size_t dfdSamplesSize = numChannels * sizeof(Implementation::KdfBasicBlockSample);
    const std::size_t dfdBlockSize = sizeof(Implementation::KdfBasicBlockHeader) + dfdSamplesSize;
    const std::size_t dfdSize = sizeof(UnsignedInt) + dfdBlockSize;
    CORRADE_INTERNAL_ASSERT(dfdSize % 4 == 0);

    Containers::Array<char> data{ValueInit, dfdSize};

    std::size_t offset = 0;

    /* Length */
    UnsignedInt& length = *reinterpret_cast<UnsignedInt*>(data.suffix(offset).data());
    offset += sizeof(length);

    length = dfdSize;

    /* Block header */
    Implementation::KdfBasicBlockHeader& header = *reinterpret_cast<Implementation::KdfBasicBlockHeader*>(data.suffix(offset).data());
    offset += sizeof(header);

    header.vendorId = Implementation::KdfBasicBlockHeader::VendorId::Khronos;
    header.descriptorType = Implementation::KdfBasicBlockHeader::DescriptorType::Basic;
    header.versionNumber = Implementation::KdfBasicBlockHeader::VersionNumber::Kdf1_3;
    header.descriptorBlockSize = dfdBlockSize;

    header.colorModel = Implementation::KdfBasicBlockHeader::ColorModel::Rgbsda;
    header.colorPrimaries = Implementation::KdfBasicBlockHeader::ColorPrimaries::Srgb;
    header.transferFunction = isSrgb(format)
        ? Implementation::KdfBasicBlockHeader::TransferFunction::Srgb
        : Implementation::KdfBasicBlockHeader::TransferFunction::Linear;
    /** @todo Do we ever have premultiplied alpha? */
    header.bytesPlane[0] = texelSize;

    /* Color channels */
    const auto samples = Containers::arrayCast<Implementation::KdfBasicBlockSample>(data.suffix(offset));
    offset += dfdSamplesSize;

    const UnsignedByte bitLength = typeSize*8;

    static const Implementation::KdfBasicBlockSample::ChannelId channelIdsRgba[4]{
        Implementation::KdfBasicBlockSample::ChannelId::Red,
        Implementation::KdfBasicBlockSample::ChannelId::Green,
        Implementation::KdfBasicBlockSample::ChannelId::Blue,
        Implementation::KdfBasicBlockSample::ChannelId::Alpha
    };
    /*
    static const Implementation::KdfBasicBlockSample::ChannelId channelIdsDepthStencil[2]{
        Implementation::KdfBasicBlockSample::ChannelId::Depth,
        Implementation::KdfBasicBlockSample::ChannelId::Stencil
    };
    static const Implementation::KdfBasicBlockSample::ChannelId channelIdsStencil[1]{
        Implementation::KdfBasicBlockSample::ChannelId::Stencil
    };
    */

    /** @todo detect depth/stencil */
    Containers::ArrayView<const Implementation::KdfBasicBlockSample::ChannelId> channelIds =
        Containers::arrayView(channelIdsRgba);

    UnsignedShort bitOffset = 0;
    for(UnsignedInt i = 0; i != numChannels; ++i) {
        auto& sample = samples[i];
        sample.bitOffset = bitOffset;
        sample.bitLength = bitLength - 1;
        const auto channelId = channelIds[i];
        sample.channelType = channelId | channelFormat(suffix, channelId);
        const auto mapping = channelMapping(suffix, typeSize);
        sample.lower = mapping[0];
        sample.upper = mapping[1];

        bitOffset += bitLength;

        Utility::Endianness::littleEndianInPlace(sample.bitOffset,
            sample.lower, sample.upper);
    }

    Utility::Endianness::littleEndianInPlace(length);
    Utility::Endianness::littleEndianInPlace(header.vendorId, header.descriptorType,
        header.versionNumber, header.descriptorBlockSize);

    CORRADE_INTERNAL_ASSERT(offset == dfdSize);

    return data;
}

typedef Containers::Pair<Containers::StringView, Containers::StringView> KeyValuePair;

Containers::Array<char> packKeyValueData(Containers::ArrayView<const KeyValuePair> pairs) {
    /* Calculate size */
    std::size_t kvdSize = 0;
    for(const KeyValuePair& entry: pairs) {
        const UnsignedInt length = entry.first().size() + 1 + entry.second().size() + 1;
        kvdSize += sizeof(length) + (length + 3)/4*4;
    }
    CORRADE_INTERNAL_ASSERT(kvdSize % 4 == 0);

    /* Pack. We assume that values are actual text strings and don't need any
       endian-swapping. */
    std::size_t offset = 0;
    Containers::Array<char> data{ValueInit, kvdSize};
    for(const KeyValuePair& entry: pairs) {
        const auto key = entry.first();
        const auto value = entry.second();
        const UnsignedInt length = key.size() + 1 + value.size() + 1;
        *reinterpret_cast<UnsignedInt*>(data.suffix(offset).data()) = length;
        Utility::Endianness::littleEndianInPlace(length);
        offset += sizeof(length);
        Utility::copy(key, data.suffix(offset).prefix(key.size()));
        offset += entry.first().size() + 1;
        Utility::copy(value, data.suffix(offset).prefix(value.size()));
        offset += entry.second().size() + 1;
        offset = (offset + 3)/4*4;
    }
    CORRADE_INTERNAL_ASSERT(offset == kvdSize);

    return data;
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

UnsignedInt leastCommonMultiple(UnsignedInt a, UnsignedInt b) {
    const UnsignedInt product = a*b;

    /* Greatest common divisor */
    while(b != 0) {
        const UnsignedInt t = a % b;
        a = b;
        b = t;
    }
    const UnsignedInt gcd = a;

    return product/gcd;
}

template<UnsignedInt dimensions>
Containers::Array<char> convertImage(const BasicImageView<dimensions>& image) {
    if(isPixelFormatImplementationSpecific(image.format())) {
        Error{} << "Trade::KtxImageConverter::convertToData(): implementation-specific formats are not supported";
        return {};
    }

    const auto vkFormat = vulkanFormat(image.format());
    if(vkFormat.first() == Implementation::VK_FORMAT_UNDEFINED) {
        Error{} << "Trade::KtxImageConverter::convertToData(): unsupported format" << image.format();
        return {};
    }

    const Containers::Array<char> dataFormatDescriptor = fillDataFormatDescriptor(image.format(), vkFormat.second());

    /* Fill key/value data. Values can be any byte-string but the values we
       write are all constant text strings. Keys must be sorted alphabetically.*/
    using namespace Containers::Literals;

    const Containers::StaticArray<2, const KeyValuePair> keyValueMap{
        /* Origin left, bottom, back (increasing right, up, out) */
        /** @todo KTX spec says "most other APIs and the majority of texture compression tools use [r, rd, rdi]".
                  Should we just always flip image data? Maybe make this configurable. */
        Containers::pair("KTXorientation"_s, "ruo"_s.prefix(dimensions)),
        Containers::pair("KTXwriter"_s, "Magnum::KtxImageConverter 1.0"_s)
    };

    const Containers::Array<char> keyValueData = packKeyValueData(keyValueMap);

    /* Calculate level offsets. Needs to be aligned to the least common
       multiple of the texel/block size and 4. */
    constexpr UnsignedInt numMipmaps = 1;
    Containers::Array<Implementation::KtxLevel> levelIndex{numMipmaps};
    const std::size_t levelIndexSize = levelIndex.size()*sizeof(Implementation::KtxLevel);

    const UnsignedInt pixelSize = UnsignedByte(image.pixelSize());
    std::size_t levelOffset = sizeof(Implementation::KtxHeader) + levelIndexSize +
        dataFormatDescriptor.size() + keyValueData.size();

    /* @todo Store mip levels from smallest to lowest for efficient streaming */
    for(UnsignedInt i = 0; i != levelIndex.size(); ++i) {
        const std::size_t alignment = leastCommonMultiple(pixelSize, 4);
        levelOffset = (levelOffset + (alignment - 1)) / alignment * alignment;
        const std::size_t levelSize = pixelSize*image.size().product();

        levelIndex[i].byteOffset = levelOffset;
        levelIndex[i].byteLength = levelSize;
        levelIndex[i].uncompressedByteLength = levelSize;

        levelOffset += levelSize;
    }

    /* Initialize data buffer */
    const std::size_t dataSize = levelOffset;
    Containers::Array<char> data{ValueInit, dataSize};

    std::size_t offset = 0;

    /* Fill header */
    auto& header = *reinterpret_cast<Implementation::KtxHeader*>(data.data());
    offset += sizeof(header);
    Utility::copy(Containers::arrayView(Implementation::KtxFileIdentifier), Containers::arrayView(header.identifier));

    header.vkFormat = vkFormat.first();
    header.typeSize = componentSize(image.format());
    header.imageSize = Vector3ui{Vector3i::pad(image.size(), 0u)};
    header.layerCount = 0;
    header.faceCount = 1;
    header.levelCount = levelIndex.size();
    header.supercompressionScheme = Implementation::SuperCompressionScheme::None;

    for(auto& level: levelIndex) {
        /* Copy the pixels into output, dropping padding (if any) */
        auto pixels = data.suffix(level.byteOffset).prefix(level.byteLength);

        std::size_t sizes[dimensions + 1];
        sizes[dimensions] = pixelSize;
        for(UnsignedInt i = 0; i != dimensions; ++i) {
            sizes[dimensions - 1 - i] = image.size()[i];
        }

        Utility::copy(image.pixels(), Containers::StridedArrayView<dimensions + 1, char>{pixels, sizes});

        endianSwap(pixels, header.typeSize);

        Utility::Endianness::littleEndianInPlace(
            level.byteOffset, level.byteLength,
            level.uncompressedByteLength);
    }

    Utility::copy(Containers::arrayCast<const char>(levelIndex), data.suffix(offset).prefix(levelIndexSize));
    offset += levelIndexSize;

    header.dfdByteOffset = offset;
    header.dfdByteLength = dataFormatDescriptor.size();
    offset += header.dfdByteLength;

    Utility::copy(dataFormatDescriptor, data.suffix(header.dfdByteOffset).prefix(header.dfdByteLength));

    header.kvdByteOffset = offset;
    header.kvdByteLength = keyValueData.size();
    offset += header.kvdByteLength;
    
    Utility::copy(keyValueData, data.suffix(header.kvdByteOffset).prefix(header.kvdByteLength));

    /* Endian-swap once we're done using the header data */
    Utility::Endianness::littleEndianInPlace(
        header.vkFormat, header.typeSize,
        header.imageSize[0], header.imageSize[1], header.imageSize[2],
        header.layerCount, header.faceCount, header.levelCount,
        header.supercompressionScheme,
        header.dfdByteOffset, header.dfdByteLength,
        header.kvdByteOffset, header.kvdByteLength);

    return data;
}

/** @todo */
template<UnsignedInt dimensions>
Containers::Array<char> convertImage(const BasicCompressedImageView<dimensions>&) {
    return {};
}

}

KtxImageConverter::KtxImageConverter() = default;

KtxImageConverter::KtxImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures KtxImageConverter::doFeatures() const {
    return ImageConverterFeature::Convert1DToData |
        ImageConverterFeature::Convert2DToData |
        ImageConverterFeature::Convert3DToData |
        ImageConverterFeature::ConvertCompressed1DToData |
        ImageConverterFeature::ConvertCompressed2DToData |
        ImageConverterFeature::ConvertCompressed3DToData;
}

Containers::Array<char> KtxImageConverter::doConvertToData(const ImageView1D& image) {
    return convertImage(image);
}

Containers::Array<char> KtxImageConverter::doConvertToData(const ImageView2D& image) {
    return convertImage(image);
}

Containers::Array<char> KtxImageConverter::doConvertToData(const ImageView3D& image) {
    return convertImage(image);
}

Containers::Array<char> KtxImageConverter::doConvertToData(const CompressedImageView1D& image) {
    return convertImage(image);
}

Containers::Array<char> KtxImageConverter::doConvertToData(const CompressedImageView2D& image) {
    return convertImage(image);
}

Containers::Array<char> KtxImageConverter::doConvertToData(const CompressedImageView3D& image) {
    return convertImage(image);
}

}}

CORRADE_PLUGIN_REGISTER(KtxImageConverter, Magnum::Trade::KtxImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3")
