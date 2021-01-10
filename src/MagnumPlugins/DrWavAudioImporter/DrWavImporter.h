#ifndef Magnum_Audio_DrWavImporter_h
#define Magnum_Audio_DrWavImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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
 * @brief Class @ref Magnum::Audio::DrWavImporter
 */

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Magnum/Audio/AbstractImporter.h>

#include "MagnumPlugins/DrWavAudioImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_DRWAVAUDIOIMPORTER_BUILD_STATIC
    #ifdef DrWavAudioImporter_EXPORTS
        #define MAGNUM_DRWAVAUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_DRWAVAUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_DRWAVAUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_DRWAVAUDIOIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_DRWAVAUDIOIMPORTER_EXPORT
#define MAGNUM_DRWAVAUDIOIMPORTER_LOCAL
#endif

namespace Magnum { namespace Audio {

/**
@brief WAV audio importer plugin using dr_wav

@m_keywords{DrWavAudioImporter WavAudioImporter}

Supports mono, stereo and surround sound files of the following formats using
the [dr_wav](https://github.com/mackron/dr_libs) library:

-   8 bit-per-channel PCM, imported as @ref BufferFormat::Mono8,
    @ref BufferFormat::Stereo8, @ref BufferFormat::Quad8,
    @ref BufferFormat::Surround51Channel8, @ref BufferFormat::Surround61Channel8
    or @ref BufferFormat::Surround71Channel8
-   12 and 16 bit-per-channel PCM, imported as @ref BufferFormat::Mono16,
    @ref BufferFormat::Stereo16, @ref BufferFormat::Quad16,
    @ref BufferFormat::Surround51Channel16, @ref BufferFormat::Surround61Channel16
    or @ref BufferFormat::Surround71Channel16
-   24 and 32 bit-per-channel PCM and 32-bit IEEE Float, imported as
    @ref BufferFormat::MonoFloat, @ref BufferFormat::StereoFloat,
    @ref BufferFormat::Quad32, @ref BufferFormat::Surround51Channel32,
    @ref BufferFormat::Surround61Channel32 or @ref BufferFormat::Surround71Channel32
-   64-bit IEEE Float, imported as @ref BufferFormat::MonoDouble,
    @ref BufferFormat::StereoDouble, @ref BufferFormat::Quad32,
    @ref BufferFormat::Surround51Channel32, @ref BufferFormat::Surround61Channel32
    or @ref BufferFormat::Surround71Channel32
-   A-Law, imported as @ref BufferFormat::MonoALaw / @ref BufferFormat::StereoALaw
-   μ-Law, imported as @ref BufferFormat::MonoMuLaw / @ref BufferFormat::StereoMuLaw

This plugins provides `WavAudioImporter`, but note that this plugin doesn't
handle some errata such as JUNK blocks in strange places.

@m_class{m-block m-primary}

@thirdparty This plugin makes use of the [dr_wav](https://mackron.github.io/dr_wav)
    library by David Reid, released into the
    @m_class{m-label m-primary} **public domain**
    ([license text](https://github.com/mackron/dr_libs/blob/b5e569af74dab39183bc1d5f8fce53296efcfeee/dr_wav.h#L3702-L3727),
    [choosealicense.com](https://choosealicense.com/licenses/unlicense/)).

@section Audio-DrWavImporter-usage Usage

This plugin depends on the @ref Audio library and is built if
`WITH_DRWAVAUDIOIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "DrWavAudioImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_DRWAVAUDIOIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::DrWavAudioImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `DrWavAudioImporter` component of
the `MagnumPlugins` package and link to the `MagnumPlugins::DrWavAudioImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED DrWavAudioImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::DrWavAudioImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class MAGNUM_DRWAVAUDIOIMPORTER_EXPORT DrWavImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit DrWavImporter();

        /** @brief Plugin manager constructor */
        explicit DrWavImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL void doClose() override;

        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL BufferFormat doFormat() const override;
        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL UnsignedInt doFrequency() const override;
        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL Containers::Array<char> doData() override;

        Containers::Optional<Containers::Array<char>> _data;
        BufferFormat _format;
        UnsignedInt _frequency;
};

}}

#endif
