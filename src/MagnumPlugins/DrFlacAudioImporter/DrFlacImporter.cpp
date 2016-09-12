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

DrFlacImporter::DrFlacImporter() = default;

DrFlacImporter::DrFlacImporter(PluginManager::AbstractManager& manager, std::string plugin): AbstractImporter(manager, std::move(plugin)) {}

auto DrFlacImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool DrFlacImporter::doIsOpened() const { return _data; }

void DrFlacImporter::doOpenData(Containers::ArrayView<const char> data) {

    drflac* _handle = drflac_open_memory(reinterpret_cast<const UnsignedByte*>(data.data()), data.size());

    if(_handle == NULL) {
        Error() << "Audio::DrFlacImporter::openData(): failed to open and decode FLAC data";
        return;
    }

    uint64_t samples = _handle->totalSampleCount;
    uint32_t frequency = _handle->sampleRate;
    uint8_t numChannels = _handle->channels;
    uint8_t bitsPerSample = _handle->bitsPerSample;

    _frequency = frequency;

    if(numChannels == 1 && bitsPerSample == 8)
        _format = Buffer::Format::Mono8;
    else if(numChannels == 1 && bitsPerSample == 16)
        _format = Buffer::Format::Mono16;
    else if(numChannels == 2 && bitsPerSample == 8)
        _format = Buffer::Format::Stereo8;
    else if(numChannels == 2 && bitsPerSample == 16)
        _format = Buffer::Format::Stereo16;
    else {
        Error() << "Audio::DrFlacImporter::openData(): unsupported channel count"
                << numChannels << "with" << bitsPerSample
                << "bits per sample";

        drflac_close(_handle);
        return;
    }

    Containers::Array<char> tempData(samples * sizeof(int32_t));

    drflac_read_s32(_handle, samples, (int32_t*)tempData.begin());
    drflac_close(_handle);

    _data = std::move(tempData);
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
