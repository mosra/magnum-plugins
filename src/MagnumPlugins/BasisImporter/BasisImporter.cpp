/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019 Jonathan Hale <squareys@googlemail.com>

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
    FROM, OUT OF OR IN CONNETCION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include "BasisImporter.h"

#include <basisu_transcoder.h>

#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/ConfigurationValue.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/String.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

namespace Magnum { namespace Trade { namespace {

/* Map BasisImporter::TargetFormat to CompressedPixelFormat. See the
   TargetFormat enum for details. */
CompressedPixelFormat textureFormat(BasisImporter::TargetFormat type) {
    switch(type) {
        case BasisImporter::TargetFormat::Etc1:
            return CompressedPixelFormat::Etc2RGB8Unorm;
        case BasisImporter::TargetFormat::Etc2:
            return CompressedPixelFormat::Etc2RGBA8Unorm;
        case BasisImporter::TargetFormat::Bc1:
            return CompressedPixelFormat::Bc1RGBUnorm;
        case BasisImporter::TargetFormat::Bc3:
            return CompressedPixelFormat::Bc3RGBAUnorm;
        case BasisImporter::TargetFormat::Bc4:
            return CompressedPixelFormat::Bc4RUnorm;
        case BasisImporter::TargetFormat::Bc5:
            return CompressedPixelFormat::Bc5RGUnorm;
        case BasisImporter::TargetFormat::Bc7M6OpaqueOnly:
            return CompressedPixelFormat::Bc7RGBAUnorm;
        case BasisImporter::TargetFormat::Pvrtc1_4OpaqueOnly:
            return CompressedPixelFormat::PvrtcRGB4bppUnorm;
        default: CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }
}

constexpr const char* FormatNames[]{
    "Etc1", "Etc2", "Bc1", "Bc3", "Bc4", "Bc5",
    "Bc7M6OpaqueOnly", "Pvrtc1_4OpaqueOnly"
};

constexpr basist::transcoder_texture_format TranscoderTextureFormat[] = {
    basist::transcoder_texture_format::cTFETC1,
    basist::transcoder_texture_format::cTFETC2,
    basist::transcoder_texture_format::cTFBC1,
    basist::transcoder_texture_format::cTFBC3,
    basist::transcoder_texture_format::cTFBC4,
    basist::transcoder_texture_format::cTFBC5,
    basist::transcoder_texture_format::cTFBC7_M6_OPAQUE_ONLY,
    basist::transcoder_texture_format::cTFPVRTC1_4_OPAQUE_ONLY
};

}}}

namespace Corrade { namespace Utility {

/* Configuration value implementation for BasisImporter::TargetFormat */
template<> struct ConfigurationValue<Magnum::Trade::BasisImporter::TargetFormat> {
    static std::string toString(Magnum::Trade::BasisImporter::TargetFormat value, ConfigurationValueFlags) {
        #define _c(value) case Magnum::Trade::BasisImporter::TargetFormat::value: return #value;
        switch(value) {
            _c(Etc1)
            _c(Etc2)
            _c(Bc1)
            _c(Bc3)
            _c(Bc4)
            _c(Bc5)
            _c(Bc7M6OpaqueOnly)
            _c(Pvrtc1_4OpaqueOnly)
        }
        #undef _c

        return "<invalid>";
    }

    static Magnum::Trade::BasisImporter::TargetFormat fromString(const std::string& value, ConfigurationValueFlags) {
        Magnum::Int i = 0;
        for(const char* name: Magnum::Trade::FormatNames) {
            if(value == name) return Magnum::Trade::BasisImporter::TargetFormat(i);
            ++i;
        }

        return Magnum::Trade::BasisImporter::TargetFormat(-1);
    }
};

}}

namespace Magnum { namespace Trade {

struct BasisImporter::State {
    /* There is only this type of codebook */
    basist::etc1_global_selector_codebook codebook;
    basist::basisu_transcoder transcoder{&codebook};

    Containers::Array<unsigned char> in;
    basist::basisu_file_info fileInfo;

