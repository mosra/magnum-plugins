/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019, 2021 Jonathan Hale <squareys@googlemail.com>

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

#include <cstring>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/ConfigurationValue.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/String.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/Trade/TextureData.h>

#include <basisu_transcoder.h>

namespace Magnum { namespace Trade { namespace {

/* Map BasisImporter::TargetFormat to (Compressed)PixelFormat. See the
   TargetFormat enum for details. */
PixelFormat pixelFormat(BasisImporter::TargetFormat type, bool isSrgb) {
    switch(type) {
        case BasisImporter::TargetFormat::RGBA8:
            return isSrgb ? PixelFormat::RGBA8Srgb : PixelFormat::RGBA8Unorm;
        default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }
}

CompressedPixelFormat compressedPixelFormat(BasisImporter::TargetFormat type, bool isSrgb) {
    switch(type) {
        case BasisImporter::TargetFormat::Etc1RGB:
            return isSrgb ? CompressedPixelFormat::Etc2RGB8Srgb : CompressedPixelFormat::Etc2RGB8Unorm;
        case BasisImporter::TargetFormat::Etc2RGBA:
            return isSrgb ? CompressedPixelFormat::Etc2RGBA8Srgb : CompressedPixelFormat::Etc2RGBA8Unorm;
        case BasisImporter::TargetFormat::Bc1RGB:
            return isSrgb ? CompressedPixelFormat::Bc1RGBSrgb : CompressedPixelFormat::Bc1RGBUnorm;
        case BasisImporter::TargetFormat::Bc3RGBA:
            return isSrgb ? CompressedPixelFormat::Bc3RGBASrgb : CompressedPixelFormat::Bc3RGBAUnorm;
        /** @todo use bc7/bc4/bc5 based on channel count? needs a bit from
            https://github.com/BinomialLLC/basis_universal/issues/66 */
        case BasisImporter::TargetFormat::Bc4R:
            return CompressedPixelFormat::Bc4RUnorm;
        case BasisImporter::TargetFormat::Bc5RG:
            return CompressedPixelFormat::Bc5RGUnorm;
        case BasisImporter::TargetFormat::Bc7RGB:
            return isSrgb ? CompressedPixelFormat::Bc7RGBASrgb : CompressedPixelFormat::Bc7RGBAUnorm;
        case BasisImporter::TargetFormat::Bc7RGBA:
            return isSrgb ? CompressedPixelFormat::Bc7RGBASrgb : CompressedPixelFormat::Bc7RGBAUnorm;
        case BasisImporter::TargetFormat::PvrtcRGB4bpp:
            return isSrgb ? CompressedPixelFormat::PvrtcRGB4bppSrgb : CompressedPixelFormat::PvrtcRGB4bppUnorm;
        case BasisImporter::TargetFormat::PvrtcRGBA4bpp:
            return isSrgb ? CompressedPixelFormat::PvrtcRGBA4bppSrgb : CompressedPixelFormat::PvrtcRGBA4bppUnorm;
        case BasisImporter::TargetFormat::Astc4x4RGBA:
            return isSrgb ? CompressedPixelFormat::Astc4x4RGBASrgb : CompressedPixelFormat::Astc4x4RGBAUnorm;
        /** @todo use etc2/eacR/eacRG based on channel count? needs a bit from
            https://github.com/BinomialLLC/basis_universal/issues/66 */
        case BasisImporter::TargetFormat::EacR:
            return CompressedPixelFormat::EacR11Unorm;
        case BasisImporter::TargetFormat::EacRG:
            return CompressedPixelFormat::EacRG11Unorm;
        default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
    }
}

constexpr const char* FormatNames[]{
    "Etc1RGB", "Etc2RGBA",
    "Bc1RGB", "Bc3RGBA", "Bc4R", "Bc5RG", "Bc7RGB", "Bc7RGBA",
    "PvrtcRGB4bpp", "PvrtcRGBA4bpp",
    "Astc4x4RGBA",
    nullptr, nullptr, /* ATC formats */
    "RGBA8", nullptr, nullptr, nullptr, /* RGB565 / BGR565 / RGBA4444 */
    nullptr, nullptr, nullptr,
    "EacR", "EacRG"
};

/* Last element has to be on the same index as last enum value */
static_assert(Containers::arraySize(FormatNames) - 1 == Int(BasisImporter::TargetFormat::EacRG), "bad string format mapping");

}}}

