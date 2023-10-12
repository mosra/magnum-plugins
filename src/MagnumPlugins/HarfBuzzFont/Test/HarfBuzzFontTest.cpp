/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Path.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Text/AbstractFont.h>
#include <Magnum/Text/AbstractShaper.h>
#include <hb.h>

#include "configure.h"

namespace Magnum { namespace Text { namespace Test { namespace {

struct HarfBuzzFontTest: TestSuite::Tester {
    explicit HarfBuzzFontTest();

    void shape();
    void shapeEmpty();
    void shaperReuse();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractFont> _manager{"nonexistent"};
};

const struct {
    const char* name;
    const char* string;
    UnsignedInt eGlyphId;
    UnsignedInt begin, end;
    Float advanceAfterV;
} ShapeData[]{
    {"", "Wave", 72, 0, ~UnsignedInt{},
        /* HarfBuzz before 1.7 and after 3.1 gives 8.0, versions between the
           other */
        #if HB_VERSION_MAJOR*100 + HB_VERSION_MINOR < 107 || \
            HB_VERSION_MAJOR*100 + HB_VERSION_MINOR >= 301
        8.0f,
        #else
        7.984384f
        #endif
    },
    {"substring", "haWavefefe", 72, 2, 6,
        #if HB_VERSION_MAJOR*100 + HB_VERSION_MINOR < 107 || \
            HB_VERSION_MAJOR*100 + HB_VERSION_MINOR >= 301
        8.0f,
        #else
        7.984384f
        #endif
    },
    /* `vě` has slightly different spacing than `ve` but there it doesn't get
       different between versions at least */
    {"UTF-8", "Wavě", 220, 0, ~UnsignedInt{}, 8.09376f},
    {"UTF-8 substring", "haWavěfefe", 220, 2, 7, 8.09376f},
};

HarfBuzzFontTest::HarfBuzzFontTest() {
    addInstancedTests({&HarfBuzzFontTest::shape},
        Containers::arraySize(ShapeData));

    addTests({&HarfBuzzFontTest::shapeEmpty,
              &HarfBuzzFontTest::shaperReuse});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #if defined(FREETYPEFONT_PLUGIN_FILENAME) && defined(HARFBUZZFONT_PLUGIN_FILENAME)
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(FREETYPEFONT_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(HARFBUZZFONT_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void HarfBuzzFontTest::shape() {
    auto&& data = ShapeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    CORRADE_COMPARE(shaper->shape(data.string, data.begin, data.end), 4);

    UnsignedInt ids[4];
    Vector2 offsets[4];
    Vector2 advances[4];
    shaper->glyphsInto(ids, offsets, advances);
    CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
        58u,            /* 'W' */
        68u,            /* 'a' */
        89u,            /* 'v' */
        data.eGlyphId   /* 'e' or 'ě' */
    }), TestSuite::Compare::Container);
    /* There are no glyph-specific offsets here */
    CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
        {}, {}, {}, {}
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
        /* Versions 3.3.0 and 3.3.1 reported {16.5f, 0.0f} here, but the
           change is reverted in 3.3.2 again "as it proved problematic". */
        {16.3594f, 0.0f},
        {8.26562f, 0.0f},
        {data.advanceAfterV, 0.0f},
        {8.34375f, 0.0f}
    }), TestSuite::Compare::Container);
}

void HarfBuzzFontTest::shapeEmpty() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    /* Shouldn't crash or do anything rogue */
    CORRADE_COMPARE(shaper->shape("Wave", 2, 2), 0);
}

void HarfBuzzFontTest::shaperReuse() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    /* Empty text */
    {
        CORRADE_COMPARE(shaper->shape("Wave", 2, 2), 0);

    /* Short text. Empty shape shouldn't have caused any broken state. */
    } {
        CORRADE_COMPARE(shaper->shape("We"), 2);
        UnsignedInt ids[2];
        Vector2 offsets[2];
        Vector2 advances[2];
        shaper->glyphsInto(ids, offsets, advances);
        CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
            58u, /* 'W' */
            72u  /* 'e' */
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
            {}, {},
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
            {HB_VERSION_MAJOR*100 + HB_VERSION_MINOR < 107 ||
             HB_VERSION_MAJOR*100 + HB_VERSION_MINOR >= 301 ? 16.3125f : 16.2969f,
             0.0f},
            {8.34375f, 0.0f}
        }), TestSuite::Compare::Container);

    /* Long text, same as in shape(), should enlarge the array for it */
    } {
        CORRADE_COMPARE(shaper->shape("Wave"), 4);
        UnsignedInt ids[4];
        Vector2 offsets[4];
        Vector2 advances[4];
        shaper->glyphsInto(ids, offsets, advances);
        CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
            58u, /* 'W' */
            68u, /* 'a' */
            89u, /* 'v' */
            72u  /* 'e' or 'ě' */
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
            {}, {}, {}, {}
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
            {16.3594f, 0.0f},
            {8.26562f, 0.0f},
            {HB_VERSION_MAJOR*100 + HB_VERSION_MINOR < 107 ||
             HB_VERSION_MAJOR*100 + HB_VERSION_MINOR >= 301 ? 8.0f : 7.984384f,
             0.0f},
            {8.34375f, 0.0f}
        }), TestSuite::Compare::Container);

    /* Short text again, should not leave the extra glyphs there */
    } {
        CORRADE_COMPARE(shaper->shape("a"), 1);
        UnsignedInt ids[1];
        Vector2 offsets[1];
        Vector2 advances[1];
        shaper->glyphsInto(ids, offsets, advances);
        CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
            68u,
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
            {},
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
            {8.26562f, 0.0f}
        }), TestSuite::Compare::Container);
    }
}

}}}}

CORRADE_TEST_MAIN(Magnum::Text::Test::HarfBuzzFontTest)
