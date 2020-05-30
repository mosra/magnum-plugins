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

void DevIlImageImporter::initialize() {
    /* You are a funny devil, DevIL. No tutorials or docs mention this function
       (except for a tiny note at http://openil.sourceforge.net/tuts/tut_step/)
       AND YET when I call ilLoadImage() without this, everything explodes. */
    ilInit();
}

DevIlImageImporter::DevIlImageImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

DevIlImageImporter::~DevIlImageImporter() { close(); }

ImporterFeatures DevIlImageImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool DevIlImageImporter::doIsOpened() const { return _image; }

void DevIlImageImporter::doClose() {
    ilDeleteImages(1, &_image);
    _image = 0;
}

/* So we can use the shorter if(!ilFoo()) */
static_assert(!IL_FALSE, "IL_FALSE doesn't have a zero value");

void DevIlImageImporter::doOpenData(const Containers::ArrayView<const char> data) {
    UnsignedInt image;
    ilGenImages(1, &image);
    ilBindImage(image);

    /* The documentation doesn't state if the data needs to stay in scope.
       Let's assume so to avoid a copy on the importer side. */
    if(!ilLoadL(IL_TYPE_UNKNOWN, data.begin(), data.size())) {
        /* iluGetString() returns empty string for 0x512, which is even more
           useless than just returning the error ID */
        Error() << "Trade::DevIlImageImporter::openData(): cannot open the image:" << reinterpret_cast<void*>(ilGetError());
        return;
    }

    /* All good, save the image */
    _image = image;
}

void DevIlImageImporter::doOpenFile(const std::string& filename) {
    UnsignedInt image;
    ilGenImages(1, &image);
    ilBindImage(image);

    if(!ilLoad(IL_TYPE_UNKNOWN, filename.data())) {
        /* iluGetString() returns empty string for 0x512, which is even more
           useless than just returning the error ID */
        Error() << "Trade::DevIlImageImporter::openFile(): cannot open the image:" << reinterpret_cast<void*>(ilGetError());
        return;
    }

    /* All good, save the image */
    _image = image;
}

UnsignedInt DevIlImageImporter::doImage2DCount() const { return 1; }

Containers::Optional<ImageData2D> DevIlImageImporter::doImage2D(UnsignedInt, UnsignedInt) {
    /* Bind the image. This was done above already, but since it's a global
       state, this avoids a mismatch in case there's more than one importer
       active at a time. */
    ilBindImage(_image);

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
        Error() << "Trade::DevIlImageImporter::image2D(): cannot convert image:" << reinterpret_cast<void*>(ilGetError());
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

    /* Adjust pixel storage if row size is not four byte aligned */
    PixelStorage storage;
    if((size.x()*components)%4 != 0)
        storage.setAlignment(1);

    return Trade::ImageData2D{storage, format, size, std::move(imageData)};
}

}}

CORRADE_PLUGIN_REGISTER(DevIlImageImporter, Magnum::Trade::DevIlImageImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.1")
