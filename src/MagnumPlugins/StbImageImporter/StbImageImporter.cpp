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

#include "StbImageImporter.h"

#include <algorithm>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ASSERT CORRADE_INTERNAL_ASSERT
/* Not defining malloc/free, because there's no equivalent for realloc in C++ */
#include "stb_image.h"

namespace Magnum { namespace Trade {

StbImageImporter::StbImageImporter() = default;

StbImageImporter::StbImageImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

StbImageImporter::~StbImageImporter() = default;

auto StbImageImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool StbImageImporter::doIsOpened() const { return _in; }

void StbImageImporter::doClose() {
    _in = nullptr;
}

void StbImageImporter::doOpenData(const Containers::ArrayView<const char> data) {
    /* Because here we're copying the data and using the _in to check if file
       is opened, having them nullptr would mean openData() would fail without
       any error message. It's not possible to do this check on the importer
       side, because empty file is valid in some formats (OBJ or glTF). We also
       can't do the full import here because then doImage2D() would need to
       copy the imported data instead anyway (and the uncompressed size is much
       larger). This way it'll also work nicely with a future openMemory(). */
    if(data.empty()) {
        Error{} << "Trade::StbImageImporter::openData(): the file is empty";
        return;
    }

    _in = Containers::Array<unsigned char>{data.size()};
    std::copy(data.begin(), data.end(), _in.data());
}

UnsignedInt StbImageImporter::doImage2DCount() const { return 1; }

Containers::Optional<ImageData2D> StbImageImporter::doImage2D(UnsignedInt) {
    Vector2i size;
    Int components;

    stbi_set_flip_vertically_on_load(true);
    /* The docs say this is enabled by default, but it's *not*. Ugh. */
    stbi_convert_iphone_png_to_rgb(true);

    stbi_uc* data;
    std::size_t channelSize;
    PixelFormat format;
    if(stbi_is_hdr_from_memory(_in, _in.size())) {
        data = reinterpret_cast<stbi_uc*>(stbi_loadf_from_memory(_in, _in.size(), &size.x(), &size.y(), &components, 0));
        channelSize = 4;
        if(data) switch(components) {
            case 1: format = PixelFormat::R32F;         break;
            case 2: format = PixelFormat::RG32F;        break;
            case 3: format = PixelFormat::RGB32F;       break;
            case 4: format = PixelFormat::RGBA32F;      break;
            default: CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }
    } else {
        data = stbi_load_from_memory(_in, _in.size(), &size.x(), &size.y(), &components, 0);
        channelSize = 1;
        if(data) switch(components) {
            case 1: format = PixelFormat::R8Unorm;      break;
            case 2: format = PixelFormat::RG8Unorm;     break;
            case 3: format = PixelFormat::RGB8Unorm;    break;
            case 4: format = PixelFormat::RGBA8Unorm;   break;
            default: CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }
    }

    if(!data) {
        Error() << "Trade::StbImageImporter::image2D(): cannot open the image:" << stbi_failure_reason();
        return Containers::NullOpt;
    }

    /* Copy the data into array with default deleter and free the original (we
       can't use custom deleter to avoid dangling function pointer call when
       the plugin is unloaded sooner than the array is deleted) */
    Containers::Array<char> imageData{std::size_t(size.product()*components*channelSize)};
    std::copy_n(reinterpret_cast<char*>(data), imageData.size(), imageData.begin());
    stbi_image_free(data);

    /* Adjust pixel storage if row size is not four byte aligned */
    PixelStorage storage;
    if((size.x()*components*channelSize)%4 != 0)
        storage.setAlignment(1);

    return Trade::ImageData2D{storage, format, size, std::move(imageData)};
}

}}

CORRADE_PLUGIN_REGISTER(StbImageImporter, Magnum::Trade::StbImageImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3")
