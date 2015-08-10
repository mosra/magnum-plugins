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
#include <Magnum/ColorFormat.h>
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

/**
 * Flags to indicate which members of a DdsHeader contain valid data.
 */
enum DdsDescriptionFlag: UnsignedInt {
    Caps = 0x00000001,       //!< Caps
    Height = 0x00000002,     //!< Height
    Width = 0x00000004,      //!< Width
    Pitch = 0x00000008,      //!< Pitch
    PixelFormat = 0x00001000,//!< PixelFormat
    MipMapCount = 0x00020000,//!< MipMapCount
    LinearSize = 0x00080000, //!< LinearSize
    Depth = 0x00800000       //!< Depth
};

/**
 * Direct Draw Surface pixel format.
 */
enum DdsPixelFormat: UnsignedInt {
    AlphaPixels = 0x00000001,//!< AlphaPixels
    FourCC = 0x00000004,     //!< FourCC
    RGB = 0x00000040,        //!< RGB
    RGBA = 0x00000041        //!< RGBA
};

/**
 * Specifies the complexity of the surfaces stored.
 */
enum DdsCaps1: UnsignedInt {
    /**
     * Set for files that contain more than one surface (a mipmap, a cubic environment map,
     * or mipmapped volume texture).
     */
    Complex = 0x00000008,
    Texture = 0x00001000,//!< Texture (required).
    MipMap = 0x00400000  //!< Is set for mipmaps.
};

/**
 * Additional detail about the surfaces stored.
 */
enum DdsCaps2: UnsignedInt {
    Cubemap = 0x00000200,         //!< Cubemap
    CubemapPositiveX = 0x00000400,//!< CubemapPositiveX
    CubemapNegativeX = 0x00000800,//!< CubemapNegativeX
    CubemapPositiveY = 0x00001000,//!< CubemapPositiveY
    CubemapNegativeY = 0x00002000,//!< CubemapNegativeY
    CubemapPositiveZ = 0x00004000,//!< CubemapPositiveZ
    CubemapNegativeZ = 0x00008000,//!< CubemapNegativeZ
    CubemapAllFaces = 0x0000FC00, //!< CubemapAllFaces
    Volume = 0x00200000           //!< Volume
};

/**
 * Compressed texture types.
 */
enum DdsCompressionTypes: UnsignedInt {
    DXT1 = 0x31545844, //!< (MAKEFOURCC('D','X','T','1')).
    DXT2 = 0x32545844, //!< (MAKEFOURCC('D','X','T','2')), not supported.
    DXT3 = 0x33545844, //!< (MAKEFOURCC('D','X','T','3')).
    DXT4 = 0x34545844, //!< (MAKEFOURCC('D','X','T','4')), not supported.
    DXT5 = 0x35545844, //!< (MAKEFOURCC('D','X','T','5')).
    DXT10 = 0x30315844 //!< (MAKEFOURCC('D','X','1','0')), not supported.
};

inline std::string fourcc(UnsignedInt enc) {
    char c[5] = { '\0' };
    c[0] = enc >> 0 & 0xFF;
    c[1] = enc >> 8 & 0xFF;
    c[2] = enc >> 16 & 0xFF;
    c[3] = enc >> 24 & 0xFF;
    return c;
}

inline void convertBGRToRGB(char* data, const unsigned int size) {
    auto pixels = reinterpret_cast<Math::Vector3<UnsignedByte>*>(data);
    std::transform(pixels, pixels + size, pixels,
        [](Math::Vector3<UnsignedByte> pixel) { return Math::swizzle<'b', 'g', 'r'>(pixel); });
}

inline void convertBGRAToRGBA(char* data, const unsigned int size) {
    auto pixels = reinterpret_cast<Math::Vector4<UnsignedByte>*>(data);
    std::transform(pixels, pixels + size, pixels,
        [](Math::Vector4<UnsignedByte> pixel) { return Math::swizzle<'b', 'g', 'r', 'a'>(pixel); });
}

