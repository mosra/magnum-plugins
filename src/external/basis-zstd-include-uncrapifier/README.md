These directories exist in order to make this Basis Universal insanity

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
