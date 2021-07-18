#ifndef Magnum_Trade_MeshOptimizerSceneConverter_h
#define Magnum_Trade_MeshOptimizerSceneConverter_h
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
 * @brief Class @ref Magnum::Trade::MeshOptimizerSceneConverter
 * @m_since_{plugins,2020,06}
 */

#include <Magnum/Trade/AbstractSceneConverter.h>

#include "MagnumPlugins/MeshOptimizerSceneConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_MESHOPTIMIZERSCENECONVERTER_BUILD_STATIC
    #ifdef MeshOptimizerSceneConverter_EXPORTS
        #define MAGNUM_MESHOPTIMIZERSCENECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_MESHOPTIMIZERSCENECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_MESHOPTIMIZERSCENECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_MESHOPTIMIZERSCENECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_MESHOPTIMIZERSCENECONVERTER_EXPORT
#define MAGNUM_MESHOPTIMIZERSCENECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief MeshOptimizer converter plugin
@m_since_{plugins,2020,06}

Integrates various algorithms from [meshoptimizer](https://github.com/zeux/meshoptimizer).

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [meshoptimizer](https://github.com/zeux/meshoptimizer) library by Arseny
    Kapoulkine, released under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/zeux/meshoptimizer/blob/master/LICENSE.md),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).

@section Trade-MeshOptimizerSceneConverter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_MESHOPTIMIZERSCENECONVERTER` is enabled when building Magnum Plugins. To
use as a dynamic plugin, load @cpp "MeshOptimizerSceneConverter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and
[meshoptimizer](https://github.com/zeux/meshoptimizer) repositories and do the
following. If you want to use system-installed meshoptimizer, omit the first
part and point `CMAKE_PREFIX_PATH` to its installation dir if necessary.

@code{.cmake}
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # needed if building dynamic plugins
add_subdirectory(meshoptimizer EXCLUDE_FROM_ALL)

set(WITH_MESHOPTIMIZERSCENECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::MeshOptimizerSceneConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `MeshOptimizerSceneConverter` component of
the `MagnumPlugins` package and link to the
`MagnumPlugins::MeshOptimizerSceneConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED MeshOptimizerSceneConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::MeshOptimizerSceneConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

@section Trade-MeshOptimizerSceneConverter-behavior Behavior and limitations

The plugin by default performs the following optimizations, which can be
@ref Trade-MeshOptimizerSceneConverter-configuration "configured further" using
plugin-specific options:

-   [Vertex cache optimization](https://github.com/zeux/meshoptimizer#vertex-cache-optimization),
    performed when @cb{.ini} optimizeVertexCache @ce is enabled
-   [Overdraw optimization](https://github.com/zeux/meshoptimizer#overdraw-optimization),
    performed when @cb{.ini} optimizeOverdraw @ce is enabled
-   [Vertex fetch optimization](https://github.com/zeux/meshoptimizer#vertex-fetch-optimization),
    performed when @cb{.ini} optimizeVertexFetch @ce is enabled

The optimizations can be done either in-place using @ref convertInPlace(MeshData&),
in which case the input is required to be an indexed triangle mesh with mutable
index data and, in case of @cb{.ini} optimizeVertexFetch @ce, also mutable
vertex data. Alternatively, the operation can be performed using
@ref convert(const MeshData&), which accepts also triangle strips and fans,
returning always an indexed triangle mesh without requiring the input to be
mutable.

The output has the same index type as input and all attributes are preserved,
including custom attributes and attributes with implementation-specific vertex
formats, except for @cb{.ini} optimizeOverdraw @ce, which needs a position
attribute in a known type.

When @ref SceneConverterFlag::Verbose is enabled, the plugin prints the output
from meshoptimizer's [efficiency analyzers](https://github.com/zeux/meshoptimizer#efficiency-analyzers)
before and after the operation.

@subsection Trade-MeshOptimizerSceneConverter-behavior-simplification Mesh simplification

By default the plugin performs only the above non-destructive operations.
[Mesh simplification](https://github.com/zeux/meshoptimizer#simplification) can
be enabled using either the @cb{.ini} simplify @ce or @cb{.ini} simplifySloppy @ce
@ref Trade-MeshOptimizerSceneConverter-configuration "configuration option" together with specifying desired @cb{.ini} simplifyTargetIndexCountThreshold @ce
--- the default value of 1.0 will leave the mesh unchanged, set it to for
example 0.25 to reduce the mesh to a fourth of its size.

The simplification process is done in @ref convert(const MeshData&) and returns
a copy of the mesh with a subset of original vertices and a reduced index
buffer, meaning the original vertex positions are used, with no interpolation
to new locations. It only requires the mesh to have a position attribute, mesh
connectivity and face seams are figured out from the index buffer. As with all
other operations, all original attributes are preserved.

@section Trade-MeshOptimizerSceneConverter-configuration Plugin-specific config

It's possible to tune various output options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/MeshOptimizerSceneConverter/MeshOptimizerSceneConverter.conf config

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_MESHOPTIMIZERSCENECONVERTER_EXPORT MeshOptimizerSceneConverter: public AbstractSceneConverter {
    public:
        /** @brief Plugin manager constructor */
        explicit MeshOptimizerSceneConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~MeshOptimizerSceneConverter();

    private:
        MAGNUM_MESHOPTIMIZERSCENECONVERTER_LOCAL SceneConverterFeatures doFeatures() const override;

        MAGNUM_MESHOPTIMIZERSCENECONVERTER_LOCAL bool doConvertInPlace(MeshData& mesh) override;
        MAGNUM_MESHOPTIMIZERSCENECONVERTER_LOCAL Containers::Optional<MeshData> doConvert(const MeshData& mesh) override;
};

}}

#endif
