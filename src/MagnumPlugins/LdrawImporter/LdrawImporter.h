#ifndef Magnum_Trade_LdrawImporter_h
#define Magnum_Trade_LdrawImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
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
 * @brief Class @ref Magnum::Trade::LdrawImporter
 * @m_since_latest_{plugins}
 */

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/LdrawImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_LDRAWIMPORTER_BUILD_STATIC
    #ifdef LdrawImporter_EXPORTS
        #define MAGNUM_LDRAWIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_LDRAWIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_LDRAWIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_LDRAWIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_LDRAWIMPORTER_EXPORT
#define MAGNUM_LDRAWIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief LDraw importer plugin
@m_since_latest_{plugins}

@ref TODOTODO

@section Trade-LdrawImporter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    through the base @ref AbstractImporter interface. See its documentation for
    introduction and usage examples.

This plugin depends on the @ref Trade library and is built if
`MAGNUM_WITH_LDRAWIMPORTER` is enabled when building Magnum Plugins. To use as
a dynamic plugin, load @cpp "LdrawImporter" @ce via
@relativeref{Corrade,PluginManager::Manager}.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(MAGNUM_WITH_LDRAWIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::LdrawImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `LdrawImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::LdrawImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED LdrawImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::LdrawImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-LdrawImporter-behavior Behavior and limitations

@ref TODOTODO
*/
class MAGNUM_LDRAWIMPORTER_EXPORT LdrawImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit LdrawImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~LdrawImporter();

    private:
        MAGNUM_LDRAWIMPORTER_LOCAL ImporterFeatures doFeatures() const override;

        MAGNUM_LDRAWIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_LDRAWIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;
        MAGNUM_LDRAWIMPORTER_LOCAL void doClose() override;

        MAGNUM_LDRAWIMPORTER_LOCAL UnsignedInt doMeshCount() const override;
        MAGNUM_LDRAWIMPORTER_LOCAL Containers::Optional<MeshData> doMesh(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_LDRAWIMPORTER_LOCAL UnsignedInt doMaterialCount() const override;
        MAGNUM_LDRAWIMPORTER_LOCAL Containers::Optional<MaterialData> doMaterial(UnsignedInt id) override;

        MAGNUM_LDRAWIMPORTER_LOCAL UnsignedLong doObjectCount() const override;
        MAGNUM_LDRAWIMPORTER_LOCAL UnsignedInt doSceneCount() const override;
        MAGNUM_LDRAWIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;
        MAGNUM_LDRAWIMPORTER_LOCAL Int doDefaultScene() const override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
