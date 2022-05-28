/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2015 Jonathan Hale <squareys@googlemail.com>

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

#include "DdsImporter.h"

#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Math/Swizzle.h>
#include <Magnum/Trade/ImageData.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

namespace {

/* Flags to indicate which members of a DdsHeader contain valid data */
enum class DdsDescriptionFlag: UnsignedInt {
    Caps = 0x00000001,
    Height = 0x00000002,
    Width = 0x00000004,
    Pitch = 0x00000008,
    PixelFormat = 0x00001000,
    MipMapCount = 0x00020000,
    LinearSize = 0x00080000,
    Depth = 0x00800000
};

/* Direct Draw Surface pixel format */
enum class DdsPixelFormatFlag: UnsignedInt {
    AlphaPixels = 0x00000001,
    FourCC = 0x00000004,
    RGB = 0x00000040,
    RGBA = 0x00000041
};

/* Specifies the complexity of the surfaces stored. Just for completeness,
   not used in the code. */
enum class DdsCap1: UnsignedInt {
    /* Set for files that contain more than one surface (a mipmap, a cubic
       environment map, or mipmapped volume texture) */
    Complex = 0x00000008,
    /* Texture (required) */
    Texture = 0x00001000,
    /* Is set for mipmaps */
    MipMap = 0x00400000
};

/* Additional detail about the surfaces stored */
enum class DdsCap2: UnsignedInt {
    Cubemap = 0x00000200,
    CubemapPositiveX = 0x00000400,
    CubemapNegativeX = 0x00000800,
    CubemapPositiveY = 0x00001000,
    CubemapNegativeY = 0x00002000,
    CubemapPositiveZ = 0x00004000,
    CubemapNegativeZ = 0x00008000,
    CubemapAllFaces = 0x0000fc00,
    Volume = 0x00200000
};

typedef Containers::EnumSet<DdsDescriptionFlag> DdsDescriptionFlags;
typedef Containers::EnumSet<DdsPixelFormatFlag> DdsPixelFormatFlags;
typedef Containers::EnumSet<DdsCap1> DdsCaps1;
typedef Containers::EnumSet<DdsCap2> DdsCaps2;
#ifdef CORRADE_TARGET_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
CORRADE_ENUMSET_OPERATORS(DdsDescriptionFlags)
CORRADE_ENUMSET_OPERATORS(DdsPixelFormatFlags)
CORRADE_ENUMSET_OPERATORS(DdsCaps1)
CORRADE_ENUMSET_OPERATORS(DdsCaps2)
#ifdef CORRADE_TARGET_CLANG
#pragma clang diagnostic pop
#endif

/* DDS file header struct. Microsoft says that the header has 124 bytes and one
   should apparently check the signature separately? Why so complicated?
   https://docs.microsoft.com/en-us/windows/win32/direct3ddds/dds-header */
struct DdsHeader {
    char signature[4]; /* DDS<space> */
    UnsignedInt size; /* Should be 124. Should I check this? */
    DdsDescriptionFlags flags;
    UnsignedInt height;
    UnsignedInt width;
    UnsignedInt pitchOrLinearSize;
    UnsignedInt depth;
    UnsignedInt mipMapCount;
    UnsignedInt reserved1[11];
    struct {
        UnsignedInt size;  /* Should be 32. Should I check this? */
        DdsPixelFormatFlags flags;
        union {
            UnsignedInt fourCC;
            char fourCCChars[4];
        };
        UnsignedInt rgbBitCount;
        UnsignedInt rBitMask;
        UnsignedInt gBitMask;
        UnsignedInt bBitMask;
        UnsignedInt aBitMask;
    } ddspf; /* Pixel format */
    DdsCaps1 caps;
    DdsCaps2 caps2;
    UnsignedInt caps3;
    UnsignedInt caps4;
    UnsignedInt reserved2;
};

static_assert(sizeof(DdsHeader) == 128, "Improper size of DdsHeader struct");

/* DDS file header extension for DXGI pixel formats */
enum class DdsDimension: UnsignedInt {
    Unknown = 0,
    /* 1 is unused (= D3D10 Resource Dimension Buffer) */
    Texture1D = 2,
    Texture2D = 3,
    Texture3D = 4,
};

enum class DdsMiscFlag: UnsignedInt {
    TextureCube = 4,
};

enum class DdsAlphaMode: UnsignedInt {
    Unknown = 0,
    Straight = 1,
    Premultiplied = 2,
    Opaque = 3,
    Custom = 4,
};

struct DdsHeaderDxt10 {
    UnsignedInt dxgiFormat;
    DdsDimension resourceDimension;
    DdsMiscFlag miscFlag;
    UnsignedInt arraySize;
    DdsAlphaMode miscFlags2;
};

constexpr struct {
    /* It still could be packed better (e.g. an union where it's either a name
       or a format and a distinction between an uncompressed format,
       uncompressed format that needs swizzle or a compressed format, but let's
       say this is good enough for now. We're explicitly not storing names of
       formats we won't ever print. */
    const char* name;
    /* Currently there's only about 110 values for compressed formats, so 8
       bits for each should be enough. Verified in
       DdsImporterTest::enumValueMatching(). */
    UnsignedByte format;
    UnsignedByte compressedFormat;
    bool needsSwizzle;
} DxgiFormatMapping[] {
#define _x(name) {#name, 0, 0, false},
#define _u(name, format) {nullptr, UnsignedInt(PixelFormat::format), 0, false},
#define _s(name, format, swizzle) {nullptr, UnsignedInt(PixelFormat::format), 0, swizzle},
#define _c(name, format) {nullptr, 0, UnsignedInt(CompressedPixelFormat::format), false},
#include "DxgiFormat.h"
#undef _c
#undef _s
#undef _u
#undef _x
};

}

