# Exporting the skin files

1.  Open the `skin.blend` file, make desired changes and save the
    blend file.
2.  Export to Collada:
    -   set *Forward Axis* to *Y*
    -   set *Up Axis* to *Z*
    -   in the *Anim* tab:
        -   disable *Include Animations*
    -   overwrite the `skin.dae` file
3.  Export to FBX:
    -   set *Scale* to 0.01
    -   disable *Bake Animation*
    -   overwrite the `skin.fbx` file
4.  Export to glTF:
    -   set *Format* to *glTF Embedded (.gltf)*
    -   in the *Animation* field:
        -   disable *Animation*
        -   enable *Skinning*
    -   in the *Geometry* field:
        -   disable *Normals*, *UVs*, *Vertex Colors*
    -   overwrite the `skin.gltf` file
5.  Verify the skinned animations play back correctly
6.  Commit the updated `skin.*` files, do a sanity check that the
    sizes didn't change much.
