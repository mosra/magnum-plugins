#ifndef Magnum_Trade_AstcImporter_h
#define Magnum_Trade_AstcImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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
 * @brief Class @ref Magnum::Trade::AstcImporter
 * @m_since_latest_{plugins}
 */

#include <Corrade/Containers/Pointer.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/AstcImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_ASTCIMPORTER_BUILD_STATIC
    #ifdef AstcImporter_EXPORTS
        #define MAGNUM_ASTCIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_ASTCIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_ASTCIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_ASTCIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_ASTCIMPORTER_EXPORT
#define MAGNUM_ASTCIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief ASTC importer plugin
@m_since_latest_{plugins}

Loads 2D and 3D ASTC (`*.astc`) files produced by
[ARM ASTC encoder](https://github.com/ARM-software/astc-encoder) and other
texture compression tools.

@section Trade-AstcImporter-usage Usage

This plugin depends on the @ref Trade library and is built if
`MAGNUM_WITH_ASTCIMPORTER` is enabled when building Magnum Plugins. To use as a
dynamic plugin, load @cpp "AstcImporter" @ce via
@ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins repository](https://github.com/mosra/magnum-plugins) and do the
following:

@code{.cmake}
set(MAGNUM_WITH_ASTCIMPORTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::AstcImporter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
into your `modules/` directory, request the `AstcImporter` component of the
`MagnumPlugins` package in CMake and link to the `MagnumPlugins::AstcImporter`
target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED AstcImporter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::AstcImporter)
@endcode

See @ref building-plugins, @ref cmake-plugins, @ref plugins and
@ref file-formats for more information.

@section Trade-AstcImporter-behavior Behavior and limitations

By default, images are imported with @ref CompressedPixelFormat::Astc4x4RGBAUnorm
to @relativeref{CompressedPixelFormat,Astc12x12RGBAUnorm} for 2D ASTC
compression and @relativeref{CompressedPixelFormat,Astc3x3x3RGBAUnorm} to
@relativeref{CompressedPixelFormat,Astc6x6x6RGBAUnorm} for 3D ASTC compression.
The file format contains only information about block size but not about the
actual type of data, thus to get `*RGBASrgb` and `*RGBAF` formats instead of
`*RGBAUnorm` you have to explicitly set the @cb{.ini} format @ce
@ref Trade-AstcImporter-configuration "configuration option".

Files with 3D ASTC blocks are always exposed as 3D images instead of 2D.
Additionally, if a file has 2D ASTC blocks but the Z size is not 1 (a 2D array
texture), the image is also exposed as 3D, with @ref ImageFlag3D::Array set.
The ARM ASTC encoder doesn't seem to support such scenario (the `-array`
option enforces use of a 3D ASTC format), but other tools might.

@m_class{m-block m-warning}

@par Imported image orientation
    Unlike @ref Trade-KtxImporter-behavior "KTX" or
    @ref Trade-BasisImporter-behavior "Basis", the file format doesn't contain
    any orientation metadata, and so it's assumed to follow the Vulkan/D3D
    coordinate system with Y down and (for 3D textures) Z forward. Because
    flipping block-compressed data is nontrivial, the image will not be flipped
    on import, instead a message will be printed to @relativeref{Magnum,Warning}
    and the data will be passed through unchanged. Set the
    @cb{.ini} assumeYUpZBackward @ce @ref Trade-AstcImporter-configuration "configuration option"
    to assume the OpenGL coordinate system and silence the warning.

@section Trade-AstcImporter-configuration Plugin-specific configuration

It's possible to tune various import options through @ref configuration(). See
below for all options and their default values:

@snippet MagnumPlugins/AstcImporter/AstcImporter.conf configuration_

See @ref plugins-configuration for more information and an example showing how
to edit the configuration values.
*/
class MAGNUM_ASTCIMPORTER_EXPORT AstcImporter: public AbstractImporter {
    public:
        /** @brief Plugin manager constructor */
        explicit AstcImporter(PluginManager::AbstractManager& manager, const Containers::StringView& plugin);

        ~AstcImporter();

    private:
        MAGNUM_ASTCIMPORTER_LOCAL ImporterFeatures doFeatures() const override;
        MAGNUM_ASTCIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_ASTCIMPORTER_LOCAL void doClose() override;
        MAGNUM_ASTCIMPORTER_LOCAL void doOpenData(Containers::Array<char>&& data, DataFlags dataFlags) override;

        MAGNUM_ASTCIMPORTER_LOCAL UnsignedInt doImage2DCount() const override;
        MAGNUM_ASTCIMPORTER_LOCAL Containers::Optional<ImageData2D> doImage2D(UnsignedInt id, UnsignedInt level) override;

        MAGNUM_ASTCIMPORTER_LOCAL UnsignedInt doImage3DCount() const override;
        MAGNUM_ASTCIMPORTER_LOCAL Containers::Optional<ImageData3D> doImage3D(UnsignedInt id, UnsignedInt level) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
