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

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/Containers/StringIterable.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Math/Swizzle.h>
#include <Magnum/Trade/ImageData.h>

#ifdef MAGNUM_BUILD_DEPRECATED
#include <Magnum/Trade/TextureData.h>
#endif

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

namespace {

/* Flags to indicate which members of a DdsHeader contain valid data. Spec says
   that "[...] when you read a .dds file, you should not rely on the DDSD_CAPS,
   DDSD_PIXELFORMAT, and DDSD_MIPMAPCOUNT flags being set because some writers
   of such a file might not set these flags." */
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

/* Specifies the complexity of the surfaces stored. "Required" to be set when
   interfacing with D3D itself, but the same information is available elsewhere
   so it's not relied on. */
enum class DdsCap1: UnsignedInt {
    Complex = 0x00000008,
    Texture = 0x00001000,
    MipMap = 0x00400000
};

/* Additional detail about the surfaces stored */
enum class DdsCap2: UnsignedInt {
    CubeMap = 0x00000200,
    CubeMapPositiveX = 0x00000400,
    CubeMapNegativeX = 0x00000800,
    CubeMapPositiveY = 0x00001000,
    CubeMapNegativeY = 0x00002000,
    CubeMapPositiveZ = 0x00004000,
    CubeMapNegativeZ = 0x00008000,
    CubeMapAllFaces = 0x0000fc00,
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
    UnsignedInt size; /* Should be 124, ignored by us */
    DdsDescriptionFlags flags; /* Unreliable, ignored by us */
    UnsignedInt height;
    UnsignedInt width;
    /* Spec says "The D3DX library (for example, D3DX11.lib) and other similar
       libraries unreliably or inconsistently provide the pitch value in the
       dwPitchOrLinearSize member of the DDS_HEADER structure.", so that means
       we should probably always assume tightly-packed and ignore this field.
       https://docs.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide */
    UnsignedInt pitchOrLinearSize;
    UnsignedInt depth; /* Can be 0 in some files, which means 1 */
    UnsignedInt mipMapCount; /* Can be 0 in some files, which means 1 */
    UnsignedInt reserved1[11]; /* GIMP, NVTT, NVTT Exporter write a name here */
    struct {
        UnsignedInt size; /* Should be 32, ignored by us */
        DdsPixelFormatFlags flags;
        union {
            UnsignedInt fourCC;
            char fourCCChars[4]; /* For printing */
        };
        /* Used by us only if the DXT10 header isn't also present (in which
           case these are often all zeros), and used also only if the FourCC
           flag isn't set. Theoretically for uncompressed fourCCs the fields
           could be used for something. */
        UnsignedInt rgbBitCount;
        UnsignedInt rBitMask;
        UnsignedInt gBitMask;
        UnsignedInt bBitMask;
        UnsignedInt aBitMask;
    } ddspf;
    DdsCaps1 caps; /* Redundant or useless information, ignored by us */
    DdsCaps2 caps2; /* Used by us only if the DXT10 header isn't also present */
    UnsignedInt caps3; /* Unused */
    UnsignedInt caps4; /* Unused */
    UnsignedInt reserved2; /* Unused */
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

/* If this is set, array size describes cube count, not 2D slice count */
enum class DdsMiscFlag: UnsignedInt {
    TextureCube = 4,
};

/* Not used by us right now, but might be potentially useful */
enum class DdsAlphaMode: UnsignedInt {
    Unknown = 0,
    Straight = 1,
    Premultiplied = 2,
    Opaque = 3,
    Custom = 4,
};

typedef Containers::EnumSet<DdsMiscFlag> DdsMiscFlags;
#ifdef CORRADE_TARGET_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
CORRADE_ENUMSET_OPERATORS(DdsMiscFlags)
#ifdef CORRADE_TARGET_CLANG
#pragma clang diagnostic pop
#endif

struct DdsHeaderDxt10 {
    UnsignedInt dxgiFormat;
    DdsDimension resourceDimension;
    DdsMiscFlags miscFlag; /* TextureCube or nothing */
    UnsignedInt arraySize; /* Should be at least 1, and exactly 1 for 3D */
    DdsAlphaMode miscFlags2; /* Not used right now */
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
#define _i() {},
#define _u(name, format) {nullptr, UnsignedInt(PixelFormat::format), 0, false},
#define _s(name, format, swizzle) {nullptr, UnsignedInt(PixelFormat::format), 0, swizzle},
#define _c(name, format) {nullptr, 0, UnsignedInt(CompressedPixelFormat::format), false},
#include "DxgiFormat.h"
#undef _c
#undef _s
#undef _u
#undef _i
#undef _x
};

}

struct DdsImporter::File {
    Containers::Array<char> in;

