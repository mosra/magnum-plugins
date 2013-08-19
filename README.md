Here are various plugins for Magnum C++11 OpenGL graphics engine. If you don't
know what Magnum is, see https://github.com/mosra/magnum.

INSTALLATION
============

You can either use packaging scripts, which are stored in `package/`
subdirectory, or compile and install everything manually. The building process
is similar to Magnum itself - see [Magnum documentation](http://mosra.cz/blog/magnum-doc/)
for more comprehensive guide for building, packaging and crosscompiling.

Minimal dependencies
--------------------

*   C++ compiler with good C++11 support. Currently there are two compilers
    which are tested to support everything needed: **GCC** >= 4.6 and **Clang**
    >= 3.1. On Windows you can use **MinGW**, Visual Studio compiler still
    lacks some needed features.
*   **CMake** >= 2.8.8
*   **Magnum** - The engine itself

Compilation, installation
-------------------------

The plugins can be built and installed using these four commands:

    mkdir -p build && cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr ..
    make
    make install

None of the plugins is built by default, see Doxygen documentation for more
information about particular plugins and their sdependencies.

Building and running unit tests
-------------------------------

If you want to build also unit tests (which are not built by default), pass
`-DBUILD_TESTS=ON` to CMake. Unit tests use Corrade's TestSuite framework and
can be run using

    ctest --output-on-failure

in build directory. Everything should pass ;-)

CONTACT
=======

Want to learn more about the library? Found a bug or want to tell me an awesome
idea? Feel free to visit my website or contact me at:

*   Website - http://mosra.cz/blog/magnum.php
*   GitHub - https://github.com/mosra/magnum-plugins
*   Twitter - https://twitter.com/czmosra
*   E-mail - mosra@centrum.cz
*   Jabber - mosra@jabbim.cz

LICENSE
=======

Magnum is licensed under MIT/Expat license, see [COPYING](COPYING) file for
details.
