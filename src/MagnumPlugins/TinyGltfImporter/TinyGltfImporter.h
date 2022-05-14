#ifndef Magnum_Trade_TinyGltfImporter_h
#define Magnum_Trade_TinyGltfImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2018 Tobias Stein <stein.tobi@t-online.de>

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

#ifdef MAGNUM_BUILD_DEPRECATED
/** @file
 * @brief Class @ref Magnum::Trade::TinyGltfImporter
 * @m_deprecated_since_latest Use @ref MagnumPlugins/GltfImporter/GltfImporter.h
 *      and the @relativeref{Magnum,Trade::GltfImporter} class instead.
 */
#endif

#include <Magnum/configure.h>

#ifdef MAGNUM_BUILD_DEPRECATED
#include <Corrade/Utility/Macros.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/PhongMaterialData.h>

#include "MagnumPlugins/TinyGltfImporter/configure.h"

#ifndef _MAGNUM_NO_DEPRECATED_TINYGLTFIMPORTER
CORRADE_DEPRECATED_FILE("use MagnumPlugins/GltfImporter/GltfImporter.h and the GltfImporter class instead")
#endif

#ifndef DOXYGEN_GENERATING_OUTPUT
namespace tinygltf {
    class Model;
    class Value;
}
#endif

namespace Magnum { namespace Trade {

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_TINYGLTFIMPORTER_BUILD_STATIC
    #ifdef TinyGltfImporter_EXPORTS
        #define MAGNUM_TINYGLTFIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_TINYGLTFIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_TINYGLTFIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_TINYGLTFIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_TINYGLTFIMPORTER_EXPORT
#define MAGNUM_TINYGLTFIMPORTER_LOCAL
#endif

/**
@brief TinyGltf importer plugin
@m_deprecated_since_latest Use the *actually lightweight* and more robust
    @ref GltfImporter plugin instead.

@m_keywords{GltfImporter}

Imports glTF and binary glTF using the [TinyGLTF](https://github.com/syoyo/tinygltf)
library.

This plugin provides the `GltfImporter` plugin.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the [TinyGLTF](https://github.com/syoyo/tinygltf)
    library, licensed under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/syoyo/tinygltf/blob/devel/LICENSE),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).
    It requires attribution for public use. TinyGLTF itself uses Niels
    Lohmann's [json.hpp](https://github.com/nlohmann/json), licensed under
    @m_class{m-label m-success} **MIT** as well
    ([license text](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT)).

@section Trade-TinyGltfImporter-usage Usage

This plugin depends on the @ref Trade library and the @ref AnyImageImporter
plugin and is built if `WITH_TINYGLTFIMPORTER` is enabled when building Magnum
Plugins. To use as a dynamic plugin, load @cpp "TinyGltfImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_ANYIMAGEIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum EXCLUDE_FROM_ALL)

set(WITH_TINYGLTFIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::TinyGltfImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `TinyGltfImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::TinyGltfImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED TinyGltfImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::TinyGltfImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-TinyGltfImporter-behavior Behavior and limitations

The plugin supports @ref ImporterFeature::OpenData and
@ref ImporterFeature::FileCallback features. The TinyGLTF library loads
everything during initial import, meaning all external file loading callbacks
are called with @ref InputFileCallbackPolicy::LoadTemporary and the resources
can be safely freed right after the @ref openData() / @ref openFile() function
exits. In case of images, the files are loaded on-demand inside @ref image2D()
calls with @ref InputFileCallbackPolicy::LoadTemporary and
@ref InputFileCallbackPolicy::Close is emitted right after the file is fully
read.

[Percent-encoded](https://datatracker.ietf.org/doc/html/rfc3986#section-2.1)
external file paths are not decoded before loading files. If you need to
support those paths, you can intercept and decode them with a file callback.

The content of the global [`extensionsRequired`](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#specifying-extensions)
array is ignored. If a glTF file requires an unknown extension, the import will
most likely succeed, but some things might be missing or not get imported
correctly.

Import of morph data is not supported at the moment.

@subsection Trade-TinyGltfImporter-behavior-objects Scene import

-   If no @cb{.json} "scene" @ce property is present and the file contains at
    least one scene, @ref defaultScene() returns @cpp 0 @ce instead of
    @cpp -1 @ce. According to the [glTF 2.0 specification](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#scenes)
    the importer is free to not render anything, but the suggested behavior
    would break even some official sample models.
-   Imported scenes always have @ref SceneMappingType::UnsignedInt and are
    always 3D. The @ref objectCount() returns count of all nodes in the file,
    while @ref SceneData::mappingBound() returns an *upper bound* on node IDs
    contained in a particular scene.
-   Nodes that are not referenced by any scene are ignored.
-   All objects contained in a scene have a @ref SceneField::Parent (of type
    @ref SceneFieldType::Int) and a @ref SceneField::ImporterState (of type
    @ref SceneFieldType::Pointer, see @ref Trade-TinyGltfImporter-state below).
    These two fields share the same object mapping, size of which gives count
    of nodes contained in the scene. The mapping is unordered and may be
    sparse if the file contains multiple scenes or nodes not referenced by any
    scene.
-   All nodes that contain transformation matrices or TRS components have a
    @ref SceneField::Transformation (of type @ref SceneFieldType::Matrix4x4).
-   If any node contains a translation, a @ref SceneField::Translation (of type
    @ref SceneFieldType::Vector3) is present; if any node contains a rotation,
    a @ref SceneField::Rotation (of type @ref SceneFieldType::Quaternion) is
    present; if any node contains a scaling, a @ref SceneField::Scaling (of
    type @ref SceneFieldType::Vector3) is present. However, if a node has
    both a transformation matrix and some TRS components, TinyGLTF parses only
    the matrix, ignoring the rest.
-   If the scene references meshes, a @ref SceneField::Mesh (of type
    @ref SceneFieldType::UnsignedInt) is present. If any of the referenced
    meshes have assigned materials, @ref SceneField::MeshMaterial (of type
    @ref SceneFieldType::Int) is present as well. While a single node can only
    reference a single mesh at most, in case it references a multi-primitive
    mesh, it's represented as several @ref SceneField::Mesh (and
    @ref SceneField::MeshMaterial) assignments. See
    @ref Trade-TinyGltfImporter-behavior-meshes and
    @ref Trade-TinyGltfImporter-behavior-materials for further details.
-   If the scene references skins, a @ref SceneField::Skin (of type
    @ref SceneFieldType::UnsignedInt) is present. A single node can only
    reference one skin at most. See
    @ref Trade-TinyGltfImporter-behavior-animations for further details.
-   If the scene references cameras, a @ref SceneField::Camera (of type
    @ref SceneFieldType::UnsignedInt) is present. A single node can only
    reference one camera at most. See
    @ref Trade-TinyGltfImporter-behavior-cameras for further details.
-   If the scene references lights, a @ref SceneField::Light (of type
    @ref SceneFieldType::UnsignedInt) is present. A single node can only
    reference one light at most. See
    @ref Trade-TinyGltfImporter-behavior-lights for further details.
-   If node rotation quaternion is not normalized, the importer prints a
    warning and normalizes it. Can be disabled per-object with the
    @cb{.ini} normalizeQuaternions @ce
    @ref Trade-TinyGltfImporter-configuration "configuration option".

@subsection Trade-TinyGltfImporter-behavior-animations Animation and skin import

-   Linear quaternion rotation tracks are postprocessed in order to make it
    possible to use the faster
    @ref Math::lerp(const Quaternion<T>&, const Quaternion<T>&, T) "Math::lerp()" /
    @ref Math::slerp(const Quaternion<T>&, const Quaternion<T>&, T) "Math::slerp()"
    functions instead of
    @ref Math::lerpShortestPath(const Quaternion<T>&, const Quaternion<T>&, T) "Math::lerpShortestPath()" /
    @ref Math::slerpShortestPath(const Quaternion<T>&, const Quaternion<T>&, T) "Math::slerpShortestPath()". Can be disabled per-animation with the
    @cb{.ini} optimizeQuaternionShortestPath @ce
    @ref Trade-TinyGltfImporter-configuration "configuration option". This
    doesn't affect spline-interpolated rotation tracks.
-   If linear quaternion rotation tracks are not normalized, the importer
    prints a warning and normalizes them. Can be disabled per-animation with
    the @cb{.ini} normalizeQuaternions @ce option, see
    @ref Trade-TinyGltfImporter-configuration "configuration option". This
    doesn't affect spline-interpolated rotation tracks.
-   Skin `skeleton` property is not imported, but you can retrieve it via
    @ref SkinData::importerState() --- see @ref Trade-TinyGltfImporter-state
    for more information
-   Morph targets are not supported
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

@subsection Trade-TinyGltfImporter-behavior-cameras Camera import

-   Cameras in glTF are specified with vertical FoV and vertical:horizontal
    aspect ratio, these values are recalculated for horizontal FoV and
    horizontal:vertical aspect ratio as is common in Magnum

@subsection Trade-TinyGltfImporter-behavior-lights Light import

-   The importer supports the [KHR_lights_punctual](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md)
    extension

@subsection Trade-TinyGltfImporter-behavior-meshes Mesh import

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
-   Joint IDs and weights for skinning are imported as custom vertex attributes
    named "JOINTS_0", "JOINTS_1", etc. and "WEIGHTS_0", "WEIGHTS_1", etc.
    Their mapping to/from a string can be queried using
    @ref meshAttributeName() and @ref meshAttributeForName(). Joint IDs are
    imported as @ref VertexFormat::Vector4ub or @ref VertexFormat::Vector4us.
    Joint weights are imported as @ref VertexFormat::Vector4,
    @ref VertexFormat::Vector4ubNormalized or
    @ref VertexFormat::Vector4usNormalized.
-   Per-vertex object ID attribute is imported as either
    @ref VertexFormat::UnsignedInt, @ref VertexFormat::UnsignedShort or
    @ref VertexFormat::UnsignedByte. By default `_OBJECT_ID` is the recognized
    name, use the @cb{.ini} objectIdAttribute @ce
    @ref Trade-TinyGltfImporter-configuration "configuration option" to change
    the identifier that's being looked for.
-   Multi-primitive meshes are split into individual meshes, nodes that
    reference a multi-primitive mesh have multiple @ref SceneField::Mesh
    (and @ref SceneField::MeshMaterial) entries in the imported @ref SceneData.
-   Attribute-less meshes either with or without an index buffer are supported,
    however since glTF has no way of specifying vertex count for those,
    returned @ref Trade::MeshData::vertexCount() is set to @cpp 0 @ce

Custom and unrecognized vertex attributes of allowed types are present in the
imported meshes as well. Their mapping to/from a string can be queried using
@ref meshAttributeName() and @ref meshAttributeForName(). Attributes with
unsupported types (such as non-normalized integer matrices) cause the import to
fail.

@subsection Trade-TinyGltfImporter-behavior-materials Material import

-   Builtin [metallic/roughness](https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material) material is imported always,
    setting @ref MaterialType::PbrMetallicRoughness on the @ref MaterialData.
    Unfortunately TinyGLTF doesn't provide a way to detect if
    metallic/roughness properties are actually present, so this type is set
    always.
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
-   If the on-by-default @cb{.ini} phongMaterialFallback @ce
    @ref Trade-TinyGltfImporter-configuration "configuration option" is
    enabled, the importer provides a Phong fallback for backwards
    compatibility:
    -   @ref MaterialType::Phong is added to material types
    -   Base color and base color texture along with custom texture coordinate
        set and transformation, if present, is exposed as a diffuse color and
        texture, unless already present together with specular color / texture
        from the specular/glossiness material
    -   All other @ref PhongMaterialData values are is kept at their defaults

@subsection Trade-TinyGltfImporter-behavior-textures Texture and image import

<ul>
<li>Texture type is always @ref Trade::TextureType::Texture2D, as glTF doesn't
support anything else</li>
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
    [KHR_texture_basisu](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_basisu/README.md)
    for Khronos Texture 2.0 images (`*.ktx2`) with [Basis Universal](https://github.com/binomialLLC/basis_universal)
    supercompression and the original provisional `GOOGLE_texture_basis`
    extension for referencing plain Basis Universal files (`*.basis`). There was
    no formal specification of the extension but the use is like below,
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

    While the `mimeType` field isn't checked by the importer, embedded data
    URIs for both extensions *need to have* their prefix set to
    `data:application/octet-stream`. TinyGLTF has a whitelist for data URI
    detection that doesn't know `image/ktx2` or `image/x-basis` and would treat
    the URI as a filename otherwise:

    @code{.json}
    {
        ...
        "images": [
            {
                "mimeType": "image/ktx2",
                "uri": "data:application/octet-stream;base64,..."
            }
        ]
    }
    @endcode
</li>
</ul>

@section Trade-TinyGltfImporter-configuration Plugin-specific config

It's possible to tune various output options through @ref configuration(). See
below for all options and their default values.

@snippet MagnumPlugins/TinyGltfImporter/TinyGltfImporter.conf config

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.

@section Trade-TinyGltfImporter-state Access to internal importer state

Access to the underlying TinyGLTF structures it is provided through
importer-specific data accessors:

-   Calling @ref importerState() returns a pointer to the `tinygltf::Model`
    structure. If you use this class statically, you get the concrete type
    instead of a @cpp const void* @ce pointer as returned by
    @ref AbstractImporter::importerState().
-   Importer state on data class instances returned from this importer return
    pointers to matching TinyGLTF structures:
    -   @ref AnimationData::importerState() returns `tinygltf::Animation`, or
        `nullptr` if the @cb{.ini} mergeAnimationClips @ce option is enabled
    -   @ref MaterialData::importerState() returns `tinygltf::Material`
    -   @ref CameraData::importerState() returns `tinygltf::Camera`
    -   @ref ImageData::importerState() returns `tinygltf::Image`
    -   @ref LightData::importerState() returns `tinygltf::Light`
    -   @ref MeshData::importerState() returns `tinygltf::Mesh`
    -   @ref SceneData::importerState() returns `tinygltf::Scene` and all
        objects have a @ref SceneField::ImporterState with their own
        `tinygltf::Node`
    -   @ref SkinData::importerState() returns `tinygltf::Skin`
    -   @ref TextureData::importerState() returns `tinygltf::Texture`

The TinyGLTF header is installed alsongside the plugin and is accessible like
this:

@code{.cpp}
#include <MagnumExternal/TinyGLTF/tiny_gltf.h>
@endcode
*/
class CORRADE_DEPRECATED("use GltfImporter instead") MAGNUM_TINYGLTFIMPORTER_EXPORT TinyGltfImporter: public AbstractImporter {
    public:
        /**
         * @brief Default constructor
         *
         * In case you want to open images, use
         * @ref TinyGltfImporter(PluginManager::Manager<AbstractImporter>&)
         * instead.
         */
        explicit TinyGltfImporter();

