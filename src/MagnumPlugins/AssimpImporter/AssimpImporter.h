#ifndef Magnum_Trade_AssimpImporter_h
#define Magnum_Trade_AssimpImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
              Vladimír Vondruš <mosra@centrum.cz>
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

namespace Magnum { namespace Trade {

/**
@brief Assimp importer

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

This plugin depends on the @ref Trade and [Assimp](http://assimp.org) libraries
and is built if `WITH_ASSIMPIMPORTER` is enabled when building Magnum Plugins.
To use as a dynamic plugin, you need to load the @cpp "AssimpImporter" @ce
plugin from `MAGNUM_PLUGINS_IMPORTER_DIR`. To use as a static plugin or as a
dependency of another plugin with CMake, you need to request the
`AssimpImporter` component of the `MagnumPlugins` package and link to the
`MagnumPlugins::AssimpImporter` target.

This plugin provides `3dsImporter`, `Ac3dImporter`, `BlenderImporter`,
`BvhImporter`, `CsmImporter`, `ColladaImporter`, `DirectXImporter`,
`DxfImporter`, `FbxImporter`, `GltfImporter`, `IfcImporter`,
`IrrlichtImporter`, `LightWaveImporter`, `ModoImporter`, `MilkshapeImporter`,
`ObjImporter`, `OgreImporter`, `OpenGexImporter`, `StanfordImporter`,
`StlImporter`, `TrueSpaceImporter`, `UnrealImporter`, `ValveImporter` and
`XglImporter` plugins.

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

@section Trade-AssimpImporter-limitations Behavior and limitations

@subsection Trade-AssimpImporter-limitations-materials Material import

-   Only materials with shading mode `aiShadingMode_Phong` are supported
-   Only the first diffuse/specular/ambient texture is loaded

@subsection Trade-AssimpImporter-limitations-lights Light import

-   The following properties are ignored:
    -   Angle inner/outer cone
    -   Linear/quadratic/constant attenuation
    -   Ambient/specular color
-   Assimp does not load a property which can be mapped to
    @ref LightData::intensity()
-   Light types other than `aiLightSource_DIRECTIONAL`, `aiLightSource_POINT`
    and `aiLightSource_SPOT` are unsupported

@subsection Trade-AssimpImporter-limitations-cameras Camera import

-   Aspect and up vector properties are ignored

@subsection Trade-AssimpImporter-limitations-meshes Mesh import

-   Only the first mesh of a `aiNode` is loaded
-   Only triangle meshes are loaded
-   Texture coordinate layers with other than two components are skipped

@subsection Trade-AssimpImporter-limitations-textures Texture import

-   Textures with mapping mode/wrapping `aiTextureMapMode_Decal` are loaded
    with @ref SamplerWrapping::ClampToEdge
-   Assimp does not appear to load any filtering information
-   Raw embedded image data is not supported

@subsection Trade-AssimpImporter-limitations-bones Bone import

-   Not supported

@subsection Trade-AssimpImporter-limitations-animations Animation import

-   Not supported

@section Trade-AssimpImporter-configuration Plugin-specific configuration

Assimp has a versatile set of processing operations applied on imported scenes.
The @cb{.conf} [postprocess] @ce group of @ref configuration() contains boolean
toggles that correspond to the [aiPostProcessSteps](http://assimp.sourceforge.net/lib_html/postprocess_8h.html#a64795260b95f5a4b3f3dc1be4f52e410)
enum. Some of them are enabled by default, some not; options for not yet
supported features are omitted. The full form of the configuration is shown
below:

@snippet MagnumPlugins/AssimpImporter/AssimpImporter.conf configuration

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
    -   @ref MeshData3D::importerState() returns `aiMesh`
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

        MAGNUM_ASSIMPIMPORTER_LOCAL Features doFeatures() const override;

        MAGNUM_ASSIMPIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL void doOpenState(const void* state, const std::string& filePath) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL void doOpenFile(const std::string& filename) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL void doClose() override;

        MAGNUM_ASSIMPIMPORTER_LOCAL Int doDefaultScene() override;
        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doSceneCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doCameraCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<CameraData> doCamera(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doObject3DCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Int doObject3DForName(const std::string& name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL std::string doObject3DName(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL std::unique_ptr<ObjectData3D> doObject3D(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doLightCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<LightData> doLight(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doMesh3DCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<MeshData3D> doMesh3D(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doMaterialCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Int doMaterialForName(const std::string& name) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL std::string doMaterialName(UnsignedInt id) override;
        MAGNUM_ASSIMPIMPORTER_LOCAL std::unique_ptr<AbstractMaterialData> doMaterial(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_ASSIMPIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id) override;

        MAGNUM_ASSIMPIMPORTER_LOCAL const void* doImporterState() const override;

        std::unique_ptr<File> _f;
};

}}

#endif
