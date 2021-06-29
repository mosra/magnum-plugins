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

#include "OpenExrImageConverter.h"

#include <cstring>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/ConfigurationValue.h>

/* OpenEXR as a CMake subproject adds the OpenEXR/ directory to include path
   but not the parent directory, so we can't #include <OpenEXR/blah>. This
   can't really be fixed from outside, so unfortunately we have to do the same
   in case of an external OpenEXR. */
#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfIO.h>
#include <ImfOutputFile.h>
#include <ImfStandardAttributes.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

namespace {

/* Unlike IStream, this does not have an example snippet in the PDF so I just
   hope I'm not doing something extremely silly. */
class MemoryOStream: public Imf::OStream {
    public:
        /** @todo propagate filename from input (only useful for error messages) */
        explicit MemoryOStream(Containers::Array<char>& data): Imf::OStream{""}, _data(data), _position{} {}

        void write(const char c[], int n) override {
            if(_position + n > _data.size())
                arrayAppend(_data, NoInit, _position + n - _data.size());

            std::memcpy(_data + _position, c, n);
            _position += n;
        }

        /* It's Imath::Int64 in 2.5 and older, which (what the fuck!) is
           actually unsigned, Imath::SInt64 is signed instead */
        std::uint64_t tellp() override { return _position; }
        void seekp(const std::uint64_t pos) override { _position = pos; }

    private:
        Containers::Array<char>& _data;
        /* 32-bit on 32-bit systems because yeah there's no way to fit 6 GB of
           pixel data into memory there anyway so who cares */
        std::size_t _position;
};

}

OpenExrImageConverter::OpenExrImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures OpenExrImageConverter::doFeatures() const { return ImageConverterFeature::Convert2DToData; }

