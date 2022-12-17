#ifndef Magnum_Trade_UfbxImporter_h
#define Magnum_Trade_UfbxImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2022 Samuli Raivio <bqqbarbhg@gmail.com>

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
 * @brief Class @ref Magnum::Trade::UfbxImporter
 */

#include <Corrade/Containers/Pointer.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/UfbxImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_UFBXIMPORTER_BUILD_STATIC
    #ifdef UfbxImporter_EXPORTS
        #define MAGNUM_UFBXIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_UFBXIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_UFBXIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_UFBXIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_UFBXIMPORTER_EXPORT
#define MAGNUM_UFBXIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief ufbx importer

@m_keywords{FbxImporter, ObjImporter}

Imports FBX files using [ufbx](https://github.com/bqqbarbhg/ufbx), also
supports OBJ files despite the name.

Supports importing of scene, object, camera, mesh, texture and image data.

This plugin provides `FbxImporter` and `ObjImporter`.

@section Trade-UfbxImporter-usage Usage

This plugin depends on the @ref Trade and @ref MeshTools libraries
and the @ref AnyImageImporter plugin and is built if
`MAGNUM_WITH_UFBXIMPORTER` is enabled when building Magnum Plugins.
To use as a dynamic plugin, load @cpp "UfbxImporter" @ce via @ref
Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(MAGNUM_WITH_UFBXIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::UfbxImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `UfbxImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::UfbxImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED UfbxImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::UfbxImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-UfbxImporter-behavior Behavior and limitations

The plugin supports @ref ImporterFeature::OpenData, @relativeref{ImporterFeature,OpenState} and @relativeref{ImporterFeature,FileCallback} features.
Immediate dependencies are loaded during the initial import meaning the
callback is called with @ref InputFileCallbackPolicy::LoadTemporary.
In case of images, the files are loaded on-demand inside @ref image2D() calls
with @ref InputFileCallbackPolicy::LoadTemporary and
@ref InputFileCallbackPolicy::Close is emitted right after the file is fully
read.

The importer recognizes @ref ImporterFlag::Verbose if built in debug mode
(@ref CORRADE_IS_DEBUG_BUILD or @cpp NDEBUG @ce if not defined). The verbose
logging prints detailed ufbx-internal callstacks on load failure that can be
used for debugging or reporting issues.

@subsection Trade-UfbxImporter-behavior-scene Scene import

-   ufbx supports only a single scene, though in practice it is extremely rare
    to have FBX files containing more than a single scene.
-   FBX files contain an implicit root node that is ignored unless the
    @cb{.ini} preserveRootNode @ce @ref Trade-UfbxImporter-configuration "configuration option" is enabled.
-   FBX files may contain nodes with "geometric transforms" that transform only
    the mesh of the node without affecting children. These are converted to
    unnamed helper nodes in the scene unless the @cb{.ini} geometricTransformNodes @ce
    @ref Trade-UfbxImporter-configuration "configuration option" is explicitly disabled.
-   Imported scenes always have @ref SceneMappingType::UnsignedInt, with
    @ref SceneData::mappingBound() equal to @ref objectCount(). The scene is
    always 3D.
-   All reported objects have a @ref SceneField::Parent (of type
    @ref SceneFieldType::Int), @ref SceneField::Translation (of type @ref SceneFieldType::Vector3d),
    @ref SceneField::Rotation (of type @ref SceneFieldType::Quaterniond),
    @ref SceneField::Scaling (of type @ref SceneFieldType::Vector3d) and
    importer-specific flags @cpp "Visibility" @ce and @cpp "GeometricTransformHelper" @ce
    (of type @ref SceneFieldType::UnsignedByte representing a boolean value).
    These five fields share the same object mapping with
    @ref SceneFieldFlag::ImplicitMapping set.
-   Scene field @cpp "Visibility" @ce specifies whether objects should be visible
    in some application-defined manner.
-   Scene field @cpp "GeometricTransformHelper" @ce is set on synthetic nodes
    that are not part of the original file, but represent FBX files' ability to
    transform geometry without affecting children, see @ref Trade-UfbxImporter-scene-processing
    for further details.
-   If the scene references meshes, a @ref SceneField::Mesh (of type
    @ref SceneFieldType::UnsignedInt) and a @ref SceneField::MeshMaterial (of
    type @ref SceneFieldType::Int) is present, both with
    @ref SceneFieldFlag::OrderedMapping set. Missing material IDs are @cpp -1 @ce.
    If a mesh contains multiple materials it is split into parts and the node
    contains each part as a separate mesh/material entry.
    The same mesh can appear instanced multiple times under many
    nodes with different materials. See @ref Trade-UfbxImporter-instancing for further details.
-   If the scene references cameras, a @ref SceneField::Camera (of type
    @ref SceneFieldType::UnsignedInt) is present, with
    @ref SceneFieldFlag::OrderedMapping set. A single camera can be referenced
    by multiple nodes. See @ref Trade-UfbxImporter-instancing for further
    details.
-   If the scene references lights, a @ref SceneField::Light (of type
    @ref SceneFieldType::UnsignedInt) is present, with
    @ref SceneFieldFlag::OrderedMapping set. A single light can be referenced
    by multiple nodes. See @ref Trade-UfbxImporter-instancing for further
    details.

@subsection Trade-UfbxImporter-behavior-materials Material import

-   Supports both legacy FBX Phong material model and more modern PBR
    materials, in some cases both are defined as PBR materials may have
    a legacy Phong material filled as a fallback.
-   The legacy FBX material model and most PBR material models have factors
    for various attributes, by default these are premultiplied into the value
    but you can retain them using the @cb{.ini} preserveMaterialFactors @ce @ref Trade-UfbxImporter-configuration "configuration option"
-   Two-sided property and alpha mode is not imported.
-   ufbx tries to normalize the various vendor-specific PBR material modes into
    a single set of attributes that are imported, see @ref Trade-UfbxImporter-pbr-attributes
    for an exhaustive listing.
-   @ref MaterialAttribute::DiffuseTextureMatrix and similar matrix attributes
    for other textures are imported.
-   @ref MaterialAttribute::DiffuseTextureCoordinates and similar UV layer
    attributes are not supported.

@subsection Trade-UfbxImporter-behavior-lights Light import

-   @ref LightData::Type::Directional and @ref LightData::Type::Ambient expect
    the attenuation to be constant, but FBX is not required to follow that.
    In that case the attenuation value from the file is ignored.
-   Area and volume lights are not supported

@subsection Trade-UfbxImporter-behavior-meshes Mesh import

-   Vertex creases and any edge or face attributes are not imported
-   The importer follows types used by ufbx truncated to 32-bit floats, thus indices are always
    @ref MeshIndexType::UnsignedInt, positions, normals, tangents and
    bitangents are always imported as @ref VertexFormat::Vector3, texture
    coordinates as @ref VertexFormat::Vector2 and colors as
    @ref VertexFormat::Vector4. Most FBX files contain geometry data natively
    as double-precision floats.
-   If a mesh contains multiple materials it is split into parts and the node
    contains each part as a separate mesh/material entry.
-   If a mesh contains faces with 1 or 2 vertices (ie. points or lines) they
    are separated to meshes with the correct primitives (@ref MeshPrimitive::Points
    and @ref MeshPrimitive::Lines)
-   Faces with more than three vertices are triangulated and represented as
    @ref MeshPrimitive::Triangles.

The meshes are indexed by default unless cb{.ini} generateIndices @ce @ref Trade-UfbxImporter-configuration "configuration option"
is disabled. Vertex position is always defined, normals can be missing unless
cb{.ini} generateMissingNormals @ce @ref Trade-UfbxImporter-configuration "configuration option"
is set. There are an arbitrary amount of UV/tangent/bitangent/color sets, you
can specify a maximum limit using cb{.ini} maxUvSets @ce,
cb{.ini} maxTangentSets @ce, cb{.ini} maxColorSets @ce @ref Trade-UfbxImporter-configuration "configuration options",
note that setting any to zero disables loading any tangents etc.

@section Trade-UfbxImporter-configuration Plugin-specific configuration

It's possible to tune various import options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/UfbxImporter/UfbxImporter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.

@section Trade-UfbxImporter-pbr-attributes PBR material attributes

ufbx normalizes PBR materials into a superset of all vendor-specific PBR material
models that ufbx supports, these attributes are exposed as custom @ref MaterialAttribute
names.

Majority of the attributes match
[Autodesk Standard Surface](https://autodesk.github.io/standard-surface/)
parameters:

Layer | Attribute | Type | OSL parameter
----- | --------- | ---- | -------------
Base | baseFactor [1] | Float | base
Base | BaseColor | Vector4 | base_color
Base | Roughness | Float | specular_roughness
Base | Metalness | Float | metalness
Base | diffuseRoughness | Float | diffuse_roughness
Base | specularFactor [1] | Float | specular
Base | SpecularColor | Vector4 | specular_color
Base | specularIor | Float | specular_IOR
Base | specularAnisotropy | Float | specular_anisotropy
Base | specularRotation | Float | specular_rotation
Base | thinFilmThickness | Float | thin_film_thickness
Base | thinFilmIor | Float | thin_film_IOR
Base | emissiveFactor [1] | Float | emission
Base | EmissiveColor | Vector3 | emission_color
Base | opacity | Vector3 | opacity
ClearCoat | LayerFactor | Float | coat
ClearCoat | color | Vector4 | coat_color
ClearCoat | Roughness | Float | coat_roughness
ClearCoat | ior | Float | coat_IOR
ClearCoat | anisotropy | Float | coat_anisotropy
ClearCoat | rotation | Float | coat_rotation
ClearCoat | NormalTexture | UnsignedInt | coat_normal
transmission | LayerFactor | Float | transmission
transmission | color | Vector4 | transmission_color
transmission | depth | Float | transmission_depth
transmission | scatter | Vector3 | transmission_scatter
transmission | scatterAnisotropy | Float | transmission_scatter_anisotropy
transmission | dispersion | Float | transmission_dispersion
transmission | extraRoughness | Float | transmission_extra_roughness
subsurface | LayerFactor | Float | subsurface
subsurface | color | Vector4 | subsurface_color
subsurface | radius | Vector3 | subsurface_radius
subsurface | scale | Float | subsurface_scale
subsurface | anisotropy | Float | subsurface_anisotropy
sheen | LayerFactor | Float | sheen
sheen | color | Vector3 | sheen_color
sheen | Roughness | Float | sheen_roughness

[1] Requires @cb{.ini} preserveMaterialFactors @ce @ref Trade-UfbxImporter-configuration "configuration option" to be enabled.

Other attributes not defined by the OSL Standard Surface:

Layer | Attribute | Type | Description
----- | --------- | ---- | -----------
Base | Glossiness | Float | Inverse of roughness used by some material models
Base | NormalTexture | UnsignedInt | Tangent-space normal map texture
Base | OcclusionTexture | UnsignedInt | Ambient occlusion texture
Base | tangentTexture | UnsignedInt | Tangent re-orientation texture
Base | displacementTexture | UnsignedInt | Displacement texture
Base | displacementFactor | Float | Displacement texture weight
Base | indirectDiffuse | Float | Factor for indirect diffuse lighting
Base | indirectSpecular | Float | Factor for indirect specular lighting
ClearCoat | Glossiness | Float | Inverse of roughness used by some material models
ClearCoat | affectBaseColor | Float | Modify the base color based on the coat color
ClearCoat | affectBaseRoughness | Float | Modify the base roughness based on the coat roughness
transmission | Roughness | Float | Transmission roughness (base Roughness not added)
transmission | Glossiness | Float | Inverse of roughness used by some material models
transmission | priority | Long | IOR transmission priority
transmission | enableInAov | Bool | Render transmission into AOVs (Arbitrary Output Variable)
subsurface | tintColor | Vector4 | Extra tint color that is multiplied after SSS calculation
subsurface | type | Long | Shader-specific subsurface random walk type
matte | LayerFactor | Float | Matte surface weight
matte | color | Vector3 | Matte surface color

*/
class MAGNUM_UFBXIMPORTER_EXPORT UfbxImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit UfbxImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~UfbxImporter();

    private:
        MAGNUM_UFBXIMPORTER_LOCAL ImporterFeatures doFeatures() const override;

        MAGNUM_UFBXIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_UFBXIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;
        MAGNUM_UFBXIMPORTER_LOCAL void doOpenFile(Containers::StringView filename) override;
        MAGNUM_UFBXIMPORTER_LOCAL void openInternal(void* state, bool fromFile);
        MAGNUM_UFBXIMPORTER_LOCAL void doClose() override;

        MAGNUM_UFBXIMPORTER_LOCAL Int doDefaultScene() const override;
        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doSceneCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL SceneField doSceneFieldForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doSceneFieldName(UnsignedInt name) override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedLong doObjectCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Long doObjectForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doObjectName(UnsignedLong id) override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doCameraCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Int doCameraForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doCameraName(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<CameraData> doCamera(UnsignedInt id) override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doLightCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Int doLightForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doLightName(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<LightData> doLight(UnsignedInt id) override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doMeshCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<MeshData> doMesh(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doMaterialCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Int doMaterialForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doMaterialName(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<MaterialData> doMaterial(UnsignedInt id) override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Int doTextureForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doTextureName(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;

        MAGNUM_UFBXIMPORTER_LOCAL AbstractImporter* setupOrReuseImporterForImage(UnsignedInt id, const char* errorPrefix);

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;
        MAGNUM_UFBXIMPORTER_LOCAL Int doImage2DForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doImage2DName(UnsignedInt id) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
