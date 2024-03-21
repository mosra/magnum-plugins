#ifndef Magnum_Trade_UrdfImporter_h
#define Magnum_Trade_UrdfImporter_h
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
 * @brief Class @ref Magnum::Trade::UrdfImporter
 * @m_since_latest_{plugins}
 */

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Array.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/UrdfImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_URDFIMPORTER_BUILD_STATIC
    #ifdef UrdfImporter_EXPORTS
        #define MAGNUM_URDFIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_URDFIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_URDFIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_URDFIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_URDFIMPORTER_EXPORT
#define MAGNUM_URDFIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief URDF importer plugin
@m_since_latest_{plugins}

@ref TODOTODO

@section Trade-UrdfImporter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_URDFIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "UrdfImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@ref TODOTODO pugixml dependency

@code{.cmake}
set(WITH_URDFIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::UrdfImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `UrdfImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::UrdfImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED UrdfImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::UrdfImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-UrdfImporter-behavior Behavior and limitations

@ref TODOTODO

@section Trade-UrdfImporter-configuration Plugin-specific config

It's possible to tune various import options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/UrdfImporter/UrdfImporter.conf config

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_URDFIMPORTER_EXPORT UrdfImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit UrdfImporter();

        /** @brief Plugin manager constructor */
        explicit UrdfImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~UrdfImporter();

    private:
        MAGNUM_URDFIMPORTER_LOCAL ImporterFeatures doFeatures() const override;

        MAGNUM_URDFIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_URDFIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;
        MAGNUM_URDFIMPORTER_LOCAL void doClose() override;

        MAGNUM_URDFIMPORTER_LOCAL Int doDefaultScene() const override;
        MAGNUM_URDFIMPORTER_LOCAL UnsignedInt doSceneCount() const override;
        MAGNUM_URDFIMPORTER_LOCAL Containers::String doSceneName(UnsignedInt id) override;
        MAGNUM_URDFIMPORTER_LOCAL Int doSceneForName(Containers::StringView name) override;
        MAGNUM_URDFIMPORTER_LOCAL Containers::String doSceneFieldName(UnsignedInt name) override;
        MAGNUM_URDFIMPORTER_LOCAL SceneField doSceneFieldForName(Containers::StringView name) override;
        MAGNUM_URDFIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;

        MAGNUM_URDFIMPORTER_LOCAL UnsignedLong doObjectCount() const override;
        MAGNUM_URDFIMPORTER_LOCAL Long doObjectForName(Containers::StringView name) override;
        MAGNUM_URDFIMPORTER_LOCAL Containers::String doObjectName(UnsignedLong id) override;

        struct State;
        Containers::Pointer<State> _in;
};

}}

#endif
