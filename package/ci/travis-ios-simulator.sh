#!/bin/bash
set -ev

git submodule update --init

# Corrade
git clone --depth 1 git://github.com/mosra/corrade.git
cd corrade

# Build native corrade-rc
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps-native \
    -DCMAKE_INSTALL_RPATH=$HOME/deps-native/lib \
    -DWITH_INTERCONNECT=OFF \
    -DWITH_PLUGINMANAGER=OFF \
    -DWITH_TESTSUITE=OFF
make -j install
cd ..

# Crosscompile Corrade
mkdir build-ios && cd build-ios
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../../toolchains/generic/iOS.cmake \
    -DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk \
    -DCMAKE_OSX_ARCHITECTURES="x86_64" \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DBUILD_STATIC=ON \
    -DTESTSUITE_TARGET_XCTEST=ON \
    -DWITH_INTERCONNECT=OFF \
    -DBUILD_STATIC=ON \
    -G Xcode
set -o pipefail && cmake --build . --config Release --target install | xcpretty
cd ../..

# Crosscompile Magnum
git clone --depth 1 git://github.com/mosra/magnum.git
cd magnum
mkdir build-ios && cd build-ios
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../../toolchains/generic/iOS.cmake \
    -DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk \
    -DCMAKE_OSX_ARCHITECTURES="x86_64" \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DWITH_AUDIO=ON \
    -DWITH_DEBUGTOOLS=OFF \
    -DWITH_MESHTOOLS=OFF \
    -DWITH_PRIMITIVES=OFF \
    -DWITH_SCENEGRAPH=OFF \
    -DWITH_SHADERS=OFF \
    -DWITH_SHAPES=OFF \
    -DWITH_TEXT=ON \
    -DWITH_TEXTURETOOLS=ON \
    -DWITH_OPENGLTESTER=ON \
    -DTARGET_GLES2=$TARGET_GLES2 \
    -DBUILD_STATIC=ON \
    -G Xcode
set -o pipefail && cmake --build . --config Release --target install | xcpretty
cd ../..

# Crosscompile
mkdir build-ios && cd build-ios
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../toolchains/generic/iOS.cmake \
    -DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk \
    -DCMAKE_OSX_ARCHITECTURES="x86_64" \
    -DCORRADE_RC_EXECUTABLE=$HOME/deps-native/bin/corrade-rc \
    -DCMAKE_INSTALL_PREFIX=$HOME/deps \
    -DWITH_ANYAUDIOIMPORTER=ON \
    -DWITH_ANYIMAGECONVERTER=ON \
    -DWITH_ANYIMAGEIMPORTER=ON \
    -DWITH_ANYSCENEIMPORTER=ON \
    -DWITH_ASSIMPIMPORTER=OFF \
    -DWITH_COLLADAIMPORTER=OFF \
    -DWITH_DDSIMPORTER=ON \
    -DWITH_DEVILIMAGEIMPORTER=OFF \
    -DWITH_DRFLACAUDIOIMPORTER=ON \
    -DWITH_DRWAVAUDIOIMPORTER=ON \
    -DWITH_FREETYPEFONT=OFF \
    -DWITH_HARFBUZZFONT=OFF \
    -DWITH_JPEGIMPORTER=OFF \
    -DWITH_MINIEXRIMAGECONVERTER=ON \
    -DWITH_OPENGEXIMPORTER=ON \
    -DWITH_PNGIMAGECONVERTER=OFF \
    -DWITH_PNGIMPORTER=OFF \
    -DWITH_STANFORDIMPORTER=ON \
    -DWITH_STBIMAGECONVERTER=ON \
    -DWITH_STBIMAGEIMPORTER=ON \
    -DWITH_STBTRUETYPEFONT=ON \
    -DWITH_STBVORBISAUDIOIMPORTER=ON \
    -DBUILD_STATIC=ON \
    -DBUILD_TESTS=ON \
    -DBUILD_GL_TESTS=ON \
    -G Xcode
set -o pipefail && cmake --build . --config Release | xcpretty

# TODO: find a better way to avoid
# Library not loaded: /System/Library/Frameworks/OpenGLES.framework/OpenGLES
# error
DYLD_FALLBACK_LIBRARY_PATH=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/System/Library/Frameworks/OpenGLES.framework/ DYLD_FALLBACK_FRAMEWORK_PATH=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk/System/Library/Frameworks CORRADE_TEST_COLOR=ON ctest -V -C Release -E GLTest
