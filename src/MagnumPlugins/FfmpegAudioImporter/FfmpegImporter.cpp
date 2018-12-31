/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
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

#include "FfmpegImporter.h"

#include <algorithm>
#include <Corrade/Containers/ScopedExit.h>
#include <Corrade/Utility/Assert.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/Math/Functions.h>


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#if __clang__
#include <iostream>
void printAudioFrameInfo(const AVCodecContext* codecContext, const AVFrame* frame)
{
    // See the following to know what data type (unsigned char, short, float, etc) to use to access the audio data:
    // http://ffmpeg.org/doxygen/trunk/samplefmt_8h.html#af9a51ca15301871723577c730b5865c5
    std::cout << "Audio frame info:\n"
              << "  Sample count: " << frame->nb_samples << '\n'
              << "  Channel count: " << codecContext->channels << '\n'
              << "  Format: " << av_get_sample_fmt_name(codecContext->sample_fmt) << '\n'
              << "  Bytes per sample: " << av_get_bytes_per_sample(codecContext->sample_fmt) << '\n'
              << "  Is planar? " << av_sample_fmt_is_planar(codecContext->sample_fmt) << '\n';


    std::cout << "frame->linesize[0] tells you the size (in bytes) of each plane\n";

    if (codecContext->channels > AV_NUM_DATA_POINTERS && av_sample_fmt_is_planar(codecContext->sample_fmt))
    {
        std::cout << "The audio stream (and its frames) have too many channels to fit in\n"
                  << "frame->data. Therefore, to access the audio data, you need to use\n"
                  << "frame->extended_data to access the audio data. It's planar, so\n"
                  << "each channel is in a different element. That is:\n"
                  << "  frame->extended_data[0] has the data for channel 1\n"
                  << "  frame->extended_data[1] has the data for channel 2\n"
                  << "  etc.\n";
    }
    else
    {
        std::cout << "Either the audio data is not planar, or there is enough room in\n"
                  << "frame->data to store all the channels, so you can either use\n"
                  << "frame->data or frame->extended_data to access the audio data (they\n"
                  << "should just point to the same data).\n";
    }

    std::cout << "If the frame is planar, each channel is in a different element.\n"
              << "That is:\n"
              << "  frame->data[0]/frame->extended_data[0] has the data for channel 1\n"
              << "  frame->data[1]/frame->extended_data[1] has the data for channel 2\n"
              << "  etc.\n";

    std::cout << "If the frame is packed (not planar), then all the data is in\n"
              << "frame->data[0]/frame->extended_data[0] (kind of like how some\n"
              << "image formats have RGB pixels packed together, rather than storing\n"
              << " the red, green, and blue channels separately in different arrays.\n";
}