/**
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

DdsImporter::DdsImporter():
    _in(nullptr),
    _imageData1D(),
    _imageData2D(),
    _imageData3D()
{

}

DdsImporter::DdsImporter(PluginManager::AbstractManager& manager, std::string plugin):
    AbstractImporter(manager, std::move(plugin)),
    _in(nullptr),
    _imageData1D(),
    _imageData2D(),
    _imageData3D()
{

}

DdsImporter::~DdsImporter() { close(); }

auto DdsImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool DdsImporter::doIsOpened() const { return _in; }

void DdsImporter::doClose() {
    delete _in;
    _in = nullptr;
}

void DdsImporter::doOpenData(const Containers::ArrayView<const char> data) {
    _in = new std::istringstream{{data, data.size()}};

    /* read magic number to verify this is a dds file. */
    char magic[4];
    _in->read(magic, 4);
    if (strncmp(magic, "DDS ", 4) != 0) {
        Error() << "Not a DDS file.";
    }

    /* read in DDS header */
    DdsHeader ddsh;
    _in->read(reinterpret_cast<char*>(&ddsh), sizeof(DdsHeader));

    UnsignedByte dimensions = 2;

    const bool isCubemap = (ddsh.caps2 & DdsCaps2::Cubemap);

    // check if image is a 3D texture
    if ((ddsh.caps2 & DdsCaps2::Volume) && (ddsh.depth > 0))
        dimensions = 3;

    bool compressed = false;
    union {
        ColorFormat uncompressed;
        CompressedColorFormat compressed;
    } colorFormat;

    /* components per pixel */
    UnsignedInt components = 4;

    /* set the color format */
    if (ddsh.ddspf.flags & DdsPixelFormat::FourCC) {
        switch (DdsCompressionTypes(ddsh.ddspf.fourCC)) {
            case DdsCompressionTypes::DXT1:
                colorFormat.compressed = CompressedColorFormat::RGBAS3tcDxt1;
                break;
            case DdsCompressionTypes::DXT3:
                colorFormat.compressed = CompressedColorFormat::RGBAS3tcDxt3;
                break;
            case DdsCompressionTypes::DXT5:
                colorFormat.compressed = CompressedColorFormat::RGBAS3tcDxt5;
                break;
            default:
                Error() << "unknown texture compression '" + fourcc(ddsh.ddspf.fourCC) + "'";
        }
        compressed = true;
    } else if (ddsh.ddspf.rgbBitCount == 32 &&
               ddsh.ddspf.rBitMask == 0x00FF0000 &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x000000FF &&
               ddsh.ddspf.aBitMask == 0xFF000000) {
        colorFormat.uncompressed = ColorFormat::BGRA;
    } else if (ddsh.ddspf.rgbBitCount == 32 &&
               ddsh.ddspf.rBitMask == 0x000000FF &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x00FF0000 &&
               ddsh.ddspf.aBitMask == 0xFF000000) {
        colorFormat.uncompressed = ColorFormat::RGBA;
    } else if (ddsh.ddspf.rgbBitCount == 24 &&
               ddsh.ddspf.rBitMask == 0x000000FF &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x00FF0000) {
        colorFormat.uncompressed = ColorFormat::RGB;
        components = 3;
    } else if (ddsh.ddspf.rgbBitCount == 24 &&
               ddsh.ddspf.rBitMask == 0x00FF0000 &&
               ddsh.ddspf.gBitMask == 0x0000FF00 &&
               ddsh.ddspf.bBitMask == 0x000000FF) {
        colorFormat.uncompressed = ColorFormat::BGR;
        components = 3;
    } else if (ddsh.ddspf.rgbBitCount == 8) {
        #ifndef MAGNUM_TARGET_GLES2
        colorFormat.uncompressed = ColorFormat::Red;
        #else
        colorFormat.uncompressed = ColorFormat::Luminance;
        #endif
    } else {
        Error() << "Unknow texture format";
    }

    const UnsignedInt width = ddsh.width;
    const UnsignedInt height = ddsh.height;
    const UnsignedInt depth = Math::max(ddsh.depth, 1u);   // set to 1 if 0

    const UnsignedInt numMipmaps = (ddsh.flags & DdsDescriptionFlag::MipMapCount)
                    ? ((ddsh.mipMapCount == 0) ? 0 : ddsh.mipMapCount - 1)
                    : 0;

    /* load all surfaces for the image (6 surfaces for cubemaps) */
    const UnsignedInt numImages = (isCubemap) ? 6 : 1;
    for (UnsignedInt n = 0; n < numImages; ++n) {
        if (compressed) {
            loadCompressedImageData(colorFormat.compressed, width, height, depth);
        } else {
            loadUncompressedImageData(colorFormat.uncompressed, width, height, depth, components);
        }

        UnsignedInt w = width;
        UnsignedInt h = height;
        UnsignedInt d = depth;

        /* load all mipmaps for current surface */
        for (UnsignedInt i = 0; i < numMipmaps && (w || h); i++) {
            /* shrink to next power of 2 */
            w = Math::max(w >> 1, 1u);
            h = Math::max(h >> 1, 1u);
            d = Math::max(d >> 1, 1u);

            if (compressed) {
                loadCompressedImageData(colorFormat.compressed, w, h, d);
            } else {
                loadUncompressedImageData(colorFormat.uncompressed, w, h, d, components);
            }
        }
    }

}

void DdsImporter::loadUncompressedImageData(const ColorFormat format, const UnsignedInt width, const UnsignedInt height, const UnsignedInt depth, const UnsignedInt components) {
    const unsigned int numPixels = width * height * depth;
    const unsigned int size = numPixels * components;

    /* load image data */
    char *data = new char[size];
    _in->read(data, size);

    ColorFormat newColorFormat = format;
    if (format == ColorFormat::BGR) {
        Debug() << "Converting from BGR to RGB";
        convertBGRToRGB(data, numPixels);
        newColorFormat = ColorFormat::RGB;
    } else if (format == ColorFormat::BGRA) {
        Debug() << "Converting from BGRA to RGBA";
        convertBGRAToRGBA(data, numPixels);
        newColorFormat = ColorFormat::RGBA;
    }
    _imageData2D.emplace_back(newColorFormat, ColorType::UnsignedByte, Vector2i{Int(width), Int(height)}, std::move(static_cast<void*>(data)));
}

void DdsImporter::loadCompressedImageData(const CompressedColorFormat format, const UnsignedInt width, const UnsignedInt height, const UnsignedInt depth) {
    const unsigned int size = (depth*((width + 3)/4)*(((height + 3)/4))*((format == CompressedColorFormat::RGBAS3tcDxt1) ? 8 : 16));

    /* load image data */
    char *data = new char[size];
    _in->read(data, size);

    _imageData2D.emplace_back(format, Vector2i{Int(width), Int(height)}, Containers::Array<char>(data, size));
}

void DdsImporter::doOpenFile(const std::string& filename) {
    /* let doOpenData handle it */
    AbstractImporter::doOpenFile(filename);
}

UnsignedInt DdsImporter::doImage2DCount() const { return 1; }

std::optional<ImageData2D> DdsImporter::doImage2D(UnsignedInt id) {
    if (id < _imageData2D.size()) {
        return std::move(_imageData2D[id]);
    }

    return std::optional<ImageData2D>();
}

}}
