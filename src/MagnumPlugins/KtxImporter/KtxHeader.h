#ifndef Magnum_Trade_KtxHeader_h
#define Magnum_Trade_KtxHeader_h
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

#include <Magnum/Magnum.h>

/* Used by both KtxImporter and KtxImageConverter, which is why it isn't
   directly inside KtxImporter.cpp. OTOH it doesn't need to be exposed
   publicly, which is why it has no docblocks. */

namespace Magnum { namespace Trade { namespace Implementation {

/* KTX2 file header */
struct KtxHeader {
    char         identifier[12]; /* File identifier */
    UnsignedInt  vkFormat;       /* VkFormat enum value, VK_FORMAT_UNDEFINED = custom */
    UnsignedInt  typeSize;       /* Size of data type in bytes */
    Vector3ui    pixelSize;      /* Image level 0 size */
    UnsignedInt  layerCount;     /* Number of array elements */
    UnsignedInt  faceCount;      /* Number of cubemap faces */
    UnsignedInt  levelCount;     /* Number of mip levels */
    UnsignedInt  supercompressionScheme; 
    /* Index */
    UnsignedInt  dfdByteOffset;  /* Offset of Data Format Descriptor */
    UnsignedInt  dfdByteLength;  /* Length of Data Format Descriptor */
    UnsignedInt  kvdByteOffset;  /* Offset of Key/Value Data */
    UnsignedInt  kvdByteLength;  /* Length of Key/Value Data */
    UnsignedLong sgdByteOffset;  /* Offset of Supercompression Global Data */
    UnsignedLong sgdByteLength;  /* Length of Supercompression Global Data */
};

static_assert(sizeof(KtxHeader) == 80, "Improper size of KtxHeader struct");

/* KTX2 mip level index element */
struct KtxLevel {
    UnsignedLong byteOffset;             /* Offset of first byte of image data */
    UnsignedLong byteLength;             /* Total size of image data */
    UnsignedLong uncompressedByteLength; /* Total size of image data before supercompression */
};

static_assert(sizeof(KtxLevel) == 24, "Improper size of KtxLevel struct");

constexpr char KtxFileIdentifier[12]{
    /* https://github.khronos.org/KTX-Specification/#_identifier */
    '\xab', 'K', 'T', 'X', ' ', '2', '0', '\xbb', '\r', '\n', '\x1a', '\n'
};

static_assert(sizeof(KtxFileIdentifier) == sizeof(KtxHeader::identifier), "Improper size of KtxFileIdentifier data");

constexpr std::size_t KtxFileVersionOffset = 5;
static_assert(KtxFileVersionOffset < sizeof(KtxFileIdentifier), "KtxFileVersionOffset out of bounds");

}}}

#endif
