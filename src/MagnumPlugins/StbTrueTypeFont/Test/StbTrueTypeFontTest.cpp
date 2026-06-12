/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022, 2023, 2024, 2025, 2026
              Vladimír Vondruš <mosra@centrum.cz>
    Copyright © 2026 Andrew Snyder <asnyder@minitab.com>

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

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/TestSuite/Compare/String.h>
#include <Corrade/Utility/Path.h>
#include <Magnum/ImageView.h>
#include <Magnum/DebugTools/CompareImage.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Text/AbstractFont.h>
#include <Magnum/Text/AbstractGlyphCache.h>
#include <Magnum/Text/AbstractShaper.h>
#include <Magnum/Text/Feature.h>
#include <Magnum/Trade/AbstractImporter.h>

#include "configure.h"

namespace Magnum { namespace Text { namespace Test { namespace {

struct StbTrueTypeFontTest: TestSuite::Tester {
    explicit StbTrueTypeFontTest();

    void empty();
    void invalid();
    void invalidMissingTables();

    void fontCountId();
    void fontCountIdFailed();

    void properties();

    void shape();
    void shapeEmpty();
    void shapeGlyphOffset();
    void shapeMultiple();

    void kerning();

    void fillGlyphCache();
    void fillGlyphCacheIncremental();
    void fillGlyphCacheArray();
    void fillGlyphCacheInvalidFormat();
    void fillGlyphCacheCannotFit();

    void openMemory();
    void openTwice();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractFont> _manager{"nonexistent"};