// #if 0
int main()
{
    // Initialize FFmpeg
    av_register_all(); // TODO: not needed at all in v4(?)

    AVFrame* frame = avcodec_alloc_frame();
    if (!frame)
    {
        std::cout << "Error allocating the frame" << std::endl;
        return 1;
    }

    // you can change the file name "01 Push Me to the Floor.wav" to whatever the file is you're reading, like "myFile.ogg" or
    // "someFile.webm" and this should still work
    AVFormatContext* formatContext = NULL;
    if (avformat_open_input(&formatContext, "../CarBots Marines vs. Zerglings-WKvX3a2J86s.mp4", NULL, NULL) != 0)
    {
        av_free(frame);
        std::cout << "Error opening the file" << std::endl;
        return 1;
    }

    if (avformat_find_stream_info(formatContext, NULL) < 0)
    {
        av_free(frame);
        avformat_close_input(&formatContext);
        std::cout << "Error finding the stream info" << std::endl;
        return 1;
    }

    // Find the audio stream
    AVCodec* cdc = nullptr;
    int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &cdc, 0);
    if (streamIndex < 0)
    {
        av_free(frame);
        avformat_close_input(&formatContext);
        std::cout << "Could not find any audio stream in the file" << std::endl;
        return 1;
    }

    AVStream* audioStream = formatContext->streams[streamIndex];
    auto a = audioStream->codecpar->codec_id;
    AVCodecContext* codecContext = audioStream->codec; // TODO: should use codecpar (since ?!) but *how*
    codecContext->codec = cdc;

    if (avcodec_open2(codecContext, codecContext->codec, NULL) != 0)
    {
        av_free(frame);
        avformat_close_input(&formatContext);
        std::cout << "Couldn't open the context with the decoder" << std::endl;
        return 1;
    }

    std::cout << "This stream has " << codecContext->channels << " channels and a sample rate of " << codecContext->sample_rate << "Hz" << std::endl;
    std::cout << "The data is in the format " << av_get_sample_fmt_name(codecContext->sample_fmt) << std::endl;

    AVPacket readingPacket;
    av_init_packet(&readingPacket);

    // Read the packets in a loop
    while (av_read_frame(formatContext, &readingPacket) == 0)
    {
        if (readingPacket.stream_index == audioStream->index)
        {
            AVPacket decodingPacket = readingPacket;

            // Audio packets can have multiple audio frames in a single packet
            while (decodingPacket.size > 0)
            {
                // Try to decode the packet into a frame
                // Some frames rely on multiple packets, so we have to make sure the frame is finished before
                // we can use it
                int gotFrame = 0;
                int result = avcodec_decode_audio4(codecContext, frame, &gotFrame, &decodingPacket); // TODO: deprecated since 4.0, see https://ffmpeg.org/doxygen/4.1/group__lavc__decoding.html#gaaa1fbe477c04455cdc7a994090100db4

                if (result >= 0 && gotFrame)
                {
                    decodingPacket.size -= result;
                    decodingPacket.data += result;

                    // We now have a fully decoded audio frame
                    printAudioFrameInfo(codecContext, frame);
                }
                else
                {
                    decodingPacket.size = 0;
                    decodingPacket.data = nullptr;
                }
            }
        }

        // You *must* call av_free_packet() after each call to av_read_frame() or else you'll leak memory
        av_free_packet(&readingPacket); // TODO: av_packet_unref since 3.1(?)
    }

    // Some codecs will cause frames to be buffered up in the decoding process. If the CODEC_CAP_DELAY flag
    // is set, there can be buffered up frames that need to be flushed, so we'll do that
    if (codecContext->codec->capabilities & CODEC_CAP_DELAY)
    {
        av_init_packet(&readingPacket);
        // Decode all the remaining frames in the buffer, until the end is reached
        int gotFrame = 0;
        while (avcodec_decode_audio4(codecContext, frame, &gotFrame, &readingPacket) >= 0 && gotFrame)
        {
            // We now have a fully decoded audio frame
            printAudioFrameInfo(codecContext, frame);
        }
    }

    // Clean up!
    av_free(frame);
    avcodec_close(codecContext);
    avformat_close_input(&formatContext);
}
#endif

