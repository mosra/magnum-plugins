The Basis encoder unconditionally includes code for loading image files from
disk (.png, .exr, etc.), but BasisImageConverter always supplies image data
in memory. So the code path using the disk loading is never run, wasting size
and increasing compile time needlessly.

There's no way to remove it through a preprocessor define so instead all
required functions are provided as empty stubs in stubs.cpp.

The following library sources are stubbed:
jpgd.cpp
apg_bmp.c // Removed in 1.16
lodepng.cpp // Removed in 1.16
pvpngreader.cpp // Added in 1.16
3rdparty/tinyexr.cpp // Added in 1.50
