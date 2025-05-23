/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
              Vladimír Vondruš <mosra@centrum.cz>
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

#include "StbVorbisImporter.h"

#include <cstdlib> /* std::free() */
#include <Corrade/Utility/Assert.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Endianness.h>

/* GCC 12 and 13 in Release warns about some clearly bogus "maybe
   uninitialized" variables inside stb_vorbis. I DON'T CARE, THE FILE IS PULLED
   IN WITH -isystem, YOU ARE NOT SUPPOSED TO COMPLAIN FOR THOSE */
#if defined(CORRADE_TARGET_GCC) && !defined(CORRADE_TARGET_CLANG) && __GNUC__ >= 12
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#define STB_VORBIS_NO_STDIO 1
#include "stb_vorbis.c"
#if defined(CORRADE_TARGET_GCC) && !defined(CORRADE_TARGET_CLANG) && __GNUC__ >= 12
#pragma GCC diagnostic pop
#endif

namespace Magnum { namespace Audio {

#ifdef MAGNUM_BUILD_DEPRECATED
StbVorbisImporter::StbVorbisImporter() = default; /* LCOV_EXCL_LINE */
#endif

StbVorbisImporter::StbVorbisImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {}

ImporterFeatures StbVorbisImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool StbVorbisImporter::doIsOpened() const { return _data; }

void StbVorbisImporter::doOpenData(Containers::ArrayView<const char> data) {
    Int numChannels, frequency;
    Short* decodedData = nullptr;

    Int samples = stb_vorbis_decode_memory(reinterpret_cast<const UnsignedByte*>(data.data()), data.size(), &numChannels, &frequency, &decodedData);

    if(samples == -1) {
        Error() << "Audio::StbVorbisImporter::openData(): the file signature is invalid";
        return;
    } else if (samples == -2) {
        /* memory allocation failure */
        Error() << "Audio::StbVorbisImporter::openData(): out of memory";
        return;
    }

    Containers::Array<char> tempData{reinterpret_cast<char*>(decodedData), size_t(samples*numChannels*2),
        [](char* data, size_t) { std::free(data); }};
    _frequency = frequency;

    /** @todo Floating-point formats */
    if(numChannels == 1)
        _format = BufferFormat::Mono16;
    else if(numChannels == 2)
        _format = BufferFormat::Stereo16;
    else if(numChannels == 4)
        _format = BufferFormat::Quad16;
    else if(numChannels == 6)
        _format = BufferFormat::Surround51Channel16;
    else if(numChannels == 7)
        _format = BufferFormat::Surround61Channel16;
    else if(numChannels == 8)
        _format = BufferFormat::Surround71Channel16;
    else {
        Error() << "Audio::StbVorbisImporter::openData(): unsupported channel count"
                << numChannels << "with" << 16 << "bits per sample";
        return;
    }

    _data = Utility::move(tempData);
}

void StbVorbisImporter::doClose() { _data = nullptr; }

BufferFormat StbVorbisImporter::doFormat() const { return _format; }

UnsignedInt StbVorbisImporter::doFrequency() const { return _frequency; }

Containers::Array<char> StbVorbisImporter::doData() {
    return Containers::Array<char>{InPlaceInit, _data};
}

}}

CORRADE_PLUGIN_REGISTER(StbVorbisAudioImporter, Magnum::Audio::StbVorbisImporter,
    MAGNUM_AUDIO_ABSTRACTIMPORTER_PLUGIN_INTERFACE)
