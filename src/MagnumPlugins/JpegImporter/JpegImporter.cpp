/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016
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

#include "JpegImporter.h"

#include <csetjmp>
#include <Corrade/Utility/Debug.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

/* On Windows we need to circumvent conflicting definition of INT32 in
   <windows.h> (included by glLoadGen from OpenGL.h). Problem with libjpeg-tubo
   only, libjpeg solves that already somehow. */
#ifdef CORRADE_TARGET_WINDOWS
#define XMD_H
#endif
#include <jpeglib.h>

#ifdef MAGNUM_TARGET_GLES2
#include <Magnum/Context.h>
#include <Magnum/Extensions.h>
#endif

namespace Magnum { namespace Trade {

JpegImporter::JpegImporter() = default;

JpegImporter::JpegImporter(PluginManager::AbstractManager& manager, std::string plugin): AbstractImporter(manager, std::move(plugin)) {}

JpegImporter::~JpegImporter() { close(); }

auto JpegImporter::doFeatures() const -> Features { return Feature::OpenData; }

bool JpegImporter::doIsOpened() const { return _in; }

void JpegImporter::doClose() { _in = nullptr; }

void JpegImporter::doOpenData(const Containers::ArrayView<const char> data) {
    _in = Containers::Array<unsigned char>(data.size());
    std::copy(data.begin(), data.end(), _in.begin());
}

UnsignedInt JpegImporter::doImage2DCount() const { return 1; }

std::optional<ImageData2D> JpegImporter::doImage2D(UnsignedInt) {
    /* Initialize structures */
    jpeg_decompress_struct file;
    Containers::Array<JSAMPROW> rows;
    Containers::Array<char> data;

    /* Fugly error handling stuff */
    /** @todo Get rid of this crap */
    struct ErrorManager {
        jpeg_error_mgr jpegErrorManager;
        std::jmp_buf setjmpBuffer;
    } errorManager;
    file.err = jpeg_std_error(&errorManager.jpegErrorManager);
    errorManager.jpegErrorManager.error_exit = [](j_common_ptr info) {
        info->err->output_message(info);
        std::longjmp(reinterpret_cast<ErrorManager*>(info->err)->setjmpBuffer, 1);
    };
    if(setjmp(errorManager.setjmpBuffer)) {
        Error() << "Trade::JpegImporter::image2D(): error while reading JPEG file";

        jpeg_destroy_decompress(&file);
        return std::nullopt;
    }

    /* Open file */
    jpeg_create_decompress(&file);
    jpeg_mem_src(&file, _in.begin(), _in.size());

    /* Read file header, start decompression */
    jpeg_read_header(&file, true);
    jpeg_start_decompress(&file);

    /* Image size and type */
    const Vector2i size(file.output_width, file.output_height);
    static_assert(BITS_IN_JSAMPLE == 8, "Only 8-bit JPEG is supported");
    constexpr PixelType type = PixelType::UnsignedByte;

    /* Image format */
    PixelFormat format = {};
    switch(file.out_color_space) {
        case JCS_GRAYSCALE:
            CORRADE_INTERNAL_ASSERT(file.out_color_components == 1);
            #ifdef MAGNUM_TARGET_GLES2
            format = Context::hasCurrent() && Context::current().isExtensionSupported<Extensions::GL::EXT::texture_rg>() ?
                PixelFormat::Red : PixelFormat::Luminance;
            #else
            format = PixelFormat::Red;
            #endif
            break;
        case JCS_RGB:
            CORRADE_INTERNAL_ASSERT(file.out_color_components == 3);
            format = PixelFormat::RGB;
            break;

        /** @todo RGBA (only in libjpeg-turbo and probably ignored) */

        default:
            Error() << "Trade::JpegImporter::image2D(): unsupported color space" << file.out_color_space;
            return std::nullopt;
    }

    /* Initialize data array, align rows to four bytes */
    const std::size_t stride = ((size.x()*file.out_color_components*BITS_IN_JSAMPLE/8 + 3)/4)*4;
    data = Containers::Array<char>{stride*std::size_t(size.y())};

    /* Read image row by row */
    rows = Containers::Array<JSAMPROW>{std::size_t(size.y())};
    for(Int i = 0; i != size.y(); ++i)
        rows[i] = reinterpret_cast<JSAMPROW>(data.data()) + (size.y() - i - 1)*stride;
    while(file.output_scanline < file.output_height)
        jpeg_read_scanlines(&file, rows + file.output_scanline, file.output_height - file.output_scanline);

    /* Cleanup */
    jpeg_finish_decompress(&file);
    jpeg_destroy_decompress(&file);

    /* Always using the default 4-byte alignment */
    return Trade::ImageData2D{format, type, size, std::move(data)};
}

}}
