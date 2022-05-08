#!/bin/bash

set -e

# in -> bin
for i in *.bin.in; do
    ./in2bin.py ${i}
done

# gltf -> embedded gltf
for i in animation buffer-invalid-short-size image image-buffer image-basis mesh skin; do
    ./gltf2embedded.py ${i}.gltf
done

# gltf -> glb
for i in animation animation-embedded buffer-invalid-short-size buffer-invalid-short-size-embedded buffer-long-size empty image image-embedded image-buffer image-buffer-embedded image-basis image-basis-embedded mesh mesh-embedded skin skin-embedded; do
    ./gltf2glb.py ${i}.gltf
done

# special cases
./gltf2glb.py buffer-invalid-notfound.gltf --no-embed
./gltf2glb.py buffer-invalid-no-uri.gltf --no-embed
./gltf2glb.py external-data.gltf --no-embed
./gltf2glb.py external-data-order.gltf --no-embed
