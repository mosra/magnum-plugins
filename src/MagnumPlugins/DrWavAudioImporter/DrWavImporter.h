#ifndef Magnum_Audio_DrWavImporter_h
#define Magnum_Audio_DrWavImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016
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

#include "Magnum/Audio/AbstractImporter.h"

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

Supports mono/stereo/surround files with 8/12/16/24/32 bits per channel.

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
        explicit DrWavImporter(PluginManager::AbstractManager& manager, std::string plugin);

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
