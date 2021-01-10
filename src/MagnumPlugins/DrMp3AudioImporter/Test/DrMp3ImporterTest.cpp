/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>
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
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/DebugStl.h>
#include <Magnum/Audio/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Audio { namespace Test { namespace {

struct DrMp3ImporterTest: TestSuite::Tester {
    explicit DrMp3ImporterTest();

    void empty();

    void zeroSamples();

    void mono16();
    void stereo16();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

DrMp3ImporterTest::DrMp3ImporterTest() {
    addTests({&DrMp3ImporterTest::empty,

              &DrMp3ImporterTest::zeroSamples,

              &DrMp3ImporterTest::mono16,
              &DrMp3ImporterTest::stereo16});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef DRMP3AUDIOIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(DRMP3AUDIOIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void DrMp3ImporterTest::empty() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrMp3AudioImporter");

    std::ostringstream out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!importer->openData({&a, 0}));
    CORRADE_COMPARE(out.str(), "Audio::DrMp3Importer::openData(): failed to open and decode MP3 data\n");
}

void DrMp3ImporterTest::zeroSamples() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrMp3AudioImporter");

    /* No error should happen, it should just give an empty buffer back */
    {
        CORRADE_EXPECT_FAIL("dr_mp3 treats 0 frames as an error, because it returns 0 also for malloc failure and such.");
        CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRMP3AUDIOIMPORTER_TEST_DIR, "zeroSamples.mp3")));
        if(!importer->isOpened()) return;
    }
    CORRADE_COMPARE(importer->format(), BufferFormat::Mono16);
    CORRADE_COMPARE(importer->frequency(), 44100);
    CORRADE_VERIFY(importer->data().empty());
}

void DrMp3ImporterTest::mono16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrMp3AudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRMP3AUDIOIMPORTER_TEST_DIR, "mono16.mp3")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Mono16);
    CORRADE_COMPARE(importer->frequency(), 44100);
    CORRADE_COMPARE_AS(importer->data().size(), 6724,
        TestSuite::Compare::Greater);
    CORRADE_COMPARE_AS(importer->data().slice(6720, 6724),
        (Containers::Array<char>{Containers::InPlaceInit, {50, 3, -100, 9}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void DrMp3ImporterTest::stereo16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrMp3AudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRMP3AUDIOIMPORTER_TEST_DIR, "stereo16.mp3")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo16);
    CORRADE_COMPARE(importer->frequency(), 44100);
    CORRADE_COMPARE_AS(importer->data().size(), 9734,
        TestSuite::Compare::Greater);
    CORRADE_COMPARE_AS(importer->data().slice(9730, 9734),
        (Containers::Array<char>{Containers::InPlaceInit, {-90, -103, -79, -103}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Audio::Test::DrMp3ImporterTest)
