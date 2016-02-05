#ifndef Magnum_Audio_StbVorbisImporter_h
#define Magnum_Audio_StbVorbisImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2015 Jonathan Hale <squareys@googlemail.com>

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

#include "Magnum/Audio/AbstractImporter.h"

namespace Magnum { namespace Audio {

/**
@brief OGG audio importer plugin using stb_vorbis

Supports mono and stereo files with 16 bits per channel. The files are
imported with @ref Buffer::Format::Mono16 or @ref Buffer::Format::Stereo16,
respectively.

This plugin is built if `WITH_STBVORBISAUDIOIMPORTER` is enabled when building
Magnum. To use dynamic plugin, you need to load `StbVorbisAudioImporter` plugin
from `MAGNUM_PLUGINS_AUDIOIMPORTER_DIR`. To use static plugin or use this as a
dependency of another plugin, you need to request `StbVorbisAudioImporter`
component of `MagnumPlugins` package in CMake and link to
`${MAGNUMPLUGINS_STBVORBISAUDIOIMPORTER_LIBRARIES}`.

This plugins provides `VorbisAudioImporter`, but note that this plugin doesn't
have complete support for all format quirks and the performance might be worse
than when using plugin dedicated for given format.

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class StbVorbisImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit StbVorbisImporter();

        /** @brief Plugin manager constructor */
        explicit StbVorbisImporter(PluginManager::AbstractManager& manager, std::string plugin);

    private:
        Features doFeatures() const override;
        bool doIsOpened() const override;
        void doOpenData(Containers::ArrayView<const char> data) override;
        void doClose() override;

        Buffer::Format doFormat() const override;
        UnsignedInt doFrequency() const override;
        Containers::Array<char> doData() override;

        Containers::Array<char> _data;
        Buffer::Format _format;
        UnsignedInt _frequency;
};

}}

#endif
