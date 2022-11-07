#!/bin/bash
set -ev

git submodule update --init

git clone --depth 1 https://github.com/mosra/corrade.git
cd corrade

# Build native corrade-rc
mkdir build && cd build || exit /b
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps-native \
    -DCORRADE_WITH_INTERCONNECT=OFF \
    -DCORRADE_WITH_PLUGINMANAGER=OFF \
    -DCORRADE_WITH_TESTSUITE=OFF \
    -DCORRADE_WITH_UTILITY=OFF \
    -G Ninja
ninja install
cd ..

# Crosscompile Corrade
mkdir build-emscripten && cd build-emscripten
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="../../toolchains/generic/Emscripten-wasm.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS_RELEASE="-DNDEBUG -O1" \
    -DCMAKE_EXE_LINKER_FLAGS_RELEASE="-O1" \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
    -DCORRADE_WITH_INTERCONNECT=OFF \
    -G Ninja
ninja install
cd ../..

# Crosscompile Magnum
git clone --depth 1 https://github.com/mosra/magnum.git
cd magnum
mkdir build-emscripten && cd build-emscripten
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="../../toolchains/generic/Emscripten-wasm.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS_RELEASE="-DNDEBUG -O1" \
    -DCMAKE_EXE_LINKER_FLAGS_RELEASE="-O1" \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCMAKE_FIND_ROOT_PATH=$HOME/deps \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
    -DMAGNUM_WITH_AUDIO=ON \
    -DMAGNUM_WITH_DEBUGTOOLS=ON \
    -DMAGNUM_WITH_GL=OFF \
    -DMAGNUM_WITH_MATERIALTOOLS=OFF \
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

# Crosscompile zstd
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
    -G Ninja
ninja install
cd ../..

# Crosscompile. MAGNUM_BUILD_GL_TESTS is enabled just to be sure, it should not
# be needed by any plugin.
# MAGNUM_WITH_BASISIMAGECONVERTER is disabled as it requires pthreads.
mkdir build-emscripten && cd build-emscripten
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="../toolchains/generic/Emscripten-wasm.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS_RELEASE="-DNDEBUG -O1" \
    -DCMAKE_EXE_LINKER_FLAGS_RELEASE="-O1" \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCMAKE_FIND_ROOT_PATH=$HOME/deps \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
    -DMAGNUM_WITH_ASSIMPIMPORTER=OFF \
    -DMAGNUM_WITH_ASTCIMPORTER=ON \
    -DMAGNUM_WITH_BASISIMAGECONVERTER=OFF \
    -DMAGNUM_WITH_BASISIMPORTER=ON -DBASIS_UNIVERSAL_DIR=$HOME/basis_universal \
    -DMAGNUM_WITH_CGLTFIMPORTER=ON \
    -DMAGNUM_WITH_DDSIMPORTER=ON \
    -DMAGNUM_WITH_DEVILIMAGEIMPORTER=OFF \
    -DMAGNUM_WITH_DRFLACAUDIOIMPORTER=ON \
    -DMAGNUM_WITH_DRMP3AUDIOIMPORTER=ON \
    -DMAGNUM_WITH_DRWAVAUDIOIMPORTER=ON \
    -DMAGNUM_WITH_FAAD2AUDIOIMPORTER=ON \
    -DMAGNUM_WITH_FREETYPEFONT=OFF \
    -DMAGNUM_WITH_GLSLANGSHADERCONVERTER=OFF \
    -DMAGNUM_WITH_GLTFIMPORTER=ON \
    -DMAGNUM_WITH_GLTFSCENECONVERTER=ON \
    -DMAGNUM_WITH_HARFBUZZFONT=OFF \
    -DMAGNUM_WITH_ICOIMPORTER=ON \
    -DMAGNUM_WITH_JPEGIMAGECONVERTER=OFF \
    -DMAGNUM_WITH_JPEGIMPORTER=OFF \
    -DMAGNUM_WITH_KTXIMAGECONVERTER=ON \
    -DMAGNUM_WITH_KTXIMPORTER=ON \
    -DMAGNUM_WITH_MESHOPTIMIZERSCENECONVERTER=OFF \
    -DMAGNUM_WITH_MINIEXRIMAGECONVERTER=ON \
    -DMAGNUM_WITH_OPENEXRIMAGECONVERTER=OFF \
    -DMAGNUM_WITH_OPENEXRIMPORTER=OFF \
    -DMAGNUM_WITH_OPENGEXIMPORTER=ON \
    -DMAGNUM_WITH_PNGIMAGECONVERTER=OFF \
    -DMAGNUM_WITH_PNGIMPORTER=OFF \
    -DMAGNUM_WITH_PRIMITIVEIMPORTER=ON \
    -DMAGNUM_WITH_SPIRVTOOLSSHADERCONVERTER=OFF \
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
    -DMAGNUM_WITH_WEBPIMPORTER=OFF \
    -DMAGNUM_BUILD_TESTS=ON \
    -DMAGNUM_BUILD_GL_TESTS=ON \
    -G Ninja
ninja $NINJA_JOBS

# Test
CORRADE_TEST_COLOR=ON ctest -V

# Test install, after running the tests as for them it shouldn't be needed
ninja install
