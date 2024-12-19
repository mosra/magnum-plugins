#!/bin/bash
set -ev

git submodule update --init

# Corrade
git clone --depth 1 --branch next https://github.com/mosra/corrade.git
cd corrade

# Build native corrade-rc
mkdir build && cd build
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
mkdir build-ios && cd build-ios
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../../toolchains/generic/iOS.cmake \
    -DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk \
    -DCMAKE_OSX_ARCHITECTURES="x86_64" \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
    -DCORRADE_TESTSUITE_TARGET_XCTEST=ON \
    -DCORRADE_WITH_INTERCONNECT=OFF \
    -DCORRADE_BUILD_STATIC=ON \
    -G Xcode
set -o pipefail && cmake --build . --config Release --target install -j$XCODE_JOBS | xcbeautify
cd ../..

# Crosscompile Magnum
git clone --depth 1 --branch next https://github.com/mosra/magnum.git
cd magnum
mkdir build-ios && cd build-ios
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../../toolchains/generic/iOS.cmake \
    -DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk \
    -DCMAKE_OSX_ARCHITECTURES="x86_64" \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
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
    -DMAGNUM_BUILD_STATIC=ON \
    -G Xcode
set -o pipefail && cmake --build . --config Release --target install -j$XCODE_JOBS | xcbeautify
cd ../..

# Crosscompile zstd
export ZSTD_VERSION=1.5.5
wget --no-check-certificate https://github.com/facebook/zstd/archive/refs/tags/v$ZSTD_VERSION.tar.gz
tar -xzf v$ZSTD_VERSION.tar.gz
cd zstd-$ZSTD_VERSION
# There's already a directory named `build`
mkdir build_ && cd build_
cmake ../build/cmake \
    -DCMAKE_TOOLCHAIN_FILE=../../toolchains/generic/iOS.cmake \
    -DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk \
    -DCMAKE_OSX_ARCHITECTURES="x86_64" \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DZSTD_BUILD_PROGRAMS=OFF \
    -DZSTD_BUILD_SHARED=OFF \
    -DZSTD_BUILD_STATIC=ON \
    -DZSTD_MULTITHREAD_SUPPORT=OFF \
    -G Ninja
ninja install
cd ../..

# Crosscompile. MAGNUM_BUILD_GL_TESTS is enabled just to be sure, it should not
# be needed by any plugin.
mkdir build-ios && cd build-ios
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../toolchains/generic/iOS.cmake \
    -DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk \
    -DCMAKE_OSX_ARCHITECTURES="x86_64" \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
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
    -DMAGNUM_WITH_FAAD2AUDIOIMPORTER=OFF \
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
    -DMAGNUM_BUILD_STATIC=ON \
    -DMAGNUM_BUILD_TESTS=ON \
    -DMAGNUM_BUILD_GL_TESTS=ON \
    -G Xcode
set -o pipefail && cmake --build . --config Release -j$XCODE_JOBS | xcbeautify

# TODO enable again once https://github.com/mosra/corrade/pull/176 is resolved
#CORRADE_TEST_COLOR=ON ctest -V -C Release

# Test install, after running the tests as for them it shouldn't be needed
set -o pipefail && cmake --build . --config Release --target install -j$XCODE_JOBS | xcbeautify
