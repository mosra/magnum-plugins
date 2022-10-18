#ifndef Magnum_Trade_GltfSceneConverter_h
#define Magnum_Trade_GltfSceneConverter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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
 * @brief Class @ref Magnum::Trade::GltfSceneConverter
 * @m_since_latest_{plugins}
 */

#include <Corrade/Containers/Pointer.h>
#include <Magnum/Trade/AbstractSceneConverter.h>

#include "MagnumPlugins/GltfSceneConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_GLTFSCENECONVERTER_BUILD_STATIC
    #ifdef GltfSceneConverter_EXPORTS
        #define MAGNUM_GLTFSCENECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_GLTFSCENECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_GLTFSCENECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_GLTFSCENECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_GLTFSCENECONVERTER_EXPORT
#define MAGNUM_GLTFSCENECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief glTF converter plugin
@m_since_latest_{plugins}

Exports full scenes to either `*.gltf` files with an external `*.bin` buffer or
to a self-contained `*.glb`. You can use @ref GltfImporter to import scenes in
this format.

@experimental

@section Trade-GltfSceneConverter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_GLTFSCENECONVERTER` is enabled when building Magnum Plugins. To use as
a dynamic plugin, load @cpp "GltfSceneConverter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_GLTFSCENECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::GltfSceneConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `GltfSceneConverter` component of
the `MagnumPlugins` package and link to the
`MagnumPlugins::GltfSceneConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED GltfSceneConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::GltfSceneConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-GltfSceneConverter-behavior Behavior and limitations

-   At the moment, alignment rules for the `*.glb` layout are not respected.

@subsection Trade-GltfSceneConverter-behavior-meshes Mesh export

-   The @ref MeshData is exported with its exact binary layout. Only padding
    before and after an index view is omitted, the vertex buffer is saved
    verbatim into the glTF buffer.
-   @ref MeshPrimitive::Points, @relativeref{MeshPrimitive,Lines},
    @relativeref{MeshPrimitive,LineLoop},
    @relativeref{MeshPrimitive,LineStrip},
    @relativeref{MeshPrimitive,Triangles},
    @relativeref{MeshPrimitive,TriangleStrip} and
    @relativeref{MeshPrimitive,TriangleFan} is supported by core glTF.
    Implementation-specific primitive types can't be exported.
-   Both non-indexed and indexed meshes with @ref MeshIndexType::UnsignedShort
    or @ref MeshIndexType::UnsignedInt are supported by core glTF,
    @ref MeshIndexType::UnsignedByte is supported but discouraged.
    Implementation-specific index types and non-contiguous index arrays can't
    be exported.
-   While glTF has a requirement that vertex / index count corresponds to the
    actual primitive type, the exporter doesn't check that at the moment.
    Attribute-less meshes and meshes with zero vertices are not allowed by the
    spec but @ref GltfImporter supports them and they can be exported with a
    warning if you disable the @cb{.ini} strict @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option".
    Attribute-less meshes with a non-zero vertex count are unrepresentable in
    glTF and thus can't be exported.
-   @ref MeshAttribute::Position in @ref VertexFormat::Vector3 is supported by
    core glTF; @relativeref{VertexFormat,Vector3b},
    @relativeref{VertexFormat,Vector3bNormalized},
    @relativeref{VertexFormat,Vector3ub},
    @relativeref{VertexFormat,Vector3ubNormalized},
    @relativeref{VertexFormat,Vector3s},
    @relativeref{VertexFormat,Vector3sNormalized},
    @relativeref{VertexFormat,Vector3us} or
    @relativeref{VertexFormat,Vector3usNormalized} will be exported with
    [KHR_mesh_quantization](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md)
    being added to required extensions. Other formats or 2D positions are not
    supported. At the moment, the position attribute doesn't have the `min` and
    `max` properties calculated even though required by the spec.
-   @ref MeshAttribute::Normal in @ref VertexFormat::Vector3 is supported by
    core glTF; @relativeref{VertexFormat,Vector3bNormalized} and
    @relativeref{VertexFormat,Vector3sNormalized} will be exported with
    [KHR_mesh_quantization](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md)
    being added to required extensions. No other formats are supported.
