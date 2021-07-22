#ifndef Magnum_Trade_KtxImageConverter_h
#define Magnum_Trade_KtxImageConverter_h
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

class MAGNUM_KTXIMAGECONVERTER_EXPORT KtxImageConverter: public AbstractImageConverter {
    public:
        /** @brief Default constructor */
        explicit KtxImageConverter();

        /** @brief Plugin manager constructor */
        explicit KtxImageConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        ImageConverterFeatures MAGNUM_KTXIMAGECONVERTER_LOCAL doFeatures() const override;

        Containers::Array<char> MAGNUM_KTXIMAGECONVERTER_LOCAL doConvertToData(const ImageView1D& image) override;
        Containers::Array<char> MAGNUM_KTXIMAGECONVERTER_LOCAL doConvertToData(const ImageView2D& image) override;
        Containers::Array<char> MAGNUM_KTXIMAGECONVERTER_LOCAL doConvertToData(const ImageView3D& image) override;

        Containers::Array<char> MAGNUM_KTXIMAGECONVERTER_LOCAL doConvertToData(const CompressedImageView1D& image) override;
        Containers::Array<char> MAGNUM_KTXIMAGECONVERTER_LOCAL doConvertToData(const CompressedImageView2D& image) override;
        Containers::Array<char> MAGNUM_KTXIMAGECONVERTER_LOCAL doConvertToData(const CompressedImageView3D& image) override;
};

}}

#endif
