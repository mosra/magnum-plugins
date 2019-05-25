/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
              Vladimír Vondruš <mosra@centrum.cz>
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
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/Audio/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Audio { namespace Test { namespace {

struct DrMp3ImporterTest: TestSuite::Tester {
    explicit DrMp3ImporterTest();

    void mono16();
    void stereo16();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractImporter> _manager{"nonexistent"};
};

DrMp3ImporterTest::DrMp3ImporterTest() {
    addTests({&DrMp3ImporterTest::mono16,
              &DrMp3ImporterTest::stereo16});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef DRMP3AUDIOIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT(_manager.load(DRMP3AUDIOIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void DrMp3ImporterTest::mono16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrMp3AudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRMP3AUDIOIMPORTER_TEST_DIR, "mono16.mp3")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Mono16);
    CORRADE_COMPARE(importer->frequency(), 44100);
    CORRADE_COMPARE_AS(importer->data().slice(6720, 6724),
        (Containers::Array<char>{Containers::InPlaceInit, {50, 3, -100, 9}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

void DrMp3ImporterTest::stereo16() {
    Containers::Pointer<AbstractImporter> importer = _manager.instantiate("DrMp3AudioImporter");
    CORRADE_VERIFY(importer->openFile(Utility::Directory::join(DRMP3AUDIOIMPORTER_TEST_DIR, "stereo16.mp3")));

    CORRADE_COMPARE(importer->format(), BufferFormat::Stereo16);
    CORRADE_COMPARE(importer->frequency(), 44100);
    CORRADE_COMPARE_AS(importer->data().slice(9730, 9734),
        (Containers::Array<char>{Containers::InPlaceInit, {-90, -103, -79, -103}}),
        TestSuite::Compare::Container<Containers::ArrayView<const char>>);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Audio::Test::DrMp3ImporterTest)
