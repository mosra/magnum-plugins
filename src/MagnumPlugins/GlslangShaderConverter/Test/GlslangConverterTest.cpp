/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020 Vladimír Vondruš <mosra@centrum.cz>

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
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/File.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Magnum/ShaderTools/AbstractConverter.h>

#include "configure.h"

namespace Magnum { namespace ShaderTools { namespace Test { namespace {

struct GlslangConverterTest: TestSuite::Tester {
    explicit GlslangConverterTest();

    void validate();
    void validateWrongInputFormat();
    void validateWrongInputVersion();
    void validateWrongOutputFormat();
    void validateWrongOutputVersionTarget();
    void validateWrongOutputVersionLanguage();
    void validateFail();
    void validateFailWrongStage();
    void validateFailFileWrongStage();
    void validateFailOverridenInputVersion();
    void validateFailOverridenOutputVersion();
    void validateFailOverridenLimit();

    /* Just a subset of the cases checked for validate(), verifying only the
       convert-specific code paths */
    void convert();
    void convertPreprocessOnlyNotImplemented();
    void convertWrongInputFormat();
    void convertWrongInputVersion();
    void convertWrongOutputFormat();
    void convertWrongOutputVersionTarget();
    void convertWrongOutputVersionLanguage();
    void convertFail();
    void convertFailWrongStage();
    void convertFailFileWrongStage();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractConverter> _converterManager{"nonexistent"};
};

const struct {
    const char* name;

    Stage stage;
    const char* filename;
    const char* alias;

    const char* inputVersion;
    const char* outputVersion;
} ValidateData[] {
    {"GL shader",
        {}, "shader.gl.frag", {},
        "", "opengl4.5"},
    {"GL shader, explicit stage",
        Stage::Fragment, "shader.gl.frag", "shader.glsl",
        "", "opengl4.5"},
    {"GL shader, <stage>.glsl",
        {}, "shader.gl.frag", "shader.frag.glsl",
        "", "opengl4.5"},
    {"GL 2.1 shader",
        {}, "shader.oldgl.frag", {},
        "110", "opengl4.5"},
    {"GLES 2.0 shader",
        {}, "shader.oldgl.frag", {},
        "100 es", "opengl4.5"},
    {"Vulkan shader, default",
        {}, "shader.vk.frag", {},
        "", ""},
    {"Vulkan 1.0 shader",
        {}, "shader.vk.frag", {},
        "", "vulkan1.0"},
    {"Vulkan 1.1 shader",
        {}, "shader.vk.frag", {},
        "", "vulkan1.1"},
    {"Vulkan 1.1 SPIR-V 1.4 shader",
        {}, "shader.vk.frag", {},
        "", "vulkan1.1 spv1.4"},
    {"Vulkan 1.2 shader",
        {}, "shader.vk.frag", {},
        "", "vulkan1.2"},
};

const struct {
    const char* name;
    ConverterFlags flags;
    Containers::Array<std::pair<Containers::StringView, Containers::StringView>> defines;
    bool valid;
    const char* message;
} ValidateFailData[] {
    {"compile warning",
        {}, {Containers::InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""}
        }}, true,
        "WARNING: 0:4: 'reserved__word' : identifiers containing consecutive underscores (\"__\") are reserved"},
    {"compile warning, Quiet",
        ConverterFlag::Quiet, {Containers::InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""}
        }}, true,
        ""},
    {"compile warning, WarningAsError",
        ConverterFlag::WarningAsError, {Containers::InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""}
        }}, false,
        /* Glslang has no concept of warnings as error so this is the same as
           the "compile warning" case except that we fail the validation */
        "WARNING: 0:4: 'reserved__word' : identifiers containing consecutive underscores (\"__\") are reserved"},
    {"link error",
        {}, {Containers::InPlaceInit, {
            {"NO_MAIN", ""}
        }}, false,
        "ERROR: Linking vertex stage: Missing entry point: Each stage requires one entry point"},
    {"compile warning + link error",
        {}, {Containers::InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""},
            {"NO_MAIN", ""}
        }}, false,
        "WARNING: 0:4: 'reserved__word' : identifiers containing consecutive underscores (\"__\") are reserved\n"
        "ERROR: Linking vertex stage: Missing entry point: Each stage requires one entry point"},
    {"compile warning + link error, Quiet",
        ConverterFlag::Quiet, {Containers::InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""},
            {"NO_MAIN", ""}
        }}, false,
        /* Same as the "link error" case */
        "ERROR: Linking vertex stage: Missing entry point: Each stage requires one entry point"},
    {"compile warning + link error, WarningAsError",
        ConverterFlag::WarningAsError, {Containers::InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""},
            {"NO_MAIN", ""}
        }}, false,
        /* Same as the "compile warning, WarningAsError" case -- it should not
           get to the linking step at all */
        "WARNING: 0:4: 'reserved__word' : identifiers containing consecutive underscores (\"__\") are reserved"},
    /** @todo link warning? found only one w/ HLSL where it can have no
        entrypoint */
};

