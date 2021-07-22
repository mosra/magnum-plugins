#!/bin/bash

set -e

# https://github.com/KhronosGroup/KTX-Software v4.0.0

toktx version1.ktx white.png

toktx --t2 --nometadata orientation-empty.ktx2 pattern.png
#toktx --t2 orientation-rd.ktx2 pattern.png
#toktx --t2 --lower_left_maps_to_s0t0 orientation-ru.ktx2 pattern.png

toktx --t2 --swizzle rgb1 swizzle-identity.ktx2 pattern.png
toktx --t2 --target_type RGBA --swizzle rgb1 swizzle-unsupported.ktx2 pattern.png

# swizzled data, same swizzle in header
# data should come out normally after the importer swizzles it
toktx --t2 --input_swizzle bgra --swizzle bgr1 bgr-swizzle-bgr.ktx2 pattern.png
toktx --t2 --target_type RGBA --input_swizzle bgra --swizzle bgra bgra-swizzle-bgra.ktx2 pattern.png

# swizzled header
# with patched swizzled vkFormat data should come out normally because both cancel each other out
toktx --t2 --swizzle bgr1 swizzle-bgr.ktx2 pattern.png
toktx --t2 --target_type RGBA --swizzle bgra swizzle-bgra.ktx2 pattern.png

# swizzled data
# with patched swizzled vkFormat data should come out normally
toktx --t2 --input_swizzle bgra bgr.ktx2 pattern.png
toktx --t2 --target_type RGBA --input_swizzle bgra bgra.ktx2 pattern.png

# 2D
toktx --t2 rgb.ktx2 pattern.png
toktx --t2 --target_type RGBA rgba.ktx2 pattern.png

# manual mipmaps
toktx --t2 --mipmap rgb-mipmaps.ktx2 @@pattern-mips.txt

#1D
toktx --t2 1d.ktx2 pattern1d.png
toktx --t2 --mipmap 1d-mipmaps.ktx2 @@pattern1d-mips.txt

# 3D
# toktx failed to create ktxTexture; KTX error: Operation not allowed in the current state.
#toktx --t2 --depth 3 3d-rgb.ktx2 @@3d-files.txt
#toktx --t2 --depth 1 3d-one-slice.ktx2 pattern.png

# TODO:
# 3D/cube
# arrays of all dimensions
# orientation: 1D l, 2D l, cubemap always rd, 3D