-   @ref MeshAttribute::Tangent in @ref VertexFormat::Vector4 is supported by
    core glTF; @relativeref{VertexFormat,Vector4bNormalized} and
    @relativeref{VertexFormat,Vector4sNormalized} will be exported with
    [KHR_mesh_quantization](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md)
    being added to required extensions. No other formats are supported.
    Three-component tangents and separate bitangents will be exported as a
    custom `_TANGENT3` / `_BITANGENT` attributes under rules that apply to
    custom attributes as described below.
-   @ref MeshAttribute::TextureCoordinates in @ref VertexFormat::Vector2,
    @relativeref{VertexFormat,Vector2ubNormalized} and
    @relativeref{VertexFormat,Vector2usNormalized} are supported by core glTF.
    The data are by default Y-flipped unless the
    @cb{.ini} textureCoordinateYFlipInMaterial @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option" is
    enabled. @relativeref{VertexFormat,Vector2b},
    @relativeref{VertexFormat,Vector2bNormalized},
    @relativeref{VertexFormat,Vector2ub},
    @relativeref{VertexFormat,Vector2s},
    @relativeref{VertexFormat,Vector2sNormalized} or
    @relativeref{VertexFormat,Vector2us} will be exported with
    [KHR_mesh_quantization](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md)
    being added to required extensions. Those can't be Y-flipped and thus
    require the @cb{.ini} textureCoordinateYFlipInMaterial @ce option to be
    explicitly enabled. Other formats are not supported.
-   @ref MeshAttribute::Color in @ref VertexFormat::Vector3 /
    @relativeref{VertexFormat,Vector4},
    @relativeref{VertexFormat,Vector3ubNormalized} /
    @relativeref{VertexFormat,Vector4ubNormalized} and
    @relativeref{VertexFormat,Vector3usNormalized} /
    @relativeref{VertexFormat,Vector4usNormalized} are supported by core glTF.
    No other formats are supported.
    @ref MeshAttribute::ObjectId in @ref VertexFormat::UnsignedByte and
    @ref VertexFormat::UnsignedShort is exported as `_OBJECT_ID` by default,
    use the @cb{.ini} objectIdAttribute @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option" to
    change the identifier name. glTF allows 32-bit integers only for index
    types, exporting @ref VertexFormat::UnsignedInt is possible with a warning
    if you disable the @cb{.ini} strict @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option".
-   Custom attributes are exported with identifiers set via
    @ref setMeshAttributeName(), and unless the custom attribute corresponds to
    a builtin glTF attribute, the name should be prefixed with a `_` to adhere
    to the spec. If no name was set for an attribute, it's exported with an
    underscore followed by its numeric value extracted using
    @ref meshAttributeCustom(MeshAttribute). Allowed formats are
    @ref VertexFormat::Float, @relativeref{VertexFormat,Byte},
    @relativeref{VertexFormat,ByteNormalized},
    @relativeref{VertexFormat,UnsignedByte},
    @relativeref{VertexFormat,UnsignedByteNormalized},
    @relativeref{VertexFormat,Short},
    @relativeref{VertexFormat,ShortNormalized},
    @relativeref{VertexFormat,UnsignedShort},
    @relativeref{VertexFormat,UnsignedShortNormalized}, its 2-, 3- and
    4-component vector counterparts, @relativeref{VertexFormat,Matrix2x2},
    @relativeref{VertexFormat,Matrix2x2bNormalizedAligned},
    @relativeref{VertexFormat,Matrix2x2sNormalized},
    @relativeref{VertexFormat,Matrix3x3},
    @relativeref{VertexFormat,Matrix3x3bNormalizedAligned},
    @relativeref{VertexFormat,Matrix3x3sNormalizedAligned},
    @relativeref{VertexFormat,Matrix4x4},
    @relativeref{VertexFormat,Matrix4x4bNormalized} and
    @relativeref{VertexFormat,Matrix4x4sNormalized}. Exporting
    @ref VertexFormat,UnsignedInt,
    @relativeref{VertexFormat,Vector2ui},
    @relativeref{VertexFormat,Vector3ui} and
    @relativeref{VertexFormat,Vector4ui} is possible with a warning if you
    disable the @cb{.ini} strict @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option". Signed
    32-bit integers, half-floats, doubles or packed types are not representable
    in glTF, implementation-specific vertex formats can't be exported.
