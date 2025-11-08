#!/bin/bash
set -ev

git submodule update --init

# Crosscompile Corrade
git clone --depth 1 https://github.com/mosra/corrade.git
cd corrade
mkdir build-emscripten && cd build-emscripten
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="../../toolchains/generic/Emscripten-wasm.cmake" \
    `# Building as Debug always, as Release optimizations take a long time` \
    `# and make no sense on the CI. Thus, benchmark output will not be` \
    `# really meaningful, but we still want to run them to catch issues.` \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCORRADE_WITH_INTERCONNECT=OFF \
    $EXTRA_OPTS \
    -G Ninja
ninja install
cd ../..

# Crosscompile Magnum
git clone --depth 1 https://github.com/mosra/magnum.git
cd magnum
mkdir build-emscripten && cd build-emscripten
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="../../toolchains/generic/Emscripten-wasm.cmake" \
    `# Building as Debug always, as Release optimizations take a long time` \
    `# and make no sense on the CI. Thus, benchmark output will not be` \
    `# really meaningful, but we still want to run them to catch issues.` \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCMAKE_FIND_ROOT_PATH=$HOME/deps \
    -DMAGNUM_WITH_AUDIO=ON \
    -DMAGNUM_WITH_DEBUGTOOLS=ON \
    -DMAGNUM_WITH_GL=OFF \
    -DMAGNUM_WITH_MATERIALTOOLS=ON \
    -DMAGNUM_WITH_MESHTOOLS=ON \
    -DMAGNUM_WITH_PRIMITIVES=ON \
    -DMAGNUM_WITH_SCENEGRAPH=OFF \
    -DMAGNUM_WITH_SCENETOOLS=OFF \
    -DMAGNUM_WITH_SHADERS=OFF \
    -DMAGNUM_WITH_SHADERTOOLS=OFF \
    -DMAGNUM_WITH_TEXT=ON \
    -DMAGNUM_WITH_TEXTURETOOLS=ON \
    -DMAGNUM_WITH_ANYIMAGEIMPORTER=ON \
    -DMAGNUM_WITH_TGAIMAGECONVERTER=ON \
    $EXTRA_OPTS \
    -G Ninja
ninja install
cd ../..

# Crosscompile FAAD2. Basically a copy of the emscripten-faad2 PKGBUILD from
# https://github.com/mosra/archlinux.
# As of 2021-09-30, CircleCI fails with an "expired certificate" error, so we
# explicitly disable the certificate check.
wget --no-check-certificate https://downloads.sourceforge.net/sourceforge/faac/faad2-2.8.8.tar.gz
tar -xzvf faad2-2.8.8.tar.gz
cd faad2-2.8.8
emconfigure ./configure --prefix=$HOME/deps
emmake make install
mv $HOME/deps/lib/{libfaad.a,faad.bc}
mv $HOME/deps/lib/{libfaad_drm.a,faad_drm.bc}
cd ..

# Crosscompile zstd. Version 1.5.1+ doesn't compile with this Emscripten
# version, saying that
#   huf_decompress_amd64.S: Input file has an unknown suffix, don't know what to do with it!
# Newer Emscriptens work fine, 1.5.0 doesn't have this file yet so it works.
export ZSTD_VERSION=1.5.0
wget --no-check-certificate https://github.com/facebook/zstd/archive/refs/tags/v$ZSTD_VERSION.tar.gz
tar -xzf v$ZSTD_VERSION.tar.gz
cd zstd-$ZSTD_VERSION
# There's already a directory named `build`
mkdir build_ && cd build_
cmake ../build/cmake \
    -DCMAKE_TOOLCHAIN_FILE="../../toolchains/generic/Emscripten-wasm.cmake" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DZSTD_BUILD_PROGRAMS=OFF \
    -DZSTD_BUILD_SHARED=OFF \
    -DZSTD_BUILD_STATIC=ON \
    -DZSTD_MULTITHREAD_SUPPORT=OFF \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    $EXTRA_OPTS \
    -G Ninja
