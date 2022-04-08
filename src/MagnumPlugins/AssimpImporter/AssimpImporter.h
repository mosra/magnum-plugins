#ifndef Magnum_Trade_AssimpImporter_h
#define Magnum_Trade_AssimpImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2017 Jonathan Hale <squareys@googlemail.com>
    Copyright © 2021 Pablo Escobar <mail@rvrs.in>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/** @file
 * @brief Class @ref Magnum::Trade::AssimpImporter
 */

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/AssimpImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_ASSIMPIMPORTER_BUILD_STATIC
    #ifdef AssimpImporter_EXPORTS
        #define MAGNUM_ASSIMPIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_ASSIMPIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_ASSIMPIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_ASSIMPIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_ASSIMPIMPORTER_EXPORT
#define MAGNUM_ASSIMPIMPORTER_LOCAL
#endif

#ifndef DOXYGEN_GENERATING_OUTPUT
namespace Assimp {
    class Importer;
    class IOSystem;
}
#endif

namespace Magnum { namespace Trade {

/**
@brief Assimp importer

@m_keywords{3dsImporter 3mfImporter Ac3dImporter BlenderImporter BvhImporter}
@m_keywords{CsmImporter ColladaImporter DirectXImporter DxfImporter FbxImporter}
@m_keywords{GltfImporter IfcImporter IrrlichtImporter LightWaveImporter}
@m_keywords{ModoImporter MilkshapeImporter ObjImporter OgreImporter}
@m_keywords{OpenGexImporter StanfordImporter StlImporter TrueSpaceImporter}
@m_keywords{UnrealImporter ValveImporter XglImporter}

Imports various formats using [Assimp](http://assimp.org), in particular:

-   Autodesk FBX (`*.fbx`)
-   COLLADA (`*.dae`)
-   glTF (`*.gltf`, `*.glb`)
-   Blender 3D (`*.blend`)
-   3ds Max 3DS and ASE (`*.3ds`, `*.ase`)
-   3D Manufacturing Format (`*.3mf`)
-   Wavefront OBJ (`*.obj`)
-   Industry Foundation Classes (IFC/Step) (`*.ifc`)
-   XGL (`*.xgl`, `*.zgl`)
-   Stanford PLY (`*.ply`)
-   AutoCAD DXF (`*.dxf`)
-   LightWave, LightWave Scene (`*.lwo`, `*.lws`)
-   Modo (`*.lxo`)
-   Stereolithography (`*.stl`)
-   DirectX X (`*.x`)
-   AC3D (`*.ac`)
-   Milkshape 3D (`*.ms3d`)
-   TrueSpace (`*.cob`, `*.scn`)
-   Biovision BVH (`*.bvh`)
-   CharacterStudio Motion (`*.csm`)
-   Ogre XML (`*.xml`)
-   Irrlicht Mesh and Scene (`*.irrmesh`, `*.irr`)
-   Quake I (`*.mdl`)
-   Quake II (`*.md2`)
-   Quake III Mesh (`*.md3`)
-   Quake III Map/BSP (`*.pk3`)
-   Return to Castle Wolfenstein (`*.mdc`)
-   Doom 3 (`*.md5*`)
-   Valve Model (`*.smd`, `*.vta`)
-   Open Game Engine Exchange (`*.ogex`)
-   Unreal (`*.3d`)
-   BlitzBasic 3D (`*.b3d`)
-   Quick3D (`*.q3d`, `*.q3s`)
-   Neutral File Format (`*.nff`)
-   Sense8 WorldToolKit (`*.nff`)
-   Object File Format (`*.off`)
-   PovRAY Raw (`*.raw`)
-   Terragen Terrain (`*.ter`)
-   3D GameStudio (3DGS), 3D GameStudio (3DGS) Terrain (`*.mdl`, `*.hmp`)
-   Izware Nendo (`*.ndo`)

Supports importing of scene, object, camera, mesh, texture, image, animation
and skin data.

This plugin provides `3dsImporter`, `3mfImporter`, `Ac3dImporter`,
`BlenderImporter`, `BvhImporter`, `CsmImporter`, `ColladaImporter`,
`DirectXImporter`, `DxfImporter`, `FbxImporter`, `GltfImporter`, `IfcImporter`,
`IrrlichtImporter`, `LightWaveImporter`, `ModoImporter`, `MilkshapeImporter`,
`ObjImporter`, `OgreImporter`, `OpenGexImporter`, `StanfordImporter`,
`StlImporter`, `TrueSpaceImporter`, `UnrealImporter`, `ValveImporter` and
`XglImporter` plugins.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the [Assimp](http://assimp.org) library,
    licensed under @m_class{m-label m-success} **BSD 3-clause**
    ([license text](http://assimp.org/index.php/license),
    [choosealicense.com](https://choosealicense.com/licenses/bsd-3-clause/)).
    It requires attribution for public use.

@section Trade-AssimpImporter-usage Usage

This plugin depends on the @ref Trade and [Assimp](http://assimp.org) libraries
and the @ref AnyImageImporter plugin and is built if `WITH_ASSIMPIMPORTER` is
enabled when building Magnum Plugins. To use as a dynamic plugin, load
@cpp "AssimpImporter" @ce via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins](https://github.com/mosra/magnum-plugins) and
[assimp](https://github.com/assimp/assimp) repositories and do the following.
If you want to use system-installed Assimp, omit the first part and point
`CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
# Disable Assimp tests, tools and exporter functionality
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
# If you won't be accessing Assimp outside of the plugin, build it as static to
# have the plugin binary self-contained
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# The following is important to avoid Assimp appending `d` to all your
# binaries. You need Assimp >= 5.0.0 for this to work, also note that after
# 5.0.1 this option is prefixed with ASSIMP_, so better set both variants.
set(INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
add_subdirectory(assimp EXCLUDE_FROM_ALL)

set(WITH_ANYIMAGEIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum EXCLUDE_FROM_ALL)

set(WITH_ASSIMPIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::AssimpImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindAssimp.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindAssimp.cmake)
into your `modules/` directory, request the `AssimpImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::AssimpImporter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED AssimpImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::AssimpImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-AssimpImporter-behavior Behavior and limitations

The plugin supports @ref ImporterFeature::OpenData, @relativeref{ImporterFeature,OpenState} and @relativeref{ImporterFeature,FileCallback} features. The Assimp library loads
everything during initial import, meaning all external file loading callbacks
are called with @ref InputFileCallbackPolicy::LoadTemporary and the resources
can be safely freed right after the @ref openData() / @ref openFile() function
exits. In some cases, Assimp will explicitly call
@ref InputFileCallbackPolicy::Close on the opened file and then open it again.
In case of images, the files are loaded on-demand inside @ref image2D() calls
with @ref InputFileCallbackPolicy::LoadTemporary and
@ref InputFileCallbackPolicy::Close is emitted right after the file is fully
read.

The importer recognizes @ref ImporterFlag::Verbose, enabling verbose logging
in Assimp when the flag is enabled. However please note that since Assimp
handles logging through a global singleton, it's not possible to have different
verbosity levels in each instance.

@subsection Trade-AssimpImporter-behavior-scene Scene import

-   Assimp supports only a single scene. For some file formats (such as
    COLLADA) Assimp fails to load the file if it doesn't contain any scene. For
    some (such as glTF) it will succeed and @ref sceneCount() will return zero.
-   Assimp's scene representation is limited to a single root node. To undo
    undesirable complexity, if the root Assimp node has children, it's ignored
    and only its children are exposed. On the other hand, for example in
    presence of @ref Trade-AssimpImporter-configuration "postprocessing flags"
    such as @cb{.ini} PreTransformVertices @ce, there's sometimes just a single
    root node. In that case the single node is imported as a single object
    instead of being ignored.
-   Imported scenes always have @ref SceneMappingType::UnsignedInt, with
    @ref SceneData::mappingBound() equal to @ref objectCount(). The scene is
    always 3D.
-   All reported objects have a @ref SceneField::Parent (of type
    @ref SceneFieldType::Int), @ref SceneField::ImporterState (of type
    @ref SceneFieldType::Pointer, see @ref Trade-AssimpImporter-state below)
    and @ref SceneField::Transformation (of type @ref SceneFieldType::Matrix4x4).
    These three fields share the same object mapping with
    @ref SceneFieldFlag::ImplicitMapping set.
-   Assimp doesn't have a builtin way to represent separate TRS components of
    a transformation, they can only be decomposed from the full matrix
    post-import. For this reason, only the @ref SceneField::Transformation is
    present in the output.
-   If the scene references meshes, a @ref SceneField::Mesh (of type
    @ref SceneFieldType::UnsignedInt) and a @ref SceneField::MeshMaterial (of
    type @ref SceneFieldType::Int) is present, both with
    @ref SceneFieldFlag::OrderedMapping set. Assimp has no concept of an
    unassigned material, so the material ID is never @cpp -1 @ce. Nodes
    containing multiple meshes or referencing a multi-primitive mesh have
    several @ref SceneField::Mesh (and @ref SceneField::MeshMaterial)
    asignments. See @ref Trade-AssimpImporter-behavior-meshes and
    @ref Trade-AssimpImporter-behavior-materials for further details.
-   If any mesh referenced by a scene is skinned, a @ref SceneField::Skin (of
    type @ref SceneFieldType::UnsignedInt) is present, with
    @ref SceneFieldFlag::OrderedMapping set. See
    @ref Trade-AssimpImporter-behavior-animations for further details.
-   If the scene references cameras, a @ref SceneField::Camera (of type
    @ref SceneFieldType::UnsignedInt) is present, with
    @ref SceneFieldFlag::OrderedMapping set. Contrary to Assimp's
    documentation, for certain formats (such as COLLADA), if one camera is
    referenced by multiple nodes, it'll get duplicated instead of the nodes
    sharing the same camera reference. A single node can also only reference
    one camera at most. See @ref Trade-AssimpImporter-behavior-cameras for
    further details.
-   If the scene references lights, a @ref SceneField::Light (of type
    @ref SceneFieldType::UnsignedInt) is present, with
    @ref SceneFieldFlag::OrderedMapping set. Contrary to Assimp's
    documentation, for certain formats (such as COLLADA), if one light is
    referenced by multiple nodes, it'll get duplicated instead of the nodes
    sharing the same light reference. A single node can also only reference one
    light at most. See @ref Trade-AssimpImporter-behavior-lights for further
    details.

@subsection Trade-AssimpImporter-behavior-animations Animation and skin import

-   Assimp sometimes adds dummy animation tracks with a single key-value pair
    and the default node transformation. If found, the importer removes these
    dummy tracks and prints a message if verbose logging is enabled. Can be
    disabled per-animation with the @cb{.ini} removeDummyAnimationTracks @ce
    @ref Trade-AssimpImporter-configuration "configuration option".
-   Channel order within animations is not always preserved
    by Assimp, depending on file type and compiler. You may have to manually
    order tracks by type and target after importing.
-   Quaternion rotation tracks are postprocessed in order to make it
    possible to use the faster
    @ref Math::lerp(const Quaternion<T>&, const Quaternion<T>&, T) "Math::lerp()" /
    @ref Math::slerp(const Quaternion<T>&, const Quaternion<T>&, T) "Math::slerp()"
    functions instead of
    @ref Math::lerpShortestPath(const Quaternion<T>&, const Quaternion<T>&, T) "Math::lerpShortestPath()" /
    @ref Math::slerpShortestPath(const Quaternion<T>&, const Quaternion<T>&, T) "Math::slerpShortestPath()".
    Can be disabled per-animation with the
    @cb{.ini} optimizeQuaternionShortestPath @ce
    @ref Trade-AssimpImporter-configuration "configuration option".
-   If quaternion rotation tracks are not normalized, the importer
    prints a warning and normalizes them. Can be disabled per-animation with
    the @cb{.ini} normalizeQuaternions @ce
    @ref Trade-AssimpImporter-configuration "configuration option".
-   Morph targets are not supported
-   Animation tracks are always imported with
    @ref Animation::Interpolation::Linear, because Assimp doesn't expose any
    interpolation modes
-   Animation tracks using `aiAnimBehaviour_DEFAULT` or `aiAnimBehaviour_REPEAT`
    are currently not implemented and fall back to using
    @ref Animation::Extrapolation::Constant
-   It's possible to request all animation clips to be merged into one using
    the @cb{.ini} mergeAnimationClips @ce option in order to for example
    preserve cinematic animations when using the Blender glTF exporter (as it
    otherwise outputs a separate clip for each object). When this option is
    enabled, @ref animationCount() always report either @cpp 0 @ce or
    @cpp 1 @ce and the merged animation has no name. With this option enabled,
    however, it can happen that multiple conflicting tracks affecting the same
    node are merged in the same clip, causing the animation to misbehave.
-   Assimp versions before commit [e3083c21f0a7beae6c37a2265b7919a02cbf83c4](https://github.com/assimp/assimp/commit/e3083c21f0a7beae6c37a2265b7919a02cbf83c4)
    read spline-interpolated glTF animation tracks incorrectly and produce
    broken animations. A warning is printed when importing glTF animations on
    these versions.
    Because it's impossible to detect the actual brokenness, the warning is
    printed even if the imported data may be correct.
-   Original skins are not exposed by Assimp, instead each mesh with joint
    weights produces its own skin. Skin names will be equal to their
    corresponding mesh names. A consequence of this is that Assimp only imports
    joint weight attributes for one skin and ignores all other skins targetting
    the same mesh.
-   You can request to merge all mesh skins into one using the
    @cb{.ini} mergeSkins @ce option. Duplicate joints (same object index and
    inverse bind matrix) will be merged and joint ids in vertex attributes
    adjusted accordingly. When this option is enabled, @ref skin3DCount()
    always report either @cpp 0 @ce or @cpp 1 @ce and the merged skin has no
    name. This option needs to be set before opening a file because it affects
    skin as well as mesh loading.

@subsection Trade-AssimpImporter-behavior-materials Material import

-   Assimp has no concept of an unassigned material and thus it may create
    dummy materials and assign them to meshes that have no material specified
-   Only materials with shading mode `aiShadingMode_Phong` are supported
-   Two-sided property and alpha mode is not imported
-   Unrecognized Assimp material attributes are imported with their raw names
    and types. You can find the attribute names in [assimp/material.h](https://github.com/assimp/assimp/blob/master/include/assimp/material.h#L944).
    Unknown or raw buffer types are imported as
    @ref MaterialAttributeType::String. To ignore all unrecognized Assimp
    materials instead, enable the @cb{.ini} ignoreUnrecognizedMaterialData @ce
    @ref Trade-AssimpImporter-configuration "configuration option".
-   To load all material attributes with the raw Assimp names and types, enable
    the @cb{.ini} forceRawMaterialData @ce
    @ref Trade-AssimpImporter-configuration "configuration option". You will
    then get attributes like `$clr.diffuse` instead of
    @ref MaterialAttribute::DiffuseColor. @ref MaterialData::types() will
    always be empty with this option enabled.
-   Assimp seems to ignore ambient textures in COLLADA files
-   For some reason, Assimp 4.1 imports STL models with ambient set to
    @cpp 0xffffff_srgbf @ce, which causes all other color information to be
    discarded. If such a case is detected and there's no ambient texture
    present, the ambient is forced back to @cpp 0x000000_srgbf @ce. See also
    [assimp/assimp#2059](https://github.com/assimp/assimp/issues/2059). This
    workaround can be disabled using the @cb{.ini} forceWhiteAmbientToBlack @ce
    @ref Trade-AssimpImporter-configuration "configuration option".

@subsection Trade-AssimpImporter-behavior-lights Light import

-   @ref LightData::intensity() is always @cpp 1.0f @ce, instead Assimp
    premultiplies @ref LightData::color() with the intensity
-   The following properties are ignored:
    -   Specular color
    -   Custom light orientation vectors --- the orientation is always only
        inherited from the node containing the light
-   @ref LightData::Type::Directional and @ref LightData::Type::Ambient expect
    the attenuation to be constant, but Assimp doesn't follow that for certain
    file formats (such as Blender). In that case the attenuation value from
    Assimp is ignored.
-   Area lights are not supported
-   For certain file formats (such as COLLADA), if a light isn't referenced by
    any node, it gets ignored during import

@subsection Trade-AssimpImporter-behavior-cameras Camera import

-   Up vector property is not imported
-   Orthographic camera support requires Assimp 5.1.0. Earlier versions import
    them as perspective cameras with arbitrary field of view and aspect ratio.
-   For certain file formats (such as COLLADA), if a camera isn't referenced by
    any node, it gets ignored during import

@subsection Trade-AssimpImporter-behavior-meshes Mesh import

-   Only point, triangle, and line meshes are loaded (quad and poly meshes
    are triangularized by Assimp)
-   Custom mesh attributes (such as `object_id` in Stanford PLY files) are
    not imported.
-   Texture coordinate layers with other than two components are skipped
-   Per-face attributes in Stanford PLY files are not imported.
-   Stanford PLY files that contain a comment before the format line fail to
    import.
-   The importer follows types used by Assimp, thus indices are always
    @ref MeshIndexType::UnsignedInt, positions, normals, tangents and
    bitangents are always imported as @ref VertexFormat::Vector3, texture
    coordinates as @ref VertexFormat::Vector2 and colors as
    @ref VertexFormat::Vector4. In other words, everything gets expanded by
    Assimp to floats, even if the original file might be using different types.
-   The imported model always has either both @ref MeshAttribute::Tangent
    @ref MeshAttribute::Bitangent or neither of them, tangents are always
    three-component with binormals separate.
-   Joint IDs and weights for skinning are imported as custom vertex attributes
    named "JOINTS" and "WEIGHTS" with formats @ref VertexFormat::Vector4ui and
    @ref VertexFormat::Vector4, respectively. Imported meshes always have
    either both or neither of them. Their mapping to/from a string can be
    queried using @ref meshAttributeName() and @ref meshAttributeForName(). By
    default, the number of weights per vertex is limited to 4, but you can
    change this limit by setting the @cb{.ini} maxJointWeights @ce
    @ref Trade-AssimpImporter-configuration "configuration option".
-   Assimp doesn't correctly import glTF meshes with multiple sets of joint
    weights, only the last set will be imported. A warning is printed when this
    is detected, but it may misfire for other meshes with non-normalized
    weights.
-   Multi-primitive meshes are split by Assimp into individual meshes, nodes
    that reference a multi-primitive mesh have multiple @ref SceneField::Mesh
    (and @ref SceneField::MeshMaterial) entries in the imported @ref SceneData.
-   For certain file formats (such as COLLADA), if a mesh isn't referenced by
    any node, it gets ignored during import

The mesh is always indexed; positions are always present, normals, colors and
texture coordinates are optional.

@subsection Trade-AssimpImporter-behavior-textures Texture import

-   Textures with mapping mode/wrapping `aiTextureMapMode_Decal` are loaded
    with @ref SamplerWrapping::ClampToEdge
-   Assimp does not appear to load any filtering information
-   Raw embedded image data is not supported

@section Trade-AssimpImporter-configuration Plugin-specific configuration

Assimp has a versatile set of configuration options and processing operations
applied on imported scenes. A subset of these is exposed via
@ref configuration(), the full form shown below. The first group of options
matches the [AI_CONFIG_*](http://assimp.sourceforge.net/lib_html/config_8h.html)
macros and has to be applied before opening first file with given plugin
instance; to change them again you need to create a new importer instance.

The @cb{.conf} [postprocess] @ce subgroup contains boolean toggles that
correspond to the [aiPostProcessSteps](http://assimp.sourceforge.net/lib_html/postprocess_8h.html#a64795260b95f5a4b3f3dc1be4f52e410)
enum. Some of them are enabled by default, some not; options for not yet
supported features are omitted. These are passed to Assimp when opening a file,
meaning a change in these will be always applied to the next opened file.

@snippet MagnumPlugins/AssimpImporter/AssimpImporter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.

@section Trade-AssimpImporter-state Access to internal importer state

The Assimp structures used to import data from a file can be accessed through
importer state methods:

-   Calling @ref importerState() returns a pointer to the imported `aiScene`
-   Importer state on data class instances returned from this importer return
    pointers to matching Assimp structures:
    -   @ref AnimationData::importerState() returns `aiAnimation`, or
        @cpp nullptr @ce if the @cb{.ini} mergeAnimationClips @ce option is
        enabled
    -   @ref CameraData::importerState() returns `aiCamera`
    -   @ref LightData::importerState() returns `aiLight`
    -   @ref MaterialData::importerState() returns `aiMaterial`
    -   @ref MeshData::importerState() returns `aiMesh`
    -   @ref ImageData2D::importerState() may return `aiTexture`, if texture
        was embedded into the loaded file.
    -   @ref SceneData::importerState() returns the root `aiNode` and all
        objects have a @ref SceneField::ImporterState with their own `aiNode`
    -   @ref SkinData::importerState() returns `aiMesh`, or `nullptr` if the
        @cb{.ini} mergeSkins @ce option is enabled
    -   @ref TextureData::importerState() returns
        @cpp std::pair<const aiMaterial*, aiTextureType> @ce,
        in which the first texture of given type in given material is referred
        to.
-   @ref openState() expects a pointer to an Assimp scene (i.e., `const aiScene*`)
    and optionally a path (in order to be able to load textures, if needed)

@todo There are more formats mentioned at http://assimp.sourceforge.net/main_features_formats.html, add aliases for them?
*/
class MAGNUM_ASSIMPIMPORTER_EXPORT AssimpImporter: public AbstractImporter {
    public:
        /**
         * @brief Default constructor
         *
         * In case you want to open images, use
         * @ref AssimpImporter(PluginManager::Manager<AbstractImporter>&)
         * instead.
         */
        explicit AssimpImporter();