namespace Corrade { namespace Utility {

/* Configuration value implementation for BasisImporter::TargetFormat */
template<> struct ConfigurationValue<Magnum::Trade::BasisImporter::TargetFormat> {
    static std::string toString(Magnum::Trade::BasisImporter::TargetFormat value, ConfigurationValueFlags) {
        if(Magnum::UnsignedInt(value) < Containers::arraySize(Magnum::Trade::FormatNames))
            return Magnum::Trade::FormatNames[Magnum::UnsignedInt(value)];

        return "<invalid>";
    }

    static Magnum::Trade::BasisImporter::TargetFormat fromString(const std::string& value, ConfigurationValueFlags) {
        Magnum::Int i = 0;
        for(const char* name: Magnum::Trade::FormatNames) {
            if(name && value == name) return Magnum::Trade::BasisImporter::TargetFormat(i);
            ++i;
        }

        return Magnum::Trade::BasisImporter::TargetFormat(~Magnum::UnsignedInt{});
    }
};

}}

namespace Magnum { namespace Trade {

struct BasisImporter::State {
    /* There is only this type of codebook */
    basist::etc1_global_selector_codebook codebook;

    Containers::Optional<basist::basisu_transcoder> basisTranscoder;
    #if BASISD_SUPPORT_KTX2
    Containers::Optional<basist::ktx2_transcoder> ktx2Transcoder;
    #else
    Containers::Optional<void*> ktx2Transcoder;
    #endif

    Containers::Array<char> in;

    TextureType textureType;
    /* Only > 1 for plain 2D .basis files with multiple images. Anything else
       (cube/array/volume) contains a single logical image. KTX2 only supports
       one image to begin with. */
    UnsignedInt numImages;
    /* Number of faces/layers/z-slices in the third image dimension, 1 if 2D */
    UnsignedInt numSlices;
    Containers::Array<UnsignedInt> numLevels;

    basist::basis_tex_format compressionType;
    bool isVideo;
    bool isYFlipped;
    bool isSrgb;

    bool noTranscodeFormatWarningPrinted = false;
    UnsignedInt lastTranscodedImageId = ~0u;

    explicit State(): codebook(basist::g_global_selector_cb_size,
        basist::g_global_selector_cb) {}
};

void BasisImporter::initialize() {
    basist::basisu_transcoder_init();
}

BasisImporter::BasisImporter(): _state{InPlaceInit} {
    /* Initialize default configuration values */
    /** @todo horrible workaround, fix this properly */
    configuration().setValue("format", "");
}

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

ImporterFeatures BasisImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool BasisImporter::doIsOpened() const {
    /* Both a transcoder and the input data have to be present or both
       have to be empty */
    CORRADE_INTERNAL_ASSERT(!(_state->basisTranscoder || _state->ktx2Transcoder) == !_state->in);
    return _state->in;
}

void BasisImporter::doClose() {
    _state->basisTranscoder = Containers::NullOpt;
    _state->ktx2Transcoder = Containers::NullOpt;
    _state->in = nullptr;
}

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

    /* Check if this is a KTX2 file. There's basist::g_ktx2_file_identifier but
       that's hidden behind BASISD_SUPPORT_KTX2 so define it ourselves, taken
       from KtxImporter/KtxHeader.h */
    constexpr char KtxFileIdentifier[12]{'\xab', 'K', 'T', 'X', ' ', '2', '0', '\xbb', '\r', '\n', '\x1a', '\n'};
    const bool isKTX2 = data.size() >= sizeof(KtxFileIdentifier) &&
        std::memcmp(data.begin(), KtxFileIdentifier, sizeof(KtxFileIdentifier)) == 0;

    /* Keep a copy of the data. We have to do this first because transcoders
       may hold on to the pointer we pass into init(). While basis_transcoder
       currently doesn't keep the pointer around, it might in the future and
       ktx2_transcoder already does. */
    _state->in = Containers::Array<char>{NoInit, data.size()};
    Utility::copy(data, _state->in);

