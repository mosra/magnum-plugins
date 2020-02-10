#!/bin/bash

set -e

# in -> bin
for i in animation animation-patching mesh-colors mesh-primitives mesh přívodní-šňůra; do
    ./in2bin.py ${i}.bin.in
done

# gltf -> embedded gltf
for i in animation image image-buffer image-basis mesh-colors mesh-primitives mesh; do
    ./gltf2embedded.py ${i}.gltf
done

# gltf -> glb
for i in animation animation-embedded camera empty image image-embedded image-buffer image-buffer-embedded image-basis image-basis-embedded light material-blinnphong material-metallicroughness material-specularglossiness material-properties mesh mesh-colors mesh-colors-embedded mesh-primitives mesh-primitives-embedded mesh mesh-embedded scene scene-nodefault object-transformation texture-default-sampler texture-empty-sampler texture; do
    ./gltf2glb.py ${i}.gltf
done

# special cases
./gltf2glb.py buffer-notfound.gltf --no-embed
./gltf2glb.py external-data.gltf --no-embed