    /* Size of one top-level slice. As it's used as an input for level size
       calculation, it doesn't take sliceCount into account. */
    Vector3i topLevelSliceSize{NoInit};
    UnsignedInt levelCount;

    /* ImageFlags3D is a superset of ImageFlags1D and ImageFlags2D, so using it
       for any dimension count */
    ImageFlags3D imageFlags;
    UnsignedInt dimensions; /* 1, 2, 3 */
    UnsignedInt sliceCount; /* 1 for non-array non-cube 1D/2D/3D images */
    std::size_t dataOffset; /* 128 or 148 for a DXT10 file */
    std::size_t sliceSize; /* Size of one slice including all mip levels */

    bool compressed;
    union Properties {
        /* Yeah fuck off C++, this is unhelpful, this is not the point where I
           want to initialize anything, THIS IS NOT, the parent struct is */
        Properties() {}

        struct {
            PixelFormat format;
            bool needsSwizzle;
            BitVector2 yzFlip{NoInit};
            UnsignedInt pixelSize;
        } uncompressed;

        struct {
            CompressedPixelFormat format;
            Vector3i blockSize{NoInit};
            UnsignedInt blockDataSize;
        } compressed;
    } properties;
};

DdsImporter::DdsImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

DdsImporter::~DdsImporter() = default;

ImporterFeatures DdsImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool DdsImporter::doIsOpened() const { return !!_f; }

void DdsImporter::doClose() { _f = nullptr; }

namespace {

Containers::Triple<std::size_t, std::size_t, Vector3i> levelOffsetSize(Vector3i size, const Vector3i& blockSize, const UnsignedInt blockDataSize, const UnsignedInt level) {
    std::size_t offset = 0;
    std::size_t dataSize = blockDataSize*((size + blockSize - Vector3i{1})/blockSize).product();
    for(UnsignedInt i = 0; i != level; ++i) {
        offset += dataSize;
        size = Math::max(size >> 1, Vector3i{1});
        dataSize = blockDataSize*((size + blockSize - Vector3i{1})/blockSize).product();
    }

    return {offset, dataSize, size};
}

Containers::Triple<std::size_t, std::size_t, Vector3i> levelOffsetSize(Vector3i size, const UnsignedInt pixelSize, const UnsignedInt level) {
    std::size_t offset = 0;
    std::size_t dataSize = size.product()*pixelSize;
    for(UnsignedInt i = 0; i != level; ++i) {
        offset += dataSize;
        size = Math::max(size >> 1, Vector3i{1});
        dataSize = size.product()*pixelSize;
    }

    return {offset, dataSize, size};
}

}

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

    /* Verify file signature */
    if(Containers::StringView{header.signature, 4} != "DDS "_s) {
        Error() << "Trade::DdsImporter::openData(): invalid file signature" << Containers::StringView{header.signature, 4};
        return;
    }