    /* Needs to load AnyImageImporter from system-wide location */
    PluginManager::Manager<Trade::AbstractImporter> _importerManager;
};

const struct {
    const char* name;
    const char* string;
    UnsignedInt eGlyphId, eGlyphClusterExtraSize;
    UnsignedInt begin, end;
    Float advanceAfterW, advanceAfterE;
} ShapeData[]{
    {"", "Weave", 72, 0, 0, ~UnsignedInt{},
        16.2969f, 8.25f},
    {"substring", "haWeavefefe", 72, 0, 2, 7,
        16.2969f, 8.25f},
    /* Width of `ě` isn't really any different from `e`, but given that the
       advance is the same as with the last `e`, it looks like the font only
       provides kerning for basic Latin and not all characters */
    {"UTF-8", "Wěave", 220, 1, 0, ~UnsignedInt{},
        16.6484f, 8.34375f},
    {"UTF-8 substring", "haWěavefefe", 220, 1, 2, 8,
        16.6484f, 8.34375f},
};

const struct {
    const char* name;
    bool reuse;
} ShapeMultipleData[]{
    {"new shaper every time", false},
    {"reuse previous shaper", false},
};

const struct {
    const char* name;
    Containers::Array<FeatureRange> features;
    Float advances[4];
} KerningData[]{
    {"none", {}, {
        16.3516f,
        8.25781f,
        7.99219f,
        8.34375f
    }},
    {"no-op", {InPlaceInit, {
        /* This one is enabled by default */
        Feature::Kerning,
        /* This one doesn't do anything */
        {Feature::SmallCapitals, false},
    }}, {
        /* Same as above, as kerning is enabled by default */
        16.3516f,
        8.25781f,
        7.99219f,
        8.34375f
    }},
    {"kerning disabled and then enabled again", {InPlaceInit, {
        {Feature::Kerning, false},
        {Feature::Kerning, true}
    }}, {
        /* Should be the same as "none" */
        16.3516f,
        8.25781f,
        7.99219f,
        8.34375f
    }},
    {"kerning disabled", {InPlaceInit, {
        {Feature::Kerning, false}
    }}, {
        16.6484f,
        8.25781f, /* same as with kerning, HarfBuzz also does no adjustment here */
        8.09375f,
        8.34375f  /* same as with kerning, HarfBuzz also does no adjustment here */
    }},
    {"kerning enabled and then disabled again", {InPlaceInit, {
        {Feature::Kerning, true},
        {Feature::Kerning, false},
    }}, {
        /* Should be the same as "kerning disabled" */
        16.6484f,
        8.25781f,
        8.09375f,
        8.34375f
    }},
    /** @todo update once enabling kerning for parts of the string is supported
        (needs to handle UTF-8 and needs to be consistent with HB, particularly
        when kerning gets disabled for just some bytes of a UTF-8 character, or
        what should it do when it's enabled just for one character in a
        pair) */
    {"kerning disabled for a part", {InPlaceInit, {
        {Feature::Kerning, false, 2, 4}
    }}, {
        /* Currently is the same as "none", but shouldn't be when
           implemented */
        16.3516f,
        8.25781f,
        7.99219f,
        8.34375f
    }},
};

const struct {
    const char* name;
    const char* characters;
} FillGlyphCacheData[]{
    {"",
        /* Including also UTF-8 characters to be sure they're handled
           properly */
        "abcdefghijklmnopqrstuvwxyzěšč"},
    {"shuffled order",
        "mvxěipbryzdhfnqlčjšswutokeacg"},
    {"duplicates",
        "mvexěipbbrzzyčbjzdgšhhfnqljswutokeakcg"},
    {"characters not in font",
        /* ☃ */
        "abcdefghijkl\xe2\x98\x83mnopqrstuvwxyzěšč"},
};

/* Shared among all plugins that implement data copying optimizations */
const struct {
    const char* name;
    bool(*open)(AbstractFont&, Containers::ArrayView<const void>, Float);
} OpenMemoryData[]{
    {"data", [](AbstractFont& font, Containers::ArrayView<const void> data, Float size) {
        /* Copy to ensure the original memory isn't referenced */
        Containers::Array<char> copy{InPlaceInit, Containers::arrayCast<const char>(data)};
        return font.openData(copy, size);
    }},
    {"memory", [](AbstractFont& font, Containers::ArrayView<const void> data, Float size) {
        return font.openMemory(data, size);
    }},
};

StbTrueTypeFontTest::StbTrueTypeFontTest() {
    addTests({&StbTrueTypeFontTest::empty,
              &StbTrueTypeFontTest::invalid,
              &StbTrueTypeFontTest::invalidMissingTables,

              &StbTrueTypeFontTest::fontCountId,
              &StbTrueTypeFontTest::fontCountIdFailed,

              &StbTrueTypeFontTest::properties});

    addInstancedTests({&StbTrueTypeFontTest::shape},
        Containers::arraySize(ShapeData));

    addTests({&StbTrueTypeFontTest::shapeEmpty,
              &StbTrueTypeFontTest::shapeGlyphOffset});

    addInstancedTests({&StbTrueTypeFontTest::shapeMultiple},
        Containers::arraySize(ShapeMultipleData));

    addInstancedTests({&StbTrueTypeFontTest::kerning},
        Containers::arraySize(KerningData));

    addInstancedTests({&StbTrueTypeFontTest::fillGlyphCache},
        Containers::arraySize(FillGlyphCacheData));

    addTests({&StbTrueTypeFontTest::fillGlyphCacheIncremental,
              &StbTrueTypeFontTest::fillGlyphCacheArray,
              &StbTrueTypeFontTest::fillGlyphCacheInvalidFormat,
              &StbTrueTypeFontTest::fillGlyphCacheCannotFit});

    addInstancedTests({&StbTrueTypeFontTest::openMemory},
        Containers::arraySize(OpenMemoryData));

    addTests({&StbTrueTypeFontTest::openTwice});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef STBTRUETYPEFONT_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_manager.load(STBTRUETYPEFONT_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
    /* Pull in the AnyImageImporter dependency for image comparison */
    _importerManager.load("AnyImageImporter");
    /* Reset the plugin dir after so it doesn't load anything else from the
       filesystem. Do this also in case of static plugins (no _FILENAME
       defined) so it doesn't attempt to load dynamic system-wide plugins. */
    #ifndef CORRADE_PLUGINMANAGER_NO_DYNAMIC_PLUGIN_SUPPORT
    _importerManager.setPluginDirectory({});
    #endif
    /* Load StbImageImporter from the build tree, if defined. Otherwise it's
       static and already loaded. */
    #ifdef STBIMAGEIMPORTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_importerManager.load(STBIMAGEIMPORTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void StbTrueTypeFontTest::empty() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");

    Containers::String out;
    Error redirectError{&out};
    char a{};
    /* Explicitly checking non-null but empty view */
    CORRADE_VERIFY(!font->openData({&a, 0}, 16.0f));
    CORRADE_COMPARE(out, "Text::StbTrueTypeFont::openData(): the file is empty\n");
}

void StbTrueTypeFontTest::invalid() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!font->openData("Oxygen.ttf", 16.0f));
    CORRADE_COMPARE(out, "Text::StbTrueTypeFont::openData(): can't get offset of font 0\n");
}

void StbTrueTypeFontTest::invalidMissingTables() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");

