/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025
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

#include <string> /** @todo remove once AbstractImporter is <string>-free */
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringStl.h> /** @todo remove once AbstractImporter is <string>-free */
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Audio/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Audio { namespace Test { namespace {

struct DrFlacImporterTest: TestSuite::Tester {
    explicit DrFlacImporterTest();

    void empty();

    void zeroSamples();

    void mono8();
    void mono16();
    void mono24();
    void mono32();

    void stereo8();
    void stereo16();
    void stereo24();

    void quad16();
    void quad24();

    void surround51Channel16();
    void surround51Channel24();

    void surround71Channel24();

    void openTwice();
    void importTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

DrFlacImporterTest::DrFlacImporterTest() {
    addTests({&DrFlacImporterTest::empty,

              &DrFlacImporterTest::zeroSamples,

              &DrFlacImporterTest::mono8,
              &DrFlacImporterTest::mono16,
              &DrFlacImporterTest::mono24,
              &DrFlacImporterTest::mono32,

              &DrFlacImporterTest::stereo8,
              &DrFlacImporterTest::stereo16,
              &DrFlacImporterTest::stereo24,

              &DrFlacImporterTest::quad16,
              &DrFlacImporterTest::quad24,

              &DrFlacImporterTest::surround51Channel16,
              &DrFlacImporterTest::surround51Channel24,

              &DrFlacImporterTest::surround71Channel24,

              &DrFlacImporterTest::openTwice,
              &DrFlacImporterTest::importTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef DRFLACAUDIOIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(DRFLACAUDIOIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void DrFlacImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");

    Containers::String out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out, "Audio::DrFlacImporter::openData(): failed to open and decode FLAC data\n");
}

void DrFlacImporterTest::zeroSamples() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");

    /* No error should happen, it should just give an empty buffer back */
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "zeroSamples.flac")));
    CORRADE_COMPARE(importer->format(), BufferFormat::Mono16);
    CORRADE_COMPARE(importer->frequency(), 22050);
    CORRADE_VERIFY(importer->data().isEmpty());
}

void DrFlacImporterTest::mono8() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "mono8.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Mono8);
    CORRADE_COMPARE(importer->frequency(), 22050);

    Containers::Array<char> data = importer->data();
    CORRADE_COMPARE(data.size(), 2136);
    CORRADE_COMPARE_AS(Containers::arrayCast<UnsignedByte>(data).prefix(4),
        Containers::arrayView<UnsignedByte>({
            127, 127, 127, 127
        }), TestSuite::Compare::Container);
}

void DrFlacImporterTest::mono16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "mono16.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Mono16);
    CORRADE_COMPARE(importer->frequency(), 44000);

    Containers::Array<char> data = importer->data();
    CORRADE_COMPARE_AS(Containers::arrayCast<Short>(data),
        Containers::arrayView<Short>({
            4125, -14991
        }), TestSuite::Compare::Container);
}

void DrFlacImporterTest::mono24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "mono24.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::MonoFloat);
    CORRADE_COMPARE(importer->frequency(), 48000);

    Containers::Array<char> data = importer->data();
    CORRADE_COMPARE(data.size(), 3696);
    #if defined(CORRADE_TARGET_ARM) && !defined(CORRADE_TARGET_APPLE)
    CORRADE_EXPECT_FAIL("Gives a wrong result on ARM, but only on Linux and not on Mac.");
    #endif
    CORRADE_COMPARE_AS(Containers::arrayCast<Float>(data).prefix(4),
        Containers::arrayView<Float>({
            -0.000548482f, -3.45707e-06f, -0.00179672f, 0.000154614f
        }), TestSuite::Compare::Container);
}

void DrFlacImporterTest::mono32() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "mono32.flac")));
    CORRADE_COMPARE(out, "Audio::DrFlacImporter::openData(): unsupported channel count 1 with 32 bits per sample\n");
}

void DrFlacImporterTest::stereo8() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "stereo8.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo8);
    CORRADE_COMPARE(importer->frequency(), 96000);

    Containers::Array<char> data = importer->data();
    CORRADE_COMPARE_AS(Containers::arrayCast<UnsignedByte>(data),
        Containers::arrayView<UnsignedByte>({
            0xde, 0xfe, 0xca, 0x7e
        }), TestSuite::Compare::Container);
}

void DrFlacImporterTest::stereo16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "stereo16.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo16);
    CORRADE_COMPARE(importer->frequency(), 44100);

    Containers::Array<char> data = importer->data();
    CORRADE_COMPARE_AS(Containers::arrayCast<Short>(data),
        Containers::arrayView<Short>({
            20263, 20263
        }), TestSuite::Compare::Container);
}

void DrFlacImporterTest::stereo24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "stereo24.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoFloat);
    CORRADE_COMPARE(importer->frequency(), 8000);

    Containers::Array<char> data = importer->data();
    CORRADE_COMPARE(data.size(), 187944);
    /** @todo find some range that isn't mostly zeros */
    CORRADE_COMPARE_AS(data.prefix(32), Containers::arrayView({
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
        '\x00', '\x00', '\x00', '\x38', '\x00', '\x00', '\x80', '\x38',
        '\x00', '\x00', '\xc0', '\xb8', '\x00', '\x00', '\x00', '\x00'
    }), TestSuite::Compare::Container);
}

void DrFlacImporterTest::quad16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "quad16.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Quad16);
    CORRADE_COMPARE(importer->frequency(), 44100);
}

void DrFlacImporterTest::quad24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "quad24.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Quad32);
    CORRADE_COMPARE(importer->frequency(), 44100);
}

void DrFlacImporterTest::surround51Channel16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "surround51Channel16.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Surround51Channel16);
    CORRADE_COMPARE(importer->frequency(), 48000);
}

void DrFlacImporterTest::surround51Channel24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "surround51Channel24.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Surround51Channel32);
    CORRADE_COMPARE(importer->frequency(), 48000);
}

void DrFlacImporterTest::surround71Channel24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "surround71Channel24.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Surround71Channel32);
    CORRADE_COMPARE(importer->frequency(), 48000);
}

void DrFlacImporterTest::openTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");

    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "mono8.flac")));
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "mono8.flac")));

    /* Shouldn't crash, leak or anything */
}

void DrFlacImporterTest::importTwice() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Path::join(DRFLACAUDIOIMPORTER_TEST_DIR, "mono8.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Mono8);
    CORRADE_COMPARE(importer->frequency(), 22050);

    /* Verify that everything is working the same way on second use */
    {
        Containers::Array<char> data = importer->data();
        CORRADE_COMPARE(data.size(), 2136);
        CORRADE_COMPARE_AS(Containers::arrayCast<UnsignedByte>(data).prefix(4),
            Containers::arrayView<UnsignedByte>({
                127, 127, 127, 127
            }), TestSuite::Compare::Container);
    } {
        Containers::Array<char> data = importer->data();
        CORRADE_COMPARE(data.size(), 2136);
        CORRADE_COMPARE_AS(Containers::arrayCast<UnsignedByte>(data).prefix(4),
            Containers::arrayView<UnsignedByte>({
                127, 127, 127, 127
            }), TestSuite::Compare::Container);
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Audio::Test::DrFlacImporterTest)
