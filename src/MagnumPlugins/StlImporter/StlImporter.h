#ifndef Magnum_Trade_StlImporter_h
#define Magnum_Trade_StlImporter_h
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
 * @brief Class @ref Magnum::Trade::StlImporter
 * @m_since_{plugins,2020,06}
 */

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Array.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/StlImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_STLIMPORTER_BUILD_STATIC
    #ifdef StlImporter_EXPORTS
        #define MAGNUM_STLIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_STLIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_STLIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_STLIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_STLIMPORTER_EXPORT
#define MAGNUM_STLIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief STL importer plugin
@m_since_{plugins,2020,06}

Imports normal and vertex information from binary
[Stereolitography STL](https://en.wikipedia.org/wiki/STL_(file_format)) files.

@section Trade-StlImporter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_STLIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "StlImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_STLIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::StlImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `StlImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::StlImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED StlImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::StlImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-StlImporter-behavior Behavior and limitations

The file is always imported as a non-indexed triangle mesh with per-face
normals (i.e., same normal for all vertices in the triangle). Both positions
and normals are imported as @ref VertexFormat::Vector3. Using the
@cb{.ini} perFaceToPerVertex @ce @ref Trade-StanfordImporter-configuration "configuration option"
it's possible to import per-face normals separately without duplicating them
for each vertex --- useful for example when you want to deduplicate the
positions and generate smooth normals from these.

Similarly to @ref StanfordImporter, ASCII files are not supported, only binary.
The [non-standard extensions for vertex colors](https://en.wikipedia.org/wiki/STL_(file_format)#Color_in_binary_STL)
are also not supported due to a lack of generally available files for testing.

@section Trade-StlImporter-configuration Plugin-specific config

It's possible to tune various import options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/StlImporter/StlImporter.conf config

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_STLIMPORTER_EXPORT StlImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit StlImporter();

        /** @brief Plugin manager constructor */
        explicit StlImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~StlImporter();

    private:
        MAGNUM_STLIMPORTER_LOCAL ImporterFeatures doFeatures() const override;

        MAGNUM_STLIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_STLIMPORTER_LOCAL void doOpenFile(const std::string& filename) override;
        MAGNUM_STLIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_STLIMPORTER_LOCAL void openDataInternal(Containers::Array<char>&& data);
        MAGNUM_STLIMPORTER_LOCAL void doClose() override;

        MAGNUM_STLIMPORTER_LOCAL UnsignedInt doMeshCount() const override;
        MAGNUM_STLIMPORTER_LOCAL UnsignedInt doMeshLevelCount(UnsignedInt id) override;
        MAGNUM_STLIMPORTER_LOCAL Containers::Optional<MeshData> doMesh(UnsignedInt id, UnsignedInt level) override;

        Containers::Optional<Containers::Array<char>> _in;
};

}}

#endif