    /* Format defined by FourCC, possibly a DXT10 header */
    bool hasDxt10Header = false;
    f->dataOffset = sizeof(DdsHeader);
    if(header.ddspf.flags & DdsPixelFormatFlag::FourCC) {
        switch(header.ddspf.fourCC) {
            case Utility::Endianness::fourCC('D', 'X', 'T', '1'):
                f->compressed = true;
                f->properties.compressed.format = CompressedPixelFormat::Bc1RGBAUnorm;
                break;
            case Utility::Endianness::fourCC('D', 'X', 'T', '3'):
                f->compressed = true;
                f->properties.compressed.format = CompressedPixelFormat::Bc2RGBAUnorm;
                break;
            case Utility::Endianness::fourCC('D', 'X', 'T', '5'):
                f->compressed = true;
                f->properties.compressed.format = CompressedPixelFormat::Bc3RGBAUnorm;
                break;
            case Utility::Endianness::fourCC('A', 'T', 'I', '1'):
                f->compressed = true;
                f->properties.compressed.format = CompressedPixelFormat::Bc4RUnorm;
                break;
            case Utility::Endianness::fourCC('B', 'C', '4', 'S'):
                f->compressed = true;
                f->properties.compressed.format = CompressedPixelFormat::Bc4RSnorm;
                break;
            case Utility::Endianness::fourCC('A', 'T', 'I', '2'):
                f->compressed = true;
                f->properties.compressed.format = CompressedPixelFormat::Bc5RGUnorm;
                break;
            case Utility::Endianness::fourCC('B', 'C', '5', 'S'):
                f->compressed = true;
                f->properties.compressed.format = CompressedPixelFormat::Bc5RGSnorm;
                break;
            /** @todo there's also a bunch of other numeric values mentioned at
                https://docs.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide
                -- what do they represent? */
            case Utility::Endianness::fourCC('D', 'X', '1', '0'): {
                if(f->in.size() < sizeof(DdsHeader) + sizeof(DdsHeaderDxt10)) {
                    Error{} << "Trade::DdsImporter::openData(): DXT10 file too short, expected at least" << sizeof(DdsHeader) + sizeof(DdsHeaderDxt10) << "bytes but got" << f->in.size();
                    return;
                }

                hasDxt10Header = true;
                f->dataOffset += sizeof(DdsHeaderDxt10);
                const DdsHeaderDxt10& headerDxt10 = *reinterpret_cast<const DdsHeaderDxt10*>(f->in.data() + sizeof(DdsHeader));

                if(headerDxt10.dxgiFormat >= Containers::arraySize(DxgiFormatMapping)) {
                    Error{} << "Trade::DdsImporter::openData(): unknown DXGI format ID" << headerDxt10.dxgiFormat;
                    return;
                }

                /* Decide about dimensionality and slice count */
                if(headerDxt10.resourceDimension == DdsDimension::Texture3D) {
                    f->dimensions = 3;
                    f->sliceCount = 1;
                    if(headerDxt10.miscFlag & DdsMiscFlag::TextureCube) {
                        Error{} << "Trade::DdsImporter::openData(): cube map flag set for a DXT10 3D texture";
                        return;
                    } else if(headerDxt10.arraySize != 1) {
                        Error{} << "Trade::DdsImporter::openData(): invalid array size" << headerDxt10.arraySize << "for a DXT10 3D texture";
                        return;
                    }
                } else if(headerDxt10.resourceDimension == DdsDimension::Texture2D) {
                    if(headerDxt10.miscFlag & DdsMiscFlag::TextureCube) {
                        f->dimensions = 3;
                        /* If it's a cube map, the slice count is actually
                           count of cubes */
                        f->sliceCount = headerDxt10.arraySize*6;
                        f->imageFlags |= ImageFlag3D::CubeMap;
                        if(headerDxt10.arraySize != 1)
                            f->imageFlags |= ImageFlag3D::Array;
                    } else if(headerDxt10.arraySize != 1) {
                        f->dimensions = 3;
                        f->sliceCount = headerDxt10.arraySize;
                        f->imageFlags |= ImageFlag3D::Array;
                    } else {
                        f->dimensions = 2;
                        f->sliceCount = 1;
                    }
                } else if(headerDxt10.resourceDimension == DdsDimension::Texture1D) {
                    if(headerDxt10.miscFlag & DdsMiscFlag::TextureCube) {
                        Error{} << "Trade::DdsImporter::openData(): cube map flag set for a DXT10 1D texture";
                        return;
                    } else if(headerDxt10.arraySize != 1) {
                        f->dimensions = 2;
                        f->sliceCount = headerDxt10.arraySize;
                        f->imageFlags |= ImageFlag3D::Array;
                    } else {
                        f->dimensions = 1;
                        f->sliceCount = 1;
                    }
                } else {
                    Error{} << "Trade::DdsImporter::openData(): invalid DXT10 resource dimension" << UnsignedInt(headerDxt10.resourceDimension);
                    return;
                }

                const auto& mapped = DxgiFormatMapping[headerDxt10.dxgiFormat];
                if(mapped.format) {
                    f->compressed = false;
                    f->properties.uncompressed.format = PixelFormat(mapped.format);
                    f->properties.uncompressed.needsSwizzle = mapped.needsSwizzle;
                } else if(mapped.compressedFormat) {
                    f->compressed = true;
                    f->properties.compressed.format = CompressedPixelFormat(mapped.compressedFormat);
                } else if(mapped.name) {
                    Error{} << "Trade::DdsImporter::openData(): unsupported format DXGI_FORMAT_" << Debug::nospace << mapped.name;
                    return;
                } else {
                    Error{} << "Trade::DdsImporter::openData(): unknown DXGI format ID" << headerDxt10.dxgiFormat;
                    return;
                }
            } break;
            case Utility::Endianness::fourCC('D', 'X', 'T', '2'):
            case Utility::Endianness::fourCC('D', 'X', 'T', '4'):
            default:
                Error() << "Trade::DdsImporter::openData(): unknown compression" << Containers::StringView{header.ddspf.fourCCChars, 4};
                return;
        }

    /* RGBA or RGBX with the alpha unspecified. The X variant is produced by
       NVidia Texture Tools from a RGB input. */
    } else if(header.ddspf.rgbBitCount == 32 &&
              header.ddspf.rBitMask == 0x000000ff &&
              header.ddspf.gBitMask == 0x0000ff00 &&
              header.ddspf.bBitMask == 0x00ff0000 &&
            ((header.ddspf.flags == DdsPixelFormatFlag::RGBA &&
              header.ddspf.aBitMask == 0xff000000) ||
             (header.ddspf.flags == DdsPixelFormatFlag::RGB &&
              header.ddspf.aBitMask == 0x00000000))) {
        f->compressed = false;
        f->properties.uncompressed.format = PixelFormat::RGBA8Unorm;
        f->properties.uncompressed.needsSwizzle = false;

    /* BGRA or BGRX with the alpha unspecified. The X variant is produced by
       NVidia Texture Tools from a RGB input. */
    } else if(header.ddspf.rgbBitCount == 32 &&
              header.ddspf.rBitMask == 0x00ff0000 &&
              header.ddspf.gBitMask == 0x0000ff00 &&
              header.ddspf.bBitMask == 0x000000ff &&
            ((header.ddspf.flags == DdsPixelFormatFlag::RGBA &&
              header.ddspf.aBitMask == 0xff000000) ||
             (header.ddspf.flags == DdsPixelFormatFlag::RGB &&
              header.ddspf.aBitMask == 0x00000000))) {
        f->compressed = false;
        f->properties.uncompressed.format = PixelFormat::RGBA8Unorm;
        f->properties.uncompressed.needsSwizzle = true;

    /* RGB */
    } else if(header.ddspf.flags == DdsPixelFormatFlag::RGB &&
              header.ddspf.rgbBitCount == 24 &&
              header.ddspf.rBitMask == 0x000000ff &&
              header.ddspf.gBitMask == 0x0000ff00 &&
              header.ddspf.bBitMask == 0x00ff0000) {
        f->compressed = false;
        f->properties.uncompressed.format = PixelFormat::RGB8Unorm;
        f->properties.uncompressed.needsSwizzle = false;

    /* BGR */
    } else if(header.ddspf.flags == DdsPixelFormatFlag::RGB &&
              header.ddspf.rgbBitCount == 24 &&
              header.ddspf.rBitMask == 0x00ff0000 &&
              header.ddspf.gBitMask == 0x0000ff00 &&
              header.ddspf.bBitMask == 0x000000ff) {
        f->compressed = false;
        f->properties.uncompressed.format = PixelFormat::RGB8Unorm;
        f->properties.uncompressed.needsSwizzle = true;

    /* Grayscale */
    } else if(header.ddspf.flags == DdsPixelFormatFlag::RGB &&
              header.ddspf.rgbBitCount == 8 &&
              header.ddspf.rBitMask == 0x000000ff &&
              header.ddspf.gBitMask == 0x00000000 &&
              header.ddspf.bBitMask == 0x00000000) {
        f->compressed = false;
        f->properties.uncompressed.format = PixelFormat::R8Unorm;
        f->properties.uncompressed.needsSwizzle = false;

    /* Something else. We're far from handling all cases, so be verbose in what
       was encountered. */
    } else {
        Error err;
        err << "Trade::DdsImporter::openData(): unknown" << header.ddspf.rgbBitCount << "bits per pixel format with";
        if(header.ddspf.flags == DdsPixelFormatFlag::RGBA)
            err << "a RGBA";
        else if(header.ddspf.flags == DdsPixelFormatFlag::RGB)
            err << "a RGB";
        else
            err << "flags" << reinterpret_cast<void*>(reinterpret_cast<const UnsignedInt&>(header.ddspf.flags)) << "and a";
        err << "mask" << Debug::packed << Math::Vector4<void*>{reinterpret_cast<void*>(header.ddspf.rBitMask), reinterpret_cast<void*>(header.ddspf.gBitMask), reinterpret_cast<void*>(header.ddspf.bBitMask), reinterpret_cast<void*>(header.ddspf.aBitMask)};
        return;
    }

