class MagnumPlugins < Formula
  desc "Plugins for the Magnum C++11/C++14 graphics engine"
  homepage "https://magnum.graphics"
  url "https://github.com/mosra/magnum-plugins/archive/v2020.06.tar.gz"
  # wget https://github.com/mosra/magnum-plugins/archive/v2020.06.tar.gz -O - | sha256sum
  sha256 "8650cab43570c826d2557d5b42459150d253316f7f734af8b3e7d0883510b40a"
  head "git://github.com/mosra/magnum-plugins.git"

  depends_on "assimp" => :recommended
  depends_on "cmake" => :build
  depends_on "magnum"
  depends_on "devil" => :optional
  depends_on "faad2" => :optional
  depends_on "freetype" => :recommended
  depends_on "glslang" => :recommended
  # While harfbuzz is a nice and tidy small lib, Homebrew is stupid and makes
  # harfbuzz depend on cairo, which is needed just for the hb-view tool which
  # NOBODY EVER USES, and cairo itself depends on a ton of other stuff.
  depends_on "harfbuzz" => :optional
  depends_on "libpng" => :recommended
  depends_on "jpeg" => :recommended
  depends_on "openexr" => :recommended
  depends_on "spirv-tools" => :recommended

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

    system "mkdir build"
    cd "build" do
      system "cmake",
        *std_cmake_args,
        "-DWITH_ASSIMPIMPORTER=#{(build.with? 'assimp') ? 'ON' : 'OFF'}",
        "-DWITH_BASISIMAGECONVERTER=ON",
        "-DWITH_BASISIMPORTER=ON",
        "-DWITH_DDSIMPORTER=ON",
        "-DWITH_DEVILIMAGEIMPORTER=#{(build.with? 'devil') ? 'ON' : 'OFF'}",
        "-DWITH_DRFLACAUDIOIMPORTER=ON",
        "-DWITH_DRMP3AUDIOIMPORTER=ON",
        "-DWITH_DRWAVAUDIOIMPORTER=ON",
        "-DWITH_FAAD2AUDIOIMPORTER=#{(build.with? 'faad2') ? 'ON' : 'OFF'}",
        "-DWITH_FREETYPEFONT=#{(build.with? 'freetype') ? 'ON' : 'OFF'}",
        "-DWITH_GLSLANGSHADERCONVERTER=#{(build.with? 'glslang') ? 'ON' : 'OFF'}",
        "-DWITH_HARFBUZZFONT=#{(build.with? 'harfbuzz') ? 'ON' : 'OFF'}",
        "-DWITH_JPEGIMAGECONVERTER=#{(build.with? 'jpeg') ? 'ON' : 'OFF'}",
        "-DWITH_JPEGIMPORTER=#{(build.with? 'jpeg') ? 'ON' : 'OFF'}",
        "-DWITH_MESHOPTIMIZERSCENECONVERTER=ON",
        "-DWITH_MINIEXRIMAGECONVERTER=ON",
        "-DWITH_OPENEXRIMAGECONVERTER=#{(build.with? 'openexr') ? 'ON' : 'OFF'}",
        "-DWITH_OPENEXRIMPORTER=#{(build.with? 'openexr') ? 'ON' : 'OFF'}",
        "-DWITH_OPENGEXIMPORTER=ON",
        "-DWITH_PNGIMAGECONVERTER=#{(build.with? 'libpng') ? 'ON' : 'OFF'}",
        "-DWITH_PNGIMPORTER=#{(build.with? 'libpng') ? 'ON' : 'OFF'}",
        "-DWITH_PRIMITIVEIMPORTER=ON",
        "-DWITH_SPIRVTOOLSSHADERCONVERTER=#{(build.with? 'spirv-tools') ? 'ON' : 'OFF'}",
        "-DWITH_STANFORDIMPORTER=ON",
        "-DWITH_STANFORDSCENECONVERTER=ON",
        "-DWITH_STBIMAGECONVERTER=ON",
        "-DWITH_STBIMAGEIMPORTER=ON",
        "-DWITH_STBTRUETYPEFONT=ON",
        "-DWITH_STBVORBISAUDIOIMPORTER=ON",
        "-DWITH_STLIMPORTER=ON",
        "-DWITH_TINYGLTFIMPORTER=ON",
        ".."
      system "cmake", "--build", "."
      system "cmake", "--build", ".", "--target", "install"
    end
  end
end
