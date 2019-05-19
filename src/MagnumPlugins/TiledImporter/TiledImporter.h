#ifndef Magnum_Trade_TILEDIMPORTER_h
#define Magnum_Trade_TILEDIMPORTER_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
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
 * @brief Class @ref Magnum::Trade::TILEDIMPORTER
 */

#include <Corrade/Containers/Array.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "MagnumPlugins/TiledImporter/configure.h"
#include "pugixml.hpp"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_TILEDIMPORTER_BUILD_STATIC
    #ifdef TiledImporter_EXPORTS
        #define MAGNUM_TILEDIMPORTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_TILEDIMPORTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_TILEDIMPORTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_TILEDIMPORTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_TILEDIMPORTER_EXPORT
#define MAGNUM_TILEDIMPORTER_LOCAL
#endif

namespace Magnum { namespace Trade {


struct TiledTilemapData;

/**
@brief Tile map importer plugin using stb_image
*/
class MAGNUM_TILEDIMPORTER_EXPORT TiledImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit TiledImporter();

        /** @brief Plugin manager constructor */
        explicit TiledImporter(PluginManager::AbstractManager& manager, const std::string& plugin);

        ~TiledImporter();

        const void* doImporterState() const {
            return _data.get();
        }

    private:
        MAGNUM_TILEDIMPORTER_LOCAL Features doFeatures() const override;
        MAGNUM_TILEDIMPORTER_LOCAL bool doIsOpened() const override;
        MAGNUM_TILEDIMPORTER_LOCAL void doClose() override;
        MAGNUM_TILEDIMPORTER_LOCAL void doOpenData(Containers::ArrayView<const char>) override;
        MAGNUM_TILEDIMPORTER_LOCAL void doOpenFile(const std::string& filename);

        std::string _currentFilename;
        pugi::xml_document _doc; // TODO Maybe move this inside the definitions
        Containers::Pointer<TiledTilemapData> _data;
};

}}

#endif

