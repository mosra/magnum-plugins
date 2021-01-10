/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include "BasisImageConverter.h"

#include <cstring>
#include <algorithm>
#include <thread>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Swizzle.h>
#include <Magnum/PixelFormat.h>
#include <basisu_enc.h>
#include <basisu_comp.h>
#include <basisu_file_headers.h>

namespace Magnum { namespace Trade {

BasisImageConverter::BasisImageConverter() = default;

BasisImageConverter::BasisImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImageConverter{manager, plugin} {}

ImageConverterFeatures BasisImageConverter::doFeatures() const { return ImageConverterFeature::ConvertData; }

Containers::Array<char> BasisImageConverter::doExportToData(const ImageView2D& image) {
    /* Check input */
    if(image.format() != PixelFormat::RGB8Unorm &&
       image.format() != PixelFormat::RGBA8Unorm &&
       image.format() != PixelFormat::RG8Unorm &&
       image.format() != PixelFormat::R8Unorm)
    {
        Error{} << "Trade::BasisImageConverter::exportToData(): unsupported format" << image.format();
        return {};
    }

    /* To retain sanity, keep this in the same order and grouping as in the
       conf file */
    basisu::basis_compressor_params params;
    #define PARAM_CONFIG(name, type) params.m_##name = configuration().value<type>(#name)
    #define PARAM_CONFIG_FIX_NAME(name, type, fixed) params.m_##name = configuration().value<type>(fixed)
    /* Options */
    PARAM_CONFIG(quality_level, int);
    PARAM_CONFIG(perceptual, bool);
    PARAM_CONFIG(debug, bool);
    PARAM_CONFIG(debug_images, bool);
    PARAM_CONFIG(compute_stats, bool);
    PARAM_CONFIG(compression_level, int);

    /* More options */
    PARAM_CONFIG(max_endpoint_clusters, int);
    PARAM_CONFIG(max_selector_clusters, int);
    PARAM_CONFIG(y_flip, bool);
    PARAM_CONFIG(check_for_alpha, bool);
    PARAM_CONFIG(force_alpha, bool);
    PARAM_CONFIG_FIX_NAME(seperate_rg_to_color_alpha, bool, "separate_rg_to_color_alpha");

    UnsignedInt threadCount = configuration().value<Int>("threads");
    if(threadCount == 0) threadCount = std::thread::hardware_concurrency();
    const bool multithreading = threadCount > 1;
    params.m_multithreading = multithreading;
    basisu::job_pool jpool{threadCount};
    params.m_pJob_pool = &jpool;

    PARAM_CONFIG(disable_hierarchical_endpoint_codebooks, bool);

    /* Mipmap generation options */
    PARAM_CONFIG(mip_gen, bool);
    PARAM_CONFIG(mip_srgb, bool);
    PARAM_CONFIG(mip_scale, float);
    PARAM_CONFIG(mip_filter, std::string);
    PARAM_CONFIG(mip_renormalize, bool);
    PARAM_CONFIG(mip_wrapping, bool);
    PARAM_CONFIG(mip_smallest_dimension, int);

    /* Backend endpoint/selector RDO codec options */
    PARAM_CONFIG(no_selector_rdo, bool);
    PARAM_CONFIG_FIX_NAME(selector_rdo_thresh, float, "selector_rdo_threshold");
    PARAM_CONFIG(no_endpoint_rdo, bool);
    PARAM_CONFIG_FIX_NAME(endpoint_rdo_thresh, float, "endpoint_rdo_threshold");

    /* Hierarchical virtual selector codebook options */
    PARAM_CONFIG_FIX_NAME(global_sel_pal, bool, "global_selector_palette");
    PARAM_CONFIG_FIX_NAME(auto_global_sel_pal, bool, "auto_global_selector_palette");
    PARAM_CONFIG_FIX_NAME(no_hybrid_sel_cb, bool, "no_hybrid_selector_codebook");
    PARAM_CONFIG_FIX_NAME(global_pal_bits, int, "global_palette_bits");
    PARAM_CONFIG_FIX_NAME(global_mod_bits, int, "global_modifier_bits");
    PARAM_CONFIG_FIX_NAME(hybrid_sel_cb_quality_thresh, float, "hybrid_sel_codebook_quality_threshold");

    /* Set various fields in the Basis file header */
    PARAM_CONFIG(userdata0, int);
    PARAM_CONFIG(userdata1, int);
    #undef PARAM_CONFIG
    #undef PARAM_CONFIG_FIX_NAME

    /* If these are enabled, the library reads PNGs from a filesystem and then
       writes basis files there also. DO NOT WANT. */
    params.m_read_source_images = false;
    params.m_write_output_basis_files = false;

    basist::etc1_global_selector_codebook sel_codebook(basist::g_global_selector_cb_size, basist::g_global_selector_cb);
    params.m_pSel_codebook = &sel_codebook;

    if(image.size().x() <= 0 || image.size().y() <= 0) {
        Error() << "Trade::BasisImageConverter::exportToData(): source image is empty";
        return {};
    }

    if(!image.data()) {
        Error() << "Trade::BasisImageConverter::exportToData(): source image data is nullptr";
        return {};
    }

    /* Copy image data into the basis image. There is no way to construct a
       basis image from existing data as it is based on a std::vector, moreover
       we need to tightly pack it and flip Y. The `dst` is an Y-flipped view
       already to make the following loops simpler. */
    params.m_source_images.emplace_back(image.size().x(), image.size().y());
    auto dst = Containers::arrayCast<Color4ub>(Containers::StridedArrayView2D<basisu::color_rgba>({params.m_source_images.back().get_ptr(), params.m_source_images.back().get_total_pixels()}, {std::size_t(image.size().y()), std::size_t(image.size().x())})).flipped<0>();

    /* basis image is always RGBA, fill in alpha if necessary */
    if(image.format() == PixelFormat::RGBA8Unorm) {
        auto src = image.pixels<Math::Vector4<UnsignedByte>>();
        for(std::size_t y = 0; y != src.size()[0]; ++y)
            for(std::size_t x = 0; x != src.size()[1]; ++x)
                dst[y][x] = src[y][x];

    } else if(image.format() == PixelFormat::RGB8Unorm) {
        auto src = image.pixels<Math::Vector3<UnsignedByte>>();
        for(std::size_t y = 0; y != src.size()[0]; ++y)
            for(std::size_t x = 0; x != src.size()[1]; ++x)
                dst[y][x] = src[y][x]; /* Alpha implicitly 255 */

    } else if(image.format() == PixelFormat::RG8Unorm) {
        auto src = image.pixels<Math::Vector2<UnsignedByte>>();
        for(std::size_t y = 0; y != src.size()[0]; ++y)
            for(std::size_t x = 0; x != src.size()[1]; ++x)
                dst[y][x] = Math::gather<'r', 'r', 'r', 'g'>(src[y][x]);

    } else if(image.format() == PixelFormat::R8Unorm) {
        auto src = image.pixels<Math::Vector<1, UnsignedByte>>();
        for(std::size_t y = 0; y != src.size()[0]; ++y)
            for(std::size_t x = 0; x != src.size()[1]; ++x)
                dst[y][x] = Math::gather<'r', 'r', 'r'>(src[y][x]);

    } else CORRADE_INTERNAL_ASSERT_UNREACHABLE();

    basisu::basis_compressor basis;
    basis.init(params);

    basisu::basis_compressor::error_code errorCode = basis.process();
    if(errorCode != basisu::basis_compressor::error_code::cECSuccess) switch(errorCode) {
        case basisu::basis_compressor::error_code::cECFailedReadingSourceImages:
            /* Emitted e.g. when source image is 0-size */
            Error{} << "Trade::BasisImageConverter::exportToData(): source image is invalid";
            return {};
        case basisu::basis_compressor::error_code::cECFailedValidating:
            /* process() will have printed additional error information to stderr */
            Error{} << "Trade::BasisImageConverter::exportToData(): type constraint validation failed";
            return {};
        case basisu::basis_compressor::error_code::cECFailedFrontEnd:
            /* process() will have printed additional error information to stderr */
            Error{} << "Trade::BasisImageConverter::exportToData(): frontend processing failed";
            return {};
        case basisu::basis_compressor::error_code::cECFailedBackend:
            Error{} << "Trade::BasisImageConverter::exportToData(): encoding failed";
            return {};
        case basisu::basis_compressor::error_code::cECFailedCreateBasisFile:
            /* process() will have printed additional error information to stderr */
            Error{} << "Trade::BasisImageConverter::exportToData(): assembling basis file data or transcoding failed";
            return {};

        /* LCOV_EXCL_START */
        case basisu::basis_compressor::error_code::cECFailedFontendExtract:
            /* This error will actually never be raised from basis_universal code */
        case basisu::basis_compressor::error_code::cECFailedWritingOutput:
            /* We do not write any files, just data */
        default:
            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
            return {};
        /* LCOV_EXCL_STOP */
    }

    const basisu::uint8_vec& out = basis.get_output_basis_file();

    Containers::Array<char> fileData{Containers::DefaultInit, out.size()};
    std::copy(out.begin(), out.end(), fileData.data());

    return fileData;
}

}}

CORRADE_PLUGIN_REGISTER(BasisImageConverter, Magnum::Trade::BasisImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.2.1")
