#ifndef Magnum_Audio_FfmpegImporter_h
#define Magnum_Audio_FfmpegImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
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
 * @brief Class @ref Magnum::Audio::FfmpegImporter
 */

#include <Corrade/Containers/Array.h>
#include <Magnum/Audio/AbstractImporter.h>

#include "MagnumPlugins/FfmpegAudioImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_FFMPEGAUDIOIMPORTER_BUILD_STATIC
    #ifdef FfmpegAudioImporter_EXPORTS
        #define MAGNUM_FFMPEGAUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_FFMPEGAUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_FFMPEGAUDIOIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_FFMPEGAUDIOIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_FFMPEGAUDIOIMPORTER_EXPORT
#define MAGNUM_FFMPEGAUDIOIMPORTER_LOCAL
#endif

namespace Magnum { namespace Audio {

/**
@brief Audio importer using FFmpeg

@todoc formats, link

This plugin depends on the @ref Audio library and is built if
`WITH_FFMPEGAUDIOIMPORTER` is enabled when building Magnum Plugins. To use
as a dynamic plugin, you need to load the @cpp "FfmpegAudioImporter" @ce
plugin from `MAGNUM_PLUGINS_AUDIOIMPORTER_DIR`. To use as a static plugin or as
a dependency of another plugin with CMake, you need to request the
`FfmpegAudioImporter` component of the `MagnumPlugins` package and link to
the `MagnumPlugins::FfmpegAudioImporter` target.

@todoc provides

@m_class{m-block m-warning}

@thirdparty This plugin makes use of the [FFmpeg](http://assimp.org) library,
    licensed under @m_class{m-label m-warning} **LGPLv2.1**
    ([license text](https://www.ffmpeg.org/legal.html),
    [choosealicense.com](https://choosealicense.com/licenses/lgpl-2.1/)). It
    requires attribution and either dynamic linking or source disclosure for
    public use.

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.
*/
class MAGNUM_FFMPEGAUDIOIMPORTER_EXPORT FfmpegImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit FfmpegImporter();

        /** @brief Plugin manager constructor */
        explicit FfmpegImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_FFMPEGAUDIOIMPORTER_LOCAL Features doFeatures() const override;
        MAGNUM_FFMPEGAUDIOIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_FFMPEGAUDIOIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_FFMPEGAUDIOIMPORTER_LOCAL void doClose() override;

        MAGNUM_FFMPEGAUDIOIMPORTER_LOCAL BufferFormat doFormat() const override;
        MAGNUM_FFMPEGAUDIOIMPORTER_LOCAL UnsignedInt doFrequency() const override;
        MAGNUM_FFMPEGAUDIOIMPORTER_LOCAL Containers::Array<char> doData() override;

        Containers::Array<char> _data;
        BufferFormat _format;
        UnsignedInt _frequency;
};

}}

#endif
