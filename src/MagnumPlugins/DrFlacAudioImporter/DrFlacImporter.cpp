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

#include "DrFlacImporter.h"

#include <Corrade/Utility/Assert.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Endianness.h>

#include <Magnum/Math/Functions.h>

#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

namespace Magnum { namespace Audio {

#define bfv(value) Buffer::Format::value
namespace {

    /* number of channels = 8, number of bits = 5 */
    const Buffer::Format  flacFormatTable[8][5] = {
        { bfv(Mono8),   bfv(Mono16),   bfv(MonoFloat),   bfv(MonoDouble)   },       // Mono
        { bfv(Stereo8), bfv(Stereo16), bfv(StereoFloat), bfv(StereoDouble) },       // Stereo
        { Buffer::Format{}, Buffer::Format{}, Buffer::Format{}, Buffer::Format{} }, // Not a thing
        { bfv(Quad8), bfv(Quad16), bfv(Quad32), bfv(Quad32) },                      // Quad
        { Buffer::Format{}, Buffer::Format{}, Buffer::Format{}, Buffer::Format{} }, // Also not a thing
        { bfv(Surround51Channel8), bfv(Surround51Channel16), bfv(Surround51Channel32), bfv(Surround51Channel32) }, // 5.1
        { bfv(Surround61Channel8), bfv(Surround61Channel16), bfv(Surround61Channel32), bfv(Surround61Channel32) }, // 6.1
        { bfv(Surround71Channel8), bfv(Surround71Channel16), bfv(Surround71Channel32), bfv(Surround71Channel32) }  // 7.1
    };

    /* Converts 32-bit PCM into lower bit levels by skipping bytes */
    Containers::Array<char> convert32PCM(const Containers::Array<char>& container, UnsignedInt samples, UnsignedInt size) {
        Containers::Array<char> convertData(samples*size);

        UnsignedInt skip = -1, index = 0;
        for(char item : container) {
            ++skip;

            if(skip > 3) skip = 0;
            if(skip < 4 - size) continue;

            convertData[index] = item;
            ++index;
        }

        return convertData;
    }

    struct DrFlacDeleter {
        void operator()(drflac* handle) { drflac_close(handle); }
    };
}
#undef bfv

DrFlacImporter::DrFlacImporter() = default;

DrFlacImporter::DrFlacImporter(PluginManager::AbstractManager& manager, std::string plugin): AbstractImporter(manager, std::move(plugin)) {}

auto DrFlacImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool DrFlacImporter::doIsOpened() const { return _data; }

void DrFlacImporter::doOpenData(Containers::ArrayView<const char> data) {

    std::unique_ptr<drflac, DrFlacDeleter> handle(drflac_open_memory(data.data(), data.size()));
    if(!handle) {
        Error() << "Audio::DrFlacImporter::openData(): failed to open and decode FLAC data";
        return;
    }

    uint64_t samples = handle->totalSampleCount;
    uint8_t numChannels = handle->channels;
    uint8_t bitsPerSample = handle->bitsPerSample;

    /* FLAC supports any bitspersample from 4 to 64, but DrFlac always gives us 32-bit samples
       So we normalize bit amounts to multiples of 8, rounding up */
    UnsignedInt normalizedBytesPerSample = (bitsPerSample + 7)/8;

    if(numChannels == 0 || numChannels == 3 || numChannels == 5 || numChannels > 8 ||
       normalizedBytesPerSample == 0 || normalizedBytesPerSample > 8) {
        Error() << "Audio::DrFlacImporter::openData(): unsupported channel count"
                << numChannels << "with" << bitsPerSample
                << "bits per sample";
        return;
    }

    /* Can't load something with no samples */
    if(samples == 0) {
        Error() << "Audio::DrFlacImporter::openData(): no samples";
        return;
    }

    _frequency = handle->sampleRate;
    _format = flacFormatTable[numChannels-1][normalizedBytesPerSample-1];
    CORRADE_INTERNAL_ASSERT(_format != Buffer::Format{});

    /* 32-bit integers need to be normalized to Double (with a 32 bit mantissa) */
    if(normalizedBytesPerSample == 4) {
        Containers::Array<Int> tempData(samples);
        drflac_read_s32(handle.get(), samples, reinterpret_cast<Int*>(tempData.begin()));

        /* If the channel is mono/stereo, we can use double samples */
        if(numChannels < 3) {
            Containers::Array<Double> doubleData(samples);

            for (UnsignedInt i = 0; i < samples; ++i) {
                doubleData[i] = Math::normalize<Double>(tempData[i]);
            }

            const char* doubleBegin = reinterpret_cast<const char*>(doubleData.begin());
            const char* doubleEnd = reinterpret_cast<const char*>(doubleData.end());

            _data = Containers::Array<char>(samples*sizeof(Double));
            std::copy(doubleBegin, doubleEnd, _data.begin());

        /* Otherwise, convert to float */
        } else {
            Containers::Array<Float> floatData(samples);

            for (UnsignedInt i = 0; i < samples; ++i) {
                floatData[i] = Math::normalize<Float>(tempData[i]);
            }

            const char* floatBegin = reinterpret_cast<const char*>(floatData.begin());
            const char* floatEnd = reinterpret_cast<const char*>(floatData.end());

            _data = Containers::Array<char>(samples*sizeof(Float));
            std::copy(floatBegin, floatEnd, _data.begin());
        }

        return;
    }

    Containers::Array<char> tempData(samples*sizeof(Int));
    drflac_read_s32(handle.get(), samples, reinterpret_cast<Int*>(tempData.begin()));

    _data = convert32PCM(tempData, samples, normalizedBytesPerSample);

    /* 8-bit needs to become unsigned */
    if(normalizedBytesPerSample == 1) {
        // Convert to unsigned
        for(char& item : _data) {
            item = item - 128;
        }
    /* 24-bit needs to become float */
    } else if(normalizedBytesPerSample == 3) {
        Containers::Array<Float> floatData(samples);

        for (UnsignedInt i = 0; i < samples; ++i) {

            UnsignedInt s0 = _data[i*3 + 0];
            UnsignedInt s1 = _data[i*3 + 1];
            UnsignedInt s2 = _data[i*3 + 2];

            Int intData = Int((s0 << 8) | (s1 << 16) | (s2 << 24));
            floatData[i] = Math::normalize<Float>(intData);
        }

        const char* floatBegin = reinterpret_cast<const char*>(floatData.begin());
        const char* floatEnd = reinterpret_cast<const char*>(floatData.end());

        _data = Containers::Array<char>(samples*sizeof(Float));
        std::copy(floatBegin, floatEnd, _data.begin());
    }

    return;
}

void DrFlacImporter::doClose() { _data = nullptr; }

Buffer::Format DrFlacImporter::doFormat() const { return _format; }

UnsignedInt DrFlacImporter::doFrequency() const { return _frequency; }

Containers::Array<char> DrFlacImporter::doData() {
    Containers::Array<char> copy(_data.size());
    std::copy(_data.begin(), _data.end(), copy.begin());
    return copy;
}

}}
