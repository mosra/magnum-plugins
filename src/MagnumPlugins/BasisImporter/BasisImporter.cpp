/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019, 2021 Jonathan Hale <squareys@googlemail.com>
    Copyright © 2021 Pablo Escobar <mail@rvrs.in>

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

#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/ConfigurationValue.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/ColorBatch.h>
#include <Magnum/Trade/ImageData.h>

#include <basisu_transcoder.h>

#ifdef MAGNUM_BUILD_DEPRECATED
#include <Magnum/Trade/TextureData.h>
#endif

namespace Magnum { namespace Trade {

using namespace Containers::Literals;

namespace {

/* Map BasisImporter::TargetFormat to (Compressed)PixelFormat. See the
   TargetFormat enum for details. */
PixelFormat pixelFormat(BasisImporter::TargetFormat type, bool isSrgb) {
    switch(type) {
        case BasisImporter::TargetFormat::RGBA8:
            return isSrgb ? PixelFormat::RGBA8Srgb : PixelFormat::RGBA8Unorm;
        case BasisImporter::TargetFormat::RGB16F:
            return PixelFormat::RGB16F;
        case BasisImporter::TargetFormat::RGBA16F:
            return PixelFormat::RGBA16F;
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
        case BasisImporter::TargetFormat::Bc6hRGB:
            return CompressedPixelFormat::Bc6hRGBUfloat;
        case BasisImporter::TargetFormat::Bc7RGBA:
            return isSrgb ? CompressedPixelFormat::Bc7RGBASrgb : CompressedPixelFormat::Bc7RGBAUnorm;
        case BasisImporter::TargetFormat::PvrtcRGB4bpp:
            return isSrgb ? CompressedPixelFormat::PvrtcRGB4bppSrgb : CompressedPixelFormat::PvrtcRGB4bppUnorm;
        case BasisImporter::TargetFormat::PvrtcRGBA4bpp:
            return isSrgb ? CompressedPixelFormat::PvrtcRGBA4bppSrgb : CompressedPixelFormat::PvrtcRGBA4bppUnorm;
        case BasisImporter::TargetFormat::Astc4x4RGBA:
            return isSrgb ? CompressedPixelFormat::Astc4x4RGBASrgb : CompressedPixelFormat::Astc4x4RGBAUnorm;
        case BasisImporter::TargetFormat::Astc4x4RGBAF:
            return CompressedPixelFormat::Astc4x4RGBAF;
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
    "Bc1RGB", "Bc3RGBA", "Bc4R", "Bc5RG", "Bc7RGBA", nullptr, /* BC7_ALT */
    "PvrtcRGB4bpp", "PvrtcRGBA4bpp",
    "Astc4x4RGBA",
    nullptr, nullptr, /* ATC formats */
    "RGBA8", nullptr, nullptr, nullptr, /* RGB565 / BGR565 / RGBA4444 */
    nullptr, nullptr, nullptr,
    "EacR", "EacRG",
    "Bc6hRGB", "Astc4x4RGBAF",
    "RGB16F", "RGBA16F",
};

/* Last element has to be on the same index as last enum value */
static_assert(Containers::arraySize(FormatNames) - 1 == Int(BasisImporter::TargetFormat::RGBA16F), "bad string format mapping");

}

}}

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
    /* Basis 1.16 got rid of global selector palettes */
    #if BASISD_LIB_VERSION < 116
    /* There is only this type of codebook */
    basist::etc1_global_selector_codebook codebook;
    #endif

    /* One transcoder for each supported file type, and of course they have
       wildly different interfaces. ktx2_transcoder is only defined if
       BASISD_SUPPORT_KTX2 != 0, so we need to ifdef around it everywhere
       it's used. */
    Containers::Optional<basist::basisu_transcoder> basisTranscoder;
    #if BASISD_SUPPORT_KTX2
    Containers::Optional<basist::ktx2_transcoder> ktx2Transcoder;
    #endif

