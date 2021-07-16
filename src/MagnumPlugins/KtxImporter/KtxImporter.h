#ifndef Magnum_Trade_KtxImporter_h
#define Magnum_Trade_KtxImporter_h
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

/** @file
 * @brief Class @ref Magnum::Trade::KtxImporter
 */

#include <Corrade/Containers/Array.h>
#include <Corrade/Utility/VisibilityMacros.h>

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

class MAGNUM_KTXIMPORTER_EXPORT KtxImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit KtxImporter();

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

    private:
        struct File;
        Containers::Pointer<File> _f;
};

}}

#endif
