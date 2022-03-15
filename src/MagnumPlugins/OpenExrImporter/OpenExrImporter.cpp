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

#include "OpenExrImporter.h"

#include <thread> /* std::thread::hardware_concurrency(), sigh */
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Configuration is <string>-free */
#include <Magnum/Trade/ImageData.h>
#include <Magnum/PixelFormat.h>

/* OpenEXR as a CMake subproject adds the OpenEXR/ directory to include path
   but not the parent directory, so we can't #include <OpenEXR/blah>. This
   can't really be fixed from outside, so unfortunately we have to do the same
   in case of an external OpenEXR. */
#include <IexBaseExc.h>
#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfInputFile.h>
#include <ImfIO.h>
#include <ImfStandardAttributes.h>
#include <ImfTiledInputFile.h>
#include <ImfTiledOutputFile.h>
#include <ImfTestFile.h>

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

namespace {

/* Basically a copy of MemoryMappedIStream in ReadingAndWritingImageFiles.pdf,
   except it's working directly on our array view. */
class MemoryIStream: public Imf::IStream {
    public:
        /** @todo propagate filename from input (only useful for error messages) */
        explicit MemoryIStream(const Containers::ArrayView<const char> data): Imf::IStream{""}, _data{data}, _position{} {}

        bool isMemoryMapped() const override { return true; }
        char* readMemoryMapped(const int n) override {
            /* Sigh, couldn't you just query file size and then do bounds check
               on your side?!?! */
            if(_position + n > _data.size())
                throw Iex::InputExc{"Reading past end of file."};

            const char* const data = _data + _position;
            _position += n;
            return const_cast<char*>(data); /* sigh WHY */
        }

        bool read(char c[], const int n) override {
            /* For some reason rawTileData() that does a very nasty checks for
               missing mip levels is calling this function with a null pointer
               (I assume it's due to the TileBuffer missing or whatever?) but
               expects the seek to be performed and all that so just pretend
               this is totally fine. */
            if(c) {
                /* Sigh, couldn't you just query file size and then do bounds
                   check on your side?!?! */
                if(_position + n > _data.size())
                    throw Iex::InputExc{"Reading past end of file."};

                std::memcpy(c, _data + _position, n);
            }

            _position += n;
            return _position < _data.size();
        }

        /* It's Imath::Int64 in 2.5 and older, which (what the fuck!) is
           actually unsigned, Imath::SInt64 is signed instead */
        std::uint64_t tellg() override { return _position; }
        void seekg(const std::uint64_t pos) override { _position = pos; }

    private:
        Containers::ArrayView<const char> _data;
        /* 32-bit on 32-bit systems because yeah there's no way to fit 6 GB of
           pixel data into memory there anyway so who cares */
        std::size_t _position;
};

}

struct OpenExrImporter::State {
    explicit State(Containers::Array<char>&& data): data{std::move(data)}, stream{this->data} {}

    Containers::Array<char> data;
    MemoryIStream stream;
    /* There's always just one or the other so ideally this should be in some
       sort of a union, but those are rather small (16 bytes) so it doesn't
       matter much. */
    Containers::Optional<Imf::InputFile> file;
    Containers::Optional<Imf::TiledInputFile> tiledFile;
    bool isCubeMap;
    Int completeLevelCount;
};

OpenExrImporter::OpenExrImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin) : AbstractImporter{manager, plugin} {}

OpenExrImporter::~OpenExrImporter() = default;

ImporterFeatures OpenExrImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool OpenExrImporter::doIsOpened() const { return !!_state; }

void OpenExrImporter::doClose() { _state = nullptr; }