    /* This tests stbtt_InitFont() failing, which happens only if some of the
       required tables are missing. So can't really fake it with bogus data
       like with invalid() above, as it could randomly crash. Instead rename
       one of the fourCC table identifiers to something else. */
    Containers::Optional<Containers::String> file = Utility::Path::readString(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"));
    CORRADE_VERIFY(file);

    Containers::MutableStringView found = file->find("cmap");
    CORRADE_VERIFY(found);
    found[0] = 'C';

    Containers::String out;
    Error redirectError{&out};
    CORRADE_VERIFY(!font->openData(*file, 16.0f));
    CORRADE_COMPARE(out, "Text::StbTrueTypeFont::openData(): font initialization failed\n");
}

void StbTrueTypeFontTest::fontCountId() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");

    CORRADE_COMPARE(font->fileFontCount(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf")), 1);
    CORRADE_COMPARE(font->fileFontCount(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttc")), 2);

    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttc"), 16.0f));
    CORRADE_COMPARE(font->glyphId(U'W'), 1);

    /* The second font is the same except for a different glyph mapping */
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttc"), 16.0f, 1));
    CORRADE_COMPARE(font->glyphId(U'W'), 3);
}

void StbTrueTypeFontTest::fontCountIdFailed() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");

    Containers::String out;
    Error redirectError{&out};
    /* This is attempting to open a filename as data */
    CORRADE_VERIFY(!font->dataFontCount("Oxygen.ttf"));
    CORRADE_VERIFY(!font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f, 1));
    CORRADE_VERIFY(!font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttc"), 16.0f, 2));
    CORRADE_COMPARE_AS(out,
        "Text::StbTrueTypeFont::dataFontCount(): font count query failed\n"
        "Text::StbTrueTypeFont::openData(): can't get offset of font 1\n"
        "Text::StbTrueTypeFont::openData(): can't get offset of font 2\n",
        TestSuite::Compare::String);
}

void StbTrueTypeFontTest::properties() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    CORRADE_COMPARE(font->size(), 16.0f);
    CORRADE_COMPARE(font->glyphCount(), 671);
    CORRADE_COMPARE(font->glyphId(U'W'), 58);

    /* Compared to FreeType, StbTrueType does not round the metrics to whole
       units, which is due to FreeType applying hinting and StbTrueType not */

    CORRADE_COMPARE(font->ascent(), 14.8516f);
    CORRADE_COMPARE(font->descent(), -3.77344f);
    CORRADE_COMPARE(font->lineHeight(), 18.625f);
    CORRADE_COMPARE(font->glyphSize(58), (Vector2{18.0f, 12.0f}));
    CORRADE_COMPARE(font->glyphAdvance(58), (Vector2{16.6484f, 0.0f}));
}

void StbTrueTypeFontTest::shape() {
    auto&& data = ShapeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    CORRADE_COMPARE(shaper->shape(data.string, data.begin, data.end), 5);

    UnsignedInt ids[5];
    Vector2 offsets[5];
    Vector2 advances[5];
    UnsignedInt clusters[5];
    shaper->glyphIdsInto(ids);
    shaper->glyphOffsetsAdvancesInto(offsets, advances);
    shaper->glyphClustersInto(clusters);
    CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
        58u,            /* 'W' */
        data.eGlyphId,  /* 'e' or 'ě' */
        68u,            /* 'a' */
        89u,            /* 'v' */
        72u             /* 'e' */
    }), TestSuite::Compare::Container);
    /* There are no glyph-specific offsets anywhere. See the glyphShapeOffset()
       below for a dedicated verification of this lack of functionality. */
    CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
        {}, {}, {}, {}, {}
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
        {data.advanceAfterW, 0.0f},
        {data.advanceAfterE, 0.0f},
        {8.25781f, 0.0f},
        {7.99219f, 0.0f},
        {8.34375f, 0.0f}
    }), TestSuite::Compare::Container);
    CORRADE_COMPARE_AS(Containers::arrayView(clusters), Containers::arrayView({
        data.begin + 0u,
        data.begin + 1u,
        data.begin + 2u + data.eGlyphClusterExtraSize,
        data.begin + 3u + data.eGlyphClusterExtraSize,
        data.begin + 4u + data.eGlyphClusterExtraSize,
    }), TestSuite::Compare::Container);
}