const struct {
    const char* name;

    Stage stage;
    const char* filename;
    const char* alias;
    const char* output;

    const char* outputVersion;
} ConvertData[] {
    /* Just a subset of what's checked for validate(), to verify code paths
       specific to convert() */
    {"GL shader",
        {}, "shader.gl.frag", {}, "shader.gl.spv",
        "opengl4.5"},
    {"GL shader, explicit stage",
        Stage::Fragment, "shader.gl.frag", "shader.glsl", "shader.gl.spv",
        "opengl4.5"},
    {"Vulkan shader, default",
        {}, "shader.vk.frag", {}, "shader.vk.spv",
        ""},
};

const struct {
    const char* name;
    ConverterFlags flags;
    Containers::Array<std::pair<Containers::StringView, Containers::StringView>> defines;
    bool success;
    const char* message;
} ConvertFailData[] {
    /* Again just a subset of what's checked for validate(), to verify code
       paths specific to convert() */
    {"compile warning",
        {}, {Containers::InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""}
        }}, true,
        "ShaderTools::GlslangConverter::convertDataToData(): compilation succeeded with the following message:\n"
        "WARNING: 0:4: 'reserved__word' : identifiers containing consecutive underscores (\"__\") are reserved\n"},
    {"compile warning, WarningAsError",
        ConverterFlag::WarningAsError, {Containers::InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""}
        }}, false,
        /* Glslang has no concept of warnings as error so this is the same as
           the "compile warning" case except that we fail the validation */
        "ShaderTools::GlslangConverter::convertDataToData(): compilation failed:\n"
        "WARNING: 0:4: 'reserved__word' : identifiers containing consecutive underscores (\"__\") are reserved\n"},
    {"link error",
        {}, {Containers::InPlaceInit, {
            {"NO_MAIN", ""}
        }}, false,
        "ShaderTools::GlslangConverter::convertDataToData(): linking failed:\n"
        "ERROR: Linking vertex stage: Missing entry point: Each stage requires one entry point\n"},
    {"compile warning + link error",
        {}, {Containers::InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""},
            {"NO_MAIN", ""}
        }}, false,
        "ShaderTools::GlslangConverter::convertDataToData(): compilation succeeded with the following message:\n"
        "WARNING: 0:4: 'reserved__word' : identifiers containing consecutive underscores (\"__\") are reserved\n"
        "ShaderTools::GlslangConverter::convertDataToData(): linking failed:\n"
        "ERROR: Linking vertex stage: Missing entry point: Each stage requires one entry point\n"}
    /** @todo link warning? found only one w/ HLSL where it can have no
        entrypoint */
};

GlslangConverterTest::GlslangConverterTest() {
    addInstancedTests({&GlslangConverterTest::validate},
        Containers::arraySize(ValidateData));

    addTests({&GlslangConverterTest::validateWrongInputFormat,
              &GlslangConverterTest::validateWrongInputVersion,
              &GlslangConverterTest::validateWrongOutputFormat,
              &GlslangConverterTest::validateWrongOutputVersionTarget,
              &GlslangConverterTest::validateWrongOutputVersionLanguage});

    addInstancedTests({&GlslangConverterTest::validateFail},
        Containers::arraySize(ValidateFailData));

    addTests({&GlslangConverterTest::validateFailWrongStage,
              &GlslangConverterTest::validateFailFileWrongStage,
              &GlslangConverterTest::validateFailOverridenInputVersion,
              &GlslangConverterTest::validateFailOverridenOutputVersion,
              &GlslangConverterTest::validateFailOverridenLimit});

    addInstancedTests({&GlslangConverterTest::convert},
        Containers::arraySize(ConvertData));

    addTests({&GlslangConverterTest::convertPreprocessOnlyNotImplemented,
              &GlslangConverterTest::convertWrongInputFormat,
              &GlslangConverterTest::convertWrongInputVersion,
              &GlslangConverterTest::convertWrongOutputFormat,
              &GlslangConverterTest::convertWrongOutputVersionTarget,
              &GlslangConverterTest::convertWrongOutputVersionLanguage});

    addInstancedTests({&GlslangConverterTest::convertFail},
        Containers::arraySize(ConvertFailData));

    addTests({&GlslangConverterTest::convertFailWrongStage,
              &GlslangConverterTest::convertFailFileWrongStage});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef GLSLANGSHADERCONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(GLSLANGSHADERCONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void GlslangConverterTest::validate() {
    auto&& data = ValidateData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setDefinitions({
        {"A_DEFINE", ""},
        {"AN_UNDEFINE", "something awful!!"},
        {"AN_UNDEFINE", nullptr}
    });
    converter->setOutputFormat({}, data.outputVersion);

    /* Fake the file loading via a callback */
    const Containers::Array<char> file = Utility::Directory::read(Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, data.filename));
    converter->setInputFileCallback([](const std::string&, InputFileCallbackPolicy, const Containers::Array<char>& file) -> Containers::Optional<Containers::ArrayView<const char>> {
        return arrayView(file);
    }, file);

    CORRADE_COMPARE(converter->validateFile(data.stage, data.alias ? data.alias : data.filename),
        std::make_pair(true, ""));
}

void GlslangConverterTest::validateWrongInputFormat() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setInputFormat(Format::Hlsl);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateData({}, {}),
        std::make_pair(false, ""));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::GlslangConverter::validateData(): input format should be Glsl or Unspecified but got ShaderTools::Format::Hlsl\n");
}

