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

#include <sstream>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Directory.h>

#include "MagnumPlugins/StbVorbisAudioImporter/StbVorbisImporter.h"

#include "configure.h"

namespace Magnum { namespace Audio { namespace Test {

class StbVorbisImporterTest: public TestSuite::Tester {
    public:
        explicit StbVorbisImporterTest();

        void wrongSize();
        void wrongSignature();
        void unsupportedChannelCount();
        void mono16();
        void stereo8();
};

StbVorbisImporterTest::StbVorbisImporterTest() {
    addTests({&StbVorbisImporterTest::wrongSignature,
              &StbVorbisImporterTest::unsupportedChannelCount,
              &StbVorbisImporterTest::mono16,
              &StbVorbisImporterTest::stereo8});
}

void StbVorbisImporterTest::wrongSignature() {
    std::ostringstream out;
    Error::setOutput(&out);

    StbVorbisImporter importer;
    CORRADE_VERIFY(!importer.openFile(Utility::Directory::join(STBVORBISIMPORTER_TEST_DIR, "wrongSignature.ogg")));
    CORRADE_COMPARE(out.str(), "Audio::StbVorbisImporter::openData(): the file signature is invalid\n");
}

void StbVorbisImporterTest::unsupportedChannelCount() {
    std::ostringstream out;
    Error::setOutput(&out);

    StbVorbisImporter importer;
    CORRADE_VERIFY(!importer.openFile(Utility::Directory::join(STBVORBISIMPORTER_TEST_DIR, "unsupportedChannelCount.ogg")));
    CORRADE_COMPARE(out.str(), "Audio::StbVorbisImporter::openData(): unsupported channel count 6 with 16 bits per sample\n");
}

void StbVorbisImporterTest::mono16() {
    StbVorbisImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(STBVORBISIMPORTER_TEST_DIR, "mono16.ogg")));

    CORRADE_COMPARE(importer.format(), Buffer::Format::Mono16);
    CORRADE_COMPARE(importer.frequency(), 96000);
    CORRADE_COMPARE_AS(importer.data(),
        Containers::Array<char>::from('\xcd', '\x0a', '\x2b', '\x0a'),
        TestSuite::Compare::Container);
}

void StbVorbisImporterTest::stereo8() {
    StbVorbisImporter importer;
    CORRADE_VERIFY(importer.openFile(Utility::Directory::join(STBVORBISIMPORTER_TEST_DIR, "stereo8.ogg")));

    CORRADE_COMPARE(importer.format(), Buffer::Format::Stereo16);
    CORRADE_COMPARE(importer.frequency(), 96000);
    CORRADE_COMPARE_AS(importer.data(),
        Containers::Array<char>::from('\x3e', '\x19', '\x1d', '\x17'),
        TestSuite::Compare::Container);
}

}}}

CORRADE_TEST_MAIN(Magnum::Audio::Test::StbVorbisImporterTest)
