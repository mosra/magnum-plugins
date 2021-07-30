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
#include <Magnum/Math/Vector3.h>
#include "MagnumPlugins/KtxImporter/KtxHeader.h"

namespace Magnum { namespace Trade {

namespace {

/* Need overloaded functions to use different pixel formats in templated code */
bool isFormatImplementationSpecific(PixelFormat format) {
    return isPixelFormatImplementationSpecific(format);
}

bool isFormatImplementationSpecific(CompressedPixelFormat format) {
    return isCompressedPixelFormatImplementationSpecific(format);
}

typedef Containers::Pair<Implementation::VkFormat, Implementation::VkFormatSuffix> FormatPair;

FormatPair vulkanFormat(PixelFormat format) {
    switch(format) {
        #define _p(vulkan, magnum, type) case PixelFormat::magnum: \
            return {vulkan, Implementation::VkFormatSuffix::type};
        #include "MagnumPlugins/KtxImporter/formatMapping.hpp"
        #undef _p
        default:
            return {{}, {}};
    }
}

FormatPair vulkanFormat(CompressedPixelFormat format) {
    /* In Vulkan there is no distinction between RGB and RGBA PVRTC:
       https://github.com/KhronosGroup/Vulkan-Docs/issues/512#issuecomment-307768667
       formatMapping.hpp (generated from Vk::PixelFormat) contains the RGBA
       variants, so we manually alias them here. We can't do this inside
       formatMapping.hpp because both Magnum and Vulkan formats must be unique
       for switch cases. */
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
        #include "MagnumPlugins/KtxImporter/formatMapping.hpp"
        #undef _c
        default:
            return {{}, {}};
    }
}

UnsignedByte componentSize(PixelFormat format) {
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
        default:
            break;
    }

    CORRADE_ASSERT_UNREACHABLE("componentSize(): unsupported format" << format, {});
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
    header.transferFunction = suffix == Implementation::VkFormatSuffix::SRGB
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


    /* Calculate size */