void StbTrueTypeFontTest::shapeEmpty() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    /* Shouldn't crash or do anything rogue */
    CORRADE_COMPARE(shaper->shape("Wave", 2, 2), 0);
}

void StbTrueTypeFontTest::shapeGlyphOffset() {
    /* Basically a copy of HarfBuzzFontTest::shapeGlyphOffset() to have a repro
       case for the lack of features in this plugin */

    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    /* See the HarfBuzzFont test for how this file is generated */
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "SourceSans3-Regular.subset.otf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    /* Compared to the HarfBuzz test, the \xcd\x8f is missing here because it
       resolves as glyph 0. The combining diacritics however resolve to the
       same glyphs. */
    CORRADE_COMPARE(shaper->shape("Ve\xcc\x8c\xcc\x8c\xcc\x8ctev"), 8);

    UnsignedInt ids[8];
    Vector2 offsets[8];
    Vector2 advances[8];
    UnsignedInt clusters[8];
    shaper->glyphIdsInto(ids);
    shaper->glyphOffsetsAdvancesInto(offsets, advances);
    shaper->glyphClustersInto(clusters);
    /* List of known IDs copied from FreeTypeFont tests, stb_truetype doesn't
       support glyph name queries */
    CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
     /* V   e   'ˇ' 'ˇ' 'ˇ' t   e   v */
        2u, 3u, 9u, 9u, 9u, 4u, 3u, 5u
    }), TestSuite::Compare::Container);
    {
        CORRADE_EXPECT_FAIL("stb_truetype doesn't have advanced shaping capabilities that would position the combining diacritics on top of the previous character and one after another.");
        CORRADE_COMPARE_AS(offsets[2], Vector2{},
            TestSuite::Compare::NotEqual);
    }
    CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
        {8.24f, 0.0f},  /* 'V' */
        {7.936f, 0.0f}, /* 'e' */
        /* The combining marks have no advance in addition to the base
           character */
        {0.0f, 0.0f},   /* 'ˇ' */
        {0.0f, 0.0f},   /* 'ˇ' */
        {0.0f, 0.0f},   /* 'ˇ' */
        {5.408f, 0.0f}, /* 't' */
        {7.936f, 0.0f}, /* 'e' */
        {7.472f, 0.0f}  /* 'v' */
    }), TestSuite::Compare::Container);
    /* Yeah so they are all zero */
    CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
        {},             /* 'V' */
        {},             /* 'e' */
        {},             /* 'ˇ' */
        {},             /* 'ˇ' */
        {},             /* 'ˇ' */
        {},             /* 't' */
        {},             /* 'e' */
        {}              /* 'v' */
    }), TestSuite::Compare::Container);
    {
        CORRADE_EXPECT_FAIL("StbTrueTypeFont doesn't merge combining diacritics into the same cluster as the preceding character.");
        CORRADE_COMPARE(clusters[2], 1);
    }
    CORRADE_COMPARE_AS(Containers::arrayView(clusters), Containers::arrayView({
        0u,             /* 'V' */
        1u,             /* 'e' */
        2u,             /* 'ˇ' */
        4u,             /* 'ˇ' */
        6u,             /* 'ˇ' */
        8u,             /* 't' */
        9u,             /* 'e' */
        10u,            /* 'v' */
    }), TestSuite::Compare::Container);
}