    /* Depth can be set to 0 by some exporters, it should be 1 at least. Not
       relying on DdsDescriptionFlag::Depth, because that may not be set
       either. */
    f->topLevelSliceSize = {Int(header.width),
                       Int(header.height),
                       Math::max(Int(header.depth), 1)};

    /* Check how many mipmaps to load, but at least one. Not relying on
       DdsDescriptionFlag::MipMapCount, because that may not be set either. */
    f->levelCount = Math::max(header.mipMapCount, 1u);

    /* If the DXT10 header was not present, fall back to parsing the legacy
       fields to decide about dimensionality and slice count. In this case
       there's no way to have an array or 1D texture. We prefer the DXT10
       information, so if it's present we don't even check correctness of the
       legacy fields. */
    if(!hasDxt10Header) {
        if(header.caps2 & DdsCap2::Volume) {
            f->dimensions = 3;
            f->sliceCount = 1;
            if(header.caps2 & DdsCap2::CubeMap) {
                Error{} << "Trade::DdsImporter::openData(): cube map flag set for a 3D texture";
                return;
            }
        } else if(header.caps2 & DdsCap2::CubeMap) {
            /* Checked above, assert just to be sure */
            CORRADE_INTERNAL_ASSERT(!(header.caps2 & DdsCap2::Volume));
            f->dimensions = 3;
            /* The cubemap can be incomplete, in that case import it as a
               regular array */
            f->sliceCount = Math::popcount(UnsignedInt(header.caps2 & DdsCap2::CubeMapAllFaces));
            if(f->sliceCount == 6) {
                f->imageFlags |= ImageFlag3D::CubeMap;
            } else {
                f->imageFlags |= ImageFlag3D::Array;
                Warning{} << "Trade::DdsImporter::openData(): the image is an incomplete cubemap, importing faces as" << f->sliceCount << "array layers";
            }
        } else {
            f->dimensions = 2;
            f->sliceCount = 1;
        }
    }

