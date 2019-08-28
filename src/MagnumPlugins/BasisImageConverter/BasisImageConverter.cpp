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
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include "BasisImageConverter.h"

#include <cstring>
#include <algorithm>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>

#include <basisu_enc.h>
#include <basisu_comp.h>
#include <basisu_file_headers.h>

#include <thread>

namespace Magnum { namespace Trade {

BasisImageConverter::BasisImageConverter() = default;

BasisImageConverter::BasisImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractImageConverter{manager, plugin} {}

auto BasisImageConverter::doFeatures() const -> Features { return Feature::ConvertData; }

Containers::Array<char> BasisImageConverter::doExportToData(const ImageView2D& image) {
    /* Check input */
    if(image.format() != PixelFormat::RGB8Unorm
        && image.format() != PixelFormat::RGBA8Unorm
        && image.format() != PixelFormat::RG8Unorm
        && image.format() != PixelFormat::R8Unorm) {
        Error() << "Trade::BasisImageConverter::exportToData(): unsupported format" << image.format();
        return {};
    }

    /* Magnum specific configuration */
    UnsignedInt threadCount = configuration().value<Int>("threads");
    if(threadCount == 0) {
        threadCount = std::thread::hardware_concurrency();
    }

    basisu::basis_compressor_params params;

    const bool multithreading = threadCount > 1;
    params.m_multithreading = multithreading;
    basisu::job_pool jpool(threadCount);
    params.m_pJob_pool = &jpool;

    #define PARAM_CONFIG(name, type) params.m_##name = configuration().value<type>(#name)
    PARAM_CONFIG(compression_level, int);

    PARAM_CONFIG(y_flip, bool);
    PARAM_CONFIG(debug, bool);
    PARAM_CONFIG(debug_images, bool);
    PARAM_CONFIG(perceptual, bool);
    PARAM_CONFIG(no_endpoint_rdo, bool);
    PARAM_CONFIG(no_selector_rdo, bool);
    PARAM_CONFIG(read_source_images, bool);
    PARAM_CONFIG(write_output_basis_files, bool);
    PARAM_CONFIG(compute_stats, bool);
    PARAM_CONFIG(check_for_alpha, bool);
    PARAM_CONFIG(force_alpha, bool);
    PARAM_CONFIG(seperate_rg_to_color_alpha, bool);
    PARAM_CONFIG(disable_hierarchical_endpoint_codebooks, bool);

    PARAM_CONFIG(hybrid_sel_cb_quality_thresh, float);

    PARAM_CONFIG(global_pal_bits, int);
    PARAM_CONFIG(global_mod_bits, int);

    PARAM_CONFIG(endpoint_rdo_thresh, float);
    PARAM_CONFIG(selector_rdo_thresh, float);

    PARAM_CONFIG(mip_gen, bool);
    PARAM_CONFIG(mip_renormalize, bool);
    PARAM_CONFIG(mip_wrapping, bool);
    PARAM_CONFIG(mip_srgb, bool);
    PARAM_CONFIG(mip_scale, float);
    PARAM_CONFIG(mip_smallest_dimension, int);
    PARAM_CONFIG(mip_filter, std::string);

    PARAM_CONFIG(max_endpoint_clusters, int);
    PARAM_CONFIG(max_selector_clusters, int);
    PARAM_CONFIG(quality_level, int);
    #undef PARAM_CONFIG

    basist::etc1_global_selector_codebook sel_codebook(basist::g_global_selector_cb_size, basist::g_global_selector_cb);
    params.m_pSel_codebook = &sel_codebook;

    /* Enable verbose debug output in basis to get reasons for failures */
    basisu::enable_debug_printf(configuration().value<bool>("enable_debug_printf"));

    if(image.size().x() <= 0 || image.size().y() <= 0) {
        Error() << "Trade::BasisImageConverter::exportToData(): source image is empty";
        return {};
    }

    if(!image.data()) {
        Error() << "Trade::BasisImageConverter::exportToData(): source image data is nullptr";
        return {};
    }

    /* Copy image data into the basis image. There is no way to construct a
       basis image from existing data as it is based on a std::vector */
    params.m_source_images.emplace_back(image.size().x(), image.size().y());

    /* Get data properties and calculate the initial slice based on subimage
       offset */
    const std::pair<Math::Vector2<std::size_t>, Math::Vector2<std::size_t>> dataProperties =
        image.dataProperties();
    Containers::ArrayView<const char> inputData = image.data().suffix(
        dataProperties.first.sum());

    basisu::image& basisImage = params.m_source_images.back();

    /* Do Y-flip and tight packing of image data */
    const std::size_t rowSize = image.size().x()*image.pixelSize();
    const std::size_t rowStride = dataProperties.second.x();
    const std::size_t packedDataSize = rowSize*image.size().y();

    /* basis image is always RGBA, fill in alpha if necessary */
    if (image.format() == PixelFormat::RGB8Unorm) {
        const std::size_t width = image.size().x();
        const std::size_t height = image.size().y();

        auto basisView = Containers::arrayCast<Color4ub>(
            Containers::arrayView<basisu::color_rgba>(basisImage.get_ptr(), width*height));
        for(Int y = 0; y < height; ++y) {
            const auto src = image.pixels<Color3ub>()[height - y -1];

            Color4ub* destPixel = basisView.suffix(y*width).begin();
            for(const Color3ub& pixel: src) {
                *(destPixel++) = pixel;
            }
        }

    } else if(image.format() == PixelFormat::RG8Unorm) {
        const std::size_t width = image.size().x();
        const std::size_t height = image.size().y();

        auto basisView = Containers::arrayCast<Color4ub>(
            Containers::arrayView<basisu::color_rgba>(basisImage.get_ptr(), width*height));
        for(Int y = 0; y < height; ++y) {
            const auto src = Containers::arrayCast<const Math::Vector2<UnsignedByte>>(inputData
                .suffix((height - y - 1)*rowStride).prefix(rowSize));

            Color4ub* destPixel = basisView.suffix(y*width).begin();
            for(const Math::Vector2<UnsignedByte> pixel: src) {
                destPixel->rgb() = Color3ub{pixel.x()};
                destPixel->a() = pixel.y();
                destPixel++;
            }
        }
    } else if(image.format() == PixelFormat::R8Unorm) {
        const std::size_t width = image.size().x();
        const std::size_t height = image.size().y();

        auto basisView = Containers::arrayCast<Color4ub>(
            Containers::arrayView<basisu::color_rgba>(basisImage.get_ptr(), width*height));
        for(Int y = 0; y < height; ++y) {
            const auto src = Containers::arrayCast<const UnsignedByte>(inputData
                .suffix((height - y - 1)*rowStride).prefix(rowSize));

            Color4ub* destPixel = basisView.suffix(y*width).begin();
            for(const UnsignedByte pixel: src) {
                (destPixel++)->rgb() = Color3ub{pixel};
            }
        }
    } else {
        CORRADE_INTERNAL_ASSERT(image.format() == PixelFormat::RGBA8Unorm);

        const std::size_t width = image.size().x();
        const std::size_t height = image.size().y();

        auto basisView = Containers::arrayCast<Color4ub>(
            Containers::arrayView<basisu::color_rgba>(basisImage.get_ptr(), width*height));
            ;
        for(Int y = 0; y < height; ++y) {
            auto basisRow = basisView.suffix(y*width).begin();
            auto row = image.pixels<Color4ub>()[height - y - 1];
            for(int x = 0; x < width; ++x) {
                 basisRow[x] = row[x];
            }
        }
    }

    basisu::basis_compressor basis;
    basis.init(params);

    basisu::basis_compressor::error_code errorCode = basis.process();
    switch(errorCode) {
        case basisu::basis_compressor::error_code::cECSuccess:
            break;
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
            CORRADE_ASSERT_UNREACHABLE();
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