    Containers::Array<char> in;

    ImageFlags3D imageFlags;
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

    UnsignedInt lastTranscodedImageId = ~0u;
    Containers::Optional<TargetFormat> lastTargetFormat;
    /* These two are reset to false in doImage() if target format changes
       compared to lastTargetFormat */
    bool noTranscodeFormatWarningPrinted = false;
    bool yFlipNotPossibleWarningPrinted = false;

    explicit State()
    #if BASISD_LIB_VERSION < 116
        : codebook(basist::g_global_selector_cb_size, basist::g_global_selector_cb)
    #endif
    {}
};

void BasisImporter::initialize() {
    basist::basisu_transcoder_init();
}

BasisImporter::BasisImporter(): _state{InPlaceInit} {
    /* Initialize default configuration values */
    /** @todo horrible workaround, fix this properly */
    configuration().setValue("format", "");
}

BasisImporter::BasisImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin): AbstractImporter{manager, plugin} {
    /* Initializes codebook */
    _state.reset(new State);

    /* Set format configuration from plugin alias */
    if(plugin.hasPrefix("BasisImporter"_s)) {
        /* Has type prefix. We can assume the substring results in a valid
           value as the plugin conf limits it to known suffixes */
        if(plugin.size() > 13) configuration().setValue("format", plugin.exceptPrefix("BasisImporter"));
    }
}

BasisImporter::~BasisImporter() = default;

ImporterFeatures BasisImporter::doFeatures() const { return ImporterFeature::OpenData; }

bool BasisImporter::doIsOpened() const {
    /* Both a transcoder and the input data have to be present or both
       have to be empty */
    #if BASISD_SUPPORT_KTX2
    CORRADE_INTERNAL_ASSERT(!(_state->basisTranscoder || _state->ktx2Transcoder) == !_state->in);
    #else
    CORRADE_INTERNAL_ASSERT(!_state->basisTranscoder == !_state->in);
    #endif
    return _state->in;
}

void BasisImporter::doClose() {
    _state->basisTranscoder = Containers::NullOpt;
    #if BASISD_SUPPORT_KTX2
    _state->ktx2Transcoder = Containers::NullOpt;
    #endif
    _state->in = nullptr;
}

