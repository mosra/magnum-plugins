#!/bin/bash

set -e

# BC6H. Compressonator doesn't support *.hdr or *.ktx2, but *.exr it does. Need
# to force StbImageImporter as it will use DevIL instead.
magnum-imageconverter -I StbImageImporter ../../StbImageImporter/Test/rgb.hdr rgb.exr
compressonatorcli -nomipmap -fd BC6H rgb.exr bc6h.dds
# Convert the 32-bit float input to 16-bit so it's easier to compare. For
# reproducibility don't embed the full KtxImageConverter revision in the file.
magnum-imageconverter rgb.exr -i forceChannelType=HALF -c generator= rgb16f.ktx2
rm rgb.exr

# BC6H signed. Compressonator knows nothing about that format, using the
# proprietary NVidia Texture Tools Exporter (download link needs a NVidia
# Developer Program membership)
LD_LIBRARY_PATH=~/Downloads/NVIDIA_Texture_Tools_Linux_3_1_6/ ~/Downloads/NVIDIA_Texture_Tools_Linux_3_1_6/nvcompress -nocuda -nomips -bc6s ../../StbImageImporter/Test/rgb.hdr bc6hs.dds
