if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2019" call "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build/vcvarsall.bat" x64 || exit /b
if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2017" call "C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC/Auxiliary/Build/vcvarsall.bat" x64 || exit /b
if "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2015" call "C:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/vcvarsall.bat" x64 || exit /b
rem Unlike with Magnum itself which copies OpenAL DLL to its output directory,
rem here we need to do that ourselves as we don't have Magnum's build dir in
rem PATH
set PATH=%APPVEYOR_BUILD_FOLDER%/openal/bin/Win64;%APPVEYOR_BUILD_FOLDER%\deps\bin;%APPVEYOR_BUILD_FOLDER%\devil\unicode;C:\Tools\vcpkg\installed\x64-windows\bin;%PATH%

rem need to explicitly specify a 64-bit target, otherwise CMake+Ninja can't
rem figure that out -- https://gitlab.kitware.com/cmake/cmake/issues/16259
rem for TestSuite we need to enable exceptions explicitly with /EH as these are
rem currently disabled -- https://github.com/catchorg/Catch2/issues/1113
if "%COMPILER%" == "msvc-clang" set COMPILER_EXTRA=-DCMAKE_C_COMPILER="C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/Llvm/bin/clang-cl.exe" -DCMAKE_CXX_COMPILER="C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/Llvm/bin/clang-cl.exe" -DCMAKE_LINKER="C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/Llvm/bin/lld-link.exe" -DCMAKE_C_FLAGS="-m64 /EHsc" -DCMAKE_CXX_FLAGS="-m64 /EHsc"

rem Build libPNG. As of 2020-08-17, vcpkg is broken on the 2019 image and needs
rem updating. Disabling the libPNG build there for now.
IF "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2019" set EXCEPT_IF_VCPKG_IS_BROKEN=OFF
IF NOT "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2019" set EXCEPT_IF_VCPKG_IS_BROKEN=ON
IF NOT "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2019" vcpkg install libpng:x64-windows || exit /b

rem Some dependencies are built on GitHub Actions and there the only available
rem MSVC version is 2019. Better than nothing, but eh.
IF "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2019" set ONLY_ON_MSVC2019=ON
IF NOT "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2019" set ONLY_ON_MSVC2019=OFF
IF "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2015" set EXCEPT_MSVC2015=OFF
IF NOT "%APPVEYOR_BUILD_WORKER_IMAGE%" == "Visual Studio 2015" set EXCEPT_MSVC2015=ON

rem build meshoptimizer
IF NOT EXIST %APPVEYOR_BUILD_FOLDER%\v0.14.zip appveyor DownloadFile https://github.com/zeux/meshoptimizer/archive/v0.14.zip || exit /b
7z x v0.14.zip || exit /b
ren meshoptimizer-0.14 meshoptimizer || exit /b
cd meshoptimizer || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    %COMPILER_EXTRA% -G Ninja || exit /b
cmake --build . --target install || exit /b
cd .. && cd .. || exit /b

rem Build Corrade
git clone --depth 1 git://github.com/mosra/corrade.git || exit /b
cd corrade || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DWITH_INTERCONNECT=OFF ^
    -DUTILITY_USE_ANSI_COLORS=ON ^
    -DBUILD_STATIC=%BUILD_STATIC% ^
    %COMPILER_EXTRA% -G Ninja || exit /b
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
    -DWITH_AUDIO=ON ^
    -DWITH_DEBUGTOOLS=ON ^
    -DWITH_GL=OFF ^
    -DWITH_MESHTOOLS=ON ^
    -DWITH_PRIMITIVES=ON ^
    -DWITH_SCENEGRAPH=OFF ^
    -DWITH_SHADERS=OFF ^
    -DWITH_TEXT=ON ^
    -DWITH_TEXTURETOOLS=ON ^
    -DWITH_ANYIMAGEIMPORTER=ON ^
    -DBUILD_STATIC=%BUILD_STATIC% ^
    -DBUILD_PLUGINS_STATIC=%BUILD_STATIC% ^
    %COMPILER_EXTRA% -G Ninja || exit /b
cmake --build . || exit /b
cmake --build . --target install || exit /b
cd .. && cd ..

rem Build. BUILD_GL_TESTS is enabled just to be sure, it should not be needed
rem by any plugin.
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCMAKE_PREFIX_PATH=%APPVEYOR_BUILD_FOLDER%/openal;%APPVEYOR_BUILD_FOLDER%/devil;C:/Tools/vcpkg/installed/x64-windows ^
    -DWITH_ASSIMPIMPORTER=%EXCEPT_MSVC2015% ^
    -DWITH_BASISIMAGECONVERTER=ON ^
    -DWITH_BASISIMPORTER=ON -DBASIS_UNIVERSAL_DIR=%APPVEYOR_BUILD_FOLDER%/basis_universal ^
    -DWITH_DDSIMPORTER=ON ^
    -DWITH_DEVILIMAGEIMPORTER=ON ^
    -DWITH_DRFLACAUDIOIMPORTER=ON ^
    -DWITH_DRMP3AUDIOIMPORTER=ON ^
    -DWITH_DRWAVAUDIOIMPORTER=ON ^
    -DWITH_FREETYPEFONT=%EXCEPT_MSVC2015% ^
    -DWITH_GLSLANGSHADERCONVERTER=%EXCEPT_MSVC2015% ^
    -DWITH_HARFBUZZFONT=OFF ^
    -DWITH_ICOIMPORTER=ON ^
    -DWITH_JPEGIMAGECONVERTER=%EXCEPT_MSVC2015% ^
    -DWITH_JPEGIMPORTER=%EXCEPT_MSVC2015% ^
    -DWITH_MESHOPTIMIZERSCENECONVERTER=ON ^
    -DWITH_MINIEXRIMAGECONVERTER=ON ^
    -DWITH_OPENEXRIMAGECONVERTER=%EXCEPT_MSVC2015% ^
    -DWITH_OPENEXRIMPORTER=%EXCEPT_MSVC2015% ^
    -DWITH_OPENGEXIMPORTER=ON ^
    -DWITH_PNGIMAGECONVERTER=%EXCEPT_IF_VCPKG_IS_BROKEN% ^
    -DWITH_PNGIMPORTER=%EXCEPT_IF_VCPKG_IS_BROKEN% ^
    -DWITH_PRIMITIVEIMPORTER=ON ^
    -DWITH_SPIRVTOOLSSHADERCONVERTER=%EXCEPT_MSVC2015% ^
    -DWITH_STANFORDIMPORTER=ON ^
    -DWITH_STANFORDSCENECONVERTER=ON ^
    -DWITH_STBIMAGECONVERTER=ON ^
    -DWITH_STBIMAGEIMPORTER=ON ^
    -DWITH_STBTRUETYPEFONT=ON ^
    -DWITH_STBVORBISAUDIOIMPORTER=ON ^
    -DWITH_STLIMPORTER=ON ^
    -DWITH_TINYGLTFIMPORTER=ON ^
    -DBUILD_TESTS=ON ^
    -DBUILD_GL_TESTS=ON ^
    -DBUILD_STATIC=%BUILD_STATIC% ^
    -DBUILD_PLUGINS_STATIC=%BUILD_STATIC% ^
    %COMPILER_EXTRA% -G Ninja || exit /b
cmake --build . || exit /b
cmake --build . --target install || exit /b

rem Test
set CORRADE_TEST_COLOR=ON
ctest -V || exit /b