    explicit State(): codebook(basist::g_global_selector_cb_size,
        basist::g_global_selector_cb) {}
};

void BasisImporter::initialize() {
    basist::basisu_transcoder_init();
}

BasisImporter::BasisImporter() = default;

BasisImporter::BasisImporter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImporter{manager, plugin} {
    /* Initializes codebook */
    _state.reset(new State);

    /* Set format configuration from plugin alias */
    if(Corrade::Utility::String::beginsWith(plugin, "BasisImporter")) {
        /* Has type prefix. We can assume the substring results in a valid
           value as the plugin conf limits it to known suffixes */
        if(plugin.size() > 13) configuration().setValue("format", plugin.substr(13));
    }
}

BasisImporter::~BasisImporter() = default;

auto BasisImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool BasisImporter::doIsOpened() const { return _state->in; }

void BasisImporter::doClose() { _state->in = nullptr; }

void BasisImporter::doOpenData(const Containers::ArrayView<const char> data) {
    /* Because here we're copying the data and using the _in to check if file
       is opened, having them nullptr would mean openData() would fail without
       any error message. It's not possible to do this check on the importer
       side, because empty file is valid in some formats (OBJ or glTF). We also
       can't do the full import here because then doImage2D() would need to
       copy the imported data instead anyway (and the uncompressed size is much
       larger). This way it'll also work nicely with a future openMemory(). */
    if(data.empty()) {
        Error{} << "Trade::BasisImporter::openData(): the file is empty";
        return;
    }

    if(!_state->transcoder.validate_header(data.data(), data.size())) {
        Error() << "Trade::BasisImporter::openData(): invalid header";
        return;
    }

    /* Save the global file info to avoid calling that again each time we check
       for image count and whatnot; start transcoding */
    if(!_state->transcoder.get_file_info(data.data(), data.size(), _state->fileInfo) ||
       !_state->transcoder.start_transcoding(data.data(), data.size())) {
        Error() << "Trade::BasisImporter::openData(): bad basis file";
        return;
    }

    _state->in = Containers::Array<unsigned char>(data.size());
    std::copy(data.begin(), data.end(), _state->in.begin());
}

UnsignedInt BasisImporter::doImage2DCount() const {
    return _state->fileInfo.m_total_images;
}

Containers::Optional<ImageData2D> BasisImporter::doImage2D(UnsignedInt index) {
    const UnsignedInt level = 0;

    const std::string targetFormatStr = configuration().value<std::string>("format");
    if(targetFormatStr.empty()) {
        Error() << "Trade::BasisImporter::image2D(): no format to transcode to was specified. Either load the plugin via one of its BasisImporterEtc1, ... aliases, or set the format explicitly via plugin configuration.";
        return Containers::NullOpt;
    }

    const auto targetFormat = configuration().value<TargetFormat>("format");
    if(Int(targetFormat) == -1) {
        Error() << "Trade::BasisImporter::image2D(): invalid transcoding target format"
            << targetFormatStr.c_str() << Debug::nospace << ", expected to be one of Etc1, Etc2, Bc1, Bc3, Bc4, Bc5, Bc7M6OpaqueOnly, Pvrtc1_4OpaqueOnly";
        return Containers::NullOpt;
    }
    const basist::transcoder_texture_format format =
        TranscoderTextureFormat[Int(targetFormat)];

    basist::basisu_image_info info;
    /* Header validation etc. is already done in doOpenData() and index is
       bounds-checked against doImage2DCount() by AbstractImporter, so by
       looking at the code there's nothing else that could fail and wasn't
       already caught before. That means we also can't craft any file to cover
       an error path, so turning this into an assert. When this blows up for
       someome, we'd most probably need to harden doOpenData() to catch that,
       not turning this into a graceful error. */
    CORRADE_INTERNAL_ASSERT_OUTPUT(_state->transcoder.get_image_info(_state->in.data(), _state->in.size(), info, index));

    UnsignedInt origWidth, origHeight, totalBlocks;
    /* Same as above, it checks for state we already verified before. If this
       blows up for someone, we can reconsider. */
    CORRADE_INTERNAL_ASSERT_OUTPUT(_state->transcoder.get_image_level_desc(_state->in.data(), _state->in.size(), index, level, origWidth, origHeight, totalBlocks));

    const UnsignedInt bytesPerBlock = basis_get_bytes_per_block(format);
    const UnsignedInt requiredSize = totalBlocks*bytesPerBlock;
    Containers::Array<char> dest{Containers::DefaultInit, requiredSize};
    if(!_state->transcoder.transcode_image_level(_state->in.data(), _state->in.size(), index, level, dest.data(), dest.size()/bytesPerBlock, basist::transcoder_texture_format(format))) {
        Error{} << "Trade::BasisImporter::image2D(): transcoding failed";
        return Containers::NullOpt;
    }

    return Trade::ImageData2D(textureFormat(targetFormat),
        {Int(origWidth), Int(origHeight)}, std::move(dest));
}

void BasisImporter::setTargetFormat(TargetFormat format) {
    configuration().setValue("format", format);
}

BasisImporter::TargetFormat BasisImporter::targetFormat() const {
    return configuration().value<TargetFormat>("format");
}

}}

CORRADE_PLUGIN_REGISTER(BasisImporter, Magnum::Trade::BasisImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3")
