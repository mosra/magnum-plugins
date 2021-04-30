/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2020 bowling-allie <allie.smith.epic@gmail.com>

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

#include "IcoImporter.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Endianness.h>
#include <Magnum/Trade/ImageData.h>

namespace Magnum { namespace Trade {

namespace {

struct IconDir {
    UnsignedShort reserved; /* must be 0 */
    UnsignedShort imageType; /* Either 1 for .ICO, or 2 for .CUR */
    UnsignedShort imageCount;
};

struct IconDirEntry {
    /* With and height in pixels. Value 0 means 256 pixels. */
    UnsignedByte imageWidth;
    UnsignedByte imageHeight;
    UnsignedByte colorCount;
    UnsignedByte reserved; /* must be 0 */
    union {
        UnsignedShort colorPlanes;
        UnsignedShort hotspotX;
    };
    union {
        UnsignedShort bitsPerPixel;
        UnsignedShort hotspotY;
    };
    UnsignedInt imageDataSize;
    UnsignedInt imageDataOffset;
};

}

struct IcoImporter::State {
    Containers::Pointer<Trade::AbstractImporter> pngImporter;
    Containers::Array<char> data;
    Containers::Array<Containers::ArrayView<const char>> levels;
};

IcoImporter::IcoImporter() = default;

IcoImporter::IcoImporter(PluginManager::AbstractManager& manager, const std::string& plugin) : AbstractImporter{manager, plugin} {}

IcoImporter::~IcoImporter() = default;

ImporterFeatures IcoImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool IcoImporter::doIsOpened() const { return !!_state; }

void IcoImporter::doClose() { _state = nullptr; }

namespace {
    constexpr const char PngHeader[] {
        '\x89', 'P', 'N', 'G', '\x0d', '\x0a', '\x1a', '\x0a'
    };
}

void IcoImporter::doOpenData(const Containers::ArrayView<const char> data) {
    if(data.size() < sizeof(IconDir)) {
        Error{} << "Trade::IcoImporter::openData(): file header too short, expected at least" << sizeof(IconDir) << "bytes but got" << data.size();
        return;
    }

    IconDir header = *reinterpret_cast<const IconDir*>(data.begin());
    Utility::Endianness::littleEndianInPlace(header.imageType, header.imageCount);

    Containers::Pointer<State> state{InPlaceInit};
    state->data = Containers::Array<char>{NoInit, data.size()};
    state->levels = Containers::Array<Containers::ArrayView<const char>>{header.imageCount};
    Utility::copy(data, state->data);

    for(UnsignedInt i = 0; i != header.imageCount; ++i) {
        std::size_t iconDirEntryOffset = sizeof(IconDir) + sizeof(IconDirEntry)*i;
        if(state->data.size() < iconDirEntryOffset + sizeof(IconDirEntry)) {
            Error{} << "Trade::IcoImporter::openData(): image header too short, expected at least" << iconDirEntryOffset + sizeof(IconDirEntry) << "bytes but got" << state->data.size();
            return;
        }

        IconDirEntry iconDirEntry = *reinterpret_cast<const IconDirEntry*>(state->data.begin() + iconDirEntryOffset);
        /* The other fields need endian swapping as well, but we don't use them
           so it's not necessary */
        Utility::Endianness::littleEndianInPlace(
            iconDirEntry.imageDataSize,
            iconDirEntry.imageDataOffset
        );

        if(state->data.size() < iconDirEntry.imageDataOffset + iconDirEntry.imageDataSize) {
            Error{} << "Trade::IcoImporter::openData(): image too short, expected at least" << iconDirEntry.imageDataOffset + iconDirEntry.imageDataSize << "bytes but got" << state->data.size();
            return;
        }

        state->levels[i] = state->data.slice(iconDirEntry.imageDataOffset, iconDirEntry.imageDataOffset + iconDirEntry.imageDataSize);
    }

    /* All good, save the state */
    _state = std::move(state);
}

UnsignedInt IcoImporter::doImage2DCount() const { return 1; }

UnsignedInt IcoImporter::doImage2DLevelCount(UnsignedInt) { return _state->levels.size(); }

Containers::Optional<ImageData2D> IcoImporter::doImage2D(UnsignedInt, UnsignedInt level) {
    if(_state->levels[level].size() < sizeof(PngHeader) || std::memcmp(_state->levels[level].data(), PngHeader, sizeof(PngHeader)) != 0) {
        Error{} << "Trade::IcoImporter::image2D(): only files with embedded PNGs are supported";
        return Containers::NullOpt;
    }

    /* just delegate actual image importing */
    if(!_state->pngImporter && !(_state->pngImporter = manager()->loadAndInstantiate("PngImporter"))) {
        Error{} << "Trade::IcoImporter::image2D(): PngImporter is not available";
        return Containers::NullOpt;
    }

    /* Note: this is uncovered by the tests because neither StbImageImporter
       nor PngImporter / DevIlImageImporter do any checks apart that could be
       triggered here. In the best case openData() checks PNG header, but that
       we do above already, so it can't be hit again here. */
    if(!_state->pngImporter->openData(_state->levels[level]))
        return Containers::NullOpt;

    return _state->pngImporter->image2D(0);
}

}}

CORRADE_PLUGIN_REGISTER(IcoImporter, Magnum::Trade::IcoImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.3")
