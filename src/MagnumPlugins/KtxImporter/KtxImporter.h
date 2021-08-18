#ifndef Magnum_Trade_KtxImporter_h
#define Magnum_Trade_KtxImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/** @file
 * @brief Class @ref Magnum::Trade::KtxImporter
 * @m_since_latest_{plugins}
 */

#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/KtxImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_KTXIMPORTER_BUILD_STATIC
    #ifdef KtxImporter_EXPORTS
        #define MAGNUM_KTXIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_KTXIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_KTXIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_KTXIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_KTXIMPORTER_EXPORT
#define MAGNUM_KTXIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief KTX2 image importer plugin
@m_since_latest_{plugins}

Supports Khronos Texture 2.0 images (`*.ktx2`).

@section Trade-KtxImporter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_KTXIMPORTER` is enabled when building Magnum Plugins. To use as a dynamic
plugin, load @cpp "KtxImporter" @ce via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(WITH_KTXIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::KtxImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `KtxImporter` component of the
`MagnumPlugins` package in CMake and link to the `MagnumPlugins::KtxImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED KtxImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::KtxImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-KtxImporter-behavior Behavior and limitations

Imports images in the following formats:

-   KTX2 with all uncompressed Vulkan formats that have an equivalent in
    @ref PixelFormat, with component swizzling as necessary
-   KTX2 with most compressed Vulkan formats that have an equivalent in
    @ref CompressedPixelFormat. None of the 3D ASTC formats are supported.

With compressed pixel formats, the image will not be flipped if the Y- or Z-axis
orientation doesn't match the output orientation. The nontrivial amount of work
involved with flipping block-compressed data makes this unfeasible. The import
will succeed but a warning will be emitted.

The importer recognizes @ref ImporterFlag::Verbose, printing additional info
when the flag is enabled.

@subsection Trade-KtxImporter-behavior-swizzle Swizzle support

Explicit swizzling via the KTXswizzle header entry supports BGR and BGRA. Any
other non-identity channel remapping is unsupported and results in an error.
*/
class MAGNUM_KTXIMPORTER_EXPORT KtxImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit KtxImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~KtxImporter();

    private:
        MAGNUM_KTXIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_KTXIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_KTXIMPORTER_LOCAL void doClose() override;
        MAGNUM_KTXIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;

        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doImage1DCount() const override;
        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doImage1DLevelCount(UnsignedInt id) override;
        MAGNUM_KTXIMPORTER_LOCAL Containers::Optional<ImageData1D> doImage1D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doImage2DLevelCount(UnsignedInt id) override;
        MAGNUM_KTXIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doImage3DCount() const override;
        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doImage3DLevelCount(UnsignedInt id) override;
        MAGNUM_KTXIMPORTER_LOCAL Containers::Optional<ImageData3D> doImage3D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_KTXIMPORTER_LOCAL UnsignedInt doTextureCount() const override;
        MAGNUM_KTXIMPORTER_LOCAL Containers::Optional<TextureData> doTexture(UnsignedInt id) override;

    private:
        struct File;
        Containers::Pointer<File> _f;

        template<UnsignedInt dimensions>
        ImageData<dimensions> doImage(UnsignedInt id, UnsignedInt level);
};

}}

#endif