Containers::Array<char> OpenExrImageConverter::doConvertToData(const ImageView2D& image) try {
    /* Figure out type and channel count */
    Imf::PixelType type;
    std::size_t channelCount;
    switch(image.format()) {
        case PixelFormat::R16F:
        case PixelFormat::RG16F:
        case PixelFormat::RGB16F:
        case PixelFormat::RGBA16F:
            type = Imf::HALF;
            break;
        case PixelFormat::R32F:
        case PixelFormat::RG32F:
        case PixelFormat::RGB32F:
        case PixelFormat::RGBA32F:
        case PixelFormat::Depth32F:
            type = Imf::FLOAT;
            break;
        case PixelFormat::R32UI:
        case PixelFormat::RG32UI:
        case PixelFormat::RGB32UI:
        case PixelFormat::RGBA32UI:
            type = Imf::UINT;
            break;
        default:
            Error{} << "Trade::OpenExrImageConverter::convertToData(): unsupported format" << image.format() << Debug::nospace << ", only *16F, *32F, *32UI and Depth32F formats supported";
            return {};
    }
    switch(image.format()) {
        case PixelFormat::R16F:
        case PixelFormat::R32F:
        case PixelFormat::R32UI:
        case PixelFormat::Depth32F:
            channelCount = 1;
            break;
        case PixelFormat::RG16F:
        case PixelFormat::RG32F:
        case PixelFormat::RG32UI:
            channelCount = 2;
            break;
        case PixelFormat::RGB16F:
        case PixelFormat::RGB32F:
        case PixelFormat::RGB32UI:
            channelCount = 3;
            break;
        case PixelFormat::RGBA16F:
        case PixelFormat::RGBA32F:
        case PixelFormat::RGBA32UI:
            channelCount = 4;
            break;
        /* Should have failed above already */
        default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }

    /* Output compression. Using the same naming scheme as exrenvmap does,
       except for no compression: https://github.com/AcademySoftwareFoundation/openexr/blob/931618b9088fd03ed4fe30cade55664da94a5854/src/bin/exrenvmap/main.cpp#L138-L174 */
    const Containers::StringView compressionString = configuration().value<Containers::StringView>("compression");
    Imf::Compression compression;
    if(compressionString.isEmpty())
        compression = Imf::NO_COMPRESSION;
    /* LCOV_EXCL_START */
    else if(compressionString == "rle"_s)
        compression = Imf::RLE_COMPRESSION;
    else if(compressionString == "zip"_s)
        compression = Imf::ZIP_COMPRESSION;
    else if(compressionString == "zips"_s)
        compression = Imf::ZIPS_COMPRESSION;
    else if(compressionString == "piz"_s)
        compression = Imf::PIZ_COMPRESSION;
    else if(compressionString == "pxr24"_s)
        compression = Imf::PXR24_COMPRESSION;
    else if(compressionString == "b44"_s)
        compression = Imf::B44_COMPRESSION;
    else if(compressionString == "b44a"_s)
        compression = Imf::B44A_COMPRESSION;
    else if(compressionString == "dwaa"_s)
        compression = Imf::DWAA_COMPRESSION;
    else if(compressionString == "dwab"_s)
        compression = Imf::DWAB_COMPRESSION;
    /* LCOV_EXCL_STOP */
    else {
        Error{} << "Trade::OpenExrImageConverter::convertToData(): unknown compression" << compressionString << Debug::nospace << ", allowed values are rle, zip, zips, piz, pxr24, b44, b44a, dwaa, dwab or empty for uncompressed output";
        return {};
    }

    /* Data window */
    const Vector2i dataOffsetMin = configuration().value<Vector2i>("dataOffset");
    const Vector2i dataOffsetMax = dataOffsetMin + image.size() - Vector2i{1};
    const Range2Di displayWindow = configuration().value("displayWindow").empty() ?
        Range2Di{{}, image.size() - Vector2i{1}} :
        configuration().value<Range2Di>("displayWindow");

    /* Header with basic info */
    Imf::Header header{
        {{displayWindow.min().x(), displayWindow.min().y()},
         {displayWindow.max().x(), displayWindow.max().y()}},
        {{dataOffsetMin.x(), dataOffsetMin.y()},
         {dataOffsetMax.x(), dataOffsetMax.y()}},
        1.0f,                   /* pixel aspect ratio, default */
        Imath::V2f{0.0f, 0.0f}, /* screen window center, default */
        1.0f,                   /* screen window width, default */
        /* Even though we're saving an image upside down, this doesn't seem to
           have any effect on anything (probably because we save all scanlines
           in one run?). So keep it at the default. */
        Imf::INCREASING_Y,
        compression
    };

    /* Get image pixel view and do the Y flipping right away. */
    const Containers::StridedArrayView3D<const char> pixels = image.pixels().flipped<0>();

    /* If a layer is specified, prefix all channels with it */
    std::string layerPrefix = configuration().value("layer");
    if(!layerPrefix.empty()) layerPrefix += '.';

    /* Write all channels that have assigned names */
    const char* const ChannelOptions[] {
        /* This will be insufficient once there's more than one allowed depth
           format */
        image.format() == PixelFormat::Depth32F ? "depth" : "r", "g", "b", "a"
    };
    constexpr std::size_t ChannelSizes[] {
        4, /* UINT */
        2, /* HALF */
        4  /* FLOAT */
    };
    Imf::FrameBuffer framebuffer;
    for(std::size_t i = 0; i != channelCount; ++i) {
        std::string name = configuration().value(ChannelOptions[i]);
        if(name.empty()) continue;

        name = layerPrefix + name;

        /* OpenEXR uses a std::map inside the Imf::FrameBuffer, but doesn't
           actually do any error checking on top, which means if we
           accidentally supply the same channel twice, it'll get ignored ... or
           maybe it overwrite the previous one. Not sure. Neither behavior
           seems desirable, so let's fail on that. */
        if(framebuffer.findSlice(name)) {
            Error{} << "Trade::OpenExrImageConverter::convertToData(): duplicate mapping for channel" << name;
            return {};
        }

        header.channels().insert(name, Imf::Channel{type});
        framebuffer.insert(name, Imf::Slice{
            type,
            /* Same as with OpenExrImporter, this is actually a pointer to the
               *last* row and the stride is negative (pixels were flipped<0>()
               above) */
            const_cast<char*>(
                static_cast<const char*>(pixels.data()) + ChannelSizes[type]*i
                /* And we have to take into account any custom data window as
                   well */
                - dataOffsetMin.y()*std::size_t(pixels.stride()[0])
                - dataOffsetMin.x()*std::size_t(pixels.stride()[1])
            ),
            std::size_t(pixels.stride()[1]),
            std::size_t(pixels.stride()[0]),
        });
    }

    /* There should be at least one channel written */
    if(framebuffer.begin() == framebuffer.end()) {
        Error{} << "Trade::OpenExrImageConverter::convertToData(): no channels assigned in plugin configuration";
        return {};
    }

    /* Play it safe and destruct everything before we touch the array */
    Containers::Array<char> data;
    {
        MemoryOStream stream{data};
        Imf::OutputFile file{stream, header};
        file.setFrameBuffer(framebuffer);
        file.writePixels(image.size().y());
    }

    /* Convert the growable array back to a non-growable with the default
       deleter so we can return it */
    arrayShrink(data);
    return data;

/* Good thing there are function try blocks, otherwise I would have to indent
   the whole thing. That would be awful. */
} catch(const Iex::BaseExc& e) {
    /* e.message() is only since 2.3.0, use what() for compatibility */
    Error{} << "Trade::OpenExrImageConverter::convertToData(): conversion error:" << e.what();
    return {};
}

}}

CORRADE_PLUGIN_REGISTER(OpenExrImageConverter, Magnum::Trade::OpenExrImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3")