void OpenExrImporter::doOpenData(Containers::Array<char>&& data, const DataFlags dataFlags) {
    /* Take over the existing array or copy the data if we can't */
    Containers::Array<char> dataCopy;
    if(dataFlags & (DataFlag::Owned|DataFlag::ExternallyOwned)) {
        dataCopy = std::move(data);
    } else {
        dataCopy = Containers::Array<char>{NoInit, data.size()};
        Utility::copy(data, dataCopy);
    }

    /* Set up the input stream using the MemoryIStream class above */
    Containers::Pointer<State> state{InPlaceInit, std::move(dataCopy)};

    /* Increase global thread count if it's not enough. Value of 0 means single
       thread, while we use 1 for the same (consistent with BasisImageConverter and potential other plugins). */
    Int threadCount = configuration().value<Int>("threads");
    if(!threadCount) {
        threadCount = std::thread::hardware_concurrency();
        if(flags() & ImporterFlag::Verbose)
            Debug{} << "Trade::OpenExrImporter::openData(): autodetected hardware concurrency to" << threadCount << "threads";
    }
    if(Imf::globalThreadCount() < threadCount - 1) {
        if(flags() & ImporterFlag::Verbose)
            Debug{} << "Trade::OpenExrImporter::openData(): increasing global OpenEXR thread pool from" << Imf::globalThreadCount() << "to" << threadCount - 1 << "extra worker threads";
        Imf::setGlobalThreadCount(threadCount - 1);
    }

    /* Open the file. There's two kinds of files, scanline and tiled. Tiled
       files support mipmaps. While tiled files can be opened through the
       scanline interface, scanline files can't be opened through the tiled
       interface and so it's not possible to have a single interface for
       dealing with mipmaps and regular files. */
    const Imf::Header* header;
    try {
        if(Imf::isTiledOpenExrFile(state->stream)) {
            state->tiledFile.emplace(state->stream, threadCount - 1);

            /* Ripmap files need extra care, we don't support those at the
               moment. */
            if(state->tiledFile->levelMode() == Imf::RIPMAP_LEVELS) {
                Warning{} << "Trade::OpenExrImporter::openData(): ripmap files not supported, importing only the top level";
                state->tiledFile = Containers::NullOpt;
                state->stream.seekg(0);
                state->file.emplace(state->stream, threadCount - 1);
                state->completeLevelCount = 1;
                header = &state->file->header();
            } else {
                state->completeLevelCount = state->tiledFile->numLevels();
                header = &state->tiledFile->header();
            }
        } else {
            state->file.emplace(state->stream, threadCount - 1);
            state->completeLevelCount = 1;
            header = &state->file->header();
        }
    } catch(const Iex::BaseExc& e) {
        /* e.message() is only since 2.3.0, use what() for compatibility */
        Error{} << "Trade::OpenExrImporter::openData(): import error:" << e.what();
        return;
    }

    /** @todo multipart support */

    /* Cube map files will be exposed as 3D images. However, because they're
       actually just a metadata bit slapped on a plain 2D image, guess what
       happens when they're mipmapped! Yes of course, they get mipmapped even
       PAST the point of 1x6, so at the end you'll end up with 1x3 and 1x1
       levels that are absolutely useless. And because OpenEXR has no concept
       of an incomplete mip chain, this is always the case. To point that out,
       we print a verbose message. */
    if(Imf::hasEnvmap(*header) && Imf::envmap(*header) == Imf::ENVMAP_CUBE) {
        state->isCubeMap = true;
        if(state->tiledFile) {
            for(Int i = 0; i != state->tiledFile->numLevels(); ++i) {
                if(state->tiledFile->levelHeight(i) < 6) {
                    if(flags() >= ImporterFlag::Verbose) Debug{} << "Trade::OpenExrImporter::openData(): last" << state->tiledFile->numLevels() - i << "levels are too small to represent six cubemap faces (" << Debug::nospace << Vector2i{state->tiledFile->levelWidth(i), state->tiledFile->levelHeight(i)} << Debug::nospace <<"), capping at" << i << "levels";
                    state->completeLevelCount = i;
                    break;
                }
            }
        }
    } else state->isCubeMap = false;

    /* OpenEXR has no concept of an incomplete mip chain (for example having no
       4x4, 2x2 and 1x1 images), instead a level (or particular tiles in it)
       can be missing. We'll ignore missing levels at the end, but still treat
       missing levels in the middle and partially missing levels as import
       error (which will fail with "Tile (a, b, c, d) is missing").

       OF COURSE nothing is ever easy and there's just a SINGLE boolean getter
       for whether the whole file is complete, with the docs suggesting that I
       catch some stupid exception DURING A READ when a tile is missing. HAHA
       FUCK, I NEED THAT SOONER THO. The info for particular tiles *is* there
       (because that's how the exception gets thrown, eh?), but hidden in a
       private struct, and even though I could use Imf::TileOffsets::readFrom()
       to reparse this information from the file header and then go somewhat
       sanely tile by tile, the corresponding include file isn't installed.

       "Fortunately" there's rawTileData(). The docs are useless and after
       wasting a ton of time deep in the sources, I realized that the only
       thing it does is sequentially reading through the file and DISCOVERING
       tiles and their coordinates as it goes:
        https://github.com/AcademySoftwareFoundation/openexr/blob/v3.0.4/src/lib/OpenEXR/ImfTiledInputFile.cpp#L470-L474
       And because the EXR file has the tile data at its very end, when this
       thing reaches the end, it'll simply cause the MemoryIStream::read() to
       throw up. Not to mention this function calls it with a NULL POINTER,
       which is very amazing, very -- or is that because the TileBuffer thing
       isn't initialized properly? Hmmm. OTOH that could mean all the data
       actually aren't copied during the process, which would make it kinda
       the same efficiency as TileOffsets::readFrom()?
        https://github.com/AcademySoftwareFoundation/openexr/blob/v3.0.4/src/lib/OpenEXR/ImfTiledInputFile.cpp#L1408
       */
    if(state->tiledFile && !state->tiledFile->isComplete()) {
        /** @todo BitArray */
        Containers::Array<bool> tilesPresentInLevel{DirectInit, std::size_t(state->tiledFile->numLevels()), false};
        try {
            for(Int level = 0; level != state->tiledFile->numLevels(); ++level) {
                for(Int y = 0; y != state->tiledFile->numYTiles(level); ++y) {
                    for(Int x = 0; x != state->tiledFile->numXTiles(level); ++x) {
                        /* For multipart files rawTileData() needs the actual
                           tile index as it seeks, for single-part it ignores
                           them and overwrites with whatever is there at that
                           point. */
                        Int dx = x, dy = y, lx = level, ly = level, pixelDataSize;
                        const char* pixelData{};
                        state->tiledFile->rawTileData(dx, dy, lx, ly, pixelData, pixelDataSize);

                        /* If it didn't throw, mark this level as present */
                        tilesPresentInLevel[lx] = true;
                    }
                }
            }
        } catch(const Iex::InputExc&) {
            /* It gotta throw at some point, but we have nothing to do about
               that */
        }

        /* Find the last level that has at least one tile (but at least one),
           everything after is going to be cut away. In case of a cubemap the
           level count might already be cut away, so print the message only if
           we're cutting further than that. */
        for(Int level = state->tiledFile->numLevels(); level > 0; --level) {
            if(tilesPresentInLevel[level - 1]) {
                if(level < state->completeLevelCount) {
                    if(flags() & ImporterFlag::Verbose)
                        Debug{} << "Trade::OpenExrImporter::openData(): last" << state->tiledFile->numLevels() - level << "levels are missing in the file, capping at" << level << "levels";
                    state->completeLevelCount = level;
                }

                break;
            }
        }
    }

    /* All good, save the state */
    _state = std::move(state);
}

