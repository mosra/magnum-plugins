Here are various plugins for Magnum OpenGL 3 graphics engine. If you don't
know what Magnum is, see https://github.com/mosra/magnum . Featured plugins:

 * Importer for non-paletted TGA images
 * Importer for COLLADA files

INSTALLATION
============

You can either use packaging scripts, which are stored in package/
subdirectory, or compile and install everything manually. The building
process is similar to Magnum itself - see Magnum documentation for more
comprehensive guide for building, packaging and crosscompiling.

Minimal dependencies
--------------------

 * C++ compiler with good C++11 support. Currently the only compiler which
   supports everything needed is **GCC** >= 4.6. **Clang** >= 3.1 (from SVN)
   will probably work too.
 * **CMake** >= 2.8.8 (needed for `OBJECT` library target)
 * **OpenGL headers**, on Linux most probably shipped with Mesa
 * **GLEW** - OpenGL extension wrangler
 * **Magnum** - The engine itself
 * **Qt** >= 4.6 - COLLADA importer needs it for XML parsing

Compilation, installation
-------------------------

The plugins can be built and installed using these four commands:

    mkdir -p build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr .. && make
    make install

Building and running unit tests
-------------------------------

If you want to build also unit tests (which are not built by default), pass
`-DBUILD_TESTS=True` to CMake. Unit tests use QtTest framework and can be run
using

    ctest -V

in build directory. Everything should pass ;-)

CONTACT
=======

Want to learn more about the library? Found a bug or want to tell me an
awesome idea? Feel free to visit my website or contact me at:

 * Website - http://mosra.cz/blog/
 * GitHub - https://github.com/mosra/magnum-plugins
 * E-mail - mosra@centrum.cz
 * Jabber - mosra@jabbim.cz
