#ifndef Magnum_Trade_KtxHeader_h
#define Magnum_Trade_KtxHeader_h
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

#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector3.h>

/* Used by both KtxImporter and KtxImageConverter, which is why it isn't
   directly inside KtxImporter.cpp. OTOH it doesn't need to be exposed
   publicly, which is why it has no docblocks. */

namespace Magnum { namespace Trade { namespace Implementation {

typedef UnsignedInt VkFormat;

/* Selected Vulkan 1.0 formats for detecting implicit swizzling to PixelFormat.
   VkFormat is UnsignedInt instead of this enum to prevent warnings when using
   arbitrary numeric values from formatMapping.hpp in switches. */
enum: UnsignedInt {
    VK_FORMAT_UNDEFINED = 0,
    VK_FORMAT_B8G8R8_UNORM = 30,
    VK_FORMAT_B8G8R8_SNORM = 31,
    VK_FORMAT_B8G8R8_UINT = 34,
    VK_FORMAT_B8G8R8_SINT = 35,
    VK_FORMAT_B8G8R8_SRGB = 36,
    VK_FORMAT_B8G8R8A8_UNORM = 44,
    VK_FORMAT_B8G8R8A8_SNORM = 45,
    VK_FORMAT_B8G8R8A8_UINT = 48,
    VK_FORMAT_B8G8R8A8_SINT = 49,
    VK_FORMAT_B8G8R8A8_SRGB = 50
};

enum VkFormatSuffix: UnsignedByte {
    UNORM = 1,
    SNORM,
    UINT,
    SINT,
    UFLOAT,
    SFLOAT,
    SRGB
    /* SCALED formats are not allowed by KTX, and not exposed by Magnum, either.
       They're usually used as vertex formats. */
};

enum SuperCompressionScheme: UnsignedInt {
    None = 0,
    BasisLZ = 1,
    Zstandard = 2,
    ZLIB = 3
};

/* KTX2 file header */
struct KtxHeader {
    char                   identifier[12];         /* File identifier */
    VkFormat               vkFormat;               /* Texel format, VK_FORMAT_UNDEFINED = custom */
    UnsignedInt            typeSize;               /* Size of channel data type, in bytes */
    Vector3ui              imageSize;              /* Image level 0 size */
    UnsignedInt            layerCount;             /* Number of array elements */
    UnsignedInt            faceCount;              /* Number of cubemap faces */
    UnsignedInt            levelCount;             /* Number of mip levels */
    SuperCompressionScheme supercompressionScheme;
    /* Index */
    UnsignedInt            dfdByteOffset;          /* Offset of Data Format Descriptor */
    UnsignedInt            dfdByteLength;          /* Length of Data Format Descriptor */
    UnsignedInt            kvdByteOffset;          /* Offset of Key/Value Data */
    UnsignedInt            kvdByteLength;          /* Length of Key/Value Data */
    UnsignedLong           sgdByteOffset;          /* Offset of Supercompression Global Data */
    UnsignedLong           sgdByteLength;          /* Length of Supercompression Global Data */
};

static_assert(sizeof(KtxHeader) == 80,
    "Improper size of KtxHeader struct");

/* KTX2 mip level index element */
struct KtxLevel {
    UnsignedLong byteOffset;             /* Offset of first byte of image data */
    UnsignedLong byteLength;             /* Total size of image data */
    UnsignedLong uncompressedByteLength; /* Total size of image data before supercompression */
};

static_assert(sizeof(KtxLevel) == 24,
    "Improper size of KtxLevel struct");

constexpr char KtxFileIdentifier[12]{
    /* https://github.khronos.org/KTX-Specification/#_identifier */
    '\xab', 'K', 'T', 'X', ' ', '2', '0', '\xbb', '\r', '\n', '\x1a', '\n'
};

static_assert(sizeof(KtxFileIdentifier) == sizeof(KtxHeader::identifier),
    "Improper size of KtxFileIdentifier data");

constexpr std::size_t KtxFileVersionOffset = 5;
constexpr std::size_t KtxFileVersionLength = 2;
static_assert(KtxFileVersionOffset + KtxFileVersionLength <= sizeof(KtxFileIdentifier),
    "KtxFileVersion(Offset|Length) out of bounds");

/* Khronos Data Format: basic block header */
struct KdfBasicBlockHeader {
    enum class VendorId: UnsignedShort {
        Khronos = 0
    };

    enum class DescriptorType: UnsignedShort {
        Basic = 0
    };

    enum class VersionNumber: UnsignedShort {
        Kdf1_3 = 2
    };

    enum class ColorModel: UnsignedByte {
        /* Uncompressed formats. There are a lot more, but KTX doesn't allow
           those. */
        Rgbsda = 1,   /* Additive colors: red, green, blue, stencil, depth, alpha */

        /* Compressed formats, each one has its own color model */
        Bc1 = 128,    /* DXT1 */
        Bc2 = 129,    /* DXT2/3 */
        Bc3 = 130,    /* DXT4/5 */
        Bc4 = 131,
        Bc5 = 132,
        Bc6h = 133,
        Bc7 = 134,
        Etc1 = 160,
        Etc2 = 161,
        Astc = 162,
        Etc1s = 163,
        Pvrtc = 164,
        Pvrtc2 = 165,

        /* Basis Universal */
        BasisUastc = 166,
        BasisEtc1s = Etc1s
    };

    enum class ColorPrimaries: UnsignedByte {
        /* We have no way to guess color space, this is the recommended default */
        Srgb = 1 /* BT.709 */
    };

    enum class TransferFunction: UnsignedByte {
        /* There are a lot more, but KTX doesn't allow those */
        Linear = 1,
        Srgb = 2
    };

    enum Flags: UnsignedByte {
        AlphaPremultiplied = 1
    };

    /* Technically, the first two members are 17 and 15 bits, but bit fields
       aren't very portable. We only check for values 0/0 so this works for our
       use case. */
    VendorId vendorId;
    DescriptorType descriptorType;
    VersionNumber versionNumber;
    UnsignedShort descriptorBlockSize;

    ColorModel colorModel;
    ColorPrimaries colorPrimaries;
    TransferFunction transferFunction;
    Flags flags;
    UnsignedByte texelBlockDimension[4];
    UnsignedByte bytesPlane[8];
};

static_assert(sizeof(KdfBasicBlockHeader) == 24, "Improper size of KdfBasicBlockHeader struct");

/* Khronos Data Format: Basic block sample element, one for each color channel */
struct KdfBasicBlockSample {
    /* Channel id encoded in lower half of channelType */
    enum ChannelId: UnsignedByte {
        /* ColorModel::Rgbsda */
        Red = 0,
        Green = 1,
        Blue = 2,
        Stencil = 13,
        Depth = 14,
        Alpha = 15,
        /* Compressed color models. Some use Red/Green/Alpha from Rgbsda if
           applicable. */
        Color = 0,
        Bc1Alpha = 1,
        Etc2Color = 2
    };

    /* Channel data type bit mask encoded in upper half of channelType */
    enum ChannelFormat: UnsignedByte {
        Linear   = 1 << (4 + 0), /* Ignore the transfer function */
        Exponent = 1 << (4 + 1),
        Signed   = 1 << (4 + 2),
        Float    = 1 << (4 + 3)
    };

    UnsignedShort bitOffset;
    UnsignedByte bitLength;   /* Length - 1 */
    UnsignedByte channelType;
    UnsignedByte position[4];
    UnsignedInt lower;
    UnsignedInt upper;
};

static_assert(sizeof(KdfBasicBlockSample) == 16, "Improper size of KdfBasicBlockSample struct");

}}}

#endif