namespace {

/* level = -1 means file is InputFile, non-negative value is TiledInputFile */
Containers::Optional<ImageData2D> imageInternal(const Utility::ConfigurationGroup& configuration, Imf::GenericInputFile& file, const Int level, const char* messagePrefix) try {
    const Imf::Header* header;
    Imath::Box2i dataWindow;
    if(level == -1) {
        header = &static_cast<Imf::InputFile&>(file).header();
        dataWindow = header->dataWindow();
    } else {
        auto& actual = static_cast<Imf::TiledInputFile&>(file);
        header = &actual.header();
        dataWindow = actual.dataWindowForLevel(level);
    }
    const Vector2i size{dataWindow.max.x - dataWindow.min.x + 1,
                        dataWindow.max.y - dataWindow.min.y + 1};

    /* Figure out channel mapping */
    const Imf::ChannelList& channels = header->channels();
    std::string mapping[]{
        configuration.value("r"),
        configuration.value("g"),
        configuration.value("b"),
        configuration.value("a")
    };
    std::string depthMapping = configuration.value("depth");

    /* If a layer is specified, prefix all channels with it. Channels that are
       empty will stay so. */
    std::string layerPrefix = configuration.value("layer");
    if(!layerPrefix.empty()) {
        layerPrefix += '.';
        for(std::string* i: {mapping, mapping + 1, mapping + 2, mapping + 3, &depthMapping})
            if(!i->empty()) *i = layerPrefix + *i;
    }

    /* Pixel type. For RGBA it's queried from the channels, for depth it's
       forced to be Depth32F. */
    Containers::Optional<Imf::PixelType> type;
    bool isDepth;

    /* Try RGBA, if at least one channel is present */
    if((!mapping[0].empty() && channels.findChannel(mapping[0])) ||
       (!mapping[1].empty() && channels.findChannel(mapping[1])) ||
       (!mapping[2].empty() && channels.findChannel(mapping[2])) ||
       (!mapping[3].empty() && channels.findChannel(mapping[3]))) {
        isDepth = false;

    /* Otherwise, if depth mapping is present, try that. That forces the output
       to be a single channel and the type to be FLOAT. */
    } else if(!depthMapping.empty() && channels.findChannel(depthMapping)) {
        mapping[0] = depthMapping;
        mapping[1] = {};
        mapping[2] = {};
        mapping[3] = {};
        type = Imf::FLOAT;
        isDepth = true;

    /* Otherwise we have no idea. Be helpful and provide all channel names in
       the error message. */
    } else {
        /* FFS crap "fancy c minus minus" APIs, NO WAY to query channel count,
           no way to use a range-for, no nothing. CRAP. */
        Containers::Array<Containers::StringView> channelNames;
        for(auto it = channels.begin(); it != channels.end(); ++it)
            arrayAppend(channelNames, InPlaceInit, it.name());

        Error{} << messagePrefix << "can't perform automatic mapping for channels named {" << Debug::nospace << ", "_s.join(channelNames) << Debug::nospace << "}, to either {" << Debug::nospace << ", "_s.join({mapping[0], mapping[1], mapping[2], mapping[3]}) << Debug::nospace << "} or" << depthMapping << Debug::nospace << ", provide desired layer and/or channel names in plugin configuration";
        return {};
    }

    /* Decide on channel count and common format for all. The `channelCount` is
       always overwriten in the loop below (and then checked by the assert for
       extra robustness), but GCC tries to be "smart" and complains that it
       "may be used unitialized". BLAH BLAH shut up. */
    std::size_t channelCount{};
    for(std::size_t i = 0; i != 4; ++i) {
        /* If there's no mapping or if the channel is not present in the file,
           skip. Mapped channels that are not present will still be added to
           the framebuffer to make OpenEXR fill them with default values, but
           they don't contribute to the channel count or common type in any
           way. */
        if(mapping[i].empty() || !channels.findChannel(mapping[i]))
            continue;

        channelCount = i + 1;

        CORRADE_INTERNAL_ASSERT(UnsignedInt(channels[mapping[i]].type) < Imf::NUM_PIXELTYPES);
        if(!type) type = channels[mapping[i]].type;
        else if(*type != channels[mapping[i]].type) {
            constexpr const char* PixelTypeName[] {
                "UINT",
                "HALF",
                "FLOAT"
            };

            /* For depth, the type is already set to FLOAT above, so this will
               double as a consistency check there as well */
            Error{} << messagePrefix << "channel" << mapping[i] << "expected to be a" << PixelTypeName[*type] << "but got" << PixelTypeName[channels[mapping[i]].type];
            return {};
        }
    }

    /* There should be at least one channel */
    CORRADE_INTERNAL_ASSERT(type);

    /* Force channel count for RGBA, if requested */
    if(!isDepth) if(const Int forceChannelCount = configuration.value<Int>("forceChannelCount")) {
        if(forceChannelCount < 0 || forceChannelCount > 4) {
            Error{} << messagePrefix << "forceChannelCount is expected to be 0-4, got" << forceChannelCount;
            return {};
        }

        channelCount = forceChannelCount;
    }

    /** @todo YUV? look how RgbaInputImage does that and apply here */

    /* Decide on the output PixelFormat */
    constexpr PixelFormat RgbaFormats[3][4] {
        { /* UINT */
            PixelFormat::R32UI,
            PixelFormat::RG32UI,
            PixelFormat::RGB32UI,
            PixelFormat::RGBA32UI,
        }, { /* HALF */
            PixelFormat::R16F,
            PixelFormat::RG16F,
            PixelFormat::RGB16F,
            PixelFormat::RGBA16F,
        }, { /* FLOAT */
            PixelFormat::R32F,
            PixelFormat::RG32F,
            PixelFormat::RGB32F,
            PixelFormat::RGBA32F,
        }
    };
    /* Currently, there's just one pixel format suitable for depth. If that
       ever changes, we need to have a DepthFormats mapping table as well. */
    CORRADE_INTERNAL_ASSERT(!isDepth || (channelCount == 1 && type == Imf::FLOAT));
    const PixelFormat format = isDepth ?
        PixelFormat::Depth32F : RgbaFormats[*type][channelCount - 1];

    /* Calculate output size, align rows to four bytes */
    constexpr std::size_t ChannelSizes[] {
        4, /* UINT */
        2, /* HALF */
        4  /* FLOAT */
    };
    const std::size_t channelSize = ChannelSizes[*type];
    const std::size_t pixelSize = channelCount*channelSize;
    const std::size_t rowStride = 4*((size.x()*pixelSize + 3)/4);

    /* Output array. If we have unassigned RGBA channels, zero-init them (the
       depth channel is always assigned). OTOH we don't care about the padding,
       that can stay random. */
    Containers::Array<char> out;
    if((mapping[0].empty() ||
        mapping[1].empty() ||
        mapping[2].empty() ||
        mapping[3].empty()) && !isDepth)
    {
        out = Containers::Array<char>{ValueInit, std::size_t{rowStride*size.y()}};
    } else {
        out = Containers::Array<char>{NoInit, std::size_t{rowStride*size.y()}};
    }

    Imf::FrameBuffer framebuffer;
    constexpr const char* FillOptions[] {
        "rFill", "gFill", "bFill", "aFill"
    };
    for(std::size_t i = 0; i != channelCount; ++i) {
        if(mapping[i].empty()) continue;

        /* OpenEXR uses a std::map inside the Imf::FrameBuffer, but doesn't
           actually do any error checking on top, which means if we
           accidentally supply the same channel twice, it'll get ignored ... or
           maybe it overwrite the previous one. Not sure. Neither behavior
           seems desirable, so let's fail on that. */
        if(framebuffer.findSlice(mapping[i])) {
            Error{} << messagePrefix << "duplicate mapping for channel" << mapping[i];
            return {};
        }

        framebuffer.insert(mapping[i], Imf::Slice{
            *type,
            out.data()
                /* For some strange reason I have to supply a pointer to the
                   first pixel ever, not the first pixel inside the data
                   window */
                - dataWindow.min.y*rowStride
                - dataWindow.min.x*pixelSize
                /* And an offset to this channel, as they're interleaved */
                + i*channelSize,
            pixelSize,
            rowStride,
            1, 1,
            configuration.value<Double>(FillOptions[i])
        });
    }

    /* Sanity check, implied from the fact that the mappings are not empty */
    CORRADE_INTERNAL_ASSERT(framebuffer.begin() != framebuffer.end());

    if(level == -1) {
        auto& actual = static_cast<Imf::InputFile&>(file);
        actual.setFrameBuffer(framebuffer);
        actual.readPixels(dataWindow.min.y, dataWindow.max.y);
    } else {
        auto& actual = static_cast<Imf::TiledInputFile&>(file);
        actual.setFrameBuffer(framebuffer);
        actual.readTiles(0, actual.numXTiles(level) - 1, 0, actual.numYTiles(level) - 1, level);
    }

    return Trade::ImageData2D{format, size, std::move(out)};

/* Good thing there are function try blocks, otherwise I would have to indent
   the whole thing. That would be awful. */
} catch(const Iex::BaseExc& e) {
    /* e.message() is only since 2.3.0, use what() for compatibility */
    Error{} << messagePrefix << "import error:" << e.what();
    return {};
}

}

