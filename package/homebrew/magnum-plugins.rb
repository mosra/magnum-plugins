class MagnumPlugins < Formula
  desc "Plugins for the Magnum C++11 graphics engine"
  homepage "https://magnum.graphics"
  # git describe origin/master, except the `v` prefix
  version "2020.06-1658-gb77a583aa"
  # Clone instead of getting an archive to have tags for version.h generation
  url "https://github.com/mosra/magnum-plugins.git", revision: "b77a583aa"
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
  depends_on "libspng" => :recommended
  depends_on "jpeg" => :recommended
  depends_on "openexr" => :recommended
  depends_on "spirv-tools" => :recommended
  # For Basis Universal. If found, Basis will use it, if not, it'll be without
  # UASTC support.
  depends_on "zstd" => :recommended
  # No idea why WebP has to depend on libpng, libjpeg, libtiff and giflib BY
  # DEFAULT but whatever. BLOAT BLOAT
  depends_on "webp" => :recommended
  # Resvg is Rust, not sure what all it installs if enabled. So make it off by
  # default.
  depends_on "resvg" => :optional

  def install
    # Bundle Basis Universal. The repo has massive useless files in its
    # history, so we're downloading just a snapshot instead of a git clone.
    # Also, WHY THE FUCK curl needs -L and -o?! why can't it just work?!
    system "curl", "-L", "https://github.com/BinomialLLC/basis_universal/archive/v1_50_0_2.tar.gz", "-o", "src/external/basis-universal.tar.gz"
    cd "src/external" do
      system "mkdir", "basis-universal"
      system "tar", "xzvf", "basis-universal.tar.gz", "-C", "basis-universal", "--strip-components=1"
    end

    # Bundle meshoptimizer
    system "curl", "-L", "https://github.com/zeux/meshoptimizer/archive/refs/tags/v0.20.tar.gz", "-o", "src/external/meshoptimizer.tar.gz"
    cd "src/external" do
      system "mkdir", "meshoptimizer"
      system "tar", "xzvf", "meshoptimizer.tar.gz", "-C", "meshoptimizer", "--strip-components=1"
    end

    system "mkdir build"
    cd "build" do
      system "cmake",
        *std_cmake_args,
        # Without this, ARM builds will try to look for dependencies in
        # /usr/local/lib and /usr/lib (which are the default locations) instead
        # of /opt/homebrew/lib which is dedicated for ARM binaries. Please
        # complain to Homebrew about this insane non-obvious filesystem layout.
        "-DCMAKE_INSTALL_NAME_DIR:STRING=#{lib}",
        "-DMAGNUM_WITH_ASSIMPIMPORTER=#{(build.with? 'assimp') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_ASTCIMPORTER=ON",
        "-DMAGNUM_WITH_BASISIMAGECONVERTER=ON",
        "-DMAGNUM_WITH_BASISIMPORTER=ON",
        "-DMAGNUM_WITH_BCDECIMAGECONVERTER=ON",
        "-DMAGNUM_WITH_CGLTFIMPORTER=ON",
        "-DMAGNUM_WITH_DDSIMPORTER=ON",
        "-DMAGNUM_WITH_DEVILIMAGEIMPORTER=#{(build.with? 'devil') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_DRFLACAUDIOIMPORTER=ON",
        "-DMAGNUM_WITH_DRMP3AUDIOIMPORTER=ON",
        "-DMAGNUM_WITH_DRWAVAUDIOIMPORTER=ON",
        "-DMAGNUM_WITH_ETCDECIMAGECONVERTER=ON",
        "-DMAGNUM_WITH_FAAD2AUDIOIMPORTER=#{(build.with? 'faad2') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_FREETYPEFONT=#{(build.with? 'freetype') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_GLSLANGSHADERCONVERTER=#{(build.with? 'glslang') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_GLTFIMPORTER=ON",
        "-DMAGNUM_WITH_GLTFSCENECONVERTER=ON",
        "-DMAGNUM_WITH_HARFBUZZFONT=#{(build.with? 'harfbuzz') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_JPEGIMAGECONVERTER=#{(build.with? 'jpeg') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_JPEGIMPORTER=#{(build.with? 'jpeg') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_KTXIMAGECONVERTER=ON",
        "-DMAGNUM_WITH_KTXIMPORTER=ON",
        "-DMAGNUM_WITH_LUNASVGIMPORTER=OFF",
        "-DMAGNUM_WITH_MESHOPTIMIZERSCENECONVERTER=ON",
        "-DMAGNUM_WITH_MINIEXRIMAGECONVERTER=ON",
        "-DMAGNUM_WITH_OPENEXRIMAGECONVERTER=#{(build.with? 'openexr') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_OPENEXRIMPORTER=#{(build.with? 'openexr') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_OPENGEXIMPORTER=ON",
        "-DMAGNUM_WITH_PLUTOSVGIMPORTER=OFF",
        "-DMAGNUM_WITH_PNGIMAGECONVERTER=#{(build.with? 'libpng') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_PNGIMPORTER=#{(build.with? 'libpng') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_PRIMITIVEIMPORTER=ON",
        "-DMAGNUM_WITH_RESVGIMPORTER=#{(build.with? 'resvg') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_SPIRVTOOLSSHADERCONVERTER=#{(build.with? 'spirv-tools') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_SPNGIMPORTER=#{(build.with? 'libspng') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_STANFORDIMPORTER=ON",
        "-DMAGNUM_WITH_STANFORDSCENECONVERTER=ON",
        "-DMAGNUM_WITH_STBDXTIMAGECONVERTER=ON",
        "-DMAGNUM_WITH_STBIMAGECONVERTER=ON",
        "-DMAGNUM_WITH_STBIMAGEIMPORTER=ON",
        "-DMAGNUM_WITH_STBRESIZEIMAGECONVERTER=ON",
        "-DMAGNUM_WITH_STBTRUETYPEFONT=ON",
        "-DMAGNUM_WITH_STBVORBISAUDIOIMPORTER=ON",
        "-DMAGNUM_WITH_STLIMPORTER=ON",
        "-DMAGNUM_WITH_TINYGLTFIMPORTER=ON",
        "-DMAGNUM_WITH_UFBXIMPORTER=ON",
        "-DMAGNUM_WITH_WEBPIMAGECONVERTER=#{(build.with? 'webp') ? 'ON' : 'OFF'}",
        "-DMAGNUM_WITH_WEBPIMPORTER=#{(build.with? 'webp') ? 'ON' : 'OFF'}",
        ".."
      system "cmake", "--build", "."
      system "cmake", "--build", ".", "--target", "install"
    end
  end
end
