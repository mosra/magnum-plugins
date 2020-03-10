#ifndef Magnum_Trade_StanfordImporter_h
#define Magnum_Trade_StanfordImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>

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
 * @brief Class @ref Magnum::Trade::StanfordImporter
 */

#include <Corrade/Containers/Pointer.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/StanfordImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_STANFORDIMPORTER_BUILD_STATIC
    #ifdef StanfordImporter_EXPORTS
        #define MAGNUM_STANFORDIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_STANFORDIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_STANFORDIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_STANFORDIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_STANFORDIMPORTER_EXPORT
#define MAGNUM_STANFORDIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief Stanford PLY importer plugin

Supports little and big endian binary formats with triangle and quad meshes,
importing vertex positions and vertex colors (if any).

@section Trade-StanfordImporter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_STANFORDIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "StanfordImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_STANFORDIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::StanfordImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `StanfordImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::StanfordImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED StanfordImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::StanfordImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

@section Trade-StanfordImporter-behavior Behavior and limitations

In order to optimize for fast import, the importer supports a restricted subset
of PLY features, which however shouldn't affect any real-world models.

-   Both Big-Endian and Little-Endian binary files are supported, with bytes
    swapped to match platform endianness. ASCII files are not supported due to
    the storage size overhead and inherent inefficiency of float literal
    parsing.
-   Position coordinates are expected to be tightly packed in a X/Y/Z order in
    order to be directly reinterpretable as a @ref Magnum::Vector3 "Vector3".
    Same goes for colors, expected channel order is R/G/B.
-   All position coordinates are expected to have the same type, and be either
    32-bit floats or (signed) bytes or shorts. Resulting position type is then
    @ref VertexFormat::Vector3, @ref VertexFormat::Vector3ub,
    @ref VertexFormat::Vector3b, @ref VertexFormat::Vector3us or
    @ref VertexFormat::Vector3s. Integer and double positions are not
    supported.
-   All color coordinates are expected to have the same type, and be either
    32-bit floats or unsigned bytes/shorts. Alpha is supported as well,
    resulting color type is then @ref VertexFormat::Vector3 /
    @ref VertexFormat::Vector4, @ref VertexFormat::Vector3ub /
    @ref VertexFormat::Vector4ub or @ref VertexFormat::Vector3us /
    @ref VertexFormat::Vector4us. Signed, 32-bit integer and double colors
    are not supported.
-   Indices are imported as either @ref MeshIndexType::UnsignedByte,
    @ref MeshIndexType::UnsignedShort or @ref MeshIndexType::UnsignedInt. Quads
    are triangulated, but higher-order polygons are not supported. Because
    there are real-world files with signed indices, signed types are allowed
    for indices as well, but interpreted as unsigned (because negative values
    wouldn't make sense anyway).

The mesh is always indexed; positions are always present, color coordinates
are optional. The file format doesn't support normals. You can generate them
using either @ref MeshTools::generateFlatNormals() /
@ref MeshTools::generateSmoothNormals() or by passing
@ref MeshTools::CompileFlag::GenerateFlatNormals /
@ref MeshTools::CompileFlag::GenerateSmoothNormals to @ref MeshTools::compile().

@subsection Trade-StanfordImporter-behavior-per-face Per-face attributes

The importer provides access to per-face attributes in a second mesh level ---
i.e., @ref meshLevelCount() always returns @cpp 2 @ce, and calling @ref mesh()
with @p level set to @cpp 1 @ce will return a @ref MeshData instance with
@ref MeshPrimitive::Faces. The faces are triangulated, which means each item
contains data for each triangle in the @ref MeshPrimitive::Triangles mesh at
@p level @cpp 0 @ce.

@subsection Trade-StanfordImporter-behavior-custom-attributes Custom attributes

Custom and unrecognized vertex and face attributes of known types are present
in the imported meshes as well. Their mapping to/from a string can be queried
using @ref meshAttributeName() and @ref meshAttributeForName(). Attributes with
unknown types cause the import to fail, as the format relies on knowing the
type size.

@section Trade-StanfordImporter-configuration Plugin-specific config

It's possible to tune various import options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/StanfordImporter/StanfordImporter.conf config

*/
class MAGNUM_STANFORDIMPORTER_EXPORT StanfordImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit StanfordImporter();

        /** @brief Plugin manager constructor */
        explicit StanfordImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~StanfordImporter();

    private:
        MAGNUM_STANFORDIMPORTER_LOCAL ImporterFeatures doFeatures() const override;

        MAGNUM_STANFORDIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_STANFORDIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_STANFORDIMPORTER_LOCAL void doClose() override;

        MAGNUM_STANFORDIMPORTER_LOCAL UnsignedInt doMeshCount() const override;
        MAGNUM_STANFORDIMPORTER_LOCAL UnsignedInt doMeshLevelCount(UnsignedInt id) override;
        MAGNUM_STANFORDIMPORTER_LOCAL Containers::Optional<MeshData> doMesh(UnsignedInt id, UnsignedInt level) override;
        MAGNUM_STANFORDIMPORTER_LOCAL MeshAttribute doMeshAttributeForName(const std::string& name) override;
        MAGNUM_STANFORDIMPORTER_LOCAL std::string doMeshAttributeName(UnsignedShort name) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
