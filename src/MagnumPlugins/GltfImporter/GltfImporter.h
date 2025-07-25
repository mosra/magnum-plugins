#ifndef Magnum_Trade_GltfImporter_h
#define Magnum_Trade_GltfImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
              Vladimír Vondruš <mosra@centrum.cz>
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
 * @brief Class @ref Magnum::Trade::GltfImporter
 * @m_since_latest_{plugins}
 */

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/GltfImporter/configure.h"

namespace Magnum { namespace Trade {

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_GLTFIMPORTER_BUILD_STATIC
    #ifdef GltfImporter_EXPORTS
        #define MAGNUM_GLTFIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_GLTFIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_GLTFIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_GLTFIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_GLTFIMPORTER_EXPORT
#define MAGNUM_GLTFIMPORTER_LOCAL
#endif

/**
@brief glTF importer plugin
@m_since_latest_{plugins}

Imports glTF (`*.gltf`) and binary glTF (`*.glb`) files. You can use
@ref GltfSceneConverter to encode scenes into this format.

@section Trade-GltfImporter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    through the base @ref AbstractImporter interface. See its documentation for
    introduction and usage examples.

This plugin depends on the @ref Trade library and the @ref AnyImageImporter
plugin and is built if `MAGNUM_WITH_GLTFIMPORTER` is enabled when building
Magnum Plugins. To use as a dynamic plugin, load @cpp "GltfImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(MAGNUM_WITH_ANYIMAGEIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum EXCLUDE_FROM_ALL)

set(MAGNUM_WITH_GLTFIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::GltfImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `GltfImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::GltfImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED GltfImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::GltfImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-GltfImporter-behavior Behavior and limitations

The plugin supports @ref ImporterFeature::OpenData and
@ref ImporterFeature::FileCallback features. All buffers are loaded on-demand
and kept in memory for any later access. As a result, external file loading
callbacks are called with @ref InputFileCallbackPolicy::LoadPermanent.
Resources returned from file callbacks can only be safely freed after closing
the importer instance. In case of images, the files are loaded on-demand inside
@ref image2D() calls with @ref InputFileCallbackPolicy::LoadTemporary and
@ref InputFileCallbackPolicy::Close is emitted right after the file is fully
read.

The content of the global [extensionsRequired](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#specifying-extensions)
array is checked against all extensions supported by the plugin. If a glTF file
requires an unknown extension, the import will fail. This behaviour can be
disabled with the @cb{.ini} ignoreRequiredExtensions @ce
@ref Trade-GltfImporter-configuration "configuration option".

Import of morph data is not supported at the moment.

The plugin recognizes @ref ImporterFlag::Quiet, which will cause all import
warnings to be suppressed. All @ref ImporterFlags are also propagated to image
importer plugins the importer delegates to.

@subsection Trade-GltfImporter-behavior-objects Scene import

-   Imported scenes always have @ref SceneMappingType::UnsignedInt and are
    always 3D. The @ref objectCount() returns count of all nodes in the file,
    while @ref SceneData::mappingBound() returns an upper bound on node IDs
    contained in a particular scene.
-   Nodes that are not referenced by any scene are ignored.
-   All objects contained in a scene have a @ref SceneField::Parent (of type
    @ref SceneFieldType::Int). Size of this field is the count of nodes
    contained in the scene. The mapping is unordered and may be sparse if the
    file contains multiple scenes or nodes not referenced by any scene.
-   All nodes that contain transformation matrices or TRS components have a
    @ref SceneField::Transformation (of type @ref SceneFieldType::Matrix4x4).
    This field is not present if all such nodes have TRS components, in which
    a matrix is considered redundant. Nodes that don't have any transformation
    matrix nor a TRS component don't have this field assigned.
-   If any node contains a translation, a @ref SceneField::Translation (of type
    @ref SceneFieldType::Vector3) is present; if any node contains a rotation,
    a @ref SceneField::Rotation (of type @ref SceneFieldType::Quaternion) is
    present; if any node contains a scaling, a @ref SceneField::Scaling (of
    type @ref SceneFieldType::Vector3) is present.
-   If the scene references meshes, a @ref SceneField::Mesh (of type
    @ref SceneFieldType::UnsignedInt) is present. If any of the referenced
    meshes have assigned materials, @ref SceneField::MeshMaterial (of type
    @ref SceneFieldType::Int) is present as well. While a single node can only
    reference a single mesh at most, in case it references a multi-primitive
    mesh, it's represented as several @ref SceneField::Mesh (and
    @ref SceneField::MeshMaterial) assignments. See
    @ref Trade-GltfImporter-behavior-meshes and
    @ref Trade-GltfImporter-behavior-materials for further details.
-   If the scene references skins, a @ref SceneField::Skin (of type
    @ref SceneFieldType::UnsignedInt) is present. A single node can only
    reference one skin at most. See
    @ref Trade-GltfImporter-behavior-animations for further details.
-   If the scene references cameras, a @ref SceneField::Camera (of type
    @ref SceneFieldType::UnsignedInt) is present. A single node can only
    reference one camera at most. See
    @ref Trade-GltfImporter-behavior-cameras for further details.
-   If the scene references lights, a @ref SceneField::Light (of type
    @ref SceneFieldType::UnsignedInt) is present. A single node can only
    reference one light at most. See
    @ref Trade-GltfImporter-behavior-lights for further details.
-   If node rotation quaternion is not normalized, the importer prints a
    warning and normalizes it. Can be disabled per-object with the
    @cb{.ini} normalizeQuaternions @ce
    @ref Trade-GltfImporter-configuration "configuration option".
-   Node [extras](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-extras)
    are imported as custom scene fields, with names exposed through
    @ref sceneFieldName() / @ref sceneFieldForName() right upon opening the
    file. The `extras` property has to be an object, otherwise it's ignored
    with a warning. Boolean values are imported as @ref SceneFieldType::Bit,
    numeric values are implicitly imported as @ref SceneFieldType::Float and
    the type can be overriden using the @cb{.ini} [customSceneFieldTypes] @ce
    @ref Trade-GltfImporter-configuration "configuration group", string values
    are imported as @ref SceneFieldType::StringOffset32. Homogeneous arrays
    with boolean, numeric and string values are imported as multiple values
    of the same field, same approach is taken if the `extras` property contains
    the same key multiple times. Objects are processed recursively, with field
    names for nested keys being separated with dots. Other value types,
    heterogeneous arrays and values that don't have a consistent type for given
    key across all nodes are ignored with a warning.
-   @ref SceneField::Mesh and @ref SceneField::MeshMaterial fields are always
    marked with @ref SceneFieldFlag::MultiEntry. No other builtin fields can
    have multiple entries for a single object. Node `extras` that were parsed
    from an array are also marked with @ref SceneFieldFlag::MultiEntry in order
    to unambiguously distinguish them from non-array values.

@subsection Trade-GltfImporter-behavior-animations Animation and skin import

-   Linear quaternion rotation tracks are postprocessed in order to make it
    possible to use the faster
    @ref Math::lerp(const Quaternion<T>&, const Quaternion<T>&, T) "Math::lerp()" /
    @ref Math::slerp(const Quaternion<T>&, const Quaternion<T>&, T) "Math::slerp()"
    functions instead of
    @ref Math::lerpShortestPath(const Quaternion<T>&, const Quaternion<T>&, T) "Math::lerpShortestPath()" /
    @ref Math::slerpShortestPath(const Quaternion<T>&, const Quaternion<T>&, T) "Math::slerpShortestPath()". Can be disabled per-animation with the
    @cb{.ini} optimizeQuaternionShortestPath @ce
    @ref Trade-GltfImporter-configuration "configuration option". This doesn't
    affect spline-interpolated rotation tracks.
-   If linear quaternion rotation tracks are not normalized, the importer
    prints a warning and normalizes them. Can be disabled per-animation with
    the @cb{.ini} normalizeQuaternions @ce
    @ref Trade-GltfImporter-configuration "configuration option". This doesn't
    affect spline-interpolated rotation tracks.
-   Skin `skeleton` property is not imported
-   Morph target animations are not supported
-   Animation tracks are always imported with
    @ref Animation::Extrapolation::Constant, because glTF doesn't support
    anything else
-   It's possible to request all animation clips to be merged into one using
    the @cb{.ini} mergeAnimationClips @ce option in order to for example
    preserve cinematic animations when using the Blender glTF exporter (as it
    otherwise outputs a separate clip for each object). When this option is
    enabled, @ref animationCount() always report either @cpp 0 @ce or
    @cpp 1 @ce and the merged animation has no name. With this option enabled,
    however, it can happen that multiple conflicting tracks affecting the same
    node are merged in the same clip, causing the animation to misbehave.

While the glTF specification allows accessors for animation input and output
and skin inverse bind matrices to be sparse or defined without a backing buffer
view, it's not implemented with an assumption that this functionality is rarely
used, and importing such an animation or skin will fail.

@subsection Trade-GltfImporter-behavior-cameras Camera import

-   Cameras in glTF are specified with vertical FoV and vertical:horizontal
    aspect ratio, these values are recalculated for horizontal FoV and
    horizontal:vertical aspect ratio as is common in Magnum

@subsection Trade-GltfImporter-behavior-lights Light import

-   The importer supports the [KHR_lights_punctual](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md)
    extension

@subsection Trade-GltfImporter-behavior-meshes Mesh import

-   Indices are imported as either @ref MeshIndexType::UnsignedByte,
    @ref MeshIndexType::UnsignedShort or @ref MeshIndexType::UnsignedInt
-   Positions are imported as @ref VertexFormat::Vector3,
    @ref VertexFormat::Vector3ub, @ref VertexFormat::Vector3b,
    @ref VertexFormat::Vector3us, @ref VertexFormat::Vector3s,
    @ref VertexFormat::Vector3ubNormalized,
    @ref VertexFormat::Vector3bNormalized,
    @ref VertexFormat::Vector3usNormalized or
    @ref VertexFormat::Vector3sNormalized (which includes the additional types
    specified by [KHR_mesh_quantization](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md))
-   Normals are imported as @ref VertexFormat::Vector3,
    @ref VertexFormat::Vector3bNormalized or
    @ref VertexFormat::Vector3sNormalized
-   Tangents are imported as @ref VertexFormat::Vector4,
    @ref VertexFormat::Vector4bNormalized or
    @ref VertexFormat::Vector4sNormalized
-   Texture coordinates are imported as @ref VertexFormat::Vector2,
    @ref VertexFormat::Vector2ub, @ref VertexFormat::Vector2b,
    @ref VertexFormat::Vector2us, @ref VertexFormat::Vector2s,
    @ref VertexFormat::Vector2ubNormalized,
    @ref VertexFormat::Vector2bNormalized,
    @ref VertexFormat::Vector2usNormalized or
    @ref VertexFormat::Vector2sNormalized (which includes the additional types
    specified by [KHR_mesh_quantization](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md)). The
    data are by default Y-flipped on import unless
    @cb{.ini} textureCoordinateYFlipInMaterial @ce is either explicitly
    enabled, or if the file contains non-normalized integer or normalized
    signed integer texture coordinates (which can't easily be flipped). In that
    case texture coordinate data are kept as-is and materials provide a texture
    transformation that does the Y-flip instead.
-   Colors are imported as @ref VertexFormat::Vector3,
    @ref VertexFormat::Vector4,
    @ref VertexFormat::Vector3ubNormalized,
    @ref VertexFormat::Vector4ubNormalized,
    @ref VertexFormat::Vector3usNormalized or
    @ref VertexFormat::Vector4usNormalized
-   Skin joint IDs are imported as *arrays* of @ref VertexFormat::UnsignedInt,
    @ref VertexFormat::UnsignedByte or @ref VertexFormat::UnsignedShort, skin
    weights then as arrays of @ref VertexFormat::Float,
    @ref VertexFormat::UnsignedByteNormalized or
    @ref VertexFormat::UnsignedShortNormalized. The
    @ref MeshData::attributeArraySize() is currently always @cpp 4 @ce, but is
    reserved to change (for example representing two consecutive sets as a
    single array of 8 items). For backwards compatibility, unless the
    @cb{.ini} compatibilitySkinningAttributes @ce
    @ref Trade-GltfImporter-configuration "configuration option" or
    @ref MAGNUM_BUILD_DEPRECATED is disabled, these are also exposed as custom
    @cpp "JOINTS" @ce and @cpp "WEIGHTS" @ce attributes with
    @ref VertexFormat::Vector4ub /
    @ref VertexFormat::Vector4us and @ref VertexFormat::Vector4 /
    @ref VertexFormat::Vector4ubNormalized /
    @ref VertexFormat::Vector4usNormalized (non-array) formats, respectively,
    with as many instances of these as needed to cover all array items. The
    compatibility attributes *alias* the builtin ones, i.e. point to the same
    memory, so their presence causes no extra overhead.
-   Per-vertex object ID attribute is imported as either
    @ref VertexFormat::UnsignedInt, @ref VertexFormat::UnsignedShort or
    @ref VertexFormat::UnsignedByte. By default `_OBJECT_ID` is the recognized
    name, use the @cb{.ini} objectIdAttribute @ce
    @ref Trade-GltfImporter-configuration "configuration option" to change
    the identifier that's being looked for.
-   If a builtin attribute doesn't match the above-specified types, a message
    is printed to @relativeref{Magnum,Warning} and it's imported as a custom
    attribute instead, with its name available through
    @ref meshAttributeName(). Such attributes are an invalid glTF but this
    allows the application to import them and fix in a postprocessing step.
    Enable the @cb{.ini} strict @ce @ref Trade-GltfImporter-configuration "configuration option"
    to fail the import in such cases instead.
-   Multi-primitive meshes are split into individual meshes, nodes that
    reference a multi-primitive mesh have multiple @ref SceneField::Mesh
    (and @ref SceneField::MeshMaterial) entries in the imported @ref SceneData.
-   Attribute-less meshes either with or without an index buffer are supported,
    however since glTF has no way of specifying vertex count for those,
    returned @ref Trade::MeshData::vertexCount() is set to @cpp 0 @ce
-   Morph targets, if present, have their attributes imported with
    @ref Trade::MeshData::attributeMorphTargetId() set to index of the morph
    target. Only 128 morph targets is supported at most.

By default, the mesh import silently allows certain features that aren't
strictly valid according to the glTF specification, such as 32-bit integer
@ref VertexFormat, because they're useful in general. Enable the
@cb{.ini} strict @ce @ref Trade-GltfImporter-configuration "configuration option"
to fail the import in such cases instead.

Custom and unrecognized vertex attributes of allowed types are present in the
imported meshes as well. Their mapping to/from a string can be queried using
@ref meshAttributeName() and @ref meshAttributeForName(). Attributes with
unsupported types (such as non-normalized integer matrices) cause the import to
fail.

As glTF vertex data may span any buffer subranges, the importer copies just the
actually used buffer ranges, aligning each to four bytes. Other than that, the
vertex data isn't repacked in any way and interleaved attributes keep their
layout even if it contains gaps with an assumption that such layout was done
for a reason. You can use @ref MeshTools::interleave() and other @ref MeshTools
algorithms to perform layout optimizations post import, if needed.

Accessors with no backing buffer views, which are meant to be zero-filled, and
sparse accessors, are deinterleaved and put at the end of the vertex data,
again aligning each to four bytes. Unlike with regular accessors, if there is
more than one attribute using the same zero-filled or sparse accessor, no
deduplication is performed. If a sparse accessor is based off data interleaved
with other attributes in the same mesh, the original data may be left in the
vertex data array even if not referenced. Again you can use
@ref MeshTools::interleave(), @ref MeshTools::filterAttributes() and other
@ref MeshTools to repack the data post import if needed.

While the glTF specification allows accessors for indices -- as opposed to
attributes --- to be sparse or defined without a backing buffer view, it's not
implemented with an assumption that this functionality is rarely used, and
importing such a mesh will fail.

@subsection Trade-GltfImporter-behavior-materials Material import

-   If present, builtin [metallic/roughness](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material) material is imported,
    setting @ref MaterialType::PbrMetallicRoughness on the @ref MaterialData.
-   If the [KHR_materials_pbrSpecularGlossiness](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Archived/KHR_materials_pbrSpecularGlossiness/README.md)
    extension is present, its properties are imported with
    @ref MaterialType::PbrSpecularGlossiness present in material types.
-   Additional normal, occlusion and emissive maps are imported, together with
    related properties
-   If the [KHR_materials_unlit](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_unlit/README.md)
    extension is present, @ref MaterialType::Flat is set in material types,
    replacing @ref MaterialType::PbrMetallicRoughness or
    @ref MaterialType::PbrSpecularGlossiness.
-   If the [KHR_materials_clearcoat](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_clearcoat/README.md)
    extension is present, @ref MaterialType::PbrClearCoat is set in material
    types, and a new layer with clearcoat properties is added
-   Custom texture coordinate sets as well as [KHR_texture_transform](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_transform/README.md)
    properties are imported on all textures.
-   Unrecognized material extensions are imported as custom layers with a `#`
    prefix. Extension properties are imported with their raw names and types,
    the following of which are supported:
    -   @ref MaterialAttributeType::String
    -   @ref MaterialAttributeType::Bool
    -   All numbers as @ref MaterialAttributeType::Float to avoid inconsistency
        with different glTF exporters. The only exception are texture indices
        and coordinate sets inside [textureInfo](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-textureinfo)
        objects, which get imported as @ref MaterialAttributeType::UnsignedInt,
        consistently with types of builtin `*Texture` and `*TextureCoordinates`
        @ref MaterialAttribute entries.
    -   Number arrays as @ref MaterialAttributeType::Vector2 /
        @ref MaterialAttributeType::Vector3 /
        @ref MaterialAttributeType::Vector4.
        Empty arrays, arrays of size 5 or higher as well as arrays containing
        anything that isn't a number are ignored.
    -   [textureInfo](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-textureinfo)
        objects, including all attributes handled for regular textures. Texture
        attributes are prefixed by the name of the object: e.g. if an extension
        has a `someTexture` property, the texture index, matrix, coordinate set
        and scale would be imported as `someTexture`, `someTextureMatrix`,
        `someTextureCoordinates` and `someTextureScale`, consistently with
        builtin texture-related @ref MaterialAttribute names. Non-texture
        object types are ignored.
    If you handle any of these custom material extensions, it may make sense
    to enable the @cb{.ini} ignoreRequiredExtensions @ce
    @ref Trade-GltfImporter-configuration "configuration option".
-   [Extras](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-extras)
    metadata is imported into the base material layer. The `extras` property
    has to be an object, otherwise it's ignored with a warning. Type support is
    the same as for unrecognized material extensions, except for [textureInfo](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-textureinfo)
    objects --- contrary to glTF material extensions, where sub-objects can be
    assumed to contain texture info, the `extras` can contain just anything.
-   If the on-by-default @cb{.ini} phongMaterialFallback @ce
    @ref Trade-GltfImporter-configuration "configuration option" is
    enabled, the importer provides a Phong fallback for backwards
    compatibility:
    -   @ref MaterialType::Phong is added to material types
    -   Base color and base color texture along with custom texture coordinate
        set and transformation, if present, is exposed as a diffuse color and
        texture, unless already present together with specular color / texture
        from the specular/glossiness material
    -   All other @ref PhongMaterialData values are is kept at their defaults

@subsection Trade-GltfImporter-behavior-textures Texture and image import

<ul>
<li>Texture type is @ref Trade::TextureType::Texture2D. If the
@cb{.ini} experimentalKhrTextureKtx @ce @ref Trade-GltfImporter-configuration "configuration option"
is enabled, it can be also @ref Trade::TextureType::Texture2DArray, as
@ref Trade-GltfImporter-behavior-textures-array "described below".</li>
<li>Z coordinate of @ref Trade::TextureData::wrapping() is always
@ref SamplerWrapping::Repeat, as glTF doesn't support 3D textures</li>
<li>
@m_class{m-nopadb}

glTF leaves the defaults of sampler properties to the application, the
following defaults have been chosen for this importer:

-   Minification/magnification/mipmap filter: @ref SamplerFilter::Linear,
    @ref SamplerMipmap::Linear
-   Wrapping (all axes): @ref SamplerWrapping::Repeat
</li>
<li>
    The importer supports the following extensions for image types not defined
    in the [core glTF 2.0 specification](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#gltf-basics):
    [MSFT_texture_dds](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/MSFT_texture_dds/README.md)
    for DirectDraw Surface images (`*.dds`),
    [EXT_texture_webp](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/EXT_texture_webp/README.md)
    for WebP images (`*.webp`),
    [EXT_texture_astc](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/EXT_texture_astc/README.md)
    for Khronos Texture 2.0 images (`*.ktx2`) with ASTC block compression,
    [KHR_texture_basisu](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_basisu/README.md)
    for Khronos Texture 2.0 images (`*.ktx2`) with [Basis Universal](https://github.com/binomialLLC/basis_universal)
    supercompression, as well as the original provisional `GOOGLE_texture_basis`
    extension for referencing plain Basis Universal files (`*.basis`). There
    was no formal specification of the extension but the use is like below,
    [equivalently to Basis own glTF example](https://github.com/BinomialLLC/basis_universal/blob/1cae1d57266e2c95bc011b0bf1ccb9940988c184/webgl/gltf/assets/AgiHqSmall.gltf#L230-L240):

    @code{.json}
    {
        ...
        "textures": [
            {
                "extensions": {
                    "GOOGLE_texture_basis": {
                        "source": 0
                    }
                }
            }
        ],
        "images": [
            {
                "mimeType": "image/x-basis",
                "uri": "texture.basis"
            }
        ],
        "extensionsUsed": [
            "GOOGLE_texture_basis"
        ],
        "extensionsRequired": [
            "GOOGLE_texture_basis"
        ]
    }
    @endcode

    The MIME type (if one exists) is ignored by the importer. Delegation to the
    correct importer alias happens via @ref AnyImageImporter which uses the
    file extension or buffer content to determine the image type.
</li>
<li>If a texture contains and extension together with a fallback source, or
multiple extensions, the image referenced by the first recognized extension
appearing in the file will be picked, others ignored.</li>
</ul>

@subsubsection Trade-GltfImporter-behavior-textures-array 2D array texture support

If the @cb{.ini} experimentalKhrTextureKtx @ce
@ref Trade-GltfImporter-configuration "configuration option" is enabled,
textures can contain also the proposed [KHR_texture_ktx](https://github.com/KhronosGroup/glTF/pull/1964) extension. Besides having an ability to use 2D
`*.ktx2` files the same way as other texture extensions listed above, it can
also reference 2D array textures. The importer behavior is as follows and may
get adapted to eventual changes in the extension proposal:

-   If a `texture` object contains the `KHR_texture_ktx` extension with a
    `layer` property, the image it references is assumed to be a 2D array.
    Otherwise, it's assumed to be 2D. The `*.ktx2` file itself is not checked
    upfront as that goes against the goal of accessing the data only once
    actually needed.
-   If the same image is referenced as both 2D and 2D array, it's an import
    error. In other words, a texture referencing a 2D array image has to have
    the `layer` property even if it would be zero.
-   Images that are assumed to be 2D arrays are available through
    @ref image3D() and related APIs, other images through @ref image2D(). The
    sum of @ref image2DCount() and @ref image3DCount() is the count of glTF
    image objects in the file.
-   Textures referencing the same 2D array image are collapsed together and
    are exposed as a @ref TextureData with @ref TextureType::Texture2DArray,
    with @ref TextureData::image() giving an index of a 3D image instead of a
    2D one. Differing samplers are not taken into account, the resulting
    @ref TextureData will always use the sampler properties referenced from the
    first texture. This means that, in this case, @ref textureCount() may
    be less the actual count of glTF texture objects in the file.
-   Materials referencing `KHR_texture_ktx` textures with the `layer` property
    then get a `*TextureLayer` attribute. I.e., if
    @ref MaterialAttribute::BaseColorTexture is a 2D array texture, the
    material will get a @ref MaterialAttribute::BaseColorTextureLayer as well,
    containing the value of the `layer` property.

@section Trade-GltfImporter-configuration Plugin-specific configuration

It's possible to tune various output options through @ref configuration(). See
below for all options and their default values.

@snippet MagnumPlugins/GltfImporter/GltfImporter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.

@section Trade-GltfImporter-state Access to internal importer state

The glTF JSON is internally parsed using @relativeref{Corrade,Utility::Json}
and you can access the parsed content through importer-specific data accessors.

-   Calling @ref importerState() returns a pointer to the
    @relativeref{Corrade,Utility::Json} instance. If you use this class
    statically, you get the concrete type instead of a @cpp const void* @ce
    pointer as returned by @ref AbstractImporter::importerState(). If not, it's
    allowed to cast away the @cpp const @ce on a mutable importer instance to
    access the parsing APIs.
-   Importer state on data class instances returned from this importer return
    pointers to @relativeref{Corrade,Utility::JsonTokenData} of particular glTF
    objects:
    -   @ref AnimationData::importerState() returns a glTF animation object, or
        `nullptr` if the @cb{.ini} mergeAnimationClips @ce option is enabled
    -   @ref CameraData::importerState() returns a glTF camera object
    -   @ref ImageData::importerState() returns a glTF image object
    -   @ref LightData::importerState() returns a glTF light object
    -   @ref MaterialData::importerState() returns a glTF material object
    -   @ref MeshData::importerState() returns a glTF mesh primitive object.
        You can access the enclosing mesh object in a third-level
        @relativeref{Corrade,Utility::JsonToken::parent()}.
    -   @ref SceneData::importerState() returns a glTF scene object and all
        objects have a @ref SceneField::ImporterState with their own glTF node
        object
    -   @ref SkinData::importerState() returns a glTF skin object
    -   @ref TextureData::importerState() returns a glTF texture object. You
        can access the glTF sampler object by going through the top-level glTF
        object accessible via @relativeref{Corrade,Utility::Json::root()}.

Be aware that not all of the JSON may be parsed when accessed --- where
possible, the importerimplementation defers parsing only to when a particular
data is accessed, and tokens unrecognized by the importers may be left
unparsed. In order to parse what you need, do it through the
@relativeref{Corrade,Utility::Json} instance that gets made mutable first. For
example, in order to access the contents of the
[CESIUM_primitive_outline](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/CESIUM_primitive_outline/README.md)
extension on a glTF mesh primitive object:

@snippet GltfImporter.cpp importerState
*/
class MAGNUM_GLTFIMPORTER_EXPORT GltfImporter: public AbstractImporter {
    public:
        #ifdef MAGNUM_BUILD_DEPRECATED
        /**
         * @brief Default constructor
         * @m_deprecated_since_latest Direct plugin instantiation isn't a
         *      supported use case anymore, instantiate through the plugin
         *      manager instead.
         */
        CORRADE_DEPRECATED("instantiate through the plugin manager instead") explicit GltfImporter();

