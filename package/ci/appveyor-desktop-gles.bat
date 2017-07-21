if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2017" call "C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC/Auxiliary/Build/vcvarsall.bat" x64 || exit /b
if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2015" call "C:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/vcvarsall.bat" x64 || exit /b
set PATH=%APPVEYOR_BUILD_FOLDER%/openal/bin/Win64;%APPVEYOR_BUILD_FOLDER%\deps\bin;%APPVEYOR_BUILD_FOLDER%\libpng\bin;%APPVEYOR_BUILD_FOLDER%\devil;%PATH%

rem Build LibJPEG
IF NOT EXIST %APPVEYOR_BUILD_FOLDER%\libjpeg-turbo-1.5.0.tar.gz appveyor DownloadFile http://downloads.sourceforge.net/project/libjpeg-turbo/1.5.0/libjpeg-turbo-1.5.0.tar.gz || exit /b
7z x libjpeg-turbo-1.5.0.tar.gz || exit /b
7z x libjpeg-turbo-1.5.0.tar || exit /b
ren libjpeg-turbo-1.5.0 libjpeg-turbo || exit /b
cd libjpeg-turbo || exit /b
mkdir build && cd build || exit /b
cmake .. -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DWITH_JPEG8=ON ^
    -DWITH_SIMD=OFF ^
    -G Ninja || exit /b
cmake --build . --target install || exit /b
cd .. && cd .. || exit /b

rem Build libPNG
IF NOT EXIST %APPVEYOR_BUILD_FOLDER%\libpng-1.6.21.zip appveyor DownloadFile https://github.com/winlibs/libpng/archive/libpng-1.6.21.zip || exit /b
IF NOT EXIST %APPVEYOR_BUILD_FOLDER%\source-archive.zip appveyor DownloadFile https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/zlib-win64/source-archive.zip || exit /b

7z x libpng-1.6.21.zip || exit /b
cd libpng-libpng-1.6.21 || exit /b
copy scripts\pnglibconf.h.prebuilt pnglibconf.h || exit /b
cd projects\vstudio2015 || exit /b
7z x ..\..\..\source-archive.zip || exit /b
ren zlib-win64 zlib-1.2.8 || exit /b

cd zlib || exit /b
msbuild /p:SolutionDir=%APPVEYOR_BUILD_FOLDER%\libpng-libpng-1.6.21\projects\vstudio2015 /p:Configuration=Release || exit /b

cd ..\libpng || exit /b
msbuild /p:SolutionDir=%APPVEYOR_BUILD_FOLDER%\libpng-libpng-1.6.21\projects\vstudio2015 /p:Configuration=Release || exit /b

cd %APPVEYOR_BUILD_FOLDER% || exit /b
mkdir libpng || exit /b
mkdir libpng\bin || exit /b
mkdir libpng\lib || exit /b
mkdir libpng\include || exit /b
copy libpng-libpng-1.6.21\projects\vstudio2015X64\Release\libpng.dll libpng\bin\libpng.dll || exit /b
copy libpng-libpng-1.6.21\projects\vstudio2015X64\Release\libpng.lib libpng\lib\libpng.lib || exit /b
copy libpng-libpng-1.6.21\projects\vstudio2015X64\Release\zlib.lib libpng\lib\zlib.lib || exit /b
copy libpng-libpng-1.6.21\png.h libpng\include\ || exit /b
copy libpng-libpng-1.6.21\png.h libpng\include\ || exit /b
copy libpng-libpng-1.6.21\pngconf.h libpng\include\ || exit /b
copy libpng-libpng-1.6.21\pnglibconf.h libpng\include\ || exit /b
copy libpng-libpng-1.6.21\projects\vstudio2015\zlib-1.2.8\zlib.h libpng\include\ || exit /b

rem Build Corrade
git clone --depth 1 git://github.com/mosra/corrade.git || exit /b
cd corrade || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_BUILD_TYPE=Debug ^
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
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCMAKE_PREFIX_PATH=%APPVEYOR_BUILD_FOLDER%/openal ^
    -DTARGET_GLES=ON ^
    -DTARGET_GLES2=%TARGET_GLES2% ^
    -DTARGET_DESKTOP_GLES=ON ^
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
    -G Ninja || exit /b
cmake --build . || exit /b
cmake --build . --target install || exit /b
cd .. && cd ..

rem Build
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_BUILD_TYPE=Debug ^
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
    -G Ninja || exit /b
cmake --build . || exit /b
cmake --build . --target install || exit /b

rem Test
ctest -V -E GLTest || exit /b
