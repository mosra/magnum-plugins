Shared test files
=================

Most of the test files are shared with TinyGltfImporter and are located in its
Test directory. Files that can't or don't make sense to be shared reside in
this folder.

Updating test files
===================

Similarly to TinyGltfImporter's test files, `*.gltf` files are "golden sources"
which are then possibly converted to `*.glb` files. See
TinyGltfImporter/Test/README.md for more information.

Batch conversion
----------------

The `convert.sh` script is a convenience tool that executes all conversion
routines on all files.