ninja install
cd ../..

# Crosscompile OpenEXR
export OPENEXR_VERSION=3.2.0
wget --no-check-certificate https://github.com/AcademySoftwareFoundation/openexr/archive/v$OPENEXR_VERSION/openexr-$OPENEXR_VERSION.tar.gz
tar -xzf openexr-$OPENEXR_VERSION.tar.gz
cd openexr-$OPENEXR_VERSION
mkdir build && cd build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="../../toolchains/generic/Emscripten-wasm.cmake" \
    -DCMAKE_CXX_FLAGS="-s DISABLE_EXCEPTION_CATCHING=0" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DOPENEXR_BUILD_TOOLS=OFF \
    -DOPENEXR_ENABLE_THREADING=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DOPENEXR_INSTALL_EXAMPLES=OFF \
    -DOPENEXR_INSTALL_TOOLS=OFF \
    -DOPENEXR_INSTALL_PKG_CONFIG=OFF \
    -DOPENEXR_FORCE_INTERNAL_IMATH=ON \
    -DOPENEXR_FORCE_INTERNAL_DEFLATE=ON \
    `# v1.18 (which is the default) has different output and the test files` \
    `# are made against v1.19 now` \
    -DOPENEXR_DEFLATE_TAG=v1.19 \
    -DIMATH_INSTALL_PKG_CONFIG=OFF \
    -DIMATH_HALF_USE_LOOKUP_TABLE=OFF \
    $EXTRA_OPTS \
    -G Ninja
ninja install
cd ../..

# Explicitly build Emscripten Ports. Doing that implicitly below likely causes
# the same thing to start building twice (such as libjpeg for both JpegImporter
# and JpegImageConverter), and then it OOMs and explodes.
if [[ "$USE_EMSCRIPTEN_PORTS" == "ON" ]]; then
    embuilder.py build freetype
    embuilder.py build harfbuzz
    embuilder.py build libpng
    embuilder.py build libjpeg
fi

