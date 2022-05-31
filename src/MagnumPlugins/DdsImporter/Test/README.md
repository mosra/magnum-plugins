Creating input files
====================

The uncompressed 2D files were originally exported from GIMP and then
hand-edited to have different formats or channel masks, and to some a DXT10
header was added.

Ground truth cubemap and array files were created using the legacy
[NVidia Texture Tools](https://github.com/castano/nvidia-texture-tools). The
compressed files were then converted from source PNGs in `BasisImporter/Test`
and from cubemap/array files using
[AMD Compressonator](https://github.com/GPUOpen-Tools/compressonator), the
weird ASTC DDS files then with the proprietary
[NVidia Texture Tools Exporter](https://developer.nvidia.com/nvidia-texture-tools-exporter).
See the `convert.sh` script.
