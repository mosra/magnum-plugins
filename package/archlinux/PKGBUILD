# Author: mosra <mosra@centrum.cz>
pkgname=magnum-plugins
pkgver=dev
pkgrel=1
pkgdesc="Plugins for Magnum OpenGL graphics engine"
arch=('i686' 'x86_64')
url="http://mosra.cz/blog/magnum.php"
license=('MIT')
depends=('magnum')
makedepends=('cmake' 'ninja')
options=(!strip)
provides=('magnum-plugins-git')

build() {
    mkdir -p "$startdir/build"
    cd "$startdir/build/"

    cmake .. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DBUILD_TESTS=TRUE \
        -G Ninja
    ninja
}

check() {
    cd "$startdir/build"
    ctest --output-on-failure
}

package() {
  cd "$startdir/build"
  DESTDIR="$pkgdir/" ninja install
}
