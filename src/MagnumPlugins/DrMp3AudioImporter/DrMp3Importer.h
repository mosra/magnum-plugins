#ifndef Magnum_Audio_DrMp3Importer_h
#define Magnum_Audio_DrMp3Importer_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019 Guillaume Jacquemin <williamjcm@users.noreply.github.com>

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
 * @brief Class @ref Magnum::Audio::DrMp3Importer
 * @m_since_{plugins,2019,10}
 */

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Magnum/Audio/AbstractImporter.h>

#include "MagnumPlugins/DrMp3AudioImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_DRMP3AUDIOIMPORTER_BUILD_STATIC
    #ifdef DrMp3AudioImporter_EXPORTS
        #define MAGNUM_DRMP3AUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_DRMP3AUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_DRMP3AUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_DRMP3AUDIOIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_DRMP3AUDIOIMPORTER_EXPORT
#define MAGNUM_DRMP3AUDIOIMPORTER_LOCAL
#endif

namespace Magnum { namespace Audio {

/**
@brief MP3 audio importer plugin using dr_mp3
@m_since_{plugins,2019,10}

@m_keywords{DrMp3AudioImporter Mp3AudioImporter}

Supports mono, stereo and surround sound files of the following formats using
the [dr_mp3](https://github.com/mackron/dr_libs) library:

-   16 bit-per-channel, imported as @ref BufferFormat::Mono16,
    @ref BufferFormat::Stereo16, @ref BufferFormat::Quad16,
    @ref BufferFormat::Surround51Channel16, @ref BufferFormat::Surround61Channel16
    or @ref BufferFormat::Surround71Channel16

This plugins provides `Mp3AudioImporter`.

@m_class{m-block m-primary}

@thirdparty This plugin makes use of the
    [dr_mp3](https://github.com/mackron/dr_libs) library by David Reid,
    released into the @m_class{m-label m-primary} **public domain**
    ([license text](https://github.com/mackron/dr_libs/blob/9177e53ba6427e16bd56e436b1dab816e6bc1b77/dr_mp3.h#L3886-L3909),
    [choosealicense.com](https://choosealicense.com/licenses/unlicense/)),
    or alternatively under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/mackron/dr_libs/blob/9177e53ba6427e16bd56e436b1dab816e6bc1b77/dr_mp3.h#L3911-L3929),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).

@section Audio-DrMp3Importer-usage Usage

This plugin depends on the @ref Audio library and is built if
`WITH_DRMP3AUDIOIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "DrMp3AudioImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_DRMP3AUDIOIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::DrMp3AudioImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `DrMp3AudioImporter` component of
the `MagnumPlugins` package and link to the `MagnumPlugins::DrMp3AudioImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED DrMp3AudioImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::DrMp3AudioImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class MAGNUM_DRMP3AUDIOIMPORTER_EXPORT DrMp3Importer: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit DrMp3Importer();

        /** @brief Plugin manager constructor */
        explicit DrMp3Importer(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_DRMP3AUDIOIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_DRMP3AUDIOIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_DRMP3AUDIOIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_DRMP3AUDIOIMPORTER_LOCAL void doClose() override;

        MAGNUM_DRMP3AUDIOIMPORTER_LOCAL BufferFormat doFormat() const override;
        MAGNUM_DRMP3AUDIOIMPORTER_LOCAL UnsignedInt doFrequency() const override;
        MAGNUM_DRMP3AUDIOIMPORTER_LOCAL Containers::Array<char> doData() override;

        Containers::Optional<Containers::Array<char>> _data;
        BufferFormat _format;
        UnsignedInt _frequency;
};

}}

#endif
