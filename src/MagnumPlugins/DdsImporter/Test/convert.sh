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

rm rgba-{63x27,64x32}.tga rgb.tga
