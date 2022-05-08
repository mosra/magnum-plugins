#!/bin/bash

set -e

# in -> bin
for i in *.bin.in; do
    ../../GltfImporter/Test/in2bin.py ${i}
done

# special cases
../../GltfImporter/Test/gltf2glb.py buffer-invalid-notfound.gltf --no-embed
../../GltfImporter/Test/gltf2glb.py buffer-invalid-no-uri.gltf --no-embed