UnsignedInt OpenExrImporter::doImage2DCount() const {
    return _state->isCubeMap ? 0 : 1;
}

UnsignedInt OpenExrImporter::doImage2DLevelCount(UnsignedInt) {
    /* This gets called only if _state->isCubeMap is false (guarded by
       doImage2DCount()) so we don't need to check for that here again */
    return _state->completeLevelCount;
}

Containers::Optional<ImageData2D> OpenExrImporter::doImage2D(UnsignedInt, const UnsignedInt level) {
    Containers::Optional<ImageData2D> image;
    if(_state->file) {
        image = imageInternal(configuration(), *_state->file, -1, "Trade::OpenExrImporter::image2D():");
    } else {
        image = imageInternal(configuration(), *_state->tiledFile, level, "Trade::OpenExrImporter::image2D():");
    }

    /* Let's stop here for a bit and contemplate on all the missed
       opportunities. The OpenEXR framebuffer contains mapping of particular
       channels to strided 2D memory locations, which sounds extremely great...
       in theory. In practice, UNFORTUNATELY:

        1.  Strides are a size_t, which means the library doesn't want me to
            use it to do an Y flip (or an X flip, for that matter).
        2.  The file contains an INCREASING_Y or DECREASING_Y attribute, but
            that's only used when writing the file, I suppose to allow
            streaming the data in Y up direction without having to buffer
            everything. It would be great if I could consume the file in the
            other direction as well, but the API doesn't allow me to and
            instead shuffles the data around only for me to shuffle them back.
        3.  file.readPixels() takes two parameters. That would be a THIRD
            opportunity to allow an Y-flip, BUT NO, the two parameters are
            interpreted the same way regardless of whether I do this or that:

                file.readPixels(dataWindow.max.y, dataWindow.min.y)
                file.readPixels(dataWindow.min.y, dataWindow.max.y)

       According to the PDFs, readPixels() is where multithreading happens, so
       calling it one by one with a different framebuffer setup to adjust for
       an Y flip would be a sequential misery. TL;DR: At first I was happy
       because EXR seemed like finally a format developed by the *real* VFX
       industry but nah, it's the same poorly implemented shit with pointless
       restrictions as everything else.

       Later I discovered that the library has very poor checks for out of
       bounds accesses and so it seems I can force `std::size_t(-rowStride)`
       together with a specially crafted base pointer and it'll work without
       throwing confused exceptions at us.

       But then I patted myself on the back for being such a 1337 H4X0R and
       deleted all that. For my sanity I'm doing a flip on the resulting data
       instead, which is also consistent with what needs to be done for
       cubemaps below. */
    if(image) Utility::flipInPlace<0>(image->mutablePixels());

    return image;
}

