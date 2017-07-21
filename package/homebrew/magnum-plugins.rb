# kate: indent-width 2;

class MagnumPlugins < Formula
  desc "Plugins for Magnum graphics engine"
  homepage "http://magnum.graphics"
  head "git://github.com/mosra/magnum-plugins.git"

  depends_on "assimp"
  depends_on "cmake"
  depends_on "magnum"
  depends_on "devil"
  depends_on "freetype"
  depends_on "harfbuzz"
  depends_on "libpng"
  depends_on "jpeg"

  def install
    system "mkdir build"
    cd "build" do
      system "cmake", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=#{prefix}", "-DWITH_ANYAUDIOIMPORTER=ON", "-DWITH_ANYIMAGECONVERTER=ON", "-DWITH_ANYIMAGEIMPORTER=ON", "-DWITH_ANYSCENEIMPORTER=ON", "-DWITH_ASSIMPIMPORTER=ON", "-DWITH_DDSIMPORTER=ON", "-DWITH_DEVILIMAGEIMPORTER=ON", "-DWITH_DRFLACAUDIOIMPORTER=ON", "-DWITH_DRWAVAUDIOIMPORTER=ON", "-DWITH_FREETYPEFONT=ON", "-DWITH_HARFBUZZFONT=ON", "-DWITH_JPEGIMPORTER=ON", "-DWITH_MINIEXRIMAGECONVERTER=ON", "-DWITH_OPENGEXIMPORTER=ON", "-DWITH_PNGIMAGECONVERTER=ON", "-DWITH_PNGIMPORTER=ON", "-DWITH_STANFORDIMPORTER=ON", "-DWITH_STBIMAGECONVERTER=ON", "-DWITH_STBIMAGEIMPORTER=ON", "-DWITH_STBTRUETYPEFONT=ON", "-DWITH_STBVORBISAUDIOIMPORTER=ON", ".."
      system "cmake", "--build", "."
      system "cmake", "--build", ".", "--target", "install"
    end
  end
end
