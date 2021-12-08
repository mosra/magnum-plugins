Updating test files
===================

The glTF `*.bin` files are generated from `*.bin.in` templates. The input file
is a Python source that's `exec()`d by the `in2bin.py` utility from
`CgltfImporter/Test`.

Batch conversion
----------------

The `convert.sh` script is a convenience tool that executes all conversion
routines on all files.
