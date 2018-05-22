# Creating GLB files

To convert .gltf files to .glb, use
 * https://glb-packer.glitch.me/
 * or https://sbtron.github.io/makeglb/

for example.

Or use [gltf-pipeline](https://github.com/AnalyticalGraphicsInc/gltf-pipeline/tree/2.0) via npm:
and convert all gltf files into glb using:

~~~
find *.gltf | grep -o "^[^\.]*" | xargs -t -I '{}' gltf-pipeline -i '{}'.gltf -o '{}'.glb -b
mv output/*.glb .
~~~
