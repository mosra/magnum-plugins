/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2016 Alice Margatroid <loveoverwhelming@gmail.com>

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
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Directory.h>

#include "MagnumPlugins/DrFlacAudioImporter/DrFlacImporter.h"

#include "configure.h"

namespace Magnum { namespace Audio { namespace Test {

class DrFlacImporterTest: public TestSuite::Tester {
    public:
        explicit DrFlacImporterTest();

        void mono8();
        void mono16();
        void stereo16();
};

DrFlacImporterTest::DrFlacImporterTest() {
    addTests({
              &DrFlacImporterTest::mono8,
              &DrFlacImporterTest::mono16,
              &DrFlacImporterTest::stereo16});
}

void DrFlacImporterTest::mono8() {
    DrFlacImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "mono8.flac")));

    CORRADE_COMPARE(importer.format(), Buffer::Format::Mono8);
    CORRADE_COMPARE(importer.frequency(), 22254);
}

void DrFlacImporterTest::mono16() {
    DrFlacImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "mono16.flac")));

    CORRADE_COMPARE(importer.format(), Buffer::Format::Mono16);
    CORRADE_COMPARE(importer.frequency(), 44100);
}

void DrFlacImporterTest::stereo16() {
    DrFlacImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "stereo16.flac")));

    CORRADE_COMPARE(importer.format(), Buffer::Format::Stereo16);
    CORRADE_COMPARE(importer.frequency(), 44100);
}


}}}

CORRADE_TEST_MAIN(Magnum::Audio::Test::DrFlacImporterTest)
