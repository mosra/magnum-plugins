#ifndef Magnum_Trade_Implementation_DataLayout_h
#define Magnum_Trade_Implementation_DataLayout_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>

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

#include <Corrade/Containers/EnumSet.h>
#include <Magnum/Trade/Trade.h>

namespace Magnum { namespace Trade { namespace Implementation {

struct DataChunkHeader32 {
    UnsignedByte version;
    char eolUnix[1];
    char eolDos[2];
    DataChunkSignature signature;
    UnsignedShort zero;
    UnsignedShort typeVersion;
    DataChunkType type;
    UnsignedInt size;
};

struct DataChunkHeader64 {
    UnsignedByte version;
    char eolUnix[1];
    char eolDos[2];
    DataChunkSignature signature;
    UnsignedShort zero;
    UnsignedShort typeVersion;
    DataChunkType type;
    UnsignedLong size;
};

struct MeshDataHeader32: DataChunkHeader32 {
    UnsignedInt indexCount;
    UnsignedInt vertexCount;
    MeshPrimitive primitive;
    MeshIndexType indexType;
    Byte:8;
    UnsignedShort attributeCount;
    UnsignedInt indexOffset;
    UnsignedInt indexDataSize;
    UnsignedInt vertexDataSize;
};

struct MeshDataHeader64: DataChunkHeader64 {
    UnsignedInt indexCount;
    UnsignedInt vertexCount;
    MeshPrimitive primitive;
    MeshIndexType indexType;
    Byte:8;
    UnsignedShort attributeCount;
    UnsignedLong indexOffset;
    UnsignedLong indexDataSize;
    UnsignedLong vertexDataSize;
};

struct MeshAttributeData32 {
    VertexFormat format;
    MeshAttribute name;
    bool isOffsetOnly;
    Byte:8;
    UnsignedInt vertexCount;
    Short stride;
    UnsignedShort arraySize;
    UnsignedInt offset;
};

struct MeshAttributeData64 {
    VertexFormat format;
    MeshAttribute name;
    bool isOffsetOnly;
    Byte:8;
    UnsignedInt vertexCount;
    Short stride;
    UnsignedShort arraySize;
    UnsignedLong offset;
};

static_assert(sizeof(DataChunkHeader32) == 20, "");
static_assert(sizeof(DataChunkHeader64) == 24, "");
static_assert(sizeof(MeshDataHeader32) == 48, "");
static_assert(sizeof(MeshDataHeader64) == 64, "");
static_assert(sizeof(MeshAttributeData32) == 20, "");
static_assert(sizeof(MeshAttributeData64) == 24, "");

}}}

#endif
