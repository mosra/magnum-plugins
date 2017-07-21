# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

EGIT_REPO_URI="git://github.com/mosra/magnum-plugins.git"

inherit cmake-utils git-r3

DESCRIPTION="Plugins for Magnum OpenGL graphics engine"
HOMEPAGE="http://magnum.graphics"

LICENSE="MIT"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE=""

RDEPEND="
	dev-libs/magnum
	dev-qt/qtcore
	media-libs/devil
	media-libs/freetype
	media-libs/harfbuzz
	virtual/jpeg
	media-libs/libpng
	media-libs/assimp
"
DEPEND="${RDEPEND}"

src_configure() {
	# general configuration
	local mycmakeargs=(
		-DCMAKE_INSTALL_PREFIX="${EPREFIX}/usr"
		-DCMAKE_BUILD_TYPE=Release
		-DWITH_ANYAUDIOIMPORTER=ON
		-DWITH_ANYIMAGECONVERTER=ON
		-DWITH_ANYIMAGEIMPORTER=ON
		-DWITH_ANYSCENEIMPORTER=ON
		-DWITH_ASSIMPIMPORTER=ON
		-DWITH_COLLADAIMPORTER=ON
		-DWITH_DDSIMPORTER=ON
		-DWITH_DEVILIMAGEIMPORTER=ON
		-DWITH_DRFLACAUDIOIMPORTER=ON
		-DWITH_DRWAVAUDIOIMPORTER=ON
		-DWITH_FREETYPEFONT=ON
		-DWITH_HARFBUZZFONT=ON
		-DWITH_JPEGIMPORTER=ON
		-DWITH_MINIEXRIMAGECONVERTER=ON
		-DWITH_OPENGEXIMPORTER=ON
		-DWITH_PNGIMAGECONVERTER=ON
		-DWITH_PNGIMPORTER=ON
		-DWITH_STANFORDIMPORTER=ON
		-DWITH_STBIMAGECONVERTER=ON
		-DWITH_STBIMAGEIMPORTER=ON
		-DWITH_STBTRUETYPEFONT=ON
		-DWITH_STBVORBISAUDIOIMPORTER=ON
	)
	cmake-utils_src_configure
}

# kate: replace-tabs off;
