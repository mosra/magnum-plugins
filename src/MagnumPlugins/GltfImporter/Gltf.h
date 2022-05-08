#ifndef Magnum_Trade_Gltf_h
#define Magnum_Trade_Gltf_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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

#include <Magnum/Types.h>

namespace Magnum { namespace Trade { namespace Implementation {

/* Binary glTF header layouts */

/* Checking glTF header and the JSON chunk header together, as there's no other
   layout possible -- 4.4.3.2 (Structured JSON Content) says "This chunk MUST
   be the very first chunk of a Binary glTF asset" */
struct GltfGlbChunkHeader {
    UnsignedInt length;         /* Chunk length */
    /** @todo drop the union once magic is printed with Debug::str */
    union {
        char magic[4];          /* ASCII "JSON" or "BIN\0", extensions may add
                                   new chunks */
        UnsignedInt id;
    };
};
struct GltfGlbHeader {
    char magic[4];              /* ASCII "glTF" */
    UnsignedInt version;        /* GLB version */
    UnsignedInt length;         /* Total file length */
    GltfGlbChunkHeader json;    /* JSON chunk */
};

/* glTF numeric constants as listed in the spec. They correspond to GL defines,
   but because the plugin should be usable even in contexts where GL headers
   are not available, it doesn't reuse the definitions. */

enum: UnsignedInt {
    /* Accessor component type */
    GltfTypeByte = 5120,            /* GL_BYTE */
    GltfTypeUnsignedByte = 5121,    /* GL_UNSIGNED_BYTE */
    GltfTypeShort = 5122,           /* GL_SHORT */
    GltfTypeUnsignedShort = 5123,   /* GL_UNSIGNED_SHORT */
                                    /* GL_INT (5124) unused */
    GltfTypeUnsignedInt = 5125,     /* GL_UNSIGNED_INT */
    GltfTypeFloat = 5126,           /* GL_FLOAT */

    /* Mesh primitive mode */
    GltfModePoints = 0,             /* GL_POINTS */
    GltfModeLines = 1,              /* GL_LINES */
    GltfModeLineLoop = 2,           /* GL_LINE_LOOP */
    GltfModeLineStrip = 3,          /* GL_LINE_STRIP */
    GltfModeTriangles = 4,          /* GL_TRIANGLES */
    GltfModeTriangleStrip = 5,      /* GL_TRIANGLE_STRIP */
    GltfModeTriangleFan = 6,        /* GL_TRIANGLE_FAN */

    /* Sampler filters */
    GltfFilterNearest = 9728,               /* GL_NEAREST */
    GltfFilterLinear = 9729,                /* GL_LINEAR */
    GltfFilterNearestMipmapNearest = 9984,  /* GL_NEAREST_MIPMAP_NEAREST */
    GltfFilterNearestMipmapLinear = 9985,   /* GL_NEAREST_MIPMAP_LINEAR */
    GltfFilterLinearMipmapNearest = 9986,   /* GL_LINEAR_MIPMAP_NEAREST */
    GltfFilterLinearMipmapLinear = 9987,    /* GL_LINEAR_MIPMAP_LINEAR */

    /* Wrapping */
    GltfWrappingClampToEdge = 33071,        /* GL_CLAMP_TO_EDGE */
    GltfWrappingMirroredRepeat = 33648,     /* GL_MIRRORED_REPEAT */
    GltfWrappingRepeat = 10497,             /* GL_REPEAT */
};

}}}

#endif
