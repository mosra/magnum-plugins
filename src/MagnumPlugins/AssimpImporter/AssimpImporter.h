#ifndef Magnum_Trade_AssimpImporter_h
#define Magnum_Trade_AssimpImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2017 Jonathan Hale <squareys@googlemail.com>

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

@m_keywords{3dsImporter Ac3dImporter BlenderImporter BvhImporter CsmImporter}
@m_keywords{ColladaImporter DirectXImporter DxfImporter FbxImporter}
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

Supports importing of scene, object, camera, mesh, texture and image data.

This plugin provides `3dsImporter`, `Ac3dImporter`, `BlenderImporter`,
`BvhImporter`, `CsmImporter`, `ColladaImporter`, `DirectXImporter`,
`DxfImporter`, `FbxImporter`, `GltfImporter`, `IfcImporter`,
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

The plugin supports @ref ImporterFeature::OpenData and
@ref ImporterFeature::FileCallback features. The Assimp library loads
everything during initial import, meaning all external file loading callbacks
are called with @ref InputFileCallbackPolicy::LoadTemporary and the resources
can be safely freed right after the @ref openData() / @ref openFile() function
exits. In some cases, Assimp will explicitly call
@ref InputFileCallbackPolicy::Close on the opened file and then open it again.
In case of images, the files are loaded on-demand inside @ref image2D() calls
with @ref InputFileCallbackPolicy::LoadTemporary and
@ref InputFileCallbackPolicy::Close is emitted right after the file is fully
read.

Import of animation data is not supported at the moment.

The importer recognizes @ref ImporterFlag::Verbose, enabling verbose logging
in Assimp when the flag is enabled. However please note that since Assimp
handles logging through a global singleton, it's not possible to have different
verbosity levels in each instance.

@subsection Trade-AssimpImporter-behavior-materials Material import

-   Only materials with shading mode `aiShadingMode_Phong` are supported
-   Two-sided property and alpha mode is not imported
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
-   Ambient lights are imported as @ref LightData::Type::Point with attenuation
    set to @cpp {1.0f, 0.0f, 0.0f} @ce
-   The following properties are ignored:
    -   Specular color
    -   Custom light orientation vectors --- the orientation is always only
        inherited from the node containing the light
-   Area lights are not supported

@subsection Trade-AssimpImporter-behavior-cameras Camera import

-   Aspect and up vector properties are not imported

@subsection Trade-AssimpImporter-behavior-meshes Mesh import

-   Only point, triangle, and line meshes are loaded (quad and poly meshes
    are triangularized by Assimp)
-   Custom mesh attributes (such as `object_id` in Stanford PLY files) are
    not imported.
-   Texture coordinate layers with other than two components are skipped
-   For some file formats (such as COLLADA), Assimp may create a dummy
    "skeleton visualizer" mesh if the file has no mesh data. For others (such
    as glTF) not.
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
-   Multi-mesh nodes and multi-primitive meshes are loaded as follows,
    consistently with the behavior of @link TinyGltfImporter @endlink:
    -   Multi-primitive meshes are split by Assimp into individual meshes
    -   The @ref meshCount() query returns a number of all *primitives*, not
        meshes
    -   The @ref object3DCount() query returns a number of all nodes extended
        with number of extra objects for each additional mesh
    -   Each node referencing a multiple meshes is split into a sequence
        of @ref MeshObjectData3D instances following each other; the extra
        nodes being a direct and immediate children of the first one with an
        identity transformation
    -   @ref object3DForName() points to the first object containing the first
        mesh, @ref object3DName() returns the same name for all objects in
        given sequence

The mesh is always indexed; positions are always present, normals, colors and
texture coordinates are optional.

@subsection Trade-AssimpImporter-behavior-textures Texture import

-   Textures with mapping mode/wrapping `aiTextureMapMode_Decal` are loaded
    with @ref SamplerWrapping::ClampToEdge
-   Assimp does not appear to load any filtering information
-   Raw embedded image data is not supported

@subsection Trade-AssimpImporter-behavior-scene Scene import

-   For some file formats (such as COLLADA), Assimp fails to load the file if
    it doesn't contain any scene. For some (such as glTF) it will succeed and
    @ref sceneCount() / @ref object3DCount() will return zero.
-   If the root node imported by Assimp has children, it's ignored and only its
    children are exposed through @ref object3D(). On the other hand, for
    example in presence of @ref Trade-AssimpImporter-configuration "postprocessing flags"
    such as `PreTransformVertices`, there's sometimes just a single root node.
    In that case the single node is imported as a single @ref object3D()
    instead of being ignored.

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

@section Trade-AssimpImporter-state Access to internal importer state

The Assimp structures used to import data from a file can be accessed through
importer state methods:

-   Calling @ref importerState() returns pointer to the imported `aiScene`
-   Calling `*::importerState()` on data class instances returned from this
    importer return pointers to matching assimp structures:
    -   @ref AbstractMaterialData::importerState() returns `aiMaterial`
    -   @ref CameraData::importerState() returns `aiCamera`
    -   @ref TextureData::importerState() returns `std::pair<const aiMaterial*, aiTextureType>`,
        in which the first texture of given type in given material is referred to.
    -   @ref MeshData::importerState() returns `aiMesh`
    -   @ref ObjectData3D::importerState() returns `aiNode`
    -   @ref LightData::importerState() returns `aiLight`
    -   @ref ImageData2D::importerState() may return `aiTexture`, if texture was embedded
        into the loaded file.
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
        explicit AssimpImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~AssimpImporter();

    private:
        struct File;

        MAGNUM_ASSIMPIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL bool doIsOpened() const override;

        MAGNUM_ASSIMPIMPORTER_LOCAL void doSetFlags(ImporterFlags flags) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL void doSetFileCallback(Containers::Optional<Containers::ArrayView<const char>>(*callback)(const std::string&, InputFileCallbackPolicy, void*), void* userData) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL void doOpenState(const void* state, const std::string& filePath) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL void doOpenFile(const std::string& filename) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL void doClose() override;

        MAGNUM_ASSIMPIMPORTER_LOCAL Int doDefaultScene() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doSceneCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doCameraCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<CameraData> doCamera(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doObject3DCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Int doObject3DForName(const std::string& name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL std::string doObject3DName(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Pointer<ObjectData3D> doObject3D(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doLightCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<LightData> doLight(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doMeshCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<MeshData> doMesh(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doMaterialCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Int doMaterialForName(const std::string& name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL std::string doMaterialName(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<MaterialData> doMaterial(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL AbstractImporter* setupOrReuseImporterForImage(UnsignedInt id, const char* errorPrefix);

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL const void* doImporterState() const override;

        Containers::Pointer<Assimp::Importer> _importer;
        Assimp::IOSystem* _ourFileCallback;
        Containers::Pointer<File> _f;
};

}}

#endif
