/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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

#if CHECK_VERSION >= 20190915
#include <assimp/MathFunctions.h>
#endif

#include <assimp/matrix4x4.h>
#include <assimp/scene.h>
#include <assimp/quaternion.h>
#include <assimp/version.h>

#ifndef CHECK_VERSION
#error CHECK_VERSION not defined
#define CHECK_VERSION 0xffffffff
#endif

int main() {
    int ret = 0;

    /* Version that breaks aiAnimation::mTicksPerSecond for FBX:
       https://github.com/assimp/assimp/commit/b3e1ee3ca0d825d384044867fc30cd0bc8417be6
       Check for aiQuaternion::operation *= added in
       https://github.com/assimp/assimp/commit/89d4d6b68f720aaf545dba9d6a701426b948df15 */
    #if CHECK_VERSION >= 20210102
    aiQuaternion quat;
    quat *= aiMatrix4x4();
    #endif

    /* First version that correctly parses glTF2 spline-interpolated animation data:
       https://github.com/assimp/assimp/commit/e3083c21f0a7beae6c37a2265b7919a02cbf83c4.
       Check for Scene::mName added in
       https://github.com/assimp/assimp/commit/afd69bea8a6a870a986b5c8ad1a07bf127b0eaa0 */
    #if CHECK_VERSION >= 20201123
    aiScene scene;
    scene.mName = "";
    #endif

    /* Support for orthographic camera width.
       https://github.com/assimp/assimp/commit/ae50c4ebdf23c7f6f61300dede5bf32e0d306eb2 */
    #if CHECK_VERSION >= 20200225
    aiCamera camera;
    camera.mOrthographicWidth = 1.0f;
    #endif

    /* Support for patch version information.
       https://github.com/assimp/assimp/commit/5cfb0fd633372bbbec87f08015139d71d330d4a6 */
    #if CHECK_VERSION >= 20191122
    ret = static_cast<int>(aiGetVersionPatch());
    #endif

    /* Assimp 5. Of all the things that could break, this version reports itself as
       4.1. Since some of the insane awful bugs got fixed in version 5, the test has
       to check against the version in order to adjust expectations. The only way I
       could make this work is checking for the getEpsilon() function added in
       https://github.com/assimp/assimp/commit/8b95479bb00b4bf8fb875f2c5b0605ddfd203b7f
       Related bug: https://github.com/assimp/assimp/issues/2693 */
    #if CHECK_VERSION >= 20190915
    ret = static_cast<int>(Assimp::Math::getEpsilon<float>());
    #endif

   /* Support for double types (ai_real, aiPTI_Double):
      https://github.com/assimp/assimp/commit/fa1d6d8c55484a1ab97b2773585ae76f71ef6fbc */
    #if CHECK_VERSION >= 20160716
    ai_real real{};
    #endif

    unsigned int version = aiGetVersionMajor()*100 + aiGetVersionMinor();
    ret = static_cast<int>(version);

    return ret;
}
