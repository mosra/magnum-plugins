class MagnumPlugins < Formula
  desc "Plugins for the Magnum C++11/C++14 graphics engine"
  homepage "https://magnum.graphics"
  url "https://github.com/mosra/magnum-plugins/archive/v2020.06.tar.gz"
  # wget https://github.com/mosra/magnum-plugins/archive/v2020.06.tar.gz -O - | sha256sum
  sha256 "8650cab43570c826d2557d5b42459150d253316f7f734af8b3e7d0883510b40a"
  head "https://github.com/mosra/magnum-plugins.git"

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
  # For Basis Universal. If found, Basis will use it, if not, it'll use a
  # bundled copy.
  depends_on "zstd" => :recommended
  # No idea why WebP has to depend on libpng, libjpeg, libtiff and giflib BY
  # DEFAULT but whatever. BLOAT BLOAT
  depends_on "webp" => :recommended

  def install
    # Bundle Basis Universal, v1_15_update2 for HEAD builds, a commit that's
    # before the UASTC support (which was not implemented yet) on 2020.06.
    # The repo has massive useless files in its history, so we're downloading
    # just a snapshot instead of a git clone. Also, WHY THE FUCK curl needs -L
    # and -o?! why can't it just work?!
    if build.head?
      system "curl", "-L", "https://github.com/BinomialLLC/basis_universal/archive/v1_15_update2.tar.gz", "-o", "src/external/basis-universal.tar.gz"
    else
      system "curl", "-L", "https://github.com/BinomialLLC/basis_universal/archive/2f43afcc97d0a5dafdb73b4e24e123cf9687a418.tar.gz", "-o", "src/external/basis-universal.tar.gz"
    end
    cd "src/external" do
      system "mkdir", "basis-universal"
      system "tar", "xzvf", "basis-universal.tar.gz", "-C", "basis-universal", "--strip-components=1"
    end

    # Bundle meshoptimizer. 0.16 for HEAD builds, 0.14 + a commit that fixes
    # the build on old Apple Clang versions on 2020.06:
    # https://github.com/zeux/meshoptimizer/pull/130
    if build.head?
      system "curl", "-L", "https://github.com/zeux/meshoptimizer/archive/refs/tags/v0.16.tar.gz", "-o", "src/external/meshoptimizer.tar.gz"
    else
      system "curl", "-L", "https://github.com/zeux/meshoptimizer/archive/97c52415c6d29f297a76482ddde22f739292446d.tar.gz", "-o", "src/external/meshoptimizer.tar.gz"
    end
    cd "src/external" do
      system "mkdir", "meshoptimizer"
      system "tar", "xzvf", "meshoptimizer.tar.gz", "-C", "meshoptimizer", "--strip-components=1"
    end

    # 2020.06 has the options unprefixed, current master has them prefixed.
    # Options not present in 2020.06 are prefixed always.
    option_prefix = build.head? ? 'MAGNUM_' : ''

    system "mkdir build"
    cd "build" do
      system "cmake",
        *std_cmake_args,
        # Without this, ARM builds will try to look for dependencies in
        # /usr/local/lib and /usr/lib (which are the default locations) instead
        # of /opt/homebrew/lib which is dedicated for ARM binaries. Please
        # complain to Homebrew about this insane non-obvious filesystem layout.
        "-DCMAKE_INSTALL_NAME_DIR:STRING=#{lib}",
        "-D#{option_prefix}WITH_ASSIMPIMPORTER=#{(build.with? 'assimp') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_ASTCIMPORTER=ON",
        "-D#{option_prefix}WITH_BASISIMAGECONVERTER=ON",
        "-D#{option_prefix}WITH_BASISIMPORTER=ON",
        "-DMAGNUM_WITH_CGLTFIMPORTER=ON",
        "-D#{option_prefix}WITH_DDSIMPORTER=ON",
        "-D#{option_prefix}WITH_DEVILIMAGEIMPORTER=#{(build.with? 'devil') ? 'ON' : 'OFF'}",
        "-D#{option_prefix}WITH_DRFLACAUDIOIMPORTER=ON",
        "-D#{option_prefix}WITH_DRMP3AUDIOIMPORTER=ON",
        "-D#{option_prefix}WITH_DRWAVAUDIOIMPORTER=ON",
        "-D#{option_prefix}WITH_FAAD2AUDIOIMPORTER=#{(build.with? 'faad2') ? 'ON' : 'OFF'}",
        "-D#{option_prefix}WITH_FREETYPEFONT=#{(build.with? 'freetype') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_GLSLANGSHADERCONVERTER=#{(build.with? 'glslang') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_GLTFIMPORTER=ON",
        "-DMAGNUM_WITH_GLTFSCENECONVERTER=ON",
        "-D#{option_prefix}WITH_HARFBUZZFONT=#{(build.with? 'harfbuzz') ? 'ON' : 'OFF'}",
        "-D#{option_prefix}WITH_JPEGIMAGECONVERTER=#{(build.with? 'jpeg') ? 'ON' : 'OFF'}",
        "-D#{option_prefix}WITH_JPEGIMPORTER=#{(build.with? 'jpeg') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_KTXIMAGECONVERTER=ON",
        "-DMAGNUM_WITH_KTXIMAGEIMPORTER=ON",
        "-DMAGNUM_WITH_MESHOPTIMIZERSCENECONVERTER=ON",
        "-DMAGNUM_WITH_MINIEXRIMAGECONVERTER=ON",
        "-DMAGNUM_WITH_OPENEXRIMAGECONVERTER=#{(build.with? 'openexr') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_OPENEXRIMPORTER=#{(build.with? 'openexr') ? 'ON' : 'OFF'}",
        "-D#{option_prefix}WITH_OPENGEXIMPORTER=ON",
        "-D#{option_prefix}WITH_PNGIMAGECONVERTER=#{(build.with? 'libpng') ? 'ON' : 'OFF'}",
        "-D#{option_prefix}WITH_PNGIMPORTER=#{(build.with? 'libpng') ? 'ON' : 'OFF'}",
        "-D#{option_prefix}WITH_PRIMITIVEIMPORTER=ON",
        "-DMAGNUM_WITH_SPIRVTOOLSSHADERCONVERTER=#{(build.with? 'spirv-tools') ? 'ON' : 'OFF'}",
        "-D#{option_prefix}WITH_STANFORDIMPORTER=ON",
        "-D#{option_prefix}WITH_STANFORDSCENECONVERTER=ON",
        "-DMAGNUM_WITH_STBDXTIMAGECONVERTER=ON",
        "-D#{option_prefix}WITH_STBIMAGECONVERTER=ON",
        "-D#{option_prefix}WITH_STBIMAGEIMPORTER=ON",
        "-DMAGNUM_WITH_STBRESIZEIMAGECONVERTER=ON",
        "-D#{option_prefix}WITH_STBTRUETYPEFONT=ON",
        "-D#{option_prefix}WITH_STBVORBISAUDIOIMPORTER=ON",
        "-D#{option_prefix}WITH_STLIMPORTER=ON",
        "-D#{option_prefix}WITH_TINYGLTFIMPORTER=ON",
        "-DMAGNUM_WITH_WEBPIMPORTER=#{(build.with? 'webp') ? 'ON' : 'OFF'}",
        ".."
      system "cmake", "--build", "."
      system "cmake", "--build", ".", "--target", "install"
    end
  end
end
