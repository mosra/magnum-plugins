/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015
              Vladimír Vondruš <mosra@centrum.cz>
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

#include <cstring>
#include <algorithm>
#include <vector>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Trade/ImageData.h>

namespace Magnum { namespace Trade {

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

typedef Corrade::Containers::EnumSet<DdsDescriptionFlag> DdsDescriptionFlags;
CORRADE_ENUMSET_OPERATORS(DdsDescriptionFlags)

/* Direct Draw Surface pixel format */
enum class DdsPixelFormatFlag: UnsignedInt {
    AlphaPixels = 0x00000001,
    FourCC = 0x00000004,
    RGB = 0x00000040,
    RGBA = 0x00000041
};

typedef Corrade::Containers::EnumSet<DdsPixelFormatFlag> DdsPixelFormatFlags;
CORRADE_ENUMSET_OPERATORS(DdsPixelFormatFlags)

/* Specifies the complexity of the surfaces stored */
enum class DdsCap1: UnsignedInt {
    /* Set for files that contain more than one surface (a mipmap, a cubic
       environment map, or mipmapped volume texture) */
    Complex = 0x00000008,
    /* Texture (required). */
    Texture = 0x00001000,
    /* Is set for mipmaps. */
    MipMap = 0x00400000
};

typedef Corrade::Containers::EnumSet<DdsCap1> DdsCaps1;
CORRADE_ENUMSET_OPERATORS(DdsCaps1)

/** Additional detail about the surfaces stored */
enum class DdsCap2: UnsignedInt {
    Cubemap = 0x00000200,
    CubemapPositiveX = 0x00000400,
    CubemapNegativeX = 0x00000800,
    CubemapPositiveY = 0x00001000,
    CubemapNegativeY = 0x00002000,
    CubemapPositiveZ = 0x00004000,
    CubemapNegativeZ = 0x00008000,
    CubemapAllFaces = 0x0000FC00,
    Volume = 0x00200000
};

typedef Corrade::Containers::EnumSet<DdsCap2> DdsCaps2;
CORRADE_ENUMSET_OPERATORS(DdsCaps2)

/*
 * Compressed texture type.
 */
enum class DdsCompressionType: UnsignedInt {
    /* MAKEFOURCC('D','X','T','1'). */
    DXT1 = 0x31545844,
    /* MAKEFOURCC('D','X','T','2'), not supported. */
    DXT2 = 0x32545844,
    /* MAKEFOURCC('D','X','T','3'). */
    DXT3 = 0x33545844,
    /* MAKEFOURCC('D','X','T','4'), not supported. */
    DXT4 = 0x34545844,
    /* MAKEFOURCC('D','X','T','5'). */
    DXT5 = 0x35545844,
    /* MAKEFOURCC('D','X','1','0'), not supported. */
    DXT10 = 0x30315844
};

/*
 * @return string from given fourcc integer.
 */
inline std::string fourcc(UnsignedInt enc) {
    char c[5] = { '\0' };
    c[0] = enc >> 0 & 0xFF;
    c[1] = enc >> 8 & 0xFF;
    c[2] = enc >> 16 & 0xFF;
    c[3] = enc >> 24 & 0xFF;
    return c;
}

void swizzlePixels(const PixelFormat format, Containers::Array<char>& data) {
    if(format == PixelFormat::RGB) {
        Debug() << "Trade::DdsImporter: converting from BGR to RGB";
        auto pixels = reinterpret_cast<Math::Vector3<UnsignedByte>*>(data.data());
        std::transform(pixels, pixels + data.size()/sizeof(Math::Vector3<UnsignedByte>), pixels,
            [](Math::Vector3<UnsignedByte> pixel) { return Math::swizzle<'b', 'g', 'r'>(pixel); });

    } else if(format == PixelFormat::RGBA) {
        Debug() << "Trade::DdsImporter: converting from BGRA to RGBA";
        auto pixels = reinterpret_cast<Math::Vector4<UnsignedByte>*>(data.data());
        std::transform(pixels, pixels + data.size()/sizeof(Math::Vector4<UnsignedByte>), pixels,
            [](Math::Vector4<UnsignedByte> pixel) { return Math::swizzle<'b', 'g', 'r', 'a'>(pixel); });

    } else CORRADE_ASSERT_UNREACHABLE();
}

/*
 * Dds file header struct.
 */
struct DdsHeader {
    UnsignedInt size;
    DdsDescriptionFlags flags;
    UnsignedInt height;
    UnsignedInt width;
    UnsignedInt pitchOrLinearSize;
    UnsignedInt depth;
    UnsignedInt mipMapCount;
    UnsignedInt reserved1[11];
    struct {
        /* pixel format */
        UnsignedInt size;
        DdsPixelFormatFlags flags;
        UnsignedInt fourCC;
        UnsignedInt rgbBitCount;
        UnsignedInt rBitMask;
        UnsignedInt gBitMask;
        UnsignedInt bBitMask;
        UnsignedInt aBitMask;
    } ddspf;
    DdsCaps1 caps;
    DdsCaps2 caps2;
    UnsignedInt caps3;
    UnsignedInt caps4;
    UnsignedInt reserved2;
};

static_assert(sizeof(DdsHeader) + 4 == 128, "Improper size of DdsHeader struct");

}

struct DdsImporter::File {
    struct ImageDataOffset {
        Vector3i dimensions;
        Containers::ArrayView<char> data;
    };

