/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
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

#include "DevIlImageImporter.h"

#include <algorithm>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

#include <IL/il.h>
#include <IL/ilu.h>

namespace Magnum { namespace Trade {

DevIlImageImporter::DevIlImageImporter() = default;

DevIlImageImporter::DevIlImageImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

DevIlImageImporter::~DevIlImageImporter() { close(); }

auto DevIlImageImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool DevIlImageImporter::doIsOpened() const { return _in; }

void DevIlImageImporter::doClose() { _in = nullptr; }

void DevIlImageImporter::doOpenData(const Containers::ArrayView<const char> data) {
    /* Because here we're copying the data and using the _in to check if file
       is opened, having them nullptr would mean openData() would fail without
       any error message. It's not possible to do this check on the importer
       side, because empty file is valid in some formats (OBJ or glTF). We also
       can't do the full import here because then doImage2D() would need to
       copy the imported data instead anyway (and the uncompressed size is much
       larger). This way it'll also work nicely with a future openMemory(). */
    if(data.empty()) {
        Error{} << "Trade::DevIlImageImporter::openData(): the file is empty";
        return;
    }

    _in = Containers::Array<unsigned char>(data.size());
    std::copy(data.begin(), data.end(), _in.begin());
}

UnsignedInt DevIlImageImporter::doImage2DCount() const { return 1; }

Containers::Optional<ImageData2D> DevIlImageImporter::doImage2D(UnsignedInt) {
    ILuint imgID = 0;
    ilGenImages(1, &imgID);
    ilBindImage(imgID);

    /* So we can use the shorter if(!ilFoo()) */
    static_assert(!IL_FALSE, "IL_FALSE doesn't have a zero value");

    if(!ilLoadL(IL_TYPE_UNKNOWN, _in, _in.size())) {
        /* iluGetString() returns empty string for 0x512, which is even more
           useless than just returning the error ID */
        Error() << "Trade::DevIlImageImporter::image2D(): cannot open the image:" << ilGetError();
        return Containers::NullOpt;
    }

    const Vector2i size{ilGetInteger(IL_IMAGE_WIDTH), ilGetInteger(IL_IMAGE_HEIGHT)};

    Int components;
    bool rgbaNeeded = false;
    PixelFormat format;
    switch(ilGetInteger(IL_IMAGE_FORMAT)) {
        /* Grayscale */
        case IL_LUMINANCE:
            format = PixelFormat::R8Unorm;
            components = 1;
            break;

        /* Grayscale + alpha */
        case IL_LUMINANCE_ALPHA:
            format = PixelFormat::RG8Unorm;
            components = 2;
            break;

        /* BGR */
        case IL_BGR:
            rgbaNeeded = true;
            format = PixelFormat::RGB8Unorm;
            components = 3;
            break;

        /* BGRA */
        case IL_BGRA:
            rgbaNeeded = true;
            format = PixelFormat::RGBA8Unorm;
            components = 4;
            break;

        /* RGB */
        case IL_RGB:
            format = PixelFormat::RGB8Unorm;
            components = 3;
            break;

        /* RGBA */
        case IL_RGBA:
            format = PixelFormat::RGBA8Unorm;
            components = 4;
            break;

        /* No idea, convert to RGBA */
        default:
            format = PixelFormat::RGBA8Unorm;
            components = 4;
            rgbaNeeded = true;
    }

    /* If the format isn't one we recognize, convert to RGB(A) */
    if(rgbaNeeded && !ilConvertImage(components == 3 ? IL_RGB : IL_RGBA, IL_UNSIGNED_BYTE)) {
        /* iluGetString() returns empty string for 0x512, which is even
           more useless than just returning the error ID */
        Error() << "Trade::DevIlImageImporter::image2D(): cannot convert image:" << ilGetError();
        return Containers::NullOpt;
    }

    /* Flip the image to match OpenGL's conventions */
    /** @todo use our own routine to avoid linking to ILU */
    ILinfo ImageInfo;
    iluGetImageInfo(&ImageInfo);
    if(ImageInfo.Origin == IL_ORIGIN_UPPER_LEFT)
        iluFlipImage();

    /* Copy the data into array that is owned by us and not by IL */
    Containers::Array<char> imageData{std::size_t(size.product()*components)};
    std::copy_n(reinterpret_cast<char*>(ilGetData()), imageData.size(), imageData.begin());

    /* Release the texture back to DevIL */
    ilDeleteImages(1, &imgID);

    /* Adjust pixel storage if row size is not four byte aligned */
    PixelStorage storage;
    if((size.x()*components)%4 != 0)
        storage.setAlignment(1);

    return Trade::ImageData2D{storage, format, size, std::move(imageData)};
}

}}

CORRADE_PLUGIN_REGISTER(DevIlImageImporter, Magnum::Trade::DevIlImageImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3")