        /**
         * @brief Constructor
         * @m_deprecated_since_latest Direct plugin instantiation isn't a
         *      supported use case anymore, instantiate through the plugin
         *      manager instead.
         */
        CORRADE_DEPRECATED("instantiate through the plugin manager instead") explicit GltfImporter(PluginManager::Manager<AbstractImporter>& manager);
        #endif

        /** @brief Plugin manager constructor */
        explicit GltfImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~GltfImporter();

        /**
         * @brief Importer state
         *
         * See @ref Trade-GltfImporter-state "class documentation" for more
         * information.
         */
        Utility::Json* importerState() {
            return static_cast<Utility::Json*>(const_cast<void*>(AbstractImporter::importerState()));
        }
        /** @overload */
        const Utility::Json* importerState() const {
            return static_cast<const Utility::Json*>(AbstractImporter::importerState());
        }

    private:
        struct Document;
        /* Returned by internal APIs below, thus have to be declared here */
        struct BufferView;
        struct Accessor;

        MAGNUM_GLTFIMPORTER_LOCAL ImporterFeatures doFeatures() const override;

        MAGNUM_GLTFIMPORTER_LOCAL bool doIsOpened() const override;

        MAGNUM_GLTFIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;
        MAGNUM_GLTFIMPORTER_LOCAL void doOpenFile(Containers::StringView filename) override;
        MAGNUM_GLTFIMPORTER_LOCAL void doClose() override;

