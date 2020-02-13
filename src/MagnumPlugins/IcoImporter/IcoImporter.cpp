/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
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

#include "IcoImporter.h"

#include <algorithm>

#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Endianness.h>
#include <Magnum/Trade/ImageData.h>

namespace Magnum { namespace Trade {

namespace {
/*
void deconstructIntToLittleEndian(
    UnsignedInt in,
    char *outByte0,
    char *outByte1,
    char *outByte2,
    char *outByte3
) {
    Utility::Endianness::littleEndianInPlace(in);
    *outByte0 = static_cast<char>(in);
    *outByte1 = static_cast<char>(in >> 8u);
    *outByte2 = static_cast<char>(in >> 16u);
    *outByte3 = static_cast<char>(in >> 24u);
}
*/

/*
struct BitmapInfoHeader {
    static constexpr std::size_t Size = 40;
    enum class CompressionMethod : UnsignedInt {
        BI_RGB = 0,
        BI_RLE8 = 1,
        BI_RLE4 = 2,
        BI_BITFIELDS = 3,
        BI_JPEG = 4,
        BI_PNG = 5,
        BI_ALPHABITFIELDS = 6,
        BI_CMYK = 11,
        BI_CMYKRLE8 = 12,
        BI_CMYKRLE4 = 13
    };

    UnsignedInt headerSize;
    Int width;
    Int height;
    UnsignedShort colorPlanes; // must be 1
    UnsignedShort bitsPerPixel; // Typical values are 1, 4, 8, 16, 24 and 32
    CompressionMethod compressionMethod;
    UnsignedInt imageDataSize;
    Int horzontalDensity; // pixel per metre
    Int verticalDensity; // pixel per metre
    UnsignedInt colorCount; // the number of colors in the color palette, or 0 to default to 2^n
    UnsignedInt importantColors; // the number of important colors used, or 0 when every color is important; generally ignored
};
static_assert(sizeof(BitmapInfoHeader) == BitmapInfoHeader::Size, "sizeof(BitmapInfoHeader) is incorrect");
*/

struct IconDir {
    static constexpr std::size_t Size = 6;
    UnsignedShort reserved; // must be 0
    UnsignedShort imageType; // must be either 1 for .ICO, or 2 for .CUR
    UnsignedShort imageCount;
};
static_assert(sizeof(IconDir) == IconDir::Size, "sizeof(IconDir) is incorrect");

struct IconDirEntry {
    static constexpr std::size_t Size = 16;
    UnsignedByte imageWidth; // pixels. Value 0 means image width is 256 pixels. :[
    UnsignedByte imageHeight; // pixels. Value 0 means image width is 256 pixels. :[
    UnsignedByte colorCount;
    UnsignedByte reserved; // must be 0
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
static_assert(sizeof(IconDirEntry) == IconDirEntry::Size, "sizeof(IconDirEntry) is incorrect");

}

IcoImporter::IcoImporter() = default;

IcoImporter::IcoImporter(PluginManager::AbstractManager& manager, const std::string& plugin) : AbstractImporter{manager, plugin} {}

IcoImporter::~IcoImporter() = default;

auto IcoImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool IcoImporter::doIsOpened() const { return _isOpened; }

void IcoImporter::doClose() {
    _isOpened = false;
    _imageDataArray = nullptr;
}

namespace {
    constexpr std::initializer_list<const char> bmpFileHeaderTemplate{'B', 'M', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    constexpr std::initializer_list<const char> pngMagicNum{static_cast<char>(0x89), 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
}

void IcoImporter::doOpenData(const Containers::ArrayView<const char> data) {
    _isOpened = false;
    /* Because here we're copying the data and using the _in to check if file
       is opened, having them nullptr would mean openData() would fail without
       any error message. It's not possible to do this check on the importer
       side, because empty file is valid in some formats (OBJ or glTF). We also
       can't do the full import here because then doImage2D() would need to
       copy the imported data instead anyway (and the uncompressed size is much
       larger). This way it'll also work nicely with a future openMemory(). */
    if(data.empty()) {
        Error{} << "Trade::IcoImporter::openData(): the file is empty";
        return;
    }

    if (data.size() < sizeof(IconDir)) {
        Error{} << "Trade::IcoImporter::image2D(): header too short";
        return;
    }

    IconDir header = *reinterpret_cast<const IconDir*>(data.begin());
    Utility::Endianness::littleEndianInPlace(
        header.reserved,
        header.imageType,
        header.imageCount
    );
    if (header.reserved != 0) {
        Error{} << "Trade::IcoImporter::image2D(): reserved data incorrect";
        return;
    }

    _imageDataArray = Containers::Array<std::pair<bool, Containers::Array<char>>>{header.imageCount};

    for (Int i = 0; i < header.imageCount; ++i) {
        Int iconDirEntryOffset = sizeof(IconDir) + (sizeof(IconDirEntry) * i);
        if (data.size() < (iconDirEntryOffset + sizeof(IconDirEntry))) {
            Error{} << "Trade::IcoImporter::image2D(): image entry header too short";
            return;
        }

        IconDirEntry iconDirEntry = *reinterpret_cast<const IconDirEntry*>(data.begin() + iconDirEntryOffset);
        Utility::Endianness::littleEndianInPlace(
            iconDirEntry.imageWidth,
            iconDirEntry.imageHeight,
            iconDirEntry.colorCount,
            iconDirEntry.reserved,
            iconDirEntry.colorPlanes,
            iconDirEntry.bitsPerPixel,
            iconDirEntry.imageDataSize,
            iconDirEntry.imageDataOffset
        );
        if (iconDirEntry.reserved != 0) {
            Error{} << "Trade::IcoImporter::image2D(): reserved data incorrect";
            continue;
        }

        const char* imageDataBegin = data.begin() + iconDirEntry.imageDataOffset;
        const char* imageDataEnd = imageDataBegin + iconDirEntry.imageDataSize;
        if (data.size() < (iconDirEntry.imageDataOffset + iconDirEntry.imageDataSize)) {
            Error{} << "Trade::IcoImporter::image2D(): image data out of bounds";
            continue;
        }

        bool isPng = std::equal(pngMagicNum.begin(), pngMagicNum.end(), imageDataBegin);
        auto &imageData = std::get<1>(_imageDataArray[i]);
        if (isPng) {
            imageData = Containers::Array<char>{Containers::NoInit, iconDirEntry.imageDataSize};
            std::copy(imageDataBegin, imageDataEnd, imageData.begin());
        } else {
            /* BMP data in ICO files isn't valid :( */
            /*
            UnsignedInt headerSize = *reinterpret_cast<const UnsignedInt*>(imageDataBegin);
            if (headerSize == 40) {
                imageData = Containers::Array<char>{iconDirEntry.imageDataSize + bmpFileHeaderTemplate.size()};
                std::copy(bmpFileHeaderTemplate.begin(), bmpFileHeaderTemplate.end(), imageData.begin());
                deconstructIntToLittleEndian(
                    iconDirEntry.imageDataSize + bmpFileHeaderTemplate.size(),
                    &imageData[2],
                    &imageData[3],
                    &imageData[4],
                    &imageData[5]
                );
                UnsignedInt headerSize = *reinterpret_cast<const UnsignedInt*>(imageDataBegin);
                Utility::Endianness::littleEndianInPlace(headerSize);
                deconstructIntToLittleEndian(
                    headerSize + bmpFileHeaderTemplate.size(),
                    &imageData[10],
                    &imageData[11],
                    &imageData[12],
                    &imageData[13]
                );
                std::copy(imageDataBegin, imageDataEnd, imageData.begin() + bmpFileHeaderTemplate.size());
            } else {
                imageData = Containers::Array<char>{};
            }
            */
            imageData = Containers::Array<char>{};
        }

        std::get<0>(_imageDataArray[i]) = isPng;
    }

    _isOpened = true;
}

UnsignedInt IcoImporter::doImage2DCount() const { return _imageDataArray.size(); }

Containers::Optional<ImageData2D> IcoImporter::doImage2D(UnsignedInt id) {
    bool isPng = std::get<0>(_imageDataArray[id]);
    if (isPng) {
        /* just delegate actual image importing */
        if (!_pngImporter && !(_pngImporter = manager()->loadAndInstantiate("PngImporter"))) {
            Error{} << "Trade::IcoImporter::image2D(): PngImporter is unavailable";
            return Containers::NullOpt;
        }

        if (!_pngImporter->openData(std::get<1>(_imageDataArray[id]))) {
            return Containers::NullOpt;
        }

        return _pngImporter->image2D(0);
    }

    /* sadly, BMP data in ICO files isn't valid, as it contains transparency. and so, we parse */
    return Containers::NullOpt;
}

}}

CORRADE_PLUGIN_REGISTER(IcoImporter, Magnum::Trade::IcoImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3")
