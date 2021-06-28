# Exporting the animation files

1.  Open the `exported-animation.blend` file, make desired changes and save the
    blend file.
2.  Export to Collada:
    -   set *Forward Axis* to *Y*
    -   set *Up Axis* to *Z*
    -   in the *Anim* tab:
        -   enable *Include Animations*
        -   set *Key Type* to *Samples*
    -   overwrite the `exported-animation.dae` file
3.  Export to FBX:
    -   set *Scale* to 0.01
    -   enable *Bake Animation*
    -   overwrite the `exported-animation.fbx` file
4.  Export to glTF:
    -   set *Format* to *glTF Embedded (.gltf)*
    -   in the *Animation* field:
        -   enable *Animation*
        -   in the *Animation* field:
            - enable *Always Sample Animations*
    -   overwrite the `exported-animation.gltf` file
5.  Verify the animations play back correctly
6.  Commit the updated `exported-animation.*` files, do a sanity check that the
    sizes didn't change much.