    /* Height should be > 1 only for 2D textures */
    if(f->topLevelSliceSize.y() != 1 && (f->dimensions == 1 || (f->dimensions == 2 && f->imageFlags & ImageFlag3D::Array))) {
        Error{} << "Trade::DdsImporter::openData(): height is" << header.height << "but the texture is 1D";
        return;
    }

    /* Depth should be > 1 only for 3D textures */
    if(f->topLevelSliceSize.z() != 1 && (f->dimensions != 3 || (f->dimensions == 3 && (f->imageFlags & (ImageFlag3D::Array|ImageFlag3D::CubeMap))))) {
        Error{} << "Trade::DdsImporter::openData(): depth is" << header.depth << "but the texture isn't 3D";
        return;
    }

    /* Cache pixel / block size so we don't need to query it again on each
       image access; calculate slice size so we can quickly access all slices
       of a particular level. Abusing the levelOffsetSize(), calling it with
       level count instead of ID gives total size of all levels. */
    if(f->compressed) {
        f->properties.compressed.blockSize = compressedPixelFormatBlockSize(f->properties.compressed.format);
        f->properties.compressed.blockDataSize = compressedPixelFormatBlockDataSize(f->properties.compressed.format);
        f->sliceSize = levelOffsetSize(f->topLevelSliceSize, f->properties.compressed.blockSize, f->properties.compressed.blockDataSize, f->levelCount).first();
    } else {
        f->properties.uncompressed.pixelSize = pixelFormatSize(f->properties.uncompressed.format);
        f->sliceSize = levelOffsetSize(f->topLevelSliceSize, f->properties.uncompressed.pixelSize, f->levelCount).first();
    }

    /* Check bounds */
    const std::size_t expectedSize = f->dataOffset + f->sliceSize*f->sliceCount;
    if(expectedSize > f->in.size()) {
        Error() << "Trade::DdsImporter::openData(): file too short, expected" << expectedSize << "bytes for" << f->sliceCount << "slices with" << f->levelCount << "levels and" << f->sliceSize << "bytes each but got" << f->in.size();
        return;
    } else if(expectedSize < f->in.size()) {
        Warning{} << "Trade::DdsImporter::openData(): ignoring" << f->in.size() - expectedSize << "extra bytes at the end of file";
    }

