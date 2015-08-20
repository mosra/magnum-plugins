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
 * Compressed texture types.
 */
enum class DdsCompressionTypes: UnsignedInt {
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

/* @brief Convert array data to RGB assuming it is in BGR. */
inline void convertBGRToRGB(char* data, const unsigned int size) {
    auto pixels = reinterpret_cast<Math::Vector3<UnsignedByte>*>(data);
    std::transform(pixels, pixels + size, pixels,
        [](Math::Vector3<UnsignedByte> pixel) { return Math::swizzle<'b', 'g', 'r'>(pixel); });
}

/* @brief Convert array data to RGBA assuming it is in BGRA. */
inline void convertBGRAToRGBA(char* data, const unsigned int size) {
    auto pixels = reinterpret_cast<Math::Vector4<UnsignedByte>*>(data);
    std::transform(pixels, pixels + size, pixels,
        [](Math::Vector4<UnsignedByte> pixel) { return Math::swizzle<'b', 'g', 'r', 'a'>(pixel); });
}

/*
 * @brief Convert array data from BGR(A) to RGB(A) depending on code.
 * @param format Format to expect data to be in.
 * @param data Array data to convert.
 * @return The format which the converted array data is in.
 */
inline PixelFormat convertPixelFormat(const PixelFormat format, Containers::Array<char>& data) {
    if(format == PixelFormat::BGR) {
        Debug() << "Trade::DdsImporter: converting from BGR to RGB";
        convertBGRToRGB(data.begin(), data.size() / 3);
        return PixelFormat::RGB;
    } else if(format == PixelFormat::BGRA) {
        Debug() << "Trade::DdsImporter: converting from BGRA to RGBA";
        convertBGRAToRGBA(data.begin(), data.size() / 4);
        return PixelFormat::RGBA;
    } else {
        return format;
    }
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

    /* components per pixel */
    UnsignedInt components;
    union {
        PixelFormat uncompressed;
        CompressedPixelFormat compressed;
    } colorFormat;

    std::vector<ImageDataOffset> imageData;
};

std::size_t DdsImporter::File::addImageDataOffset(const Vector3i& dims, const std::size_t offset)
{
    if(compressed) {
        const unsigned int size = (dims.z()*((dims.x() + 3)/4)*(((dims.y() + 3)/4))*((colorFormat.compressed == CompressedPixelFormat::RGBAS3tcDxt1) ? 8 : 16));

        imageData.push_back({dims, in.slice(offset, offset + size)});

        return offset + size;
    } else {
        const unsigned int numPixels = dims.product();
        const unsigned int size = numPixels*components;

        imageData.push_back({dims, in.slice(offset, offset + size)});

        return offset + size;
    }
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
    if(ddsh.ddspf.flags & DdsPixelFormatFlag::FourCC) {
        switch(DdsCompressionTypes(ddsh.ddspf.fourCC)) {
            case DdsCompressionTypes::DXT1:
                f->colorFormat.compressed = CompressedPixelFormat::RGBAS3tcDxt1;
                break;
            case DdsCompressionTypes::DXT3:
                f->colorFormat.compressed = CompressedPixelFormat::RGBAS3tcDxt3;
                break;
            case DdsCompressionTypes::DXT5:
                f->colorFormat.compressed = CompressedPixelFormat::RGBAS3tcDxt5;
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
        f->colorFormat.uncompressed = PixelFormat::BGRA;
    } else if(ddsh.ddspf.rgbBitCount == 32 &&
               ddsh.ddspf.rBitMask == 0x000000FF &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x00FF0000 &&
               ddsh.ddspf.aBitMask == 0xFF000000) {
        f->colorFormat.uncompressed = PixelFormat::RGBA;
    } else if(ddsh.ddspf.rgbBitCount == 24 &&
               ddsh.ddspf.rBitMask == 0x000000FF &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x00FF0000) {
        f->colorFormat.uncompressed = PixelFormat::RGB;
        f->components = 3;
    } else if(ddsh.ddspf.rgbBitCount == 24 &&
               ddsh.ddspf.rBitMask == 0x00FF0000 &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x000000FF) {
        f->colorFormat.uncompressed = PixelFormat::BGR;
        f->components = 3;
    } else if(ddsh.ddspf.rgbBitCount == 8) {
        #ifndef MAGNUM_TARGET_GLES2
        f->colorFormat.uncompressed = PixelFormat::Red;
        #else
        f->colorFormat.uncompressed = PixelFormat::Luminance;
        #endif
    } else {
        Error() << "Trade::DdsImporter::openData(): unknown format";
        return;
    }

    const Vector3i size{Int(ddsh.width), Int(ddsh.height), Int(Math::max(ddsh.depth, 1u))};

    /* check how many mipmaps to load */
    const UnsignedInt numMipmaps = (ddsh.flags & DdsDescriptionFlag::MipMapCount)
                    ? ((ddsh.mipMapCount == 0) ? 0 : ddsh.mipMapCount - 1)
                    : 0;

    /* load all surfaces for the image (6 surfaces for cubemaps) */
    const UnsignedInt numImages = (isCubemap) ? 6 : 1;
    size_t offset = sizeof(DdsHeader) + magicNumberSize;
    for(UnsignedInt n = 0; n < numImages; ++n) {
        offset = f->addImageDataOffset(size, offset);

        Vector3i mipSize{size};

        /* load all mipmaps for current surface */
        for(UnsignedInt i = 0; i < numMipmaps && (mipSize.x() || mipSize.y()); i++) {
            /* shrink to next power of 2 */
            mipSize = Math::max(mipSize >> 1, Vector3i{1});

            offset = f->addImageDataOffset(mipSize, offset);
        }
    }

    /* Everything okay, save the file for later use */
    _f = std::move(f);
}

UnsignedInt DdsImporter::doImage2DCount() const {  return _f->volume ? 0 : _f->imageData.size(); }

std::optional<ImageData2D> DdsImporter::doImage2D(UnsignedInt id) {
    File::ImageDataOffset& dataOffset = _f->imageData[id];

    /* copy image data */
    Containers::Array<char> data = Containers::Array<char>(dataOffset.data.size());
    std::copy(dataOffset.data.begin(), dataOffset.data.end(), data.begin());

    if(_f->compressed) {
        return ImageData2D(_f->colorFormat.compressed, dataOffset.dimensions.xy(), std::move(data));
    } else {
        PixelFormat newPixelFormat = convertPixelFormat(_f->colorFormat.uncompressed, data);
        return ImageData2D(newPixelFormat, PixelType::UnsignedByte, dataOffset.dimensions.xy(), std::move(data));
    }
}

UnsignedInt DdsImporter::doImage3DCount() const { return _f->volume ? _f->imageData.size() : 0; }

std::optional<ImageData3D> DdsImporter::doImage3D(UnsignedInt id) {
    File::ImageDataOffset& dataOffset = _f->imageData[id];

    /* copy image data */
    Containers::Array<char> data = Containers::Array<char>(dataOffset.data.size());
    std::copy(dataOffset.data.begin(), dataOffset.data.end(), data.begin());

    if(_f->compressed) {
        return ImageData3D(_f->colorFormat.compressed, dataOffset.dimensions, std::move(data));
    } else {
        PixelFormat newPixelFormat = convertPixelFormat(_f->colorFormat.uncompressed, data);
        return ImageData3D(newPixelFormat, PixelType::UnsignedByte, dataOffset.dimensions, std::move(data));
    }
}

}}
