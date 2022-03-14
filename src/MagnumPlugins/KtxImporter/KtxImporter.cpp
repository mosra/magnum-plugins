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

#include "KtxImporter.h"

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once PluginMetadata is <string>-free */
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/EndiannessBatch.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/BoolVector.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Swizzle.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/TextureData.h>
#include "MagnumPlugins/KtxImporter/KtxHeader.h"

namespace Magnum { namespace Trade {

namespace {

template<std::size_t> struct TypeForSize {};
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

enum SwizzleType: UnsignedByte {
    None = 0,
    BGR,
    BGRA
};

inline SwizzleType& operator^=(SwizzleType& a, SwizzleType b) {
    /* This is meant to toggle single enum values, make sure it's not being
       used for other bit-fiddling crimes */
    CORRADE_INTERNAL_ASSERT(a == SwizzleType::None || a == b);
    return a = SwizzleType(a ^ b);
}

/** @todo implement these in TextureTools(?) with proper optimizations,
    together with an ability to endian-swap on the fly */
template<typename T> void swizzlePixels(SwizzleType type, Containers::ArrayView<char> data) {
    if(type == SwizzleType::BGR) {
        for(auto& pixel: Containers::arrayCast<Math::Vector3<T>>(data))
            pixel = Math::gather<'b', 'g', 'r'>(pixel);
    } else if(type == SwizzleType::BGRA) {
        for(auto& pixel: Containers::arrayCast<Math::Vector4<T>>(data))
            pixel = Math::gather<'b', 'g', 'r', 'a'>(pixel);
    }
}

void swizzlePixels(SwizzleType type, UnsignedInt typeSize, Containers::ArrayView<char> data) {
    switch(typeSize) {
        case 1:
            swizzlePixels<TypeForSize<1>::Type>(type, data);
            return;
        case 2:
            swizzlePixels<TypeForSize<2>::Type>(type, data);
            return;
        case 4:
            swizzlePixels<TypeForSize<4>::Type>(type, data);
            return;
        case 8:
            swizzlePixels<TypeForSize<8>::Type>(type, data);
            return;
    }

    CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

struct Format {
    union {
        PixelFormat uncompressed;
        CompressedPixelFormat compressed;
    };

    bool isCompressed;
    bool isDepth;

    /* Size of entire pixel/block */
    UnsignedInt size;
    /* Compressed block size, (1,1,1) for uncompressed formats */
    Vector3i blockSize;
    /* Size of underlying data type, 1 for block-compressed formats */
    UnsignedInt typeSize;

    SwizzleType swizzle;
};

Containers::Optional<Format> decodeFormat(Implementation::VkFormat vkFormat) {
    Format f{};

    /* Find uncompressed pixel format. Note that none of the formats forbidden
       by KTX are supported by Magnum, so we filter those at the same time.
       This might change in the future, but we'll be fine as long as
       formatMapping.hpp isn't updated without adding an extra check. */
    PixelFormat format{};
    switch(vkFormat) {
        #define _c(vulkan, magnum, _type) case vulkan: format = PixelFormat::magnum; break;
        #include "MagnumPlugins/KtxImporter/formatMapping.hpp"
        #undef _c
        default:
            break;
    }

    /* PixelFormat doesn't contain any of the swizzled formats. Figure it out
       from the Vulkan format and remember that we need to swizzle in
       doImage(). */
    if(format == PixelFormat{}) {
        switch(vkFormat) {
            case Implementation::VK_FORMAT_B8G8R8_UNORM:
                format = PixelFormat::RGB8Unorm;  break;
            case Implementation::VK_FORMAT_B8G8R8_SNORM:
                format = PixelFormat::RGB8Snorm;  break;
            case Implementation::VK_FORMAT_B8G8R8_UINT:
                format = PixelFormat::RGB8UI;     break;
            case Implementation::VK_FORMAT_B8G8R8_SINT:
                format = PixelFormat::RGB8I;      break;
            case Implementation::VK_FORMAT_B8G8R8_SRGB:
                format = PixelFormat::RGB8Srgb;   break;
            case Implementation::VK_FORMAT_B8G8R8A8_UNORM:
                format = PixelFormat::RGBA8Unorm; break;
            case Implementation::VK_FORMAT_B8G8R8A8_SNORM:
                format = PixelFormat::RGBA8Snorm; break;
            case Implementation::VK_FORMAT_B8G8R8A8_UINT:
                format = PixelFormat::RGBA8UI;    break;
            case Implementation::VK_FORMAT_B8G8R8A8_SINT:
                format = PixelFormat::RGBA8I;     break;
            case Implementation::VK_FORMAT_B8G8R8A8_SRGB:
                format = PixelFormat::RGBA8Srgb;  break;
            default:
                break;
        }

        if(format != PixelFormat{}) {
            f.size = pixelSize(format);
            CORRADE_INTERNAL_ASSERT(f.size == 3 || f.size == 4);
            f.swizzle = (f.size == 3) ? SwizzleType::BGR : SwizzleType::BGRA;
        }
    }

    if(format != PixelFormat{}) {
        /* Depth formats are allowed by KTX. We only really use isDepth for
           validation. */
        switch(format) {
            case PixelFormat::Depth16Unorm:
            case PixelFormat::Depth24Unorm:
            case PixelFormat::Depth32F:
            case PixelFormat::Stencil8UI:
            case PixelFormat::Depth16UnormStencil8UI:
            case PixelFormat::Depth24UnormStencil8UI:
            case PixelFormat::Depth32FStencil8UI:
                f.isDepth = true;
                break;
            default:
                /* PixelFormat covers all of Vulkan's depth formats */
                break;
        }

        f.size = pixelSize(format);
        f.blockSize = {1, 1, 1};
        f.uncompressed = format;
        return f;
    }

    /* Find block-compressed pixel format, no swizzling possible */
    /** @todo KTX supports 3D ASTC formats through an unreleased extension.
        Supposedly the enum values won't change so we could manually map them,
        although it'd be easier if Magnum did this. See
        https://github.com/KhronosGroup/KTX-Specification/pull/97 and
        https://github.com/KhronosGroup/KTX-Software/blob/f99221eb1c5ad92fd859765a0c66517ea4059160/lib/dfdutils/vulkan/vulkan_core.h#L1061 */
    CompressedPixelFormat compressedFormat{};
    switch(vkFormat) {
        #define _c(vulkan, magnum, _type) case vulkan: compressedFormat = CompressedPixelFormat::magnum; break;
        #include "MagnumPlugins/KtxImporter/compressedFormatMapping.hpp"
        #undef _c
        default:
            break;
    }

    if(compressedFormat != CompressedPixelFormat{}) {
        f.size = compressedBlockDataSize(compressedFormat);
        f.blockSize = compressedBlockSize(compressedFormat);
        f.compressed = compressedFormat;
        f.isCompressed = true;
        return f;
    }

    /** @todo Support all Vulkan formats allowed by the KTX spec. Create custom
        PixelFormat with pixelFormatWrap and manually fill PixelStorage/
        CompressedPixelStorage. We can take all the necessary info from
        https://github.com/KhronosGroup/KTX-Specification/blob/master/formats.json

        Do we also want this for the KtxImageConverter? This would allow users
        to pass in images with implementation-specific PixelFormat using the
        Vulkan format enum directly, however it's unclear how to fill in the
        DFD for formats we're not aware of. Isn't it easier to just support all
        Vulkan formats in PixelFormat? */

    return {};
}

}

struct KtxImporter::File {
    struct LevelData {
        Vector3i size;
        Containers::ArrayView<char> data;
    };

