#ifndef Magnum_Trade_JtImporter_h
#define Magnum_Trade_JtImporter_h
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
 * @brief Class @ref Magnum::Trade::JtImporter
 * @m_since_{plugins,2020,06}
 */

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/JtImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_JTIMPORTER_BUILD_STATIC
    #ifdef JtImporter_EXPORTS
        #define MAGNUM_JTIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_JTIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_JTIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_JTIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_JTIMPORTER_EXPORT
#define MAGNUM_JTIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief JT importer plugin
@m_since_latest_{plugins}

@ref TODO description, link

@section Trade-JtImporter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    through the base @ref AbstractImporter interface. See its documentation for
    introduction and usage examples.

This plugin depends on the @ref Trade library and is built if
`MAGNUM_WITH_JTIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "JtImporter" @ce via
@relativeref{Corrade,PluginManager::Manager}.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(MAGNUM_WITH_JTIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::JtImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `JtImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::JtImporter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED JtImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::JtImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-JtImporter-behavior Behavior and limitations

@ref TODOTODO

*/
class MAGNUM_JTIMPORTER_EXPORT JtImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit JtImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~JtImporter();

    private:
        struct State;

        MAGNUM_JTIMPORTER_LOCAL ImporterFeatures doFeatures() const override;

        MAGNUM_JTIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_JTIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;
        MAGNUM_JTIMPORTER_LOCAL void doClose() override;

        MAGNUM_JTIMPORTER_LOCAL UnsignedInt doMeshCount() const override;
        MAGNUM_JTIMPORTER_LOCAL Containers::Optional<MeshData> doMesh(UnsignedInt id, UnsignedInt level) override;

        Containers::Pointer<State> _state;
};

}}

#endif
