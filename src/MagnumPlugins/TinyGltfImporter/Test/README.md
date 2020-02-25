Updating test files
===================

The `*.gltf` files are "golden sources" from which `*.glb` files are created.
Simply use the bundled `gltf2glb.py` utility:

```sh
./gltf2glb.py file.gltf # creates file.glb with buffers packed inside
```

The `*.bin` files, if needed, are created from `*.bin.in` templates. The input
file is a Python source that's `exec()`d by the `in2bin.py` utility. It needs
to have a `type` variable describing the output binary layout (using Python
[struct](https://docs.python.org/3.6/library/struct.html) syntax) and an
`input` array containing a list of floats/integers that match the `type`. After
that, convert the file like this:

```sh
./in2bin.py file.bin.in # creates file.bin
```

Basis texture sources
---------------------

The `texture.basis` file is created from `texture.png` using the following
command:

```sh
basisu texture.png -output_file texture.basis -y_flip -mipmap -mip_smallest 2
```

Embedding buffers
-----------------

Tests that use files referencing external binary blobs also have variants that
test the same, but embedded as a data URI (and then the same, but as a GLB
file). This is done using the `gltf2embedded.py` utility:

```sh
./gltf2embedded.py file.gltf # creates file-embedded.gltf with buffers embedded
./gltf2glb.py file-embedded.gltf # created file-embedded.glb
```

Batch conversion
----------------

The `convert.sh` script is a convenience tool that executes all conversion
routines on all files.