    Containers::Array<char> in;

    /* Dimensions of the source image (1-3) */
    UnsignedByte numDimensions;
    /* Dimensions of the imported image data, including extra dimensions for
       array layers or cube map faces */
    UnsignedByte numDataDimensions;
    TextureType type;
    BoolVector3 flip;

    Format pixelFormat;

    /* Usually only one image with n or n+1 dimensions, multiple images for
       3D array layers */
    Containers::Array<Containers::Array<LevelData>> imageData;
};

KtxImporter::KtxImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

KtxImporter::~KtxImporter() = default;

ImporterFeatures KtxImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool KtxImporter::doIsOpened() const {
    /* Only one of these can be populated at a time */
    CORRADE_INTERNAL_ASSERT(!_f || !_basisImporter);
    return _f || (_basisImporter && _basisImporter->isOpened());
}

void KtxImporter::doClose() {
    _f = nullptr;
    _basisImporter = nullptr;
}

void KtxImporter::doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) {
    /* Check if the file is long enough for the header */
    if(data.size() < sizeof(Implementation::KtxHeader)) {
        Error{} << "Trade::KtxImporter::openData(): file too short, expected"
            << sizeof(Implementation::KtxHeader)
            << "bytes for the header but got only" << data.size();
        return;
    }

    Implementation::KtxHeader header = *reinterpret_cast<const Implementation::KtxHeader*>(data.data());

    /* KTX2 uses little-endian everywhere */
    Utility::Endianness::littleEndianInPlace(
        header.vkFormat, header.typeSize,
        header.imageSize[0], header.imageSize[1], header.imageSize[2],
        header.layerCount, header.faceCount, header.levelCount,
        header.supercompressionScheme,
        header.dfdByteOffset, header.dfdByteLength,
        header.kvdByteOffset, header.kvdByteLength);

    using namespace Containers::Literals;

    /* Check magic string */
    const Containers::StringView identifier{header.identifier, sizeof(header.identifier)};
    const Containers::StringView expected{Implementation::KtxFileIdentifier, sizeof(header.identifier)};
    if(identifier != expected) {
        /* Print a useful error for a KTX file with an unsupported version.
           KTX1 uses the same magic string but with a different version string. */
        if(identifier.hasPrefix(expected.prefix(Implementation::KtxFileVersionOffset))) {
            const Containers::StringView version = identifier.exceptPrefix(Implementation::KtxFileVersionOffset).prefix(Implementation::KtxFileVersionLength);
            if(version != "20"_s) {
                Error{} << "Trade::KtxImporter::openData(): unsupported KTX version, expected 20 but got" << version;
                return;
            }
        }

        Error{} << "Trade::KtxImporter::openData(): wrong file signature";
        return;
    }

    /* Read header data and perform some sanity checks, including byte ranges */

    if(header.imageSize.x() == 0) {
        Error{} << "Trade::KtxImporter::openData(): invalid image size, width is 0";
        return;
    }

    Containers::Pointer<File> f{InPlaceInit};

    /* Number of array layers, imported as extra image dimensions (except
       for 3D images, there it's one Image3D per layer).

       layerCount == 1 is an array image with one level, we export it as such
       so that there are no surprises. This is equivalent to how we handle
       depth == 1. */
    const bool isLayered = header.layerCount > 0;
    const UnsignedInt numLayers = Math::max(header.layerCount, 1u);

    const bool isCubeMap = header.faceCount == 6;
    const UnsignedInt numFaces = header.faceCount;

    /* Get image dimensions and remember the type for doTexture() */
    if(header.imageSize.y() > 0) {
        if(header.imageSize.z() > 0) {
            f->numDimensions = 3;
            f->type = TextureType::Texture3D;
        } else {
            f->numDimensions = 2;
            if(isCubeMap)
                f->type = isLayered ? TextureType::CubeMapArray : TextureType::CubeMap;
            else
                f->type = isLayered ? TextureType::Texture2DArray : TextureType::Texture2D;
        }
    } else if(header.imageSize.z() > 0) {
        Error{} << "Trade::KtxImporter::openData(): invalid image size, depth is" << header.imageSize.z() << "but height is 0";
        return;
    } else {
        f->numDimensions = 1;
        f->type = isLayered ? TextureType::Texture1DArray : TextureType::Texture1D;
    }

    f->numDataDimensions = Math::min<UnsignedByte>(f->numDimensions + UnsignedByte(isLayered || isCubeMap), 3);

    CORRADE_INTERNAL_ASSERT(f->numDimensions >= 1 && f->numDimensions <= 3);
    CORRADE_INTERNAL_ASSERT(f->numDataDimensions >= f->numDimensions);
    CORRADE_INTERNAL_ASSERT(f->numDataDimensions - f->numDimensions <= 1);

    /* Make size 1 instead of 0 in missing dimensions for simpler calculations */
    const Vector3i size = Math::max(Vector3i{header.imageSize}, 1);

    if(numFaces != 1) {
        if(numFaces != 6) {
            Error{} << "Trade::KtxImporter::openData(): expected either 1 or 6 faces for cube maps but got" << numFaces;
            return;
        }

        if(f->numDimensions != 2 || size.x() != size.y()) {
            Error{} << "Trade::KtxImporter::openData(): cube map dimensions must be 2D and square, but got" << header.imageSize;
            return;
        }
    }

    /* levelCount == 0 indicates to the user/importer to generate mipmaps. We
       don't really care either way since we don't generate mipmaps or pass
       this on to the user. */
    const UnsignedInt numMipmaps = Math::max(header.levelCount, 1u);

    const UnsignedInt maxLevelCount = Math::log2(size.max()) + 1;
    if(numMipmaps > maxLevelCount) {
        Error{} << "Trade::KtxImporter::openData(): expected at most" <<
            maxLevelCount << "mip levels but got" << numMipmaps;
        return;
    }

    const std::size_t levelIndexEnd = sizeof(header) + numMipmaps*sizeof(Implementation::KtxLevel);
    if(data.size() < levelIndexEnd) {
        Error{} << "Trade::KtxImporter::openData(): file too short, expected" <<
            levelIndexEnd << "bytes for level index but got only" << data.size();
        return;
    }

    const std::size_t kvdEnd = header.kvdByteOffset + header.kvdByteLength;
    if(data.size() < kvdEnd) {
        Error{} << "Trade::KtxImporter::openData(): file too short, expected" <<
            kvdEnd << "bytes for key/value data but got only" << data.size();
        return;
    }

    /* Detect Basis-compressed files and forward to BasisImporter. Any other
       custom format is not supported. */
    if(header.vkFormat == Implementation::VK_FORMAT_UNDEFINED) {
        /* BasisLZ (Basis ETC1 + LZ compression) is indicated by the
           supercompression scheme. Basis UASTC on the other hand is indicated
           by the DFD color model. */
        if(header.supercompressionScheme != Implementation::SuperCompressionScheme::BasisLZ) {
            /* This is the only place we need to read the DFD so all checks
                can reside here */
            const std::size_t dfdEnd = header.dfdByteOffset + header.dfdByteLength;
            if(data.size() < dfdEnd) {
                Error{} << "Trade::KtxImporter::openData(): file too short, expected" <<
                    dfdEnd << "bytes for data format descriptor but got only" << data.size();
                return;
            }

            if(header.dfdByteLength < sizeof(UnsignedInt) + sizeof(Implementation::KdfBasicBlockHeader)) {
                Error{} << "Trade::KtxImporter::openData(): data format descriptor too short, "
                    "expected at least" << sizeof(UnsignedInt) + sizeof(Implementation::KdfBasicBlockHeader) <<
                    "bytes but got" << header.dfdByteLength;
                return;
            }

            const auto& dfd = *reinterpret_cast<const Implementation::KdfBasicBlockHeader*>(
                data.exceptPrefix(header.dfdByteOffset + sizeof(UnsignedInt)).data());

            /* colorModel is a byte, no need to endian-swap */
            if(dfd.colorModel != Implementation::KdfBasicBlockHeader::ColorModel::BasisUastc) {
                Error{} << "Trade::KtxImporter::openData(): custom formats are not supported";
                return;
            }
        }

        /* Try to load the BasisImporter plugin. The manager will print a
           message on its own when not found, so just explain why we need it. */
        const std::string plugin = "BasisImporter";
        if(!(manager()->load(plugin) & PluginManager::LoadState::Loaded)) {
            Error{} << "Trade::KtxImporter::openData(): can't forward a Basis Universal image to BasisImporter";
            return;
        }

        if(flags() & ImporterFlag::Verbose) {
            const PluginManager::PluginMetadata* const metadata = manager()->metadata(plugin);
            CORRADE_INTERNAL_ASSERT(metadata);
            Debug{} << "Trade::KtxImporter::openData(): image is compressed with Basis Universal, forwarding to" << metadata->name();
        }

        /* Instantiate the plugin, propagate flags */
        Containers::Pointer<AbstractImporter> basisImporter = static_cast<PluginManager::Manager<AbstractImporter>*>(manager())->instantiate(plugin);
        basisImporter->setFlags(flags());

        /* Propagate format. If not set here, keep whatever was set for
           BasisImporter (as users are instructed to set the option globally).
           If both are set, a value in this plugin gets a precendence, however
           print a warning in that case. */
        const auto format = configuration().group("basis")->value<Containers::StringView>("format");
        if(!format.isEmpty()) {
            const auto originalFormat = basisImporter->configuration().value<Containers::StringView>("format");
            if(!originalFormat.isEmpty() && format != originalFormat)
                Warning{} << "Trade::KtxImporter::openData(): overwriting BasisImporter format from" << originalFormat << "to" << format;
            basisImporter->configuration().setValue("format", format);
        }

        /* Try to open the data with BasisImporter (error output should be
           printed by the plugin itself). All other functions transparently
           forward to that importer instance if it's populated. */
        if(!basisImporter->openData(data)) return;

        /* Success, save the instance */
        _basisImporter = std::move(basisImporter);
        return;
    }

    /** @todo Support supercompression */
    if(header.supercompressionScheme != Implementation::SuperCompressionScheme::None) {
        Error{} << "Trade::KtxImporter::openData(): supercompression is currently not supported";
        return;
    }

    /* typeSize is the size of the format's underlying type, not the texel
       size, e.g. 2 for RG16F. For any sane format it should be a
       power-of-two between 1 and 8. */
    if(header.typeSize < 1 || header.typeSize > 8 ||
        (header.typeSize & (header.typeSize - 1)))
    {
        Error{} << "Trade::KtxImporter::openData(): unsupported type size" << header.typeSize;
        return;
    }

    /* Get generic format info from Vulkan format */
    const auto pixelFormat = decodeFormat(header.vkFormat);
    if(!pixelFormat) {
        Error{} << "Trade::KtxImporter::openData(): unsupported format" << header.vkFormat;
        return;
    }
    f->pixelFormat = *pixelFormat;

    if(f->pixelFormat.isDepth && f->numDimensions == 3) {
        Error{} << "Trade::KtxImporter::openData(): 3D images can't have depth/stencil format";
        return;
    }

    /* Block-compressed formats have no implicit swizzle */
    CORRADE_INTERNAL_ASSERT(!f->pixelFormat.isCompressed || f->pixelFormat.swizzle == SwizzleType::None);

    if(f->pixelFormat.isCompressed && header.typeSize != 1) {
        Error{} << "Trade::KtxImporter::openData(): invalid type size for compressed format, expected 1 but got" << header.typeSize;
        return;
    }
    f->pixelFormat.typeSize = header.typeSize;

    /* Take over the existing array or copy the data if we can't */
    if(dataFlags & (DataFlag::Owned|DataFlag::ExternallyOwned)) {
        f->in = std::move(data);
    } else {
        f->in = Containers::Array<char>{NoInit, data.size()};
        Utility::copy(data, f->in);
    }

    /* The level index contains byte ranges for each mipmap, from largest to
       smallest. Each mipmap contains tightly packed images ordered by
       layers, faces/slices, rows, columns. */
    const std::size_t levelIndexSize = numMipmaps*sizeof(Implementation::KtxLevel);
    const auto levelIndex = Containers::arrayCast<Implementation::KtxLevel>(
        f->in.exceptPrefix(sizeof(Implementation::KtxHeader)).prefix(levelIndexSize));

    /* Extract image data views. Only one image with extra dimensions for array
       layers and/or cube map faces, except for 3D array images where it's one
       image per layer. */
    const bool is3DArrayImage = f->numDimensions == 3 && isLayered;
    const UnsignedInt numImages = is3DArrayImage ? numLayers : 1;
    f->imageData = Containers::Array<Containers::Array<File::LevelData>>{numImages};
    for(UnsignedInt image = 0; image != numImages; ++image)
        f->imageData[image] = Containers::Array<File::LevelData>{numMipmaps};

    Vector3i mipSize{size};
    for(UnsignedInt i = 0; i != numMipmaps; ++i) {
        auto& level = levelIndex[i];
        Utility::Endianness::littleEndianInPlace(level.byteOffset,
            level.byteLength, level.uncompressedByteLength);

        /* Both lengths should be equal without supercompression. Be lenient here
           and only emit a warning in case some shitty exporter gets this wrong. */
        if(header.supercompressionScheme == Implementation::SuperCompressionScheme::None &&
            level.byteLength != level.uncompressedByteLength)
        {
            Warning{} << "Trade::KtxImporter::openData(): byte length" << level.byteLength
                << "is not equal to uncompressed byte length" << level.uncompressedByteLength
                << "for an image without supercompression, ignoring the latter";
        }

        const std::size_t levelEnd = level.byteOffset + level.byteLength;
        if(f->in.size() < levelEnd) {
            Error{} << "Trade::KtxImporter::openData(): file too short, expected"
                << levelEnd << "bytes for level data but got only" << f->in.size();
            return;
        }

        Vector3i levelSize = mipSize;
        if(f->numDimensions < f->numDataDimensions) {
            CORRADE_INTERNAL_ASSERT(!is3DArrayImage);
            CORRADE_INTERNAL_ASSERT(levelSize[f->numDimensions] == 1);
            levelSize[f->numDimensions] = numFaces*numLayers;
        }

        std::size_t imageLength;
        if(f->pixelFormat.isCompressed) {
            const Vector3i& blockSize = f->pixelFormat.blockSize;
            const Vector3i blockCount = (levelSize + (blockSize - Vector3i{1}))/blockSize;
            imageLength = blockCount.product()*f->pixelFormat.size;
        } else
            imageLength = levelSize.product()*f->pixelFormat.size;
        const std::size_t totalLength = imageLength*numImages;

        if(level.byteLength < totalLength) {
            Error{} << "Trade::KtxImporter::openData(): level data too short, "
                "expected at least" << totalLength << "bytes but got" << level.byteLength;
            return;
        }

        for(UnsignedInt image = 0; image != numImages; ++image) {
            const std::size_t offset = level.byteOffset + image*imageLength;
            f->imageData[image][i] = {levelSize, f->in.exceptPrefix(offset).prefix(imageLength)};
        }

        /* Halve each dimension, rounding down */
        mipSize = Math::max(mipSize >> 1, 1);
    }

    /* Read key/value data, optional */

    enum KeyValueType: UnsignedByte {
        CubeMapIncomplete,
        Orientation,
        Swizzle,
        Count
    };

    struct KeyValueEntry {
        Containers::StringView key;
        Containers::ArrayView<const char> value;
    } keyValueEntries[KeyValueType::Count]{
        {"KTXcubemapIncomplete"_s, {}},
        {"KTXorientation"_s, {}},
        {"KTXswizzle"_s, {}},
    };

    if(header.kvdByteLength > 0) {
        Containers::ArrayView<const char> keyValueData{f->in.exceptPrefix(header.kvdByteOffset).prefix(header.kvdByteLength)};
        /* Loop through entries, each one consisting of:

           UnsignedInt length
           Byte data[length]
           Byte padding[...]

           data[] begins with a zero-terminated key, the rest of the bytes is
           the value content. Value alignment must be implicitly done through
           key length, hence the funny KTX keys with multiple underscores. Any
           multi-byte numbers in values must be endian-swapped later. */
        UnsignedInt current = 0;
        while(current + sizeof(UnsignedInt) < keyValueData.size()) {
            /* Length without padding */
            const UnsignedInt length = *reinterpret_cast<const UnsignedInt*>(keyValueData.exceptPrefix(current).data());
            Utility::Endianness::littleEndianInPlace(length);
            current += sizeof(length);

            if(current + length <= keyValueData.size()) {
                const Containers::StringView entry{keyValueData.exceptPrefix(current).prefix(length)};
                const Containers::Array3<Containers::StringView> split = entry.partition('\0');
                const auto key = split[0];
                const auto value = split[2];

                if(key.isEmpty())
                    Warning{} << "Trade::KtxImporter::openData(): invalid key/value entry, skipping";
                else {
                    for(UnsignedInt i = 0; i != Containers::arraySize(keyValueEntries); ++i) {
                        if(key == keyValueEntries[i].key) {
                            if(!keyValueEntries[i].value.isEmpty())
                                Warning{} << "Trade::KtxImporter::openData(): key" << key << "already set, skipping";
                            else
                                keyValueEntries[i].value = value;
                            break;
                        }
                    }
                }
            }
            /* Length value is dword-aligned, guaranteed for the first length
               by the file layout */
            current += (length + 3)/4*4;
        }
    }

    /* Read image orientation so we can flip if needed.

       l/r = left/right
       u/d = up/down
       o/i = out of/into screen

       The spec strongly recommends defaulting to rdi, Magnum/GL expects ruo. */
    {
        constexpr auto targetOrientation = "ruo"_s;

        bool useDefaultOrientation = true;
        const Containers::StringView orientation{keyValueEntries[KeyValueType::Orientation].value};
        /* If the orientation string is too short or invalid, a warning gets
           printed and the default is used.
           Strings that are null-terminated but too short pass the first if
           but get caught by the test for valid orientation characters. */
        if(orientation.size() >= f->numDimensions) {
            constexpr Containers::StringView validOrientations[3]{"rl"_s, "du"_s, "io"_s};
            for(UnsignedByte i = 0; i != f->numDimensions; ++i) {
                useDefaultOrientation = !validOrientations[i].contains(orientation[i]);
                if(useDefaultOrientation) break;
                f->flip.set(i, orientation[i] != targetOrientation[i]);
            }
        }

        if(useDefaultOrientation) {
            constexpr auto defaultOrientation = "rdi"_s;
            constexpr Containers::StringView defaultDirections[3]{
                "right"_s, "down"_s, "forward"_s
            };

            const BoolVector3 flip = Math::notEqual(
                Math::Vector3<char>::from(defaultOrientation.data()),
                Math::Vector3<char>::from(targetOrientation.data()));
            /* Leave non-existing dimensions unset, otherwise they affect the
               result of any() and none() */
            for(UnsignedByte i = 0; i != f->numDimensions; ++i)
                f->flip.set(i, flip[i]);

            Warning{} << "Trade::KtxImporter::openData(): missing or invalid "
                "orientation, assuming" << ", "_s.join(Containers::arrayView(defaultDirections).prefix(f->numDimensions));
        }
    }

    /* We can't reasonably perform axis flips on block-compressed data.
       Emit a warning and pretend there is no flipping necessary. */
    if(f->pixelFormat.isCompressed && f->flip.any()) {
        f->flip = BoolVector3{};
        Warning{} << "Trade::KtxImporter::openData(): block-compressed image "
            "was encoded with non-default axis orientations, imported data "
            "will have wrong orientation";
    }

    /** @todo KTX spec seems to really insist on rd for cube maps but the
        wording is odd, I can't tell if they're saying it's mandatory or not:
        https://www.khronos.org/registry/KTX/specs/2.0/ktxspec_v2.html#cubemapOrientation
        The toktx tool from Khronos Texture Tools also forces rd for cube maps,
        so we might want to do that in the converter as well. */

    /* Incomplete cube maps are a 'feature' of KTX files. We just import them
       as layers (which is how they're exposed to us). */
    if(numFaces != 6 && !keyValueEntries[KeyValueType::CubeMapIncomplete].value.isEmpty()) {
        Warning{} << "Trade::KtxImporter::openData(): image contains incomplete "
            "cube map faces, importing faces as array layers";
    }

    /* Read swizzle information */
    if(!f->pixelFormat.isDepth) {
        /* Remove trailing zero and anything after it */
        auto swizzle = Containers::StringView{keyValueEntries[KeyValueType::Swizzle].value}.partition('\0')[0];
        if(!swizzle.isEmpty()) {
            /* We don't know the channel count of block-compressed formats so
               comparing only the necessary prefix of the swizzle string can't
               be done for them. The spec says to use 0 and 1 for missing
               channels so in theory this is possible, but anyone who wants an
               identity swizzle simply won't have an entry in the key/value
               data in the first place. Reading the DFD for getting the channel
               count is terrible overkill, so try our best for normal formats
               and leave it at that. */
            if(!f->pixelFormat.isCompressed) {
                const std::size_t numChannels = f->pixelFormat.size / f->pixelFormat.typeSize;
                swizzle = swizzle.prefix(Math::min(numChannels, swizzle.size()));
            }
            if(!"rgba"_s.hasPrefix(swizzle)) {
                bool handled = false;
                /* Special cases already supported for 8-bit Vulkan formats */
                if(!f->pixelFormat.isCompressed) {
                    if(swizzle == "bgr"_s) {
                        f->pixelFormat.swizzle ^= SwizzleType::BGR;
                        handled = true;
                    } else if(swizzle == "bgra"_s) {
                        f->pixelFormat.swizzle ^= SwizzleType::BGRA;
                        handled = true;
                    }
                }
                if(!handled) {
                    Error{} << "Trade::KtxImporter::openData(): unsupported channel mapping" << swizzle;
                    return;
                }
            }
        }
    }

    if(flags() & ImporterFlag::Verbose) {
        if(f->flip.any()) {
            const Containers::StringView axes[3]{
                f->flip[0] ? "x"_s : ""_s,
                f->flip[1] ? "y"_s : ""_s,
                f->flip[2] ? "z"_s : ""_s,
            };
            Debug{} << "Trade::KtxImporter::openData(): image will be flipped along" << " and "_s.joinWithoutEmptyParts(axes);
        }

        switch(f->pixelFormat.swizzle) {
            case SwizzleType::BGR:
                Debug{} << "Trade::KtxImporter::openData(): format requires conversion from BGR to RGB";
                break;
            case SwizzleType::BGRA:
                Debug{} << "Trade::KtxImporter::openData(): format requires conversion from BGRA to RGBA";
                break;
            default:
                break;
        }
    }

    /** @todo Read KTXanimData and expose frame time between images */

    _f = std::move(f);
}

template<UnsignedInt dimensions> ImageData<dimensions> KtxImporter::doImage(UnsignedInt id, UnsignedInt level) {
    const File::LevelData& levelData = _f->imageData[id][level];
    const auto size = Math::Vector<dimensions, Int>::pad(levelData.size);
    Containers::Array<char> data{NoInit, levelData.data.size()};

    /* Block-compressed images don't have any flipping, swizzling or endian
       swapping performed on them. Special-casing this mainly to avoid having
       to calculate the block count for the strided array view. We already know
       the entire data size. */
    if(_f->pixelFormat.isCompressed) {
        CORRADE_INTERNAL_ASSERT(_f->flip.none());
        CORRADE_INTERNAL_ASSERT(_f->pixelFormat.swizzle == SwizzleType::None);
        CORRADE_INTERNAL_ASSERT(_f->pixelFormat.typeSize == 1);

        Utility::copy(levelData.data, data);
        return ImageData<dimensions>(_f->pixelFormat.compressed, size, std::move(data));
    }

    /* Uncompressed image */

    /* Copy image data, flipping along axes if necessary. Assuming src is
       tightly packed, stride gets calculated implicitly. */
    Containers::StridedArrayView4D<const char> src{levelData.data, {
        std::size_t(levelData.size.z()),
        std::size_t(levelData.size.y()),
        std::size_t(levelData.size.x()),
        _f->pixelFormat.size
    }};
    Containers::StridedArrayView4D<char> dst{data, src.size()};

    if(_f->flip[2]) src = src.flipped<0>();
    if(_f->flip[1]) src = src.flipped<1>();
    if(_f->flip[0]) src = src.flipped<2>();

    /* Without flipped dimensions this becomes a single memcpy */
    Utility::copy(src, dst);

    /* Swizzle BGR(A) if necessary */
    swizzlePixels(_f->pixelFormat.swizzle, _f->pixelFormat.typeSize, data);

    endianSwap(data, _f->pixelFormat.typeSize);

    /* Adjust pixel storage if row size is not four byte aligned */
    PixelStorage storage;
    if((levelData.size.x()*_f->pixelFormat.size)%4 != 0)
        storage.setAlignment(1);

    return ImageData<dimensions>{storage, _f->pixelFormat.uncompressed, size, std::move(data)};
}

UnsignedInt KtxImporter::doImage1DCount() const {
    if(_basisImporter)
        return _basisImporter->image1DCount();
    else if(_f->numDataDimensions == 1)
        return _f->imageData.size();
    else
        return 0;
}

UnsignedInt KtxImporter::doImage1DLevelCount(UnsignedInt id)  {
    if(_basisImporter)
        return _basisImporter->image1DLevelCount(id);
    else
        return _f->imageData[id].size();
}

Containers::Optional<ImageData1D> KtxImporter::doImage1D(UnsignedInt id, UnsignedInt level) {
    if(_basisImporter)
        return _basisImporter->image1D(id, level);
    else
        return doImage<1>(id, level);
}

UnsignedInt KtxImporter::doImage2DCount() const {
    if(_basisImporter)
        return _basisImporter->image2DCount();
    else if(_f->numDataDimensions == 2)
        return _f->imageData.size();
    else
        return 0;
}

UnsignedInt KtxImporter::doImage2DLevelCount(UnsignedInt id) {
    if(_basisImporter)
        return _basisImporter->image2DLevelCount(id);
    else
        return _f->imageData[id].size();
}

Containers::Optional<ImageData2D> KtxImporter::doImage2D(UnsignedInt id, UnsignedInt level) {
    if(_basisImporter)
        return _basisImporter->image2D(id, level);
    else
        return doImage<2>(id, level);
}

UnsignedInt KtxImporter::doImage3DCount() const {
    if(_basisImporter)
        return _basisImporter->image3DCount();
    else if(_f->numDataDimensions == 3)
        return _f->imageData.size();
    else
        return 0;
}

UnsignedInt KtxImporter::doImage3DLevelCount(UnsignedInt id) {
    if(_basisImporter)
        return _basisImporter->image3DLevelCount(id);
    else
        return _f->imageData[id].size();
}

Containers::Optional<ImageData3D> KtxImporter::doImage3D(UnsignedInt id, const UnsignedInt level) {
    if(_basisImporter)
        return _basisImporter->image3D(id, level);
    else
        return doImage<3>(id, level);
}

UnsignedInt KtxImporter::doTextureCount() const {
    if(_basisImporter)
        return _basisImporter->textureCount();
    else
        return _f->imageData.size();
}

Containers::Optional<TextureData> KtxImporter::doTexture(UnsignedInt id) {
    if(_basisImporter)
        return _basisImporter->texture(id);
    else
        return TextureData{_f->type, SamplerFilter::Linear, SamplerFilter::Linear,
            SamplerMipmap::Linear, SamplerWrapping::Repeat, id};
}

}}

CORRADE_PLUGIN_REGISTER(KtxImporter, Magnum::Trade::KtxImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.5")
