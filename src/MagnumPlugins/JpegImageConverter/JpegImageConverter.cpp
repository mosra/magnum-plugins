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

#include "JpegImageConverter.h"

#include <csetjmp>
#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

#ifdef CORRADE_TARGET_WINDOWS
/* On Windows we need to circumvent conflicting definition of INT32 in
   <windows.h> (included from OpenGL headers). Problem with libjpeg-tubo only,
   libjpeg solves that already somehow. */
#define XMD_H

/* In case of MSYS2 MinGW packages, jmorecfg.h for some reason includes
   shlwapi.h, which includes objbase.h and rpc.h which in turn does the dreaded
   #define interface struct, breaking CORRADE_PLUGIN_REGISTER() on builds that
   enable BUILD_PLUGINS_STATIC. This is not reproducible with plain MinGW and
   hand-compiled libjpeg nor MSVC, so really specific to this patch:
   https://github.com/msys2/MINGW-packages/blob/e9badcb5e7ee88e87f6cb6a23b6e0ba69468135b/mingw-w64-libjpeg-turbo/0001-header-compat.mingw.patch
   Defining NOSHLWAPI makes the shlwapi.h header a no-op, fixing this issue. */
#ifdef __MINGW32__
#define NOSHLWAPI
#endif
#endif

#include <jpeglib.h>

namespace Magnum { namespace Trade {

JpegImageConverter::JpegImageConverter() {
    /** @todo horrible workaround, fix this properly */
    configuration().setValue("jpegQuality", 0.8f);
}

JpegImageConverter::JpegImageConverter(PluginManager::AbstractManager& manager, std::string plugin): AbstractImageConverter(manager, std::move(plugin)) {}

ImageConverterFeatures JpegImageConverter::doFeatures() const { return ImageConverterFeature::Convert2DToData; }

Containers::Array<char> JpegImageConverter::doConvertToData(const ImageView2D& image) {
    static_assert(BITS_IN_JSAMPLE == 8, "Only 8-bit JPEG is supported");

    Int components;
    J_COLOR_SPACE colorSpace;
    switch(image.format()) {
        case PixelFormat::R8Unorm:
            components = 1;
            colorSpace = JCS_GRAYSCALE;
            break;
        case PixelFormat::RGB8Unorm:
            components = 3;
            colorSpace = JCS_RGB;
            break;
        case PixelFormat::RGBA8Unorm:
            #ifdef JCS_EXTENSIONS
            components = 4;
            colorSpace = JCS_EXT_RGBX;
            Warning{} << "Trade::JpegImageConverter::convertToData(): ignoring alpha channel";
            break;
            #else
            Error{} << "Trade::JpegImageConverter::convertToData(): RGBA input (with alpha ignored) requires libjpeg-turbo";
            return nullptr;
            #endif
        default:
            Error() << "Trade::JpegImageConverter::convertToData(): unsupported pixel format" << image.format();
            return nullptr;
    }

    /* Initialize structures. Needs to be before the setjmp crap in order to
       avoid leaks on error. */
    jpeg_compress_struct info;
    struct DestinationManager {
        jpeg_destination_mgr jpegDestinationManager;
        std::string output;
    } destinationManager;

    Containers::Array<JSAMPROW> rows;
    Containers::Array<char> data;

    /* Fugly error handling stuff */
    /** @todo Get rid of this crap */
    struct ErrorManager {
        jpeg_error_mgr jpegErrorManager;
        std::jmp_buf setjmpBuffer;
        char message[JMSG_LENGTH_MAX]{};
    } errorManager;
    info.err = jpeg_std_error(&errorManager.jpegErrorManager);
    errorManager.jpegErrorManager.error_exit = [](j_common_ptr info) {
        auto& errorManager = *reinterpret_cast<ErrorManager*>(info->err);
        info->err->format_message(info, errorManager.message);
        std::longjmp(errorManager.setjmpBuffer, 1);
    };
    if(setjmp(errorManager.setjmpBuffer)) {
        Error{} << "Trade::JpegImageConverter::convertToData(): error:" << errorManager.message;
        jpeg_destroy_compress(&info);
        return nullptr;
    }

    /* Create the compression structure */
    jpeg_create_compress(&info);
    info.dest = reinterpret_cast<jpeg_destination_mgr*>(&destinationManager);
    info.dest->init_destination = [](j_compress_ptr info) {
        auto& destinationManager = *reinterpret_cast<DestinationManager*>(info->dest);
        destinationManager.output.resize(1); /* It crashes if the buffer has zero free space */
        info->dest->next_output_byte = reinterpret_cast<JSAMPLE*>(&destinationManager.output[0]);
        info->dest->free_in_buffer = destinationManager.output.size()/sizeof(JSAMPLE);
    };
    info.dest->term_destination = [](j_compress_ptr info) {
        auto& destinationManager = *reinterpret_cast<DestinationManager*>(info->dest);
        destinationManager.output.resize(destinationManager.output.size() - info->dest->free_in_buffer);
    };
    info.dest->empty_output_buffer = [](j_compress_ptr info) -> boolean {
        auto& destinationManager = *reinterpret_cast<DestinationManager*>(info->dest);
        const std::size_t oldSize = destinationManager.output.size();
        destinationManager.output.resize(oldSize*2); /* Double capacity each time it is exceeded */
        info->dest->next_output_byte = reinterpret_cast<JSAMPLE*>(&destinationManager.output[0] + oldSize);
        info->dest->free_in_buffer = (destinationManager.output.size() - oldSize)/sizeof(JSAMPLE);
        return boolean(true);
    };

    /* Fill the info structure */
    info.image_width = image.size().x();
    info.image_height = image.size().y();
    info.input_components = components;
    info.in_color_space = colorSpace;

    jpeg_set_defaults(&info);
    jpeg_set_quality(&info, Int(configuration().value<Float>("jpegQuality")*100.0f), boolean(true));
    jpeg_start_compress(&info, boolean(true));

    /* Get data properties and calculate the initial slice based on subimage
       offset */
    const std::pair<Math::Vector2<std::size_t>, Math::Vector2<std::size_t>> dataProperties = image.dataProperties();
    Containers::ArrayView<const char> inputData = image.data().suffix(dataProperties.first.sum());

    while(info.next_scanline < info.image_height) {
        /* libJPEG HAVE YOU EVER HEARD ABOUT CONST ARGUMENTS?! IT'S NOT 1978
           ANYMORE */
        JSAMPROW row = reinterpret_cast<JSAMPROW>(const_cast<char*>(inputData.suffix((image.size().y() - info.next_scanline - 1)*dataProperties.second.x()).data()));
        jpeg_write_scanlines(&info, &row, 1);
    }

    jpeg_finish_compress(&info);
    jpeg_destroy_compress(&info);

    /* Copy the string into the output array (I would kill for having std::string::release()) */
    Containers::Array<char> fileData{NoInit, destinationManager.output.size()};
    std::copy(destinationManager.output.begin(), destinationManager.output.end(), fileData.data());
    return fileData;
}

}}

CORRADE_PLUGIN_REGISTER(JpegImageConverter, Magnum::Trade::JpegImageConverter,
    "cz.mosra.magnum.Trade.AbstractImageConverter/0.3")
