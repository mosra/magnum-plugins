/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2016 Alice Margatroid <loveoverwhelming@gmail.com>
    Copyright © 2019 Guillaume Jacquemin <williamjcm@users.noreply.github.com>

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
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringStl.h> /** @todo remove once AbstractImporter is <string>-free */
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/Audio/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Audio { namespace Test { namespace {

struct DrWavImporterTest: TestSuite::Tester {
    explicit DrWavImporterTest();

    void empty();
    void wrongSignature();
    void unsupportedChannelCount();
    void unsupportedBitRate();
    void invalidPadding();
    void invalidLength();
    void invalidDataChunk();
    void invalidFactChunk();

    void zeroSamples();

    void mono8();
    void mono8ALaw();
    void mono8MuLaw();
    void mono16();
    void mono24();

    void stereo8();
    void stereo8ALaw();
    void stereo8MuLaw();
    void stereo12();
    void stereo16();
    void stereo24();
    void stereo32();

    void surround51Channel16();
    void surround71Channel24();

    void mono32f();
    void stereo32f();
    void stereo64f();

    void extensionsALaw();
    void extensionsMuLaw();

    void extensions12();
    void extensions16();
    void extensions24();
    void extensions32();

    void extensions32f();
    void extensions64f();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

DrWavImporterTest::DrWavImporterTest() {
    addTests({&DrWavImporterTest::empty,
              &DrWavImporterTest::wrongSignature,
              &DrWavImporterTest::unsupportedChannelCount,
              &DrWavImporterTest::unsupportedBitRate,

              &DrWavImporterTest::invalidPadding,
              &DrWavImporterTest::invalidLength,
              &DrWavImporterTest::invalidDataChunk,
              &DrWavImporterTest::invalidFactChunk,

              &DrWavImporterTest::zeroSamples,

              &DrWavImporterTest::mono8,
              &DrWavImporterTest::mono8ALaw,
              &DrWavImporterTest::mono8MuLaw,
              &DrWavImporterTest::mono16,
              &DrWavImporterTest::mono24,

              &DrWavImporterTest::stereo8,
              &DrWavImporterTest::stereo8ALaw,
              &DrWavImporterTest::stereo8MuLaw,
              &DrWavImporterTest::stereo12,
              &DrWavImporterTest::stereo16,
              &DrWavImporterTest::stereo24,
              &DrWavImporterTest::stereo32,

              &DrWavImporterTest::surround51Channel16,
              &DrWavImporterTest::surround71Channel24,

              &DrWavImporterTest::mono32f,
              &DrWavImporterTest::stereo32f,
              &DrWavImporterTest::stereo64f,

              &DrWavImporterTest::extensionsALaw,
              &DrWavImporterTest::extensionsMuLaw,

              &DrWavImporterTest::extensions12,
              &DrWavImporterTest::extensions16,
              &DrWavImporterTest::extensions24,
              &DrWavImporterTest::extensions32,

              &DrWavImporterTest::extensions32f,
              &DrWavImporterTest::extensions64f});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef DRWAVAUDIOIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(DRWAVAUDIOIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void DrWavImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");

    std::ostringstream out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out.str(), "Audio::DrWavImporter::openData(): failed to open and decode WAV data\n");
}

void DrWavImporterTest::wrongSignature() {
    std::ostringstream out;
    Error redirectError{&out};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "wrongSignature.wav")));
    CORRADE_COMPARE(out.str(), "Audio::DrWavImporter::openData(): failed to open and decode WAV data\n");
}

void DrWavImporterTest::unsupportedChannelCount() {
    std::ostringstream out;
    Error redirectError{&out};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "unsupportedChannelCount.wav")));
    CORRADE_COMPARE(out.str(), "Audio::DrWavImporter::openData(): unsupported channel count 3 with 8 bits per sample\n");
}

void DrWavImporterTest::unsupportedBitRate() {
    std::ostringstream out;
    Error redirectError{&out};

    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "unsupportedBitRate.wav")));
    CORRADE_COMPARE(out.str(), "Audio::DrWavImporter::openData(): unsupported channel count 1 with 80 bits per sample\n");
}

void DrWavImporterTest::invalidPadding() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "invalidPadding.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::MonoMuLaw);
    CORRADE_COMPARE(importer->frequency(), 8000);
}

void DrWavImporterTest::invalidLength() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "invalidLength.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Mono16);
    CORRADE_COMPARE(importer->frequency(), 8000);
}

void DrWavImporterTest::invalidDataChunk() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "invalidDataChunk.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::MonoMuLaw);
    CORRADE_COMPARE(importer->frequency(), 8000);
}