UnsignedInt OpenExrImporter::doImage3DCount() const {
    return _state->isCubeMap ? 1 : 0;
}

UnsignedInt OpenExrImporter::doImage3DLevelCount(UnsignedInt) {
    /* This gets called only if _state->isCubeMap is true (guarded by
       doImage3DCount()) so we don't need to check for that here again */
    return _state->completeLevelCount;
}

Containers::Optional<ImageData3D> OpenExrImporter::doImage3D(UnsignedInt, const UnsignedInt level) {
    Containers::Optional<ImageData2D> image2D;
    if(_state->file) {
        image2D = imageInternal(configuration(), *_state->file, -1, "Trade::OpenExrImporter::image3D():");
    } else {
        image2D = imageInternal(configuration(), *_state->tiledFile, level, "Trade::OpenExrImporter::image3D():");
    }
    if(!image2D) return {};

    /* Compared to the (simple) 2D case, the cube map case is a lot more
       complex -- either GL or EXR is insane and so I have to flip differently
       for each face:

        +X is X-flipped
        -X is X-flipped
        +Y is Y-flipped
        -Y is Y-flipped
        +Z is X-flipped
        -Z is X-flipped

       It could have worked by creating six different framebuffers, with each
       set up differently, however while Y flip would be possible using the
       `std::size_t(-rowStride)` hack mentioned above unfortunately I can't do
       the same for X. The scanline copying code in question
       (copyIntoFrameBuffer() in ImfMisc.cpp and the code calling it from
       ImfScanLineInputFile.cpp) is along these lines, i.e. endPtr being
       already smaller than writePtr to begin with and thus the loop never
       entered:

        char* writePtr = linePtr + dMinX*xStride;
        char* endPtr = linePtr + dmaxX*xStride;
        …
        while(writePtr <= endPtr) {
            …
            writePtr += xStride;
        }

       Which means I'd have to special-case the ±X/±Z faces and perform X flip
       manually, at which point I realized I could just throw it all away and
       do the flip in post on the imported data, thus happily sharing all code
       between the 2D and cubemap case. */
    const Containers::StridedArrayView3D<const char> pixels2D = image2D->pixels();
    const Containers::StridedArrayView4D<char> pixels{image2D->mutableData(),
        {6,
         pixels2D.size()[0]/6,
         pixels2D.size()[1],
         pixels2D.size()[2]},
        {std::ptrdiff_t(pixels2D.stride()[0]*(pixels2D.size()[0]/6)),
         pixels2D.stride()[0],
         pixels2D.stride()[1],
         pixels2D.stride()[2]}
    };
    Utility::flipInPlace<1>(pixels[0]);
    Utility::flipInPlace<1>(pixels[1]);
    Utility::flipInPlace<0>(pixels[2]);
    Utility::flipInPlace<0>(pixels[3]);
    Utility::flipInPlace<1>(pixels[4]);
    Utility::flipInPlace<1>(pixels[5]);

    return ImageData3D{image2D->format(), {Int(pixels.size()[2]), Int(pixels.size()[1]), Int(pixels.size()[0])}, image2D->release()};
}

}}

CORRADE_PLUGIN_REGISTER(OpenExrImporter, Magnum::Trade::OpenExrImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.5")
