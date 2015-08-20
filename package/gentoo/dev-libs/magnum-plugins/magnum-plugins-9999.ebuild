# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

EGIT_REPO_URI="git://github.com/mosra/magnum-plugins.git"

inherit cmake-utils git-r3

DESCRIPTION="Plugins for Magnum OpenGL graphics engine"
HOMEPAGE="http://mosra.cz/blog/magnum.php"

LICENSE="MIT"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE=""

RDEPEND="
	dev-libs/magnum
	dev-qt/qtcore
	media-libs/freetype
	media-libs/harfbuzz
	virtual/jpeg
	media-libs/libpng
"
DEPEND="${RDEPEND}"

src_configure() {
	# general configuration
	local mycmakeargs=(
		-DCMAKE_INSTALL_PREFIX="${EPREFIX}/usr"
		-DCMAKE_BUILD_TYPE=Release
		-DWITH_ANYIMAGEIMPORTER=ON
		-DWITH_ANYSCENEIMPORTER=ON
		-DWITH_COLLADAIMPORTER=ON
		-DWITH_DDSIMPORTER=ON
		-DWITH_FREETYPEFONT=ON
		-DWITH_HARFBUZZFONT=ON
		-DWITH_JPEGIMPORTER=ON
		-DWITH_OPENGEXIMPORTER=ON
		-DWITH_PNGIMPORTER=ON
		-DWITH_STANFORDIMPORTER=ON
		-DWITH_STBIMAGEIMPORTER=ON
		-DWITH_STBPNGIMAGECONVERTER=ON
	)
	cmake-utils_src_configure
}

# kate: replace-tabs off;
