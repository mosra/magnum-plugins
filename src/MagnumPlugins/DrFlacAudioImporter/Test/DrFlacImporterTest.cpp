/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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
#include <Corrade/Containers/Array.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/DebugStl.h>
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

    void stereo8();
    void stereo16();
    void stereo24();

    void quad16();
    void quad24();

    void surround51Channel16();
    void surround51Channel24();

    void surround71Channel24();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

DrFlacImporterTest::DrFlacImporterTest() {
    addTests({&DrFlacImporterTest::empty,

              &DrFlacImporterTest::zeroSamples,

              &DrFlacImporterTest::mono8,
              &DrFlacImporterTest::mono16,
              &DrFlacImporterTest::mono24,

              &DrFlacImporterTest::stereo8,
              &DrFlacImporterTest::stereo16,
              &DrFlacImporterTest::stereo24,

              &DrFlacImporterTest::quad16,
              &DrFlacImporterTest::quad24,

              &DrFlacImporterTest::surround51Channel16,
              &DrFlacImporterTest::surround51Channel24,

              &DrFlacImporterTest::surround71Channel24});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef DRFLACAUDIOIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(DRFLACAUDIOIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void DrFlacImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");

    std::ostringstream out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out.str(), "Audio::DrFlacImporter::openData(): failed to open and decode FLAC data\n");
}

void DrFlacImporterTest::zeroSamples() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");

    /* No error should happen, it should just give an empty buffer back */
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "zeroSamples.flac")));
    CORRADE_COMPARE(importer->format(), BufferFormat::Mono16);
    CORRADE_COMPARE(importer->frequency(), 22050);
    CORRADE_VERIFY(importer->data().empty());
}

void DrFlacImporterTest::mono8() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "mono8.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Mono8);
    CORRADE_COMPARE(importer->frequency(), 22050);

    CORRADE_COMPARE(importer->data().size(), 2136);
    CORRADE_COMPARE_AS(importer->data().prefix(4),
        (Containers::Array<char>{Containers::InPlaceInit, {127, 127, 127, 127}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void DrFlacImporterTest::mono16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "mono16.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Mono16);
    CORRADE_COMPARE(importer->frequency(), 44000);

    CORRADE_COMPARE_AS(importer->data(),
        (Containers::Array<char>{Containers::InPlaceInit, {'\x1d', '\x10', '\x71', '\xc5'}}),
        TestSuite::Compare::Container);
}

void DrFlacImporterTest::mono24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "mono24.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::MonoFloat);
    CORRADE_COMPARE(importer->frequency(), 48000);

    CORRADE_COMPARE(importer->data().size(), 3696);
    CORRADE_COMPARE_AS(importer->data().prefix(4),
        (Containers::Array<char>{Containers::InPlaceInit, {0, -56, 15, -70}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void DrFlacImporterTest::stereo8() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "stereo8.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo8);
    CORRADE_COMPARE(importer->frequency(), 96000);

    CORRADE_COMPARE_AS(importer->data(),
        (Containers::Array<char>{Containers::InPlaceInit, {'\xde', '\xfe', '\xca', '\x7e'}}),
        TestSuite::Compare::Container);
}

void DrFlacImporterTest::stereo16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "stereo16.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo16);
    CORRADE_COMPARE(importer->frequency(), 44100);

    CORRADE_COMPARE_AS(importer->data(),
        (Containers::Array<char>{Containers::InPlaceInit, {39, 79, 39, 79}}),
        TestSuite::Compare::Container);
}

void DrFlacImporterTest::stereo24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "stereo24.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::StereoFloat);
    CORRADE_COMPARE(importer->frequency(), 8000);

    CORRADE_COMPARE(importer->data().size(), 187944);
    CORRADE_COMPARE_AS(importer->data().prefix(32),
        (Containers::Array<char>{Containers::InPlaceInit, {
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 56, 0, 0, -128, 56, 0,
            0, -64, -72, 0, 0, 0, 0}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void DrFlacImporterTest::quad16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "quad16.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Quad16);
    CORRADE_COMPARE(importer->frequency(), 44100);
}

void DrFlacImporterTest::quad24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "quad24.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Quad32);
    CORRADE_COMPARE(importer->frequency(), 44100);
}

void DrFlacImporterTest::surround51Channel16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "surround51Channel16.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Surround51Channel16);
    CORRADE_COMPARE(importer->frequency(), 48000);
}

void DrFlacImporterTest::surround51Channel24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "surround51Channel24.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Surround51Channel32);
    CORRADE_COMPARE(importer->frequency(), 48000);
}

void DrFlacImporterTest::surround71Channel24() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrFlacAudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRFLACAUDIOIMPORTER_TEST_DIR, "surround71Channel24.flac")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Surround71Channel32);
    CORRADE_COMPARE(importer->frequency(), 48000);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Audio::Test::DrFlacImporterTest)
