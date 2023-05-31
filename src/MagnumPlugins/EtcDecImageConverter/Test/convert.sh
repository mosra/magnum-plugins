#!/bin/bash

set -e

# Amazingly enough, neither NVTT nor Compressonator have a way to produce EAC
# files. As a fallback, those are generated from Basis-encoded files instead,
# with the source being rgba-63x27.png. Hopefully there isn't too much data
# lost in the process. For reproducibility don't embed the full
# KtxImageConverter revision in the file.
magnum-imageconverter ../../BasisImporter/Test/rgba.ktx2 -i basis/format=EacR -c generator= eac-r.ktx2
# This one is from 64x32.png to verify proper behavior with full blocks
magnum-imageconverter ../../BasisImporter/Test/rgba-pow2.ktx2 -i basis/format=EacRG -c generator= eac-rg.ktx2
# TODO Creating signed EAC is apparently impossible in any of the tools I have
# access to.

# ETC2 RGB8A1 and RGBA8. RGB8 is taken from KtxImporter tests. The DAMN THING
# has a bug where it swaps order of the channels so I have to perform a channel
# swap with imagemagick before to make it produce a correct output:
#   https://github.com/GPUOpen-Tools/compressonator/issues/144
#   https://github.com/GPUOpen-Tools/compressonator/issues/199
#   https://github.com/GPUOpen-Tools/compressonator/issues/247
# For the RGB8A1 I'm using a RGB input because otherwise half of the image
# becomes transparent due to the punchthrough alpha. And of course the
# -AlphaThreshold option doesn't work for ETC, FFS.
convert ../../BasisImporter/Test/rgb-63x27.png -channel-fx "red<=>blue" bgr-63x27.png
convert ../../BasisImporter/Test/rgba-64x32.png -channel-fx "red<=>blue" bgra-64x32.png
compressonatorcli -nomipmap -fd ETC2_RGBA1 bgr-63x27.png etc-rgb8a1.ktx
compressonatorcli -nomipmap -fd ETC2_RGBA bgra-64x32.png etc-rgba8.ktx
rm bgr-63x27.png
rm bgra-64x32.png

# The damn thing also can't save KTX2, so it has to be converted using ktx2ktx2
# from KTX-Software afterwards. The -f is to overwrite files if they exist, it
# has to be first because the argument parsing is utter crap apparently.
# LD_LIBRARY_PATH, ffs, is RPATH really that hard to set up?!
LD_LIBRARY_PATH=~/Downloads/KTX-Software-4.1.0-Linux-x86_64/lib ~/Downloads/KTX-Software-4.1.0-Linux-x86_64/bin/ktx2ktx2 -f etc-rgb8a1.ktx
LD_LIBRARY_PATH=~/Downloads/KTX-Software-4.1.0-Linux-x86_64/lib ~/Downloads/KTX-Software-4.1.0-Linux-x86_64/bin/ktx2ktx2 -f etc-rgba8.ktx
rm etc-rgb8a1.ktx
rm etc-rgba8.ktx
