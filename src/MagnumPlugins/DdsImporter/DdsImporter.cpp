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

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>
#include <MagnumPlugins/DdsImporter/DdsImporter.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>

#ifdef MAGNUM_TARGET_GLES2
#include <Magnum/Context.h>
#include <Magnum/Extensions.h>
#endif

namespace Magnum { namespace Trade {

namespace {

/*
 * Flags to indicate which members of a DdsHeader contain valid data.
 */
enum DdsDescriptionFlag: UnsignedInt {
    Caps = 0x00000001,
    Height = 0x00000002,
    Width = 0x00000004,
    Pitch = 0x00000008,
    PixelFormat = 0x00001000,
    MipMapCount = 0x00020000,
    LinearSize = 0x00080000,
    Depth = 0x00800000
};

/*
 * Direct Draw Surface pixel format.
 */
enum DdsPixelFormat: UnsignedInt {
    AlphaPixels = 0x00000001,
    FourCC = 0x00000004,
    RGB = 0x00000040,
    RGBA = 0x00000041
};

/*
 * Specifies the complexity of the surfaces stored.
 */
enum DdsCaps1: UnsignedInt {
    /* Set for files that contain more than one surface (a mipmap, a cubic environment map,
     * or mipmapped volume texture). */
    Complex = 0x00000008,
    /* Texture (required). */
    Texture = 0x00001000,
    /* Is set for mipmaps. */
    MipMap = 0x00400000
};

/**
 * Additional detail about the surfaces stored.
 */
enum DdsCaps2: UnsignedInt {
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

/*
 * Compressed texture types.
 */
enum DdsCompressionTypes: UnsignedInt {
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
        Debug() << "Converting from BGR to RGB";
        convertBGRToRGB(data.begin(), data.size() / 3);
        return PixelFormat::RGB;
    } else if(format == PixelFormat::BGRA) {
        Debug() << "Converting from BGRA to RGBA";
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
    UnsignedInt     size;
    UnsignedInt     flags;
    UnsignedInt     height;
    UnsignedInt     width;
    UnsignedInt     pitchOrLinearSize;
    UnsignedInt     depth;
    UnsignedInt     mipMapCount;
    UnsignedInt     reserved1[11];
    struct {
        /* pixel format */
        UnsignedInt size;
        UnsignedInt flags;
        UnsignedInt fourCC;
        UnsignedInt rgbBitCount;
        UnsignedInt rBitMask;
        UnsignedInt gBitMask;
        UnsignedInt bBitMask;
        UnsignedInt aBitMask;
    } ddspf;
    UnsignedInt     caps;
    UnsignedInt     caps2;
    UnsignedInt     caps3;
    UnsignedInt     caps4;
    UnsignedInt     reserved2;
};

}

DdsImporter::DdsImporter():
    _in(nullptr),
    _compressed(false),
    _volume(false),
    _imageData()
{

}

DdsImporter::DdsImporter(PluginManager::AbstractManager& manager, std::string plugin):
    AbstractImporter(manager, std::move(plugin)),
    _in(nullptr),
    _compressed(false),
    _volume(false),
    _imageData()
{

}

DdsImporter::~DdsImporter() { close(); }

auto DdsImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool DdsImporter::doIsOpened() const { return _in; }

void DdsImporter::doClose() {
    _in = nullptr;
}

void DdsImporter::doOpenData(const Containers::ArrayView<const char> data) {
    /* clear previous data */
    _in = Containers::Array<char>(data.size());
    _imageData.clear();
    std::copy(data.begin(), data.end(), _in.begin());

    constexpr size_t magicNumberSize = 4;
    /* read magic number to verify this is a dds file. */
    if(strncmp(_in.prefix(magicNumberSize).data(), "DDS ", magicNumberSize) != 0) {
        Error() << "Not a DDS file.";
    }

    /* read in DDS header */
    const DdsHeader& ddsh = *reinterpret_cast<const DdsHeader*>(_in.suffix(magicNumberSize).data());

    /* check if image is a 2D or 3D texture */
    _volume = ((ddsh.caps2 & DdsCaps2::Volume) && (ddsh.depth > 0));

    /* check if image is a cubemap */
    const bool isCubemap = (ddsh.caps2 & DdsCaps2::Cubemap);

    /* set the color format */
    _components = 4;
    _compressed = false;
    if(ddsh.ddspf.flags & DdsPixelFormat::FourCC) {
        switch(DdsCompressionTypes(ddsh.ddspf.fourCC)) {
            case DdsCompressionTypes::DXT1:
                _colorFormat.compressed = CompressedPixelFormat::RGBAS3tcDxt1;
                break;
            case DdsCompressionTypes::DXT3:
                _colorFormat.compressed = CompressedPixelFormat::RGBAS3tcDxt3;
                break;
            case DdsCompressionTypes::DXT5:
                _colorFormat.compressed = CompressedPixelFormat::RGBAS3tcDxt5;
                break;
            default:
                Error() << "unknown texture compression '" + fourcc(ddsh.ddspf.fourCC) + "'";
        }
        _compressed = true;
    } else if(ddsh.ddspf.rgbBitCount == 32 &&
               ddsh.ddspf.rBitMask == 0x00FF0000 &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x000000FF &&
               ddsh.ddspf.aBitMask == 0xFF000000) {
        _colorFormat.uncompressed = PixelFormat::BGRA;
    } else if(ddsh.ddspf.rgbBitCount == 32 &&
               ddsh.ddspf.rBitMask == 0x000000FF &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x00FF0000 &&
               ddsh.ddspf.aBitMask == 0xFF000000) {
        _colorFormat.uncompressed = PixelFormat::RGBA;
    } else if(ddsh.ddspf.rgbBitCount == 24 &&
               ddsh.ddspf.rBitMask == 0x000000FF &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x00FF0000) {
        _colorFormat.uncompressed = PixelFormat::RGB;
        _components = 3;
    } else if(ddsh.ddspf.rgbBitCount == 24 &&
               ddsh.ddspf.rBitMask == 0x00FF0000 &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x000000FF) {
        _colorFormat.uncompressed = PixelFormat::BGR;
        _components = 3;
    } else if(ddsh.ddspf.rgbBitCount == 8) {
        #ifndef MAGNUM_TARGET_GLES2
        _colorFormat.uncompressed = PixelFormat::Red;
        #else
        _colorFormat.uncompressed = PixelFormat::Luminance;
        #endif
    } else {
        Error() << "Unknow texture format";
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
        offset = addImageDataOffset(size, offset);

        Vector3i mipSize{size};

        /* load all mipmaps for current surface */
        for(UnsignedInt i = 0; i < numMipmaps && (mipSize.x() || mipSize.y()); i++) {
            /* shrink to next power of 2 */
            mipSize = Math::max(mipSize >> 1, Vector3i{1});

            offset = addImageDataOffset(mipSize, offset);
        }
    }

}

size_t DdsImporter::addImageDataOffset(const Vector3i& dims, const size_t offset)
{
    if(_compressed) {
        const unsigned int size = (dims.z()*((dims.x() + 3)/4)*(((dims.y() + 3)/4))*((_colorFormat.compressed == CompressedPixelFormat::RGBAS3tcDxt1) ? 8 : 16));

        _imageData.push_back({dims, _in.slice(offset, offset + size)});

        return offset + size;
    } else {
        const unsigned int numPixels = dims.product();
        const unsigned int size = numPixels * _components;

        _imageData.push_back({dims, _in.slice(offset, offset + size)});

        return offset + size;
    }
}

UnsignedInt DdsImporter::doImage2DCount() const {  return (_volume) ? 0 : _imageData.size(); }

std::optional<ImageData2D> DdsImporter::doImage2D(UnsignedInt id) {
    ImageDataOffset& dataOffset = _imageData[id];

    /* copy image data */
    Containers::Array<char> data = Containers::Array<char>(dataOffset._data.size());
    std::copy(dataOffset._data.begin(), dataOffset._data.end(), data.begin());

    if(_compressed) {
        return ImageData2D(_colorFormat.compressed, dataOffset._dimensions.xy(), std::move(data));
    } else {
        PixelFormat newPixelFormat = convertPixelFormat(_colorFormat.uncompressed, data);
        return ImageData2D(newPixelFormat, PixelType::UnsignedByte, dataOffset._dimensions.xy(), static_cast<void*>(data.release()));
    }
}

UnsignedInt DdsImporter::doImage3DCount() const { return (_volume) ? _imageData.size() : 0; }

std::optional<ImageData3D> DdsImporter::doImage3D(UnsignedInt id) {
    ImageDataOffset& dataOffset = _imageData[id];

    /* copy image data */
    Containers::Array<char> data = Containers::Array<char>(dataOffset._data.size());
    std::copy(dataOffset._data.begin(), dataOffset._data.end(), data.begin());

    if(_compressed) {
        return ImageData3D(_colorFormat.compressed, dataOffset._dimensions, std::move(data));
    } else {
        PixelFormat newPixelFormat = convertPixelFormat(_colorFormat.uncompressed, data);
        return ImageData3D(newPixelFormat, PixelType::UnsignedByte, dataOffset._dimensions, static_cast<void*>(data.release()));
    }
}

}}
