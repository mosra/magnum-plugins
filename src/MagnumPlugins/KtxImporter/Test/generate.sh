#!/bin/bash

set -e

# The Khronos tools don't support array, cubemap or 3D textures, so we need
# PVRTexTool as well. That doesn't support 3D textures, but we can patch the
# header of 2D array textures to turn them into 3D textures.
# Not a fan of using closed-source tools, but it works...
# https://github.com/KhronosGroup/KTX-Software v4.0.0
# https://developer.imaginationtech.com/pvrtextool/ v5.1.0

toktx version1.ktx black.png

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
toktx --t2 2d-rgb.ktx2 pattern.png
toktx --t2 --target_type RGBA 2d-rgba.ktx2 pattern.png

# manual mipmaps, full pyramid and one without the last
toktx --t2 --mipmap 2d-mipmaps.ktx2 pattern.png pattern-mip1.png pattern-mip2.png
toktx --t2 --mipmap --levels 2 2d-mipmaps-incomplete.ktx2 pattern.png pattern-mip1.png

# 2D layers
# output is an image with numLayers=0, wtf
#toktx --t2 --layers 3 2d-layers.ktx2 pattern.png pattern.png black.png
PVRTexToolCLI -i pattern.png,pattern.png,black.png -o 2d-layers.ktx2 -array -f r8g8b8,UBN,sRGB

# cube map
# toktx failed to create ktxTexture; KTX error: Operation not allowed in the current state.
#toktx --t2 --cubemap cube.ktx2 pattern.png pattern.png pattern.png pattern.png pattern.png pattern.png
PVRTexToolCLI -i cube+x.png,cube-x.png,cube+y.png,cube-y.png,cube+z.png,cube-z.png -o cubemap.ktx2 -cube -f r8g8b8,UBN,sRGB
PVRTexToolCLI -i cube+x.png,cube-x.png,cube+y.png,cube-y.png,cube+z.png,cube-z.png -o cubemap-mipmaps.ktx2 -cube -m -mfilter linear -f r8g8b8,UBN,sRGB
# faces for layer 0, then layer 1
PVRTexToolCLI -i cube+x.png,cube-x.png,cube+y.png,cube-y.png,cube+z.png,cube-z.png,cube+x.png,cube-x.png,cube+y.png,cube-y.png,cube+z.png,cube-z.png -o cubemap-layers.ktx2 -cube -array -f r8g8b8,UBN,sRGB

# 1D
toktx --t2 1d.ktx2 pattern-1d.png
toktx --t2 --mipmap 1d-mipmaps.ktx2 pattern-1d.png pattern-mip1.png pattern-mip2.png
# output is an image with numLayers=0, wtf
#toktx --t2 --layers 3 1d-layers.ktx2 pattern-1d.png pattern-1d.png black1d.png
PVRTexToolCLI -i pattern-1d.png,pattern-1d.png,black1d.png -o 1d-layers.ktx2 -array -f r8g8b8,UBN,sRGB

# 3D
# toktx failed to create ktxTexture; KTX error: Operation not allowed in the current state.
#toktx --t2 --depth 3 3d-rgb.ktx2 black.png pattern.png pattern.png
# PVRTexTool doesn't support 3D images, sigh
# Create a layered image and patch the header to make it 3D. The level layout
# is identical to a 3D image, but the orientation metadata will become invalid.
# This won't pass validation but KtxImporter should ignore invalid metadata.
# However, because the z axis will be flipped, we need to pass the images
# in reverse order here compared to 2d-layers.ktx2.
PVRTexToolCLI -i black.png,pattern.png,pattern.png -o 3d.ktx2 -array -f r8g8b8,UBN,sRGB
# depth = 3, numLayers = 0
printf '\x03\x00\x00\x00\x00\x00\x00\x00' | dd conv=notrunc of=3d.ktx2 bs=1 seek=28
# We can't patch a 2D array texture with mipmaps into a 3D texture because the
# number of layers stays the same, unlike shrinking z in mipmap levels
# TODO find another way to generate a test file
#PVRTexToolCLI -i black.png,pattern.png,pattern.png -o 3d-mipmaps.ktx2 -array -m -mfilter nearest -f r8g8b8,UBN,sRGB
#printf '\x03\x00\x00\x00\x00\x00\x00\x00' | dd conv=notrunc of=3d-mipmaps.ktx2 bs=1 seek=28
PVRTexToolCLI -i black.png,pattern.png,pattern.png,black.png,black.png,pattern.png -o 3d-layers.ktx2 -array -f r8g8b8,UBN,sRGB
printf '\x03\x00\x00\x00\x02\x00\x00\x00' | dd conv=notrunc of=3d-layers.ktx2 bs=1 seek=28

# Compressed
# PVRTC and BC* don't support non-power-of-2
PVRTexToolCLI -i pattern-pot.png -o 2d-compressed-pvrtc.ktx2 -f PVRTC1_4,UBN,sRGB
PVRTexToolCLI -i pattern-pot.png -o 2d-compressed-bc1.ktx2 -f BC1,UBN,sRGB
PVRTexToolCLI -i pattern-pot.png -o 2d-compressed-bc2.ktx2 -f BC2,UBN,sRGB
PVRTexToolCLI -i pattern-pot.png -o 2d-compressed-bc3.ktx2 -f BC3,UBN,sRGB
PVRTexToolCLI -i pattern-pot.png -o 2d-compressed-bc4.ktx2 -f BC4,UBN,sRGB
PVRTexToolCLI -i pattern-pot.png -o 2d-compressed-bc5.ktx2 -f BC5,UBN,sRGB
PVRTexToolCLI -i pattern-uneven.png -o 2d-compressed-etc2.ktx2 -f ETC2_RGB,UBN,sRGB

PVRTexToolCLI -i pattern-1d.png -o 1d-compressed-bc1.ktx2 -f BC1,UBN,sRGB
PVRTexToolCLI -i pattern-1d-uneven.png -o 1d-compressed-etc2.ktx2 -f ETC2_RGB,UBN,sRGB

# TODO ASTC

# TODO:
# 3D mips
# combined arrays + mips
# orientation: 1D l, 2D l, cubemap always rd, 3D
# compressed