        /**
         * @brief Constructor
         *
         * The plugin needs access to plugin manager for importing images.
         */
        explicit AssimpImporter(PluginManager::Manager<AbstractImporter>& manager);

        /** @brief Plugin manager constructor */
        explicit AssimpImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~AssimpImporter();

    private:
        struct File;

        MAGNUM_ASSIMPIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL bool doIsOpened() const override;

        MAGNUM_ASSIMPIMPORTER_LOCAL void doSetFlags(ImporterFlags flags) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL void doSetFileCallback(Containers::Optional<Containers::ArrayView<const char>>(*callback)(const std::string&, InputFileCallbackPolicy, void*), void* userData) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL void doOpenState(const void* state, Containers::StringView filePath) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL void doOpenFile(Containers::StringView filename) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL void doClose() override;

        MAGNUM_ASSIMPIMPORTER_LOCAL Int doDefaultScene() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doSceneCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Int doSceneForName(Containers::StringView name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::String doSceneName(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedLong doObjectCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Long doObjectForName(Containers::StringView name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::String doObjectName(UnsignedLong id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doCameraCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Int doCameraForName(Containers::StringView name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::String doCameraName(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<CameraData> doCamera(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doLightCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Int doLightForName(Containers::StringView name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::String doLightName(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<LightData> doLight(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doMeshCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Int doMeshForName(Containers::StringView name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::String doMeshName(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<MeshData> doMesh(UnsignedInt id, UnsignedInt level) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL MeshAttribute doMeshAttributeForName(Containers::StringView name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::String doMeshAttributeName(UnsignedShort name) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doMaterialCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Int doMaterialForName(Containers::StringView name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::String doMaterialName(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<MaterialData> doMaterial(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL AbstractImporter* setupOrReuseImporterForImage(UnsignedInt id, const char* errorPrefix);

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doAnimationCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::String doAnimationName(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Int doAnimationForName(Containers::StringView name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<AnimationData> doAnimation(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doSkin3DCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Int doSkin3DForName(Containers::StringView name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::String doSkin3DName(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<SkinData3D> doSkin3D(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL const void* doImporterState() const override;

        Containers::Pointer<Assimp::Importer> _importer;
        Assimp::IOSystem* _ourFileCallback;
        Containers::Pointer<File> _f;
};

}}

#endif
