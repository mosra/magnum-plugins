call "C:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/vcvarsall.bat" amd64 || exit /b
set PATH=%APPVEYOR_BUILD_FOLDER%/openal/bin/Win64;%APPVEYOR_BUILD_FOLDER%\deps\bin;%PATH%

rem Build Corrade
git clone --depth 1 git://github.com/mosra/corrade.git || exit /b
cd corrade || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DWITH_INTERCONNECT=OFF ^
    -G Ninja || exit /b
cmake --build . || exit /b
cmake --build . --target install || exit /b
cd .. && cd ..

rem Build Magnum
git clone --depth 1 git://github.com/mosra/magnum.git || exit /b
cd magnum || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCMAKE_PREFIX_PATH="%APPVEYOR_BUILD_FOLDER%/openal" ^
    -DWITH_AUDIO=ON ^
    -DWITH_WINDOWLESSWGLAPPLICATION=ON ^
    -DWITH_WGLCONTEXT=ON ^
    -DWITH_MAGNUMFONT=ON ^
    -DWITH_MAGNUMFONTCONVERTER=ON ^
    -DWITH_OBJIMPORTER=ON ^
    -DWITH_TGAIMAGECONVERTER=ON ^
    -DWITH_TGAIMPORTER=ON ^
    -DWITH_WAVAUDIOIMPORTER=ON ^
    -DWITH_DISTANCEFIELDCONVERTER=ON ^
    -DWITH_FONTCONVERTER=ON ^
    -G Ninja || exit /b
cmake --build . || exit /b
cmake --build . --target install || exit /b
cd .. && cd ..

rem Build LibJPEG Turbo
cd libjpeg-turbo
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=C:/Sys -DWITH_JPEG8=ON -DWITH_SIMD=OFF -G Ninja
cmake --build .
cmake --build . --target install
cd .. && cd ..

rem Build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=C:/Sys -DCMAKE_PREFIX_PATH=%APPVEYOR_BUILD_FOLDER%/openal -DWITH_ANYAUDIOIMPORTER=ON -DWITH_ANYIMAGECONVERTER=ON -DWITH_ANYIMAGEIMPORTER=ON -DWITH_ANYSCENEIMPORTER=ON -DWITH_DDSIMPORTER=ON -DWITH_FREETYPEFONT=OFF -DWITH_HARFBUZZFONT=OFF -DWITH_JPEGIMPORTER=ON -DWITH_MINIEXRIMAGECONVERTER=ON -DWITH_OPENGEXIMPORTER=ON -DWITH_PNGIMAGECONVERTER=OFF -DWITH_PNGIMPORTER=OFF -DWITH_STANFORDIMPORTER=ON -DWITH_STBIMAGEIMPORTER=ON -DWITH_STBPNGIMAGECONVERTER=ON -DWITH_STBTRUETYPEFONT=ON -DWITH_STBVORBISAUDIOIMPORTER=ON -DBUILD_TESTS=ON -DBUILD_GL_TESTS=ON -G Ninja
cmake --build .
cmake --build . --target install
cmake . -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/Deploy -DBUILD_TESTS=OFF
cmake --build . --target install
cd ../Deploy
7z a ../magnum-plugins.zip *
