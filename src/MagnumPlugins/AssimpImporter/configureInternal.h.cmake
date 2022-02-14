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

#define ASSIMP_VERSION ${ASSIMP_VERSION}
#define ASSIMP_HAS_DOUBLES (ASSIMP_VERSION>= 20160716)
#define ASSIMP_IS_VERSION_5_OR_GREATER (ASSIMP_VERSION >= 20190915)
#define ASSIMP_HAS_VERSION_PATCH (ASSIMP_VERSION >= 20191122)
#define ASSIMP_HAS_ORTHOGRAPHIC_CAMERA (ASSIMP_VERSION >= 20200225)
#define ASSIMP_HAS_SCENE_NAME (ASSIMP_VERSION >= 20201123)
#define ASSIMP_HAS_BROKEN_GLTF_SPLINES (ASSIMP_VERSION < 20201123)
