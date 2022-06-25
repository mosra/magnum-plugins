/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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

#include "AstcImporter.h"

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Format.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

namespace {

/* Source: https://stackoverflow.com/questions/22600678/determine-internal-format-of-given-astc-compressed-image-through-its-header
   Yes, really, this is the only source for this. Even the file utility uses
   the exact same link in its magic detection:
   https://github.com/file/file/blob/b489768b7065b6dae4bda05c737fa73ae50c50fc/magic/Magdir/images#L3263-L3276 */
struct AstcHeader {
    union {
        char magic[4];
        UnsignedInt magicNumber;
    };
    Vector3ub blockSize;
    /* 24-bit LE */
    UnsignedByte sizeX[3];
    UnsignedByte sizeY[3];
    UnsignedByte sizeZ[3];
};

static_assert(sizeof(AstcHeader) == 16, "Invalid AstcHeader size");

/* All ASTC formats are 128-bit blocks */
constexpr Int AstcBlockDataSize = 128/8;

}

struct AstcImporter::State {
    CompressedPixelFormat format;
    Vector3i size;
    /* Could be retrieved from format block size, but that's a giant switch.
       This makes the structure 8 bytes larger, which is 64x more memory than
       needed to store a single bit, but that doesn't matter here. Can't set
       Z=0 to mark 2D images, because the file can have zero size and still be
       a 3D format. */
    bool is3D;
    ImageFlags3D flags;
    /* Needed because the data might be longer */
    std::size_t dataSize;
    Containers::Array<char> data;
};

AstcImporter::AstcImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin) : AbstractImporter{manager, plugin} {}

AstcImporter::~AstcImporter() = default;

ImporterFeatures AstcImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool AstcImporter::doIsOpened() const { return !!_state; }

void AstcImporter::doClose() { _state = nullptr; }