void GlslangConverterTest::validateWrongInputVersion() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setInputFormat(Format::Glsl, "100");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateData({}, {}),
        std::make_pair(false, ""));
    CORRADE_COMPARE(out.str(),
        /* Yep, it's silly as 100 is a valid GLSL version. But this way we know
           it's silly. */
        "ShaderTools::GlslangConverter::validateData(): input format version should be one of supported GLSL #version strings but got 100\n");
}

void GlslangConverterTest::validateWrongOutputFormat() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setOutputFormat(Format::Glsl);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateData({}, {}),
        std::make_pair(false, ""));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::GlslangConverter::validateData(): output format should be Unspecified but got ShaderTools::Format::Glsl\n");
}

void GlslangConverterTest::validateWrongOutputVersionTarget() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setOutputFormat(Format::Unspecified, "vulkan2.0");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateData({}, {}),
        std::make_pair(false, ""));
    CORRADE_COMPARE(out.str(),
        /* Yep, it's silly. But this way we know it's silly. */
        "ShaderTools::GlslangConverter::validateData(): output format version target should be opengl4.5 or vulkanX.Y but got vulkan2.0\n");
}

void GlslangConverterTest::validateWrongOutputVersionLanguage() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setOutputFormat(Format::Unspecified, "vulkan1.1 spv2.1");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateData({}, {}),
        std::make_pair(false, ""));
    CORRADE_COMPARE(out.str(),
        /* Yep, it's silly. But this way we know it's silly. */
        "ShaderTools::GlslangConverter::validateData(): output format version language should be spvX.Y but got spv2.1\n");
}

void GlslangConverterTest::validateFail() {
    auto&& data = ValidateFailData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setFlags(data.flags);
    converter->setDefinitions(data.defines);

    Containers::StringView file = R"(#version 330

#ifdef RESERVED_IDENTIFIER
const float reserved__word = 2.0;
#endif

#ifndef NO_MAIN
void main() {
    gl_Position = vec4(0.0);
}
#endif
)";

    CORRADE_COMPARE(converter->validateData({}, file),
        std::make_pair(data.valid, data.message));
}

void GlslangConverterTest::validateFailWrongStage() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    /* Same source as validate() (so it's guaranteed to be valid), just with
       wrong stage */

    converter->setDefinitions({
        {"A_DEFINE", ""},
        {"AN_UNDEFINE", "something awful!!"},
        {"AN_UNDEFINE", nullptr}
    });
    /* We're interested in the first error only */
    converter->configuration().setValue("cascadingErrors", false);

    /* Don't specify the stage -- vertex will be assumed, which doesn't have
       gl_FragCoord */
    CORRADE_COMPARE(converter->validateData(Stage::Unspecified, Utility::Directory::read(Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, "shader.vk.frag"))),
        std::make_pair(false, /* Yes, trailing whitespace. Fuck me. */
            "ERROR: 0:28: 'gl_FragCoord' : undeclared identifier \n"
            "ERROR: 0:28: '' : compilation terminated \n"
            "ERROR: 2 compilation errors.  No code generated."));
}

