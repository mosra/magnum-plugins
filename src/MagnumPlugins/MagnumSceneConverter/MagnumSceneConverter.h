#ifndef Magnum_Trade_MagnumSceneConverter_h
#define Magnum_Trade_MagnumSceneConverter_h
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
 * @brief Class @ref Magnum::Trade::MagnumSceneConverter
 * @m_since_latest_{plugins}
 */

#include <Magnum/Trade/AbstractSceneConverter.h>

#include "MagnumPlugins/MagnumSceneConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_MAGNUMSCENECONVERTER_BUILD_STATIC
    #ifdef MagnumSceneConverter_EXPORTS
        #define MAGNUM_MAGNUMSCENECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_MAGNUMSCENECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_MAGNUMSCENECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_MAGNUMSCENECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_MAGNUMSCENECONVERTER_EXPORT
#define MAGNUM_MAGNUMSCENECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief Magnum blob converter plugin
@m_since_latest_{plugins}

@m_keywords{MagnumLittle32SceneConverter MagnumLittle64SceneConverter}
@m_keywords{MagnumBig32SceneConverter MagnumBig64SceneConverter}

Extends the builtin capabilities of @ref MeshData::serialize() with an
ability to serialize into blobs of different bitness or endianness than current
platform. See also @ref MagnumImporter, which extends the capabilities of
@ref MeshData::deserialize() the same way. The output blob is in one of the
following formats:

-   @ref DataChunkSignature::Little32 if the plugin was loaded as
    `MagnumLittle32SceneConverter`
-   @ref DataChunkSignature::Little64 if the plugin was loaded as
    `MagnumLittle64SceneConverter`
-   @ref DataChunkSignature::Big32 if the plugin was loaded as
    `MagnumBig32SceneConverter`
-   @ref DataChunkSignature::Big64 if the plugin was loaded as
    `MagnumBig64SceneConverter`

If the plugin is loaded as `MagnumSceneConverter`, the output format matches
current platform (@ref DataChunkSignature::Current), and is equivalent to
calling @ref MeshData::serialize().

Provides the `MagnumLittle32SceneConverter`, `MagnumLittle64SceneConverter`,
`MagnumBig32SceneConverter` and `MagnumBig64SceneConverter` plugins.

@section Trade-MagnumSceneConverter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_MAGNUMSCENECONVERTER` is enabled when building Magnum Plugins. To use as
a dynamic plugin, load @cpp "MagnumSceneConverter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_MAGNUMSCENECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::MagnumSceneConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `MagnumSceneConverter` component of
the `MagnumPlugins` package and link to the
`MagnumPlugins::MagnumSceneConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED MagnumSceneConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::MagnumSceneConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class MAGNUM_MAGNUMSCENECONVERTER_EXPORT MagnumSceneConverter: public AbstractSceneConverter {
    public:
        /**
         * @brief Plugin manager constructor
         *
         * Outputs files in format based on which alias was used to load the
         * plugin.
         */
        explicit MagnumSceneConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~MagnumSceneConverter();

    private:
        MAGNUM_MAGNUMSCENECONVERTER_LOCAL SceneConverterFeatures doFeatures() const override;

        MAGNUM_MAGNUMSCENECONVERTER_LOCAL Containers::Array<char> doConvertToData(const MeshData& mesh) override;

        DataChunkSignature _signature;
};

}}

#endif
