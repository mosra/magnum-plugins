#!/bin/bash
set -ev

# Corrade
git clone --depth 1 https://github.com/mosra/corrade.git
cd corrade

# Build native corrade-rc
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps-native \
    -DWITH_INTERCONNECT=OFF \
    -DWITH_PLUGINMANAGER=OFF \
    -DWITH_TESTSUITE=OFF \
    -DWITH_UTILITY=OFF \
    -G Ninja
ninja install
cd ..

# Crosscompile Corrade
mkdir build-android-x86 && cd build-android-x86
cmake .. \
    -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_SYSTEM_VERSION=29 \
    -DCMAKE_ANDROID_ARCH_ABI=x86 \
    -DCMAKE_ANDROID_NDK_TOOLCHAIN_VERSION=clang \
    -DCMAKE_ANDROID_STL_TYPE=c++_static \
    -DCMAKE_BUILD_TYPE=Release \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DWITH_INTERCONNECT=OFF \
    -G Ninja
ninja install
cd ../..

# Crosscompile Magnum
git clone --depth 1 https://github.com/mosra/magnum.git
cd magnum
mkdir build-android-x86 && cd build-android-x86
cmake .. \
    -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_SYSTEM_VERSION=29 \
    -DCMAKE_ANDROID_ARCH_ABI=x86 \
    -DCMAKE_ANDROID_NDK_TOOLCHAIN_VERSION=clang \
    -DCMAKE_ANDROID_STL_TYPE=c++_static \
    -DCMAKE_BUILD_TYPE=Release \
    `# playing with fire and not setting up FIND_ROOT_PATH as magnum should` \
    `# not need any dependencies from the NDK` \
    -DCMAKE_FIND_ROOT_PATH=$HOME/deps \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DWITH_AUDIO=OFF \
    -DWITH_DEBUGTOOLS=ON \
    -DWITH_GL=OFF \
    -DWITH_MESHTOOLS=ON \
    -DWITH_PRIMITIVES=ON \
    -DWITH_SCENEGRAPH=OFF \
    -DWITH_SCENETOOLS=OFF \
    -DWITH_SHADERS=OFF \
    -DWITH_TEXT=ON \
    -DWITH_TEXTURETOOLS=ON \
    -DWITH_ANYIMAGEIMPORTER=ON \
    -G Ninja
ninja install
cd ../..

# Crosscompile zstd
export ZSTD_VERSION=1.5.0
wget --no-check-certificate https://github.com/facebook/zstd/archive/refs/tags/v$ZSTD_VERSION.tar.gz
tar -xzf v$ZSTD_VERSION.tar.gz
cd zstd-$ZSTD_VERSION
# There's already a directory named `build`
mkdir build_ && cd build_
cmake ../build/cmake \
    -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_SYSTEM_VERSION=29 \
    -DCMAKE_ANDROID_ARCH_ABI=x86 \
    -DCMAKE_ANDROID_NDK_TOOLCHAIN_VERSION=clang \
    -DCMAKE_ANDROID_STL_TYPE=c++_static \
    -DCMAKE_BUILD_TYPE=Release \
    -DZSTD_BUILD_PROGRAMS=OFF \
    -DZSTD_BUILD_SHARED=OFF \
    -DZSTD_BUILD_STATIC=ON \
    -DZSTD_MULTITHREAD_SUPPORT=OFF \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -G Ninja
ninja install
cd ../..

# Crosscompile. BUILD_GL_TESTS is enabled just to be sure, it should not be
# needed by any plugin.
mkdir build-android-x86 && cd build-android-x86
cmake .. \
    -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_SYSTEM_VERSION=29 \
    -DCMAKE_ANDROID_ARCH_ABI=x86 \
    -DCMAKE_ANDROID_NDK_TOOLCHAIN_VERSION=clang \
    -DCMAKE_ANDROID_STL_TYPE=c++_static \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_FIND_ROOT_PATH="/opt/android/sdk/ndk/21.4.7075529/toolchains/llvm/prebuilt/linux-x86_64/sysroot;$HOME/deps" \
    -DCMAKE_FIND_LIBRARY_CUSTOM_LIB_SUFFIX=/i686-linux-android/29 \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
    -DCMAKE_PREFIX_PATH=$HOME/deps \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DWITH_ASSIMPIMPORTER=OFF \
    -DWITH_BASISIMAGECONVERTER=OFF \
    -DWITH_BASISIMPORTER=ON -DBASIS_UNIVERSAL_DIR=$HOME/basis_universal \
    -DWITH_CGLTFIMPORTER=ON \
    -DWITH_DDSIMPORTER=ON \
    -DWITH_DEVILIMAGEIMPORTER=OFF \
    -DWITH_DRFLACAUDIOIMPORTER=OFF \
    -DWITH_DRMP3AUDIOIMPORTER=OFF \
    -DWITH_DRWAVAUDIOIMPORTER=OFF \
    -DWITH_FAAD2AUDIOIMPORTER=OFF \
    -DWITH_FREETYPEFONT=OFF \
    -DWITH_GLSLANGSHADERCONVERTER=OFF \
    -DWITH_GLTFIMPORTER=ON \
    -DWITH_HARFBUZZFONT=OFF \
    -DWITH_ICOIMPORTER=ON \
    -DWITH_JPEGIMAGECONVERTER=OFF \
    -DWITH_JPEGIMPORTER=OFF \
    -DWITH_KTXIMAGECONVERTER=ON \
    -DWITH_KTXIMPORTER=ON \
    -DWITH_MESHOPTIMIZERSCENECONVERTER=OFF \
    -DWITH_MINIEXRIMAGECONVERTER=ON \
    -DWITH_OPENEXRIMAGECONVERTER=OFF \
    -DWITH_OPENEXRIMPORTER=OFF \
    -DWITH_OPENGEXIMPORTER=ON \
    -DWITH_PNGIMAGECONVERTER=OFF \
    -DWITH_PNGIMPORTER=OFF \
    -DWITH_PRIMITIVEIMPORTER=ON \
    -DWITH_SPIRVTOOLSSHADERCONVERTER=OFF \
    -DWITH_STANFORDIMPORTER=ON \
    -DWITH_STANFORDSCENECONVERTER=ON \
    -DWITH_STBDXTIMAGECONVERTER=ON \
    -DWITH_STBIMAGECONVERTER=ON \
    -DWITH_STBIMAGEIMPORTER=ON \
    -DWITH_STBTRUETYPEFONT=ON \
    -DWITH_STBVORBISAUDIOIMPORTER=OFF \
    -DWITH_STLIMPORTER=ON \
    -DWITH_TINYGLTFIMPORTER=ON \
    -DBUILD_TESTS=ON \
    -DBUILD_GL_TESTS=ON \
    -G Ninja
ninja $NINJA_JOBS

# Wait for emulator to start (done in parallel to build) and run tests
circle-android wait-for-boot
CORRADE_TEST_COLOR=ON ctest -V

# Test install, after running the tests as for them it shouldn't be needed
ninja install
