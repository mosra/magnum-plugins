#!/bin/bash

set -e

# RGB
basisu rgb-63x27.png -output_file rgb.basis -y_flip
# Without y-flip
basisu rgb-63x27.png -output_file rgb-noflip.basis
# Linear image data
basisu rgb-63x27.png -output_file rgb-linear.basis -y_flip -linear

# RGBA
basisu rgba-63x27.png -output_file rgba.basis -force_alpha -y_flip

# Multiple images
basisu rgba-63x27.png rgba-27x63.png -output_file rgba-2images-mips.basis -y_flip -mipmap -mip_smallest 16 -mip_filter box

# Required for PVRTC1 target, which requires pow2 dimensions
basisu rgb-64x32.png -output_file rgb-pow2.basis -y_flip
basisu rgb-64x32.png -output_file rgb-linear-pow2.basis -y_flip -linear
basisu rgba-64x32.png -output_file rgba-pow2.basis -force_alpha -y_flip