    Containers::ScopeGuard resourceGuard{_state.get(), [](BasisImporter::State* state) {
        state->basisTranscoder = Containers::NullOpt;
        state->ktx2Transcoder = Containers::NullOpt;
        state->in = nullptr;
    }};

    if(isKTX2) {
        if(!basist::basisu_transcoder_supports_ktx2()) {
            Error() << "Trade::BasisImporter::openData(): opening a KTX2 file but Basis Universal was compiled without KTX2 support";
            return;
        }

        #if BASISD_SUPPORT_KTX2
        _state->ktx2Transcoder.emplace(&_state->codebook);

        /* Init handles all the validation checks, there's no extra function
           for that */
        if(!_state->ktx2Transcoder->init(_state->in.data(), _state->in.size())) {
            Error{} << "Trade::BasisImporter::openData(): invalid KTX2 header, or not Basis compressed";
            return;
        }

        /* Check for supercompression and print a useful error if basisu was
           compiled without Zstandard support. Not exposed in ktx2_transcoder,
           get it from the KTX2 header directly. */
        const basist::ktx2_header& header = *reinterpret_cast<const basist::ktx2_header*>(_state->in.data());
        if(header.m_supercompression_scheme == basist::KTX2_SS_ZSTANDARD && !basist::basisu_transcoder_supports_ktx2_zstd()) {
            Error() << "Trade::BasisImporter::openData(): file uses Zstandard supercompression but Basis Universal was compiled without Zstandard support";
            return;
        }

        /* Start transcoding */
        if(!_state->ktx2Transcoder->start_transcoding()) {
            Error() << "Trade::BasisImporter::openData(): bad KTX2 file";
            return;
        }

        /* Save some global file info we need later */

        /* Remember the type for doTexture(). ktx2_transcoder::init() already
           checked we're dealing with a valid 2D texture. */
        _state->isVideo = false;
        if(_state->ktx2Transcoder->get_faces() != 1)
            _state->textureType = _state->ktx2Transcoder->get_layers() > 0 ? TextureType::CubeMapArray : TextureType::CubeMap;
        else if(_state->ktx2Transcoder->is_video()) {
            _state->textureType = TextureType::Texture2D;
            _state->isVideo = true;
        } else
            _state->textureType = _state->ktx2Transcoder->get_layers() > 0 ? TextureType::Texture2DArray : TextureType::Texture2D;

        /* KTX2 files only ever contain one image, but for videos we choose to
           expose layers as multiple images, one for each frame */
        if(_state->isVideo) {
            _state->numImages = Math::max(_state->ktx2Transcoder->get_layers(), 1u);
            _state->numSlices = 1;
        } else {
            _state->numImages = 1;
            _state->numSlices = _state->ktx2Transcoder->get_faces()*Math::max(_state->ktx2Transcoder->get_layers(), 1u);
        }
        _state->numLevels = Containers::Array<UnsignedInt>{DirectInit, _state->numImages, _state->ktx2Transcoder->get_levels()};

        _state->compressionType = _state->ktx2Transcoder->get_format();

        /* Get y-flip flag from KTXorientation key/value entry. If it's
           missing, the default is Y-down. Y-up = flipped. */
        const basisu::uint8_vec* orientation = _state->ktx2Transcoder->find_key("KTXorientation");
        _state->isYFlipped = orientation && orientation->size() >= 2 && (*orientation)[1] == 'u';

        _state->isSrgb = _state->ktx2Transcoder->get_dfd_transfer_func() == basist::KTX2_KHR_DF_TRANSFER_SRGB;
        #endif
    } else {
        _state->basisTranscoder.emplace(&_state->codebook);

        if(!_state->basisTranscoder->validate_header(_state->in.data(), _state->in.size())) {
            Error() << "Trade::BasisImporter::openData(): invalid basis header";
            return;
        }

        /* Start transcoding */
        basist::basisu_file_info fileInfo;
        if(!_state->basisTranscoder->get_file_info(_state->in.data(), _state->in.size(), fileInfo) ||
           !_state->basisTranscoder->start_transcoding(_state->in.data(), _state->in.size())) {
            Error() << "Trade::BasisImporter::openData(): bad basis file";
            return;
        }

        /* Save some global file info we need later. We can't save fileInfo
           directly because that's specific to .basis files, and there's no
           equivalent in ktx2_transcoder. */

        /* This is checked by basis_transcoder::start_transcoding() */
        CORRADE_INTERNAL_ASSERT(fileInfo.m_total_images > 0);

        /* Remember the type for doTexture(). Depending on the type, we're
           either dealing with multiple independent 2D images or each image is
           an array layer, cubemap face or depth slice. */
        _state->isVideo = false;
        switch(fileInfo.m_tex_type) {
            case basist::basis_texture_type::cBASISTexTypeVideoFrames:
                /* Decoding all video frames at once is usually not what you
                   want, so we treat videos as independent 2D images. We still
                   have to check that the sizes match, and need to remember
                   this is a video to disallow seeking (not supported by
                   basisu). */
                _state->isVideo = true;
                CORRADE_FALLTHROUGH
            case basist::basis_texture_type::cBASISTexType2D:
                _state->textureType = TextureType::Texture2D;
                break;
            case basist::basis_texture_type::cBASISTexType2DArray:
                _state->textureType = TextureType::Texture2DArray;
                break;
            case basist::basis_texture_type::cBASISTexTypeCubemapArray:
                _state->textureType = fileInfo.m_total_images > 6 ? TextureType::CubeMapArray : TextureType::CubeMap;
                break;
            case basist::basis_texture_type::cBASISTexTypeVolume:
                _state->textureType = TextureType::Texture3D;
                break;
            default:
                /* This is caught by basis_transcoder::get_file_info() */
                CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }

        if(_state->textureType == TextureType::Texture2D) {
            _state->numImages = fileInfo.m_total_images;
            _state->numSlices = 1;
        } else {
            _state->numImages = 1;
            _state->numSlices = fileInfo.m_total_images;
        }

        CORRADE_INTERNAL_ASSERT(fileInfo.m_image_mipmap_levels.size() == fileInfo.m_total_images);

        /* Get mip level counts and check that they, as well as resolution, are
           equal for all non-independent images (layers, faces, video frames).
           These checks, including the cube map checks below, are either not
           necessary for the KTX2 file format or are already handled by
           ktx2_transcoder. */
        const bool imageSizeMustMatch = _state->textureType != TextureType::Texture2D || _state->isVideo;
        basist::basisu_image_info imageInfo;
        UnsignedInt firstWidth = 0, firstHeight = 0;
        _state->numLevels = Containers::Array<UnsignedInt>{NoInit, _state->numImages};
        for(UnsignedInt i = 0; i != fileInfo.m_total_images; ++i) {
            /* Header validation etc. is already done in get_file_info and
               start_transcoding, so by looking at the code there's nothing else
               that could fail and wasn't already caught before */
            CORRADE_INTERNAL_ASSERT_OUTPUT(_state->basisTranscoder->get_image_info(_state->in.data(), _state->in.size(), imageInfo, i));
            if(i == 0) {
                firstWidth = imageInfo.m_orig_width;
                firstHeight = imageInfo.m_orig_height;
            }

            if(imageSizeMustMatch && (imageInfo.m_orig_width != firstWidth || imageInfo.m_orig_height != firstHeight)) {
                Error{} << "Trade::BasisImporter::openData(): image slices have mismatching size, expected"
                    << firstWidth << "by" << firstHeight << "but got"
                    << imageInfo.m_orig_width << "by" << imageInfo.m_orig_height;
                return;
            }

            if(imageSizeMustMatch && i > 0 && imageInfo.m_total_levels != _state->numLevels[0]) {
                Error{} << "Trade::BasisImporter::openData(): image slices have mismatching level counts, expected"
                    << _state->numLevels[0] << "but got" << imageInfo.m_total_levels;
                return;
            }

            if(i < _state->numImages)
                _state->numLevels[i] = imageInfo.m_total_levels;
        }

        /* Mip levels in basis are per 2D image, so for 3D images they don't
           halve in the z-dimension. Turn it into a 2D array texture so users
           don't get surprised by the mip z-size not changing, and print a
           warning. */
        if(_state->textureType == TextureType::Texture3D && _state->numLevels[0] > 1) {
            Warning{} << "Trade::BasisImporter::openData(): found a 3D image with 2D mipmaps, importing as a 2D array texture";
            _state->textureType = TextureType::Texture2DArray;
        }

        if(_state->textureType == TextureType::CubeMap || _state->textureType == TextureType::CubeMapArray) {
            if(_state->numSlices % 6 != 0) {
                Error{} << "Trade::BasisImporter::openData(): cube map face count must be a multiple of 6 but got" << _state->numSlices;
                return;
            }

            if(firstWidth != firstHeight) {
                Error{} << "Trade::BasisImporter::openData(): cube map must be square but got"
                    << firstWidth << "by" << firstHeight;
                return;
            }
        }

        _state->compressionType = fileInfo.m_tex_format;
        _state->isYFlipped = fileInfo.m_y_flipped;

        /* For some reason cBASISHeaderFlagSRGB is not exposed in basisu_file_info,
           get it from the basis header directly */
        const basist::basis_file_header& header = *reinterpret_cast<const basist::basis_file_header*>(_state->in.data());
        _state->isSrgb = header.m_flags & basist::basis_header_flags::cBASISHeaderFlagSRGB;
    }