    /* Decide about data flipping. Unlike KTX or Basis, the file format doesn't
       contain any orientation metadata, so we have to rely on an
       externally-provided hint. */
    if(configuration().value<bool>("assumeYUpZBackward")) {
        /* No flipping if Y up / Z backward is assumed */
        f->properties.uncompressed.yzFlip = BitVector2{0x0};
    } else {
        /* Can't flip compressed blocks at the moment, so print a warning at
           least */
        if(f->compressed) {
            Warning{} << "Trade::DdsImporter::openData(): block-compressed image is assumed to be encoded with Y down and Z forward, imported data will have wrong orientation. Enable assumeYUpZBackward to suppress this warning.";

        /* For uncompressed data decide about the flip orientations */
        } else {
            f->properties.uncompressed.yzFlip = BitVector2{0x0};
            /* Z gets flipped only for a 3D texture */
            if(f->dimensions == 3 && !(f->imageFlags & (ImageFlag3D::Array|ImageFlag3D::CubeMap)))
                f->properties.uncompressed.yzFlip.set(1, true);
            /* Y gets flipped only if it's not a 1D (array) texture */
            if(f->dimensions == 3 || (f->dimensions == 2 && !(f->imageFlags & ImageFlag3D::Array)))
                f->properties.uncompressed.yzFlip.set(0, true);
        }
    }

    if(flags() & ImporterFlag::Verbose) {
        if(f->properties.uncompressed.yzFlip.any()) {
            const Containers::StringView axes[3]{
                f->properties.uncompressed.yzFlip[0] ? "y"_s : ""_s,
                f->properties.uncompressed.yzFlip[1] ? "z"_s : ""_s
            };
            Debug{} << "Trade::DdsImporter::openData(): image will be flipped along" << " and "_s.joinWithoutEmptyParts(axes);
        }

        if(!f->compressed && f->properties.uncompressed.needsSwizzle) {
            if(f->properties.uncompressed.format == PixelFormat::RGB8Unorm)
                Debug{} << "Trade::DdsImporter::openData(): format requires conversion from BGR to RGB";
            else if(f->properties.uncompressed.format == PixelFormat::RGBA8Unorm)
                Debug{} << "Trade::DdsImporter::openData(): format requires conversion from BGRA to RGBA";
            else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }
    }

    /* Everything okay, save the file for later use */
    _f = std::move(f);
}

