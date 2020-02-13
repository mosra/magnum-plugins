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

#include <sstream>
#include <Corrade/Containers/Optional.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "configure.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct IcoImporterTest: TestSuite::Tester {
    explicit IcoImporterTest();

    //void bmp_1bpp_1ba_2sp();
    //void bmp_4bpp_1ba_16sp();
    //void bmp_8bpp_1ba_256sp();
    //void bmp_24bpp_1ba_np();
    //void bmp_32bpp_8ba_np();
    void bmp();
    void png();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

IcoImporterTest::IcoImporterTest() {

    addTests({
         //&IcoImporterTest::bmp_1bpp_1ba_2sp,
         //&IcoImporterTest::bmp_4bpp_1ba_16sp,
         //&IcoImporterTest::bmp_8bpp_1ba_256sp,
         //&IcoImporterTest::bmp_24bpp_1ba_np,
         //&IcoImporterTest::bmp_32bpp_8ba_np,
         &IcoImporterTest::bmp,
         &IcoImporterTest::png
    });

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef ICOIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(ICOIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}
/*
void IcoImporterTest::bmp_1bpp_1ba_2sp() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "stone.ico")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), (Vector2i{8, 8}));
}

void IcoImporterTest::bmp_4bpp_1ba_16sp() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "stone.ico")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(1);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), (Vector2i{16, 16}));
}

void IcoImporterTest::bmp_8bpp_1ba_256sp() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "stone.ico")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(2);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), (Vector2i{32, 32}));
}

void IcoImporterTest::bmp_24bpp_1ba_np() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "stone.ico")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(3);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), (Vector2i{64, 64}));
}

void IcoImporterTest::bmp_32bpp_8ba_np() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "stone.ico")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(4);

    CORRADE_VERIFY(image);
    //CORRADE_COMPARE(image->size(), (Vector2i{128, 128}));
}
*/
void IcoImporterTest::bmp() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "stone.ico")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(4);

    CORRADE_EXPECT_FAIL("IcoImporter does not support BMPs yet.");
    CORRADE_VERIFY(image);
    //CORRADE_COMPARE(image->size(), (Vector2i{128, 128}));
}

void IcoImporterTest::png() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("IcoImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(ICOIMPORTER_TEST_DIR, "stone.ico")));

    Containers::Optional<Trade::ImageData2D> image = importer->image2D(5);
    CORRADE_VERIFY(image);
    CORRADE_COMPARE(image->size(), (Vector2i{256, 256}));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::IcoImporterTest)
