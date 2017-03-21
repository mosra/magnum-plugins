/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

#ifdef MAGNUM_TARGET_GLES2
#include <Magnum/Context.h>
#include <Magnum/Extensions.h>
#endif

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
    _in = Containers::Array<unsigned char>{data.size()};
    std::copy(data.begin(), data.end(), _in.data());
}

UnsignedInt StbImageImporter::doImage2DCount() const { return 1; }

std::optional<ImageData2D> StbImageImporter::doImage2D(UnsignedInt) {
    Vector2i size;
    Int components;

    stbi_set_flip_vertically_on_load(true);

    stbi_uc* data;
    std::size_t channelSize;
    PixelType type;
    if(stbi_is_hdr_from_memory(_in, _in.size())) {
        data = reinterpret_cast<stbi_uc*>(stbi_loadf_from_memory(_in, _in.size(), &size.x(), &size.y(), &components, 0));
        channelSize = 4;
        type = PixelType::Float;
    } else {
        data = stbi_load_from_memory(_in, _in.size(), &size.x(), &size.y(), &components, 0);
        channelSize = 1;
        type = PixelType::UnsignedByte;
    }

    if(!data) {
        Error() << "Trade::StbImageImporter::image2D(): cannot open the image:" << stbi_failure_reason();
        return std::nullopt;
    }

    PixelFormat format;
    switch(components) {
        case 1:
            #ifndef MAGNUM_TARGET_GLES2
            format = PixelFormat::Red;
            #elif !defined(MAGNUM_TARGET_WEBGL)
            format = Context::hasCurrent() && Context::current().isExtensionSupported<Extensions::GL::EXT::texture_rg>() ?
                PixelFormat::Red : PixelFormat::Luminance;
            #else
            format = PixelFormat::Luminance;
            #endif
            break;
        case 2:
            #ifndef MAGNUM_TARGET_GLES2
            format = PixelFormat::RG;
            #elif !defined(MAGNUM_TARGET_WEBGL)
            format = Context::hasCurrent() && Context::current().isExtensionSupported<Extensions::GL::EXT::texture_rg>() ?
                PixelFormat::RG : PixelFormat::LuminanceAlpha;
            #else
            format = PixelFormat::LuminanceAlpha;
            #endif
            break;

        case 3: format = PixelFormat::RGB; break;
        case 4: format = PixelFormat::RGBA; break;
        default: CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
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

    return Trade::ImageData2D{storage, format, type, size, std::move(imageData)};
}

}}