void AstcImporter::doOpenData(Containers::Array<char>&& data, const DataFlags dataFlags) {
    /* Unlike with e.g. TgaImporter, where doOpenData() only takes over the
       data array, here we need to parse the format to decide whether it's a
       2D or a 3D image. And while at it, why not do also all other checks. */

    /* There should be at least the header */
    if(data.size() < sizeof(AstcHeader)) {
        Error{} << "Trade::AstcImporter::openData(): file header too short, expected at least" << sizeof(AstcHeader) << "bytes but got" << data.size();
        return;
    }

    /* Check magic, SCALABLE, unfortunately in LE so it's not as visible */
    AstcHeader header = *reinterpret_cast<const AstcHeader*>(data.begin());
    if(Containers::StringView{header.magic, 4} != "\x13\xAB\xA1\x5C"_s) {
        Error{} << "Trade::AstcImporter::openData(): invalid file magic 0x" << Debug::nospace << Utility::format("{:.8X}", header.magicNumber);
        return;
    }

    /* Calculate format from block size */
    CompressedPixelFormat format;
    switch(header.blockSize.x() << 16 | header.blockSize.y() << 8 | header.blockSize.z()) {
        /* LCOV_EXCL_START */
        #define _c(sizeX, sizeY) case sizeX << 16 | sizeY << 8 | 1:         \
            format = CompressedPixelFormat::Astc ## sizeX ## x ## sizeY ## RGBAUnorm; \
            break;
        _c(4, 4)
        _c(5, 4)
        _c(5, 5)
        _c(6, 5)
        _c(6, 6)
        _c(8, 5)
        _c(8, 6)
        _c(8, 8)
        _c(10, 5)
        _c(10, 6)
        _c(10, 8)
        _c(10, 10)
        _c(12, 10)
        _c(12, 12)
        #undef _c

        #define _c(sizeX, sizeY, sizeZ) case sizeX << 16 | sizeY << 8 | sizeZ:         \
            format = CompressedPixelFormat::Astc ## sizeX ## x ## sizeY ## x ## sizeZ ## RGBAUnorm; \
            break;
        _c(3, 3, 3)
        _c(4, 3, 3)
        _c(4, 4, 3)
        _c(4, 4, 4)
        _c(5, 4, 4)
        _c(5, 5, 4)
        _c(5, 5, 5)
        _c(6, 5, 5)
        _c(6, 6, 5)
        _c(6, 6, 6)
        #undef _c
        /* LCOV_EXCL_STOP */

        default:
            Error{} << "Trade::AstcImporter::openData(): invalid block size" << Debug::packed << header.blockSize;
        return;
    }

    /* Patch the format if requested. Relies on CompressedPixelFormat::Astc*
       values consistently being first Unorm, then Srgb, then F. */
    const Containers::StringView formatPatch = configuration().value<Containers::StringView>("format");
    if(formatPatch == "srgb"_s) {
        format = CompressedPixelFormat(UnsignedInt(format) + 1);
    } else if(formatPatch == "float"_s) {
        format = CompressedPixelFormat(UnsignedInt(format) + 2);
    } else if(formatPatch != "linear"_s) {
        Error{} << "Trade::AstcImporter::openData(): invalid format" << formatPatch << Debug::nospace << ", expected linear, srgb or float";
        return;
    }

    /* Image size, check if file isn't too short */
    const Vector3i size{
        header.sizeX[0] | header.sizeX[1] << 8 | header.sizeX[2] << 16,
        header.sizeY[0] | header.sizeY[1] << 8 | header.sizeY[2] << 16,
        header.sizeZ[0] | header.sizeZ[1] << 8 | header.sizeZ[2] << 16
    };
    const std::size_t dataSize = AstcBlockDataSize*((size + Vector3i{header.blockSize} - Vector3i{1})/Vector3i{header.blockSize}).product();
    if(sizeof(AstcHeader) + dataSize > data.size()) {
        Error{} << "Trade::AstcImporter::openData(): file too short, expected" << sizeof(AstcHeader) + dataSize << "bytes but got" << data.size();
        return;
    } else if(sizeof(AstcHeader) + dataSize < data.size()) {
        Warning{} << "Trade::AstcImporter::openData(): ignoring" << data.size() - sizeof(AstcHeader) - dataSize << "extra bytes at the end of file";
    }

    /* Unlike KTX or Basis, the file format doesn't contain any orientation
       metadata, so we have to rely on an externally-provided hint */
    if(!configuration().value<bool>("assumeYUpZBackward")) {
        Warning{} << "Trade::AstcImporter::openData(): image is assumed to be encoded with Y down and Z forward, imported data will have wrong orientation. Enable assumeYUpZBackward to suppress this warning.";
    }

    /* All good now, let's save everything */
    _state.emplace();
    _state->format = format;
    _state->size = size;
    /* An image is 3D if ... */
    _state->is3D =
        /* it has a 3D block, */
        header.blockSize.z() != 1 ||
        /* it has Z size larger than 1, */
        size.z() > 1 ||
        /* or it has Z size 0 and XY is non-zero, in which case the 2D image
           would imply Z = 1. If XY is zero, then it can be exposed as an empty
           2D image. */
        (!size.z() && size.xy().product());
    /* Mark the image as 2D array if it's 3D but has a 2D format */
    if(_state->is3D && header.blockSize.z() == 1)
        _state->flags |= ImageFlag3D::Array;
    _state->dataSize = dataSize;

    /* Take over the existing array or copy the data if we can't. For
       simplicity we copy it including the tiny header so we don't need to have
       different handling based on whether the data was taken over or copied
       later. */
    if(dataFlags & (DataFlag::Owned|DataFlag::ExternallyOwned)) {
        _state->data = std::move(data);
    } else {
        _state->data = Containers::Array<char>{NoInit, data.size()};
        Utility::copy(data, _state->data);
    }
}

UnsignedInt AstcImporter::doImage2DCount() const {
    return _state->is3D ? 0 : 1;
}

Containers::Optional<ImageData2D> AstcImporter::doImage2D(UnsignedInt, UnsignedInt) {
    Containers::Array<char> data{NoInit, _state->dataSize};
    Utility::copy(_state->data.slice(sizeof(AstcHeader), sizeof(AstcHeader) + _state->dataSize), data);
    return ImageData2D{_state->format, _state->size.xy(), std::move(data), ImageFlag2D(UnsignedShort(_state->flags))};
}

UnsignedInt AstcImporter::doImage3DCount() const {
    return _state->is3D ? 1 : 0;
}

Containers::Optional<ImageData3D> AstcImporter::doImage3D(UnsignedInt, UnsignedInt) {
    Containers::Array<char> data{NoInit, _state->dataSize};
    Utility::copy(_state->data.slice(sizeof(AstcHeader), sizeof(AstcHeader) + _state->dataSize), data);
    return ImageData3D{_state->format, _state->size, std::move(data), _state->flags};
}

}}

CORRADE_PLUGIN_REGISTER(AstcImporter, Magnum::Trade::AstcImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.5")