namespace {

void swizzlePixels(const PixelFormat format, const Containers::ArrayView<char> data) {
    if(format == PixelFormat::RGB8Unorm) {
        for(Vector3ub& pixel: Containers::arrayCast<Vector3ub>(data))
            pixel = Math::gather<'b', 'g', 'r'>(pixel);
    } else if(format == PixelFormat::RGBA8Unorm) {
        for(Vector4ub& pixel: Containers::arrayCast<Vector4ub>(data))
            pixel = Math::gather<'b', 'g', 'r', 'a'>(pixel);
    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
}

void flipPixels(const BitVector2 yzFlip, const UnsignedInt pixelSize, const Vector3i& size, const Containers::ArrayView<char> data) {
    const Containers::StridedArrayView4D<char> view{data, {
        std::size_t(size.z()),
        std::size_t(size.y()),
        std::size_t(size.x()),
        pixelSize
    }};
    if(yzFlip[0]) Utility::flipInPlace<1>(view);
    if(yzFlip[1]) Utility::flipInPlace<0>(view);
}

}

template<UnsignedInt dimensions> ImageData<dimensions> DdsImporter::doImage(UnsignedInt, const UnsignedInt level) {
    /* Calculate input offset, data size and image slice size */
    const Containers::Triple<std::size_t, std::size_t, Vector3i> offsetSize = _f->compressed ?
        levelOffsetSize(_f->topLevelSliceSize, _f->properties.compressed.blockSize, _f->properties.compressed.blockDataSize, level) :
        levelOffsetSize(_f->topLevelSliceSize, _f->properties.uncompressed.pixelSize, level);

    /* Image size is slice size combined with slice count */
    Vector3i imageSize = offsetSize.third();
    if(_f->sliceCount != 1) {
        CORRADE_INTERNAL_ASSERT(imageSize[dimensions - 1] == 1);
        imageSize[dimensions - 1] = _f->sliceCount;
    }

    /* Allocate image data */
    Containers::Array<char> data{NoInit, offsetSize.second()*_f->sliceCount};

    /* Copy all slices */
    for(std::size_t i = 0; i != _f->sliceCount; ++i) {
        const std::size_t inputOffset = _f->dataOffset + i*_f->sliceSize + offsetSize.first();
        const std::size_t outputOffset = i*offsetSize.second();
        Utility::copy(_f->in.slice(inputOffset, inputOffset + offsetSize.second()),
            data.slice(outputOffset, outputOffset + offsetSize.second()));
    }

    /* Compressed image */
    if(_f->compressed)
        return ImageData<dimensions>{_f->properties.compressed.format, Math::Vector<dimensions, Int>::pad(imageSize), std::move(data), ImageFlag<dimensions>(UnsignedShort(_f->imageFlags))};

    /* Uncompressed. Swizzle and flip if needed. */
    if(_f->properties.uncompressed.needsSwizzle)
        swizzlePixels(_f->properties.uncompressed.format, data);
    flipPixels(_f->properties.uncompressed.yzFlip, _f->properties.uncompressed.pixelSize, imageSize, data);

    /* Adjust pixel storage if row size is not four byte aligned */
    PixelStorage storage;
    if((imageSize.x()*_f->properties.uncompressed.pixelSize % 4 != 0))
        storage.setAlignment(1);

    /** @todo expose DdsAlphaMode::Premultiplied through ImageFlags once it has
        such flag */
    return ImageData<dimensions>{storage, _f->properties.uncompressed.format, Math::Vector<dimensions, Int>::pad(imageSize), std::move(data), ImageFlag<dimensions>(UnsignedShort(_f->imageFlags))};
}

UnsignedInt DdsImporter::doImage1DCount() const {
    return _f->dimensions == 1 ? 1 : 0;
}

UnsignedInt DdsImporter::doImage1DLevelCount(UnsignedInt) {
    return _f->levelCount;
}

Containers::Optional<ImageData1D> DdsImporter::doImage1D(const UnsignedInt id, const UnsignedInt level) {
    return doImage<1>(id, level);
}

UnsignedInt DdsImporter::doImage2DCount() const {
    return _f->dimensions == 2 ? 1 : 0;
}

UnsignedInt DdsImporter::doImage2DLevelCount(UnsignedInt) {
    return _f->levelCount;
}

Containers::Optional<ImageData2D> DdsImporter::doImage2D(const UnsignedInt id, const UnsignedInt level) {
    return doImage<2>(id, level);
}

UnsignedInt DdsImporter::doImage3DCount() const {
    return _f->dimensions == 3 ? 1 : 0;
}

UnsignedInt DdsImporter::doImage3DLevelCount(UnsignedInt) {
    return _f->levelCount;
}

Containers::Optional<ImageData3D> DdsImporter::doImage3D(const UnsignedInt id, const UnsignedInt level) {
    return doImage<3>(id, level);
}

#ifdef MAGNUM_BUILD_DEPRECATED
UnsignedInt DdsImporter::doTextureCount() const { return 1; }

Containers::Optional<TextureData> DdsImporter::doTexture(UnsignedInt) {
    TextureType type;
    if(_f->dimensions == 3) {
        if(_f->imageFlags >= (ImageFlag3D::CubeMap|ImageFlag3D::Array))
            type = TextureType::CubeMapArray;
        else if(_f->imageFlags & ImageFlag3D::CubeMap)
            type = TextureType::CubeMap;
        else if(_f->imageFlags & ImageFlag3D::Array)
            type = TextureType::Texture2DArray;
        else
            type = TextureType::Texture3D;
    } else if(_f->dimensions == 2) {
        if(_f->imageFlags & ImageFlag3D::Array)
            type = TextureType::Texture1DArray;
        else
            type = TextureType::Texture2D;
    } else {
        type = TextureType::Texture1D;
    }

    return TextureData{type,
        SamplerFilter::Linear,
        SamplerFilter::Linear,
        SamplerMipmap::Linear,
        SamplerWrapping::Repeat, 0};
}
#endif

}}

CORRADE_PLUGIN_REGISTER(DdsImporter, Magnum::Trade::DdsImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.5")
