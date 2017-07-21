rem Workaround for CMake not wanting sh.exe on PATH for MinGW. AARGH.
set PATH=%PATH:C:\Program Files\Git\usr\bin;=%
set PATH=C:\tools\mingw64\bin;%APPVEYOR_BUILD_FOLDER%/openal/bin/Win64;%APPVEYOR_BUILD_FOLDER%\deps\bin;%APPVEYOR_BUILD_FOLDER%\devil;%PATH%

rem Build LibJPEG
IF NOT EXIST %APPVEYOR_BUILD_FOLDER%\libjpeg-turbo-1.5.0.tar.gz appveyor DownloadFile http://downloads.sourceforge.net/project/libjpeg-turbo/1.5.0/libjpeg-turbo-1.5.0.tar.gz || exit /b
7z x libjpeg-turbo-1.5.0.tar.gz || exit /b
7z x libjpeg-turbo-1.5.0.tar || exit /b
ren libjpeg-turbo-1.5.0 libjpeg-turbo || exit /b
cd libjpeg-turbo || exit /b
mkdir build && cd build || exit /b
cmake .. -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DWITH_JPEG8=ON ^
    -DWITH_SIMD=OFF ^
    -G "MinGW Makefiles" || exit /b
cmake --build . --target install -- -j || exit /b
cd .. && cd .. || exit /b

rem Build Corrade. Could not get Ninja to work, meh.
git clone --depth 1 git://github.com/mosra/corrade.git || exit /b
cd corrade || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DWITH_INTERCONNECT=OFF ^
    -G "MinGW Makefiles" || exit /b
cmake --build . -- -j || exit /b
cmake --build . --target install -- -j || exit /b
cd .. && cd ..

rem Build Magnum
git clone --depth 1 git://github.com/mosra/magnum.git || exit /b
cd magnum || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCMAKE_PREFIX_PATH=%APPVEYOR_BUILD_FOLDER%/openal ^
    -DWITH_AUDIO=ON ^
    -DWITH_DEBUGTOOLS=OFF ^
    -DWITH_MESHTOOLS=OFF ^
    -DWITH_PRIMITIVES=OFF ^
    -DWITH_SCENEGRAPH=OFF ^
    -DWITH_SHADERS=OFF ^
    -DWITH_SHAPES=OFF ^
    -DWITH_TEXT=ON ^
    -DWITH_TEXTURETOOLS=ON ^
    -DWITH_OPENGLTESTER=ON ^
    -DWITH_OBJIMPORTER=ON ^
    -DWITH_TGAIMPORTER=ON ^
    -DWITH_WAVAUDIOIMPORTER=ON ^
    -G "MinGW Makefiles" || exit /b
cmake --build . -- -j || exit /b
cmake --build . --target install -- -j || exit /b
cd .. && cd ..

rem Build
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCMAKE_PREFIX_PATH=%APPVEYOR_BUILD_FOLDER%/openal;%APPVEYOR_BUILD_FOLDER%/devil ^
    -DWITH_ANYAUDIOIMPORTER=ON ^
    -DWITH_ANYIMAGECONVERTER=ON ^
    -DWITH_ANYIMAGEIMPORTER=ON ^
    -DWITH_ANYSCENEIMPORTER=ON ^
    -DWITH_ASSIMPIMPORTER=OFF ^
    -DWITH_DDSIMPORTER=ON ^
    -DWITH_DEVILIMAGEIMPORTER=ON ^
    -DWITH_DRFLACAUDIOIMPORTER=ON ^
    -DWITH_DRWAVAUDIOIMPORTER=ON ^
    -DWITH_FREETYPEFONT=OFF ^
    -DWITH_HARFBUZZFONT=OFF ^
    -DWITH_JPEGIMPORTER=ON ^
    -DWITH_MINIEXRIMAGECONVERTER=ON ^
    -DWITH_OPENGEXIMPORTER=ON ^
    -DWITH_PNGIMAGECONVERTER=OFF ^
    -DWITH_PNGIMPORTER=OFF ^
    -DWITH_STANFORDIMPORTER=ON ^
    -DWITH_STBIMAGECONVERTER=ON ^
    -DWITH_STBIMAGEIMPORTER=ON ^
    -DWITH_STBTRUETYPEFONT=ON ^
    -DWITH_STBVORBISAUDIOIMPORTER=ON ^
    -DBUILD_TESTS=ON ^
    -DBUILD_GL_TESTS=ON ^
    -G "MinGW Makefiles" || exit /b
cmake --build . -- -j || exit /b
cmake --build . --target install -- -j || exit /b

rem Test
ctest -V -E GLTest || exit /b
