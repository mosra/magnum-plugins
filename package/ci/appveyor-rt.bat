if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2017" call "C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC/Auxiliary/Build/vcvarsall.bat" x64 || exit /b
if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2015" call "C:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/vcvarsall.bat" x64 || exit /b
if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2015" set GENERATOR=Visual Studio 14 2015
if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2017" set GENERATOR=Visual Studio 15 2017
set PATH=%APPVEYOR_BUILD_FOLDER%\deps-native\bin;%PATH%

git clone --depth 1 git://github.com/mosra/corrade.git || exit /b
cd corrade || exit /b

rem Build native corrade-rc
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps-native ^
    -DWITH_INTERCONNECT=OFF ^
    -DWITH_PLUGINMANAGER=OFF ^
    -DWITH_TESTSUITE=OFF ^
    -DWITH_UTILITY=OFF ^
    -G Ninja || exit /b
cmake --build . --target install || exit /b
cd .. || exit /b

rem Crosscompile Corrade
mkdir build-rt && cd build-rt || exit /b
cmake .. ^
    -DCMAKE_SYSTEM_NAME=WindowsStore ^
    -DCMAKE_SYSTEM_VERSION=10.0 ^
    -DCORRADE_RC_EXECUTABLE=%APPVEYOR_BUILD_FOLDER%/deps-native/bin/corrade-rc.exe ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DWITH_INTERCONNECT=OFF ^
    -DBUILD_STATIC=ON ^
    -G "%GENERATOR%" -A x64 || exit /b
cmake --build . --config Release --target install -- /m /v:m || exit /b
cd .. && cd ..

rem Crosscompile Magnum
git clone --depth 1 git://github.com/mosra/magnum.git || exit /b
cd magnum || exit /b
mkdir build-rt && cd build-rt || exit /b
cmake .. ^
    -DCMAKE_SYSTEM_NAME=WindowsStore ^
    -DCMAKE_SYSTEM_VERSION=10.0 ^
    -DCORRADE_RC_EXECUTABLE=%APPVEYOR_BUILD_FOLDER%/deps-native/bin/corrade-rc.exe ^
    -DCMAKE_PREFIX_PATH=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DWITH_AUDIO=OFF ^
    -DWITH_DEBUGTOOLS=OFF ^
    -DWITH_GL=OFF ^
    -DWITH_MESHTOOLS=ON ^
    -DWITH_PRIMITIVES=ON ^
    -DWITH_SCENEGRAPH=OFF ^
    -DWITH_SHADERS=OFF ^
    -DWITH_TEXT=ON ^
    -DWITH_TEXTURETOOLS=ON ^
    -DWITH_ANYIMAGEIMPORTER=ON ^
    -DBUILD_STATIC=ON ^
    -G "%GENERATOR%" -A x64 || exit /b
cmake --build . --config Release --target install -- /m /v:m || exit /b
cd .. && cd ..

rem Crosscompile
mkdir build-rt && cd build-rt || exit /b
cmake .. ^
    -DCMAKE_SYSTEM_NAME=WindowsStore ^
    -DCMAKE_SYSTEM_VERSION=10.0 ^
    -DCMAKE_PREFIX_PATH=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DWITH_ASSIMPIMPORTER=OFF ^
    -DWITH_BASISIMAGECONVERTER=ON ^
    -DWITH_BASISIMPORTER=ON -DBASIS_UNIVERSAL_DIR=%APPVEYOR_BUILD_FOLDER%/basis_universal ^
    -DWITH_DDSIMPORTER=ON ^
    -DWITH_DEVILIMAGEIMPORTER=OFF ^
    -DWITH_DRFLACAUDIOIMPORTER=OFF ^
    -DWITH_DRMP3AUDIOIMPORTER=OFF ^
    -DWITH_DRWAVAUDIOIMPORTER=OFF ^
    -DWITH_FREETYPEFONT=OFF ^
    -DWITH_GLSLANGSHADERCONVERTER=OFF ^
    -DWITH_HARFBUZZFONT=OFF ^
    -DWITH_ICOIMPORTER=ON ^
    -DWITH_JPEGIMAGECONVERTER=OFF ^
    -DWITH_JPEGIMPORTER=OFF ^
    -DWITH_MESHOPTIMIZERSCENECONVERTER=OFF ^
    -DWITH_MINIEXRIMAGECONVERTER=ON ^
    -DWITH_OPENEXRIMAGECONVERTER=OFF ^
    -DWITH_OPENGEXIMPORTER=OFF ^
    -DWITH_OPENGEXIMPORTER=ON ^
    -DWITH_PNGIMAGECONVERTER=OFF ^
    -DWITH_PNGIMPORTER=OFF ^
    -DWITH_PRIMITIVEIMPORTER=ON ^
    -DWITH_SPIRVTOOLSSHADERCONVERTER=OFF ^
    -DWITH_STANFORDIMPORTER=ON ^
    -DWITH_STANFORDSCENECONVERTER=ON ^
    -DWITH_STBIMAGECONVERTER=ON ^
    -DWITH_STBIMAGEIMPORTER=ON ^
    -DWITH_STBTRUETYPEFONT=ON ^
    -DWITH_STBVORBISAUDIOIMPORTER=OFF ^
    -DWITH_STLIMPORTER=ON ^
    -DWITH_TINYGLTFIMPORTER=ON ^
    -DBUILD_STATIC=ON ^
    -G "%GENERATOR%" -A x64 || exit /b
cmake --build . --config Release -- /m /v:m || exit /b
