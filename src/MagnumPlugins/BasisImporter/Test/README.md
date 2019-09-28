Creating input Basis files
==========================

The images were converted from central cutouts from `ambient-texture.tga` and `diffuse-alpha-texture.tga`
from the [Magnum Shaders test files](https://github.com/mosra/magnum/tree/master/src/Magnum/Shaders/Test/TestFiles).

- `rgb_63x27.png`,
- `rgba_63x27.png`,
- `rgb_27x63.png`,
- `rgba_27x63.png`,
- `rgb_32x64.png`,
- `rgba_32x64.png`,
- `rgb_64x32.png`,
- `rgba_64x32.png`

using the official basis universal
[conversion tool](https://github.com/BinomialLLC/basis_universal/#command-line-compression-tool).

To convert, run the following commands:

```sh
basisu rgb_63x27.png -output_file rgb.basis -y_flip
basisu rgb_63x27.png -output_file rgb-noflip.basis

basisu rgb_63x27.png rgb_27x63.png -output_file rgb_2_images.basis -y_flip
basisu rgba_63x27.png rgba_27x63.png -output_file rgba_2_images.basis -force_alpha -y_flip

# Required for PVRTC1 target, which requires pow2 dimensions
basisu rgb_64x32.png rgb_32x64.png -output_file rgb_2_images_pow2.basis -y_flip
basisu rgba_64x32.png rgba_32x64.png -output_file rgba_2_images_pow2.basis -force_alpha -y_flip
```