void GlslangConverterTest::validateFailFileWrongStage() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    /* Same source as validate() (so it's guaranteed to be valid), just with
       wrong stage */

    converter->setDefinitions({
        {"A_DEFINE", ""},
        {"AN_UNDEFINE", "something awful!!"},
        {"AN_UNDEFINE", nullptr}
    });
    /* We're interested in the first error only */
    converter->configuration().setValue("cascadingErrors", false);

    /* Fake the file loading via a callback */
    const Containers::Array<char> file = Utility::Directory::read(Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, "shader.vk.frag"));
    converter->setInputFileCallback([](const std::string&, InputFileCallbackPolicy, const Containers::Array<char>& file) -> Containers::Optional<Containers::ArrayView<const char>> {
        return arrayView(file);
    }, file);

    /* And supply a generic filename to cause the stage to be not detected. The
       filename should be also shown in the output. */
    CORRADE_COMPARE(converter->validateFile(Stage::Unspecified, "shader.glsl"),
        std::make_pair(false, /* Yes, trailing whitespace. Fuck me. */
            "ERROR: shader.glsl:28: 'gl_FragCoord' : undeclared identifier \n"
            "ERROR: shader.glsl:28: '' : compilation terminated \n"
            "ERROR: 2 compilation errors.  No code generated."));
}

void GlslangConverterTest::validateFailOverridenInputVersion() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setInputFormat({}, "120");
    converter->setOutputFormat({}, "opengl4.5");
    /* We're interested in the first error only */
    converter->configuration().setValue("cascadingErrors", false);

    /* Using syntax that isn't available in GLSL 1.10 */
    const char data[] = R"(
in vec4 position;

void main() {
    gl_Position = position;
}
)";
    CORRADE_COMPARE(converter->validateData({}, data),
        std::make_pair(false, /* Yes, trailing whitespace. Fuck me. */
            "ERROR: 0:2: 'in for stage inputs' : not supported for this version or the enabled extensions \n"
            "ERROR: 0:2: '' : compilation terminated \n"
            "ERROR: 2 compilation errors.  No code generated."));
}

void GlslangConverterTest::validateFailOverridenOutputVersion() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setOutputFormat({}, "opengl4.5");

    /* The shader uses Vulkan-specific features, should fail */
    Containers::StringView data = R"(#version 450
layout(push_constant) uniform Thing {
    vec4 position;
};
)";
    CORRADE_COMPARE(converter->validateData({}, data),
        std::make_pair(false, /* Yes, trailing whitespace. Fuck me. */
            "ERROR: 0:2: 'push_constant' : only allowed when using GLSL for Vulkan \n"
            "ERROR: 1 compilation errors.  No code generated."));
}

void GlslangConverterTest::validateFailOverridenLimit() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setOutputFormat({}, "opengl4.5");
    converter->configuration().group("builtins")->setValue("maxCombinedTextureImageUnits", 8);
    /* We're interested in the first error only */
    converter->configuration().setValue("cascadingErrors", false);

    /* Sampler binding is outside of the limit */
    Containers::StringView data = R"(#version 450
layout(binding=8) uniform sampler2D textureData;
)";
    CORRADE_COMPARE(converter->validateData(Stage::Fragment, data),
        std::make_pair(false, /* Yes, trailing whitespace. Fuck me. */
            "ERROR: 0:2: 'binding' : sampler binding not less than gl_MaxCombinedTextureImageUnits \n"
            "ERROR: 1 compilation errors.  No code generated."));
}

void GlslangConverterTest::convert() {
    auto&& data = ConvertData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setDefinitions({
        {"A_DEFINE", ""},
        {"AN_UNDEFINE", "something awful!!"},
        {"AN_UNDEFINE", nullptr},
        /* We target SPIR-V so the file needs an explicit layout(location=)
           always */
        {"NEED_LOCATION", ""}
    });
    converter->setOutputFormat({}, data.outputVersion);

    /* Fake the file loading via a callback */
    const Containers::Array<char> file = Utility::Directory::read(Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, data.filename));
    converter->setInputFileCallback([](const std::string&, InputFileCallbackPolicy, const Containers::Array<char>& file) -> Containers::Optional<Containers::ArrayView<const char>> {
        return arrayView(file);
    }, file);

    std::string output = Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_OUTPUT_DIR, data.output);
    CORRADE_VERIFY(converter->convertFileToFile(data.stage, data.alias ? data.alias : data.filename, output));
    CORRADE_COMPARE_AS(output,
        Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, data.output),
        TestSuite::Compare::File);
}

void GlslangConverterTest::convertPreprocessOnlyNotImplemented() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setFlags(ConverterFlag::PreprocessOnly);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::GlslangConverter::convertDataToData(): PreprocessOnly is not implemented yet, sorry\n");
}

