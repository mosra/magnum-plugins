#ifndef Magnum_Audio_StbVorbisImporter_h
#define Magnum_Audio_StbVorbisImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2015 Jonathan Hale <squareys@googlemail.com>
    Copyright © 2016 Alice Margatroid <loveoverwhelming@gmail.com>

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
 * @brief Class @ref Magnum::Audio::StbVorbisImporter
 */

#include <Corrade/Containers/Array.h>
#include <Magnum/Audio/AbstractImporter.h>

#include "MagnumPlugins/StbVorbisAudioImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_STBVORBISAUDIOIMPORTER_BUILD_STATIC
    #ifdef StbVorbisAudioImporter_EXPORTS
        #define MAGNUM_STBVORBISAUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_STBVORBISAUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_STBVORBISAUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_STBVORBISAUDIOIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_STBVORBISAUDIOIMPORTER_EXPORT
#define MAGNUM_STBVORBISAUDIOIMPORTER_LOCAL
#endif

namespace Magnum { namespace Audio {

/**
@brief OGG audio importer plugin using stb_vorbis

@m_keywords{StbVorbisAudioImporter VorbisAudioImporter}

Supports mono, stereo and surround sound files with 16 bits per channel using
the [stb_vorbis](https://github.com/nothings/stb) library. The files are
imported with @ref BufferFormat::Mono16, @ref BufferFormat::Stereo16,
@ref BufferFormat::Quad16, @ref BufferFormat::Surround51Channel16,
@ref BufferFormat::Surround61Channel16 and @ref BufferFormat::Surround71Channel16,
respectively.

This plugins provides `VorbisAudioImporter`, but note that this plugin doesn't
have complete support for all format quirks and the performance might be worse
than when using plugin dedicated for given format.

@m_class{m-block m-primary}

@thirdparty This plugin makes use of the
    [stb_vorbis](https://github.com/nothings/stb) library by Sean Barrett,
    released into the @m_class{m-label m-primary} **public domain**
    ([license text](https://github.com/nothings/stb/blob/e6afb9cbae4064da8c3e69af3ff5c4629579c1d2/stb_vorbis.c#L5444-L5460),
    [choosealicense.com](https://choosealicense.com/licenses/unlicense/)),
    or alternatively under @m_class{m-label m-success} **MIT**
    ([license text](https://github.com/nothings/stb/blob/master/stb_vorbis.c#L5426-L5442),
    [choosealicense.com](https://choosealicense.com/licenses/mit/)).

@section Audio-StbVorbisImporter-usage Usage

This plugin depends on the @ref Audio library and is built if
`WITH_STBVORBISAUDIOIMPORTER` is enabled when building Magnum Plugins. To use
as a dynamic plugin, load @cpp "StbVorbisAudioImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_STBVORBISAUDIOIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::StbVorbisAudioImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `StbVorbisAudioImporter` component
of the `MagnumPlugins` package and link to the
`MagnumPlugins::StbVorbisAudioImporter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED StbVorbisAudioImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::StbVorbisAudioImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class MAGNUM_STBVORBISAUDIOIMPORTER_EXPORT StbVorbisImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit StbVorbisImporter();

        /** @brief Plugin manager constructor */
        explicit StbVorbisImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_STBVORBISAUDIOIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_STBVORBISAUDIOIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_STBVORBISAUDIOIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_STBVORBISAUDIOIMPORTER_LOCAL void doClose() override;

        MAGNUM_STBVORBISAUDIOIMPORTER_LOCAL BufferFormat doFormat() const override;
        MAGNUM_STBVORBISAUDIOIMPORTER_LOCAL UnsignedInt doFrequency() const override;
        MAGNUM_STBVORBISAUDIOIMPORTER_LOCAL Containers::Array<char> doData() override;

        Containers::Array<char> _data;
        BufferFormat _format;
        UnsignedInt _frequency;
};

}}

#endif