    std::size_t addImageDataOffset(const Vector3i& dims, std::size_t offset);

    Containers::Array<char> in;

    bool compressed;
    bool volume;
    bool needsSwizzle;

    /* components per pixel */
    UnsignedInt components;
    union {
        PixelFormat uncompressed;
        CompressedPixelFormat compressed;
    } pixelFormat;

    std::vector<ImageDataOffset> imageData;
};

std::size_t DdsImporter::File::addImageDataOffset(const Vector3i& dims, const std::size_t offset)
{
    const std::size_t size = compressed ?
        (dims.z()*((dims.x() + 3)/4)*(((dims.y() + 3)/4))*((pixelFormat.compressed == CompressedPixelFormat::RGBAS3tcDxt1) ? 8 : 16)) :
        dims.product()*components;

    imageData.push_back({dims, in.slice(offset, offset + size)});

    return offset + size;
}

DdsImporter::DdsImporter() = default;

DdsImporter::DdsImporter(PluginManager::AbstractManager& manager, std::string plugin): AbstractImporter{manager, std::move(plugin)} {}

DdsImporter::~DdsImporter() = default;

auto DdsImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool DdsImporter::doIsOpened() const { return !!_f; }

void DdsImporter::doClose() { _f = nullptr; }

void DdsImporter::doOpenData(const Containers::ArrayView<const char> data) {
    std::unique_ptr<File> f{new File};

    /* clear previous data */
    f->in = Containers::Array<char>(data.size());
    std::copy(data.begin(), data.end(), f->in.begin());

    constexpr size_t magicNumberSize = 4;
    /* read magic number to verify this is a dds file. */
    if(strncmp(f->in.prefix(magicNumberSize).data(), "DDS ", magicNumberSize) != 0) {
        Error() << "Trade::DdsImporter::openData(): wrong file signature";
        return;
    }

    /* read in DDS header */
    const DdsHeader& ddsh = *reinterpret_cast<const DdsHeader*>(f->in.suffix(magicNumberSize).data());

    /* check if image is a 2D or 3D texture */
    f->volume = ((ddsh.caps2 & DdsCap2::Volume) && (ddsh.depth > 0));

    /* check if image is a cubemap */
    const bool isCubemap = !!(ddsh.caps2 & DdsCap2::Cubemap);

    /* set the color format */
    f->components = 4;
    f->compressed = false;
    f->needsSwizzle = false;
    if(ddsh.ddspf.flags & DdsPixelFormatFlag::FourCC) {
        switch(DdsCompressionType(ddsh.ddspf.fourCC)) {
            case DdsCompressionType::DXT1:
                f->pixelFormat.compressed = CompressedPixelFormat::RGBAS3tcDxt1;
                break;
            case DdsCompressionType::DXT3:
                f->pixelFormat.compressed = CompressedPixelFormat::RGBAS3tcDxt3;
                break;
            case DdsCompressionType::DXT5:
                f->pixelFormat.compressed = CompressedPixelFormat::RGBAS3tcDxt5;
                break;
            default:
                Error() << "Trade::DdsImporter::openData(): unknown compression" << fourcc(ddsh.ddspf.fourCC);
                return;
        }
        f->compressed = true;
    } else if(ddsh.ddspf.rgbBitCount == 32 &&
               ddsh.ddspf.rBitMask == 0x00FF0000 &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x000000FF &&
               ddsh.ddspf.aBitMask == 0xFF000000) {
        f->pixelFormat.uncompressed = PixelFormat::RGBA;
        f->needsSwizzle = true;
    } else if(ddsh.ddspf.rgbBitCount == 32 &&
               ddsh.ddspf.rBitMask == 0x000000FF &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x00FF0000 &&
               ddsh.ddspf.aBitMask == 0xFF000000) {
        f->pixelFormat.uncompressed = PixelFormat::RGBA;
    } else if(ddsh.ddspf.rgbBitCount == 24 &&
               ddsh.ddspf.rBitMask == 0x000000FF &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x00FF0000) {
        f->pixelFormat.uncompressed = PixelFormat::RGB;
        f->components = 3;
    } else if(ddsh.ddspf.rgbBitCount == 24 &&
               ddsh.ddspf.rBitMask == 0x00FF0000 &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x000000FF) {
        f->pixelFormat.uncompressed = PixelFormat::RGB;
        f->needsSwizzle = true;
        f->components = 3;
    } else if(ddsh.ddspf.rgbBitCount == 8) {
        #ifndef MAGNUM_TARGET_GLES2
        f->pixelFormat.uncompressed = PixelFormat::Red;
        #else
        f->pixelFormat.uncompressed = PixelFormat::Luminance;
        #endif
    } else {
        Error() << "Trade::DdsImporter::openData(): unknown format";
        return;
    }

    const Vector3i size{Int(ddsh.width), Int(ddsh.height), Int(Math::max(ddsh.depth, 1u))};

    /* check how many mipmaps to load */
    const UnsignedInt numMipmaps = ddsh.flags & DdsDescriptionFlag::MipMapCount ? ddsh.mipMapCount : 1;

    /* load all surfaces for the image (6 surfaces for cubemaps) */
    const UnsignedInt numImages = isCubemap ? 6 : 1;
    std::size_t offset = sizeof(DdsHeader) + magicNumberSize;
    for(UnsignedInt n = 0; n < numImages; ++n) {
        Vector3i mipSize{size};

        /* load all mipmaps for current surface */
        for(UnsignedInt i = 0; i < numMipmaps; i++) {
            offset = f->addImageDataOffset(mipSize, offset);

            /* shrink to next power of 2 */
            mipSize = Math::max(mipSize >> 1, Vector3i{1});
        }
    }

    /* Everything okay, save the file for later use */
    _f = std::move(f);
}

UnsignedInt DdsImporter::doImage2DCount() const {  return _f->volume ? 0 : _f->imageData.size(); }

std::optional<ImageData2D> DdsImporter::doImage2D(UnsignedInt id) {
    const File::ImageDataOffset& dataOffset = _f->imageData[id];

    /* copy image data */
    Containers::Array<char> data = Containers::Array<char>(dataOffset.data.size());
    std::copy(dataOffset.data.begin(), dataOffset.data.end(), data.begin());

    /* Compressed image */
    if(_f->compressed)
        return ImageData2D(_f->pixelFormat.compressed, dataOffset.dimensions.xy(), std::move(data));

    /* Uncompressed */
    if(_f->needsSwizzle) swizzlePixels(_f->pixelFormat.uncompressed, data);

    /* Adjust pixel storage if row size is not four byte aligned */
    PixelStorage storage;
    if((dataOffset.dimensions.x()*PixelStorage::pixelSize(_f->pixelFormat.uncompressed, PixelType::UnsignedByte))%4 != 0)
        storage.setAlignment(1);

    return ImageData2D{storage, _f->pixelFormat.uncompressed, PixelType::UnsignedByte, dataOffset.dimensions.xy(), std::move(data)};
}

UnsignedInt DdsImporter::doImage3DCount() const { return _f->volume ? _f->imageData.size() : 0; }

std::optional<ImageData3D> DdsImporter::doImage3D(UnsignedInt id) {
    const File::ImageDataOffset& dataOffset = _f->imageData[id];

    /* copy image data */
    Containers::Array<char> data = Containers::Array<char>(dataOffset.data.size());
    std::copy(dataOffset.data.begin(), dataOffset.data.end(), data.begin());

    /* Compressed image */
    if(_f->compressed)
        return ImageData3D(_f->pixelFormat.compressed, dataOffset.dimensions, std::move(data));

    /* Uncompressed */
    if(_f->needsSwizzle) swizzlePixels(_f->pixelFormat.uncompressed, data);

    /* Adjust pixel storage if row size is not four byte aligned */
    PixelStorage storage;
    if((dataOffset.dimensions.x()*PixelStorage::pixelSize(_f->pixelFormat.uncompressed, PixelType::UnsignedByte))%4 != 0)
        storage.setAlignment(1);

    return ImageData3D{storage, _f->pixelFormat.uncompressed, PixelType::UnsignedByte, dataOffset.dimensions, std::move(data)};
}

}}