-   @ref MeshAttribute::TextureCoordinates, @ref MeshAttribute::Color, (custom)
    `JOINTS` and `WEIGHTS` attributes are suffixed with `_0`, `_1`, ... for
    each occurence. Second and following occurences of other attributes are
    prefixed with an underscore if not already and suffixed with `_1`, `_2`,
    ..., so e.g. a second position attribute becomes `_POSITION_1`.
-   Mesh name, if passed, is saved into the file. Additionally the buffer views
    and accessors referenced by it will be annotated with mesh ID and name,
    and attribute index and name if the @cb{.ini} accessorNames @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option" is
    enabled.
-   Due to a material and a mesh being tied together in a glTF file, meshes
    that are referenced by a scene are written in the order they are referenced
    from @ref SceneData, and get duplicated (including the name) if the same
    mesh gets used with different materials.
-   At the moment, alignment rules for vertex stride are not respected.
-   At the moment, each attribute has its own dedicated buffer view instead of
    a single view being shared by multiple interleaved attributes. This also
    implies that for single-vertex meshes the buffer view size might sometimes
    be larger than stride, which is not allowed by the spec.

@subsection Trade-GltfSceneConverter-behavior-images Image and texture export

-   Images are converted using a converter specified in the
    @cb{.ini} imageConverter @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option",
    propagating flags set via @ref setFlags() and all configuration options
    from the @cb{.ini} [imageConverter] @ce group to it. An
    @ref AbstractImageConverter plugin manager has to be registered
    using @relativeref{Corrade,PluginManager::Manager::registerExternalManager()}
    for image conversion to work.
-   By default, images are saved as external files for a `*.gltf` output and
    embedded into the buffer for a `*.glb` output. This behavior can be
    overriden using the @cb{.ini} bundleImages @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option" on a
    per-image basis.
-   Core glTF supports only JPEG and PNG file formats. Basis-encoded KTX2 files
    can be saved with the [KHR_texture_basisu](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_basisu/README.md) extension by
    setting @cb{.ini} imageConverter=BasisKtxImageConverter @ce. The
    [MSFT_texture_dds](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/MSFT_texture_dds/README.md)
    and [EXT_texture_webp](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/EXT_texture_webp/README.md)
    extensions are not exported because there's currently no image converter
    capable of saving DDS and WebP files. Other formats (such as TGA,
    OpenEXR...) are not supported by the spec but @ref GltfImporter supports
    them and they can be exported if the @cb{.ini} strict
    @ce @ref Trade-GltfSceneConverter-configuration "configuration option"
    is disabled. Such images are then referenced directly without any
    extension.
