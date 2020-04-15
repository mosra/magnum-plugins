#ifndef Magnum_Trade_MagnumImporter_h
#define Magnum_Trade_MagnumImporter_h
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
 * @brief Class @ref Magnum::Trade::MagnumImporter
 * @m_since_latest_{plugins}
 */

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/MagnumImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_MAGNUMIMPORTER_BUILD_STATIC
    #ifdef MagnumImporter_EXPORTS
        #define MAGNUM_MAGNUMIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_MAGNUMIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_MAGNUMIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_MAGNUMIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_MAGNUMIMPORTER_EXPORT
#define MAGNUM_MAGNUMIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief Magnum blob importer plugin
@m_since_latest_{plugins}

Extends the builtin capabilities of @ref MeshData::deserialize() with an
ability to deserialize blobs of different bitness or endianness than current
platform. See also @ref MagnumSceneConverter, which extends the capabilities of
@ref MeshData::serialize() the same way.

@section Trade-MagnumImporter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_MAGNUMIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "MagnumImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_MAGNUMIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::MagnumImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `MagnumImporter` component of the
`MagnumPlugins` package and link to the `MagnumPlugins::MagnumImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED MagnumImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::MagnumImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class MAGNUM_MAGNUMIMPORTER_EXPORT MagnumImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit MagnumImporter();

        /** @brief Plugin manager constructor */
        explicit MagnumImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~MagnumImporter();

    private:
        MAGNUM_MAGNUMIMPORTER_LOCAL ImporterFeatures doFeatures() const override;

        MAGNUM_MAGNUMIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_MAGNUMIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_MAGNUMIMPORTER_LOCAL void doClose() override;

        MAGNUM_MAGNUMIMPORTER_LOCAL UnsignedInt doMeshCount() const override;
        MAGNUM_MAGNUMIMPORTER_LOCAL Containers::Optional<MeshData> doMesh(UnsignedInt id, UnsignedInt level) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
