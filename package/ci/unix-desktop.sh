#!/bin/bash
set -ev

# Corrade
git clone --depth 1 https://github.com/mosra/corrade.git
cd corrade
mkdir build && cd build
cmake .. \
    -DCMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS" \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCMAKE_INSTALL_RPATH=$HOME/deps/lib \
    -DCMAKE_BUILD_TYPE=$CONFIGURATION \
    -DCORRADE_WITH_INTERCONNECT=OFF \
    -DCORRADE_BUILD_DEPRECATED=$BUILD_DEPRECATED \
    -DCORRADE_BUILD_STATIC=$BUILD_STATIC \
    -G Ninja
ninja install
cd ../..

# Magnum
git clone --depth 1 https://github.com/mosra/magnum.git
cd magnum
mkdir build && cd build
cmake .. \
    -DCMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS" \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCMAKE_INSTALL_RPATH=$HOME/deps/lib \
    -DCMAKE_BUILD_TYPE=$CONFIGURATION \
    -DMAGNUM_WITH_AUDIO=ON \
    -DMAGNUM_WITH_DEBUGTOOLS=ON \
    -DMAGNUM_WITH_GL=OFF \
    -DMAGNUM_WITH_MATERIALTOOLS=ON \
    -DMAGNUM_WITH_MESHTOOLS=ON \
    -DMAGNUM_WITH_PRIMITIVES=ON \
    -DMAGNUM_WITH_SCENEGRAPH=OFF \
    -DMAGNUM_WITH_SCENETOOLS=OFF \
    -DMAGNUM_WITH_SHADERS=OFF \
    -DMAGNUM_WITH_TEXT=ON \
    -DMAGNUM_WITH_TEXTURETOOLS=ON \
    -DMAGNUM_WITH_ANYIMAGEIMPORTER=ON \
    -DMAGNUM_WITH_TGAIMAGECONVERTER=ON \
    -DMAGNUM_BUILD_DEPRECATED=$BUILD_DEPRECATED \
    -DMAGNUM_BUILD_STATIC=$BUILD_STATIC \
    -DMAGNUM_BUILD_PLUGINS_STATIC=$BUILD_STATIC \
    -G Ninja
ninja install
cd ../..

