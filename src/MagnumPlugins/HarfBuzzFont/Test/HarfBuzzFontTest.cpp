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
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/Utility/DebugStl.h> /** @todo remove once Debug is stream-free */
#include <Corrade/Utility/Endianness.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Text/AbstractFont.h>
#include <Magnum/Text/AbstractShaper.h>
#include <Magnum/Text/Direction.h>
#include <Magnum/Text/Feature.h>
#include <Magnum/Text/Script.h>
#include <hb.h>

#include "configure.h"

namespace Magnum { namespace Text { namespace Test { namespace {

struct HarfBuzzFontTest: TestSuite::Tester {
    explicit HarfBuzzFontTest();

    void scriptMapping();

    void shape();
    void shapeDifferentScriptLanguageDirection();
    void shapeAutodetectScriptLanguageDirection();
    void shapeUnsupportedScript();
    void shapeEmpty();

    void shaperReuse();
    void shaperReuseAutodetection();

    void shapeFeatures();

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

const struct {
    const char* name;
    ShapeDirection direction;
    bool flip;
} ShapeDifferentScriptLanguageDirectionData[]{
    {"left to right", ShapeDirection::LeftToRight, false},
    {"right to left", ShapeDirection::RightToLeft, true},
    {"top to bottom", ShapeDirection::TopToBottom, false},
    {"bottom to top", ShapeDirection::BottomToTop, true},
};

const struct {
    const char* name;
    bool explicitlySetUnspecified;
} ShapeAutodetectScriptLanguageDirectionData[]{
    {"", false},
    {"explicitly set unspecified values", true}
};

const struct {
    const char* name;
    Containers::Array<FeatureRange> features;
    Float advances[4];
} ShapeFeaturesData[]{
    {"none", {}, {
        /* Versions 3.3.0 and 3.3.1 reported {16.5f, 0.0f} here, but the
           change is reverted in 3.3.2 again "as it proved problematic". */
        16.3594f,
        8.26562f,
        /* HarfBuzz before 1.7 and after 3.1 gives 8.0, versions between the
           other */
        #if HB_VERSION_MAJOR*100 + HB_VERSION_MINOR < 107 || \
            HB_VERSION_MAJOR*100 + HB_VERSION_MINOR >= 301
        8.0f,
        #else
        7.984384f,
        #endif
        8.34375f
    }},
    {"no-op", {InPlaceInit, {
        /* These are enabled by HarfBuzz by default */
        Feature::Kerning,
        Feature::StandardLigatures
    }}, {
        /* Same as above, as kerning is enabled by default */
        16.3594f,
        8.26562f,
        #if HB_VERSION_MAJOR*100 + HB_VERSION_MINOR < 107 || \
            HB_VERSION_MAJOR*100 + HB_VERSION_MINOR >= 301
        8.0f,
        #else
        7.984384f,
        #endif
        8.34375f
    }},
    {"kerning disabled and then enabled again", {InPlaceInit, {
        {Feature::Kerning, false},
        {Feature::Kerning, true}
    }}, {
        /* Should be the same as "none" */
        16.3594f,
        8.26562f,
        #if HB_VERSION_MAJOR*100 + HB_VERSION_MINOR < 107 || \
            HB_VERSION_MAJOR*100 + HB_VERSION_MINOR >= 301
        8.0f,
        #else
        7.984384f,
        #endif
        8.34375f
    }},
    {"kerning disabled", {InPlaceInit, {
        {Feature::Kerning, false}
    }}, {
        /* Not quite the same as what FreeTypeFont gives back, but different
           from above at least */
        16.6562f,
        8.26562f, /* same as with kerning */
        8.09375f,
        8.34375f  /* same as with kerning */
    }},
    {"kerning enabled and then disabled again", {InPlaceInit, {
        {Feature::Kerning, true},
        {Feature::Kerning, false},
    }}, {
        /* Should be the same as "kerning disabled" */
        16.6562f,
        8.26562f,
        8.09375f,
        8.34375f
    }},
    {"kerning enabled and disabled for a part", {InPlaceInit, {
        {Feature::Kerning, 0, 2, true},
        {Feature::Kerning, 2, 4, false},
    }}, {
        16.3594f, /* same as with kerning */
        8.26562f, /* same as with kerning */
        8.09375f,
        8.34375f  /* same as with kerning */
    }},
    {"kerning disabled and enabled for a part", {InPlaceInit, {
        /* Just different order from above, should result in the same */
        {Feature::Kerning, 2, 4, false},
        {Feature::Kerning, 0, 2, true},
    }}, {
        16.3594f,
        8.26562f,
        8.09375f,
        8.34375f
    }},
};

HarfBuzzFontTest::HarfBuzzFontTest() {
    addTests({&HarfBuzzFontTest::scriptMapping});

    addInstancedTests({&HarfBuzzFontTest::shape},
        Containers::arraySize(ShapeData));

    addInstancedTests({&HarfBuzzFontTest::shapeDifferentScriptLanguageDirection},
        Containers::arraySize(ShapeDifferentScriptLanguageDirectionData));

    addInstancedTests({&HarfBuzzFontTest::shapeAutodetectScriptLanguageDirection},
        Containers::arraySize(ShapeAutodetectScriptLanguageDirectionData));

    addTests({&HarfBuzzFontTest::shapeUnsupportedScript,
              &HarfBuzzFontTest::shapeEmpty,

              &HarfBuzzFontTest::shaperReuse,
              &HarfBuzzFontTest::shaperReuseAutodetection});

    addInstancedTests({&HarfBuzzFontTest::shapeFeatures},
        Containers::arraySize(ShapeFeaturesData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #if defined(FREETYPEFONT_PLUGIN_FILENAME) && defined(HARFBUZZFONT_PLUGIN_FILENAME)
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(FREETYPEFONT_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(HARFBUZZFONT_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void HarfBuzzFontTest::scriptMapping() {
    /* The FourCC values should match between the Script enum and HarfBuzz to
       not need expensive mapping. Eh, actually, they don't match, as HB_TAG()
       creates an Endian-dependent value, so ntaL instead of Latn on Little
       Endian. I couldn't find any documentation or a bug report on why this
       differs from what OpenType fonts actually have (where it's Big-Endian
       always, i.e. Latn), apart from one "oops" in this old commit:
        https://github.com/harfbuzz/harfbuzz/commit/fcd6f5326166e993b8f5222efbaffe916da98f0a */
    CORRADE_COMPARE(UnsignedInt(Script::Unspecified), HB_SCRIPT_INVALID);
    #define _c(name, hb) CORRADE_COMPARE(UnsignedInt(Script::name), Utility::Endianness::bigEndian(HB_SCRIPT_ ## hb));
    #define _c_include_unsupported 0
    #include "../scriptMapping.h"
    #undef _c_include_unsupported
    #undef _c

    /* Verify the header indeed contains cases for all Script values. It's not
       guarded with -Werror=switch as that would mean a Magnum update adding a
       new Script value would break a build of HarfBuzzFont tests, which is
       undesirable. So it's just a warning. */
    Script script = Script::Unknown;
    switch(script) {
        case Script::Unspecified:
        #define _c(name, hb) case Script::name:
        #define _c_include_all 1
        #include "../scriptMapping.h"
        #undef _c_include_all
        #undef _c
            CORRADE_VERIFY(UnsignedInt(script));
    }

    /* Also verify that the header contains all hb_script_t values supported by
       this HarfBuzz version. Also not -Werror=switch as this could break on a
       HarfBuzz update. */
    hb_script_t hbScript = HB_SCRIPT_UNKNOWN;
    switch(hbScript) {
        case HB_SCRIPT_INVALID:
        #define _c(name, hb) case HB_SCRIPT_ ## hb:
        #define _c_include_supported 1
        #include "../scriptMapping.h"
        #undef _c_include_supported
        #undef _c
        case _HB_SCRIPT_MAX_VALUE:
        /* These two values used to be different before 2.0.0, not anymore:
            https://github.com/harfbuzz/harfbuzz/commit/90dd255e570bf8ea3436e2f29242068845256e55 */
        #if !HB_VERSION_ATLEAST(2, 0, 0)
        case _HB_SCRIPT_MAX_VALUE_SIGNED:
        #endif
            CORRADE_VERIFY(UnsignedInt(hbScript));
    }
}

void HarfBuzzFontTest::shape() {
    auto&& data = ShapeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    /* There's no script / language / direction set by default */
    CORRADE_COMPARE(shaper->script(), Script::Unspecified);
    CORRADE_COMPARE(shaper->language(), "");
    CORRADE_COMPARE(shaper->direction(), ShapeDirection::Unspecified);

    /* Shape a text */
    CORRADE_VERIFY(shaper->setScript(Script::Latin));
    CORRADE_VERIFY(shaper->setLanguage("en"));
    CORRADE_VERIFY(shaper->setDirection(ShapeDirection::LeftToRight));
    CORRADE_COMPARE(shaper->shape(data.string, data.begin, data.end), 4);

    /* The script / language / direction set above should get used for
       shaping */
    CORRADE_COMPARE(shaper->script(), Script::Latin);
    CORRADE_COMPARE(shaper->language(), "en");
    CORRADE_COMPARE(shaper->direction(), ShapeDirection::LeftToRight);

    UnsignedInt ids[4];
    Vector2 offsets[4];
    Vector2 advances[4];
    shaper->glyphIdsInto(ids);
    shaper->glyphOffsetsAdvancesInto(offsets, advances);
    CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
        58u,            /* 'W' */
        68u,            /* 'a' */
        89u,            /* 'v' */
        data.eGlyphId   /* 'e' or 'ě' */
    }), TestSuite::Compare::Container);
    /* There are no glyph-specific offsets here */
    /** @todo test this with something, Zalgo or other combining diacritics */
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

void HarfBuzzFontTest::shapeDifferentScriptLanguageDirection() {
    auto&& data = ShapeDifferentScriptLanguageDirectionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    CORRADE_VERIFY(shaper->setScript(Script::Greek));
    CORRADE_VERIFY(shaper->setLanguage("el"));
    CORRADE_VERIFY(shaper->setDirection(data.direction));
    CORRADE_COMPARE(shaper->shape("Ελλάδα"), 6);
    CORRADE_COMPARE(shaper->script(), Script::Greek);
    CORRADE_COMPARE(shaper->language(), "el");
    CORRADE_COMPARE(shaper->direction(), data.direction);

    UnsignedInt ids[6];
    shaper->glyphIdsInto(ids);

    UnsignedInt expectedIds[]{
        450,    /* 'Ε' */
        487,    /* 'λ' */
        487,    /* 'λ' again */
        472,    /* 'ά' */
        480,    /* 'δ' */
        477,    /* 'α' */
    };
    CORRADE_COMPARE_AS(Containers::arrayView(ids),
        data.flip ?
            Containers::stridedArrayView(expectedIds).flipped<0>() :
            Containers::stridedArrayView(expectedIds),
        TestSuite::Compare::Container);

    /* Advances and offsets aren't really important here */
}

void HarfBuzzFontTest::shapeAutodetectScriptLanguageDirection() {
    auto&& data = ShapeAutodetectScriptLanguageDirectionData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    if(data.explicitlySetUnspecified) {
        CORRADE_VERIFY(shaper->setScript(Script::Unspecified));
        CORRADE_VERIFY(shaper->setLanguage(""));
        CORRADE_VERIFY(shaper->setDirection(ShapeDirection::Unspecified));
    }

    CORRADE_COMPARE(shaper->shape("	العربية"), 8);
    CORRADE_COMPARE(shaper->script(), Script::Arabic);
    {
        CORRADE_EXPECT_FAIL("HarfBuzz uses current locale for language autodetection, not the actual text");
        CORRADE_COMPARE(shaper->language(), "ar");
    }
    CORRADE_COMPARE(shaper->language(), "c");
    CORRADE_COMPARE(shaper->direction(), ShapeDirection::RightToLeft);

    /* The font doesn't have Arabic glyphs, so this is all invalid */
    UnsignedInt ids[8];
    shaper->glyphIdsInto(ids);
    CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
    }), TestSuite::Compare::Container);
}

void HarfBuzzFontTest::shapeUnsupportedScript() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    /* Passing an unknown Script value will pass, as the check is
       blacklist-based to not have to switch through all possible values on
       HarfBuzz versions that support everything. Plus that also allows to
       pass values that new HarfBuzz supports but Script doesn't list yet, a
       whitelist would reject that. */
    CORRADE_VERIFY(shaper->setScript(script("Yolo")));

