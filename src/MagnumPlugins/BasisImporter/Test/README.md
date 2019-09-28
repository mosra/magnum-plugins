Creating input Basis files
==========================

The images were converted from central cutouts from `ambient-texture.tga` and `diffuse-alpha-texture.tga`
from the [Magnum Shaders test files](https://github.com/mosra/magnum/tree/master/src/Magnum/Shaders/Test/TestFiles).

- `rgb-63x27.png`,
- `rgba-63x27.png`,
- `rgb-27x63.png`,
- `rgba-27x63.png`,
- `rgb-32x64.png`,
- `rgba-32x64.png`,
- `rgb-64x32.png`,
- `rgba-64x32.png`

using the official basis universal
[conversion tool](https://github.com/BinomialLLC/basis_universal/#command-line-compression-tool).

To convert, run the following commands:

```sh
basisu rgb-63x27.png -output_file rgb.basis -y_flip
basisu rgb-63x27.png -output_file rgb-noflip.basis

basisu rgb-63x27.png rgb-27x63.png -output_file rgb-2images.basis -y_flip
basisu rgba-63x27.png rgba-27x63.png -output_file rgba-2images.basis -force_alpha -y_flip

# Required for PVRTC1 target, which requires pow2 dimensions
basisu rgb-64x32.png rgb-32x64.png -output_file rgb-2images-pow2.basis -y_flip
basisu rgba-64x32.png rgba-32x64.png -output_file rgba-2images-pow2.basis -force_alpha -y_flip
```
