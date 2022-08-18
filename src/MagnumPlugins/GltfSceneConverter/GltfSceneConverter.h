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
to a self-contained `*.glb`.

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
    spec but can be exported with a warning if you disable the
    @cb{.ini} strict @ce @ref Trade-GltfSceneConverter-configuration "configuration option".
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
    @relativeref{VertexFormat,Vector2usNormalized} are supported by core glTF;
    @relativeref{VertexFormat,Vector2b},
    @relativeref{VertexFormat,Vector2bNormalized},
    @relativeref{VertexFormat,Vector2ub},
    @relativeref{VertexFormat,Vector2s},
    @relativeref{VertexFormat,Vector2sNormalized} or
    @relativeref{VertexFormat,Vector2us} will be exported with
    [KHR_mesh_quantization](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md)
    being added to required extensions. Other formats are not supported.
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
    and accessors referenced by it will be annotated with mesh ID name, and
    index or attribute name if the @cb{.ini} accessorNames @ce
    @ref Trade-GltfSceneConverter-configuration "configuration option" is
    enabled.
-   At the moment, alignment rules for vertex stride are not respected.
-   At the moment, each attribute has its own dedicated buffer view instead of
    a single view being shared by multiple interleaved attributes. This also
    implies that for single-vertex meshes the buffer view size might sometimes
    be larger than stride, which is not allowed by the spec.

@section Trade-GltfSceneConverter-configuration Plugin-specific config

It's possible to tune various output options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/GltfSceneConverter/GltfSceneConverter.conf config

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

        MAGNUM_GLTFSCENECONVERTER_LOCAL void doSetMeshAttributeName(UnsignedShort attribute, Containers::StringView name) override;
        MAGNUM_GLTFSCENECONVERTER_LOCAL bool doAdd(const UnsignedInt id, const MeshData& mesh, Containers::StringView name) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