    /* Added in 3.0 */
    CORRADE_COMPARE(shaper->setScript(Script::OldUyghur),
                    HB_VERSION_ATLEAST(3, 0, 0));
    /* Added in 3.4 */
    CORRADE_COMPARE(shaper->setScript(Script::Math),
                    HB_VERSION_ATLEAST(3, 4, 0));
    /* Added in 5.2 */
    #if HB_VERSION_ATLEAST(5, 2, 0)
    CORRADE_SKIP("Can only test on HarfBuzz before 5.2.0");
    #endif
    CORRADE_VERIFY(!shaper->setScript(Script::Kawi));
}

void HarfBuzzFontTest::shapeEmpty() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    /* Shouldn't crash or do anything rogue */
    CORRADE_COMPARE(shaper->shape("Wave", 2, 2), 0);

    /* Interestingly enough it doesn't detect the script even though it has
       the surrounding context to guess from */
    CORRADE_COMPARE(shaper->script(), Script::Unspecified);
    CORRADE_COMPARE(shaper->language(), "c");
    CORRADE_COMPARE(shaper->direction(), ShapeDirection::LeftToRight);
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
        shaper->glyphIdsInto(ids);
        shaper->glyphOffsetsAdvancesInto(offsets, advances);
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
        shaper->glyphIdsInto(ids);
        shaper->glyphOffsetsAdvancesInto(offsets, advances);
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
        shaper->glyphIdsInto(ids);
        shaper->glyphOffsetsAdvancesInto(offsets, advances);
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

void HarfBuzzFontTest::shaperReuseAutodetection() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    /* There's no script / language / direction set by default */
    CORRADE_COMPARE(shaper->script(), Script::Unspecified);
    CORRADE_COMPARE(shaper->language(), "");
    CORRADE_COMPARE(shaper->direction(), ShapeDirection::Unspecified);

