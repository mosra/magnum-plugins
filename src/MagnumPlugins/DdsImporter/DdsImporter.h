#ifndef Magnum_Trade_DdsImporter_h
#define Magnum_Trade_DdsImporter_h
/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2015 Jonathan Hale <squareys@googlemail.com>

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
 * @brief Class @ref Magnum::Trade::DdsImporter
 */
#include <vector>
#include <Magnum/Trade/AbstractImporter.h>

namespace Magnum { namespace Trade {

/**
@brief DDS importer plugin

*/
class DdsImporter: public AbstractImporter {
    public:
        /** @brief Default constructor */
        explicit DdsImporter();

        /** @brief Plugin manager constructor */
        explicit DdsImporter(PluginManager::AbstractManager& manager, std::string plugin);

        ~DdsImporter();

    private:
        Features doFeatures() const override;
        bool doIsOpened() const override;
        void doClose() override;
        void doOpenData(Containers::ArrayView<const char> data) override;
        void doOpenFile(const std::string& filename) override;

        UnsignedInt doImage2DCount() const override;
        std::optional<ImageData2D> doImage2D(UnsignedInt id) override;

        UnsignedInt doImage3DCount() const override;
        std::optional<ImageData3D> doImage3D(UnsignedInt id) override;

    private:
        void loadUncompressedImageData(ColorFormat format, const Vector3i& dims, UnsignedInt components,
            UnsignedByte numDimensions);
        void loadCompressedImageData(CompressedColorFormat format, const Vector3i& dims,
            UnsignedByte numDimensions);

        std::istream* _in;

        std::vector<ImageData2D> _imageData2D;
        std::vector<ImageData3D> _imageData3D;
};

}}

#endif