void StbTrueTypeFontTest::shapeMultiple() {
    auto&& data = ShapeMultipleData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

    /* Empty text */
    {
        CORRADE_COMPARE(shaper->shape("Wave", 2, 2), 0);

    /* Short text. Empty shape shouldn't have caused any broken state. */
    } {
        if(!data.reuse)
            shaper = font->createShaper();

        CORRADE_COMPARE(shaper->shape("We"), 2);
        UnsignedInt ids[2];
        Vector2 offsets[2];
        Vector2 advances[2];
        UnsignedInt clusters[2];
        shaper->glyphIdsInto(ids);
        shaper->glyphOffsetsAdvancesInto(offsets, advances);
        shaper->glyphClustersInto(clusters);
        CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
            58u, /* 'W' */
            72u  /* 'e' */
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
            {}, {},
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
            {16.2969f, 0.0f},
            {8.34375f, 0.0f}
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(clusters), Containers::arrayView({
            0u,
            1u
        }), TestSuite::Compare::Container);

    /* Long text, same as in shape(), should enlarge the array for it */
    } {
        if(!data.reuse)
            shaper = font->createShaper();

        CORRADE_COMPARE(shaper->shape("Wěave"), 5);
        UnsignedInt ids[5];
        Vector2 offsets[5];
        Vector2 advances[5];
        UnsignedInt clusters[5];
        shaper->glyphIdsInto(ids);
        shaper->glyphOffsetsAdvancesInto(offsets, advances);
        shaper->glyphClustersInto(clusters);
        CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
            58u,  /* 'W' */
            220u, /* 'ě' */
            68u,  /* 'a' */
            89u,  /* 'v' */
            72u   /* 'e' */
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
            {}, {}, {}, {}, {}
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
            {16.6484f, 0.0f},
            {8.34375f, 0.0f},
            {8.25781f, 0.0f},
            {7.99219f, 0.0f},
            {8.34375f, 0.0f}
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(clusters), Containers::arrayView({
            0u,
            1u,
            3u,
            4u,
            5u
        }), TestSuite::Compare::Container);

    /* Short text again, should not leave the extra glyphs there */
    } {
        if(!data.reuse)
            shaper = font->createShaper();

        CORRADE_COMPARE(shaper->shape("ave"), 3);
        UnsignedInt ids[3];
        Vector2 offsets[3];
        Vector2 advances[3];
        UnsignedInt clusters[3];
        shaper->glyphIdsInto(ids);
        shaper->glyphOffsetsAdvancesInto(offsets, advances);
        shaper->glyphClustersInto(clusters);
        CORRADE_COMPARE_AS(Containers::arrayView(ids), Containers::arrayView({
            68u,
            89u,
            72u
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(offsets), Containers::arrayView<Vector2>({
            {}, {}, {}
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(advances), Containers::arrayView<Vector2>({
            {8.25781f, 0.0f},
            {7.99219f, 0.0f},
            {8.34375f, 0.0f}
        }), TestSuite::Compare::Container);
        CORRADE_COMPARE_AS(Containers::arrayView(clusters), Containers::arrayView({
            0u, 1u, 2u
        }), TestSuite::Compare::Container);
    }
}

void StbTrueTypeFontTest::kerning() {
    auto&& data = KerningData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    Containers::Pointer<AbstractShaper> shaper = font->createShaper();

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

void StbTrueTypeFontTest::fillGlyphCache() {
    auto&& data = FillGlyphCacheData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Ideally this would be tested at least partially without the image, but
       adding extra logic for that would risk that the image might accidentally
       not get checked at all */
    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct GlyphCache: AbstractGlyphCache {
        explicit GlyphCache(PluginManager::Manager<Trade::AbstractImporter>& importerManager, PixelFormat format, const Vector2i& size, const Vector2i& padding): AbstractGlyphCache{format, size, padding}, importerManager(importerManager) {}

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i& offset, const ImageView2D& image) override {
            /* The passed image is just the filled subset, compare the whole
               thing for more predictable results */
            CORRADE_COMPARE(offset, Vector2i{});
            CORRADE_COMPARE(image.size(), (Vector2i{64, 45}));
            CORRADE_COMPARE_WITH(this->image().pixels<UnsignedByte>()[0],
                Utility::Path::join(STBTRUETYPEFONT_TEST_DIR, "glyph-cache.png"),
                DebugTools::CompareImageToFile{importerManager});
            called = true;
        }

        bool called = false;
        PluginManager::Manager<Trade::AbstractImporter>& importerManager;
    /* Default padding is 1, set back to 0 to verify it's not overwriting
       neighbors by accident */
    } cache{_importerManager, PixelFormat::R8Unorm, Vector2i{64}, {}};

    /* Should call doSetImage() above, which then performs image comparison */
    font->fillGlyphCache(cache, data.characters);
    CORRADE_VERIFY(cache.called);

    /* The font should associate itself with the cache */
    CORRADE_COMPARE(cache.fontCount(), 1);
    CORRADE_COMPARE(cache.findFont(*font), 0);

    /* 26 ASCII characters, 3 UTF-8 ones + one "not found" glyph, and one
       invalid glyph from the cache itself. The count should be the same in all
       cases as the input is deduplicated and characters not present in the
       font get substituted for glyph 0. */
    CORRADE_COMPARE(cache.glyphCount(), 26 + 3 + 1 + 1);

    /* Check positions of a few select glyphs. They should all retain the same
       position regardless of how the input is shuffled. Again, this is
       different from FreeType, most probably due to stb_truetype not
       implementing hinting. */

    /* Invalid glyph in the cache is deliberately not changed as that'd cause
       a mess if multiple fonts would each want to set its own */
    CORRADE_COMPARE(cache.glyph(0), Containers::triple(
        Vector2i{},
        0,
        Range2Di{}));
    /* Invalid glyph */
    CORRADE_COMPARE(cache.glyph(0, 0), Containers::triple(
        Vector2i{},
        0,
        Range2Di{{7, 14}, {12, 25}}));
    /* Above the baseline */
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('k')), Containers::triple(
        Vector2i{},
        0,
        Range2Di{{34, 13}, {43, 25}}));
    /* Below the baseline */
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('g')), Containers::triple(
        Vector2i{0, -4},
        0,
        Range2Di{{45, 0}, {54, 13}}));
    /* UTF-8 */
    UnsignedInt sId = font->glyphId(
        /* MSVC (but not clang-cl) doesn't support UTF-8 in char32_t literals
           but it does it regular strings. Still a problem in MSVC 2022, what a
           trash fire, can't you just give up on those codepage insanities
           already, ffs?! */
        #if defined(CORRADE_TARGET_MSVC) && !defined(CORRADE_TARGET_CLANG)
        U'\u0161'
        #else
        U'š'
        #endif
    );
    CORRADE_COMPARE(cache.glyph(0, sId), Containers::triple(
        Vector2i{0, -1},
        0,
        Range2Di{{20, 0}, {27, 14}}));
}