void DrWavImporterTest::invalidFactChunk() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "invalidFactChunk.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Mono16);
    CORRADE_COMPARE(importer->frequency(), 22050);

    CORRADE_COMPARE(importer->data().size(), 3724);
    CORRADE_COMPARE_AS(importer->data().prefix(8), Containers::arrayView({
        '\xe5', '\xf5', '\xff', '\xf7', '\x18', '\xfa', '\x7f', '\xfb'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::zeroSamples() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");

    /* No error should happen, it should just give an empty buffer back */
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "zeroSamples.wav")));
    CORRADE_COMPARE(importer->format(), BufferFormat::MonoFloat);
    CORRADE_COMPARE(importer->frequency(), 44000);
    CORRADE_VERIFY(importer->data().isEmpty());
}

void DrWavImporterTest::mono8() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "mono8.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Mono8);
    CORRADE_COMPARE(importer->frequency(), 22050);

    CORRADE_COMPARE(importer->data().size(), 2136);
    CORRADE_COMPARE_AS(importer->data().prefix(4), Containers::arrayView({
        '\x7f', '\x7f', '\x7f', '\x7f'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::mono8ALaw() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "mono8ALaw.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::MonoALaw);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 4096);
    CORRADE_COMPARE_AS(importer->data().prefix(8), Containers::arrayView({
        '\x57', '\x54', '\x55', '\x55', '\x55', '\xd5', '\xd5', '\xd5'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::mono8MuLaw() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "mono8MuLaw.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::MonoMuLaw);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 4096);
    CORRADE_COMPARE_AS(importer->data().prefix(8), Containers::arrayView({
        '\xfb', '\xfd', '\xff', '\xfe', '\xff', '\x7f', '\x7f', '\x7e'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::mono16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "mono16.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Mono16);
    CORRADE_COMPARE(importer->frequency(), 44000);

    CORRADE_COMPARE_AS(importer->data(), Containers::arrayView({
        '\x1d', '\x10', '\x71', '\xc5'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::mono24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "mono24.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::MonoFloat);
    CORRADE_COMPARE(importer->frequency(), 48000);

    CORRADE_COMPARE(importer->data().size(), 3696);
    CORRADE_COMPARE_AS(importer->data().prefix(8), Containers::arrayView({
        '\x0', '\xc8', '\xf', '\xba', '\x0', '\x74', '\xbc', '\xba'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::stereo8() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "stereo8.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo8);
    CORRADE_COMPARE(importer->frequency(), 96000);

    CORRADE_COMPARE_AS(importer->data(), Containers::arrayView({
        '\xde', '\xfe', '\xca', '\x7e'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::stereo8ALaw() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "stereo8ALaw.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoALaw);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 4096);
    CORRADE_COMPARE_AS(importer->data().prefix(8), Containers::arrayView({
        '\xd5', '\xd5', '\xd5', '\xd5', '\xd5', '\xd5', '\x55', '\xd5'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::stereo8MuLaw() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "stereo8MuLaw.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoMuLaw);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 4096);
    CORRADE_COMPARE_AS(importer->data().prefix(8), Containers::arrayView({
        '\xff', '\xff', '\xff', '\xff', '\xff', '\xff', '\x7f', '\xff'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::stereo12() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "stereo12.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo16);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE_AS(importer->data().prefix(32), Containers::arrayView({
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x01', '\x00', '\x02', '\x00', '\xfd', '\xff', '\x00', '\x00',
        '\x00', '\x00', '\xfc', '\xff', '\x06', '\x00', '\x04', '\x00',
        '\xfe', '\xff', '\x01', '\x00', '\xfe', '\xff', '\xfe', '\xff'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::stereo16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "stereo16.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo16);
    CORRADE_COMPARE(importer->frequency(), 44100);

    CORRADE_COMPARE_AS(importer->data(), Containers::arrayView({
        '\x27', '\x4f', '\x27', '\x4f'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::stereo24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "stereo24.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoFloat);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 187944);
    CORRADE_COMPARE_AS(importer->data().prefix(32), Containers::arrayView({
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x38', '\x00', '\x00', '\x80', '\x38',
        '\x00', '\x00', '\xc0', '\xb8', '\x00', '\x00', '\x00', '\x00'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::stereo32() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "stereo32.wav")));

    {
        CORRADE_EXPECT_FAIL("Importing 32-bit format as Double is not yet implemented.");
        CORRADE_COMPARE(importer->format(), BufferFormat::StereoDouble);
    }
    CORRADE_COMPARE(importer->format(), BufferFormat::StereoFloat);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 187944);
    CORRADE_COMPARE_AS(importer->data().prefix(32), Containers::arrayView({
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x38', '\x00', '\x00', '\x80', '\x38',
        '\x00', '\x00', '\xc0', '\xb8', '\x00', '\x00', '\x00', '\x00'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::surround51Channel16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "surround51Channel16.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Surround51Channel16);
    CORRADE_COMPARE(importer->frequency(), 48000);
}

void DrWavImporterTest::surround71Channel24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "surround71Channel24.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Surround71Channel32);
    CORRADE_COMPARE(importer->frequency(), 48000);
}

void DrWavImporterTest::mono32f() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "mono32f.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::MonoFloat);
    CORRADE_COMPARE(importer->frequency(), 48000);

    CORRADE_COMPARE(importer->data().size(), 3920);
    CORRADE_COMPARE_AS(importer->data().prefix(16), Containers::arrayView({
        '\x00', '\x00', '\x00', '\x00', '\x6c', '\x39', '\x99', '\x3b',
        '\x03', '\x3f', '\x2a', '\x3c', '\xdf', '\xaf', '\x88', '\x3c'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::stereo32f() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "stereo32f.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoFloat);
    CORRADE_COMPARE(importer->frequency(), 44100);

    CORRADE_COMPARE(importer->data().size(), 1352);
    CORRADE_COMPARE_AS(importer->data().prefix(8), Containers::arrayView({
        '\x11', '\xb3', '\x99', '\x38', '\x05', '\x32', '\x48', '\x38'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::stereo64f() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "stereo64f.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoDouble);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 375888);
    CORRADE_COMPARE_AS(importer->data().prefix(64), Containers::arrayView({
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x3f',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x10', '\x3f',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x18', '\xbf',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::extensionsALaw() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "extensionALaw.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoALaw);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 46986);
    CORRADE_COMPARE_AS(importer->data().prefix(8), Containers::arrayView({
        '\xd5', '\xd5', '\xd5', '\xd5', '\xd5', '\xd5', '\x55', '\xd5'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::extensionsMuLaw() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "extensionMuLaw.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoMuLaw);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 46986);
    CORRADE_COMPARE_AS(importer->data().prefix(8), Containers::arrayView({
        '\xff', '\xff', '\xff', '\xff', '\xff', '\xff', '\x7f', '\xff'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::extensions12() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "extension12.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo16);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 93972);
    CORRADE_COMPARE_AS(importer->data().prefix(16), Containers::arrayView({
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x01', '\x00', '\x02', '\x00', '\xfd', '\xff', '\x00', '\x00'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::extensions16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "extension16.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo16);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 93972);
    CORRADE_COMPARE_AS(importer->data().prefix(16), Containers::arrayView({
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x01', '\x00', '\x02', '\x00', '\xfd', '\xff', '\x00', '\x00'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::extensions24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "extension24.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoFloat);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 187944);
    CORRADE_COMPARE_AS(importer->data().prefix(32), Containers::arrayView({
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x38', '\x00', '\x00', '\x80', '\x38',
        '\x00', '\x00', '\xc0', '\xb8', '\x00', '\x00', '\x00', '\x00'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::extensions32() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "extension32.wav")));

    {
        CORRADE_EXPECT_FAIL("Importing 32-bit format as Double is not yet implemented.");
        CORRADE_COMPARE(importer->format(), BufferFormat::StereoDouble);
    }
    CORRADE_COMPARE(importer->format(), BufferFormat::StereoFloat);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 187944);
    CORRADE_COMPARE_AS(importer->data().prefix(32), Containers::arrayView({
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x38', '\x00', '\x00', '\x80', '\x38',
        '\x00', '\x00', '\xc0', '\xb8', '\x00', '\x00', '\x00', '\x00'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::extensions32f() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "extension32f.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoFloat);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 187944);
    CORRADE_COMPARE_AS(importer->data().prefix(32), Containers::arrayView({
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x38', '\x00', '\x00', '\x80', '\x38',
        '\x00', '\x00', '\xc0', '\xb8', '\x00', '\x00', '\x00', '\x00'
    }), TestSuite::Compare::Container);
}

void DrWavImporterTest::extensions64f() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrWavAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRWAVAUDIOIMPORTER_TEST_DIR, "extension64f.wav")));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoDouble);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 375888);
    CORRADE_COMPARE_AS(importer->data().prefix(64), Containers::arrayView({
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x3f',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x10', '\x3f',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x18', '\xbf',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'
    }), TestSuite::Compare::Container);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Audio::Test::DrWavImporterTest)