struct DdsImporter::File {
    struct ImageDataOffset {
        Vector3i dimensions;
        Containers::ArrayView<char> data;
    };

    Containers::Array<char> in;

    bool compressed;
    bool volume;
    bool needsSwizzle;

    union {
        PixelFormat uncompressed;
        CompressedPixelFormat compressed;
    } pixelFormat;

    Containers::Array<ImageDataOffset> imageData;
};

DdsImporter::DdsImporter() = default;

DdsImporter::DdsImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

DdsImporter::~DdsImporter() = default;

ImporterFeatures DdsImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool DdsImporter::doIsOpened() const { return !!_f; }

void DdsImporter::doClose() { _f = nullptr; }

void DdsImporter::doOpenData(Containers::Array<char>&& data, const DataFlags dataFlags) {
    Containers::Pointer<File> f{new File};

    /* Take over the existing array or copy the data if we can't */
    if(dataFlags & (DataFlag::Owned|DataFlag::ExternallyOwned)) {
        f->in = std::move(data);
    } else {
        f->in = Containers::Array<char>{NoInit, data.size()};
        Utility::copy(data, f->in);
    }

    /* Read in DDS header */
    if(f->in.size() < sizeof(DdsHeader)) {
        Error{} << "Trade::DdsImporter::openData(): file too short, expected at least" << sizeof(DdsHeader) << "bytes but got" << f->in.size();
        return;
    }
    const DdsHeader& header = *reinterpret_cast<const DdsHeader*>(f->in.data());
    std::size_t offset = sizeof(DdsHeader);

    /* Verify file signature */
    if(Containers::StringView{header.signature, 4} != "DDS "_s) {
        Error() << "Trade::DdsImporter::openData(): invalid file signature" << Containers::StringView{header.signature, 4};
        return;
    }

    /* Check if image is a 2D or 3D texture */
    f->volume = ((header.caps2 & DdsCap2::Volume) && (header.depth > 0));

    /* Compressed */
    if(header.ddspf.flags & DdsPixelFormatFlag::FourCC) {
        switch(header.ddspf.fourCC) {
            case Utility::Endianness::fourCC('D', 'X', 'T', '1'):
                f->compressed = true;
                f->needsSwizzle = false;
                f->pixelFormat.compressed = CompressedPixelFormat::Bc1RGBAUnorm;
                break;
            case Utility::Endianness::fourCC('D', 'X', 'T', '3'):
                f->compressed = true;
                f->needsSwizzle = false;
                f->pixelFormat.compressed = CompressedPixelFormat::Bc2RGBAUnorm;
                break;
            case Utility::Endianness::fourCC('D', 'X', 'T', '5'):
                f->compressed = true;
                f->needsSwizzle = false;
                f->pixelFormat.compressed = CompressedPixelFormat::Bc3RGBAUnorm;
                break;
            case Utility::Endianness::fourCC('A', 'T', 'I', '1'):
                f->compressed = true;
                f->needsSwizzle = false;
                f->pixelFormat.compressed = CompressedPixelFormat::Bc4RUnorm;
                break;
            case Utility::Endianness::fourCC('B', 'C', '4', 'S'):
                f->compressed = true;
                f->needsSwizzle = false;
                f->pixelFormat.compressed = CompressedPixelFormat::Bc4RSnorm;
                break;
            case Utility::Endianness::fourCC('A', 'T', 'I', '2'):
                f->compressed = true;
                f->needsSwizzle = false;
                f->pixelFormat.compressed = CompressedPixelFormat::Bc5RGUnorm;
                break;
            case Utility::Endianness::fourCC('B', 'C', '5', 'S'):
                f->compressed = true;
                f->needsSwizzle = false;
                f->pixelFormat.compressed = CompressedPixelFormat::Bc5RGSnorm;
                break;
            case Utility::Endianness::fourCC('D', 'X', '1', '0'): {
                if(f->in.size() < sizeof(DdsHeader) + sizeof(DdsHeaderDxt10)) {
                    Error{} << "Trade::DdsImporter::openData(): DXT10 file too short, expected at least" << sizeof(DdsHeader) + sizeof(DdsHeaderDxt10) << "bytes but got" << f->in.size();
                    return;
                }
                const DdsHeaderDxt10& headerDxt10 = *reinterpret_cast<const DdsHeaderDxt10*>(f->in.data() + sizeof(DdsHeader));
                offset += sizeof(DdsHeaderDxt10);

                if(headerDxt10.dxgiFormat >= Containers::arraySize(DxgiFormatMapping)) {
                    Error{} << "Trade::DdsImporter::openData(): unknown DXGI format ID" << headerDxt10.dxgiFormat;
                    return;
                }

                const auto& mapped = DxgiFormatMapping[headerDxt10.dxgiFormat];
                if(mapped.format) {
                    f->compressed = false;
                    f->needsSwizzle = mapped.needsSwizzle;
                    f->pixelFormat.uncompressed = PixelFormat(mapped.format);
                } else if(mapped.compressedFormat) {
                    f->compressed = true;
                    f->needsSwizzle = false;
                    f->pixelFormat.compressed = CompressedPixelFormat(mapped.compressedFormat);
                } else {
                    Error{} << "Trade::DdsImporter::openData(): unsupported format DXGI_FORMAT_" << Debug::nospace << mapped.name;
                    return;
                }
            } break;
            case Utility::Endianness::fourCC('D', 'X', 'T', '2'):
            case Utility::Endianness::fourCC('D', 'X', 'T', '4'):
            default:
                Error() << "Trade::DdsImporter::openData(): unknown compression" << Containers::StringView{header.ddspf.fourCCChars, 4};
                return;
        }

    /* RGBA */
    } else if(header.ddspf.rgbBitCount == 32 &&
              header.ddspf.rBitMask == 0x000000ff &&
              header.ddspf.gBitMask == 0x0000ff00 &&
              header.ddspf.bBitMask == 0x00ff0000 &&
              header.ddspf.aBitMask == 0xff000000) {
        f->pixelFormat.uncompressed = PixelFormat::RGBA8Unorm;
        f->compressed = false;
        f->needsSwizzle = false;

    /* BGRA */
    } else if(header.ddspf.rgbBitCount == 32 &&
              header.ddspf.rBitMask == 0x00ff0000 &&
              header.ddspf.gBitMask == 0x0000ff00 &&
              header.ddspf.bBitMask == 0x000000ff &&
              header.ddspf.aBitMask == 0xff000000) {
        f->pixelFormat.uncompressed = PixelFormat::RGBA8Unorm;
        f->compressed = false;
        f->needsSwizzle = true;

    /* RGB */
    } else if(header.ddspf.rgbBitCount == 24 &&
              header.ddspf.rBitMask == 0x000000ff &&
              header.ddspf.gBitMask == 0x0000ff00 &&
              header.ddspf.bBitMask == 0x00ff0000) {
        f->pixelFormat.uncompressed = PixelFormat::RGB8Unorm;
        f->compressed = false;
        f->needsSwizzle = false;

    /* BGR */
    } else if(header.ddspf.rgbBitCount == 24 &&
              header.ddspf.rBitMask == 0x00ff0000 &&
              header.ddspf.gBitMask == 0x0000ff00 &&
              header.ddspf.bBitMask == 0x000000ff) {
        f->pixelFormat.uncompressed = PixelFormat::RGB8Unorm;
        f->compressed = false;
        f->needsSwizzle = true;

    /* Grayscale */
    } else if(header.ddspf.rgbBitCount == 8) {
        f->pixelFormat.uncompressed = PixelFormat::R8Unorm;
        f->compressed = false;
        f->needsSwizzle = false;

    } else {
        Error() << "Trade::DdsImporter::openData(): unknown" << header.ddspf.rgbBitCount << "bits per pixel format with a RGBA mask" << Debug::packed << Math::Vector4<void*>{reinterpret_cast<void*>(header.ddspf.rBitMask), reinterpret_cast<void*>(header.ddspf.gBitMask), reinterpret_cast<void*>(header.ddspf.bBitMask), reinterpret_cast<void*>(header.ddspf.aBitMask)};
        return;
    }

    const Vector3i size{Int(header.width), Int(header.height), Int(Math::max(header.depth, 1u))};

    /* Check how many mipmaps to load */
    const UnsignedInt numMipmaps = header.flags & DdsDescriptionFlag::MipMapCount ? header.mipMapCount : 1;

    /* Load all surfaces for the image (6 surfaces for cubemaps) */
    const UnsignedInt numImages = header.caps2 & DdsCap2::Cubemap ? 6 : 1;
    for(UnsignedInt n = 0; n < numImages; ++n) {
        Vector3i mipSize{size};

        /* Load all mipmaps for current surface */
        for(UnsignedInt i = 0; i < numMipmaps; ++i) {
            std::size_t size;
            if(f->compressed) {
                const Vector3i blockSize = compressedBlockSize(f->pixelFormat.compressed);
                const std::size_t blockDataSize = compressedBlockDataSize(f->pixelFormat.compressed);
                size = blockDataSize*((mipSize + Vector3i{blockSize} - Vector3i{1})/Vector3i{blockSize}).product();
            } else {
                size = mipSize.product()*pixelSize(f->pixelFormat.uncompressed);
            }

            const std::size_t end = offset + size;
            if(f->in.size() < end) {
                Error() << "Trade::DdsImporter::openData(): file too short, expected" << end << "bytes for image" << n << "level" << i << "but got" << f->in.size();
                return;
            }

            arrayAppend(f->imageData, InPlaceInit, mipSize, f->in.slice(offset, end));

            offset = end;

            /* Shrink to next power of 2 */
            mipSize = Math::max(mipSize >> 1, Vector3i{1});
        }
    }

    /* Everything okay, save the file for later use */
    _f = std::move(f);
}

