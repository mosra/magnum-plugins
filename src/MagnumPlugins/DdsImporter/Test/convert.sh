#!/bin/bash

set -e

# Compressonator is beyond stupid and can't eat PNGs, somehow. Convert to TGA
# first.
convert ../../PngImporter/Test/rgb.png rgb.tga
convert ../../BasisImporter/Test/rgba-64x32.png rgba-64x32.tga
convert ../../BasisImporter/Test/rgba-63x27.png rgba-63x27.tga

compressonatorcli -nomipmap -fd DXT1 rgb.tga dxt1.dds

compressonatorcli -nomipmap -fd BC2 rgba-64x32.tga dxt3.dds
compressonatorcli -nomipmap -fd BC2 rgba-63x27.tga dxt3-incomplete-blocks.dds

compressonatorcli -nomipmap -fd DXT5 rgb.tga dxt5.dds

# This produces the legacy format somehow, not DX10
compressonatorcli -nomipmap -fd BC4 rgb.tga bc4unorm.dds
compressonatorcli -nomipmap -fd BC4_S rgb.tga bc4snorm.dds
compressonatorcli -nomipmap -fd BC5 rgb.tga bc5unorm.dds
compressonatorcli -nomipmap -fd BC5_S rgb.tga bc5snorm.dds

compressonatorcli -nomipmap -fd BC7 rgba-64x32.tga dxt10-bc7.dds

# Non-standard ASTC-compressed DDS using the proprietary NVidia Texture Tools
# Exporter (download link needs a NVidia Developer Program membership)
# path/to/NVIDIA_Texture_Tools_Linux_3_1_6/nvcompress -nocuda -astc_ldr_8x5 ../../PngImporter/Test/rgb.png dxt10-astc8x5.dds

# Array and cube map using the discontinued NVidia Texture Tools. Use the same
# images as with BasisImporter but reduce their size a bit usig ImageMagick.
convert ../../BasisImporter/Test/rgba-27x27.png -resize 5x5 rgba-5x5.png
convert ../../BasisImporter/Test/rgba-27x27-slice1.png -resize 5x5 rgba-5x5-slice1.png
convert ../../BasisImporter/Test/rgba-27x27-slice1.png -resize 5x5 rgba-5x5-slice2.png
nvassemble -array rgba-5x5{,-slice1,-slice2}.png
mv output.dds dxt10-rgba8unorm-array.dds
# For lack of better data, the cubemap repeats the 3 slices twice
nvassemble -cube rgba-5x5{,-slice1,-slice2,,-slice1,-slice2}.png
mv output.dds rgba8unorm-cube.dds

# And the final boss, cube with mips. Can't be uncompressed because the legacy
# nvcompress NVTT tool crashes, so I have to use Compressonator which knows
# only compressed formats.
compressonatorcli -miplevels 3 -fd DXT1 rgba8unorm-cube.dds dxt1-cube-mips.dds
compressonatorcli -miplevels 3 -fd BC7 rgba8unorm-cube.dds dxt10-bc7-cube-mips.dds
# Compressonator saves arraySize not as cube count, but 2D slice count, so
# patch it to contain 1 instead of 6
# https://github.com/GPUOpen-Tools/compressonator/issues/196
# https://stackoverflow.com/a/7290825
printf '\x01' | dd of=dxt10-bc7-cube-mips.dds bs=1 seek=140 conv=notrunc

# DXT10 cube (and cube array) is made by hand, because Compressonator doesn't
# handle arrays correctly, there it uses just the first slice, so we can't have
# a ground truth file for that.

# 3D block-compressed textures, to test Z-flip
nvassemble -volume rgba-5x5{,-slice1,-slice2}.png
compressonatorcli -nomipmap -fd DXT1 output.dds dxt1-3d.dds
compressonatorcli -nomipmap -fd BC7 output.dds dxt10-bc7-3d.dds
rm output.dds
# THE DAMN THING DOESN'T STORE THE ACTUAL Z SIZE IN THE FILE EITHER!!!
#   Trade::DdsImporter::openData(): ignoring 64 extra bytes at the end of file
# and a 5x5x1 image is what I get after opening this, the 64 bytes being 2x2x2
# 8-byte blocks that form the second and third slice. FFS. Vaguely related:
#   https://github.com/GPUOpen-Tools/compressonator/issues/111
printf '\x03' | dd of=dxt1-3d.dds bs=1 seek=24 conv=notrunc
# AND FOR DXT10 IT STORES THE DEPTH AS ARRAY SIZE!!!
printf '\x01' | dd of=dxt10-bc7-3d.dds bs=1 seek=140 conv=notrunc

rm rgba-{63x27,64x32}.tga rgb.tga rgba-5x5{,-slice1,-slice2}.png