void GlslangConverterTest::convertWrongInputFormat() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setInputFormat(Format::Hlsl);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::GlslangConverter::convertDataToData(): input format should be Glsl or Unspecified but got ShaderTools::Format::Hlsl\n");
}

void GlslangConverterTest::convertWrongInputVersion() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setInputFormat(Format::Glsl, "100");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        /* Yep, it's silly as 100 is a valid GLSL version. But this way we know
           it's silly. */
        "ShaderTools::GlslangConverter::convertDataToData(): input format version should be one of supported GLSL #version strings but got 100\n");
}

void GlslangConverterTest::convertWrongOutputFormat() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setOutputFormat(Format::Glsl);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::GlslangConverter::validateData(): output format should be Spirv or Unspecified but got ShaderTools::Format::Glsl\n");
}

void GlslangConverterTest::convertWrongOutputVersionTarget() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setOutputFormat(Format::Unspecified, "vulkan2.0");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        /* Yep, it's silly. But this way we know it's silly. */
        "ShaderTools::GlslangConverter::convertDataToData(): output format version target should be opengl4.5 or vulkanX.Y but got vulkan2.0\n");
}

void GlslangConverterTest::convertWrongOutputVersionLanguage() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setOutputFormat(Format::Unspecified, "vulkan1.1 spv2.1");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        /* Yep, it's silly. But this way we know it's silly. */
        "ShaderTools::GlslangConverter::convertDataToData(): output format version language should be spvX.Y but got spv2.1\n");
}

void GlslangConverterTest::convertFail() {
    auto&& data = ConvertFailData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setFlags(data.flags);
    converter->setDefinitions(data.defines);

    Containers::StringView file = R"(#version 330

#ifdef RESERVED_IDENTIFIER
const float reserved__word = 2.0;
#endif

#ifndef NO_MAIN
void main() {
    gl_Position = vec4(0.0);
}
#endif
)";

    std::ostringstream out;
    Error redirectError{&out};
    Warning redirectWarning{&out};
    CORRADE_COMPARE(!!converter->convertDataToData({}, file), data.success);
    CORRADE_COMPARE(out.str(), data.message);
}

void GlslangConverterTest::convertFailWrongStage() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    /* Same source as convert() (so it's guaranteed to be valid), just with
       wrong stage */

    converter->setDefinitions({
        {"A_DEFINE", ""},
        {"AN_UNDEFINE", "something awful!!"},
        {"AN_UNDEFINE", nullptr}
    });
    /* We're interested in the first error only */
    converter->configuration().setValue("cascadingErrors", false);

    /* Don't specify the stage -- vertex will be assumed, which doesn't have
       gl_FragCoord */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData(Stage::Unspecified, Utility::Directory::read(Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, "shader.vk.frag"))));
    CORRADE_COMPARE(out.str(), /* Yes, trailing whitespace. Fuck me. */
        "ShaderTools::GlslangConverter::convertDataToData(): compilation failed:\n"
        "ERROR: 0:28: 'gl_FragCoord' : undeclared identifier \n"
        "ERROR: 0:28: '' : compilation terminated \n"
        "ERROR: 2 compilation errors.  No code generated.\n");
}

void GlslangConverterTest::convertFailFileWrongStage() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    /* Same source as validate() (so it's guaranteed to be valid), just with
       wrong stage */

    converter->setDefinitions({
        {"A_DEFINE", ""},
        {"AN_UNDEFINE", "something awful!!"},
        {"AN_UNDEFINE", nullptr}
    });
    /* We're interested in the first error only */
    converter->configuration().setValue("cascadingErrors", false);

    /* Fake the file loading via a callback */
    const Containers::Array<char> file = Utility::Directory::read(Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, "shader.vk.frag"));
    converter->setInputFileCallback([](const std::string&, InputFileCallbackPolicy, const Containers::Array<char>& file) -> Containers::Optional<Containers::ArrayView<const char>> {
        return arrayView(file);
    }, file);

    /* And supply a generic filename to cause the stage to be not detected. The
       filename should be also shown in the output. */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertFileToFile(Stage::Unspecified, "shader.glsl", ""));
    CORRADE_COMPARE(out.str(), /* Yes, trailing whitespace. Fuck me. */
        "ShaderTools::GlslangConverter::convertDataToData(): compilation failed:\n"
        "ERROR: shader.glsl:28: 'gl_FragCoord' : undeclared identifier \n"
        "ERROR: shader.glsl:28: '' : compilation terminated \n"
        "ERROR: 2 compilation errors.  No code generated.\n");
}

}}}}

CORRADE_TEST_MAIN(Magnum::ShaderTools::Test::GlslangConverterTest)
