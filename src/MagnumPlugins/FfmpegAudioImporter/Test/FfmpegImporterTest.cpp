/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018
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
#include <Corrade/Containers/Array.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Audio/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Audio { namespace Test {

class FfmpegImporterTest: public TestSuite::Tester {
    public:
        explicit FfmpegImporterTest();

        void wrongSize();
        void wrongSignature();
        void unsupportedChannelCount();
        void mono16();
        void stereo8();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

FfmpegImporterTest::FfmpegImporterTest() {
    addTests({&FfmpegImporterTest::wrongSignature,
              &FfmpegImporterTest::unsupportedChannelCount,
              &FfmpegImporterTest::mono16,
              &FfmpegImporterTest::stereo8});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef FFMPEGAUDIOIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(FFMPEGAUDIOIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void FfmpegImporterTest::wrongSignature() {
    std::ostringstream out;
    Error redirectError{&out};

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("FfmpegAudioImporter");
    CORRADE_VERIFY(!importer->openFile(WRONG_SIGNATURE_OGG_FILE));
    CORRADE_COMPARE(out.str(), "Audio::FfmpegImporter::openData(): the file signature is invalid\n");
}

void FfmpegImporterTest::unsupportedChannelCount() {
    std::ostringstream out;
    Error redirectError{&out};

    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("FfmpegAudioImporter");
    CORRADE_VERIFY(!importer->openFile(UNSUPPORTED_CHANNEL_COUNT_OGG_FILE));
    CORRADE_COMPARE(out.str(), "Audio::FfmpegImporter::openData(): unsupported channel count 5 with 16 bits per sample\n");
}

void FfmpegImporterTest::mono16() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("FfmpegAudioImporter");
    CORRADE_VERIFY(importer->openFile(MONO16_OGG_FILE));

    CORRADE_COMPARE(importer->format(), BufferFormat::MonoFloat); // TODO: why not 16
    CORRADE_COMPARE(importer->frequency(), 96000);
    CORRADE_COMPARE_AS(importer->data(),
        (Containers::Array<char>{Containers::InPlaceInit, {
            '\xcd', '\x0a', '\x2b', '\x0a'}}),
        TestSuite::Compare::Container);
}

void FfmpegImporterTest::stereo8() {
    std::unique_ptr<AbstractImporter> importer = _manager.instantiate("FfmpegAudioImporter");
    CORRADE_VERIFY(importer->openFile(STEREO8_OGG_FILE));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoFloat); // TODO: why not 16
    CORRADE_COMPARE(importer->frequency(), 96000);
    CORRADE_COMPARE_AS(importer->data(),
        (Containers::Array<char>{Containers::InPlaceInit, {
            '\x3e', '\x19', '\x1d', '\x17'}}),
        TestSuite::Compare::Container);
}

}}}

CORRADE_TEST_MAIN(Magnum::Audio::Test::FfmpegImporterTest)
