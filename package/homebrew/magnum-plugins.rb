class MagnumPlugins < Formula
  desc "Plugins for the Magnum C++11/C++14 graphics engine"
  homepage "https://magnum.graphics"
  url "https://github.com/mosra/magnum-plugins/archive/v2019.10.tar.gz"
  # wget https://github.com/mosra/magnum-plugins/archive/v2019.10.tar.gz -O - | sha256sum
  sha256 "d475e7d153a926e3194f9f7f7716e9530214e35b829438f8900c5c0c84741d03"
  head "git://github.com/mosra/magnum-plugins.git"

  depends_on "assimp"
  depends_on "cmake"
  depends_on "magnum"
  depends_on "devil"
  depends_on "faad2"
  depends_on "freetype"
  depends_on "harfbuzz"
  depends_on "libpng"
  depends_on "jpeg"

  def install
    # Bundle Basis Universal, a commit that's before the UASTC support (which
    # is not implemented yet). The repo has massive useless files in its
    # history, so we're downloading just a snapshot instead of a git clone.
    # Also, WHY THE FUCK curl needs -L and -o?! why can't it just work?!
    system "curl", "-L", "https://github.com/BinomialLLC/basis_universal/archive/2f43afcc97d0a5dafdb73b4e24e123cf9687a418.tar.gz", "-o", "src/external/basis-universal.tar.gz"
    cd "src/external" do
      system "mkdir", "basis-universal"
      system "tar", "xzvf", "basis-universal.tar.gz", "-C", "basis-universal", "--strip-components=1"
    end

    # Bundle meshoptimizer 0.14 + a commit that fixes the build on old Apple
    # Clang versions: https://github.com/zeux/meshoptimizer/pull/130
    system "curl", "-L", "https://github.com/zeux/meshoptimizer/archive/97c52415c6d29f297a76482ddde22f739292446d.tar.gz", "-o", "src/external/meshoptimizer.tar.gz"
    cd "src/external" do
      system "mkdir", "meshoptimizer"
      system "tar", "xzvf", "meshoptimizer.tar.gz", "-C", "meshoptimizer", "--strip-components=1"
    end

    # Temporarily disable Basis on 2019.10 because there it doesn't understand
    # the bundled location yet and would fail. MeshOptimizer wasn't there yet
    # so that's okay; we don't care that the sources are downloaded above but
    # unused. TODO: remove when next stable is out
    basis = head? ? "ON" : "OFF"

    system "mkdir build"
    cd "build" do
      system "cmake", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=#{prefix}", "-DWITH_ASSIMPIMPORTER=ON", "-DWITH_BASISIMAGECONVERTER=#{basis}", "-DWITH_BASISIMPORTER=#{basis}", "-DWITH_DDSIMPORTER=ON", "-DWITH_DEVILIMAGEIMPORTER=ON", "-DWITH_DRFLACAUDIOIMPORTER=ON", "-DWITH_DRMP3AUDIOIMPORTER=ON", "-DWITH_DRWAVAUDIOIMPORTER=ON", "-DWITH_FAAD2AUDIOIMPORTER=ON", "-DWITH_FREETYPEFONT=ON", "-DWITH_HARFBUZZFONT=ON", "-DWITH_JPEGIMAGECONVERTER=ON", "-DWITH_JPEGIMPORTER=ON", "-DWITH_MESHOPTIMIZERSCENECONVERTER=ON", "-DWITH_MINIEXRIMAGECONVERTER=ON", "-DWITH_OPENGEXIMPORTER=ON", "-DWITH_PNGIMAGECONVERTER=ON", "-DWITH_PNGIMPORTER=ON", "-DWITH_PRIMITIVEIMPORTER=ON", "-DWITH_STANFORDIMPORTER=ON", "-DWITH_STANFORDSCENECONVERTER=ON", "-DWITH_STBIMAGECONVERTER=ON", "-DWITH_STBIMAGEIMPORTER=ON", "-DWITH_STBTRUETYPEFONT=ON", "-DWITH_STBVORBISAUDIOIMPORTER=ON", "-DWITH_STLIMPORTER=ON", "-DWITH_TINYGLTFIMPORTER=ON", ".."
      system "cmake", "--build", "."
      system "cmake", "--build", ".", "--target", "install"
    end
  end
end
