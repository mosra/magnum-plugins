#!/bin/bash

set -e

# in -> bin
for i in *.bin.in; do
    ../../TinyGltfImporter/Test/in2bin.py ${i}
done