        MAGNUM_GLTFIMPORTER_LOCAL UnsignedInt doAnimationCount() const override;
        MAGNUM_GLTFIMPORTER_LOCAL Int doAnimationForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doAnimationName(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<AnimationData> doAnimation(UnsignedInt id) override;

        MAGNUM_GLTFIMPORTER_LOCAL UnsignedInt doCameraCount() const override;
        MAGNUM_GLTFIMPORTER_LOCAL Int doCameraForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doCameraName(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<CameraData> doCamera(UnsignedInt id) override;

        MAGNUM_GLTFIMPORTER_LOCAL UnsignedInt doLightCount() const override;
        MAGNUM_GLTFIMPORTER_LOCAL Int doLightForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doLightName(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<LightData> doLight(UnsignedInt id) override;

        MAGNUM_GLTFIMPORTER_LOCAL Int doDefaultScene() const override;

        MAGNUM_GLTFIMPORTER_LOCAL UnsignedInt doSceneCount() const override;
        MAGNUM_GLTFIMPORTER_LOCAL Int doSceneForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doSceneName(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL SceneField doSceneFieldForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doSceneFieldName(SceneField name) override;

        MAGNUM_GLTFIMPORTER_LOCAL UnsignedLong doObjectCount() const override;
        MAGNUM_GLTFIMPORTER_LOCAL Long doObjectForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doObjectName(UnsignedLong id) override;

        MAGNUM_GLTFIMPORTER_LOCAL UnsignedInt doSkin3DCount() const override;
        MAGNUM_GLTFIMPORTER_LOCAL Int doSkin3DForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doSkin3DName(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<SkinData3D> doSkin3D(UnsignedInt id) override;

        MAGNUM_GLTFIMPORTER_LOCAL UnsignedInt doMeshCount() const override;
        MAGNUM_GLTFIMPORTER_LOCAL Int doMeshForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doMeshName(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<MeshData> doMesh(UnsignedInt id, UnsignedInt level) override;
        MAGNUM_GLTFIMPORTER_LOCAL MeshAttribute doMeshAttributeForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doMeshAttributeName(MeshAttribute name) override;

        MAGNUM_GLTFIMPORTER_LOCAL UnsignedInt doMaterialCount() const override;
        MAGNUM_GLTFIMPORTER_LOCAL Int doMaterialForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doMaterialName(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<MaterialData> doMaterial(UnsignedInt id) override;

        MAGNUM_GLTFIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_GLTFIMPORTER_LOCAL Int doTextureForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doTextureName(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;

        MAGNUM_GLTFIMPORTER_LOCAL AbstractImporter* setupOrReuseImporterForImage(const char* errorPrefix, UnsignedInt id, UnsignedInt expectedDimensions);

        MAGNUM_GLTFIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_GLTFIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL Int doImage2DForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doImage2DName(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_GLTFIMPORTER_LOCAL UnsignedInt doImage3DCount() const override;
        MAGNUM_GLTFIMPORTER_LOCAL UnsignedInt doImage3DLevelCount(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL Int doImage3DForName(Containers::StringView name) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::String doImage3DName(UnsignedInt id) override;
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<ImageData3D> doImage3D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<Containers::Array<char>> loadUri(const char* errorPrefix, Containers::StringView uri);
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<Containers::ArrayView<const char>> parseBuffer(const char* const errorPrefix, UnsignedInt id);
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<BufferView> parseBufferView(const char* errorPrefix, UnsignedInt bufferViewId);
        MAGNUM_GLTFIMPORTER_LOCAL Containers::Optional<Accessor> parseAccessor(const char* const errorPrefix, UnsignedInt accessorId);
        MAGNUM_GLTFIMPORTER_LOCAL bool materialTexture(Utility::JsonToken gltfTexture, Containers::Array<MaterialAttributeData>& attributes, Containers::StringView attribute, Containers::StringView extraAttributePrefix, bool warningOnly = false);
        MAGNUM_GLTFIMPORTER_LOCAL bool materialTexture(Utility::JsonToken gltfTexture, Containers::Array<MaterialAttributeData>& attributes, Containers::StringView attribute, bool warningOnly = false);

        MAGNUM_GLTFIMPORTER_LOCAL const void* doImporterState() const override;

        Containers::Pointer<Document> _d;
};

}}

#endif
