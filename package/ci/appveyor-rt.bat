if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2017" call "C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC/Auxiliary/Build/vcvarsall.bat" x64 || exit /b
if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2015" call "C:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/vcvarsall.bat" x64 || exit /b
if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2017" set GENERATOR=Visual Studio 15 2017
rem This is what should make the *native* corrade-rc getting found. Corrade
rem should not cross-compile it on this platform, because then it'd get found
rem instead of the native version with no way to distinguish the two, and all
rem hell breaks loose. Thus also not passing CORRADE_RC_EXECUTABLE anywhere
rem below to ensure this doesn't regress.
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
    -DMAGNUM_WITH_AUDIO=OFF ^
    -DMAGNUM_WITH_DEBUGTOOLS=OFF ^
    -DMAGNUM_WITH_GL=OFF ^
    -DMAGNUM_WITH_MATERIALTOOLS=OFF ^
    -DMAGNUM_WITH_MESHTOOLS=ON ^
    -DMAGNUM_WITH_PRIMITIVES=ON ^
    -DMAGNUM_WITH_SCENEGRAPH=OFF ^
    -DMAGNUM_WITH_SCENETOOLS=OFF ^
    -DMAGNUM_WITH_SHADERS=OFF ^
    -DMAGNUM_WITH_SHADERTOOLS=OFF ^
    -DMAGNUM_WITH_TEXT=ON ^
    -DMAGNUM_WITH_TEXTURETOOLS=ON ^
    -DMAGNUM_WITH_ANYIMAGEIMPORTER=ON ^
    -DMAGNUM_BUILD_STATIC=ON ^
    -G "%GENERATOR%" -A x64 || exit /b
cmake --build . --config Release --target install -- /m /v:m || exit /b
cd .. && cd ..

rem Crosscompile. No tests because they take ages to build, each executable is
rem a msix file, and they can't be reasonably run either. F this platform.
mkdir build-rt && cd build-rt || exit /b
cmake .. ^
    -DCMAKE_SYSTEM_NAME=WindowsStore ^
    -DCMAKE_SYSTEM_VERSION=10.0 ^
    -DCMAKE_PREFIX_PATH=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DMAGNUM_WITH_ASSIMPIMPORTER=OFF ^
    -DMAGNUM_WITH_ASTCIMPORTER=ON ^
    -DMAGNUM_WITH_BASISIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_BASISIMPORTER=ON -DBASIS_UNIVERSAL_DIR=%APPVEYOR_BUILD_FOLDER%/basis_universal ^
    -DMAGNUM_WITH_BCDECIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_CGLTFIMPORTER=ON ^
    -DMAGNUM_WITH_DDSIMPORTER=ON ^
    -DMAGNUM_WITH_DEVILIMAGEIMPORTER=OFF ^
    -DMAGNUM_WITH_DRFLACAUDIOIMPORTER=OFF ^
    -DMAGNUM_WITH_DRMP3AUDIOIMPORTER=OFF ^
    -DMAGNUM_WITH_DRWAVAUDIOIMPORTER=OFF ^
    -DMAGNUM_WITH_ETCDECIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_FREETYPEFONT=OFF ^
    -DMAGNUM_WITH_GLSLANGSHADERCONVERTER=OFF ^
    -DMAGNUM_WITH_GLTFIMPORTER=ON ^
    -DMAGNUM_WITH_GLTFSCENECONVERTER=ON ^
    -DMAGNUM_WITH_HARFBUZZFONT=OFF ^
    -DMAGNUM_WITH_ICOIMPORTER=ON ^
    -DMAGNUM_WITH_JPEGIMAGECONVERTER=OFF ^
    -DMAGNUM_WITH_JPEGIMPORTER=OFF ^
    -DMAGNUM_WITH_KTXIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_KTXIMPORTER=ON ^
    -DMAGNUM_WITH_LUNASVGIMPORTER=OFF ^
    -DMAGNUM_WITH_MESHOPTIMIZERSCENECONVERTER=OFF ^
    -DMAGNUM_WITH_MINIEXRIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_OPENEXRIMAGECONVERTER=OFF ^
    -DMAGNUM_WITH_OPENGEXIMPORTER=OFF ^
    -DMAGNUM_WITH_OPENGEXIMPORTER=ON ^
    -DMAGNUM_WITH_PNGIMAGECONVERTER=OFF ^
    -DMAGNUM_WITH_PNGIMPORTER=OFF ^
    -DMAGNUM_WITH_PRIMITIVEIMPORTER=ON ^
    -DMAGNUM_WITH_RESVGIMPORTER=OFF ^
    -DMAGNUM_WITH_SPIRVTOOLSSHADERCONVERTER=OFF ^
    -DMAGNUM_WITH_SPNGIMPORTER=OFF ^
    -DMAGNUM_WITH_STANFORDIMPORTER=ON ^
    -DMAGNUM_WITH_STANFORDSCENECONVERTER=ON ^
    -DMAGNUM_WITH_STBDXTIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_STBIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_STBIMAGEIMPORTER=ON ^
    -DMAGNUM_WITH_STBRESIZEIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_STBTRUETYPEFONT=ON ^
    -DMAGNUM_WITH_STBVORBISAUDIOIMPORTER=OFF ^
    -DMAGNUM_WITH_STLIMPORTER=ON ^
    -DMAGNUM_WITH_TINYGLTFIMPORTER=ON ^
    -DMAGNUM_WITH_UFBXIMPORTER=ON ^
    -DMAGNUM_WITH_WEBPIMAGECONVERTER=OFF ^
    -DMAGNUM_WITH_WEBPIMPORTER=OFF ^
    -DMAGNUM_BUILD_STATIC=ON ^
    -G "%GENERATOR%" -A x64 || exit /b
cmake --build . --config Release -- /m /v:m || exit /b

rem Test install
cmake --build . --config Release --target install || exit /b