-   If the @cb{.ini} experimentalKhrTextureKtx @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option" is
    enabled, generic KTX2 images can be saved with the proposed
    [KHR_texture_ktx](https://github.com/KhronosGroup/glTF/pull/1964)
    extension.
-   Image and texture names, if passed, are saved into the file. Additionally
    the buffer views referenced by embedded images will be annotated with image
    ID and name if the @cb{.ini} accessorNames @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option" is
    enabled.
-   The texture is required to only be added after all images it references
-   At the moment, there's no support for exporting multi-level images even
    though the KTX2 container is capable of storing these.

@subsubsection Trade-GltfSceneConverter-behavior-images-array 2D array texture export

If the @cb{.ini} experimentalKhrTextureKtx @ce
@ref Trade-GltfSceneConverter-configuration "configuration option" is enabled,
the plugin supports also 3D images and 2D array textures using a proposed
[KHR_texture_ktx](https://github.com/KhronosGroup/glTF/pull/1964) extension.

-   Only KTX2 images are supported for 3D, i.e. with either
    @cb{.ini} imageConverter=KtxImageConverter @ce or
    @cb{.ini} imageConverter=BasisKtxImageConverter @ce. They need to have
    @ref ImageFlag3D::Array set.
-   Use a @ref TextureType::Texture2DArray texture to reference the 3D images.
    Due to how the extension is designed, the resulting glTF then has the
    texture duplicated for each layer of the image; @ref GltfImporter then
    @ref Trade-GltfImporter-behavior-textures-array "undoes the duplication again on import".
-   Due to how the extension is designed, the presence of the texture
    referencing the 3D image is *essential* for properly recognizing the image
    as 3D on import. Without it, the image gets recognized as 2D and the import
    will subsequently fail due to the image file not actually being 2D.
-   A material referencing a 2D array texture implicitly uses the first
    (@cpp 0 @ce) layer. Use the `*TextureLayer` attributes (such as
    @ref MaterialAttribute::BaseColorTextureLayer for a
    @ref MaterialAttribute::BaseColorTexture) to specify a layer. The layer
    index has to be smaller than the Z dimension of the image.

@subsection Trade-GltfSceneConverter-behavior-materials Material export

-   Implicitly, only attributes that from glTF material defaults are written to
    save file on size. Enable the @cb{.ini} keepMaterialDefaults @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option" to
    write them as well.
-   Both a separate @ref MaterialAttribute::MetalnessTexture and
    @ref MaterialAttribute::RoughnessTexture as well as a combined
    @ref MaterialAttribute::NoneRoughnessMetallicTexture is supported, but
    either both or neither textures have to be present and have to satisfy glTF
    packing rules as described in
    @ref PbrMetallicRoughnessMaterialData::hasNoneRoughnessMetallicTexture().
-   @ref MaterialAttribute::NormalTextureSwizzle has to be
    @ref MaterialTextureSwizzle::RGB, if present;
    @ref MaterialAttribute::OcclusionTextureSwizzle has to be
    @ref MaterialTextureSwizzle::R, if present.
-   If @ref MaterialType::Flat is present, the
    [KHR_materials_unlit](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_unlit/README.md)
    extension is included in the output. Other @ref MaterialTypes are ignored,
    the material is only filled based on the attributes present.
-   If any
    @ref MaterialAttribute::BaseColorTextureMatrix "MaterialAttribute::*TextureMatrix"
    attributes are present, the [KHR_texture_transform](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_transform/README.md)
    extension is included in the output. At the moment, only offset and scaling
    is written, rotation is ignored with a warning. If the
    @cb{.ini} textureCoordinateYFlipInMaterial @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option" is
    enabled, all material textures will contain an Y-flip transformation in
    addition to any existing transformation.
-   Material names, if passed, are saved into the file
-   The material is required to only be added after all textures it references
-   An informational warning is printed for all attributes that were unused
    due to not having a glTF equivalent, due to referring to a texture but the
    texture attribute isn't present or due to the support not being implemented
    yet. The only exception is Phong @ref MaterialAttribute::DiffuseColor and
    @relativeref{MaterialAttribute,DiffuseTexture} properties if they match the
    corresponding @ref MaterialAttribute::BaseColor /
    @relativeref{MaterialAttribute,BaseColorTexture} attributes. Such
    attributes are produced by @ref GltfImporter for compatibility purposes
    when the @cb{.ini} phongMaterialFallback @ce @ref Trade-GltfImporter-configuration "configuration option"
    is enabled and are redundant.
-   At the moment, custom material properties and layers are not exported

@subsection Trade-GltfSceneConverter-behavior-scenes Scene export

-   Only 3D scenes are supported
-   Only objects with a @ref SceneField::Parent entry are exported, all other
    objects are ignored with a warning. This also implies that object IDs are
    not preserved, as otherwise the glTF would contain a lot of empty
    unreferenced node objects.
-   The @ref SceneField::Parent hierarchy is required to be acyclical
-   To satisfy glTF requirements, if both @ref SceneField::Transformation and
    at least one of @relativeref{SceneField,Translation},
    @relativeref{SceneField,Rotation} and
    @relativeref{SceneField,Scaling} is present, only the TRS component(s) are
    saved into the file and not the matrix, assuming the transformation matrix
    is equivalent to them
-   Object and scene names, if passed, are saved into the file
-   The scene is required to only be added after all meshes and materials it
    references
-   Custom @ref SceneFieldType::Float, @ref SceneFieldType::UnsignedInt and
    @ref SceneFieldType::Int fields are exported if a name is set for them via
    @ref setSceneFieldName(). Custom fields of other types and without a name
    assigned are ignored with a warning.
-   At the moment, only @ref SceneField::Parent,
    @relativeref{SceneField,Transformation},
    @relativeref{SceneField,Translation}, @relativeref{SceneField,Rotation},
    @relativeref{SceneField,Scaling}, @relativeref{SceneField,Mesh}
    and @relativeref{SceneField,MeshMaterial} is exported, other builtin fields
    are ignored with a warning
-   At the moment, duplicate fields including multiple mesh assignments are
    ignored with a warning
-   At the moment, only a single scene can be exported. As a consequence,
    information about the default scene is redundant and thus not written.

@section Trade-GltfSceneConverter-configuration Plugin-specific config

It's possible to tune various output options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/GltfSceneConverter/GltfSceneConverter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_GLTFSCENECONVERTER_EXPORT GltfSceneConverter: public AbstractSceneConverter {
    public:
        /** @brief Plugin manager constructor */
        explicit GltfSceneConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~GltfSceneConverter();

    private:
        MAGNUM_GLTFSCENECONVERTER_LOCAL SceneConverterFeatures doFeatures() const override;
        MAGNUM_GLTFSCENECONVERTER_LOCAL bool doBeginFile(Containers::StringView filename) override;
        MAGNUM_GLTFSCENECONVERTER_LOCAL bool doBeginData() override;
        MAGNUM_GLTFSCENECONVERTER_LOCAL Containers::Optional<Containers::Array<char>> doEndData() override;
        MAGNUM_GLTFSCENECONVERTER_LOCAL void doAbort() override;

        MAGNUM_GLTFSCENECONVERTER_LOCAL void doSetDefaultScene(UnsignedInt id) override;
        MAGNUM_GLTFSCENECONVERTER_LOCAL void doSetObjectName(UnsignedLong object, Containers::StringView name) override;
        MAGNUM_GLTFSCENECONVERTER_LOCAL void doSetSceneFieldName(UnsignedInt field, Containers::StringView name) override;
        MAGNUM_GLTFSCENECONVERTER_LOCAL bool doAdd(const UnsignedInt id, const SceneData& scene, Containers::StringView name) override;

        MAGNUM_GLTFSCENECONVERTER_LOCAL void doSetMeshAttributeName(UnsignedShort attribute, Containers::StringView name) override;
        MAGNUM_GLTFSCENECONVERTER_LOCAL bool doAdd(const UnsignedInt id, const MeshData& mesh, Containers::StringView name) override;

        MAGNUM_GLTFSCENECONVERTER_LOCAL bool doAdd(UnsignedInt id, const MaterialData& material, Containers::StringView name) override;

        MAGNUM_GLTFSCENECONVERTER_LOCAL bool doAdd(UnsignedInt id, const TextureData& texture, Containers::StringView name) override;

        template<UnsignedInt dimensions> MAGNUM_GLTFSCENECONVERTER_LOCAL bool convertAndWriteImage(UnsignedInt id, Containers::StringView name, AbstractImageConverter& imageConverter, const ImageData<dimensions>& image, bool bundleImages);
        MAGNUM_GLTFSCENECONVERTER_LOCAL bool doAdd(UnsignedInt id, const ImageData2D& image, Containers::StringView name) override;
        MAGNUM_GLTFSCENECONVERTER_LOCAL bool doAdd(UnsignedInt id, const ImageData3D& image, Containers::StringView name) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
