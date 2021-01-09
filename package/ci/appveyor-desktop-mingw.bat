rem Workaround for CMake not wanting sh.exe on PATH for MinGW. AARGH.
set PATH=%PATH:C:\Program Files\Git\usr\bin;=%
rem Unlike with Magnum itself which copies OpenAL DLL to its output directory,
rem here we need to do that ourselves as we don't have Magnum's build dir in
rem PATH
set PATH=C:\mingw-w64\x86_64-7.2.0-posix-seh-rt_v5-rev1\mingw64\bin;%APPVEYOR_BUILD_FOLDER%/openal/bin/Win64;%APPVEYOR_BUILD_FOLDER%\deps\bin;%APPVEYOR_BUILD_FOLDER%\devil\unicode;%PATH%

rem Build LibJPEG. I spent a FUCKING WEEK trying to get libjpeg-turbo built on
rem GH Actions for the older GCC 7.2 that's used here. NOT FUCKING POSSIBLE.
rem
rem -   When using the default GH Actions mingw setup, the built package fails
rem     to link here with __imp___acrt_iob_func, similarly as described at
rem     https://github.com/rust-lang/rust/issues/47048
rem -   Downgrading a bunch of packages to make GCC 7.2 used on GH Actions is
rem     IMPOSSIBLE as:
rem     -   the "vanilla" cmake installed via pacman fails to recognize the
rem         MSYS platform, and then the GCC crashes
rem     -   the mingw-w64-x86_64-cmake package straight out crashes at runtime
rem         so not usable either
rem -   attempting to install the libjpeg packages here FUCKING FAILS TOO
rem     because
rem     -   they're either too old and 404,
rem     -   or pacman is too old and needs to be manually upgraded to a
rem         zstd-capable version, after which install works
rem     -   but the packages are NOT FOUND, so one has to add c:/msys to
rem         CMAKE_PREFIX_PATH
rem     -   but that makes everything EXPLODE again and suddenly it's using
rem         GCC 9.1 and Corrade installed in home/deps/ isn't found anymore
rem
rem So I'm just downloading and building by hand here. Every damn time.
rem Fortunately at least, Assimp MinGW build from GH Actions works, so we don't
rem need to build ALL THE STUFF, just some. FFS.
IF NOT EXIST %APPVEYOR_BUILD_FOLDER%\libjpeg-turbo-1.5.0.tar.gz appveyor DownloadFile http://downloads.sourceforge.net/project/libjpeg-turbo/1.5.0/libjpeg-turbo-1.5.0.tar.gz || exit /b
7z x libjpeg-turbo-1.5.0.tar.gz || exit /b
7z x libjpeg-turbo-1.5.0.tar || exit /b
ren libjpeg-turbo-1.5.0 libjpeg-turbo || exit /b
cd libjpeg-turbo || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_CXX_FLAGS="--coverage" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DWITH_JPEG8=ON ^
    -DWITH_SIMD=OFF ^
    -G Ninja || exit /b
cmake --build . --target install || exit /b
cd .. && cd .. || exit /b

rem build meshoptimizer
IF NOT EXIST %APPVEYOR_BUILD_FOLDER%\v0.14.zip appveyor DownloadFile https://github.com/zeux/meshoptimizer/archive/v0.14.zip || exit /b
7z x v0.14.zip || exit /b
ren meshoptimizer-0.14 meshoptimizer || exit /b
cd meshoptimizer || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_CXX_FLAGS="--coverage" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -G Ninja || exit /b
cmake --build . --target install || exit /b
cd .. && cd .. || exit /b

rem Build Corrade
git clone --depth 1 git://github.com/mosra/corrade.git || exit /b
cd corrade || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_CXX_FLAGS="--coverage" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DWITH_INTERCONNECT=OFF ^
    -DUTILITY_USE_ANSI_COLORS=ON ^
    -G Ninja || exit /b
cmake --build . || exit /b
cmake --build . --target install || exit /b
cd .. && cd ..

rem Build Magnum
git clone --depth 1 git://github.com/mosra/magnum.git || exit /b
cd magnum || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_CXX_FLAGS="--coverage" ^
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
    -G Ninja || exit /b
cmake --build . || exit /b
cmake --build . --target install || exit /b
cd .. && cd ..

rem Build. BUILD_GL_TESTS is enabled just to be sure, it should not be needed
rem by any plugin.
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_CXX_FLAGS="--coverage" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCMAKE_PREFIX_PATH=%APPVEYOR_BUILD_FOLDER%/openal;%APPVEYOR_BUILD_FOLDER%/devil ^
    -DWITH_ASSIMPIMPORTER=ON ^
    -DWITH_BASISIMAGECONVERTER=ON ^
    -DWITH_BASISIMPORTER=ON -DBASIS_UNIVERSAL_DIR=%APPVEYOR_BUILD_FOLDER%/basis_universal ^
    -DWITH_DDSIMPORTER=ON ^
    -DWITH_DEVILIMAGEIMPORTER=ON ^
    -DWITH_DRFLACAUDIOIMPORTER=ON ^
    -DWITH_DRMP3AUDIOIMPORTER=ON ^
    -DWITH_DRWAVAUDIOIMPORTER=ON ^
    -DWITH_FREETYPEFONT=OFF ^
    -DWITH_GLSLANGSHADERCONVERTER=OFF ^
    -DWITH_HARFBUZZFONT=OFF ^
    -DWITH_ICOIMPORTER=ON ^
    -DWITH_JPEGIMAGECONVERTER=ON ^
    -DWITH_JPEGIMPORTER=ON ^
    -DWITH_MESHOPTIMIZERSCENECONVERTER=ON ^
    -DWITH_MINIEXRIMAGECONVERTER=ON ^
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
    -DWITH_STBVORBISAUDIOIMPORTER=ON ^
    -DWITH_STLIMPORTER=ON ^
    -DWITH_TINYGLTFIMPORTER=ON ^
    -DBUILD_TESTS=ON ^
    -DBUILD_GL_TESTS=ON ^
    -G Ninja || exit /b
cmake --build . || exit /b
cmake --build . --target install || exit /b

rem Test
set CORRADE_TEST_COLOR=ON
ctest -V || exit /b

rem Coverage upload
set PATH=C:\msys64\usr\bin;%PATH%
bash %APPVEYOR_BUILD_FOLDER%\package\ci\appveyor-lcov.sh || exit /b
codecov -f coverage.info -X gcov