        /**
         * @brief Constructor
         *
         * The plugin needs access to plugin manager for importing images.
         */
        explicit TinyGltfImporter(PluginManager::Manager<AbstractImporter>& manager);

        /** @brief Plugin manager constructor */
        explicit TinyGltfImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~TinyGltfImporter();

        /**
         * @brief Importer state
         *
         * See @ref Trade-TinyGltfImporter-state "class documentation" for more
         * information.
         */
        const tinygltf::Model* importerState() const {
            return static_cast<const tinygltf::Model*>(AbstractImporter::importerState());
        }

    private:
        struct Document;

        MAGNUM_TINYGLTFIMPORTER_LOCAL ImporterFeatures doFeatures() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL bool doIsOpened() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL void doOpenFile(Containers::StringView filename) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL void doClose() override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doAnimationCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::String doAnimationName(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doAnimationForName(Containers::StringView name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<AnimationData> doAnimation(UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<CameraData> doCamera(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doCameraForName(Containers::StringView name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::String doCameraName(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doCameraCount() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<LightData> doLight(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doLightForName(Containers::StringView name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::String doLightName(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doLightCount() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doDefaultScene() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doSceneForName(Containers::StringView name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::String doSceneName(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doSceneCount() const override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedLong doObjectCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Long doObjectForName(Containers::StringView name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::String doObjectName(UnsignedLong id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doSkin3DCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doSkin3DForName(Containers::StringView name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::String doSkin3DName(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<SkinData3D> doSkin3D(UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doMeshCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doMeshForName(Containers::StringView name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::String doMeshName(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<MeshData> doMesh(UnsignedInt id, UnsignedInt level) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL MeshAttribute doMeshAttributeForName(Containers::StringView name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::String doMeshAttributeName(UnsignedShort name) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL bool materialTexture(const char* name, UnsignedInt texture, UnsignedInt texCoord, const tinygltf::Value& extensions, Containers::Array<MaterialAttributeData>& attributes, MaterialAttribute attribute, MaterialAttribute matrixAttribute, MaterialAttribute coordinateAttribute) const;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doMaterialCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doMaterialForName(Containers::StringView name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::String doMaterialName(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<MaterialData> doMaterial(UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doTextureForName(Containers::StringView name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::String doTextureName(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL AbstractImporter* setupOrReuseImporterForImage(UnsignedInt id, const char* errorPrefix);

        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Int doImage2DForName(Containers::StringView name) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::String doImage2DName(UnsignedInt id) override;
        MAGNUM_TINYGLTFIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_TINYGLTFIMPORTER_LOCAL const void* doImporterState() const override;

        Containers::Pointer<Document> _d;
};

}}
#else
#error use MagnumPlugins/GltfImporter/GltfImporter.h and the GltfImporter class instead
#endif

#endif
