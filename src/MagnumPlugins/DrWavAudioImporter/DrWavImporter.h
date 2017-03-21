#ifndef Magnum_Audio_DrWavImporter_h
#define Magnum_Audio_DrWavImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
              Vladimír Vondruš <mosra@centrum.cz>
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
#include <Magnum/Audio/AbstractImporter.h>

#include "MagnumPlugins/DrWavAudioImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_DRWAVAUDIOIMPORTER_BUILD_STATIC
    #if defined(DrWavAudioImporter_EXPORTS) || defined(DrWavAudioImporterObjects_EXPORTS)
        #define MAGNUM_DRWAVAUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_DRWAVAUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_DRWAVAUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_DRWAVAUDIOIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#endif

namespace Magnum { namespace Audio {

/**
@brief WAV audio importer plugin using dr_wav

Supports mono, stereo and surround sound files of the following formats:

-   8 bit-per-channel PCM, imported as @ref Buffer::Format::Mono8,
    @ref Buffer::Format::Stereo8, @ref Buffer::Format::Quad8,
    @ref Buffer::Format::Surround51Channel8, @ref Buffer::Format::Surround61Channel8
    or @ref Buffer::Format::Surround71Channel8
-   12 and 16 bit-per-channel PCM, imported as @ref Buffer::Format::Mono16,
    @ref Buffer::Format::Stereo16, @ref Buffer::Format::Quad16,
    @ref Buffer::Format::Surround51Channel16, @ref Buffer::Format::Surround61Channel16
    or @ref Buffer::Format::Surround71Channel16
-   24 and 32 bit-per-channel PCM and 32-bit IEEE Float, imported as
    @ref Buffer::Format::MonoFloat, @ref Buffer::Format::StereoFloat,
    @ref Buffer::Format::Quad32, @ref Buffer::Format::Surround51Channel32,
    @ref Buffer::Format::Surround61Channel32 or @ref Buffer::Format::Surround71Channel32
-   64-bit IEEE Float, imported as @ref Buffer::Format::MonoDouble,
    @ref Buffer::Format::StereoDouble, @ref Buffer::Format::Quad32,
    @ref Buffer::Format::Surround51Channel32, @ref Buffer::Format::Surround61Channel32
    or @ref Buffer::Format::Surround71Channel32
-   A-Law, imported as @ref Buffer::Format::MonoALaw / @ref Buffer::Format::StereoALaw
-   μ-Law, imported as @ref Buffer::Format::MonoMuLaw / @ref Buffer::Format::StereoMuLaw

This plugin is built if `WITH_DRWAVAUDIOIMPORTER` is enabled when building
Magnum. To use dynamic plugin, you need to load `DrWavAudioImporter` plugin
from `MAGNUM_PLUGINS_AUDIOIMPORTER_DIR`. To use static plugin or use this as a
dependency of another plugin, you need to request `DrWavAudioImporter`
component of `MagnumPlugins` package in CMake and link to
`MagnumPlugins::DrWavAudioImporter`.

This plugins provides `WavAudioImporter`, but note that this plugin doesn't
handle some errata such as JUNK blocks in strange places.

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
        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL Features doFeatures() const override;
        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL void doClose() override;

        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL Buffer::Format doFormat() const override;
        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL UnsignedInt doFrequency() const override;
        MAGNUM_DRWAVAUDIOIMPORTER_LOCAL Containers::Array<char> doData() override;

        Containers::Array<char> _data;
        Buffer::Format _format;
        UnsignedInt _frequency;
};

}}

#endif
