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

#include "OpenExrImporter.h"

#include <cstring>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/ConfigurationGroup.h>
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
#include <ImfInputPart.h>
#include <ImfMultiPartInputFile.h>
#include <ImfIO.h>

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
            /* Sigh, couldn't you just query file size and then do bounds check
               on your side?!?! */
            if(_position + n > _data.size())
                throw Iex::InputExc{"Reading past end of file."};

            std::memcpy(c, _data + _position, n);
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
    explicit State(Containers::Array<char>&& data): data{std::move(data)}, stream{this->data}, file{stream} {}

    Containers::Array<char> data;
    MemoryIStream stream;
    Imf::InputFile file;
};

OpenExrImporter::OpenExrImporter(PluginManager::AbstractManager& manager, const std::string& plugin) : AbstractImporter{manager, plugin} {}

OpenExrImporter::~OpenExrImporter() = default;

ImporterFeatures OpenExrImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool OpenExrImporter::doIsOpened() const { return !!_state; }

void OpenExrImporter::doClose() { _state = nullptr; }

void OpenExrImporter::doOpenData(const Containers::ArrayView<const char> data) {
    /* Make an owned copy of the data */
    Containers::Array<char> dataCopy{NoInit, data.size()};
    Utility::copy(data, dataCopy);

    /* Open the file */
    Containers::Pointer<State> state;
    try {
        state.emplace(std::move(dataCopy));
    } catch(const Iex::BaseExc& e) {
        /* e.message() is only since 2.3.0, use what() for compatibility */
        Error{} << "Trade::OpenExrImporter::openData(): import error:" << e.what();
        return;
    }

    /** @todo thread setup */
    /** @todo multipart support */

    /* All good, save the state */
    _state = std::move(state);
}

UnsignedInt OpenExrImporter::doImage2DCount() const { return 1; }

Containers::Optional<ImageData2D> OpenExrImporter::doImage2D(UnsignedInt, UnsignedInt) try {
    const Imf::Header header = _state->file.header();
    const Imath::Box2i dataWindow = header.dataWindow();
    const Vector2i size{dataWindow.max.x - dataWindow.min.x + 1,
                        dataWindow.max.y - dataWindow.min.y + 1};

    /* Figure out channel mapping */
    const Imf::ChannelList& channels = header.channels();
    std::string mapping[]{
        configuration().value("r"),
        configuration().value("g"),
        configuration().value("b"),
        configuration().value("a")
    };
    std::string depthMapping = configuration().value("depth");

    /* If a layer is specified, prefix all channels with it. Channels that are
       empty will stay so. */
    std::string layerPrefix = configuration().value("layer");
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

        Error{} << "Trade::OpenExrImporter::image2D(): can't perform automatic mapping for channels named {" << Debug::nospace << ", "_s.join(channelNames) << Debug::nospace << "}, to either {" << Debug::nospace << ", "_s.join({mapping[0], mapping[1], mapping[2], mapping[3]}) << Debug::nospace << "} or" << depthMapping << Debug::nospace << ", provide desired layer and/or channel names in plugin configuration";
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
            Error{} << "Trade::OpenExrImporter::image2D(): channel" << mapping[i] << "expected to be a" << PixelTypeName[*type] << "but got" << PixelTypeName[channels[mapping[i]].type];
            return {};
        }
    }

    /* There should be at least one channel */
    CORRADE_INTERNAL_ASSERT(type);

    /* Force channel count for RGBA, if requested */
    if(!isDepth) if(const Int forceChannelCount = configuration().value<Int>("forceChannelCount")) {
        if(forceChannelCount < 0 || forceChannelCount > 4) {
            Error{} << "Trade::OpenExrImporter::image2D(): forceChannelCount is expected to be 0-4, got" << forceChannelCount;
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

    /* Set up the output array and framebuffer layout for reading. The
       framebuffer contains mapping of particular channels to strided 2D memory
       locations, which sounds extremely great... in theory. In practice,
       UNFORTUNATELY:

        1.  Strides are a size_t, which means the library doesn't want us to
            use it to do an Y flip.
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

       FORTUNATELY, the library has very poor checks for out of bounds accesses
       and so it seems we can force a `-rowStride` together with a specially
       crafted base pointer and it'll work without throwing confused exceptions
       at us. Hopefully  */
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
            Error{} << "Trade::OpenExrImporter::image2D(): duplicate mapping for channel" << mapping[i];
            return {};
        }

        framebuffer.insert(mapping[i], Imf::Slice{
            *type,
            out.data()
                /* For some strange reason I have to supply a pointer to the
                   channel of the first pixel ever, not the first pixel inside
                   the data window */
                - dataWindow.min.y*rowStride - dataWindow.min.x*pixelSize
                /* This, however, actually has to points to the first pixel of
                   the *last* row (plus the minimal offset agin, which then
                   cancels out with the above), as we do an Y flip */
                + dataWindow.min.y*rowStride + dataWindow.max.y*rowStride
                /* And an offset to this channel, as they're interleaved */
                + i*channelSize,
            pixelSize,
            -rowStride, /* Y flip */
            1, 1,
            configuration().value<Double>(FillOptions[i])
        });
    }

    /* Sanity check, implied from the fact that the mappings are not empty */
    CORRADE_INTERNAL_ASSERT(framebuffer.begin() != framebuffer.end());

    _state->file.setFrameBuffer(framebuffer);
    _state->file.readPixels(dataWindow.min.y, dataWindow.max.y);

    return Trade::ImageData2D{format, size, std::move(out)};

/* Good thing there are function try blocks, otherwise I would have to indent
   the whole thing. That would be awful. */
} catch(const Iex::BaseExc& e) {
    /* e.message() is only since 2.3.0, use what() for compatibility */
    Error{} << "Trade::OpenExrImporter::image2D(): import error:" << e.what();
    return {};
}

}}

CORRADE_PLUGIN_REGISTER(OpenExrImporter, Magnum::Trade::OpenExrImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.3")