void StbTrueTypeFontTest::fillGlyphCacheIncremental() {
    /* Ideally this would be tested at least partially without the image, but
       adding extra logic for that would risk that the image might accidentally
       not get checked at all */
    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct GlyphCache: AbstractGlyphCache {
        explicit GlyphCache(PluginManager::Manager<Trade::AbstractImporter>& importerManager, PixelFormat format, const Vector2i& size, const Vector2i& padding): AbstractGlyphCache{format, size, padding}, importerManager(importerManager) {}

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector2i& offset, const ImageView2D& image) override {
            /* The passed image is just the filled subset, compare the whole
               thing for more predictable results */
            if(called == 0) {
                CORRADE_COMPARE(offset, Vector2i{});
                CORRADE_COMPARE(image.size(), (Vector2i{64, 26}));
            } else if(called == 1) {
                CORRADE_COMPARE(offset, (Vector2i{0, 25}));
                CORRADE_COMPARE(image.size(), (Vector2i{64, 20}));
                CORRADE_COMPARE_WITH(this->image().pixels<UnsignedByte>()[0],
                    Utility::Path::join(STBTRUETYPEFONT_TEST_DIR, "glyph-cache.png"),
                    DebugTools::CompareImageToFile{importerManager});
            } else CORRADE_FAIL("This shouldn't get called more than twice");
            ++called;
        }

        Int called = 0;
        PluginManager::Manager<Trade::AbstractImporter>& importerManager;
    /* Default padding is 1, set back to 0 to verify it's not overwriting
       neighbors by accident */
    } cache{_importerManager, PixelFormat::R8Unorm, Vector2i{64}, {}};

    /* First call with the bottom half of the glyph cache until the invalid
       glyph */
    font->fillGlyphCache(cache, "jgpqčěšdbylhktfi");
    CORRADE_COMPARE(cache.called, 1);

    /* The font should associate itself with the cache now */
    CORRADE_COMPARE(cache.fontCount(), 1);
    CORRADE_COMPARE(cache.findFont(*font), 0);

    /* 17 characters + one global invalid glyph */
    CORRADE_COMPARE(cache.glyphCount(), 17 + 1);

    /* Second call with the rest */
    font->fillGlyphCache(cache, "oacesmnuwvxzr");
    CORRADE_COMPARE(cache.called, 2);

    /* The font should not be added again */
    CORRADE_COMPARE(cache.fontCount(), 1);

    /* There's now all glyphs like in fillGlyphCache() */
    CORRADE_COMPARE(cache.glyphCount(), 26 + 3 + 1 + 1);

    /* Positions of the glyphs should be the same as in fillGlyphCache() */
    CORRADE_COMPARE(cache.glyph(0), Containers::triple(
        Vector2i{},
        0,
        Range2Di{}));
    CORRADE_COMPARE(cache.glyph(0, 0), Containers::triple(
        Vector2i{},
        0,
        Range2Di{{7, 14}, {12, 25}}));
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('k')), Containers::triple(
        Vector2i{},
        0,
        Range2Di{{34, 13}, {43, 25}}));
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('g')), Containers::triple(
        Vector2i{0, -4},
        0,
        Range2Di{{45, 0}, {54, 13}}));
    UnsignedInt sId = font->glyphId(
        /* MSVC (but not clang-cl) doesn't support UTF-8 in char32_t literals
           but it does it regular strings. Still a problem in MSVC 2022, what a
           trash fire, can't you just give up on those codepage insanities
           already, ffs?! */
        #if defined(CORRADE_TARGET_MSVC) && !defined(CORRADE_TARGET_CLANG)
        U'\u0161'
        #else
        U'š'
        #endif
    );
    CORRADE_COMPARE(cache.glyph(0, sId), Containers::triple(
        Vector2i{0, -1},
        0,
        Range2Di{{20, 0}, {27, 14}}));
}

