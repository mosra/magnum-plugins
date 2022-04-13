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

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
