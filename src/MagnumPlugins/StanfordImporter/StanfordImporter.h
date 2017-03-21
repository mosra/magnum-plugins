#ifndef Magnum_Trade_StanfordImporter_h
#define Magnum_Trade_StanfordImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017
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

/** @file
 * @brief Class @ref Magnum::Trade::StanfordImporter
 */

#include <memory>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/StanfordImporter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_STANFORDIMPORTER_BUILD_STATIC
    #if defined(StanfordImporter_EXPORTS) || defined(StanfordImporterObjects_EXPORTS)
        #define MAGNUM_STANFORDIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_STANFORDIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_STANFORDIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_STANFORDIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#endif

namespace Magnum { namespace Trade {

/**
@brief Stanford PLY importer plugin

Supports little and big endian binary format (ASCII files are not supported),
triangle/quad meshes. Only vertex positions are imported.

This plugin is built if `WITH_STANFORDIMPORTER` is enabled when building
Magnum Plugins. To use dynamic plugin, you need to load `StanfordImporter`
plugin from `MAGNUM_PLUGINS_IMPORTER_DIR`. To use static plugin, you need to
request `StanfordImporter` component of `MagnumPlugins` package in CMake and
link to `MagnumPlugins::StanfordImporter` target. See @ref building-plugins,
@ref cmake-plugins and @ref plugins for more information.
*/
class MAGNUM_STANFORDIMPORTER_EXPORT StanfordImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit StanfordImporter();

        /** @brief Plugin manager constructor */
        explicit StanfordImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~StanfordImporter();

    private:
        MAGNUM_STANFORDIMPORTER_LOCAL Features doFeatures() const override;

        MAGNUM_STANFORDIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_STANFORDIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char> data) override;
        MAGNUM_STANFORDIMPORTER_LOCAL void doOpenFile(const std::string& filename) override;
        MAGNUM_STANFORDIMPORTER_LOCAL void doClose() override;

        MAGNUM_STANFORDIMPORTER_LOCAL UnsignedInt doMesh3DCount() const override;
        MAGNUM_STANFORDIMPORTER_LOCAL std::optional<MeshData3D> doMesh3D(UnsignedInt id) override;

        std::unique_ptr<std::istream> _in;
};

}}

#endif
