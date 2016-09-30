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

#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

namespace Magnum { namespace Audio {

#define bvf(value) Buffer::Format::value
namespace {

    // number of channels = 8
    // number of bits = 5
    const Buffer::Format  flacFormatTable[9][5] = {
        { bvf(Mono8),   bvf(Mono8),   bvf(Mono16),   bvf(MonoFloat),   bvf(MonoDouble)   },   // None
        { bvf(Mono8),   bvf(Mono8),   bvf(Mono16),   bvf(MonoFloat),   bvf(MonoDouble)   },   // Mono
        { bvf(Stereo8), bvf(Stereo8), bvf(Stereo16), bvf(StereoFloat), bvf(StereoDouble) },   // Stereo
        { bvf(Quad8), bvf(Quad8), bvf(Quad16), bvf(Quad32), bvf(Quad32) },                    // Not a thing
        { bvf(Quad8), bvf(Quad8), bvf(Quad16), bvf(Quad32), bvf(Quad32) },                    // Quad
        { bvf(Quad8), bvf(Quad8), bvf(Quad16), bvf(Quad32), bvf(Quad32) },                    // Also not a thing
        { bvf(Surround51Channel8), bvf(Surround51Channel8), bvf(Surround51Channel16), bvf(Surround51Channel32), bvf(Surround51Channel32) }, // 5.1
        { bvf(Surround61Channel8), bvf(Surround61Channel8), bvf(Surround61Channel16), bvf(Surround61Channel32), bvf(Surround61Channel32) }, // 6.1
        { bvf(Surround71Channel8), bvf(Surround71Channel8), bvf(Surround71Channel16), bvf(Surround71Channel32), bvf(Surround71Channel32) }  // 7.1
    };

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
}
#undef bvf

DrFlacImporter::DrFlacImporter() = default;

DrFlacImporter::DrFlacImporter(PluginManager::AbstractManager& manager, std::string plugin): AbstractImporter(manager, std::move(plugin)) {}

auto DrFlacImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool DrFlacImporter::doIsOpened() const { return _data; }

void DrFlacImporter::doOpenData(Containers::ArrayView<const char> data) {
    drflac* handle = drflac_open_memory(data.data(), data.size());
    if(!handle) {
        Error() << "Audio::DrFlacImporter::openData(): failed to open and decode FLAC data";
        return;
    }

    uint64_t samples = handle->totalSampleCount;
    uint8_t numChannels = handle->channels;
    uint8_t bitsPerSample = handle->bitsPerSample;

    // Normalize bit amounts to multiples of 8, rounding up
    UnsignedInt normalizedBitsPerSample = (bitsPerSample / 8) + ((bitsPerSample % 8) ? 1 : 0);

    if(numChannels == 0 || numChannels == 3 || numChannels == 5 || numChannels > 8 ||
       normalizedBitsPerSample == 0 || normalizedBitsPerSample > 2) {
        Error() << "Audio::DrFlacImporter::openData(): unsupported channel count"
                << numChannels << "with" << bitsPerSample
                << "bits per sample";

        drflac_close(handle);
        return;
    }

    _frequency = handle->sampleRate;
    _format = flacFormatTable[numChannels][normalizedBitsPerSample];

    Containers::Array<char> tempData(samples*sizeof(Int));
    drflac_read_s32(handle, samples, reinterpret_cast<Int*>(tempData.begin()));
    drflac_close(handle);

    switch(normalizedBitsPerSample)
    {
        case 1: {
            _data = convert32PCM(tempData, samples, sizeof(UnsignedByte));

            // Convert to unsigned
            for(char& item : _data) {
                item = item - 128;
            }
            break;
        }
        case 2: {
            _data = convert32PCM(tempData, samples, sizeof(Short));
            break;
        }
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