    /* Arabic text gets detected as such */
    {
        CORRADE_COMPARE(shaper->shape("	العربية"), 8);
        CORRADE_COMPARE(shaper->script(), Script::Arabic);
        {
            CORRADE_EXPECT_FAIL("HarfBuzz uses current locale for language autodetection, not the actual text");
            CORRADE_COMPARE(shaper->language(), "ar");
        }
        CORRADE_COMPARE(shaper->language(), "c");
        CORRADE_COMPARE(shaper->direction(), ShapeDirection::RightToLeft);

    /* Greek text should then not be treated as RTL and such */
    } {
        CORRADE_COMPARE(shaper->shape("Ελλάδα"), 6);
        CORRADE_COMPARE(shaper->script(), Script::Greek);
        {
            CORRADE_EXPECT_FAIL("HarfBuzz uses current locale for language autodetection, not the actual text");
            CORRADE_COMPARE(shaper->language(), "el");
        }
        CORRADE_COMPARE(shaper->language(), "c");
        CORRADE_COMPARE(shaper->direction(), ShapeDirection::LeftToRight);

    /* Empty text shouldn't inherit anything from before either and produce a
       result consistent with shapeEmpty() */
    } {
        CORRADE_COMPARE(shaper->shape("Wave", 2, 2), 0);
        CORRADE_COMPARE(shaper->script(), Script::Unspecified);
        CORRADE_COMPARE(shaper->language(), "c");
        CORRADE_COMPARE(shaper->direction(), ShapeDirection::LeftToRight);
    }
}

void HarfBuzzFontTest::shapeFeatures() {
    auto&& data = ShapeFeaturesData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractFont> font = _manager.instantiate("HarfBuzzFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    /* Shape a text */
    CORRADE_VERIFY(shaper->setScript(Script::Latin));
    CORRADE_VERIFY(shaper->setLanguage("en"));
    CORRADE_VERIFY(shaper->setDirection(ShapeDirection::LeftToRight));
    CORRADE_COMPARE(shaper->shape("Wave", data.features), 4);

    /* Verify the shaped glyph IDs match expectations, other IDs would have
       vastly different advances */
    UnsignedInt ids[4];
    Vector2 offsets[4];
    Vector2 advances[4];
    shaper->glyphIdsInto(ids);
    shaper->glyphOffsetsAdvancesInto(offsets, advances);
    CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
        58u,    /* 'W' */
        68u,    /* 'a' */
        89u,    /* 'v' */
        72u,    /* 'e' */
    }), TestSuite::Compare::Container);

    /* Assuming Y advance is always 0 */
    CORRADE_COMPARE_AS(Containers::stridedArrayView(advances).slice(&Vector2::x),
        Containers::stridedArrayView(data.advances),
        TestSuite::Compare::Container);
}

}}}}

CORRADE_TEST_MAIN(Magnum::Text::Test::HarfBuzzFontTest)
