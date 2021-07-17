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

#include "KtxImporter.h"

#include <algorithm>
#include <map>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/EndiannessBatch.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Swizzle.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Vk/PixelFormat.h>
#include "MagnumPlugins/KtxImporter/KtxHeader.h"

namespace Magnum { namespace Trade {

namespace {

std::size_t imageLength(const Vector3i& size, PixelFormat format) {
    return size.product()*pixelSize(format);
}

/** @todo Use CompressedPixelStorage::dataProperties for this */
std::size_t imageLength(const Vector3i& size, CompressedPixelFormat format) {
    const Vector3i blockSize = compressedBlockSize(format);
    const Vector3i blockCount = (size + (blockSize - Vector3i{1})) / blockSize;
    return blockCount.product()*compressedBlockDataSize(format);
}

void endianSwap(Containers::ArrayView<char> data, UnsignedInt typeSize) {
    switch(typeSize) {
        case 1:
            /* Single-byte or block-compressed format, nothing to do */
            break;
        case 2:
            Utility::Endianness::littleEndianInPlace(Containers::arrayCast<UnsignedShort>(data));
            break;
        case 4:
            Utility::Endianness::littleEndianInPlace(Containers::arrayCast<UnsignedInt>(data));
            break;
        case 8:
            Utility::Endianness::littleEndianInPlace(Containers::arrayCast<UnsignedLong>(data));
            break;
        default:
            CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
            break;
    }
}

void swizzlePixels(const PixelFormat format, Containers::Array<char>& data, const char* verbosePrefix) {
    if(format == PixelFormat::RGB8Unorm) {
        if(verbosePrefix) Debug{} << verbosePrefix << "converting from BGR to RGB";
        auto pixels = reinterpret_cast<Math::Vector3<UnsignedByte>*>(data.data());
        std::transform(pixels, pixels + data.size()/sizeof(Math::Vector3<UnsignedByte>), pixels,
            [](Math::Vector3<UnsignedByte> pixel) { return Math::gather<'b', 'g', 'r'>(pixel); });
    } else if(format == PixelFormat::RGBA8Unorm) {
        if(verbosePrefix) Debug{} << verbosePrefix << "converting from BGRA to RGBA";
        auto pixels = reinterpret_cast<Math::Vector4<UnsignedByte>*>(data.data());
        std::transform(pixels, pixels + data.size()/sizeof(Math::Vector4<UnsignedByte>), pixels,
            [](Math::Vector4<UnsignedByte> pixel) { return Math::gather<'b', 'g', 'r', 'a'>(pixel); });

    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

bool validateHeader(const Implementation::KtxHeader& header, std::size_t fileSize, const char* prefix) {
    /* Check magic string to verify this is a KTX2 file */
    const auto identifier = Containers::StringView{header.identifier, sizeof(header.identifier)};
    if(identifier != Implementation::KtxFileIdentifier) {
        /* Print a useful error if this is a KTX file with an unsupported
           version. KTX1 uses the same identifier but different version string. */
        if(std::memcmp(identifier.data(), Implementation::KtxFileIdentifier, Implementation::KtxFileVersionOffset) == 0)
            Error() << prefix << "unsupported KTX version, expected 20 but got" <<
                identifier.suffix(Implementation::KtxFileVersionOffset).prefix(2);
        else
            Error() << prefix << "wrong file signature";
        return false;
    }

    /* typeSize is the size of the format's underlying type, not the texel
       size, e.g. 2 for RG16F. For any sane format it should be a
       power-of-two between 1 and 8. */
    if(header.typeSize < 1 || header.typeSize > 8 ||
        (header.typeSize & (header.typeSize - 1)))
    {
        Error{} << prefix << "unsupported type size" << header.typeSize;
        return false;
    }

    if(header.pixelSize.x() == 0) {
        Error{} << prefix << "invalid image size, width is 0";
        return false;
    }

    if(header.pixelSize.y() == 0 && header.pixelSize.z() > 0) {
        Error{} << prefix << "invalid image size, depth is" << header.pixelSize.z() << "but height is 0";
        return false;
    }

    if(header.faceCount != 1) {
        if(header.faceCount != 6) {
            Error{} << prefix << "invalid cubemap face count, expected 1 or 6 but got" << header.faceCount;
            return false;
        }

        if(header.pixelSize.z() > 0 || header.pixelSize.x() != header.pixelSize.y()) {
            Error{} << prefix << "invalid cubemap dimensions, must be 2D and square, but got" << header.pixelSize;
            return false;
        }
    }

    const UnsignedInt maxLevelCount = Math::log2(header.pixelSize.max()) + 1;
    if(header.levelCount > maxLevelCount) {
        Error{} << prefix << "too many mipmap levels, expected at most" <<
            maxLevelCount << "but got" << header.levelCount;
        return false;
    }

    const std::size_t levelIndexEnd = sizeof(header) + Math::max(header.levelCount, 1u)*sizeof(Implementation::KtxLevel);
    if(fileSize < levelIndexEnd) {
        Error{} << prefix << "level index too short, expected at least" <<
            levelIndexEnd << "bytes but got" << fileSize;
        return false;
    }

    const std::size_t dfdEnd = header.dfdByteOffset + header.dfdByteLength;
    if(fileSize < dfdEnd) {
        Error{} << prefix << "data format descriptor out of bounds, expected at least" <<
            dfdEnd << "bytes but got" << fileSize;
        return false;
    }

    const std::size_t kvdEnd = header.kvdByteOffset + header.kvdByteLength;
    if(fileSize < kvdEnd) {
        Error{} << prefix << "key/value data out of bounds, expected at least" <<
            kvdEnd << "bytes but got" << fileSize;
        return false;
    }

    return true;
}

bool validateLevel(const Implementation::KtxHeader& header, std::size_t fileSize, const Implementation::KtxLevel& level, std::size_t imageLength, const char* prefix) {
    CORRADE_INTERNAL_ASSERT(imageLength > 0);

    /* Both lengths should be equal without supercompression. Be lenient
       here and only emit a warning in case some shitty exporter gets this
       wrong. */
    if(header.supercompressionScheme == 0 && level.byteLength != level.uncompressedByteLength) {
        Warning{} << prefix << "mismatching image data sizes, both compressed "
            "and uncompressed should be equal but got" <<
            level.byteLength << "and" << level.uncompressedByteLength;
    }

    const std::size_t levelEnd = level.byteOffset + level.byteLength;
    if(fileSize < levelEnd) {
        Error{} << prefix << "level data out of bounds, expected at least" <<
            levelEnd << "bytes but got" << fileSize;
        return false;
    }

    const std::size_t totalLength = imageLength*Math::max(header.layerCount, 1u)*header.faceCount;
    if(level.byteLength < totalLength) {
        Error{} << prefix << "level data too short, expected at least" <<
            totalLength << "bytes but got" << level.byteLength;
        return false;
    }

    return true;
}

/** @todo Swizzle channels if necessary. Vk::PixelFormat doesn't contain any
          of the swizzled formats like B8G8R8A8. */
/** @todo Support all Vulkan formats allowed by the KTX spec. Create custom
          PixelFormat with pixelFormatWrap and manually fill PixelStorage/
          CompressedPixelStorage. We can take all the necessary info from
          https://github.com/KhronosGroup/KTX-Specification/blob/master/formats.json
          Do we also need this for the KtxImageConverter? This would allow
          users to pass in images with implementation-specific PixelFormat
          using the Vulkan format enum directly. */
PixelFormat pixelFormat(Vk::PixelFormat format) {
    #define _c(val) case Vk::PixelFormat:: ## val: return PixelFormat:: ## val;
    switch(format) {
        _c(R8Unorm)
        _c(RG8Unorm)
        _c(RGB8Unorm)
        _c(RGBA8Unorm)
        _c(R8Snorm)
        _c(RG8Snorm)
        _c(RGB8Snorm)
        _c(RGBA8Snorm)
        _c(R8Srgb)
        _c(RG8Srgb)
        _c(RGB8Srgb)
        _c(RGBA8Srgb)
        _c(R8UI)
        _c(RG8UI)
        _c(RGB8UI)
        _c(RGBA8UI)
        _c(R8I)
        _c(RG8I)
        _c(RGB8I)
        _c(RGBA8I)
        _c(R16Unorm)
        _c(RG16Unorm)
        _c(RGB16Unorm)
        _c(RGBA16Unorm)
        _c(R16Snorm)
        _c(RG16Snorm)
        _c(RGB16Snorm)
        _c(RGBA16Snorm)
        _c(R16UI)
        _c(RG16UI)
        _c(RGB16UI)
        _c(RGBA16UI)
        _c(R16I)
        _c(RG16I)
        _c(RGB16I)
        _c(RGBA16I)
        _c(R32UI)
        _c(RG32UI)
        _c(RGB32UI)
        _c(RGBA32UI)
        _c(R32I)
        _c(RG32I)
        _c(RGB32I)
        _c(RGBA32I)
        _c(R16F)
        _c(RG16F)
        _c(RGB16F)
        _c(RGBA16F)
        _c(R32F)
        _c(RG32F)
        _c(RGB32F)
        _c(RGBA32F)
        _c(Depth16Unorm)
        _c(Depth24Unorm)
        _c(Depth32F)
        _c(Stencil8UI)
        _c(Depth16UnormStencil8UI)
        _c(Depth24UnormStencil8UI)
        _c(Depth32FStencil8UI)
        default:
            return {};
    }
    #undef _c
}

CompressedPixelFormat compressedPixelFormat(Vk::PixelFormat format) {
    #define _c(val) case Vk::PixelFormat::Compressed ## val: return CompressedPixelFormat:: ## val;
    switch(format) {
        _c(Bc1RGBUnorm)
        _c(Bc1RGBSrgb)
        _c(Bc1RGBAUnorm)
        _c(Bc1RGBASrgb)
        _c(Bc2RGBAUnorm)
        _c(Bc2RGBASrgb)
        _c(Bc3RGBAUnorm)
        _c(Bc3RGBASrgb)
        _c(Bc4RUnorm)
        _c(Bc4RSnorm)
        _c(Bc5RGUnorm)
        _c(Bc5RGSnorm)
        _c(Bc6hRGBUfloat)
        _c(Bc6hRGBSfloat)
        _c(Bc7RGBAUnorm)
        _c(Bc7RGBASrgb)
        _c(EacR11Unorm)
        _c(EacR11Snorm)
        _c(EacRG11Unorm)
        _c(EacRG11Snorm)
        _c(Etc2RGB8Unorm)
        _c(Etc2RGB8Srgb)
        _c(Etc2RGB8A1Unorm)
        _c(Etc2RGB8A1Srgb)
        _c(Etc2RGBA8Unorm)
        _c(Etc2RGBA8Srgb)
        _c(Astc4x4RGBAUnorm)
        _c(Astc4x4RGBASrgb)
        _c(Astc4x4RGBAF)
        _c(Astc5x4RGBAUnorm)
        _c(Astc5x4RGBASrgb)
        _c(Astc5x4RGBAF)
        _c(Astc5x5RGBAUnorm)
        _c(Astc5x5RGBASrgb)
        _c(Astc5x5RGBAF)
        _c(Astc6x5RGBAUnorm)
        _c(Astc6x5RGBASrgb)
        _c(Astc6x5RGBAF)
        _c(Astc6x6RGBAUnorm)
        _c(Astc6x6RGBASrgb)
        _c(Astc6x6RGBAF)
        _c(Astc8x5RGBAUnorm)
        _c(Astc8x5RGBASrgb)
        _c(Astc8x5RGBAF)
        _c(Astc8x6RGBAUnorm)
        _c(Astc8x6RGBASrgb)
        _c(Astc8x6RGBAF)
        _c(Astc8x8RGBAUnorm)
        _c(Astc8x8RGBASrgb)
        _c(Astc8x8RGBAF)
        _c(Astc10x5RGBAUnorm)
        _c(Astc10x5RGBASrgb)
        _c(Astc10x5RGBAF)
        _c(Astc10x6RGBAUnorm)
        _c(Astc10x6RGBASrgb)
        _c(Astc10x6RGBAF)
        _c(Astc10x8RGBAUnorm)
        _c(Astc10x8RGBASrgb)
        _c(Astc10x8RGBAF)
        _c(Astc10x10RGBAUnorm)
        _c(Astc10x10RGBASrgb)
        _c(Astc10x10RGBAF)
        _c(Astc12x10RGBAUnorm)
        _c(Astc12x10RGBASrgb)
        _c(Astc12x10RGBAF)
        _c(Astc12x12RGBAUnorm)
        _c(Astc12x12RGBASrgb)
        _c(Astc12x12RGBAF)
        _c(PvrtcRGBA2bppUnorm)
        _c(PvrtcRGBA2bppSrgb)
        _c(PvrtcRGBA4bppUnorm)
        _c(PvrtcRGBA4bppSrgb)
        /* CompressedPixelFormat has no Pvrtc2 */
        /*
        _c(Pvrtc2RGBA2bppUnorm)
        _c(Pvrtc2RGBA2bppSrgb)
        _c(Pvrtc2RGBA4bppUnorm)
        _c(Pvrtc2RGBA4bppSrgb)
        */
        default:
            return {};
    }
    #undef _c
}

}

struct KtxImporter::File {
    struct LevelData {
        Vector3i size;
        Containers::ArrayView<char> data;
    };

    Containers::Array<char> in;

    UnsignedByte dimensions;
    bool needsSwizzle;

    union {
        PixelFormat uncompressed;
        CompressedPixelFormat compressed;
    } pixelFormat;
    bool compressed;
    UnsignedInt typeSize;

    /* Each array layer is an image with faces and mipmaps as levels */
    Containers::Array<Containers::Array<LevelData>> imageData;
};

KtxImporter::KtxImporter() = default;

KtxImporter::KtxImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

KtxImporter::~KtxImporter() = default;

ImporterFeatures KtxImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool KtxImporter::doIsOpened() const { return !!_f; }

void KtxImporter::doClose() { _f = nullptr; }

void KtxImporter::doOpenData(const Containers::ArrayView<const char> data) {
    /* Check if the file is long enough for the header */
    if(data.size() < sizeof(Implementation::KtxHeader)) {
        Error{} << "Trade::KtxImporter::openData(): file header too short, expected at least" << sizeof(Implementation::KtxHeader) << "bytes but got" << data.size();
        return;
    }

    Implementation::KtxHeader header = *reinterpret_cast<const Implementation::KtxHeader*>(data.data());
    std::size_t offset = sizeof(Implementation::KtxHeader);

    /* KTX2 uses little-endian everywhere */
    Utility::Endianness::littleEndianInPlace(
        header.vkFormat, header.typeSize,
        header.pixelSize[0], header.pixelSize[1], header.pixelSize[2],
        header.layerCount, header.faceCount, header.levelCount,
        header.supercompressionScheme,
        header.dfdByteOffset, header.dfdByteLength,
        header.kvdByteOffset, header.kvdByteLength,
        header.sgdByteOffset, header.sgdByteLength);

    /* Perform some sanity checks on header data and check if
       there is enough space for the level index, data format
       descriptor and key/value data */
    if(!validateHeader(header, data.size(), "Trade::KtxImporter::openData():"))
        return;

    Containers::Pointer<File> f{new File};
    f->in = Containers::Array<char>{NoInit, data.size()};
    Utility::copy(data, f->in);

    /** @todo Support Basis compression */
    const PixelFormat format = pixelFormat(Vk::PixelFormat(header.vkFormat));
    if(format == PixelFormat{}) {
        const CompressedPixelFormat compressedFormat = compressedPixelFormat(Vk::PixelFormat(header.vkFormat));
        if(compressedFormat == CompressedPixelFormat{}) {
            Error{} << "Trade::KtxImporter::openData(): unsupported format" << header.vkFormat;
            return;
        }
        f->pixelFormat.compressed = compressedFormat;
        f->compressed = true;
    } else {
        f->pixelFormat.uncompressed = format;
        f->compressed = false;
    }

    if(f->compressed && header.typeSize != 1) {
        Error{} << "Trade::KtxImporter::openData(): invalid type size for compressed format, expected 1 but got" << header.typeSize;
        return;
    }
    f->typeSize = header.typeSize;

    /** @todo Support supercompression */
    if(header.supercompressionScheme != 0) {
        Error{} << "Trade::KtxImporter::openData(): supercompression is currently not supported";
        return;
    }

    f->dimensions = Math::min(header.pixelSize, Vector3ui{1}).sum();
    CORRADE_INTERNAL_ASSERT(f->dimensions >= 1 && f->dimensions <= 3);

    /** @todo Assert that 3D images can't have depth format */

    /* Make size in each dimension at least 1 so we don't choke on size
       calculations using product(). */
    const Vector3i size = Math::max(Vector3i{header.pixelSize}, Vector3i{1});

    /* Number of array layers, imported as separate images */
    const UnsignedInt numLayers = Math::max(header.layerCount, 1u);

    /* Cubemap faces, either 1 or 6. KTX allows incomplete cubemaps but those
       have to be array layers with extra metadata inside key/value data. */
    const UnsignedInt numFaces = header.faceCount;

    /* levelCount of 0 indicates to the user to generate mipmaps, not allowed
       for block-compressed formats. We don't really care either way
       and treat 0 and 1 the same. */
    const UnsignedInt numMipmaps = Math::max(header.levelCount, 1u);

    /* KTX stores byte ranges for each mipmap in the level index, from largest
       to smallest. The actual pixel data is usually arranged in reverse order
       for streaming but we only need the ranges. */
    const std::size_t levelIndexSize = numMipmaps*sizeof(Implementation::KtxLevel);
    const auto levelIndex = Containers::arrayCast<Implementation::KtxLevel>(
        f->in.suffix(offset).prefix(levelIndexSize));

    /* We later loop over all levels multiple times, so prepare and validate
       a few things up-front */
    Containers::Array<std::size_t> levelLengths{numMipmaps};
    Vector3i mipSize{size};
    for(UnsignedInt i = 0; i != levelIndex.size(); ++i) {
        auto& level = levelIndex[i];
        Utility::Endianness::littleEndianInPlace(level.byteOffset,
            level.byteLength, level.uncompressedByteLength);

        levelLengths[i] = f->compressed
            ? imageLength(mipSize, f->pixelFormat.compressed)
            : imageLength(mipSize, f->pixelFormat.uncompressed);

        if(!validateLevel(header, data.size(), level, levelLengths[i], "Trade::KtxImporter::openData():"))
            return;

        /* Shrink to next power of 2 */
        mipSize = Math::max(mipSize >> 1, Vector3i{1});
    }

    /** @todo Read data format descriptor */
    if(header.dfdByteLength > 0) {
        const auto DataFormatDescriptorData = f->in.suffix(header.dfdByteOffset).prefix(header.dfdByteLength);
    }

    /* Read key/value data, optional */
    std::map<Containers::StringView, Containers::ArrayView<const char>> keyValueMap;
    if(header.kvdByteLength > 0) {
        auto keyValueData = f->in.suffix(header.kvdByteOffset).prefix(header.kvdByteLength);
        /* Loop through entries, each one consisting of:
        
           UnsignedInt length
           Byte data[length]
           Byte padding[...]

           data begins with a zero-terminated key, the rest of the bytes is the
           value content. Value alignment must be implicitly done through key
           length, hence the funny KTX keys with multiple underscores. Any
           multi-byte numbers in values must be endian-swapped later. */
        UnsignedInt current = 0;
        while(current + sizeof(UnsignedInt) < keyValueData.size()) {
            /* Length without padding */
            const UnsignedInt length = *reinterpret_cast<const UnsignedInt*>(keyValueData.suffix(current).data());
            current += sizeof(length);

            if(current + length < keyValueData.size()) {
                const auto entry = keyValueData.suffix(current).prefix(length);

                /* Split at zero separating the key string and value */
                const std::size_t keyLength = std::find(entry.begin(), entry.end(), '\0') - entry.begin();
                if(keyLength == 0 || keyLength >= entry.size())
                    Warning{} << "Trade::KtxImporter::openData(): invalid key/value entry, skipping";
                else {
                    const Containers::StringView key{entry.data()};
                    CORRADE_INTERNAL_ASSERT(key.size() == keyLength);

                    if(keyValueMap.count(key) > 0)
                        keyValueMap[key] = entry.suffix(keyLength + 1);
                    else
                        Warning{} << "Trade::KtxImporter::openData(): key" <<
                            key << "already set, skipping";
                }
            }
            /* Length value is dword-aligned, guaranteed for the first length
               by the file layout */
            current += (length + 3)/4*4;
        }
    }

    /** @todo Determine if format needs swizzling */
    f->needsSwizzle = false;

    f->imageData = Containers::Array<Containers::Array<File::LevelData>>{numLayers};

    /** @todo Export cubemaps as 3D images instead of levels */

    for(UnsignedInt layer = 0; layer != numLayers; ++layer) {
        f->imageData[layer] = Containers::Array<File::LevelData>{numFaces*numMipmaps};
        /* This matches the level order of DdsImporter: faces, mipmaps */
        std::size_t currentLevel = 0;
        /* +X, -X, +Y, -Y, +Z, -Z */
        for(UnsignedInt face = 0; face != numFaces; ++face) {
            mipSize = size;

            /* Load all mipmaps for current face */
            for(UnsignedInt i = 0; i != levelIndex.size(); ++i) {
                /* Each mipmap byte range contains tightly packed images,
                   ordered as follows:
                   layers, faces, (slices, rows, columns) */
                const std::size_t length = levelLengths[i];
                const std::size_t imageOffset = (layer*numFaces + face)*length;

                /* validateLevel already checked if the level data is large
                   enough for all images */
                f->imageData[layer][currentLevel++] = {mipSize, f->in.suffix(levelIndex[i].byteOffset).suffix(imageOffset).prefix(length)};

                /* Shrink to next power of 2 */
                mipSize = Math::max(mipSize >> 1, Vector3i{1});
            }
        }
    }

    /** @todo Read KTXorientation and flip image? What is the default? */
    /** @todo Read KTXswizzle and apply it. Can't do that for block-compressed formats, just ignore in that case? */
    /** @todo Read KTXanimData and expose frame time between images through
              doImporterState (same as StbImageImporter). What if we need
              doImporterState for something else, like other API format
              enum values (KTXdxgiFormat__ etc.)? */

    _f = std::move(f);
}

UnsignedInt KtxImporter::doImage1DCount() const { return (_f->dimensions == 1) ? _f->imageData.size() : 0; }

UnsignedInt KtxImporter::doImage1DLevelCount(UnsignedInt id) { return _f->imageData[id].size(); }

Containers::Optional<ImageData1D> KtxImporter::doImage1D(UnsignedInt id, UnsignedInt level) {
    const File::LevelData& levelData = _f->imageData[id][level];

    /* Copy image data */
    Containers::Array<char> data{NoInit, levelData.data.size()};
    Utility::copy(levelData.data, data);

    /* Endian-swap if necessary */
    endianSwap(data, _f->typeSize);

    /* Compressed image */
    if(_f->compressed) {
        /* Using default CompressedPixelStorage, need explicit one once we
           support all Vulkan formats */
        return ImageData1D(_f->pixelFormat.compressed, levelData.size.x(), std::move(data));
    }

    /* Uncompressed */
    if(_f->needsSwizzle) swizzlePixels(_f->pixelFormat.uncompressed, data,
        flags() & ImporterFlag::Verbose ? "Trade::KtxImporter::image2D():" : nullptr);

    /* Rows are tightly packed */
    PixelStorage storage;
    storage.setAlignment(1);

    return ImageData1D{storage, _f->pixelFormat.uncompressed, levelData.size.x(), std::move(data)};
}

UnsignedInt KtxImporter::doImage2DCount() const { return (_f->dimensions == 2) ? _f->imageData.size() : 0; }

UnsignedInt KtxImporter::doImage2DLevelCount(UnsignedInt id) { return _f->imageData[id].size(); }

Containers::Optional<ImageData2D> KtxImporter::doImage2D(UnsignedInt id, UnsignedInt level) {
    const File::LevelData& levelData = _f->imageData[id][level];

    /* Copy image data */
    Containers::Array<char> data{NoInit, levelData.data.size()};
    Utility::copy(levelData.data, data);

    /* Endian-swap if necessary */
    endianSwap(data, _f->typeSize);

    /* Compressed image */
    if(_f->compressed) {
        /* Using default CompressedPixelStorage, need explicit one once we
           support all Vulkan formats */
        return ImageData2D(_f->pixelFormat.compressed, levelData.size.xy(), std::move(data));
    }

    /* Uncompressed */
    if(_f->needsSwizzle) swizzlePixels(_f->pixelFormat.uncompressed, data,
        flags() & ImporterFlag::Verbose ? "Trade::KtxImporter::image2D():" : nullptr);

    /* Rows are tightly packed */
    PixelStorage storage;
    storage.setAlignment(1);

    return ImageData2D{storage, _f->pixelFormat.uncompressed, levelData.size.xy(), std::move(data)};
}

UnsignedInt KtxImporter::doImage3DCount() const { return (_f->dimensions == 3) ? _f->imageData.size() : 0; }

UnsignedInt KtxImporter::doImage3DLevelCount(UnsignedInt id) { return _f->imageData[id].size(); }

Containers::Optional<ImageData3D> KtxImporter::doImage3D(UnsignedInt id, const UnsignedInt level) {
    const File::LevelData& levelData = _f->imageData[id][level];

    /* Copy image data */
    Containers::Array<char> data{NoInit, levelData.data.size()};
    Utility::copy(levelData.data, data);

    /* Endian-swap if necessary */
    endianSwap(data, _f->typeSize);

    /* Compressed image */
    if(_f->compressed) {
        /* Using default CompressedPixelStorage, need explicit one once we
           support all Vulkan formats */
        return ImageData3D(_f->pixelFormat.compressed, levelData.size, std::move(data));
    }

    /* Uncompressed */
    if(_f->needsSwizzle) swizzlePixels(_f->pixelFormat.uncompressed, data,
        flags() & ImporterFlag::Verbose ? "Trade::KtxImporter::image3D():" : nullptr);

    /* Rows are tightly packed */
    PixelStorage storage;
    storage.setAlignment(1);

    return ImageData3D{storage, _f->pixelFormat.uncompressed, levelData.size, std::move(data)};
}

}}

CORRADE_PLUGIN_REGISTER(KtxImporter, Magnum::Trade::KtxImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.3")
