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

#include "OpenExrImageConverter.h"

#include <thread> /* std::thread::hardware_concurrency(), sigh */
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Configuration is <string>-free */
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
#include <ImfTiledOutputFile.h>

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

OpenExrImageConverter::OpenExrImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures OpenExrImageConverter::doFeatures() const { return ImageConverterFeature::ConvertLevels2DToData|ImageConverterFeature::ConvertLevels3DToData; }

namespace {

Containers::Optional<Containers::Array<char>> convertToDataInternal(const Utility::ConfigurationGroup& configuration, const ImageConverterFlags flags, const PixelFormat format, const Int levelCount, void(*const preparePixelsForLevel)(Int, const Containers::StridedArrayView3D<char>&, void*), const Containers::StridedArrayView3D<char>& pixels, void* const state) try {
    /* Figure out type and channel count */
    Imf::PixelType type;
    std::size_t channelCount;
    switch(format) {
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
            Error{} << "Trade::OpenExrImageConverter::convertToData(): unsupported format" << format << Debug::nospace << ", only *16F, *32F, *32UI and Depth32F formats supported";
            return {};
    }
    switch(format) {
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
    const Containers::StringView compressionString = configuration.value<Containers::StringView>("compression");
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
    const Vector2i imageSize{Int(pixels.size()[1]), Int(pixels.size()[0])};
    const Vector2i dataOffsetMin = configuration.value<Vector2i>("dataOffset");
    const Vector2i dataOffsetMax = dataOffsetMin + imageSize - Vector2i{1};
    const Range2Di displayWindow = configuration.value("displayWindow").empty() ?
        Range2Di{{}, imageSize - Vector2i{1}} :
        configuration.value<Range2Di>("displayWindow");

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

    /* Compression levels, ZIP only since 3.1.3, DWA is set differently in
       earlier versions. There's also setDefault{Zip,Dwa}CompressionLevel() but
       because it's global I won't ever touch it. Also I hope setting DWA/ZIP
       compression even if it's not actually used won't be a problem. */
    #if OPENEXR_VERSION_MAJOR*10000 + OPENEXR_VERSION_MINOR*100 + OPENEXR_VERSION_PATCH >= 30103
    header.zipCompressionLevel() =
        configuration.value<Int>("zipCompressionLevel");
    header.dwaCompressionLevel() =
        configuration.value<Float>("dwaCompressionLevel");
    #else
    /* Add this header attribute only if it's a non-default value and we're
       actually using the DWA compression -- otherwise it just inflates the
       header size and has no reason to be there. */
    if((compression == Imf::DWAA_COMPRESSION || compression == Imf::DWAB_COMPRESSION) && configuration.value<Float>("dwaCompressionLevel") != 45.0f)
        Imf::addDwaCompressionLevel(header, configuration.value<Float>("dwaCompressionLevel"));
    #endif

    /* Set envmap metadata, if specified. The 2D/3D doConvertToData() already
       guards that latlong is only set for 2D and cubemap only for 3D plus all
       the size restrictions, so we can just assert here. */
    if(configuration.value("envmap") == "latlong") {
        Imf::addEnvmap(header, Imf::Envmap::ENVMAP_LATLONG);
    } else if(configuration.value("envmap") == "cube") {
        Imf::addEnvmap(header, Imf::Envmap::ENVMAP_CUBE);
    } else CORRADE_INTERNAL_ASSERT(configuration.value("envmap").empty());

    /* If a layer is specified, prefix all channels with it */
    std::string layerPrefix = configuration.value("layer");
    if(!layerPrefix.empty()) layerPrefix += '.';

    /* Write all channels that have assigned names */
    const char* const ChannelOptions[] {
        /* This will be insufficient once there's more than one allowed depth
           format */
        format == PixelFormat::Depth32F ? "depth" : "r", "g", "b", "a"
    };
    constexpr std::size_t ChannelSizes[] {
        4, /* UINT */
        2, /* HALF */
        4  /* FLOAT */
    };
    Imf::FrameBuffer framebuffer;
    for(std::size_t i = 0; i != channelCount; ++i) {
        std::string name = configuration.value(ChannelOptions[i]);
        if(name.empty()) continue;

        name = layerPrefix + name;

        /* OpenEXR uses a std::map inside the Imf::FrameBuffer, but doesn't
           actually do any error checking on top, which means if we
           accidentally supply the same channel twice, it'll get ignored ... or
           maybe it overwrites the previous one. Not sure. Neither behavior
           seems desirable, so let's fail on that. */
        if(framebuffer.findSlice(name)) {
            Error{} << "Trade::OpenExrImageConverter::convertToData(): duplicate mapping for channel" << name;
            return {};
        }

        header.channels().insert(name, Imf::Channel{type});
        framebuffer.insert(name, Imf::Slice{
            type,
            const_cast<char*>(static_cast<const char*>(pixels.data()))
                /* For some strange reason I have to supply a pointer to the
                   first pixel ever, not the first pixel inside the data
                   window */
                - dataOffsetMin.y()*std::size_t(pixels.stride()[0])
                - dataOffsetMin.x()*std::size_t(pixels.stride()[1])
                /* And an offset to this channel, as they're interleaved */
                + i*ChannelSizes[type],
            std::size_t(pixels.stride()[1]),
            std::size_t(pixels.stride()[0])
        });
    }

    /* There should be at least one channel written */
    if(framebuffer.begin() == framebuffer.end()) {
        Error{} << "Trade::OpenExrImageConverter::convertToData(): no channels assigned in plugin configuration";
        return {};
    }

    /* Increase global thread count if it's not enough. Value of 0 means single
       thread, while we use 1 for the same (consistent with BasisImageConverter and potential other plugins). */
    Int threadCount = configuration.value<Int>("threads");
    if(!threadCount) {
        threadCount = std::thread::hardware_concurrency();
        if(flags & ImageConverterFlag::Verbose)
            Debug{} << "Trade::OpenExrImageConverter::convertToData(): autodetected hardware concurrency to" << threadCount << "threads";
    }
    if(Imf::globalThreadCount() < threadCount - 1) {
        if(flags & ImageConverterFlag::Verbose)
            Debug{} << "Trade::OpenExrImageConverter::convertToData(): increasing global OpenEXR thread pool from" << Imf::globalThreadCount() << "to" << threadCount - 1 << "extra worker threads";
        Imf::setGlobalThreadCount(threadCount - 1);
    }

    /* Play it safe and destruct everything before we touch the array */
    Containers::Array<char> data;
    {
        MemoryOStream stream{data};

        /* Scanline output. Only if we have just one level and the output
           wasn't forced to be tiled. */
        if(levelCount == 1 && !configuration.value<bool>("forceTiledOutput")) {
            Imf::OutputFile file{stream, header, threadCount - 1};
            file.setFrameBuffer(framebuffer);

            /* For consistency, the pixels are assumed to be ready only after
               the prepareLevel() is called also in the single-level case */
            preparePixelsForLevel(0, pixels, state);
            file.writePixels(imageSize.y());

        /* Tiled output */
        } else {
            const Vector2i tileSize = configuration.value<Vector2i>("tileSize");
            header.setTileDescription(Imf::TileDescription{
                UnsignedInt(tileSize.x()),
                UnsignedInt(tileSize.y()),
                /* If we have just one level (because forceTiledOutput was
                   set), don't save as a mipmapped file because then it would
                   report all remaining levels as missing. */
                /** @todo ripmaps? */
                levelCount == 1 ? Imf::ONE_LEVEL : Imf::MIPMAP_LEVELS,
                Imf::ROUND_DOWN}); /** @todo configurable? can't use a >> 1 then */

            Imf::TiledOutputFile file{stream, header, threadCount - 1};
            file.setFrameBuffer(framebuffer);

            /* There doesn't seem to be a way to set level count, it's
               implicitly from the base size and rounding mode. For sanity
               check that we don't have more levels than the OpenEXR expects,
               this is expected to be checked gracefully by the caller. OTOH if
               we have less levels, the unwritten mips will get automatically
               marked as incomplete. */
            CORRADE_INTERNAL_ASSERT(file.numLevels() >= levelCount);

            /* Generate pixels for each levels and write them. This implicitly
               assumes that the first level is the largest and the remaining
               levels are each 2x smaller with ROUND_DOWN, the callers are
               checking for that to prevent garbled output. */
            for(Int level = 0; level != levelCount; ++level) {
                preparePixelsForLevel(level, pixels, state);
                file.writeTiles(0, file.numXTiles(level) - 1, 0, file.numYTiles(level) - 1, level);
            }
        }
    }

    /* Convert the growable array back to a non-growable with the default
       deleter so we can return it */
    arrayShrink(data);

    /* GCC 4.8 and Clang 3.8 need extra help here */
    return Containers::optional(std::move(data));

/* Good thing there are function try blocks, otherwise I would have to indent
   the whole thing. That would be awful. */
} catch(const Iex::BaseExc& e) {
    /* e.message() is only since 2.3.0, use what() for compatibility */
    Error{} << "Trade::OpenExrImageConverter::convertToData(): conversion error:" << e.what();
    return {};
}

}

Containers::Optional<Containers::Array<char>> OpenExrImageConverter::doConvertToData(const Containers::ArrayView<const ImageView2D> imageLevels) {
    if(configuration().value("envmap") == "latlong") {
        if(imageLevels[0].size().x() != 2*imageLevels[0].size().y()) {
            Error{} << "Trade::OpenExrImageConverter::convertToData(): a lat/long environment map has to have a 2:1 aspect ratio, got" << imageLevels[0].size();
            return {};
        }
    } else if(!configuration().value("envmap").empty()) {
        Error{} << "Trade::OpenExrImageConverter::convertToData(): unknown envmap option" << configuration().value("envmap") << "for a 2D image, expected either empty or latlong for 2D images and cube for 3D images";
        return {};
    }

    /* Verify that the image size gets divided by 2 in each level, rounded
       down, all the way to a 1x1 pixel image. If the image is not square, the
       shorter side stays at 1 px until the longer side gets there as well,
       however there should be only one 1x1 level at most. */
    for(std::size_t i = 1; i != imageLevels.size(); ++i) {
        const Vector2i expectedSize = imageLevels[0].size() >> i;
        if(expectedSize.isZero()) {
            Error{} << "Trade::OpenExrImageConverter::convertToData(): there can be only" << i << "levels with base image size" << imageLevels[0].size() << "but got" << imageLevels.size();
            return {};
        }
        if(imageLevels[i].size() != Math::max(expectedSize, Vector2i{1})) {
            Error{} << "Trade::OpenExrImageConverter::convertToData(): size of image at level" << i << "expected to be" << Math::max(expectedSize, Vector2i{1}) << "but got" << imageLevels[i].size();
            return {};
        }
    }

    /* According to my tests, Y flip could be done during image writing the
       same as when reading by supplying `std::size_t(-rowStride)`, as
       described in OpenExrImporter::doImage2D(). However, again, although it
       requires allocating a copy to perform the manual flip, I think it's the
       saner approach after all. */
    struct State {
        Containers::ArrayView<const ImageView2D> imageLevels;
        Containers::Array<char> flippedData;
    } state{
        imageLevels,
        Containers::Array<char>{NoInit, std::size_t(imageLevels[0].size().product()*imageLevels[0].pixelSize())},
    };
    const Containers::StridedArrayView3D<char> flippedPixels{state.flippedData, {
        std::size_t(imageLevels[0].size().y()),
        std::size_t(imageLevels[0].size().x()),
        /* pixels() returns a zero stride if the view is empty, do that here as
           well to avoid hitting an assert inside copy() */
        /** @todo why?! figure out and fix */
        imageLevels[0].size().isZero() ? 0 : imageLevels[0].pixelSize()
    }};
    return convertToDataInternal(configuration(), flags(), imageLevels[0].format(), imageLevels.size(), [](Int level, const Containers::StridedArrayView3D<char>& flippedPixels, void* const data) {
        State& state = *reinterpret_cast<State*>(data);
        const Containers::StridedArrayView3D<const char> pixels = state.imageLevels[level].pixels();
        const Containers::StridedArrayView3D<char> flippedPixelsForLevel = flippedPixels.prefix(pixels.size());
        Utility::copy(pixels.flipped<0>(), flippedPixelsForLevel);
    }, flippedPixels, &state);
}

Containers::Optional<Containers::Array<char>> OpenExrImageConverter::doConvertToData(const Containers::ArrayView<const ImageView3D> imageLevels) {
    /* Only cube map saving is supported right now, no deep data */
    if(configuration().value("envmap").empty()) {
        Error{} << "Trade::OpenExrImageConverter::convertToData(): arbitrary 3D image saving not implemented yet, the envmap option has to be set to cube in the configuration in order to save a cube map";
        return {};
    }

    if(configuration().value("envmap") == "cube") {
        if(imageLevels[0].size().x() != imageLevels[0].size().y() || imageLevels[0].size().z() != 6) {
            Error{} << "Trade::OpenExrImageConverter::convertToData(): a cubemap has to have six square slices, got" << imageLevels[0].size();
            return {};
        }

        /* Verify that the image size gets divided by 2 in each level, rounded
           down, all the way to a 1x1 pixel image, but still with 6 slices. The
           image has to be square so the additional complexity with rounding
           up to 1 from the 2D case doesn't apply here. */
        for(std::size_t i = 1; i != imageLevels.size(); ++i) {
            const Vector3i expectedSize{imageLevels[0].size().xy() >> i, 6};
            if(expectedSize.xy().isZero()) {
                Error{} << "Trade::OpenExrImageConverter::convertToData(): there can be only" << i << "levels with base cubemap image size" << imageLevels[0].size() << "but got" << imageLevels.size();
                return {};
            }
            if(imageLevels[i].size() != expectedSize) {
                Error{} << "Trade::OpenExrImageConverter::convertToData(): size of cubemap image at level" << i << "expected to be" << expectedSize << "but got" << imageLevels[i].size();
                return {};
            }
        }

    } else {
        Error{} << "Trade::OpenExrImageConverter::convertToData(): unknown envmap option" << configuration().value("envmap") << "for a 3D image, expected either empty or latlong for 2D images and cube for 3D images";
        return {};
    }

    /* Compared to the (simple) 2D case, the cube map case is a lot more
       complex -- either GL or EXR is insane and so we have to flip differently
       for each face:

        +X is X-flipped
        -X is X-flipped
        +Y is Y-flipped
        -Y is Y-flipped
        +Z is X-flipped
        -Z is X-flipped

       Moreover, the image can have arbitrary imageHeight() in its pixel
       storage, however OpenEXR treats even the cubemap as a 2D framebuffer and
       so there's no possibility to have arbitrary gaps between faces.

       Originally I had this implemented as a straight copy of the 2D
       conversion code, with each face getting a dedicated framebuffer, with Y
       flips done by OpenEXR itself and X flips done manually to a scratch
       memory first (because, as described in OpenExrImporter::doImage3D(), it
       can't do them on their own). So basically three slightly different
       copies of the same code doing framebuffer setup, channel mapping etc.,
       and then I realized I would need to extend & test *each* for mipmap
       support.

       This variant, which copies everything to a scratch memory first, doing
       desired flips in the process, is less efficient, but far easier to
       maintain. */
    struct State {
        Containers::ArrayView<const ImageView3D> imageLevels;
        Containers::Array<char> flippedData;
    } state{
        imageLevels,
        Containers::Array<char>{NoInit, std::size_t(imageLevels[0].size().product()*imageLevels[0].pixelSize())}
    };
    /* A 2D framebuffer for OpenEXR. From this we have to recreate a 3D view
       every time to access particular layers. Can't create a 3D view upfront
       and slice it because it has to be contiguous in Y. */
    Containers::StridedArrayView3D<char> flippedPixelsFlattened{state.flippedData, {
        std::size_t(imageLevels[0].size().z()*imageLevels[0].size().y()),
        std::size_t(imageLevels[0].size().x()),
        imageLevels[0].pixelSize()
    }};
    return convertToDataInternal(configuration(), flags(), imageLevels[0].format(), imageLevels.size(), [](const Int level, const Containers::StridedArrayView3D<char>& flippedPixelsFlattened, void* const data) {
        State& state = *reinterpret_cast<State*>(data);
        const Containers::StridedArrayView4D<const char> pixels = state.imageLevels[level].pixels();
        const Containers::StridedArrayView4D<char> flippedPixelsForLevel{
            state.flippedData,
            pixels.size(),
            {flippedPixelsFlattened.stride()[0]*std::ptrdiff_t(pixels.size()[1]),
             flippedPixelsFlattened.stride()[0],
             flippedPixelsFlattened.stride()[1],
             flippedPixelsFlattened.stride()[2]}
        };
        Utility::copy(pixels[0].flipped<1>(), flippedPixelsForLevel[0]);
        Utility::copy(pixels[1].flipped<1>(), flippedPixelsForLevel[1]);
        Utility::copy(pixels[2].flipped<0>(), flippedPixelsForLevel[2]);
        Utility::copy(pixels[3].flipped<0>(), flippedPixelsForLevel[3]);
        Utility::copy(pixels[4].flipped<1>(), flippedPixelsForLevel[4]);
        Utility::copy(pixels[5].flipped<1>(), flippedPixelsForLevel[5]);
    }, flippedPixelsFlattened, &state);
}

}}

CORRADE_PLUGIN_REGISTER(OpenExrImageConverter, Magnum::Trade::OpenExrImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3.2")
