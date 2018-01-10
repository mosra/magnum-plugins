Here are various plugins for the Magnum C++11/C++14 graphics engine — asset
import and conversion, text rendering and more.

[![Join the chat at https://gitter.im/mosra/magnum](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/mosra/magnum?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Build Status](https://travis-ci.org/mosra/magnum-plugins.svg?branch=master)](https://travis-ci.org/mosra/magnum-plugins)
[![Build Status](https://ci.appveyor.com/api/projects/status/nkdlwaxm2i9d6vpx/branch/master?svg=true)](https://ci.appveyor.com/project/mosra/magnum-plugins/branch/master)
[![Coverage Status](https://coveralls.io/repos/github/mosra/magnum-plugins/badge.svg?branch=master)](https://coveralls.io/github/mosra/magnum-plugins?branch=master)
[![MIT License](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

-   Project homepage — http://magnum.graphics/
-   Documentation — http://doc.magnum.graphics/
-   GitHub project page — https://github.com/mosra/magnum-plugins

SUPPORTED PLATFORMS
===================

-   **Linux** and embedded Linux
-   **Windows**, **Windows RT** (Store/Phone)
-   **macOS**, **iOS**
-   **Android**
-   **Web** ([asm.js](http://asmjs.org/) or [WebAssembly](http://webassembly.org/)),
    through [Emscripten](http://kripken.github.io/emscripten-site/)

BUILDING MAGNUM PLUGINS
=======================

You can either use packaging scripts, which are stored in the
[package/](https://github.com/mosra/magnum-plugins/tree/master/package)
subdirectory, or compile and install everything manually. The building process
is similar to Magnum itself — see the
[building documentation](http://doc.magnum.graphics/magnum/building-plugins.html)
for more comprehensive guide for building, packaging and crosscompiling.

Minimal dependencies
--------------------

-   C++ compiler with good C++11 support. Compilers which are tested to have
    everything needed are **GCC** >= 4.7, **Clang** >= 3.1 and **MSVC** >= 2015.
    On Windows you can also use **MinGW-w64**.
-   **CMake** >= 2.8.12
-   **Corrade**, **Magnum** — The engine itself

Compilation, installation
-------------------------

The libraries can be built and installed using these commands:

```sh
git clone git://github.com/mosra/magnum-plugins && cd magnum-plugins
mkdir -p build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j
make install # sudo may be required
```

None of the libraries are built by default, see CMake options or the full
[building documentation](http://doc.magnum.graphics/magnum/building-plugins.html)
for more information about particular libraries and their dependencies.

Building and running unit tests
-------------------------------

If you want to build also unit tests (which are not built by default), pass
`-DBUILD_TESTS=ON` to CMake. Unit tests use Corrade's TestSuite framework and
can be run using

```sh
ctest --output-on-failure
```

in build directory. Everything should pass ;)

Building documentation
----------------------

The documentation is written using [Doxygen](https://doxygen.org) and is built
as part of of the main Magnum documentation. Clone the projects next to each
other and build the docs using the following:

```sh
cd ../magnum # to the directory where Magnum Doxyfile is
doxygen
```

You might need to create the `build/` directory if it doesn't exist yet.
Resulting HTML documentation will be in the `build/doc/` directory. Snapshot of
the documentation is [also available for online viewing](http://doc.magnum.graphics/).

CONTACT & SUPPORT
=================

If you want to contribute to Magnum, if you spotted a bug, need a feature or
have an awesome idea, you can get a copy of the sources from GitHub and start
right away! There is the already mentioned guide about
[how to download and build Magnum Plugins](http://doc.magnum.graphics/magnum/building-plugins.html)
and also a guide about [coding style and best practices](http://doc.magnum.graphics/magnum/coding-style.html)
which you should follow to keep the library as consistent and maintainable as
possible.

-   Project homepage — http://magnum.graphics/
-   Documentation — http://doc.magnum.graphics/
-   GitHub project page — https://github.com/mosra/magnum-plugins
-   Gitter community chat — https://gitter.im/mosra/magnum
-   E-mail — info@magnum.graphics
-   IRC — join the `#magnum-engine` channel on freenode
-   Google Groups mailing list — magnum-engine@googlegroups.com ([archive](https://groups.google.com/forum/#!forum/magnum-engine))
-   Author's personal Twitter — https://twitter.com/czmosra

See also the Magnum Project [Contact & Support page](http://magnum.graphics/contact/)
for further information.

CREDITS
=======

See the [CREDITS.md](CREDITS.md) file for details. Big thanks to everyone
involved!

LICENSE
=======

Magnum is licensed under the MIT/Expat license, see the [COPYING](COPYING) file
for details.
