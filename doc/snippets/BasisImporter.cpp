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
    FROM, OUT OF OR IN CONNETCION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <Corrade/Containers/Optional.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#ifdef MAGNUM_TARGET_GL
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Extensions.h>
#endif

using namespace Magnum;

int main() {

{
PluginManager::Manager<Trade::AbstractImporter> manager;
/* [target-format-suffix] */
/* Choose ETC2 target format */
Containers::Pointer<Trade::AbstractImporter> importerEtc2 =
    manager.instantiate("BasisImporterEtc2");

/* Choose BC5 target format */
Containers::Pointer<Trade::AbstractImporter> importerBc5 =
    manager.instantiate("BasisImporterBc5");
/* [target-format-suffix] */
}

{
PluginManager::Manager<Trade::AbstractImporter> manager;
Containers::Optional<Trade::ImageData2D> image;
/* [target-format-config] */
/* Instantiate the plugin under its default name. At this point, the plugin
   would decompress to full RGBA8, which is usually not what you want. */
Containers::Pointer<Trade::AbstractImporter> importer =
    manager.instantiate("BasisImporter");
importer->openFile("mytexture.basis");

/* Transcode the image to BC5 */
importer->configuration().setValue("format", "Bc5");
image = importer->image2D(0);
// ...

/* Transcode the same image, but to ETC2 now */
importer->configuration().setValue("format", "Etc2");
image = importer->image2D(0);
// ...
/* [target-format-config] */
}

#ifdef MAGNUM_TARGET_GL
{
PluginManager::Manager<Trade::AbstractImporter> manager;
/* [gl-extension-checks] */
if(PluginManager::PluginMetadata* metadata = manager.metadata("BasisImporter")) {
    GL::Context& context = GL::Context::current();
    using namespace GL::Extensions;
    #ifdef MAGNUM_TARGET_WEBGL
    if(context.isExtensionSupported<WEBGL::compressed_texture_astc>())
    #else
    if(context.isExtensionSupported<KHR::texture_compression_astc_ldr>())
    #endif
    {
        metadata->configuration().setValue("format", "Astc4x4RGBA");
    }
    #ifdef MAGNUM_TARGET_GLES
    else if(context.isExtensionSupported<EXT::texture_compression_bptc>())
    #else
    else if(context.isExtensionSupported<ARB::texture_compression_bptc>())
    #endif
    {
        metadata->configuration().setValue("format", "Bc7RGBA");
    }
    #ifdef MAGNUM_TARGET_WEBGL
    else if(context.isExtensionSupported<WEBGL::compressed_texture_s3tc>())
    #elif defined(MAGNUM_TARGET_GLES)
    else if(context.isExtensionSupported<EXT::texture_compression_s3tc>() ||
            context.isExtensionSupported<ANGLE::texture_compression_dxt5>())
    #else
    else if(context.isExtensionSupported<EXT::texture_compression_s3tc>())
    #endif
    {
        metadata->configuration().setValue("format", "Bc3RGBA");
    }
    #ifndef MAGNUM_TARGET_GLES2
    else
    #ifndef MAGNUM_TARGET_GLES
    if(context.isExtensionSupported<ARB::ES3_compatibility>())
    #endif
    {
        metadata->configuration().setValue("format", "Etc2RGBA");
    }
    #else /* For ES2, fall back to PVRTC as ETC2 is not available */
    else
    #ifdef MAGNUM_TARGET_WEBGL
    if(context.isExtensionSupported<WEBGL::compressed_texture_pvrtc>())
    #else
    if(context.isExtensionSupported<IMG::texture_compression_pvrtc>())
    #endif
    {
        metadata->configuration().setValue("format", "PvrtcRGBA4bpp");
    }
    #endif
    #if defined(MAGNUM_TARGET_GLES2) || !defined(MAGNUM_TARGET_GLES)
    else /* ES3 has ETC2 always */
    {
        /* Fall back to uncompressed if nothing else is supported */
        metadata->configuration().setValue("format", "RGBA8");
    }
    #endif
}
/* [gl-extension-checks] */
}
#endif

}
