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

/* Map BasisTranscodingType to Magnum::CompressedPixelFormat */
CompressedPixelFormat textureFormat(BasisTranscodingType type, bool hasAlpha) {
    switch(type) {
        case BasisTranscodingType::Etc1:
        case BasisTranscodingType::Etc2:
            return (hasAlpha) ? CompressedPixelFormat::Etc2RGBA8Unorm
                : CompressedPixelFormat::Etc2RGB8Unorm;
        case BasisTranscodingType::Bc1:
            return (hasAlpha) ? CompressedPixelFormat::Bc1RGBAUnorm
                : CompressedPixelFormat::Bc1RGBUnorm;
        case BasisTranscodingType::Bc3:
            return CompressedPixelFormat::Bc3RGBAUnorm;
        case BasisTranscodingType::Bc4:
            return CompressedPixelFormat::Bc4RUnorm;
        case BasisTranscodingType::Bc5:
            return CompressedPixelFormat::Bc5RGUnorm;
        case BasisTranscodingType::Bc7M6OpaqueOnly:
            return CompressedPixelFormat::Bc7RGBAUnorm;
        case BasisTranscodingType::Pvrtc1_4OpaqueOnly:
            return CompressedPixelFormat::PvrtcRGB4bppUnorm;
        default:
            CORRADE_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }
}

const char* FormatNames[]{
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

/* Configuration value implementation for BasisTranscodingType */
template<> struct ConfigurationValue<Magnum::Trade::BasisTranscodingType> {

    static std::string toString(
        const Magnum::Trade::BasisTranscodingType& value,
        ConfigurationValueFlags)
    {
        #define _c(value) case Magnum::Trade::BasisTranscodingType::value: return #value;
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

    static Magnum::Trade::BasisTranscodingType fromString(
        const std::string& value, ConfigurationValueFlags)
    {
        Magnum::Int i = 0;
        for(const char* name: Magnum::Trade::FormatNames) {
            if(value == name) return Magnum::Trade::BasisTranscodingType(i);
            ++i;
        }

        return Magnum::Trade::BasisTranscodingType(-1);
    }
};

}}

namespace Magnum { namespace Trade {

struct BasisImporter::State {
    /* There is only this type of codebook */
    basist::etc1_global_selector_codebook _codebook;
    basist::basisu_transcoder _transcoder{&_codebook};

    explicit State(): _codebook(basist::g_global_selector_cb_size,
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

        /* Has type prefix */
        if(plugin.length() > 13) {
            /* We can assume the substring results in a valid value
               as the plugin conf limits it to known suffixes */
            configuration().setValue("format", plugin.substr(13));
        }
    }
}

BasisImporter::~BasisImporter() = default;

auto BasisImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool BasisImporter::doIsOpened() const { return _in; }

void BasisImporter::doClose() { _in = nullptr; }

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

    if(!_state->_transcoder.validate_header(data.data(), data.size())) {
        Error() << "Trade::BasisImporter::openData(): invalid header";
        return;
    }

    if(!_state->_transcoder.start_transcoding(data.data(), data.size())) {
        Error() << "Trade::BasisImporter::openData(): bad basis file";
        return;
    }

    _in = Containers::Array<unsigned char>(data.size());
    std::copy(data.begin(), data.end(), _in.begin());
}

UnsignedInt BasisImporter::doImage2DCount() const {
    return _state->_transcoder.get_total_images(_in.data(), _in.size());
}

Containers::Optional<ImageData2D> BasisImporter::doImage2D(UnsignedInt index) {
    const UnsignedInt level = 0;

    const std::string targetFormatStr = configuration().value<std::string>("format");
    if(targetFormatStr.empty()) {
        Error() << "Trade::BasisImporter::image2D(): no format to transcode "
            "to was specified. Either load the plugin via one of its BasisImporterEtc1, ... "
            "aliases, or set the format explicitly via plugin configuration.";
        return Containers::NullOpt;
    }

    const BasisTranscodingType targetFormat =
        configuration().value<BasisTranscodingType>("format");
    if(Int(targetFormat) == -1) {
        Error() << "Trade::BasisImporter::image2D(): invalid transcoding target format"
            << targetFormatStr.c_str() << Debug::nospace << ", expected to be one of:"
            << "Etc1, Etc2, Bc1, Bc3, Bc4, Bc5, Bc7M6OpaqueOnly, Pvrtc1_4OpaqueOnly";
        return Containers::NullOpt;
    }
    const basist::transcoder_texture_format format =
        TranscoderTextureFormat[Int(targetFormat)];
    const UnsignedInt bytes_per_block = basis_get_bytes_per_block(format);

    basist::basisu_image_info info;
    if (!_state->_transcoder.get_image_info(_in.data(), _in.size(), info, index)) {
        Error{} << "Trade::BasisImporter::image2D(): unable to get image info";
        return Containers::NullOpt;
    }
    const bool hasAlpha = info.m_alpha_flag;

    UnsignedInt origWidth, origHeight, totalBlocks;
    if (!_state->_transcoder.get_image_level_desc(_in.data(), _in.size(), index,
            level, origWidth, origHeight, totalBlocks))
    {
        Error{} << "Trade::BasisImporter::image2D(): unable to retrieve mip level description";
        return Containers::NullOpt;
    }

    const UnsignedInt requiredSize = totalBlocks*bytes_per_block;
    Containers::Array<char> dest{Containers::DefaultInit, requiredSize};
    const UnsignedInt status = _state->_transcoder.transcode_image_level(
      _in.data(), _in.size(), index, level, dest.data(), dest.size()/bytes_per_block,
      static_cast<basist::transcoder_texture_format>(format));
    if(!status) {
        Error{} << "Trade::BasisImporter::image2D(): transcoding failed";
        return Containers::NullOpt;
    }

    return Trade::ImageData2D(textureFormat(targetFormat, hasAlpha),
        Vector2i{Int(origWidth), Int(origHeight)},
        std::move(dest));
}

void BasisImporter::setTargetFormat(BasisTranscodingType format) {
    configuration().setValue("format", format);
}

BasisTranscodingType BasisImporter::targetFormat() const {
    return configuration().value<BasisTranscodingType>("format");
}

}}

CORRADE_PLUGIN_REGISTER(BasisImporter, Magnum::Trade::BasisImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3")
