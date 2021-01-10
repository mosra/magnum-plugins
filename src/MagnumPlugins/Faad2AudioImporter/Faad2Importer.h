#ifndef Magnum_Audio_Faad2Importer_h
#define Magnum_Audio_Faad2Importer_h
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
 * @brief Class @ref Magnum::Audio::Faad2Importer
 */

#include <vector>
#include <Magnum/Audio/AbstractImporter.h>

#include "MagnumPlugins/Faad2AudioImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_FAAD2AUDIOIMPORTER_BUILD_STATIC
    #ifdef Faad2AudioImporter_EXPORTS
        #define MAGNUM_FAAD2AUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_FAAD2AUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_FAAD2AUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_FAAD2AUDIOIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_FAAD2AUDIOIMPORTER_EXPORT
#define MAGNUM_FAAD2AUDIOIMPORTER_LOCAL
#endif

namespace Magnum { namespace Audio {

/**
@brief AAC audio importer plugin using FAAD2

@m_keywords{Faad2AudioImporter AacAudioImporter}

Supports mono and stereo AAC files with 16 bits per channel using the
[FAAD2](https://www.audiocoding.com) library. The files are always imported
with @ref BufferFormat::Stereo16.

This plugins provides `AacAudioImporter`.

@m_class{m-block m-danger}

@thirdparty This plugin makes use of the [FAAD2](https://www.audiocoding.com/faad2.html)
    library, licensed under @m_class{m-label m-danger} **GPLv2**
    ([license text](http://www.opensource.org/licenses/gpl-license.php),
    [choosealicense.com](https://choosealicense.com/licenses/gpl-2.0/)). It
    requires attribution and source disclosure for public use. It's subject to
    patent licensing for commercial use.

@section Audio-Faad2Importer-usage Usage

This plugin depends on the @ref Audio library and is built if
`WITH_FAAD2AUDIOIMPORTER` is enabled when building Magnum Plugins. To use
as a dynamic plugin, load @cpp "Faad2AudioImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following. Using FAAD2 itself as a CMake subproject isn't tested at the moment,
so you need to provide it as a system dependency and point `CMAKE_PREFIX_PATH`
to its installation dir if necessary.

@code{.cmake}
set(WITH_FAAD2AUDIOIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::Faad2AudioImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindFAAD2.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindFAAD2.cmake)
into your `modules/` directory, request the `Faad2AudioImporter` component of
the `MagnumPlugins` package and link to the `MagnumPlugins::Faad2AudioImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED Faad2AudioImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::Faad2AudioImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class MAGNUM_FAAD2AUDIOIMPORTER_EXPORT Faad2Importer: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit Faad2Importer();

        /** @brief Plugin manager constructor */
        explicit Faad2Importer(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_FAAD2AUDIOIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_FAAD2AUDIOIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_FAAD2AUDIOIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_FAAD2AUDIOIMPORTER_LOCAL void doClose() override;

        MAGNUM_FAAD2AUDIOIMPORTER_LOCAL BufferFormat doFormat() const override;
        MAGNUM_FAAD2AUDIOIMPORTER_LOCAL UnsignedInt doFrequency() const override;
        MAGNUM_FAAD2AUDIOIMPORTER_LOCAL Containers::Array<char> doData() override;

        std::vector<UnsignedShort> _samples;
        BufferFormat _format;
        UnsignedInt _frequency;
};

}}

#endif
