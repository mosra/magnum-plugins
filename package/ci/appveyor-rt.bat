if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2017" call "C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC/Auxiliary/Build/vcvarsall.bat" x64 || exit /b
if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2015" call "C:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/vcvarsall.bat" x64 || exit /b
if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2015" set GENERATOR=Visual Studio 14 2015
if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2017" set GENERATOR=Visual Studio 15 2017
set PATH=%APPVEYOR_BUILD_FOLDER%\deps-native\bin;%PATH%

git clone --depth 1 https://github.com/mosra/corrade.git || exit /b
cd corrade || exit /b

rem Build native corrade-rc
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps-native ^
    -DCORRADE_WITH_INTERCONNECT=OFF ^
    -DCORRADE_WITH_PLUGINMANAGER=OFF ^
    -DCORRADE_WITH_TESTSUITE=OFF ^
    -DCORRADE_WITH_UTILITY=OFF ^
    -G Ninja || exit /b
cmake --build . --target install || exit /b
cd .. || exit /b

rem Crosscompile Corrade
mkdir build-rt && cd build-rt || exit /b
cmake .. ^
    -DCMAKE_SYSTEM_NAME=WindowsStore ^
    -DCMAKE_SYSTEM_VERSION=10.0 ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCORRADE_RC_EXECUTABLE=%APPVEYOR_BUILD_FOLDER%/deps-native/bin/corrade-rc.exe ^
    -DCORRADE_WITH_INTERCONNECT=OFF ^
    -DCORRADE_BUILD_STATIC=ON ^
    -G "%GENERATOR%" -A x64 || exit /b
cmake --build . --config Release --target install -- /m /v:m || exit /b
cd .. && cd ..

rem Crosscompile Magnum
git clone --depth 1 https://github.com/mosra/magnum.git || exit /b
cd magnum || exit /b
mkdir build-rt && cd build-rt || exit /b
cmake .. ^
    -DCMAKE_SYSTEM_NAME=WindowsStore ^
    -DCMAKE_SYSTEM_VERSION=10.0 ^
    -DCMAKE_PREFIX_PATH=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCORRADE_RC_EXECUTABLE=%APPVEYOR_BUILD_FOLDER%/deps-native/bin/corrade-rc.exe ^
    -DMAGNUM_WITH_AUDIO=OFF ^
    -DMAGNUM_WITH_DEBUGTOOLS=OFF ^
    -DMAGNUM_WITH_GL=OFF ^
    -DMAGNUM_WITH_MESHTOOLS=ON ^
    -DMAGNUM_WITH_PRIMITIVES=ON ^
    -DMAGNUM_WITH_SCENEGRAPH=OFF ^
    -DMAGNUM_WITH_SCENETOOLS=OFF ^
    -DMAGNUM_WITH_SHADERS=OFF ^
    -DMAGNUM_WITH_TEXT=ON ^
    -DMAGNUM_WITH_TEXTURETOOLS=ON ^
    -DMAGNUM_WITH_ANYIMAGEIMPORTER=ON ^
    -DMAGNUM_BUILD_STATIC=ON ^
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
    -DWITH_ASTCIMPORTER=ON ^
    -DWITH_BASISIMAGECONVERTER=ON ^
    -DWITH_BASISIMPORTER=ON -DBASIS_UNIVERSAL_DIR=%APPVEYOR_BUILD_FOLDER%/basis_universal ^
    -DWITH_CGLTFIMPORTER=ON ^
    -DWITH_DDSIMPORTER=ON ^
    -DWITH_DEVILIMAGEIMPORTER=OFF ^
    -DWITH_DRFLACAUDIOIMPORTER=OFF ^
    -DWITH_DRMP3AUDIOIMPORTER=OFF ^
    -DWITH_DRWAVAUDIOIMPORTER=OFF ^
    -DWITH_FREETYPEFONT=OFF ^
    -DWITH_GLSLANGSHADERCONVERTER=OFF ^
    -DWITH_GLTFIMPORTER=ON ^
    -DWITH_HARFBUZZFONT=OFF ^
    -DWITH_ICOIMPORTER=ON ^
    -DWITH_JPEGIMAGECONVERTER=OFF ^
    -DWITH_JPEGIMPORTER=OFF ^
    -DWITH_KTXIMAGECONVERTER=ON ^
    -DWITH_KTXIMPORTER=ON ^
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
    -DWITH_STBDXTIMAGECONVERTER=ON ^
    -DWITH_STBIMAGECONVERTER=ON ^
    -DWITH_STBIMAGEIMPORTER=ON ^
    -DWITH_STBTRUETYPEFONT=ON ^
    -DWITH_STBVORBISAUDIOIMPORTER=OFF ^
    -DWITH_STLIMPORTER=ON ^
    -DWITH_TINYGLTFIMPORTER=ON ^
    -DWITH_WEBPIMPORTER=OFF ^
    -DBUILD_STATIC=ON ^
    -G "%GENERATOR%" -A x64 || exit /b
cmake --build . --config Release -- /m /v:m || exit /b