    std::size_t offset = 0;
    }

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
void copyPixels(const BasicImageView<dimensions>& image, Containers::ArrayView<char> pixels) {
    std::size_t sizes[dimensions + 1];
    sizes[dimensions] = image.pixelSize();
    for(UnsignedInt i = 0; i != dimensions; ++i) {
        sizes[dimensions - 1 - i] = image.size()[i];
    }

    /* Copy the pixels into output, dropping padding (if any) */
    Utility::copy(image.pixels(), Containers::StridedArrayView<dimensions + 1, char>{pixels, sizes});
}

template<UnsignedInt dimensions>
void copyPixels(const BasicCompressedImageView<dimensions>& image, Containers::ArrayView<char> pixels) {
    /** @todo How do we deal with CompressedPixelStorage::skip?
              ImageView::pixels handles this for non-compressed images. */
    Utility::copy(image.data(), pixels);
}

/* Using a template template parameter to deduce the image dimensions while
   matching both ImageView and CompressedImageView. Matching on the ImageView
   typedefs doesn't work, so we need the extra parameter of BasicImageView. */
template<UnsignedInt dimensions, typename T, template<UnsignedInt, typename> class View>
Containers::Array<char> convertLevels(Containers::ArrayView<const View<dimensions, T>> imageLevels) {
    if(imageLevels.empty()) {
        Error() << "Trade::KtxImageConverter::convertToData(): expected at least 1 mip level";
        return {};
    }

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
       constant text strings. Keys must be sorted alphabetically.*/
    using namespace Containers::Literals;

    const Containers::Pair<Containers::StringView, Containers::StringView> keyValueMap[]{
        /* Origin left, bottom, back (increasing right, up, out) */
        /** @todo KTX spec says "most other APIs and the majority of texture
                  compression tools use [r, rd, rdi]".
                  Should we just always flip image data? Maybe make this
                  configurable. */
        Containers::pair("KTXorientation"_s, "ruo"_s.prefix(dimensions)),
        Containers::pair("KTXwriter"_s, "Magnum::KtxImageConverter 1.0"_s)
    };

    /* Calculate size */
    std::size_t kvdSize = 0;
    for(const auto& entry: keyValueMap) {
        const UnsignedInt length = entry.first().size() + 1 + entry.second().size() + 1;
        kvdSize += sizeof(length) + (length + 3)/4*4;
    }
    CORRADE_INTERNAL_ASSERT(kvdSize % 4 == 0);

    /* Pack. We assume that values are text strings, no endian-swapping needed. */
    std::size_t kvdOffset = 0;
    Containers::Array<char> keyValueData{ValueInit, kvdSize};
    for(const auto& entry: keyValueMap) {
        const auto key = entry.first();
        const auto value = entry.second();
        const UnsignedInt length = key.size() + 1 + value.size() + 1;
        *reinterpret_cast<UnsignedInt*>(keyValueData.suffix(kvdOffset).data()) = length;
        Utility::Endianness::littleEndianInPlace(length);
        kvdOffset += sizeof(length);
        Utility::copy(key, keyValueData.suffix(kvdOffset).prefix(key.size()));
        kvdOffset += entry.first().size() + 1;
        Utility::copy(value, keyValueData.suffix(kvdOffset).prefix(value.size()));
        kvdOffset += entry.second().size() + 1;
        kvdOffset = (kvdOffset + 3)/4*4;
    }
    CORRADE_INTERNAL_ASSERT(kvdOffset == kvdSize);

    /* Fill level index */
    const Math::Vector<dimensions, Int> size = imageLevels.front().size();
    if(size.product() == 0) {
        Error() << "Trade::KtxImageConverter::convertToData(): image for level 0 is empty";
        return {};
    }

    const UnsignedInt numMipmaps = Math::min<UnsignedInt>(imageLevels.size(), Math::log2(size.max()) + 1);
    if(imageLevels.size() > numMipmaps)
        Warning{} << "Trade::KtxImageConverter::convertToData(): expected at most" <<
            numMipmaps << "mip level images but got" << imageLevels.size() << Debug::nospace <<
            ", extra images will be ignored";

    Containers::Array<Implementation::KtxLevel> levelIndex{numMipmaps};

    const std::size_t levelIndexSize = numMipmaps*sizeof(Implementation::KtxLevel);
    std::size_t levelOffset = sizeof(Implementation::KtxHeader) + levelIndexSize +
        dataFormatDescriptor.size() + keyValueData.size();

    /** @todo Handle block-compressed formats. Need block size and block count
              in each dimension. How should skip be handled, if it's not a
              multiple of the block size? */
    const UnsignedInt pixelSize = imageLevels.front().pixelSize();

    for(UnsignedInt i = 0; i != levelIndex.size(); ++i) {
        /* Store mip levels from smallest to largest for efficient streaming */
        const UnsignedInt mip = levelIndex.size() - 1 - i;
        const Math::Vector<dimensions, Int> mipSize = Math::max(size >> mip, 1);

        const View<dimensions, T>& image = imageLevels[mip];

        if(image.format() != format) {
            Error() << "Trade::KtxImageConverter::convertToData(): expected "
                "format" << format << "for level" << mip << "but got" << image.format();
            return {};
        }

        if(image.size() != mipSize) {
            Error() << "Trade::KtxImageConverter::convertToData(): expected "
                "size" << mipSize << "for level" << mip << "but got" << image.size();
            return {};
        }

        if(!image.data()) {
            Error() << "Trade::KtxImageConverter::convertToData(): image data "
                "for level" << mip << "is nullptr";
            return {};
        }
        
        /* Offset needs to be aligned to the least common multiple of the
           texel/block size and 4. Not needed with supercompression. */
        const std::size_t alignment = leastCommonMultiple(pixelSize, 4);
        levelOffset = (levelOffset + (alignment - 1))/alignment*alignment;
        const std::size_t levelSize = pixelSize*image.size().product();

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
    header.typeSize = componentSize(format);
    header.imageSize = Vector3ui{Vector3i::pad(size, 0u)};
    /** @todo Handle different image types (cube and/or array) once this can be
              queried from images */
    header.layerCount = 0;
    header.faceCount = 1;
    header.levelCount = levelIndex.size();
    header.supercompressionScheme = Implementation::SuperCompressionScheme::None;

    for(UnsignedInt i = 0; i != levelIndex.size(); ++i) {
        const Implementation::KtxLevel& level = levelIndex[i];
        const View<dimensions, T>& image = imageLevels[i];
        const auto pixels = data.suffix(level.byteOffset).prefix(level.byteLength);
        copyPixels(image, pixels);

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

}

KtxImageConverter::KtxImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures KtxImageConverter::doFeatures() const {
    return ImageConverterFeature::ConvertLevels1DToData |
        ImageConverterFeature::ConvertLevels2DToData |
        ImageConverterFeature::ConvertLevels3DToData |
        ImageConverterFeature::ConvertCompressedLevels1DToData |
        ImageConverterFeature::ConvertCompressedLevels2DToData |
        ImageConverterFeature::ConvertCompressedLevels3DToData;
}

Containers::Array<char> KtxImageConverter::doConvertToData(Containers::ArrayView<const ImageView1D> imageLevels) {
    return convertLevels(imageLevels);
}

Containers::Array<char> KtxImageConverter::doConvertToData(Containers::ArrayView<const ImageView2D> imageLevels) {
    return convertLevels(imageLevels);
}

Containers::Array<char> KtxImageConverter::doConvertToData(Containers::ArrayView<const ImageView3D> imageLevels) {
    return convertLevels(imageLevels);
}

Containers::Array<char> KtxImageConverter::doConvertToData(Containers::ArrayView<const CompressedImageView1D> imageLevels) {
    //return convertLevels(imageLevels);
    return {};
}

Containers::Array<char> KtxImageConverter::doConvertToData(Containers::ArrayView<const CompressedImageView2D> imageLevels) {
    //return convertLevels(imageLevels);
    return {};
}

Containers::Array<char> KtxImageConverter::doConvertToData(Containers::ArrayView<const CompressedImageView3D> imageLevels) {
    //return convertLevels(imageLevels);
    return {};
}

}}

CORRADE_PLUGIN_REGISTER(KtxImageConverter, Magnum::Trade::KtxImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3.1")