# Crosscompile. MAGNUM_BUILD_GL_TESTS is enabled just to be sure, it should not
# be needed by any plugin.
# MAGNUM_WITH_BASISIMAGECONVERTER is disabled as it requires pthreads.
mkdir build-emscripten && cd build-emscripten
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="../toolchains/generic/Emscripten-wasm.cmake" \
    `# Building as Debug always, as Release optimizations take a long time` \
    `# and make no sense on the CI. Thus, benchmark output will not be` \
    `# really meaningful, but we still want to run them to catch issues.` \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCMAKE_FIND_ROOT_PATH=$HOME/deps \
    -DMAGNUM_USE_EMSCRIPTEN_PORTS_FREETYPE=$USE_EMSCRIPTEN_PORTS \
    -DMAGNUM_USE_EMSCRIPTEN_PORTS_HARFBUZZ=$USE_EMSCRIPTEN_PORTS \
    -DMAGNUM_USE_EMSCRIPTEN_PORTS_LIBJPEG=$USE_EMSCRIPTEN_PORTS \
    -DMAGNUM_USE_EMSCRIPTEN_PORTS_LIBPNG=$USE_EMSCRIPTEN_PORTS \
    -DMAGNUM_WITH_ASSIMPIMPORTER=OFF \
    -DMAGNUM_WITH_ASTCIMPORTER=ON \
    -DMAGNUM_WITH_BASISIMAGECONVERTER=OFF \
    -DMAGNUM_WITH_BASISIMPORTER=ON -DBASIS_UNIVERSAL_DIR=$HOME/basis_universal \
    -DMAGNUM_WITH_BCDECIMAGECONVERTER=ON \
    -DMAGNUM_WITH_CGLTFIMPORTER=ON \
    -DMAGNUM_WITH_DDSIMPORTER=ON \
    -DMAGNUM_WITH_DEVILIMAGEIMPORTER=OFF \
    -DMAGNUM_WITH_DRFLACAUDIOIMPORTER=ON \
    -DMAGNUM_WITH_DRMP3AUDIOIMPORTER=ON \
    -DMAGNUM_WITH_DRWAVAUDIOIMPORTER=ON \
    -DMAGNUM_WITH_ETCDECIMAGECONVERTER=ON \
    -DMAGNUM_WITH_FAAD2AUDIOIMPORTER=ON \
    -DMAGNUM_WITH_FREETYPEFONT=$USE_EMSCRIPTEN_PORTS \
    -DMAGNUM_WITH_GLSLANGSHADERCONVERTER=OFF \
    -DMAGNUM_WITH_GLTFIMPORTER=ON \
    -DMAGNUM_WITH_GLTFSCENECONVERTER=ON \
    -DMAGNUM_WITH_HARFBUZZFONT=$USE_EMSCRIPTEN_PORTS \
    -DMAGNUM_WITH_ICOIMPORTER=ON \
    -DMAGNUM_WITH_JPEGIMAGECONVERTER=$USE_EMSCRIPTEN_PORTS \
    -DMAGNUM_WITH_JPEGIMPORTER=$USE_EMSCRIPTEN_PORTS \
    -DMAGNUM_WITH_KTXIMAGECONVERTER=ON \
    -DMAGNUM_WITH_KTXIMPORTER=ON \
    -DMAGNUM_WITH_LUNASVGIMPORTER=OFF \
    -DMAGNUM_WITH_MESHOPTIMIZERSCENECONVERTER=OFF \
    -DMAGNUM_WITH_MINIEXRIMAGECONVERTER=ON \
    -DMAGNUM_WITH_OPENEXRIMAGECONVERTER=ON \
    -DMAGNUM_WITH_OPENEXRIMPORTER=ON \
    -DMAGNUM_WITH_OPENGEXIMPORTER=ON \
    -DMAGNUM_WITH_PNGIMAGECONVERTER=$USE_EMSCRIPTEN_PORTS \
    -DMAGNUM_WITH_PNGIMPORTER=$USE_EMSCRIPTEN_PORTS \
    -DMAGNUM_WITH_PRIMITIVEIMPORTER=ON \
    -DMAGNUM_WITH_RESVGIMPORTER=OFF \
    -DMAGNUM_WITH_SPIRVTOOLSSHADERCONVERTER=OFF \
    -DMAGNUM_WITH_SPNGIMPORTER=OFF \
    -DMAGNUM_WITH_STANFORDIMPORTER=ON \
    -DMAGNUM_WITH_STANFORDSCENECONVERTER=ON \
    -DMAGNUM_WITH_STBDXTIMAGECONVERTER=ON \
    -DMAGNUM_WITH_STBIMAGECONVERTER=ON \
    -DMAGNUM_WITH_STBIMAGEIMPORTER=ON \
    -DMAGNUM_WITH_STBRESIZEIMAGECONVERTER=ON \
    -DMAGNUM_WITH_STBTRUETYPEFONT=ON \
    -DMAGNUM_WITH_STBVORBISAUDIOIMPORTER=ON \
    -DMAGNUM_WITH_STLIMPORTER=ON \
    -DMAGNUM_WITH_TINYGLTFIMPORTER=ON \
    -DMAGNUM_WITH_UFBXIMPORTER=ON \
    -DMAGNUM_WITH_WEBPIMAGECONVERTER=OFF \
    -DMAGNUM_WITH_WEBPIMPORTER=OFF \
    -DMAGNUM_BUILD_TESTS=ON \
    -DMAGNUM_BUILD_GL_TESTS=ON \
    $EXTRA_OPTS \
    -G Ninja
ninja $NINJA_JOBS

# Test
CORRADE_TEST_COLOR=ON ctest -V

# Test install, after running the tests as for them it shouldn't be needed
ninja install
