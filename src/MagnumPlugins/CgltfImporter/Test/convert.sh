#!/bin/bash

set -e

# in -> bin
for i in *.bin.in; do
    ../../TinyGltfImporter/Test/in2bin.py ${i}
done

# gltf -> embedded gltf
for i in buffer-short-size; do
    ../../TinyGltfImporter/Test/gltf2embedded.py ${i}.gltf
done

# gltf -> glb
for i in buffer-short-size buffer-short-size-embedded; do
    ../../TinyGltfImporter/Test/gltf2glb.py ${i}.gltf
done

# special cases
../../TinyGltfImporter/Test/gltf2glb.py external-data-order.gltf --no-embed
