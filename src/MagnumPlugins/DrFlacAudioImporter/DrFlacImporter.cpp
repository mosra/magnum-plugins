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

#include "DrFlacImporter.h"

#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Utility/Assert.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Endianness.h>

#include <Magnum/Math/Packing.h>

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO /* Otherwise it includes windows.h, ugh */
#include "dr_flac.h"

namespace Magnum { namespace Audio {

namespace {

#define _v(value) BufferFormat::value
/* number of channels = 1-8, number of bytes = 1-4 */
const BufferFormat flacFormatTable[8][4] = {
    {_v(Mono8),   _v(Mono16),   _v(MonoFloat),   _v(MonoDouble)}, /* Mono */
    {_v(Stereo8), _v(Stereo16), _v(StereoFloat), _v(StereoDouble)}, /* Stereo */
    {BufferFormat{}, BufferFormat{}, BufferFormat{}, BufferFormat{}}, /* Not a thing */
    {_v(Quad8), _v(Quad16), _v(Quad32), _v(Quad32)},    /* Quad */
    {BufferFormat{}, BufferFormat{}, BufferFormat{}, BufferFormat{}}, /* Also not a thing */
    {_v(Surround51Channel8), _v(Surround51Channel16), _v(Surround51Channel32), _v(Surround51Channel32)}, /* 5.1 */
    {_v(Surround61Channel8), _v(Surround61Channel16), _v(Surround61Channel32), _v(Surround61Channel32)}, /* 6.1 */
    {_v(Surround71Channel8), _v(Surround71Channel16), _v(Surround71Channel32), _v(Surround71Channel32)}  /* 7.1 */
};
#undef _v

/* Converts 32-bit PCM into lower bit levels by skipping bytes */
Containers::Array<char> convert32PCM(const Containers::Array<char>& container, const UnsignedInt samples, const UnsignedInt size) {
    Containers::Array<char> convertData(samples*size);

    UnsignedInt skip = -1, index = 0;
    for(char item: container) {
        ++skip;

        if(skip > 3) skip = 0;
        if(skip < 4 - size) continue;

        convertData[index] = item;
        ++index;
    }

    return convertData;
}

}

DrFlacImporter::DrFlacImporter() = default;

DrFlacImporter::DrFlacImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

ImporterFeatures DrFlacImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool DrFlacImporter::doIsOpened() const { return !!_data; }

void DrFlacImporter::doOpenData(Containers::ArrayView<const char> data) {
    drflac* const handle = drflac_open_memory(data.data(), data.size());
    if(!handle) {
        Error() << "Audio::DrFlacImporter::openData(): failed to open and decode FLAC data";
        return;
    }
    Containers::ScopeGuard drflacClose{handle, drflac_close};

    const std::uint64_t samples = handle->totalSampleCount;
    const std::uint8_t numChannels = handle->channels;
    const std::uint8_t bitsPerSample = handle->bitsPerSample;

    /* FLAC supports any bitspersample from 4 to 64, but DrFlac always gives us
       32-bit samples. So we normalize bit amounts to multiples of 8, rounding
       up. */
    const UnsignedInt normalizedBytesPerSample = (bitsPerSample + 7)/8;

    if(numChannels == 0 || numChannels == 3 || numChannels == 5 || numChannels > 8 ||
       normalizedBytesPerSample == 0 || normalizedBytesPerSample > 8) {
        Error() << "Audio::DrFlacImporter::openData(): unsupported channel count"
                << numChannels << "with" << bitsPerSample
                << "bits per sample";
        return;
    }

    _frequency = handle->sampleRate;
    _format = flacFormatTable[numChannels-1][normalizedBytesPerSample-1];
    CORRADE_INTERNAL_ASSERT(_format != BufferFormat{});

    /* 32-bit integers need to be normalized to Double (with a 32 bit mantissa) */
    if(normalizedBytesPerSample == 4) {
        Containers::Array<Int> tempData(samples);
        drflac_read_s32(handle, samples, reinterpret_cast<Int*>(tempData.begin()));

        /* If the channel is mono/stereo, we can use double samples */
        if(numChannels < 3) {
            Containers::Array<Double> doubleData(samples);

            for(std::size_t i = 0; i < samples; ++i) {
                doubleData[i] = Math::unpack<Double>(tempData[i]);
            }

            const char* doubleBegin = reinterpret_cast<const char*>(doubleData.begin());
            const char* doubleEnd = reinterpret_cast<const char*>(doubleData.end());

            _data = Containers::Array<char>(samples*sizeof(Double));
            std::copy(doubleBegin, doubleEnd, _data->begin());

        /* Otherwise, convert to float */
        } else {
            Containers::Array<Float> floatData(samples);

            for(std::size_t i = 0; i < samples; ++i) {
                floatData[i] = Math::unpack<Float>(tempData[i]);
            }

            const char* floatBegin = reinterpret_cast<const char*>(floatData.begin());
            const char* floatEnd = reinterpret_cast<const char*>(floatData.end());

            _data = Containers::Array<char>(samples*sizeof(Float));
            std::copy(floatBegin, floatEnd, _data->begin());
        }

        return;
    }

    Containers::Array<char> tempData(samples*sizeof(Int));
    drflac_read_s32(handle, samples, reinterpret_cast<Int*>(tempData.begin()));

    _data = convert32PCM(tempData, samples, normalizedBytesPerSample);

    /* 8-bit needs to become unsigned */
    if(normalizedBytesPerSample == 1) {
        for(char& item: *_data) item = item - 128;

    /* 24-bit needs to become float */
    } else if(normalizedBytesPerSample == 3) {
        Containers::Array<Float> floatData(samples);

        for(std::size_t i = 0; i != samples; ++i) {
            const UnsignedInt s0 = (*_data)[i*3 + 0];
            const UnsignedInt s1 = (*_data)[i*3 + 1];
            const UnsignedInt s2 = (*_data)[i*3 + 2];

            const Int intData = Int((s0 << 8) | (s1 << 16) | (s2 << 24));
            floatData[i] = Math::unpack<Float>(intData);
        }

        const char* const floatBegin = reinterpret_cast<const char*>(floatData.begin());
        const char* const floatEnd = reinterpret_cast<const char*>(floatData.end());

        _data = Containers::Array<char>(samples*sizeof(Float));
        std::copy(floatBegin, floatEnd, _data->begin());
    }
}

void DrFlacImporter::doClose() { _data = Containers::NullOpt; }

BufferFormat DrFlacImporter::doFormat() const { return _format; }

UnsignedInt DrFlacImporter::doFrequency() const { return _frequency; }

Containers::Array<char> DrFlacImporter::doData() {
    Containers::Array<char> copy(_data->size());
    std::copy(_data->begin(), _data->end(), copy.begin());
    return copy;
}

}}

CORRADE_PLUGIN_REGISTER(DrFlacAudioImporter, Magnum::Audio::DrFlacImporter,
    "cz.mosra.magnum.Audio.AbstractImporter/0.1")
