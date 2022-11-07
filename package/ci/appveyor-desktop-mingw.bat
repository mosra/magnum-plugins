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

rem Build meshoptimizer
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

rem Build zstd
IF NOT EXIST %APPVEYOR_BUILD_FOLDER%\v1.5.0.zip appveyor DownloadFile https://github.com/facebook/zstd/archive/refs/tags/v1.5.0.zip || exit /b
7z x v1.5.0.zip || exit /b
ren zstd-1.5.0 zstd || exit /b
cd zstd || exit /b
rem There's already a directory named `build`
mkdir build_ && cd build_ || exit /b
cmake ../build/cmake ^
    -DCMAKE_CXX_FLAGS="--coverage" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DZSTD_BUILD_PROGRAMS=OFF ^
    -DZSTD_BUILD_SHARED=OFF ^
    -DZSTD_BUILD_STATIC=ON ^
    -DZSTD_MULTITHREAD_SUPPORT=OFF ^
    -G Ninja || exit /b
cmake --build . --target install || exit /b
cd .. && cd .. || exit /b

rem Build Corrade
git clone --depth 1 https://github.com/mosra/corrade.git || exit /b
cd corrade || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_CXX_FLAGS="--coverage" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCORRADE_WITH_INTERCONNECT=OFF ^
    -DCORRADE_UTILITY_USE_ANSI_COLORS=ON ^
    -G Ninja || exit /b
cmake --build . || exit /b
cmake --build . --target install || exit /b
cd .. && cd ..

rem Build Magnum
git clone --depth 1 https://github.com/mosra/magnum.git || exit /b
cd magnum || exit /b
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_CXX_FLAGS="--coverage" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCMAKE_PREFIX_PATH=%APPVEYOR_BUILD_FOLDER%/openal ^
    -DMAGNUM_WITH_AUDIO=ON ^
    -DMAGNUM_WITH_DEBUGTOOLS=ON ^
    -DMAGNUM_WITH_GL=OFF ^
    -DMAGNUM_WITH_MATERIALTOOLS=OFF ^
    -DMAGNUM_WITH_MESHTOOLS=ON ^
    -DMAGNUM_WITH_PRIMITIVES=ON ^
    -DMAGNUM_WITH_SCENEGRAPH=OFF ^
    -DMAGNUM_WITH_SCENETOOLS=OFF ^
    -DMAGNUM_WITH_SHADERS=OFF ^
    -DMAGNUM_WITH_TEXT=ON ^
    -DMAGNUM_WITH_TEXTURETOOLS=ON ^
    -DMAGNUM_WITH_ANYIMAGEIMPORTER=ON ^
    -DMAGNUM_WITH_TGAIMAGECONVERTER=ON ^
    -G Ninja || exit /b
cmake --build . || exit /b
cmake --build . --target install || exit /b
cd .. && cd ..

rem Build. MAGNUM_BUILD_GL_TESTS is enabled just to be sure, it should not be
rem needed by any plugin.
mkdir build && cd build || exit /b
cmake .. ^
    -DCMAKE_CXX_FLAGS="--coverage" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%/deps ^
    -DCMAKE_PREFIX_PATH=%APPVEYOR_BUILD_FOLDER%/openal;%APPVEYOR_BUILD_FOLDER%/devil ^
    -DMAGNUM_WITH_ASSIMPIMPORTER=ON ^
    -DMAGNUM_WITH_ASTCIMPORTER=ON ^
    -DMAGNUM_WITH_BASISIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_BASISIMPORTER=ON -DBASIS_UNIVERSAL_DIR=%APPVEYOR_BUILD_FOLDER%/basis_universal ^
    -DMAGNUM_WITH_CGLTFIMPORTER=ON ^
    -DMAGNUM_WITH_DDSIMPORTER=ON ^
    -DMAGNUM_WITH_DEVILIMAGEIMPORTER=ON ^
    -DMAGNUM_WITH_DRFLACAUDIOIMPORTER=ON ^
    -DMAGNUM_WITH_DRMP3AUDIOIMPORTER=ON ^
    -DMAGNUM_WITH_DRWAVAUDIOIMPORTER=ON ^
    -DMAGNUM_WITH_FREETYPEFONT=OFF ^
    -DMAGNUM_WITH_GLSLANGSHADERCONVERTER=OFF ^
    -DMAGNUM_WITH_GLTFIMPORTER=ON ^
    -DMAGNUM_WITH_GLTFSCENECONVERTER=ON ^
    -DMAGNUM_WITH_HARFBUZZFONT=OFF ^
    -DMAGNUM_WITH_ICOIMPORTER=ON ^
    -DMAGNUM_WITH_JPEGIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_JPEGIMPORTER=ON ^
    -DMAGNUM_WITH_KTXIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_KTXIMPORTER=ON ^
    -DMAGNUM_WITH_MESHOPTIMIZERSCENECONVERTER=ON ^
    -DMAGNUM_WITH_MINIEXRIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_OPENEXRIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_OPENEXRIMPORTER=ON ^
    -DMAGNUM_WITH_OPENGEXIMPORTER=ON ^
    -DMAGNUM_WITH_PNGIMAGECONVERTER=OFF ^
    -DMAGNUM_WITH_PNGIMPORTER=OFF ^
    -DMAGNUM_WITH_PRIMITIVEIMPORTER=ON ^
    -DMAGNUM_WITH_SPIRVTOOLSSHADERCONVERTER=OFF ^
    -DMAGNUM_WITH_STANFORDIMPORTER=ON ^
    -DMAGNUM_WITH_STANFORDSCENECONVERTER=ON ^
    -DMAGNUM_WITH_STBDXTIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_STBIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_STBIMAGEIMPORTER=ON ^
    -DMAGNUM_WITH_STBRESIZEIMAGECONVERTER=ON ^
    -DMAGNUM_WITH_STBTRUETYPEFONT=ON ^
    -DMAGNUM_WITH_STBVORBISAUDIOIMPORTER=ON ^
    -DMAGNUM_WITH_STLIMPORTER=ON ^
    -DMAGNUM_WITH_TINYGLTFIMPORTER=ON ^
    -DMAGNUM_WITH_WEBPIMPORTER=OFF ^
    -DMAGNUM_BUILD_TESTS=ON ^
    -DMAGNUM_BUILD_GL_TESTS=ON ^
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
