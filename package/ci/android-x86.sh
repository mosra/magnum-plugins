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
    -DCORRADE_WITH_INTERCONNECT=OFF \
    -DCORRADE_WITH_PLUGINMANAGER=OFF \
    -DCORRADE_WITH_TESTSUITE=OFF \
    -DCORRADE_WITH_UTILITY=OFF \
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
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
    -DCORRADE_WITH_INTERCONNECT=OFF \
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
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
    -DMAGNUM_WITH_AUDIO=OFF \
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
    -G Ninja
ninja install
cd ../..

# Crosscompile zstd
export ZSTD_VERSION=1.5.5
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

# Crosscompile. MAGNUM_BUILD_GL_TESTS is enabled just to be sure, it should not
# be needed by any plugin.
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
    -DCMAKE_PREFIX_PATH=$HOME/deps \
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
    -DMAGNUM_WITH_DRFLACAUDIOIMPORTER=OFF \
    -DMAGNUM_WITH_DRMP3AUDIOIMPORTER=OFF \
    -DMAGNUM_WITH_DRWAVAUDIOIMPORTER=OFF \
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
    -DMAGNUM_WITH_LUNASVGIMPORTER=OFF \
    -DMAGNUM_WITH_MESHOPTIMIZERSCENECONVERTER=OFF \
    -DMAGNUM_WITH_MINIEXRIMAGECONVERTER=ON \
    -DMAGNUM_WITH_OPENEXRIMAGECONVERTER=OFF \
    -DMAGNUM_WITH_OPENEXRIMPORTER=OFF \
    -DMAGNUM_WITH_OPENGEXIMPORTER=ON \
    -DMAGNUM_WITH_PNGIMAGECONVERTER=OFF \
    -DMAGNUM_WITH_PNGIMPORTER=OFF \
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
    -DMAGNUM_WITH_STBVORBISAUDIOIMPORTER=OFF \
    -DMAGNUM_WITH_STLIMPORTER=ON \
    -DMAGNUM_WITH_TINYGLTFIMPORTER=ON \
    -DMAGNUM_WITH_UFBXIMPORTER=ON \
    -DMAGNUM_WITH_WEBPIMAGECONVERTER=OFF \
    -DMAGNUM_WITH_WEBPIMPORTER=OFF \
    -DMAGNUM_BUILD_TESTS=ON \
    -DMAGNUM_BUILD_GL_TESTS=ON \
    -G Ninja
ninja $NINJA_JOBS

# Wait for emulator to start (done in parallel to build) and run tests
circle-android wait-for-boot
# `adb push` uploads timeout quite often, and then CircleCI waits 10 minutes
# until it aborts the job due to no output. CTest can do timeouts on its own,
# but somehow the default is 10M seconds, which is quite a lot honestly.
# Instead set the timeout to 15 seconds which should be enough even for very
# heavy future benchmarks, and try ten more times if it gets stuck. Other repos
# have just three times, but here we have *a lot* test files.
CORRADE_TEST_COLOR=ON ctest -V --timeout 15 --repeat after-timeout:10

# Test install, after running the tests as for them it shouldn't be needed
ninja install
