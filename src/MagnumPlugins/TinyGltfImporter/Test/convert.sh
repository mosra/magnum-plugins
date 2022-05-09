#!/bin/bash

set -e

# in -> bin
for i in *.bin.in; do
    ../../CgltfImporter/Test/in2bin.py ${i}
done

# special cases
../../CgltfImporter/Test/gltf2glb.py buffer-invalid-notfound.gltf --no-embed
../../CgltfImporter/Test/gltf2glb.py buffer-invalid-no-uri.gltf --no-embed
