/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/DebugStl.h> /** @todo drop when Debug is stream-free */
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/FormatStl.h> /** @todo drop when Debug is stream-free */

#include "MagnumPlugins/GltfImporter/decode.h"

namespace Magnum { namespace Trade { namespace Test { namespace {

struct GltfImporterDecodeTest: TestSuite::Tester {
    explicit GltfImporterDecodeTest();

    void uri();
    void uriInvalid();

    void base64();
    void base64Padding();
    void base64Invalid();
};

using namespace Containers::Literals;

const struct {
    const char* name;
    const char* input;
    const char* message;
} UriInvalidData[]{
    {"too short", "hello%0",
        "invalid URI escape sequence %0"},
    {"invalid first char", "hello%h3",
        "invalid URI escape sequence %h3"},
    {"invalid second char", "hello%3h",
        "invalid URI escape sequence %3h"},
};

const struct {
    std::size_t outputSize;
    Containers::StringView input;
} Base64PaddingData[]{
    /* Data from https://en.wikipedia.org/wiki/Base64 */
    {11, "bGlnaHQgd29yay4="},
    {11, "bGlnaHQgd29yay4"},
    {10, "bGlnaHQgd29yaw=="},
    {10, "bGlnaHQgd29yaw"},
    {9, "bGlnaHQgd29y"},
    {8, "bGlnaHQgd28="},
    {8, "bGlnaHQgd28"},
    {7, "bGlnaHQgdw=="},
    {7, "bGlnaHQgdw"}
};

const struct {
    const char* name;
    const char* input;
    const char* message;
} Base64InvalidData[]{
    {"padding in second to last but not last byte",
        "bG=n",
        "invalid Base64 block bG=n"},
    {"padding in a three-character block",
        "bG=",
        "invalid Base64 padding bytes bG="},
    {"padding in a two-character block",
        "b=",
        "invalid Base64 padding bytes b="},
    {"padding in a one-character block",
        "=",
        "invalid Base64 length 1, expected 0, 2 or 3 padding bytes but got 1"},
    {"double padding in a three-character block",
        "b==",
        "invalid Base64 padding bytes b=="},
    {"double padding in a two-character block",
        "==",
        "invalid Base64 padding bytes =="},
    {"invalid byte in the first char",
        "bGln_HQg",
        "invalid Base64 block _HQg"},
    {"invalid byte in the second char",
        "bGlna_Qg",
        "invalid Base64 block a_Qg"},
    {"invalid byte in the third char",
        "bGlnaH_g",
        "invalid Base64 block aH_g"},
    {"invalid byte in the fourth char",
        "bGlnaHQ_",
        "invalid Base64 block aHQ_"},
    {"invalid byte in the first padding char",
        "d29y_y4=",
        "invalid Base64 padding bytes _y4"},
    {"invalid byte in the second padding char",
        "d29ya_4=",
        "invalid Base64 padding bytes a_4"},
    {"invalid byte in the third padding char",
        "d29yay_=",
        "invalid Base64 padding bytes ay_"},
    {"byte > 127 in the input",
        "d2\xffyay4=",
        "invalid Base64 block d2\xffy"},
    {"byte > 127 in the input padding",
        "d29yay\xff=",
        "invalid Base64 padding bytes ay\xff"}
};

GltfImporterDecodeTest::GltfImporterDecodeTest() {
    addTests({&GltfImporterDecodeTest::uri});

    addInstancedTests({&GltfImporterDecodeTest::uriInvalid},
        Containers::arraySize(UriInvalidData));

    addTests({&GltfImporterDecodeTest::base64});

    addInstancedTests({&GltfImporterDecodeTest::base64Padding},
        Containers::arraySize(Base64PaddingData));

    addInstancedTests({&GltfImporterDecodeTest::base64Invalid},
        Containers::arraySize(Base64InvalidData));
}

void GltfImporterDecodeTest::uri() {
    /* Empty */
    {
        Containers::Optional<Containers::String> out = decodeUri("foo():", "");
        CORRADE_VERIFY(out);
        CORRADE_COMPARE(Containers::StringView{*out}, "");

    /* Mixed lowercase and uppercase */
    } {
        Containers::Optional<Containers::String> out = decodeUri("foo():", "he%6cl%6F");
        CORRADE_VERIFY(out);
        CORRADE_COMPARE(Containers::StringView{*out}, "hello");

    /* Hex in the first byte */
    } {
        Containers::Optional<Containers::String> out = decodeUri("foo():", "he%C6l%f6");
        CORRADE_VERIFY(out);
        CORRADE_COMPARE(Containers::StringView{*out}, "he\xC6l\xf6");

    /* Boundary characters */
    } {
        Containers::Optional<Containers::String> out = decodeUri("foo():", "%00%99%aa%ff%AA%FF");
        CORRADE_VERIFY(out);
        CORRADE_COMPARE(Containers::StringView{*out}, "\x00\x99\xaa\xff\xAA\xFF"_s);

    /* Literal % (no, %% isn't a valid escape according to
       https://datatracker.ietf.org/doc/html/rfc3986#section-2.1) */
    } {
        Containers::Optional<Containers::String> out = decodeUri("foo():", "%25");
        CORRADE_VERIFY(out);
        CORRADE_COMPARE(Containers::StringView{*out}, "%");
    }
}

void GltfImporterDecodeTest::uriInvalid() {
    auto&& data = UriInvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(decodeUri("foo():", data.input), Containers::NullOpt);
    CORRADE_COMPARE(out.str(), Utility::formatString("foo(): {}\n", data.message));
}

void GltfImporterDecodeTest::base64() {
    /* Empty */
    {
        Containers::Optional<Containers::Array<char>> out = decodeBase64("foo():", "");
        CORRADE_VERIFY(out);
        CORRADE_COMPARE(Containers::StringView{*out}, "");

    /* Zeros (shouldn't be treated as invalid) */
    } {
        Containers::Optional<Containers::Array<char>> out = decodeBase64("foo():", "AAAA");
        CORRADE_VERIFY(out);
        CORRADE_COMPARE(Containers::StringView{*out}, "\0\0\0"_s);

    /* This should contain sufficiently enough characters from the set to be a
       good enough test. From https://en.wikipedia.org/wiki/Base64. */
    } {
        Containers::Optional<Containers::Array<char>> out = decodeBase64("foo():", "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu");
        CORRADE_VERIFY(out);
        CORRADE_COMPARE(Containers::StringView{*out}, "Many hands make light work.");

    /* Verify the two weird extra chars as well (cross-checked with `base64`
       Unix utility, assuming UTF-8 input). This also verifies that bytes over
       127 on the output are handled correctly. */
    } {
        Containers::Optional<Containers::Array<char>> out = decodeBase64("foo():", "b8W+xa/Fvm8h");
        CORRADE_VERIFY(out);
        CORRADE_COMPARE(Containers::StringView{*out}, "ožůžo!");
    }
}

void GltfImporterDecodeTest::base64Padding() {
    auto&& data = Base64PaddingData[testCaseInstanceId()];
    setTestCaseDescription(Utility::format("{}-byte output{}", data.outputSize, data.input.hasSuffix('=') ? ", padded" : ""));

    Containers::Optional<Containers::Array<char>> out = decodeBase64("foo():", data.input);
    CORRADE_VERIFY(out);
    CORRADE_COMPARE(out->size(), data.outputSize);
    CORRADE_COMPARE(Containers::StringView{*out}, "light work."_s.prefix(data.outputSize));
}

void GltfImporterDecodeTest::base64Invalid() {
    auto&& data = Base64InvalidData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(decodeBase64("foo():", data.input), Containers::NullOpt);
    CORRADE_COMPARE(out.str(), Utility::formatString("foo(): {}\n", data.message));
}

}}}}

CORRADE_TEST_MAIN(Magnum::Trade::Test::GltfImporterDecodeTest)
