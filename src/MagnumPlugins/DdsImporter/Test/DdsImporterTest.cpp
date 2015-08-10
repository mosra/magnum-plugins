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

#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/ColorFormat.h>
#include <Magnum/Trade/ImageData.h>

#include "MagnumPlugins/DdsImporter/DdsImporter.h"

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test {

struct DdsImporterTest: TestSuite::Tester {
    explicit DdsImporterTest();

    void testUncompressedRgb();
    void testUncompressedRgbWithMips();
    void testUncompressedRgbVolume();
    void testDxt1Compressed();
    void testDxt3Compressed();
    void testDxt5Compressed();
};

DdsImporterTest::DdsImporterTest() {
    addTests({&DdsImporterTest::testUncompressedRgb});
    addTests({&DdsImporterTest::testUncompressedRgbWithMips});
    addTests({&DdsImporterTest::testUncompressedRgbVolume});
    addTests({&DdsImporterTest::testDxt1Compressed});
    addTests({&DdsImporterTest::testDxt3Compressed});
    addTests({&DdsImporterTest::testDxt5Compressed});
}

void DdsImporterTest::testUncompressedRgb() {
    DdsImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(DDSIMPORTER_TEST_DIR, "rgb_uncompressed.dds")));

    const char pixels[] = {'\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77',
                           '\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77',
                           '\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77'};

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), ColorFormat::RGB);
    CORRADE_COMPARE(image->type(), ColorType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), Containers::ArrayView<const char>(pixels),
        TestSuite::Compare::Container);
}

void DdsImporterTest::testUncompressedRgbWithMips() {
    DdsImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(DDSIMPORTER_TEST_DIR, "rgb_uncompressed_mips.dds")));

    const char pixels[] = {'\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77',
                           '\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77',
                           '\xde', '\xad', '\xb5',
                           '\xca', '\xfe', '\x77'};
    const char mipPixels[] = {'\xd4', '\xd5', '\x96'};

    /* check image */
    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_COMPARE(image->format(), ColorFormat::RGB);
    CORRADE_COMPARE(image->type(), ColorType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), Containers::ArrayView<const char>(pixels),
            TestSuite::Compare::Container);

    /* check mip 0 */
    std::optional<Trade::ImageData2D> mip = importer.image2D(1);
    CORRADE_VERIFY(mip);
    CORRADE_COMPARE(mip->size(), Vector2i(3, 2));
    CORRADE_COMPARE(mip->format(), ColorFormat::RGB);
    CORRADE_COMPARE(mip->type(), ColorType::UnsignedByte);
    CORRADE_COMPARE_AS(mip->data(), Containers::ArrayView<const char>(mipPixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::testUncompressedRgbVolume() {
    DdsImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(DDSIMPORTER_TEST_DIR, "rgb_uncompressed_volume.dds")));

    const char pixels[] = {
        /* slice 0 */
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        /* slice 1 */
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        /* slice 2 */
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77',
        '\xde', '\xad', '\xb5',
        '\xca', '\xfe', '\x77'};

    std::optional<Trade::ImageData3D> image = importer.image3D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector3i(3, 2, 3));
    CORRADE_COMPARE(image->format(), ColorFormat::RGB);
    CORRADE_COMPARE(image->type(), ColorType::UnsignedByte);
    CORRADE_COMPARE_AS(image->data(), Containers::ArrayView<const char>(pixels),
        TestSuite::Compare::Container);
}


void DdsImporterTest::testDxt1Compressed() {
    DdsImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(DDSIMPORTER_TEST_DIR, "rgba_dxt1.dds")));

    const char pixels[] = {'\x76', '\xdd', '\xee', '\xcf', '\x04', '\x51', '\x04', '\x51'};

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->compressedFormat(), CompressedColorFormat::RGBAS3tcDxt1);
    CORRADE_COMPARE_AS(image->data(), Containers::ArrayView<const char>(pixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::testDxt3Compressed() {
    DdsImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(DDSIMPORTER_TEST_DIR, "rgba_dxt3.dds")));

    const char pixels[] = {'\xff', '\xff', '\xff', '\xff', '\xff', '\xff', '\xff', '\xff',
                           '\x76', '\xdd', '\xee', '\xcf', '\x04', '\x51', '\x04', '\x51'};

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->compressedFormat(), CompressedColorFormat::RGBAS3tcDxt3);
    CORRADE_COMPARE_AS(image->data(), Containers::ArrayView<const char>(pixels),
            TestSuite::Compare::Container);
}

void DdsImporterTest::testDxt5Compressed() {
    DdsImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(DDSIMPORTER_TEST_DIR, "rgba_dxt5.dds")));

    const char pixels[] = {'\xff', '\xff', '\x49', '\x92', '\x24', '\x49', '\x92', '\x24',
                           '\x76', '\xdd', '\xee', '\xcf', '\x04', '\x51', '\x04', '\x51'};

    std::optional<Trade::ImageData2D> image = importer.image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), Vector2i(3, 2));
    CORRADE_VERIFY(image->isCompressed());
    CORRADE_COMPARE(image->compressedFormat(), CompressedColorFormat::RGBAS3tcDxt5);
    CORRADE_COMPARE_AS(image->data(), Containers::ArrayView<const char>(pixels),
            TestSuite::Compare::Container);
}

}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::DdsImporterTest)
