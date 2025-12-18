#ifndef Magnum_Trade_BasisImageConverter_h
#define Magnum_Trade_BasisImageConverter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2019 Jonathan Hale <squareys@googlemail.com>
    Copyright © 2021, 2022, 2024 Pablo Escobar <mail@rvrs.in>

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
 * @brief Class @ref Magnum::Trade::BasisImageConverter
 * @m_since_{plugins,2019,10}
 */

#include <Magnum/Trade/AbstractImageConverter.h>

#include "MagnumPlugins/BasisImageConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_BASISIMAGECONVERTER_BUILD_STATIC
    #ifdef BasisImageConverter_EXPORTS
        #define MAGNUM_BASISIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_BASISIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_BASISIMAGECONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_BASISIMAGECONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_BASISIMAGECONVERTER_EXPORT
#define MAGNUM_BASISIMAGECONVERTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief Basis Universal image converter plugin
@m_since_{plugins,2019,10}

@m_keywords{BasisKtxImageConverter}

Creates [Basis Universal](https://github.com/binomialLLC/basis_universal)
compressed image files (`*.basis` or `*.ktx2`) from 2D and 2D array images with
optional mip levels. You can use @ref BasisImporter to import images in this
format.

This plugin provides `BasisKtxImageConverter`.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [Basis Universal GPU Texture Codec](https://github.com/BinomialLLC/basis_universal)
    library, licensed under @m_class{m-label m-success} **Apache-2.0**
    ([license text](https://opensource.org/licenses/Apache-2.0),
    [choosealicense.com](https://choosealicense.com/licenses/apache-2.0/)). It
    requires attribution for public use.

@section Trade-BasisImageConverter-usage Usage

@m_class{m-note m-success}

@par
    This class is a plugin that's meant to be dynamically loaded and used
    via the base @ref AbstractImageConverter interface. See its documentation
    for introduction and usage examples.

This plugin depends on the @ref Trade and [Basis Universal](https://github.com/binomialLLC/basis_universal)
libraries and is built if `MAGNUM_WITH_BASISIMAGECONVERTER` is enabled when
building Magnum Plugins. To use as a dynamic plugin, load
@cpp "BasisImageConverter" @ce via @ref Corrade::PluginManager::Manager.
Current version of the plugin is tested against the
[`v1_50_0_2`](https://github.com/BinomialLLC/basis_universal/tree/v1_50_0_2),
`1.16.4` and `v1_15_update2` tags, but could possibly compile against newer
versions as well.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins](https://github.com/mosra/magnum-plugins),
[zstd](https://github.com/facebook/zstd) and
[basis-universal](https://github.com/BinomialLLC/basis_universal) repositories
and do the following. Basis uses Zstd for KTX2 images, instead of bundling you
can depend on an externally-installed zstd package, in which case omit the
first part of the setup below. If Zstd isn't bundled and isn't found
externally, Basis will be compiled without Zstd support.

@code{.cmake}
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
# Create a static library so the plugin is self-contained
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
# Basis doesn't use any multithreading in zstd, this prevents a need to link to
# pthread on Linux
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
# Don't build Zstd tests if enable_testing() was called in parent project
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(zstd/build/cmake EXCLUDE_FROM_ALL)

set(BASIS_UNIVERSAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/basis-universal)
set(MAGNUM_WITH_BASISIMAGECONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::BasisImageConverter)
@endcode

When compiling Basis from source with `BASIS_UNIVERSAL_DIR`, Basis will be
built with OpenCL support if the [OpenCL package](https://cmake.org/cmake/help/v3.1/module/FindOpenCL.html)
is found. To build Basis without a hard dependency on OpenCL, pass
`-DCMAKE_DISABLE_FIND_PACKAGE_OpenCL=ON` to CMake, or  @cmake set() @ce it
before the @cmake add_subdirectory() @ce call if  using Magnum Plugins as a
subproject.

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `BasisImageConverter` component of
the `MagnumPlugins` package and link to the
`MagnumPlugins::BasisImageConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED BasisImageConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::BasisImageConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-BasisImageConverter-behavior Behavior and limitations

Accepts 2D, 2D array, cube map and cube map array images, recognizing
@ref ImageFlag3D::Array and/or @ref ImageFlag3D::CubeMap in passed images.
The @ref PixelFormat::R8Unorm, @relativeref{PixelFormat,R8Srgb},
@relativeref{PixelFormat,RG8Unorm}, @relativeref{PixelFormat,RG8Srgb},
@relativeref{PixelFormat,RGB8Unorm}, @relativeref{PixelFormat,RGB8Srgb},
@relativeref{PixelFormat,RGBA8Unorm}, @relativeref{PixelFormat,RGBA8Srgb},
@relativeref{PixelFormat,R32F} and @relativeref{PixelFormat,RG32F},
@relativeref{PixelFormat,RGB32F} and @relativeref{PixelFormat,RGBA32F}
formats are supported.

HDR floating point formats require at least version 1.50 of Basis.

The alpha channel of @relativeref{PixelFormat,RGBA32F} will be dropped and
transcoded to 1 because Basis UASTC HDR doesn't support alpha channels.

Even though the KTX container format supports 1D, 1D array and 3D images, Basis
Universal doesn't. In particular, if a 2D image with @ref ImageFlag2D::Array is
passed, the conversion will fail as it's not possible to represent 1D array
images without a significant loss in quality and layer cross-talk. On the other
hand, if a 3D image without @ref ImageFlag3D::Array is passed, a warning is
printed and the file is saved as a 2D array image.

The plugin recognizes @ref ImageConverterFlag::Quiet, which will cause all
conversion warnings to be suppressed.

@subsection Trade-BasisImageConverter-behavior-multilevel Multilevel images

Images can be saved with multiple levels by using the list variants of
@ref convertToFile() / @ref convertToData(). Largest level is expected to be
first, with each following level having width and height divided by two,
rounded down. Because only 2D array images are supported, depth has to have the
same size in all levels. Incomplete mip chains are supported.

To generate mip levels from a single top-level image instead, you can use the
@cb{.ini} mip_gen @ce @ref Trade-BasisImageConverter-configuration "configuration option".

@subsection Trade-BasisImageConverter-behavior-swizzling Implicit swizzling

If no user-specified channel mapping is supplied through the
@cb{.ini} swizzle @ce @ref Trade-BasisImageConverter-configuration "configuration option",
the converter swizzles 1- and 2-channel formats before compression as follows:

-   1-channel formats (@ref PixelFormat::R8Unorm / @ref PixelFormat::R8Srgb /
    @ref PixelFormat::R32F) are remapped as RRR, producing an opaque gray-scale
    image
-   2-channel formats (@ref PixelFormat::RG8Unorm / @ref PixelFormat::RG8Srgb /
    @ref PixelFormat::RG32F) are remapped as RRRG, ie. G becomes the alpha
    channel. This significantly improves compressed image quality because RGB
    and alpha get separate slices instead of the two channels being compressed
    into a single slice.

Setting the @cb{.ini} swizzle @ce option to any value disables this behavior.
To keep the original channel order, set @cb{.ini} swizzle=rgba @ce.

@subsection Trade-BasisImageConverter-behavior-ktx Converting to KTX2

To create Khronos Texture 2.0 (`*.ktx2`) files, either load the plugin as
`BasisKtxImageConverter` or call @ref convertToFile() with the `.ktx2`
extension. In all other cases, a Basis Universal (`*.basis`) file is created.

The @ref extension() is @cpp "ktx2" @ce and @ref mimeType() @cpp "image/ktx2" @ce
if the plugin is loaded as `BasisKtxImageConverter`. In all other cases (and
independently of what file extension is used in a call to @ref convertToFile()),
@ref extension is @cpp "basis" @ce and @ref mimeType() is an empty string as
the Basis Universal file format doesn't have a registered MIME type.

@subsection Trade-BasisImageConverter-behavior-loading Loading the plugin fails with undefined symbol: pthread_create

On Linux it may happen that loading the plugin will fail with
`undefined symbol: pthread_create`. The Basis encoder is optionally
multithreaded and while linking the dynamic plugin library to `pthread` would
resolve this particular error, the actual thread creation (if the
@cb{.ini} threads @ce @ref Trade-BasisImageConverter-configuration "configuration option"
is set to something else than `1`) later would cause @ref std::system_error to
be thrown (or, worst case, crashing on a null function pointer call on some
systems). Unfortunately there's no portable way to detect this case at runtime
and fail gracefully, so the plugin requires *the application* to link to
`pthread` instead. With CMake it can be done like this:

@code{.cmake}
find_package(Threads REQUIRED)
target_link_libraries(your-application PRIVATE Threads::Threads)
@endcode

@subsection Trade-BasisImageConverter-behavior-multithreading Thread safety

While the encoder library *should* behave in a way that doesn't modify any
global state, in versions before 1.16 the library initialization done at plugin
load time is populating global safe in a non-thread-safe way. Thus you have to
ensure that the plugin isn't loaded from multiple threads at the same time, or
loaded while being already used from another thread.

@section Trade-BasisImageConverter-configuration Plugin-specific configuration

Basis compression can be configured to produce better quality or reduce
encoding time. Configuration options are equivalent to parameters of the C++
encoder API in `basis_compressor`. The `basisu` tool options mostly match the
encoder API parameters and its [help text](https://github.com/BinomialLLC/basis_universal/blob/v1_50_0_2/basisu_tool.cpp#L100)
provides useful descriptions of most of the parameters, their ranges and the
impact on quality/speed. The full form of the configuration is shown below:

@snippet MagnumPlugins/BasisImageConverter/BasisImageConverter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_BASISIMAGECONVERTER_EXPORT BasisImageConverter: public AbstractImageConverter {
    public:
        #ifdef MAGNUM_BUILD_DEPRECATED
        /**
         * @brief Output file format
         * @m_deprecated_since_latest Direct plugin instantiation isn't a
         *      supported use case anymore and thus it isn't possible to pass
         *      the format to @ref BasisImageConverter(Format). Instantiate
         *      through the plugin manager as `BasisImageConverter` or
         *      `BasisKtxImageConverter` or specify a `*.basis` or `*.ktx2`
         *      extension to choose between the formats.
         */
        /* Not marked with CORRADE_DEPRECATED() as the enum values are used
           internally */
        enum class Format: Int {
            /* 0 used for default value, Basis unless overridden by
               convertToFile */

            Basis = 1,    /**< Output Basis images */
            Ktx,          /**< Output KTX2 images */
        };
        #endif

        /**
         * @brief Initialize Basis encoder
         * @m_since_latest_{plugins}
         */
        static void initialize();

        /**
         * @brief Finalize Basis encoder
         * @m_since_latest_{plugins}
         */
        static void finalize();

        #ifdef MAGNUM_BUILD_DEPRECATED
        /**
         * @brief Default constructor
         * @m_deprecated_since_latest Direct plugin instantiation isn't a
         *      supported use case anymore and thus it isn't possible to pass
         *      the format to the constructor. Instantiate through the plugin
         *      manager as `BasisImageConverter` or `BasisKtxImageConverter` or
         *      specify a `*.basis` or `*.ktx2` extension to choose between the
         *      formats.
         */
        CORRADE_DEPRECATED("instantiate through the plugin manager instead") explicit BasisImageConverter(Format format = Format{});
        #endif

        /** @brief Plugin manager constructor */
        explicit BasisImageConverter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

    private:
        MAGNUM_BASISIMAGECONVERTER_LOCAL ImageConverterFeatures doFeatures() const override;
        MAGNUM_BASISIMAGECONVERTER_LOCAL Containers::String doExtension() const override;
        MAGNUM_BASISIMAGECONVERTER_LOCAL Containers::String doMimeType() const override;

        MAGNUM_BASISIMAGECONVERTER_LOCAL Containers::Optional<Containers::Array<char>> doConvertToData(Containers::ArrayView<const ImageView2D> imageLevels) override;
        MAGNUM_BASISIMAGECONVERTER_LOCAL Containers::Optional<Containers::Array<char>> doConvertToData(Containers::ArrayView<const ImageView3D> imageLevels) override;

        template<UnsignedInt dimensions> MAGNUM_BASISIMAGECONVERTER_LOCAL bool convertLevelsToFile(const Containers::ArrayView<const BasicImageView<dimensions>> imageLevels, const Containers::StringView filename);

        MAGNUM_BASISIMAGECONVERTER_LOCAL bool doConvertToFile(const Containers::ArrayView<const ImageView2D> imageLevels, const Containers::StringView filename) override;
        MAGNUM_BASISIMAGECONVERTER_LOCAL bool doConvertToFile(const Containers::ArrayView<const ImageView3D> imageLevels, const Containers::StringView filename) override;

        #ifndef MAGNUM_BUILD_DEPRECATED
        enum class Format: Int;
        #endif
        Format _format;
};

}}

#endif
