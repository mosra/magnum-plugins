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

#include "StbImageImporter.h"

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ASSERT CORRADE_INTERNAL_ASSERT

/* Use our own thread-local, and only if CORRADE_BUILD_MULTITHREADED is set.
   Verified in StbImageImporterTest::multithreaded(). */
#define STBI_NO_THREAD_LOCALS
#ifdef CORRADE_BUILD_MULTITHREADED
#define STBI_THREAD_LOCAL CORRADE_THREAD_LOCAL
#endif

/* Not defining malloc/free, because there's no equivalent for realloc in C++ */
#include "stb_image.h"

namespace Magnum { namespace Trade {

struct StbImageImporter::State {
    Containers::Array<char> data;

    /* Gif size, frame stride and delays */
    Vector3i gifSize;
    std::size_t gifFrameStride;
    Containers::Array<int> gifDelays;
};

StbImageImporter::StbImageImporter() {
    /** @todo horrible workaround, fix this properly */
    configuration().setValue("forceChannelCount", 0);
}

StbImageImporter::StbImageImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

StbImageImporter::~StbImageImporter() = default;

ImporterFeatures StbImageImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool StbImageImporter::doIsOpened() const { return !!_in; }

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

    /* NOTE: the StbImageImporterTest::multithreaded() test depends on these
       two being located here. If that changes, the test needs to be adapted to
       check those elsewhere. */
    stbi_set_flip_vertically_on_load_thread(true);
    /* The docs say this is enabled by default, but it's *not*. Ugh. */
    /** @todo do BGR -> RGB processing here instead, this may get obsolete:
        https://github.com/nothings/stb/pull/950 */
    stbi_convert_iphone_png_to_rgb_thread(true);

    /* Try to open as a gif. If that succeeds, great. If that fails, the actual
       opening (and error handling) is done in doImage2D(). */
    {
        int* delays;
        Vector3i size;
        int components;
        stbi_uc* gifData = stbi_load_gif_from_memory(reinterpret_cast<const stbi_uc*>(data.begin()), data.size(), &delays, &size.x(), &size.y(), &size.z(), &components, 0);
        if(gifData) {
            _in.emplace();

            /* Acquire ownership of the data */
            _in->gifDelays = Containers::Array<int>{delays, std::size_t(size.z()),
                [](int* data, std::size_t) { stbi_image_free(data); }};
            _in->data = Containers::Array<char>{reinterpret_cast<char*>(gifData),
                std::size_t(size.product()*components),
                [](char* data, std::size_t) { stbi_image_free(data); }};

            /* Save size, decide on frame stride. stb_image says that for GIF
               the result is always four-channel, so take a shortcut and report
               the images as PixelFormat::RGBA8Unorm always -- that also means
               we don't need to handle alignment explicitly. */
            _in->gifSize = size;
            CORRADE_INTERNAL_ASSERT(components == 4);
            CORRADE_INTERNAL_ASSERT((size.x()*components)%4 == 0);
            _in->gifFrameStride = size.xy().product()*components;
            return;
        }
    }

    _in.emplace();
    _in->data = Containers::Array<char>{data.size()};
    Utility::copy(data, _in->data);
}

const void* StbImageImporter::doImporterState() const {
    return _in->gifDelays.data();
}

UnsignedInt StbImageImporter::doImage2DCount() const {
    return _in->gifSize.isZero() ? 1 : _in->gifSize.z();
}

Containers::Optional<ImageData2D> StbImageImporter::doImage2D(const UnsignedInt id, UnsignedInt) {
    /* This is a GIF that was loaded already during data opening. Return Nth
       image */
    if(!_in->gifSize.isZero()) {
        Containers::Array<char> imageData{_in->gifFrameStride};
        Utility::copy(Containers::arrayCast<char>(
            _in->data.slice(id*_in->gifFrameStride, (id + 1)*_in->gifFrameStride)),
            imageData);
        return Trade::ImageData2D{PixelFormat::RGBA8Unorm, _in->gifSize.xy(), std::move(imageData)};
    }

    Vector2i size;
    Int components;

    const Int forceChannelCount = configuration().value<Int>("forceChannelCount");

    stbi_uc* data;
    std::size_t channelSize;
    PixelFormat format;
    if(stbi_is_hdr_from_memory(reinterpret_cast<const stbi_uc*>(_in->data.data()), _in->data.size())) {
        data = reinterpret_cast<stbi_uc*>(stbi_loadf_from_memory(reinterpret_cast<const stbi_uc*>(_in->data.data()), _in->data.size(), &size.x(), &size.y(), &components, forceChannelCount));
        channelSize = 4;
        /* stb_image still returns the original component count in components,
           which we don't want/need */
        if(forceChannelCount) components = forceChannelCount;
        if(data) switch(components) {
            case 1: format = PixelFormat::R32F;         break;
            case 2: format = PixelFormat::RG32F;        break;
            case 3: format = PixelFormat::RGB32F;       break;
            case 4: format = PixelFormat::RGBA32F;      break;
            default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }
    } else if(stbi_is_16_bit_from_memory(reinterpret_cast<const stbi_uc*>(_in->data.data()), _in->data.size())) {
        data = reinterpret_cast<stbi_uc*>(stbi_load_16_from_memory(reinterpret_cast<const stbi_uc*>(_in->data.data()), _in->data.size(), &size.x(), &size.y(), &components, forceChannelCount));
        channelSize = 2;
        /* stb_image still returns the original component count in components,
           which we don't want/need */
        if(forceChannelCount) components = forceChannelCount;
        if(data) switch(components) {
            case 1: format = PixelFormat::R16Unorm;     break;
            case 2: format = PixelFormat::RG16Unorm;    break;
            case 3: format = PixelFormat::RGB16Unorm;   break;
            case 4: format = PixelFormat::RGBA16Unorm;  break;
            default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }
    } else {
        data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(_in->data.data()), _in->data.size(), &size.x(), &size.y(), &components, forceChannelCount);
        channelSize = 1;
        /* stb_image still returns the original component count in components,
           which we don't want/need */
        if(forceChannelCount) components = forceChannelCount;
        if(data) switch(components) {
            case 1: format = PixelFormat::R8Unorm;      break;
            case 2: format = PixelFormat::RG8Unorm;     break;
            case 3: format = PixelFormat::RGB8Unorm;    break;
            case 4: format = PixelFormat::RGBA8Unorm;   break;
            default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
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
    Utility::copy(Containers::arrayView(reinterpret_cast<char*>(data), imageData.size()), imageData);
    stbi_image_free(data);

    /* Adjust pixel storage if row size is not four byte aligned */
    PixelStorage storage;
    if((size.x()*components*channelSize)%4 != 0)
        storage.setAlignment(1);

    return Trade::ImageData2D{storage, format, size, std::move(imageData)};
}

}}

CORRADE_PLUGIN_REGISTER(StbImageImporter, Magnum::Trade::StbImageImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.3")
