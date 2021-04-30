/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019 Guillaume Jacquemin <williamjcm@users.noreply.github.com>

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

#include "DrMp3Importer.h"

#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Assert.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Endianness.h>

#include <Magnum/Math/Packing.h>

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

namespace Magnum { namespace Audio {

namespace {

#define _v(value) BufferFormat::value
/* number of channels = 1-8, number of bytes = 1-4 */
const BufferFormat mp3FormatTable[8][4] = {
    {BufferFormat{}, _v(Mono16),   BufferFormat{}, BufferFormat{}}, /* Mono */
    {BufferFormat{}, _v(Stereo16), BufferFormat{}, BufferFormat{}}, /* Stereo */
    {BufferFormat{}, BufferFormat{}, BufferFormat{}, BufferFormat{}}, /* Not a thing */
    {BufferFormat{}, _v(Quad16), BufferFormat{}, BufferFormat{}},    /* Quad */
    {BufferFormat{}, BufferFormat{}, BufferFormat{}, BufferFormat{}}, /* Also not a thing */
    {BufferFormat{}, _v(Surround51Channel16), BufferFormat{}, BufferFormat{}}, /* 5.1 */
    {BufferFormat{}, _v(Surround61Channel16), BufferFormat{}, BufferFormat{}}, /* 6.1 */
    {BufferFormat{}, _v(Surround71Channel16), BufferFormat{}, BufferFormat{}}  /* 7.1 */
};
#undef _v

}

DrMp3Importer::DrMp3Importer() = default;

DrMp3Importer::DrMp3Importer(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

ImporterFeatures DrMp3Importer::doFeatures() const { return ImporterFeature::OpenData; }

bool DrMp3Importer::doIsOpened() const { return !!_data; }

void DrMp3Importer::doOpenData(Containers::ArrayView<const char> data) {
    drmp3_config config;
    config.outputChannels = config.outputSampleRate = 0;
    drmp3_uint64 frameCount;

    drmp3_int16* const decodedPointer = drmp3_open_memory_and_read_s16(data.data(), data.size(), &config, &frameCount);
    if(!decodedPointer) {
        Error() << "Audio::DrMp3Importer::openData(): failed to open and decode MP3 data";
        return;
    }

    Containers::Array<char> decodedData{reinterpret_cast<char*>(decodedPointer), std::size_t(frameCount*sizeof(Short)), [](char* data, std::size_t) {
        drmp3_free(data);
    }};

    const std::uint32_t numChannels = config.outputChannels;

    if(numChannels == 0 || numChannels == 3 || numChannels == 5 || numChannels > 8) {
        Error() << "Audio::DrMp3Importer::openData(): unsupported channel count"
                << numChannels;
        return;
    }

    _frequency = config.outputSampleRate;
    _format = mp3FormatTable[numChannels - 1][1];
    CORRADE_INTERNAL_ASSERT(_format != BufferFormat{});

    /* All good, save the data */
    _data = std::move(decodedData);
}

void DrMp3Importer::doClose() { _data = Containers::NullOpt; }

BufferFormat DrMp3Importer::doFormat() const { return _format; }

UnsignedInt DrMp3Importer::doFrequency() const { return _frequency; }

Containers::Array<char> DrMp3Importer::doData() {
    Containers::Array<char> copy{NoInit, _data->size()};
    Utility::copy(*_data, copy);
    return copy;
}

}}

CORRADE_PLUGIN_REGISTER(DrMp3AudioImporter, Magnum::Audio::DrMp3Importer,
    "cz.mosra.magnum.Audio.AbstractImporter/0.1")
