### Zstd include path workaround

The `zstd` and `put-this-on-include-path` directories exist in order to make
this Basis Universal insanity

```cpp
#include "../zstd/zstd.h"
```

actually include an external Zstd installation. I.e., an up-to-date version
with all SECURITY FIXES and OPTIMIZATIONS that happened since the original file
was put in the Basis repository in April 2021. The `put-this-on-include-path/`
subdirectory is added to the include path, which then makes `../zstd/zstd.h`
point to the `zstd.h` file in the `zstd/` subdirectory here. That file then
uses `#include <zstd.h>` (with `<>` instead of `""`) to include the external
`zstd.h` instead of the bundled file.

The real fix is in https://github.com/BinomialLLC/basis_universal/pull/228,
waiting to get some attention since December 2021. Haha.

### Image loading stubs

The Basis encoder unconditionally includes code for loading image files from
disk (.png, .exr, etc.), but BasisImageConverter always supplies image data
in memory. So the code path using the disk loading is never run, wasting size
and increasing compile time needlessly.

There's no way to remove it through a preprocessor define so instead all
required functions are provided as empty stubs in image-loading-stubs.cpp.

The issue is tracked at https://github.com/BinomialLLC/basis_universal/issues/269.

The following library sources are stubbed:
jpgd.cpp
apg_bmp.c // Removed in 1.16
lodepng.cpp // Removed in 1.16
pvpngreader.cpp // Added in 1.16
3rdparty/tinyexr.cpp // Added in 1.50