void StbTrueTypeFontTest::fillGlyphCacheArray() {
    /* Ideally this would be tested at least partially without the image, but
       adding extra logic for that would risk that the image might accidentally
       not get checked at all */
    if(_importerManager.loadState("PngImporter") == PluginManager::LoadState::NotFound)
        CORRADE_SKIP("PngImporter plugin not found, cannot test");

    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct GlyphCache: AbstractGlyphCache {
        explicit GlyphCache(PluginManager::Manager<Trade::AbstractImporter>& importerManager, PixelFormat format, const Vector3i& size, const Vector2i& padding): AbstractGlyphCache{format, size, padding}, importerManager(importerManager) {}

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector3i& offset, const ImageView3D& image) override {
            /* The passed image is just the filled subset, compare the whole
               thing for more predictable results */
            CORRADE_COMPARE(offset, Vector3i{});
            CORRADE_COMPARE(image.size(), (Vector3i{48, 48, 2}));
            CORRADE_COMPARE_WITH(this->image().pixels<UnsignedByte>()[0],
                Utility::Path::join(STBTRUETYPEFONT_TEST_DIR, "glyph-cache-array0.png"),
                DebugTools::CompareImageToFile{importerManager});
            CORRADE_COMPARE_WITH(this->image().pixels<UnsignedByte>()[1],
                Utility::Path::join(STBTRUETYPEFONT_TEST_DIR, "glyph-cache-array1.png"),
                DebugTools::CompareImageToFile{importerManager});
            called = true;
        }

        bool called = false;
        PluginManager::Manager<Trade::AbstractImporter>& importerManager;
    /* Default padding is 1, set back to 0 to verify it's not overwriting
       neighbors by accident */
    } cache{_importerManager, PixelFormat::R8Unorm, {48, 48, 2}, {}};

    /* Should call doSetImage() above, which then performs image comparison */
    font->fillGlyphCache(cache, "abcdefghijklmnopqrstuvwxyzěšč");
    CORRADE_VERIFY(cache.called);

    /* The font should associate itself with the cache */
    CORRADE_COMPARE(cache.fontCount(), 1);
    CORRADE_COMPARE(cache.findFont(*font), 0);

    /* Same as in fillGlyphCache() */
    CORRADE_COMPARE(cache.glyphCount(), 26 + 3 + 1 + 1);

    /* Positions are spread across two layers now */
    CORRADE_COMPARE(cache.glyph(0), Containers::triple(
        Vector2i{},
        0,
        Range2Di{}));
    /* Invalid glyph */
    CORRADE_COMPARE(cache.glyph(0, 0), Containers::triple(
        Vector2i{},
        0,
        Range2Di{{21, 27}, {26, 38}}));
    /* First layer */
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('g')), Containers::triple(
        Vector2i{0, -4},
        0,
        Range2Di{{39, 13}, {48, 26}}));
    /* Second layer */
    CORRADE_COMPARE(cache.glyph(0, font->glyphId('n')), Containers::triple(
        Vector2i{0, 0},
        1,
        Range2Di{{40, 9}, {48, 18}}));
}