namespace Magnum { namespace Audio {

FfmpegImporter::FfmpegImporter() = default;

FfmpegImporter::FfmpegImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {}

auto FfmpegImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool FfmpegImporter::doIsOpened() const { return _data; }

void FfmpegImporter::doOpenData(Containers::ArrayView<const char> data) {
    /* https://stackoverflow.com/a/20610535 */
    /* http://www.ffmpeg.org/doxygen/trunk/doc_2examples_2avio_reading_8c-example.html */

    /* Fumble the bumbles */
    /* Why the HELL do I need to allocate to read a file from memory?! WHY THE
       DAMN HELL IS THIS SO FUCKING OVERENGINEERED?! FUCK!!! */
    constexpr std::size_t AvioBufferSize = 4096;
    std::uint8_t* avioBuffer = reinterpret_cast<unsigned char*>(av_malloc(AvioBufferSize));
    struct AvioData {
        Containers::ArrayView<const char> view;
        std::size_t seek;
    } avioData{data, 0};
    auto read = [](void* userPtr, std::uint8_t* buffer, int size) {
        auto& avioData = *static_cast<AvioData*>(userPtr);
        const std::size_t end = Math::min(avioData.seek + size, avioData.view.size());
        int count = end - avioData.seek;
        std::copy(avioData.view + avioData.seek, avioData.view + end, buffer);
        avioData.seek = end;

        /* End-of-file has to be returned as a special value instead of just
           "zero bytes read", otherwise it causes
           "Invalid return value 0 for stream protocol" to be printed */
        return count ? count : AVERROR_EOF;
    };
    AVIOContext* const io = avio_alloc_context(avioBuffer, AvioBufferSize, 0, &avioData, read, nullptr, nullptr);
    CORRADE_INTERNAL_ASSERT(io);
    Containers::ScopedExit ioBufferFree{&io->buffer, av_freep};

    /* Fringle the pringles */
    AVFormatContext* formatContext = avformat_alloc_context();
    CORRADE_INTERNAL_ASSERT(formatContext);
    Containers::ScopedExit formatContextClose{&formatContext, avformat_close_input};
    formatContext->pb = io;
    if(const int ret = avformat_open_input(&formatContext, nullptr, nullptr, nullptr)) {
        Error{} << "Audio::FfmpegImporter::openData(): could not open the file:" << /*av_err2str*/(ret);
        return;
    }

    /* Rustle the rumbles */
    /** @todo why the fuck am i doing this why the damn hell, it doesn't return any fucking thing. ALSO HOW CAN THIS FAIL?! */
    if(const int ret = avformat_find_stream_info(formatContext, nullptr)) {
        Error{} << "Audio::FfmpegImporter::openData(): could not find stream info:" << /*av_err2str*/(ret);
        return;
    }

    /* Fizzle the bizzle */
    AVCodec* codec;
    const int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if(streamIndex < 0) {
        Error{} << "Audio::FfmpegImporter::openData(): could not find any audio stream in the file";
        return;
    }

    /* Light my fire */
    AVStream* audioStream = formatContext->streams[streamIndex];
    /** @todo should be codecpar (since when?!) but how?! */
    AVCodecContext* codecContext = audioStream->codec;
    codecContext->codec = codec;
    if(const int ret = avcodec_open2(codecContext, codecContext->codec, nullptr) != 0) {
        Error{} << "Audio::FfmpegImporter::openData(): could not open context with chosen decoder";
        return;
    }

    /* FLIGHT MY RIFLE */
    _frequency = codecContext->sample_rate;
    _format = {};
    const Int channelCount = codecContext->channels;
//     const Int bytesPerSample = codecContext->sample_fmt;
    if(codecContext->sample_fmt == AV_SAMPLE_FMT_U8 ||
       codecContext->sample_fmt == AV_SAMPLE_FMT_U8P) {
        if(channelCount == 1)
            _format = BufferFormat::Mono8;
        else if(channelCount == 2)
            _format = BufferFormat::Stereo8;
        else if(channelCount == 4)
            _format = BufferFormat::Quad8;
        else if(channelCount == 6)
            _format = BufferFormat::Surround51Channel8;
        else if(channelCount == 7)
            _format = BufferFormat::Surround61Channel8;
        else if(channelCount == 8)
            _format = BufferFormat::Surround71Channel8;
    } else if(codecContext->sample_fmt == AV_SAMPLE_FMT_S16 ||
              codecContext->sample_fmt == AV_SAMPLE_FMT_S16P) {
        if(channelCount == 1)
            _format = BufferFormat::Mono16;
        else if(channelCount == 2)
            _format = BufferFormat::Stereo16;
        else if(channelCount == 4)
            _format = BufferFormat::Quad16;
        else if(channelCount == 6)
            _format = BufferFormat::Surround51Channel16;
        else if(channelCount == 7)
            _format = BufferFormat::Surround61Channel16;
        else if(channelCount == 8)
            _format = BufferFormat::Surround71Channel16;
    } else if(codecContext->sample_fmt == AV_SAMPLE_FMT_FLT ||
              codecContext->sample_fmt == AV_SAMPLE_FMT_FLTP) {
        if(channelCount == 1)
            _format = BufferFormat::MonoFloat;
        else if(channelCount == 2)
            _format = BufferFormat::StereoFloat;
    } else if(codecContext->sample_fmt == AV_SAMPLE_FMT_DBL ||
              codecContext->sample_fmt == AV_SAMPLE_FMT_DBLP) {
        if(channelCount == 1)
            _format = BufferFormat::MonoDouble;
        else if(channelCount == 2)
            _format = BufferFormat::StereoDouble;
    }
    if(!ALenum(_format)) {
        Error() << "Audio::FfmpegImporter::openData(): unsupported format"
            << av_get_sample_fmt_name(codecContext->sample_fmt) << "with"
            << channelCount << "channels";
        return;
    }

    Debug{} << channelCount << _frequency << av_get_sample_fmt_name(codecContext->sample_fmt);

    /* Foom the boomble */
    AVPacket readingPacket;
    av_init_packet(&readingPacket);
    AVFrame* frame = av_frame_alloc(); // was avcodec_alloc_frame() before
    CORRADE_INTERNAL_ASSERT(frame);
    const std::size_t sampleSize = av_get_bytes_per_sample(codecContext->sample_fmt);
    std::vector<char> decoded;
    /* Otherwise we would need to look into extra_data. AV_NUM_DATA_POINTERS is
       8 on my machine and max supported config is 7.1, so it should fit. */
    CORRADE_INTERNAL_ASSERT(codecContext->channels <= AV_NUM_DATA_POINTERS);
    while(av_read_frame(formatContext, &readingPacket) == 0) {
        if(readingPacket.stream_index == audioStream->index) { // TODO: Y not streamIndex?
            AVPacket decodingPacket = readingPacket;

            /* Audio packets can have multiple audio frames in a single packet,
               OTOH one frame can be also split across multiple frames */
            while(decodingPacket.size > 0) {
                int frameDone = 0;
                const int result = avcodec_decode_audio4(codecContext, frame, &frameDone, &decodingPacket); // TODO: deprecated since 4.0, see https://ffmpeg.org/doxygen/4.1/group__lavc__decoding.html#gaaa1fbe477c04455cdc7a994090100db4

                if(result >= 0 && frameDone) {
                    decodingPacket.size -= result;
                    decodingPacket.data += result;

                    // We now have a fully decoded audio frame

                    const std::size_t pos = decoded.size();
                    decoded.resize(decoded.size() + frame->nb_samples*sampleSize);
//                     printAudioFrameInfo(codecContext, frame);

                    /* Data for each channel are at a separate location,
                       interleave them back */
                    if(av_sample_fmt_is_planar(codecContext->sample_fmt) && channelCount > 1) {
                        for(std::size_t sample = 0; sample != std::size_t(frame->nb_samples); ++sample) {
                            for(std::size_t channel = 0; channel != std::size_t(channelCount); ++channel) {
                                for(std::size_t i = 0; i != sampleSize; ++i)
                                    decoded[pos + sample*sampleSize + i] = frame->data[channel][sample*sampleSize + i];
                            }
                        }

//                         av_get_bytes_per_sample(codecContext->sample_fmt)

                    /* Otherwise just memcpy the whole thing */
                    } else std::copy_n(frame->data[0], frame->nb_samples*sampleSize, &decoded[pos]);

                } else {
                    decodingPacket.size = 0;
                    decodingPacket.data = nullptr;
                }
            }
        }

        // You *must* call av_free_packet() after each call to av_read_frame() or else you'll leak memory
        av_free_packet(&readingPacket); // TODO: av_packet_unref since 3.1(?)
    }

    /* All good, copy the data to an array */
    _data = Containers::Array<char>{decoded.size()};
    std::copy(decoded.begin(), decoded.end(), _data.begin());

#if 0
    Int numChannels, frequency;
    Short* decodedData = nullptr;

    Int samples = stb_vorbis_decode_memory(reinterpret_cast<const UnsignedByte*>(data.data()), data.size(), &numChannels, &frequency, &decodedData);

    if(samples == -1) {
        Error() << "Audio::FfmpegImporter::openData(): the file signature is invalid";
        return;
    } else if (samples == -2) {
        /* memory allocation failure */
        Error() << "Audio::FfmpegImporter::openData(): out of memory";
        return;
    }

    Containers::Array<char> tempData{reinterpret_cast<char*>(decodedData), size_t(samples*numChannels*2),
        [](char* data, size_t) { std::free(data); }};
    _frequency = frequency;

    /** @todo Floating-point formats */
    if(numChannels == 1)
        _format = BufferFormat::Mono16;
    else if(numChannels == 2)
        _format = BufferFormat::Stereo16;
    else if(numChannels == 4)
        _format = BufferFormat::Quad16;
    else if(numChannels == 6)
        _format = BufferFormat::Surround51Channel16;
    else if(numChannels == 7)
        _format = BufferFormat::Surround61Channel16;
    else if(numChannels == 8)
        _format = BufferFormat::Surround71Channel16;
    else {
        Error() << "Audio::FfmpegImporter::openData(): unsupported channel count"
                << numChannels << "with" << 16 << "bits per sample";
        return;
    }

    _data = std::move(tempData);
#endif

    return;
}

void FfmpegImporter::doClose() { _data = nullptr; }

BufferFormat FfmpegImporter::doFormat() const { return _format; }

UnsignedInt FfmpegImporter::doFrequency() const { return _frequency; }

Containers::Array<char> FfmpegImporter::doData() {
    Containers::Array<char> copy(_data.size());
    std::copy(_data.begin(), _data.end(), copy.begin());
    return copy;
}

}}

CORRADE_PLUGIN_REGISTER(StbVorbisAudioImporter, Magnum::Audio::FfmpegImporter,
    "cz.mosra.magnum.Audio.AbstractImporter/0.1")
