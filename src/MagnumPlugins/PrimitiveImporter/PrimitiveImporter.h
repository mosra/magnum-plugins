#ifndef Magnum_Trade_PrimitiveImporter_h
#define Magnum_Trade_PrimitiveImporter_h
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
 * @brief Class @ref Magnum::Trade::PrimitiveImporter
 * @m_since_{plugins,2020,06}
 */

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/PrimitiveImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_PRIMITIVEIMPORTER_BUILD_STATIC
    #ifdef PrimitiveImporter_EXPORTS
        #define MAGNUM_PRIMITIVEIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_PRIMITIVEIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_PRIMITIVEIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_PRIMITIVEIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_PRIMITIVEIMPORTER_EXPORT
#define MAGNUM_PRIMITIVEIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief Primitive importer plugin
@m_since_{plugins,2020,06}

Exposes primitives from the @ref Primitives library through importer APIs.
Applications that have an importer pipeline already set up can use this plugin
to access builtin primitives for prototyping and testing purposes without
needing to write explicit code. For applications that don't have an importer
pipeline, using the @ref Primitives library directly is more straightforward.

@section Trade-PrimitiveImporter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_PRIMITIVEIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "PrimitiveImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_PRIMITIVEIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::PrimitiveImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `PrimitiveImporter` component the
`MagnumPlugins` package and link to the `MagnumPlugins::PrimitiveImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED PrimitiveImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::PrimitiveImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

@section Trade-PrimitiveImporter-behavior Behavior

Upon calling @ref openData() with arbitrary data (or @ref openFile() with an
arbitrary *existing* file), the importer will expose all primitives through
@ref mesh() and additionally list one scene with all primitives as direct 2D or
3D child objects for easy import to existing scenes. Both objects and meshes
can be accessed through name of the respective function in the @ref Primitives
namespace (so e.g. loading a `uvSphereSolid` mesh will give you
@ref Primitives::uvSphereSolid()).

@section Trade-PrimitiveImporter-configuration Plugin-specific config

By default the primitives are created with the same options that were used to
create screenshots in the @ref Primitives names. Those options can be then
customized through various import options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/PrimitiveImporter/PrimitiveImporter.conf config

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_PRIMITIVEIMPORTER_EXPORT PrimitiveImporter: public AbstractImporter {
    public:
        /* Default constructor not provided as I would need to replicate the
           whole configuration in it (ugh) */

        /** @brief Plugin manager constructor */
        explicit PrimitiveImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~PrimitiveImporter();

    private:
        MAGNUM_PRIMITIVEIMPORTER_LOCAL ImporterFeatures doFeatures() const override;

        MAGNUM_PRIMITIVEIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_PRIMITIVEIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_PRIMITIVEIMPORTER_LOCAL void doClose() override;

        MAGNUM_PRIMITIVEIMPORTER_LOCAL Int doDefaultScene() const override;

        MAGNUM_PRIMITIVEIMPORTER_LOCAL UnsignedInt doSceneCount() const override;
        MAGNUM_PRIMITIVEIMPORTER_LOCAL Containers::Optional<SceneData> doScene(UnsignedInt id) override;

        MAGNUM_PRIMITIVEIMPORTER_LOCAL UnsignedInt doObject2DCount() const override;
        MAGNUM_PRIMITIVEIMPORTER_LOCAL Int doObject2DForName(const std::string& name) override;
        MAGNUM_PRIMITIVEIMPORTER_LOCAL std::string doObject2DName(UnsignedInt id) override;
        MAGNUM_PRIMITIVEIMPORTER_LOCAL Containers::Pointer<ObjectData2D> doObject2D(UnsignedInt id) override;

        MAGNUM_PRIMITIVEIMPORTER_LOCAL UnsignedInt doObject3DCount() const override;
        MAGNUM_PRIMITIVEIMPORTER_LOCAL Int doObject3DForName(const std::string& name) override;
        MAGNUM_PRIMITIVEIMPORTER_LOCAL std::string doObject3DName(UnsignedInt id) override;
        MAGNUM_PRIMITIVEIMPORTER_LOCAL Containers::Pointer<ObjectData3D> doObject3D(UnsignedInt id) override;

        MAGNUM_PRIMITIVEIMPORTER_LOCAL UnsignedInt doMeshCount() const override;
        MAGNUM_PRIMITIVEIMPORTER_LOCAL Int doMeshForName(const std::string& name) override;
        MAGNUM_PRIMITIVEIMPORTER_LOCAL std::string doMeshName(UnsignedInt id) override;
        MAGNUM_PRIMITIVEIMPORTER_LOCAL Containers::Optional<MeshData> doMesh(UnsignedInt id, UnsignedInt level) override;

        bool _opened = false;
};

}}

#endif
