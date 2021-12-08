#!/bin/bash

set -e

# in -> bin
for i in *.bin.in; do
    ./in2bin.py ${i}
done

# gltf -> embedded gltf
for i in animation buffer-short-size image image-buffer image-basis mesh skin; do
    ./gltf2embedded.py ${i}.gltf
done

# gltf -> glb
for i in animation animation-embedded buffer-short-size buffer-short-size-embedded buffer-wrong-size camera empty image image-embedded image-buffer image-buffer-embedded image-basis image-basis-embedded light mesh mesh-embedded scene scene-transformation skin skin-embedded texture-default-sampler texture-empty-sampler texture; do
    ./gltf2glb.py ${i}.gltf
done

# special cases
./gltf2glb.py buffer-notfound.gltf --no-embed
./gltf2glb.py buffer-no-uri.gltf --no-embed
./gltf2glb.py external-data.gltf --no-embed
./gltf2glb.py external-data-order.gltf --no-embed
