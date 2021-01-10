/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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

#include "Faad2Importer.h"

#include <cstring>
#include <algorithm>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Utility/Assert.h>
#include <Corrade/Utility/Debug.h>

#include <neaacdec.h>

namespace Magnum { namespace Audio {

Faad2Importer::Faad2Importer() = default;

Faad2Importer::Faad2Importer(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

ImporterFeatures Faad2Importer::doFeatures() const { return ImporterFeature::OpenData; }

bool Faad2Importer::doIsOpened() const { return !_samples.empty(); }

void Faad2Importer::doOpenData(Containers::ArrayView<const char> data) {
    /* Init the library */
    const NeAACDecHandle decoder = NeAACDecOpen();
    Containers::ScopeGuard exit{decoder, NeAACDecClose};

    /* Decide what's the sample format. For some reason this doesn't depend on
       the file. */
    /** @todo how do I detect what format is actually stored?! */
    CORRADE_INTERNAL_ASSERT(NeAACDecGetCurrentConfiguration(decoder)->outputFormat = FAAD_FMT_16BIT);

    /* Open the file. I expected anything but a need for a const_cast. Ugh. */
    /* For raw AAC files it returns always 0, not skipping any header:
       https://github.com/knik0/faad2/blob/7da4a83b230d069a9d731b1e64f6e6b52802576a/libfaad/decoder.c#L327-L339 */
    unsigned long samplerate = 0;
    unsigned char channels = 0;
    long result = NeAACDecInit(decoder, const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data.data())), data.size(), &samplerate, &channels);
    if(result < 0) {
        Error{} << "Audio::Faad2Importer::openData(): can't read file header";
        return;
    }

    _frequency = samplerate;

    if(channels == 2)
        _format = BufferFormat::Stereo16;
    else {
        /* Mono files are always upgraded to stereo for some reason, so I
           always assume stereo anyway:
           https://github.com/knik0/faad2/blob/7da4a83b230d069a9d731b1e64f6e6b52802576a/libfaad/decoder.c#L353-L358 */
        Error{} << "Audio::Faad2Importer::openData(): unsupported channel count"
                << channels << "with" << 16 << "bits per sample";
        return;
    }

    /** @todo do it not via a vector -- is there any way to get the sample
        count beforehand? the faad fronted does it by manually parsing the
        headers and NO WAY IN HELL i am doing that here. So be it.
        https://github.com/knik0/faad2/blob/7da4a83b230d069a9d731b1e64f6e6b52802576a/frontend/main.c#L613-L630 */
    std::size_t pos = result;
    std::vector<UnsignedShort> samples;
    while(pos < data.size()) {
        NeAACDecFrameInfo info;
        void* sampleBuffer = NeAACDecDecode(decoder, &info, const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data.data())) + pos, data.size() - pos);
        if(info.error) {
            Error{} << "Audio::Faad2Importer::openData(): decoding error";
            return;
        }

        std::size_t end = samples.size();
        samples.resize(samples.size() + info.samples);
        std::copy_n(reinterpret_cast<UnsignedShort*>(sampleBuffer), info.samples, &samples[0] + end);
        pos += info.bytesconsumed;
    }

    _samples = std::move(samples);
}

void Faad2Importer::doClose() { _samples.clear(); }

BufferFormat Faad2Importer::doFormat() const { return _format; }

UnsignedInt Faad2Importer::doFrequency() const { return _frequency; }

Containers::Array<char> Faad2Importer::doData() {
    Containers::Array<char> copy(_samples.size()*2);
    std::memcpy(copy.begin(), _samples.data(), copy.size());
    return copy;
}

}}

CORRADE_PLUGIN_REGISTER(Faad2AudioImporter, Magnum::Audio::Faad2Importer,
    "cz.mosra.magnum.Audio.AbstractImporter/0.1")
