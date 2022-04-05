#ifndef Magnum_Trade_KtxImageConverter_h
#define Magnum_Trade_KtxImageConverter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
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
 * @brief Class @ref Magnum::Trade::KtxImageConverter
 * @m_since_latest_{plugins}
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/KtxImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_KTXIMAGECONVERTER_BUILD_STATIC
    #ifdef KtxImageConverter_EXPORTS
        #define MAGNUM_KTXIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_KTXIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_KTXIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_KTXIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_KTXIMAGECONVERTER_EXPORT
#define MAGNUM_KTXIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief KTX2 image converter plugin
@m_since_latest_{plugins}

Creates Khronos Texture 2.0 (`*.ktx2`) files from 1D, 2D and 3D images with
optional mip levels. You can use @ref KtxImporter to import images in this
format.

@section Trade-KtxImageConverter-usage Usage

This plugin depends on the @ref Trade library and is built if
`WITH_KTXIMAGECONVERTER` is enabled when building Magnum Plugins. To use as
a dynamic plugin, load @cpp "KtxImageConverter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins](https://github.com/mosra/magnum-plugins) and do the following:

@code{.cmake}
set(WITH_KTXIMAGECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::KtxImageConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `KtxImageConverter` component
of the `MagnumPlugins` package and link to the
`MagnumPlugins::KtxImageConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED KtxImageConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::KtxImageConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-KtxImageConverter-behavior Behavior and limitations

@subsection Trade-KtxImageConverter-behavior-formats Supported formats

The following formats can be written:

-   all formats in @ref PixelFormat
-   all formats in @ref CompressedPixelFormat, except for 3D ASTC formats

@subsection Trade-KtxImageConverter-behavior-types Image types

Cube map images can be written but there is currently no way to mark them
properly in the metadata. Exported files will be 3D images with faces exposed
as depth slices.

Array images will be written as images with one extra dimension for the array
layers. For example, a 2x3 2D array image with 4 layers will result in a 3D
image with size 2x3x4.

@subsection Trade-KtxImageConverter-behavior-multilevel Multilevel images

All image types can be saved with multiple levels by using the list
variants of @ref convertToFile() / @ref convertToData(). Largest level is
expected to be first, with each following level having width, height and depth
divided by two, rounded down. Incomplete mip chains are supported.

Due to the way @ref Trade-KtxImageConverter-behavior-types "non-trivial image types"
are handled, the level sizes are always expected to match the resulting image
type. This means that array images with multiple levels can currently not be
exported and produce a level size mismatch error.

@subsection Trade-KtxImageConverter-behavior-supercompression Supercompression

Saving files with [supercompression](https://github.khronos.org/KTX-Specification/#supercompressionSchemes)
is not supported. You can however use @ref BasisImageConverter to create
Basis-supercompressed KTX2 files.

@section Trade-KtxImageConverter-configuration Plugin-specific configuration

It's possible to tune various metadata options through @ref configuration().
See below for all options and their default values:

@snippet MagnumPlugins/KtxImageConverter/KtxImageConverter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_KTXIMAGECONVERTER_EXPORT KtxImageConverter: public AbstractImageConverter {
    public:
        /** @brief Plugin manager constructor */
        explicit KtxImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

    private:
        MAGNUM_KTXIMAGECONVERTER_LOCAL ImageConverterFeatures doFeatures() const override;

        MAGNUM_KTXIMAGECONVERTER_LOCAL Containers::Optional<Containers::Array<char>> doConvertToData(Containers::ArrayView<const ImageView1D> imageLevels) override;
        MAGNUM_KTXIMAGECONVERTER_LOCAL Containers::Optional<Containers::Array<char>> doConvertToData(Containers::ArrayView<const ImageView2D> imageLevels) override;
        MAGNUM_KTXIMAGECONVERTER_LOCAL Containers::Optional<Containers::Array<char>> doConvertToData(Containers::ArrayView<const ImageView3D> imageLevels) override;

        MAGNUM_KTXIMAGECONVERTER_LOCAL Containers::Optional<Containers::Array<char>> doConvertToData(Containers::ArrayView<const CompressedImageView1D> imageLevels) override;
        MAGNUM_KTXIMAGECONVERTER_LOCAL Containers::Optional<Containers::Array<char>> doConvertToData(Containers::ArrayView<const CompressedImageView2D> imageLevels) override;
        MAGNUM_KTXIMAGECONVERTER_LOCAL Containers::Optional<Containers::Array<char>> doConvertToData(Containers::ArrayView<const CompressedImageView3D> imageLevels) override;
};

}}

#endif