void StbTrueTypeFontTest::fillGlyphCacheInvalidFormat() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct GlyphCache: AbstractGlyphCache {
        explicit GlyphCache(PixelFormat format, const Vector2i& size, const Vector2i& padding): AbstractGlyphCache{format, size, padding} {}

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector3i&, const ImageView3D&) override {
            CORRADE_FAIL("This shouldn't be called.");
        }
    } cache{PixelFormat::R8Srgb, {16, 16}, {}};

    Containers::String out;
    Error redirectError{&out};
    font->fillGlyphCache(cache, "");
    CORRADE_COMPARE(out, "Text::StbTrueTypeFont::fillGlyphCache(): expected a PixelFormat::R8Unorm glyph cache but got PixelFormat::R8Srgb\n");
}

void StbTrueTypeFontTest::fillGlyphCacheCannotFit() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    struct GlyphCache: AbstractGlyphCache {
        explicit GlyphCache(PixelFormat format, const Vector2i& size, const Vector2i& padding): AbstractGlyphCache{format, size, padding} {}

        GlyphCacheFeatures doFeatures() const override { return {}; }
        void doSetImage(const Vector3i&, const ImageView3D&) override {
            CORRADE_FAIL("This shouldn't be called.");
        }
    } cache{PixelFormat::R8Unorm, {16, 32}, {}};

    Containers::String out;
    Error redirectError{&out};
    font->fillGlyphCache(cache, "HELLO");
    CORRADE_COMPARE_AS(out,
        "Text::StbTrueTypeFont::fillGlyphCache(): cannot fit 5 glyphs with a total area of 524 pixels into a cache of size Vector(16, 32, 1) and Vector(16, 0, 1) filled so far\n",
        TestSuite::Compare::String);
}

void StbTrueTypeFontTest::openMemory() {
    auto&& data = OpenMemoryData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    /* Same as properties() except that it uses openData() & openMemory()
       instead of openFile() to test data copying on import */

    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");

    Containers::Optional<Containers::Array<char>> memory = Utility::Path::read(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"));
    CORRADE_VERIFY(memory);
    CORRADE_VERIFY(data.open(*font, *memory, 16.0f));

    CORRADE_COMPARE(font->size(), 16.0f);
    CORRADE_COMPARE(font->glyphCount(), 671);
    CORRADE_COMPARE(font->glyphId(U'W'), 58);
}

void StbTrueTypeFontTest::openTwice() {
    Containers::Pointer<AbstractFont> font = _manager.instantiate("StbTrueTypeFont");

    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));
    CORRADE_VERIFY(font->openFile(Utility::Path::join(FREETYPEFONT_TEST_DIR, "Oxygen.ttf"), 16.0f));

    /* Shouldn't crash, leak or anything */
}

}}}}

CORRADE_TEST_MAIN(Magnum::Text::Test::StbTrueTypeFontTest)