namespace {

void swizzlePixels(const PixelFormat format, Containers::Array<char>& data, const char* verbosePrefix) {
    if(format == PixelFormat::RGB8Unorm) {
        if(verbosePrefix) Debug{} << verbosePrefix << "converting from BGR to RGB";
        for(Vector3ub& pixel: Containers::arrayCast<Vector3ub>(data))
            pixel = Math::gather<'b', 'g', 'r'>(pixel);
    } else if(format == PixelFormat::RGBA8Unorm) {
        if(verbosePrefix) Debug{} << verbosePrefix << "converting from BGRA to RGBA";
        for(Vector4ub& pixel: Containers::arrayCast<Vector4ub>(data))
            pixel = Math::gather<'b', 'g', 'r', 'a'>(pixel);
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

}

template<UnsignedInt dimensions> ImageData<dimensions> DdsImporter::doImage(const char* prefix, UnsignedInt, UnsignedInt level) {
    const File::ImageDataOffset& dataOffset = _f->imageData[level];

    /* Copy image data */
    Containers::Array<char> data{NoInit, dataOffset.data.size()};
    Utility::copy(dataOffset.data, data);

    /* Compressed image */
    if(_f->compressed)
        return ImageData<dimensions>(_f->pixelFormat.compressed, Math::Vector<dimensions, Int>::pad(dataOffset.dimensions), std::move(data));

    /* Uncompressed */
    if(_f->needsSwizzle) swizzlePixels(_f->pixelFormat.uncompressed, data,
        flags() & ImporterFlag::Verbose ? prefix : nullptr);

    /* Adjust pixel storage if row size is not four byte aligned */
    PixelStorage storage;
    if((dataOffset.dimensions.x()*pixelSize(_f->pixelFormat.uncompressed))%4 != 0)
        storage.setAlignment(1);

    return ImageData<dimensions>{storage, _f->pixelFormat.uncompressed, Math::Vector<dimensions, Int>::pad(dataOffset.dimensions), std::move(data)};
}

UnsignedInt DdsImporter::doImage2DCount() const {  return _f->volume ? 0 : 1; }

UnsignedInt DdsImporter::doImage2DLevelCount(UnsignedInt) {  return _f->imageData.size(); }

Containers::Optional<ImageData2D> DdsImporter::doImage2D(const UnsignedInt id, const UnsignedInt level) {
    return doImage<2>("Trade::DdsImporter::image2D():", id, level);
}

UnsignedInt DdsImporter::doImage3DCount() const { return _f->volume ? 1 : 0; }

UnsignedInt DdsImporter::doImage3DLevelCount(UnsignedInt) {  return _f->imageData.size(); }

Containers::Optional<ImageData3D> DdsImporter::doImage3D(const UnsignedInt id, const UnsignedInt level) {
    return doImage<3>("Trade::DdsImporter::image3D():", id, level);
}

}}

CORRADE_PLUGIN_REGISTER(DdsImporter, Magnum::Trade::DdsImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.5")
