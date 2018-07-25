Updating test files
===================

The `*.gltf` files are "golden sources" from which `*.glb` files are created.
Simply use the bundled `gltf2glb.py` utility:

```sh
./gltf2glb.py file.gltf # creates file.glb with everything packed inside
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
