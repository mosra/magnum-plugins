#ifndef Magnum_Trade_UfbxImporter_h
#define Magnum_Trade_UfbxImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2022, 2023 Samuli Raivio <bqqbarbhg@gmail.com>

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
 * @m_since_latest
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
@brief FBX and OBJ importer using ufbx
@m_since_latest

@m_keywords{FbxImporter ObjImporter}

Imports FBX files using [ufbx](https://github.com/ufbx/ufbx), also
supports OBJ files despite the name.

This plugin provides `FbxImporter` and `ObjImporter`.

@m_class{m-block m-primary}

@thirdparty This plugin makes use of the
    [ufbx](https://github.com/ufbx/ufbx) library by Samuli Raivio, released
    into the @m_class{m-label m-primary} **public domain**
    ([license text](https://github.com/ufbx/ufbx/blob/567defbc0499198fb03636d4a0568d8b6e66fb0c/LICENSE#L22-L38),
    [choosealicense.com](https://choosealicense.com/licenses/unlicense/)),
    or alternatively under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/ufbx/ufbx/blob/567defbc0499198fb03636d4a0568d8b6e66fb0c/LICENSE#L4-L20),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).

@section Trade-UfbxImporter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    through the base @ref AbstractImporter interface. See its documentation for
    introduction and usage examples.

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

The plugin supports @ref ImporterFeature::OpenData and
@relativeref{ImporterFeature,FileCallback} features. Immediate dependencies are
loaded during the initial import meaning the callback is called with
@ref InputFileCallbackPolicy::LoadTemporary. In case of images, the files are
loaded on-demand inside @ref image2D() calls with
@ref InputFileCallbackPolicy::LoadTemporary and
@ref InputFileCallbackPolicy::Close is emitted right after the file is fully
read.

The importer recognizes @ref ImporterFlag::Verbose if built in debug mode
(@ref CORRADE_IS_DEBUG_BUILD defined or @cpp NDEBUG @ce not defined). The
verbose logging prints detailed `ufbx`-internal callstacks on load failure that
can be used for debugging or reporting issues.

@subsection Trade-UfbxImporter-behavior-scene Scene import

-   `ufbx` supports only a single scene, though in practice it is extremely
    rare to have FBX files containing more than a single scene. The only
    exception is OBJ `.mtl` files when opened directly instead of through the
    top-level `.obj` --- those are treated as material libraries and thus
    expose no scene at all.
-   FBX files may contain nodes with "geometric transforms" that transform only
    the mesh of the node without affecting children. These are converted to
    unnamed helper nodes by default, see
    @ref Trade-UfbxImporter-processing-geometry-transforms for further options
    and information.
-   Imported scenes always have @ref SceneMappingType::UnsignedInt, with
    @ref SceneData::mappingBound() equal to @ref objectCount(). The scene is
    always 3D.
-   All reported objects have a @ref SceneField::Parent (of type
    @ref SceneFieldType::Int), @ref SceneField::Translation (of type
    @ref SceneFieldType::Vector3d), @ref SceneField::Rotation (of type
    @ref SceneFieldType::Quaterniond), @ref SceneField::Scaling (of type
    @ref SceneFieldType::Vector3d) and importer-specific flags
    @cpp "Visibility" @ce and @cpp "GeometricTransformHelper" @ce (both of type
    @ref SceneFieldType::Bit). These five fields share the same object mapping
    with @ref SceneFieldFlag::ImplicitMapping set.
-   Scene field @cpp "Visibility" @ce specifies whether objects should be
    visible in some application-defined manner.
-   Scene field @cpp "GeometryTransformHelper" @ce is set on synthetic nodes
    that are not part of the original file, but represent FBX files' ability to
    transform geometry without affecting children, see
    @ref Trade-UfbxImporter-processing-geometry-transforms for further details.
-   If the scene references meshes, a @ref SceneField::Mesh (of type
    @ref SceneFieldType::UnsignedInt) and a @ref SceneField::MeshMaterial (of
    type @ref SceneFieldType::Int) is present, both with
    @ref SceneFieldFlag::OrderedMapping set. Missing material IDs are
    @cpp -1 @ce. If a mesh contains multiple materials it is split into parts
    and the node contains each part as a separate mesh/material entry. The same
    mesh can appear instanced multiple times under many nodes with different
    materials.
-   If the scene references cameras, a @ref SceneField::Camera (of type
    @ref SceneFieldType::UnsignedInt) is present, with
    @ref SceneFieldFlag::OrderedMapping set. A single camera can be referenced
    by multiple nodes.
-   If the scene references lights, a @ref SceneField::Light (of type
    @ref SceneFieldType::UnsignedInt) is present, with
    @ref SceneFieldFlag::OrderedMapping set. A single light can be referenced
    by multiple nodes.
-   The node transformations are expressed in a file dependent units and axes.
    @ref UfbxImporter supports normalizing them to a consistent space, see
    @ref Trade-UfbxImporter-processing-unit-normalization for further details.

@subsection Trade-UfbxImporter-behavior-materials Material import

-   Supports both legacy FBX Phong material model and more modern PBR
    materials, in some cases both are defined as PBR materials may have a
    legacy Phong material filled as a fallback.
-   [PBR material properties in OBJ files](http://exocortex.com/blog/extending_wavefront_mtl_to_support_pbr)
    are supported as well.
-   The legacy FBX material model and most PBR material models have factors for
    various attributes, by default these are premultiplied into the value but
    you can retain them using the @cb{.ini} preserveMaterialFactors @ce
    @ref Trade-UfbxImporter-configuration "configuration option"
-   `ufbx` tries to normalize the various vendor-specific PBR material modes
    into a single set of attributes that are imported, see
    @ref Trade-UfbxImporter-pbr-attributes for an exhaustive listing.
-   @ref MaterialAttribute::DiffuseTextureMatrix and similar matrix attributes
    for other textures are imported.
-   @ref MaterialAttribute::DiffuseTextureCoordinates and similar UV layer
    attributes are not supported, as FBX stores them as UV set names instead of
    layer indices, and the conversion would be fragile.
-   FBX materials have no equivalent for the
    @ref MaterialAttribute::DoubleSided, @ref MaterialAttribute::AlphaMask and
    @ref MaterialAttribute::AlphaBlend properties, and thus they are undefined
    for materials.
-   In addition to opening OBJ files, `ufbx` allows directly opening a `.mtl`
    file, which will result in just the material data being parsed, without any
    meshes or a scene present.

@subsection Trade-UfbxImporter-behavior-lights Light import

-   @ref LightData::Type::Directional and @ref LightData::Type::Ambient expect
    the attenuation to be constant, but FBX is not required to follow that.
    In that case the attenuation value from the file is ignored.
-   Area and volume lights are not supported

@subsection Trade-UfbxImporter-behavior-meshes Mesh import

-   Vertex creases and any edge or face attributes are not imported
-   The importer follows types used by `ufbx` truncated to 32-bit, thus indices
    are always @ref MeshIndexType::UnsignedInt, positions, normals, tangents
    and bitangents are always imported as @ref VertexFormat::Vector3, texture
    coordinates as @ref VertexFormat::Vector2 and colors as
    @ref VertexFormat::Vector4. Most FBX files contain geometry data natively
    as double-precision floats.
-   If a mesh contains multiple materials it is split into parts and the node
    contains each part as a separate mesh/material entry.
-   If a mesh contains faces with 1 or 2 vertices (ie. points or lines) they
    are separated to meshes with the correct primitives
    (@ref MeshPrimitive::Points and @relativeref{MeshPrimitive,Lines})
-   Faces with more than three vertices are triangulated and represented as
    @ref MeshPrimitive::Triangles.

The meshes are indexed by default unless @cb{.ini} generateIndices @ce
@ref Trade-UfbxImporter-configuration "configuration option" is disabled.
Vertex position is always defined, normals can be missing unless
@cb{.ini} generateMissingNormals @ce @ref Trade-UfbxImporter-configuration "configuration option"
is set. There are an arbitrary amount of UV/tangent/bitangent/color sets, you
can specify a maximum limit using @cb{.ini} maxUvSets @ce,
@cb{.ini} maxTangentSets @ce and @cb{.ini} maxColorSets @ce
@ref Trade-UfbxImporter-configuration "configuration options", note that
setting any to zero disables loading any tangents etc.

@subsection Trade-UfbxImporter-behavior-animations Animation and skin import

-   FBX represents rotations using Euler angles, which are converted to
    quaternions on import.
-   Cubic FBX animation curves are resampled as linear interpolation as time
    tangents prevent representing them as cubic Hermite splines, the resampling
    rate can be configured via the @cb{.ini} resampleRate
    @ce @ref Trade-UfbxImporter-configuration "configuration option".
-   FBX represents rotations as Euler angles so even linear rotations are
    resampled by default to preserve the original motion, this can be disabled
    via the @cb{.ini} resampleRotation @ce
    @ref Trade-UfbxImporter-configuration "configuration option".
-   Morph targets are not supported
-   Animation tracks are always imported with
    @ref Animation::Extrapolation::Constant because FBX does not contain any
    extrapolation information.
-   @ref UfbxImporter imports FBX animation stacks as individual animations by
    default. You can specify the @cb{.ini} animationLayers @ce
    @ref Trade-UfbxImporter-configuration "configuration option" to import each
    animation layer as an individual animation. Currently this loses
    information about what layers animation stacks contain.
-   As many FBX exporters resample animations, to avoid double resampling
    @ref UfbxImporter will avoid resampling closely spaced keyframes, this
    interval can be adjusted via the @cb{.ini} minimumSampleRate @ce
    @ref Trade-UfbxImporter-configuration "configuration option".
-   Constant keyframes are represented as nearly equal time keyframes, the
    constant interpolation interval can be adjusted via the
    @cb{.ini} constantInterpolationDuration @ce
    @ref Trade-UfbxImporter-configuration "configuration option".
-   Skin deformers are supported but only the first skin deformer for a mesh is
    imported currently.

@subsection Trade-UfbxImporter-behavior-textures Texture import

-   Only textures with filenames are retained in the imported scene as
    textures.
-   Layered textures are converted into material layers.
-   FBX textures have no defined @ref SamplerFilter, so @ref UfbxImporter sets
    all filters to @ref SamplerFilter::Linear.

@subsection Trade-UfbxImporter-behavior-images Image import

-   Both external and embedded images are supported via the
    @ref AnyImageImporter plugin.
-   Only 2D images are supported.

@section Trade-UfbxImporter-processing Scene processing

Plain FBX files can be tedious to work with as they can contain unusual scene
graph features and may be defined in arbitrary coordinate/unit spaces. `ufbx`
can process the scene representation internally to make it easier to interpret,
which is exposed as @ref Trade-UfbxImporter-configuration "configuration options"
explained below.

@subsection Trade-UfbxImporter-processing-unit-normalization Unit normalization

FBX supports arbitrary coordinate systems and units. The default unit is in
centimeters (which can often be seen as exporting a file from Blender with the
default settings and ending up with a file where everything is 100x larger),
though meters and even inches are reasonably common. The coordinate systems
most commonly are right-handed Y-up or Z-up.

Currently the plugin does not support querying the coordinate/unit system of
the file so the only way to deal with units is using the
@cb{.ini} normalizeUnits @ce @ref Trade-UfbxImporter-configuration "configuration option".
This will normalize the file into the glTF system: units are one meter and the
coordinates are +X right, +Y up, -Z forward (+Z front in FBX terms). By default
the option will adjust the object transforms directly, meaning if you load a
file authored in the glTF space it should look like what it did before
exporting.

If you want to retain the node transformations written in the FBX *file*, you
can set @cb{.ini} unitNormalizationHandling=transformRoot @ce
@ref Trade-UfbxImporter-configuration "in the configuration", this will result
in the scene containing an additional root node containing the unit/axis
transform as transform/rotation/scaling.

@subsection Trade-UfbxImporter-processing-geometry-transforms Geometry transforms

FBX nodes can contain somewhat confusingly named "geometric transforms" which
are referred to "geometry transforms" here for clarity. These transformations
only affect the immediate geometry (or rarely attached light, camera, etc.) of
the node, without transforming the child nodes. Most scene graphs don't
natively support this, including the standard one in @ref SceneData.

By default @ref UfbxImporter creates "helper nodes" that contain the geometry
transforms, so by default they will be transparently supported without any
extra implementation effort. @ref SceneData returned by @ref UfbxImporter
contains an extra @ref SceneField named @cpp "GeometryTransformHelper" @ce
mapped implicitly per node containing information whether a node is a helper or
a normal one.

You can change how @ref UfbxImporter handles the geometry transforms via the
@cb{.ini} geometryTransformHandling @ce @ref Trade-UfbxImporter-configuration "configuration option",
the default option being `helperNodes`.

Using `modifyGeometry` will attempt to elide the helper nodes by modifying the
actual mesh geometry, "baking" the geometry transform into the vertex data.
This will fall back into creating helper nodes if necessary, for example for
lights/cameras or instanced meshes.

Using `preserve` will not modify the scene at all and exposes the original
geometry transform values via three additional @ref SceneField "SceneFields"
per node: @cpp "GeometryTranslation" @ce (@ref SceneFieldType::Vector3d),
@cpp "GeometryRotation" @ce (@ref SceneFieldType::Quaterniond) and
@cpp "GeometryRotation" @ce (@ref SceneFieldType::Vector3d). These behave in
the same way as the standard @ref SceneField::Translation,
@relativeref{SceneField,Rotation} and @relativeref{SceneField,Scaling}, but
affect only the immediate geometry of the node.

@section Trade-UfbxImporter-pbr-attributes PBR material attributes

`ufbx` normalizes PBR materials into a superset of all vendor-specific PBR
material models that `ufbx` supports, these attributes are exposed as custom
@ref MaterialAttribute names.

Majority of the attributes match
[Autodesk Standard Surface](https://autodesk.github.io/standard-surface/)
parameters:

@m_class{m-fullwidth}

Layer  | Attribute | Type | OSL parameter
------ | --------- | ---- | -------------
<em>(base)</em> | baseColorFactor @m_class{m-footnote} [1](#note1) | @relativeref{MaterialAttributeType,Float} | base
<em>(base)</em> | @relativeref{MaterialAttribute,BaseColor} | @relativeref{MaterialAttributeType,Vector4} | base_color
<em>(base)</em> | @relativeref{MaterialAttribute,Roughness} | @relativeref{MaterialAttributeType,Float} | specular_roughness
<em>(base)</em> | @relativeref{MaterialAttribute,Metalness} | @relativeref{MaterialAttributeType,Float} | metalness
<em>(base)</em> | diffuseRoughness | @relativeref{MaterialAttributeType,Float} | diffuse_roughness
<em>(base)</em> | specularColorFactor @m_class{m-footnote} [1](#note1) | @relativeref{MaterialAttributeType,Float} | specular
<em>(base)</em> | @relativeref{MaterialAttribute,SpecularColor} | @relativeref{MaterialAttributeType,Vector4} | specular_color
<em>(base)</em> | specularIor | @relativeref{MaterialAttributeType,Float} | specular_IOR
<em>(base)</em> | specularAnisotropy | @relativeref{MaterialAttributeType,Float} | specular_anisotropy
<em>(base)</em> | specularRotation | @relativeref{MaterialAttributeType,Float} | specular_rotation
<em>(base)</em> | thinFilmThickness | @relativeref{MaterialAttributeType,Float} | thin_film_thickness
<em>(base)</em> | thinFilmIor | @relativeref{MaterialAttributeType,Float} | thin_film_IOR
<em>(base)</em> | emissiveColorFactor @m_class{m-footnote} [1](#note1) | @relativeref{MaterialAttributeType,Float} | emission
<em>(base)</em> | @relativeref{MaterialAttribute,EmissiveColor} | @relativeref{MaterialAttributeType,Vector3} | emission_color
<em>(base)</em> | opacity | @relativeref{MaterialAttributeType,Vector3} | opacity
@relativeref{MaterialLayer,ClearCoat} | @relativeref{MaterialAttribute,LayerFactor} | @relativeref{MaterialAttributeType,Float} | coat
@relativeref{MaterialLayer,ClearCoat} | color | @relativeref{MaterialAttributeType,Vector4} | coat_color
@relativeref{MaterialLayer,ClearCoat} | @relativeref{MaterialAttribute,Roughness} | @relativeref{MaterialAttributeType,Float} | coat_roughness
@relativeref{MaterialLayer,ClearCoat} | ior | @relativeref{MaterialAttributeType,Float} | coat_IOR
@relativeref{MaterialLayer,ClearCoat} | anisotropy | @relativeref{MaterialAttributeType,Float} | coat_anisotropy
@relativeref{MaterialLayer,ClearCoat} | rotation | @relativeref{MaterialAttributeType,Float} | coat_rotation
@relativeref{MaterialLayer,ClearCoat} | @relativeref{MaterialAttribute,NormalTexture} | @relativeref{MaterialAttributeType,UnsignedInt} | coat_normal
transmission | @relativeref{MaterialAttribute,LayerFactor} | @relativeref{MaterialAttributeType,Float} | transmission
transmission | color | @relativeref{MaterialAttributeType,Vector4} | transmission_color
transmission | depth | @relativeref{MaterialAttributeType,Float} | transmission_depth
transmission | scatter | @relativeref{MaterialAttributeType,Vector3} | transmission_scatter
transmission | scatterAnisotropy | @relativeref{MaterialAttributeType,Float} | transmission_scatter_anisotropy
transmission | dispersion | @relativeref{MaterialAttributeType,Float} | transmission_dispersion
transmission | extraRoughness | @relativeref{MaterialAttributeType,Float} | transmission_extra_roughness
subsurface | @relativeref{MaterialAttribute,LayerFactor} | @relativeref{MaterialAttributeType,Float} | subsurface
subsurface | color | @relativeref{MaterialAttributeType,Vector4} | subsurface_color
subsurface | radius | @relativeref{MaterialAttributeType,Vector3} | subsurface_radius
subsurface | scale | @relativeref{MaterialAttributeType,Float} | subsurface_scale
subsurface | anisotropy | @relativeref{MaterialAttributeType,Float} | subsurface_anisotropy
sheen | @relativeref{MaterialAttribute,LayerFactor} | @relativeref{MaterialAttributeType,Float} | sheen
sheen | color | @relativeref{MaterialAttributeType,Vector3} | sheen_color
sheen | @relativeref{MaterialAttribute,Roughness} | @relativeref{MaterialAttributeType,Float} | sheen_roughness

@m_class{m-footnote}

<dl>
<dt id="note1">1.</dt>
<dd>@anchor note1
Requires @cb{.ini} preserveMaterialFactors @ce @ref Trade-UfbxImporter-configuration "configuration option" to be enabled.</dd>
</dl>

Other attributes not defined by the OSL Standard Surface:

@m_class{m-fullwidth}

Layer | Attribute | Type | Description
----- | --------- | ---- | -----------
<em>(base)</em> | @relativeref{MaterialAttribute,Glossiness} | @relativeref{MaterialAttributeType,Float} | Inverse of roughness used by some material models
<em>(base)</em> | @relativeref{MaterialAttribute,NormalTexture} | @relativeref{MaterialAttributeType,UnsignedInt} | Tangent-space normal map texture
<em>(base)</em> | @relativeref{MaterialAttribute,OcclusionTexture} | @relativeref{MaterialAttributeType,UnsignedInt} | Ambient occlusion texture
<em>(base)</em> | tangentTexture | @relativeref{MaterialAttributeType,UnsignedInt} | Tangent re-orientation texture
<em>(base)</em> | displacementTexture | @relativeref{MaterialAttributeType,UnsignedInt} | Displacement texture
<em>(base)</em> | displacementFactor | @relativeref{MaterialAttributeType,Float} | Displacement texture weight
<em>(base)</em> | indirectDiffuse | @relativeref{MaterialAttributeType,Float} | Factor for indirect diffuse lighting
<em>(base)</em> | indirectSpecular | @relativeref{MaterialAttributeType,Float} | Factor for indirect specular lighting
@relativeref{MaterialLayer,ClearCoat} | @relativeref{MaterialAttribute,Glossiness} | @relativeref{MaterialAttributeType,Float} | Inverse of roughness used by some material models
@relativeref{MaterialLayer,ClearCoat} | affectBaseColor | @relativeref{MaterialAttributeType,Float} | Modify the base color based on the coat color
@relativeref{MaterialLayer,ClearCoat} | affectBaseRoughness | @relativeref{MaterialAttributeType,Float} | Modify the base roughness based on the coat roughness
transmission | @relativeref{MaterialAttribute,Roughness} | @relativeref{MaterialAttributeType,Float} | Transmission roughness (base Roughness not added)
transmission | @relativeref{MaterialAttribute,Glossiness} | @relativeref{MaterialAttributeType,Float} | Inverse of roughness used by some material models
transmission | priority | @relativeref{MaterialAttributeType,Long} | IOR transmission priority
transmission | enableInAov | @relativeref{MaterialAttributeType,Bool} | Render transmission into AOVs (Arbitrary Output Variable)
subsurface | tintColor | @relativeref{MaterialAttributeType,Vector4} | Extra tint color that is multiplied after SSS calculation
subsurface | type | @relativeref{MaterialAttributeType,Long} | Shader-specific subsurface random walk type
matte | @relativeref{MaterialAttribute,LayerFactor} | @relativeref{MaterialAttributeType,Float} | Matte surface weight
matte | color | @relativeref{MaterialAttributeType,Vector3} | Matte surface color

@section Trade-UfbxImporter-configuration Plugin-specific configuration

It's possible to tune various import options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/UfbxImporter/UfbxImporter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
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
        MAGNUM_UFBXIMPORTER_LOCAL void openInternal(void* opaqueScene, const void *opaqueOpts, bool fromFile);
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

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doAnimationCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doAnimationName(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL Int doAnimationForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<AnimationData> doAnimation(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL AnimationTrackTarget doAnimationTrackTargetForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doAnimationTrackTargetName(UnsignedShort name) override;

        MAGNUM_UFBXIMPORTER_LOCAL UnsignedInt doSkin3DCount() const override;
        MAGNUM_UFBXIMPORTER_LOCAL Int doSkin3DForName(Containers::StringView name) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::String doSkin3DName(UnsignedInt id) override;
        MAGNUM_UFBXIMPORTER_LOCAL Containers::Optional<SkinData3D> doSkin3D(UnsignedInt id) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