void BasisImporter::doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) {
    /* Because here we're copying the data and using the _in to check if file
       is opened, having them nullptr would mean openData() would fail without
       any error message. It's not possible to do this check on the importer
       side, because empty file is valid in some formats (OBJ or glTF). We also
       can't do the full import here because then doImage2D() would need to
       copy the imported data instead anyway (and the uncompressed size is much
       larger). This way it'll also work nicely with a future openMemory(). */
    if(data.isEmpty()) {
        Error{} << "Trade::BasisImporter::openData(): the file is empty";
        return;
    }

    /* Check if this is a KTX2 file. There's basist::g_ktx2_file_identifier but
       that's hidden behind BASISD_SUPPORT_KTX2 so define it ourselves, taken
       from KtxImporter/KtxHeader.h */
    const bool isKTX2 = Containers::StringView{data}.hasPrefix("\xabKTX 20\xbb\r\n\x1a\n"_s);
    #if !BASISD_SUPPORT_KTX2
    /** @todo Can we test this? Maybe disable this on some CI, BC7 is already
        disabled on Emscripten. */
    if(isKTX2) {
        Error{} << "Trade::BasisImporter::openData(): opening a KTX2 file but Basis Universal was compiled without KTX2 support";
        return;
    }
    #endif

    /* Keep the original data, take over the existing array or copy the data if
       we can't. We have to do this first because transcoders may hold on to
       the pointer we pass into init()/start_transcoding(). While
       basis_transcoder currently doesn't keep the pointer around, it might in
       the future and ktx2_transcoder already does. */
    Containers::Pointer<State> state{InPlaceInit};
    if(dataFlags & (DataFlag::Owned|DataFlag::ExternallyOwned)) {
        state->in = Utility::move(data);
    } else {
        state->in = Containers::Array<char>{NoInit, data.size()};
        Utility::copy(data, state->in);
    }

    /* In case of KTX2, if KTXorientation is missing in the file and this
       option isn't set, a warning is printed below to hint at an exporter
       bug (i.e., https://github.com/BinomialLLC/basis_universal/issues/258) */
    const Containers::StringView assumeYUp = configuration().value<Containers::StringView>("assumeYUp");

    #if BASISD_SUPPORT_KTX2
    if(isKTX2) {
        state->ktx2Transcoder.emplace(
            #if BASISD_LIB_VERSION < 116
            &state->codebook
            #endif
        );

        /* init() handles all the validation checks, there's no extra function
           for that */
        if(!state->ktx2Transcoder->init(state->in.data(), state->in.size())) {
            Error{} << "Trade::BasisImporter::openData(): invalid KTX2 header, or not Basis compressed; might want to use KtxImporter instead";
            return;
        }

        /* Check for supercompression and print a useful error if basisu was
           compiled without Zstandard support. Not exposed in ktx2_transcoder,
           get it from the KTX2 header directly. */
        /** @todo Can we test this? Maybe disable this on some CI, BC7 is
            already disabled on Emscripten. */
        #if !BASISD_SUPPORT_KTX2_ZSTD
        if(reinterpret_cast<const basist::ktx2_header*>(state->in.data())->m_supercompression_scheme == basist::KTX2_SS_ZSTANDARD) {
            Error{} << "Trade::BasisImporter::openData(): file uses Zstandard supercompression but Basis Universal was compiled without Zstandard support";
            return;
        }
        #endif

        /* Start transcoding */
        if(!state->ktx2Transcoder->start_transcoding()) {
            Error{} << "Trade::BasisImporter::openData(): bad KTX2 file";
            return;
        }

        /* Save some global file info we need later */

        /* Remember the type for doTexture(). ktx2_transcoder::init() already
           checked we're dealing with a valid 2D texture. basisu -tex_type 3d
           results in 2D array textures, and there's no get_depth() at all. */
        state->isVideo = state->ktx2Transcoder->is_video();
        /* Importing videos with many frames as a single array images would be
           impractical, so mark the texture as array only if it's not a
           video. */
        if(state->ktx2Transcoder->get_layers() > 0 && !state->isVideo)
            state->imageFlags |= ImageFlag3D::Array;
        if(state->ktx2Transcoder->get_faces() != 1)
            state->imageFlags |= ImageFlag3D::CubeMap;

        /* KTX2 files only ever contain one image, but for videos we choose to
           expose layers as multiple images, one for each frame */
        if(state->isVideo) {
            state->numImages = Math::max(state->ktx2Transcoder->get_layers(), 1u);
            state->numSlices = 1;
        } else {
            state->numImages = 1;
            state->numSlices = state->ktx2Transcoder->get_faces()*Math::max(state->ktx2Transcoder->get_layers(), 1u);
        }
        state->numLevels = Containers::Array<UnsignedInt>{DirectInit, state->numImages, state->ktx2Transcoder->get_levels()};

        state->compressionType = state->ktx2Transcoder->get_format();

        /* Get y-flip flag from KTXorientation key/value entry. If it's
           missing, the default is Y-down. Y-up = flipped. In an ideal world,
           we wouldn't need to warn if it's missing, but since Basis still
           doesn't bother writing the correct metadata even after FOUR YEARS,
           the warning is essential to help debugging cases where the input
           data were actually Y-flipped but it's impossible to tell from the
           file: https://github.com/BinomialLLC/basis_universal/issues/258 */
        /** @todo once ImageFlag::YUp/YDown is a thing, label these as
            "neither" instead of assuming Y down */
        const basisu::uint8_vec* orientation = state->ktx2Transcoder->find_key("KTXorientation");
        if(!orientation && !assumeYUp && !(flags() & ImporterFlag::Quiet))
            Warning{} << "Trade::BasisImporter::openData(): missing orientation metadata, assuming Y down. Set the assumeYUp option to suppress this warning.";
        state->isYFlipped = orientation && orientation->size() >= 2 && (*orientation)[1] == 'u';

        state->isSrgb = state->ktx2Transcoder->get_dfd_transfer_func() == basist::KTX2_KHR_DF_TRANSFER_SRGB;
    } else
    #endif
    {
        /* .basis file */
        state->basisTranscoder.emplace(
            #if BASISD_LIB_VERSION < 116
            &state->codebook
            #endif
        );

        if(!state->basisTranscoder->validate_header(state->in.data(), state->in.size())) {
            Error{} << "Trade::BasisImporter::openData(): invalid basis header";
            return;
        }

        /* Start transcoding */
        basist::basisu_file_info fileInfo;
        if(!state->basisTranscoder->get_file_info(state->in.data(), state->in.size(), fileInfo) ||
           !state->basisTranscoder->start_transcoding(state->in.data(), state->in.size())) {
            Error{} << "Trade::BasisImporter::openData(): bad basis file";
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
        state->isVideo = false;
        switch(fileInfo.m_tex_type) {
            case basist::basis_texture_type::cBASISTexTypeVideoFrames:
                /* Decoding all video frames at once is usually not what you
                   want, so we treat videos as independent 2D images. We still
                   have to check that the sizes match, and need to remember
                   this is a video to disallow seeking (not supported by
                   basisu). */
                state->isVideo = true;
                break;
            case basist::basis_texture_type::cBASISTexType2D:
                /* That's the default (empty) ImageFlags */
                break;
            case basist::basis_texture_type::cBASISTexType2DArray:
                state->imageFlags |= ImageFlag3D::Array;
                break;
            case basist::basis_texture_type::cBASISTexTypeCubemapArray:
                state->imageFlags |= ImageFlag3D::CubeMap;
                if(fileInfo.m_total_images > 6)
                    state->imageFlags |= ImageFlag3D::Array;
                break;
            case basist::basis_texture_type::cBASISTexTypeVolume:
                /* Import 3D textures as 2D arrays because:
                   - 3D textures are not supported in KTX2 so this unifies the
                     formats
                   - mip levels are always 2D images for each slice, meaning
                     they wouldn't halve in the z-dimension as users would very
                     likely expect
                   - so why is this type here at all, actually?? */
                if(!(flags() & ImporterFlag::Quiet))
                    Warning{} << "Trade::BasisImporter::openData(): importing 3D texture as a 2D array texture";
                state->imageFlags |= ImageFlag3D::Array;
                break;
            /* This is caught by basis_transcoder::get_file_info() */
            default: CORRADE_INTERNAL_ASSERT_UNREACHABLE(); /* LCOV_EXCL_LINE */
        }

        /* If the texture type is 2D, it can be multi-image. Otherwise, for
           arrays and cube maps the images are actually slices of a 3D image. */
        if(!(state->imageFlags & (ImageFlag3D::Array|ImageFlag3D::CubeMap))) {
            state->numImages = fileInfo.m_total_images;
            state->numSlices = 1;
        } else {
            state->numImages = 1;
            state->numSlices = fileInfo.m_total_images;
        }

        CORRADE_INTERNAL_ASSERT(fileInfo.m_image_mipmap_levels.size() == fileInfo.m_total_images);

        /* Get mip level counts and check that they, as well as resolution, are
           equal for all non-independent images (layers, faces, video frames).
           These checks, including the cube map checks below, are either not
           necessary for the KTX2 file format or are already handled by
           ktx2_transcoder. */
        const bool imageSizeMustMatch = (state->imageFlags & (ImageFlag3D::Array|ImageFlag3D::CubeMap)) || state->isVideo;
        UnsignedInt firstWidth = 0, firstHeight = 0;
        state->numLevels = Containers::Array<UnsignedInt>{NoInit, state->numImages};
        for(UnsignedInt i = 0; i != fileInfo.m_total_images; ++i) {
            /* Header validation etc. is already done in get_file_info and
               start_transcoding, so by looking at the code there's nothing else
               that could fail and wasn't already caught before */
            basist::basisu_image_info imageInfo;
            CORRADE_INTERNAL_ASSERT_OUTPUT(state->basisTranscoder->get_image_info(state->in.data(), state->in.size(), imageInfo, i));
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

            if(imageSizeMustMatch && i > 0 && imageInfo.m_total_levels != state->numLevels[0]) {
                Error{} << "Trade::BasisImporter::openData(): image slices have mismatching level counts, expected"
                    << state->numLevels[0] << "but got" << imageInfo.m_total_levels;
                return;
            }

            if(i < state->numImages)
                state->numLevels[i] = imageInfo.m_total_levels;
        }

        if(state->imageFlags & ImageFlag3D::CubeMap) {
            if(state->numSlices % 6 != 0) {
                Error{} << "Trade::BasisImporter::openData(): cube map face count must be a multiple of 6 but got" << state->numSlices;
                return;
            }

            if(firstWidth != firstHeight) {
                Error{} << "Trade::BasisImporter::openData(): cube map must be square but got"
                    << firstWidth << "by" << firstHeight;
                return;
            }
        }

        state->compressionType = fileInfo.m_tex_format;
        state->isYFlipped = fileInfo.m_y_flipped;

        /* For some reason cBASISHeaderFlagSRGB is not exposed in basisu_file_info,
           get it from the basis header directly */
        const basist::basis_file_header& header = *reinterpret_cast<const basist::basis_file_header*>(state->in.data());
        state->isSrgb = header.m_flags & basist::basis_header_flags::cBASISHeaderFlagSRGB;
    }

    /* Override the Y-flipped status if set in configuration */
    if(assumeYUp)
        state->isYFlipped = configuration().value<bool>("assumeYUp");

    #if BASISD_SUPPORT_KTX2
    /* There has to be exactly one transcoder */
    CORRADE_INTERNAL_ASSERT(!state->ktx2Transcoder != !state->basisTranscoder);
    #endif
    /* There's one image with faces/layers, or multiple images without any */
    CORRADE_INTERNAL_ASSERT(state->numImages == 1 || state->numSlices == 1);

    if(flags() & ImporterFlag::Verbose) {
        if(!state->isYFlipped)
            Debug{} << "Trade::BasisImporter::openData(): image will be flipped along Y";
        if(state->isVideo)
            Debug{} << "Trade::BasisImporter::openData(): file contains video frames, images must be transcoded sequentially";
    }

    _state = Utility::move(state);
}

template<UnsignedInt dimensions> Containers::Optional<ImageData<dimensions>> BasisImporter::doImage(const UnsignedInt id, const UnsignedInt level) {
    static_assert(dimensions >= 2 && dimensions <= 3, "Only 2D and 3D images are supported");
    constexpr const char* prefixes[2]{"Trade::BasisImporter::image2D():", "Trade::BasisImporter::image3D():"};
    constexpr const char* prefix = prefixes[dimensions - 2];

    const auto targetFormatStr = configuration().value<Containers::StringView>("format");
    Containers::Optional<TargetFormat> targetFormat;
    if(targetFormatStr) {
        targetFormat = configuration().value<TargetFormat>("format");
        if(UnsignedInt(*targetFormat) == ~UnsignedInt{}) {
            Error{} << prefix << "invalid transcoding target format" << targetFormatStr << Debug::nospace
                << ", expected to be one of Etc1RGB, Etc2RGBA, EacR, EacRG, Bc1RGB, Bc3RGBA, Bc4R, Bc5RG, Bc6hRGB, Bc7RGBA, Pvrtc1RGB4bpp, Pvrtc1RGBA4bpp, Astc4x4RGBA, Astc4x4RGBAF, RGBA8, RGB16F or RGBA16F";
            return Containers::NullOpt;
        }
    }

    /* If the target format changed from last time, reset the warning counters
       so they're printed again */
    if(_state->lastTargetFormat != targetFormat) {
        _state->lastTargetFormat = targetFormat;
        _state->noTranscodeFormatWarningPrinted = false;
        _state->yFlipNotPossibleWarningPrinted = false;
    }

    #if BASISD_LIB_VERSION >= 150
    const bool isHdr = _state->compressionType == basist::basis_tex_format::cUASTC_HDR_4x4;
    #else
    constexpr bool isHdr = false;
    #endif

    /* If no target format was specified, print a warning and fall back to
       RGBA. Don't save it to _state->lastTargetFormat because then we
       wouldn't be able to distinguish the case of explicit RGBA and no
       format set */
    if(!targetFormat) {
        if(!(flags() & ImporterFlag::Quiet) && !_state->noTranscodeFormatWarningPrinted)
            Warning{} << prefix << "no format to transcode to was specified, falling back to uncompressed" << (isHdr ? "RGBA16F" : "RGBA8") << Debug::nospace << "."
                << "To get rid of this warning, either explicitly set the format option to one of Etc1RGB, Etc2RGBA, EacR, EacRG, Bc1RGB, Bc3RGBA, Bc4R, Bc5RG, Bc6hRGB, Bc7RGBA, Pvrtc1RGB4bpp, Pvrtc1RGBA4bpp, Astc4x4RGBA, Astc4x4RGBAF, RGBA8, RGB16F or RGBA16F, or load the plugin via one of its BasisImporterEtc1RGB, ... aliases.";
        targetFormat = isHdr ? TargetFormat::RGBA16F : TargetFormat::RGBA8;
        _state->noTranscodeFormatWarningPrinted = true;
    }

    const auto format = basist::transcoder_texture_format(Int(*targetFormat));

    #if BASISD_LIB_VERSION >= 150
    const bool wantHdr = basist::basis_transcoder_format_is_hdr(format);

    /* Basis doesn't support transcoding from LDR to HDR or vice versa */
    if(isHdr != wantHdr) {
        Error{} << prefix << "Can't transcode" << (isHdr ? "HDR" : "LDR") << "image to"
            << (wantHdr ? "HDR" : "LDR") << "format" << targetFormatStr;
        return Containers::NullOpt;
    }
    #endif

    /* Some target formats may be unsupported, either because support wasn't
       compiled in or UASTC doesn't support a certain format. All of the
       formats in TargetFormat are transcodable from UASTC so we can provide a
       slightly more useful error message than "impossible!". */
    if(!basist::basis_is_format_supported(format, _state->compressionType)) {
        Error{} << prefix << "Basis Universal was compiled without support for" << targetFormatStr;
        return Containers::NullOpt;
    }

    UnsignedInt origWidth, origHeight, totalBlocks, numFaces;
    bool isIFrame;
    #if BASISD_SUPPORT_KTX2
    if(_state->ktx2Transcoder) {
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
    } else
    #endif
    {
        /* See comment right above */
        basist::basisu_image_level_info levelInfo;
        CORRADE_INTERNAL_ASSERT_OUTPUT(_state->basisTranscoder->get_image_level_info(_state->in.data(), _state->in.size(), levelInfo, id, level));

        origWidth = levelInfo.m_orig_width;
        origHeight = levelInfo.m_orig_height;
        totalBlocks = levelInfo.m_total_blocks;
        isIFrame = levelInfo.m_iframe_flag;

        numFaces = _state->imageFlags & ImageFlag3D::CubeMap ? 6 : 1;
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

    const bool isUncompressed = basist::basis_transcoder_format_is_uncompressed(format);

    const Vector3ui size{origWidth, origHeight, _state->numSlices};
    UnsignedInt rowStride, outputRowsInPixels, outputSizeInBlocksOrPixels;
    if(isUncompressed) {
        rowStride = size.x();
        outputRowsInPixels = size.y();
        outputSizeInBlocksOrPixels = size.x()*size.y();
    } else {
        rowStride = 0; /* left up to Basis to calculate */
        outputRowsInPixels = 0; /* not used for compressed data */
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
            #if BASISD_SUPPORT_KTX2
            if(_state->ktx2Transcoder) {
                const UnsignedInt currentLayer = id + l;
                if(!_state->ktx2Transcoder->transcode_image_level(level, currentLayer, f, dest.data() + offset, outputSizeInBlocksOrPixels, format, 0, rowStride, outputRowsInPixels)) {
                    Error{} << prefix << "transcoding failed";
                    return Containers::NullOpt;
                }
            } else
            #endif
            {
                const UnsignedInt currentId = id + (l*numFaces + f);
                if(!_state->basisTranscoder->transcode_image_level(_state->in.data(), _state->in.size(), currentId, level, dest.data() + offset, outputSizeInBlocksOrPixels, format, 0, rowStride, nullptr, outputRowsInPixels)) {
                    Error{} << prefix << "transcoding failed";
                    return Containers::NullOpt;
                }
            }
        }
    }

    if(isUncompressed) {
        Trade::ImageData<dimensions> out{pixelFormat(*targetFormat, _state->isSrgb), Math::Vector<dimensions, Int>::pad(Vector3i{size}), Utility::move(dest), ImageFlag<dimensions>(UnsignedShort(_state->imageFlags))};

        /* Flip if needed */
        if(!_state->isYFlipped)
            Utility::flipInPlace<dimensions - 2>(out.mutablePixels());

        /* GCC 4.8 needs extra help here */
        return Containers::optional(Utility::move(out));
    } else {
        const CompressedPixelFormat compressedFormat = compressedPixelFormat(*targetFormat, _state->isSrgb);

        /* Flip if needed */
        /** @todo In 2019, I submitted a PR to Basis that would flip the image
            directly in the ETC1S representation:
             https://github.com/BinomialLLC/basis_universal/pull/79
            With that, the whole process would only need to be implemented and
            tested for a single format where it's lossless, not on A DOZEN,
            with some of them making quality tradeoffs. But, as usual with any
            PRs submitted by anybody to this library, the developers just don't
            care at all, so four years later I'm doing it myself on the output
            formats instead. */
        if(!_state->isYFlipped) {
            /** @todo clean this up once blocks() is a thing */
            const Vector3i blockSize = compressedPixelFormatBlockSize(compressedFormat);
            const Vector3i sizeInBlocks = (Vector3i{size} + blockSize - Vector3i{1})/blockSize;
            const UnsignedInt blockDataSize = compressedPixelFormatBlockDataSize(compressedFormat);
            const Containers::StridedArrayView4D<char> blocks{dest, {
                std::size_t(sizeInBlocks.z()),
                std::size_t(sizeInBlocks.y()),
                std::size_t(sizeInBlocks.x()),
                blockDataSize
            }};
            bool flipped = true;
            switch(*targetFormat) {
                case TargetFormat::Bc1RGB:
                    Math::yFlipBc1InPlace(blocks);
                    break;
                case TargetFormat::Bc3RGBA:
                    Math::yFlipBc3InPlace(blocks);
                    break;
                case TargetFormat::Bc4R:
                    Math::yFlipBc4InPlace(blocks);
                    break;
                case TargetFormat::Bc5RG:
                    Math::yFlipBc5InPlace(blocks);
                    break;
                case TargetFormat::Etc1RGB:
                case TargetFormat::Etc2RGBA:
                case TargetFormat::Bc6hRGB:
                case TargetFormat::Bc7RGBA:
                case TargetFormat::PvrtcRGB4bpp:
                case TargetFormat::PvrtcRGBA4bpp:
                case TargetFormat::Astc4x4RGBA:
                case TargetFormat::Astc4x4RGBAF:
                case TargetFormat::EacR:
                case TargetFormat::EacRG:
                    if(!(flags() & ImporterFlag::Quiet) && !_state->yFlipNotPossibleWarningPrinted)
                        Warning{} << prefix << "Y flip is not yet implemented for" << compressedFormat << Debug::nospace << ", imported data will have wrong orientation. Enable assumeYUp to suppress this warning.";
                    _state->yFlipNotPossibleWarningPrinted = true;
                    flipped = false;
                    break;
                /* We'd be in the isUncompressed branch above for this */
                /* LCOV_EXCL_START */
                case TargetFormat::RGBA8:
                case TargetFormat::RGB16F:
                case TargetFormat::RGBA16F:
                    CORRADE_INTERNAL_ASSERT_UNREACHABLE();
                /* LCOV_EXCL_STOP */
            }

            if(flipped && !(flags() & ImporterFlag::Quiet) && size.y() % blockSize.y() != 0)
                Warning{} << prefix << "Y-flipping a compressed image that's not whole blocks, the result will be shifted by" << (blockSize.y() - (size.y() % blockSize.y())) << "pixels";
        }

        return Trade::ImageData<dimensions>{compressedFormat, Math::Vector<dimensions, Int>::pad(Vector3i{size}), Utility::move(dest), ImageFlag<dimensions>(UnsignedShort(_state->imageFlags))};
    }
}

UnsignedInt BasisImporter::doImage2DCount() const {
    return _state->imageFlags & (ImageFlag3D::Array|ImageFlag3D::CubeMap) ? 0 : _state->numImages;
}

UnsignedInt BasisImporter::doImage2DLevelCount(const UnsignedInt id) {
    return _state->numLevels[id];
}

Containers::Optional<ImageData2D> BasisImporter::doImage2D(const UnsignedInt id, const UnsignedInt level) {
    return doImage<2>(id, level);
}

UnsignedInt BasisImporter::doImage3DCount() const {
    return _state->imageFlags & (ImageFlag3D::Array|ImageFlag3D::CubeMap) ? _state->numImages : 0;
}

UnsignedInt BasisImporter::doImage3DLevelCount(const UnsignedInt id) {
    return _state->numLevels[id];
}

Containers::Optional<ImageData3D> BasisImporter::doImage3D(const UnsignedInt id, const UnsignedInt level) {
    return doImage<3>(id, level);
}

#ifdef MAGNUM_BUILD_DEPRECATED
UnsignedInt BasisImporter::doTextureCount() const {
    return _state->numImages;
}

Containers::Optional<TextureData> BasisImporter::doTexture(UnsignedInt id) {
    TextureType type;
    if(_state->imageFlags >= (ImageFlag3D::CubeMap|ImageFlag3D::Array))
        type = TextureType::CubeMapArray;
    else if(_state->imageFlags & ImageFlag3D::CubeMap)
        type = TextureType::CubeMap;
    else if(_state->imageFlags & ImageFlag3D::Array)
        type = TextureType::Texture2DArray;
    else
        type = TextureType::Texture2D;

    return TextureData{type, SamplerFilter::Linear, SamplerFilter::Linear,
        SamplerMipmap::Linear, SamplerWrapping::Repeat, id};
}
#endif

void BasisImporter::setTargetFormat(TargetFormat format) {
    configuration().setValue("format", format);
}

BasisImporter::TargetFormat BasisImporter::targetFormat() const {
    return configuration().value<TargetFormat>("format");
}

}}

CORRADE_PLUGIN_REGISTER(BasisImporter, Magnum::Trade::BasisImporter,
    MAGNUM_TRADE_ABSTRACTIMPORTER_PLUGIN_INTERFACE)