    /* There has to be exactly one transcoder */
    CORRADE_INTERNAL_ASSERT(!_state->ktx2Transcoder != !_state->basisTranscoder);
    /* Can't have a KTX2 transcoder without KTX2 support compiled into basisu */
    CORRADE_INTERNAL_ASSERT(BASISD_SUPPORT_KTX2 || !_state->ktx2Transcoder);
    /* These file formats don't support 1D images */
    CORRADE_INTERNAL_ASSERT(_state->textureType != TextureType::Texture1D && _state->textureType != TextureType::Texture1DArray);
    /* There's one image with faces/layers, or multiple images without any */
    CORRADE_INTERNAL_ASSERT(_state->numImages == 1 || _state->numSlices == 1);

    /* All good, release the resource guard */
    resourceGuard.release();

    if(flags() & ImporterFlag::Verbose) {
        if(_state->isVideo)
            Debug{} << "Trade::BasisImporter::openData(): file contains video frames, images must be transcoded sequentially";
    }
}

template<UnsignedInt dimensions>
Containers::Optional<ImageData<dimensions>> BasisImporter::doImage(const UnsignedInt id, const UnsignedInt level) {
    static_assert(dimensions >= 2 && dimensions <= 3, "Only 2D and 3D images are supported");
    constexpr const char* prefixes[2]{"Trade::BasisImporter::image2D():", "Trade::BasisImporter::image3D():"};
    constexpr const char* prefix = prefixes[dimensions - 2];

    const std::string targetFormatStr = configuration().value<std::string>("format");
    TargetFormat targetFormat;
    if(targetFormatStr.empty()) {
        if(!_state->noTranscodeFormatWarningPrinted)
            Warning{} << prefix << "no format to transcode to was specified, falling back to uncompressed RGBA8. To get rid of this warning either load the plugin via one of its BasisImporterEtc1RGB, ... aliases, or explicitly set the format option in plugin configuration.";
        targetFormat = TargetFormat::RGBA8;
        _state->noTranscodeFormatWarningPrinted = true;
    } else {
        targetFormat = configuration().value<TargetFormat>("format");
        if(UnsignedInt(targetFormat) == ~UnsignedInt{}) {
            Error() << prefix << "invalid transcoding target format" << targetFormatStr << Debug::nospace
                << ", expected to be one of EacR, EacRG, Etc1RGB, Etc2RGBA, Bc1RGB, Bc3RGBA, Bc4R, Bc5RG, Bc7RGB, Bc7RGBA, Pvrtc1RGB4bpp, Pvrtc1RGBA4bpp, Astc4x4RGBA, RGBA8";
            return Containers::NullOpt;
        }
    }

    const auto format = basist::transcoder_texture_format(Int(targetFormat));
    const bool isUncompressed = basist::basis_transcoder_format_is_uncompressed(format);

    /* Some target formats may be unsupported, either because support wasn't
       compiled in or UASTC doesn't support a certain format.
       None of the formats in TargetFormat are currently affected by UASTC. */
    if(!basist::basis_is_format_supported(format, _state->compressionType)) {
        /** @todo Mention that some formats may not be compiled in? Since it's
            really the only way to trigger this error. */
        Error{} << prefix << "unsupported transcoding target format" << targetFormatStr << "for a"
            << (_state->compressionType == basist::basis_tex_format::cUASTC4x4 ? "UASTC" : "ETC1S") << "image";
        return Containers::NullOpt;
    }

    /** @todo Don't decode to PVRTC if width/height (not origWidth/origHeight?)
        is not a power-of-two:
        https://github.com/BinomialLLC/basis_universal/blob/77b7df8e5df3532a42ef3c76de0c14cc005d0f65/basisu_tool.cpp#L1458 */

    UnsignedInt origWidth, origHeight, totalBlocks, numFaces;
    bool isIFrame;
    if(_state->ktx2Transcoder) {
        #if BASISD_SUPPORT_KTX2
        basist::ktx2_image_level_info levelInfo;
        /* Header validation etc. is already done in doOpenData() and id is
           bounds-checked against doImage2DCount() by AbstractImporter, so by
           looking at the code there's nothing else that could fail and wasn't
           already caught before. That means we also can't craft any file to
           cover an error path, so turning this into an assert. When this blows
           up for someome, we'd most probably need to harden doOpenData() to
           catch that, not turning this into a graceful error.

           For independent images and videos we use the correct layer. For
           images as slices, they're all the same size, (checked in
           doOpenData()) and isIFrame is not used, so any layer or face works. */
        CORRADE_INTERNAL_ASSERT_OUTPUT(_state->ktx2Transcoder->get_image_level_info(levelInfo, level, id, 0));

        origWidth = levelInfo.m_orig_width;
        origHeight = levelInfo.m_orig_height;
        totalBlocks = levelInfo.m_total_blocks;
        /* m_iframe_flag is always false for UASTC video:
           https://github.com/BinomialLLC/basis_universal/issues/259
           However, it's safe to assume the first frame is always an I-frame. */
        isIFrame = levelInfo.m_iframe_flag || id == 0;

        numFaces = _state->ktx2Transcoder->get_faces();
        #endif
    } else {
        /* See comment right above */
        basist::basisu_image_level_info levelInfo;
        CORRADE_INTERNAL_ASSERT_OUTPUT(_state->basisTranscoder->get_image_level_info(_state->in.data(), _state->in.size(), levelInfo, id, level));

        origWidth = levelInfo.m_orig_width;
        origHeight = levelInfo.m_orig_height;
        totalBlocks = levelInfo.m_total_blocks;
        isIFrame = levelInfo.m_iframe_flag;

        numFaces = (_state->textureType == TextureType::CubeMap || _state->textureType == TextureType::CubeMapArray) ? 6 : 1;
    }

    const UnsignedInt numLayers = _state->numSlices/numFaces;

    /* basisu doesn't allow seeking to arbitrary video frames. If this isn't an
       I-frame, only allow transcoding the frame following the last P-frame. */
    if(_state->isVideo) {
        const UnsignedInt expectedImageId = _state->lastTranscodedImageId + 1;
        if(!isIFrame && id != expectedImageId) {
            Error{} << prefix << "video frames must be transcoded sequentially, expected frame"
                << expectedImageId << (expectedImageId == 0 ? "but got" : "or 0 but got") << id;
            return Containers::NullOpt;
        }
        _state->lastTranscodedImageId = id;
    }

    /* No flags used by transcode_image_level() by default */
    const std::uint32_t flags = 0;
    if(!_state->isYFlipped) {
        /** @todo replace with the flag once the PR is submitted */
        Warning{} << "Trade::BasisImporter::image2D(): the image was not encoded Y-flipped, imported data will have wrong orientation";
        //flags |= basist::basisu_transcoder::cDecodeFlagsFlipY;
    }

    const Vector3ui size{origWidth, origHeight, _state->numSlices};
    UnsignedInt rowStride, outputRowsInPixels, outputSizeInBlocksOrPixels;
    if(isUncompressed) {
        rowStride = size.x();
        outputRowsInPixels = size.y();
        outputSizeInBlocksOrPixels = size.x()*size.y();
    } else {
        rowStride = 0; /* left up to Basis to calculate */
        outputRowsInPixels = 0; /* not used for compressed data */
        /** @todo Make sure this is correct for layer/cube/depth textures */
        outputSizeInBlocksOrPixels = totalBlocks;
    }

    const UnsignedInt sliceSize = basis_get_bytes_per_block_or_pixel(format)*outputSizeInBlocksOrPixels;
    const UnsignedInt dataSize = sliceSize*size.z();
    Containers::Array<char> dest{DefaultInit, dataSize};

    /* There's no function for transcoding the entire level, so loop over all
       layers and faces and transcode each one. This matches the image layout
       imported by KtxImporter, ie. all faces +X through -Z for the first
       layer, then all faces of the second layer, etc.

       If the user is requesting id > 0, there can't be any layers or faces,
       this is already asserted in doOpenData(). This allows us to calculate
       the layer (KTX2) or image id to transcode with a simple addition. */
    for(UnsignedInt l = 0; l != numLayers; ++l) {
        for(UnsignedInt f = 0; f != numFaces; ++f) {
            const UnsignedInt offset = (l*numFaces + f)*sliceSize;
            if(_state->ktx2Transcoder) {
                #if BASISD_SUPPORT_KTX2
                const UnsignedInt currentLayer = id + l;
                if(!_state->ktx2Transcoder->transcode_image_level(level, currentLayer, f, dest.data() + offset, outputSizeInBlocksOrPixels, format, flags, rowStride, outputRowsInPixels)) {
                    Error{} << "Trade::BasisImporter::image2D(): transcoding failed";
                    return Containers::NullOpt;
                }
                #endif
            } else {
                const UnsignedInt currentId = id + (l*numFaces + f);
                if(!_state->basisTranscoder->transcode_image_level(_state->in.data(), _state->in.size(), currentId, level, dest.data() + offset, outputSizeInBlocksOrPixels, format, flags, rowStride, nullptr, outputRowsInPixels)) {
                    Error{} << "Trade::BasisImporter::image2D(): transcoding failed";
                    return Containers::NullOpt;
                }
            }
        }
    }

    if(isUncompressed)
        return Trade::ImageData<dimensions>{pixelFormat(targetFormat, _state->isSrgb), Math::Vector<dimensions, Int>::pad(Vector3i{size}), std::move(dest)};
    else
        return Trade::ImageData<dimensions>{compressedPixelFormat(targetFormat, _state->isSrgb), Math::Vector<dimensions, Int>::pad(Vector3i{size}), std::move(dest)};
}

UnsignedInt BasisImporter::doImage2DCount() const {
    return _state->textureType == TextureType::Texture2D ? _state->numImages : 0;
}

UnsignedInt BasisImporter::doImage2DLevelCount(const UnsignedInt id) {
    return _state->numLevels[id];
}

Containers::Optional<ImageData2D> BasisImporter::doImage2D(const UnsignedInt id, const UnsignedInt level) {
    return doImage<2>(id, level);
}

UnsignedInt BasisImporter::doImage3DCount() const {
    return _state->textureType != TextureType::Texture2D ? _state->numImages : 0;
}

UnsignedInt BasisImporter::doImage3DLevelCount(const UnsignedInt id) {
    return _state->numLevels[id];
}

Containers::Optional<ImageData3D> BasisImporter::doImage3D(const UnsignedInt id, const UnsignedInt level) {
    return doImage<3>(id, level);
}

UnsignedInt BasisImporter::doTextureCount() const {
    return _state->numImages;
}

Containers::Optional<TextureData> BasisImporter::doTexture(UnsignedInt id) {
    return TextureData{_state->textureType, SamplerFilter::Linear, SamplerFilter::Linear,
        SamplerMipmap::Linear, SamplerWrapping::Repeat, id};
}

void BasisImporter::setTargetFormat(TargetFormat format) {
    configuration().setValue("format", format);
}

BasisImporter::TargetFormat BasisImporter::targetFormat() const {
    return configuration().value<TargetFormat>("format");
}

}}

CORRADE_PLUGIN_REGISTER(BasisImporter, Magnum::Trade::BasisImporter,
    "cz.mosra.magnum.Trade.AbstractImporter/0.3.3")
