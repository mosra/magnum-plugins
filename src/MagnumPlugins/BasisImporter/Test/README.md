Creating input Basis files
==========================

The images were converted from central cutouts from `ambient-texture.tga` and `diffuse-alpha-texture.tga`
from the [Magnum Shaders test files](https://github.com/mosra/magnum/tree/master/src/Magnum/Shaders/Test/TestFiles).

-   `rgb-63x27.png`
-   `rgb-64x32.png`
-   `rgba-27x63.png`
-   `rgba-63x27.png`
-   `rgba-64x32.png`

using the official basis universal
[conversion tool](https://github.com/BinomialLLC/basis_universal/#command-line-compression-tool).

To convert, run the following commands:

```sh
basisu rgb-63x27.png -output_file rgb.basis -y_flip
basisu rgb-63x27.png -output_file rgb-noflip.basis
basisu rgba-63x27.png -output_file rgba.basis -force_alpha -y_flip
basisu rgba-63x27.png rgba-27x63.png -output_file rgba-2images-mips.basis -y_flip -mipmap -mip_smallest 16 -mip_filter box

# Required for PVRTC1 target, which requires pow2 dimensions
basisu rgb-64x32.png -output_file rgb-pow2.basis -y_flip
basisu rgba-64x32.png -output_file rgba-pow2.basis -force_alpha -y_flip
```

For mipmap testing, the PNG image is resized to two more levels. Using the
box filter, the same as Basis itself, to have the least difference.

```sh
convert rgba-63x27.png -filter box -resize 31x13\! rgba-31x13.png
convert rgba-63x27.png -filter box -resize 15x6\! rgba-15x6.png
pngcrush -ow rgba-31x13.png
pngcrush -ow rgba-15x6.png
```