# Build. MAGNUM_BUILD_GL_TESTS is enabled just to be sure, it should not be
# needed by any plugin.
mkdir build && cd build
cmake .. \
    -DCMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS" \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCMAKE_INSTALL_RPATH=$HOME/deps/lib \
    -DCMAKE_BUILD_TYPE=$CONFIGURATION \
    -DCMAKE_DISABLE_FIND_PACKAGE_OpenCL=${DISABLE_OPENCL:-OFF} \
    -DMAGNUM_WITH_ASSIMPIMPORTER=ON \
    -DMAGNUM_WITH_ASTCIMPORTER=ON \
    -DMAGNUM_WITH_BASISIMAGECONVERTER=ON \
    -DMAGNUM_WITH_BASISIMPORTER=ON -DBASIS_UNIVERSAL_DIR=$HOME/basis_universal \
    -DMAGNUM_WITH_BCDECIMAGECONVERTER=ON \
    -DMAGNUM_WITH_CGLTFIMPORTER=$BUILD_DEPRECATED \
    -DMAGNUM_WITH_DDSIMPORTER=ON \
    -DMAGNUM_WITH_DEVILIMAGEIMPORTER=ON \
    -DMAGNUM_WITH_DRFLACAUDIOIMPORTER=ON \
    -DMAGNUM_WITH_DRMP3AUDIOIMPORTER=ON \
    -DMAGNUM_WITH_DRWAVAUDIOIMPORTER=ON \
    -DMAGNUM_WITH_ETCDECIMAGECONVERTER=ON \
    $FREETYPE_INCLUDE_DIR_freetype2 \
    -DMAGNUM_WITH_FAAD2AUDIOIMPORTER=ON \
    -DMAGNUM_WITH_FREETYPEFONT=ON \
    -DMAGNUM_WITH_GLSLANGSHADERCONVERTER=ON \
    -DMAGNUM_WITH_GLTFIMPORTER=ON \
    -DMAGNUM_WITH_GLTFSCENECONVERTER=ON \
    -DMAGNUM_WITH_HARFBUZZFONT=ON \
    -DMAGNUM_WITH_ICOIMPORTER=ON \
    -DMAGNUM_WITH_JPEGIMAGECONVERTER=ON \
    -DMAGNUM_WITH_JPEGIMPORTER=ON \
    -DMAGNUM_WITH_KTXIMAGECONVERTER=ON \
    -DMAGNUM_WITH_KTXIMPORTER=ON \
    -DMAGNUM_WITH_LUNASVGIMPORTER=$WITH_LUNASVGIMPORTER \
    -DMAGNUM_WITH_MESHOPTIMIZERSCENECONVERTER=ON \
    -DMAGNUM_WITH_MINIEXRIMAGECONVERTER=ON \
    -DMAGNUM_WITH_OPENEXRIMAGECONVERTER=ON \
    -DMAGNUM_WITH_OPENEXRIMPORTER=ON \
    -DMAGNUM_WITH_OPENGEXIMPORTER=ON \
    `# Using the same option as for LunaSVG as the library is by the same` \
    `# author with the same requirements.` \
    -DMAGNUM_WITH_PLUTOSVGIMPORTER=$WITH_LUNASVGIMPORTER \
    -DMAGNUM_WITH_PNGIMAGECONVERTER=ON \
    -DMAGNUM_WITH_PNGIMPORTER=ON \
    -DMAGNUM_WITH_PRIMITIVEIMPORTER=ON \
    `# Ubuntu 22.04+ has it in repos, but not the C API. Homebrew has it,` \
    `# but only after update and then it wants to build llvm from scratch.` \
    -DMAGNUM_WITH_RESVGIMPORTER=OFF \
    -DMAGNUM_WITH_SPIRVTOOLSSHADERCONVERTER=ON \
    -DMAGNUM_WITH_SPNGIMPORTER=ON \
    -DMAGNUM_WITH_STANFORDIMPORTER=ON \
    -DMAGNUM_WITH_STANFORDSCENECONVERTER=ON \
    -DMAGNUM_WITH_STBDXTIMAGECONVERTER=ON \
    -DMAGNUM_WITH_STBIMAGECONVERTER=ON \
    -DMAGNUM_WITH_STBIMAGEIMPORTER=ON \
    -DMAGNUM_WITH_STBRESIZEIMAGECONVERTER=ON \
    -DMAGNUM_WITH_STBTRUETYPEFONT=ON \
    -DMAGNUM_WITH_STBVORBISAUDIOIMPORTER=ON \
    -DMAGNUM_WITH_STLIMPORTER=ON \
    -DMAGNUM_WITH_TINYGLTFIMPORTER=$BUILD_DEPRECATED \
    -DMAGNUM_WITH_UFBXIMPORTER=ON \
    -DMAGNUM_WITH_WEBPIMAGECONVERTER=ON \
    -DMAGNUM_WITH_WEBPIMPORTER=ON \
    -DMAGNUM_BUILD_TESTS=ON \
    -DMAGNUM_BUILD_GL_TESTS=ON \
    -DMAGNUM_BUILD_STATIC=$BUILD_STATIC \
    -DMAGNUM_BUILD_PLUGINS_STATIC=$BUILD_STATIC \
    -G Ninja
ninja $NINJA_JOBS

# UfbxImporter likely triggers https://github.com/google/sanitizers/issues/1322
# since https://github.com/mosra/magnum-plugins/pull/136 when running the
# imageEmbedded() test case. It manifests as
#
#   Tracer caught signal 11: addr=0x0 pc=0x512718 sp=0x7fc42b3d3d10
#   ==8082==LeakSanitizer has encountered a fatal error.
#
# only when running through CTest from CMake 3.4, not directly, and
# ASAN_OPTIONS=intercept_tls_get_addr=0 (as suggested in the issue above) seems
# to fix it. May want to revisit once https://reviews.llvm.org/D147459 lands in
# a public release and that public release becomes Magnum's "min spec" target.
# So around 2030 I'd say.
#
# Same now happens for BasisImporter as of
# https://github.com/mosra/corrade/commit/157a43bb0084cd89ce020b1f871e8cd20856cc80
# or possibly some of the earlier cleanup commits, the smallest repro case is
# with passing `--only -14` to it (via corrade_add_test() ARGUMENTS). Not any
# less cases, and the order has to be exactly this (i.e., `--shuffle` makes it
# crash only if `unconfigured()` is the last case). LSAN_OPTIONS=use_tls=0
# avoids the crash, but then the damn thing reports leaks in `ld-linux.so` any
# time a plugin gets loaded.
ASAN_OPTIONS="color=always" LSAN_OPTIONS="color=always" TSAN_OPTIONS="color=always" CORRADE_TEST_COLOR=ON ctest -V -E "BasisImporter|UfbxImporter"
ASAN_OPTIONS="color=always intercept_tls_get_addr=0" LSAN_OPTIONS="color=always" TSAN_OPTIONS="color=always" CORRADE_TEST_COLOR=ON ctest -V -R "BasisImporter|UfbxImporter"

# Test install, after running the tests as for them it shouldn't be needed
ninja install
