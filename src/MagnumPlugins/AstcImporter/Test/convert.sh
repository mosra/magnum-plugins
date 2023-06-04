#!/bin/bash

set -e

astcenc -cl ../../BasisImporter/Test/rgba-64x32.png 8x8.astc 8x8 100
astcenc -cl ../../BasisImporter/Test/rgba-63x27.png 12x10-incomplete-blocks.astc 12x10 100

# Arrays. Unfortunately the tool needs special naming, so we have to rename.
cp ../../BasisImporter/Test/rgba-27x27.png rgba-3d_0.png
cp ../../BasisImporter/Test/rgba-27x27-slice1.png rgba-3d_1.png
cp ../../BasisImporter/Test/rgba-27x27-slice2.png rgba-3d_2.png
astcenc -cl rgba-3d.png 3x3x3.astc 3x3x3 100 -zdim 3
rm rgba-3d_{0,1,2}.png

# The same, but 2D array. OF COURSE astcenc has no idea that something like
# this would be possible, so it's instead done by concatenating two 2D
# together, cutting away the second header and patching the Z size to be 2.
# astcenc -cl rgba-3d.png 12x12-array-incomplete-blocks.astc 12x12 100 -array 2
astcenc -cl ../../BasisImporter/Test/rgba-27x27.png 12x12-array-incomplete-blocks.astc 12x12 100
astcenc -cl ../../BasisImporter/Test/rgba-27x27-slice2.png 12x12-array-incomplete-blocks-1.astc 12x12 100
# https://unix.stackexchange.com/q/6852
dd bs=1 skip=16 if=12x12-array-incomplete-blocks-1.astc of=12x12-array-incomplete-blocks-data.astc
cat 12x12-array-incomplete-blocks-data.astc >> 12x12-array-incomplete-blocks.astc
# https://stackoverflow.com/a/7290825
printf '\x02' | dd of=12x12-array-incomplete-blocks.astc bs=1 seek=13 conv=notrunc
rm 12x12-array-incomplete-blocks-{1,data}.astc
