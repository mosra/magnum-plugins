#!/bin/bash

set -e

# in -> bin
for i in *.bin.in; do
    ../../CgltfImporter/Test/in2bin.py ${i}
done
