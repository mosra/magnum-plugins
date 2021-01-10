#ifndef Magnum_Trade_IcoImporter_h
#define Magnum_Trade_IcoImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2020 bowling-allie <allie.smith.epic@gmail.com>

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
 * @brief Class @ref Magnum::Trade::IcoImporter
 * @m_since_{plugins,2020,06}
 */

#include <Corrade/Containers/Pointer.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/IcoImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_ICOIMPORTER_BUILD_STATIC
    #ifdef IcoImporter_EXPORTS
        #define MAGNUM_ICOIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_ICOIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_ICOIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_ICOIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_ICOIMPORTER_EXPORT
#define MAGNUM_ICOIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief ICO importer plugin
@m_since_{plugins,2020,06}

Loads Windows icon/cursor (`*.ico` / `*.cur`) files with embedded PNGs.

@section Trade-IcoImporter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_ICOIMPORTER` is enabled when building Magnum Plugins. To use as a dynamic
plugin, load @cpp "IcoImporter" @ce via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_ICOIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::IcoImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `DdsImporter` component of the
`MagnumPlugins` package in CMake and link to the `MagnumPlugins::IcoImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED IcoImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::IcoImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-IcoImporter-behavior Behavior and limitations

The importer will report count of all icon sizes in @ref image2DLevelCount()
and you can then import each using the second parameter of @ref image2D().
Currently, only embedded PNGs are supported --- for them it will delegate
loading to any plugin that provides `PngImporter`; for images that are BMPs,
@ref image2D() will fail. You can use @ref DevIlImageImporter in that case
instead, but please @ref Trade-DevIlImageImporter-behavior-ico "be aware of its limitations".
*/
class MAGNUM_ICOIMPORTER_EXPORT IcoImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit IcoImporter();

        /** @brief Plugin manager constructor */
        explicit IcoImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~IcoImporter();

    private:
        MAGNUM_ICOIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_ICOIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_ICOIMPORTER_LOCAL void doClose() override;
        MAGNUM_ICOIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_ICOIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_ICOIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_ICOIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
