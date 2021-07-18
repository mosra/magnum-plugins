#ifndef Magnum_Trade_StanfordSceneConverter_h
#define Magnum_Trade_StanfordSceneConverter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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
 * @brief Class @ref Magnum::Trade::StanfordSceneConverter
 * @m_since_{plugins,2020,06}
 */

#include <Magnum/Trade/AbstractSceneConverter.h>

#include "MagnumPlugins/StanfordSceneConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_STANFORDSCENECONVERTER_BUILD_STATIC
    #ifdef StanfordSceneConverter_EXPORTS
        #define MAGNUM_STANFORDSCENECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_STANFORDSCENECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_STANFORDSCENECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_STANFORDSCENECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_STANFORDSCENECONVERTER_EXPORT
#define MAGNUM_STANFORDSCENECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief Stanford PLY converter plugin
@m_since_{plugins,2020,06}

@m_keywords{PLY}

Exports to either Little- or Big-Endian binary files with triangle faces.

@section Trade-StanfordSceneConverter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_STANFORDSCENECONVERTER` is enabled when building Magnum Plugins. To use as
a dynamic plugin, load @cpp "StanfordSceneConverter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_STANFORDSCENECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::StanfordSceneConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `StanfordSceneConverter` component of
the `MagnumPlugins` package and link to the
`MagnumPlugins::StanfordSceneConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED StanfordSceneConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::StanfordSceneConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-StanfordSceneConverter-behavior Behavior and limitations

Produces binary files, ASCII export is not implemented. The data are by default
exported in machine endian, use the @cb{.ini} endianness @ce
@ref Trade-StanfordImporter-configuration "configuration option" to perform an
endian swap on the output data.

Exports the following attributes, custom attributes and attributes not listed
below are skipped with a warning:

-   @ref MeshAttribute::Position, written as `x`/`y`/`z`. 2D positions are not
    supported.
-   @ref MeshAttribute::Normal as `nx`/`ny`/`nz`
-   @ref MeshAttribute::TextureCoordinates as `u`/`v`
-   @ref MeshAttribute::Color as `red`/`green`/`blue` and optional `alpha`, if
    the input is four-channel
-   @ref MeshAttribute::ObjectId by default as `object_id`, use the
    @cb{.ini} objectIdAttribute @ce
    @ref Trade-StanfordImporter-configuration "configuration option" to change
    the identifier that's being written.

Supported component formats --- attributes of other formats and implementation-specific formats are skipped with a warning:

-   @ref VertexFormat::Float, written as `float`
-   @ref VertexFormat::Double as `double`
-   @ref VertexFormat::UnsignedByte / @ref VertexFormat::UnsignedByteNormalized
    as `uchar`
-   @ref VertexFormat::Byte / @ref VertexFormat::ByteNormalized as `char`
-   @ref VertexFormat::UnsignedShort / @ref VertexFormat::UnsignedShortNormalized
    as `ushort`
-   @ref VertexFormat::Short / @ref VertexFormat::ShortNormalized as `short`
-   @ref VertexFormat::UnsignedInt as `uint`
-   @ref VertexFormat::Int as `int`

Index type of the input mesh is preserved, written as `uchar` / `ushort` /
`uint`. Face size is always @cpp 3 @ce, written as `uchar`. if the mesh is not
indexed, a trivial index buffer of type @ref MeshIndexType::UnsignedInt is
generated. The faces are always triangles, @ref MeshPrimitive::TriangleStrip
and @ref MeshPrimitive::TriangleFan meshes are converted to indexed
@ref MeshPrimitive::Triangles first; points, lines and other primitives are
not supported.


@section Trade-StanfordSceneConverter-configuration Plugin-specific config

It's possible to tune various import options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/StanfordSceneConverter/StanfordSceneConverter.conf config

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_STANFORDSCENECONVERTER_EXPORT StanfordSceneConverter: public AbstractSceneConverter {
    public:
        /** @brief Plugin manager constructor */
        explicit StanfordSceneConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~StanfordSceneConverter();

    private:
        MAGNUM_STANFORDSCENECONVERTER_LOCAL SceneConverterFeatures doFeatures() const override;
        MAGNUM_STANFORDSCENECONVERTER_LOCAL Containers::Array<char> doConvertToData(const MeshData& mesh) override;
};

}}

#endif
