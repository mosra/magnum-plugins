#!/bin/bash

set -e

# Compressonator is beyond stupid and can't eat PNGs, somehow. Convert to TGA
# first.
convert ../../PngImporter/Test/rgb.png rgb.tga
convert ../../BasisImporter/Test/rgba-64x32.png rgba-64x32.tga
convert ../../BasisImporter/Test/rgba-63x27.png rgba-63x27.tga

compressonatorcli-bin -nomipmap -fd DXT1 rgb.tga dxt1.dds

compressonatorcli-bin -nomipmap -fd BC2 rgba-64x32.tga dxt3.dds
compressonatorcli-bin -nomipmap -fd BC2 rgba-63x27.tga dxt3-incomplete-blocks.dds

compressonatorcli-bin -nomipmap -fd DXT5 rgb.tga dxt5.dds

# This produces the legacy format somehow, not DX10
compressonatorcli-bin -nomipmap -fd BC4 rgb.tga bc4unorm.dds
compressonatorcli-bin -nomipmap -fd BC4_S rgb.tga bc4snorm.dds
compressonatorcli-bin -nomipmap -fd BC5 rgb.tga bc5unorm.dds
compressonatorcli-bin -nomipmap -fd BC5_S rgb.tga bc5snorm.dds

compressonatorcli-bin -nomipmap -fd BC7 rgba-64x32.tga dxt10-bc7.dds

# Non-standard ASTC-compressed DDS using the proprietary NVidia Texture Tools
# Exporter (download link needs a NVidia Developer Program membership)
# path/to/NVIDIA_Texture_Tools_Linux_3_1_6/nvcompress -nocuda -astc_ldr_8x5 ../../PngImporter/Test/rgb.png dxt10-astc8x5.dds

rm rgba-{63x27,64x32}.tga rgb.tga
