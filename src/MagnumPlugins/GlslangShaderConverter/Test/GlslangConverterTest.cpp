/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
                2020, 2021 Vladimír Vondruš <mosra@centrum.cz>

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
#include <unordered_map>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/FileCallback.h>
#include <Magnum/ShaderTools/AbstractConverter.h>
#include <Magnum/ShaderTools/Stage.h>

/* Mirrors what's in the plugin source */
#ifdef __has_include
#if __has_include(<glslang/build_info.h>)
#include <glslang/build_info.h>
#else
#include <glslang/Include/revision.h>
#endif
#else
#include <glslang/Include/revision.h>
#endif

#include "configure.h"

namespace Magnum { namespace ShaderTools { namespace Test { namespace {

struct GlslangConverterTest: TestSuite::Tester {
    explicit GlslangConverterTest();

    void validate();
    void validateIncludes();
    void validateIncludesCallback();
    void validateWrongInputFormat();
    void validateWrongInputVersion();
    void validateWrongOutputFormat();
    void validateWrongOutputVersionTarget();
    void validateWrongOutputVersionLanguage();
    void validateWrongOutputFormatForGenericOpenGL();
    void validateFail();
    void validateFailWrongStage();
    void validateFailFileWrongStage();
    void validateFailOverridenInputVersion();
    void validateFailOverridenOutputVersion();
    void validateFailOverridenLimit();
    void validateFailIncludeNotFound();

    /* Just a subset of the cases checked for validate(), verifying only the
       convert-specific code paths */
    void convert();
    void convertIncludes();
    void convertPreprocessOnlyNotImplemented();
    void convertWrongInputFormat();
    void convertWrongInputVersion();
    void convertWrongOutputFormat();
    void convertWrongOutputVersionTarget();
    void convertWrongOutputVersionLanguage();
    void convertWrongDebugInfoLevel();
    void convertFail();
    void convertFailWrongStage();
    void convertFailFileWrongStage();

    void vulkanNoExplicitLayout();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractConverter> _converterManager{"nonexistent"};
};

using namespace Containers::Literals;

const struct {
    const char* name;

    Stage stage;
    const char* filename;
    const char* alias;

    const char* inputVersion;
    Format outputFormat;
    const char* outputVersion;
    bool spirvShouldBeValidated;
} ValidateData[] {
    /* GCC 4.8 doesn't like using just {} for Stage, Format or const char* */
    {"GL shader",
        Stage{}, "shader.gl.frag", nullptr,
        "", Format{}, "opengl4.5", false},
    {"GL shader, generic version",
        Stage{}, "shader.gl.frag", nullptr,
        "", Format{}, "opengl", false},
    {"GL shader, SPIR-V output format",
        Stage{}, "shader.gl.frag", nullptr,
        "", Format::Spirv, "opengl4.5", true},
    {"GL shader, SPIR-V included in output version",
        Stage{}, "shader.gl.frag", nullptr,
        "", Format{}, "opengl4.5 spv1.0", true},
    {"GL shader, explicit stage",
        Stage::Fragment, "shader.gl.frag", "shader.glsl",
        "", Format{}, "opengl4.5", false},
    {"GL shader, <stage>.glsl",
        Stage{}, "shader.gl.frag", "shader.frag.glsl",
        "", Format{}, "opengl4.5", false},
    {"GL 2.1 shader",
        Stage{}, "shader.oldgl.frag", nullptr,
        "110", Format{}, "opengl4.5", false},
    {"GLES 2.0 shader",
        Stage{}, "shader.oldgl.frag", nullptr,
        "100 es", Format{}, "opengl4.5", false},
    {"Vulkan shader, default",
        Stage{}, "shader.vk.frag", nullptr,
        "", Format{}, "", true},
    {"Vulkan shader, SPIR-V target",
        Stage{}, "shader.vk.frag", nullptr,
        "", Format::Spirv, "", true},
    {"Vulkan 1.0 shader",
        Stage{}, "shader.vk.frag", nullptr,
        "", Format{}, "vulkan1.0", true},
    {"Vulkan 1.1 shader",
        Stage{}, "shader.vk.frag", nullptr,
        "", Format{}, "vulkan1.1", true},
    {"Vulkan 1.1 SPIR-V 1.4 shader",
        Stage{}, "shader.vk.frag", nullptr,
        "", Format{}, "vulkan1.1 spv1.4", true},
    #if defined(GLSLANG_VERSION_MAJOR) || LSLANG_PATCH_LEVEL >= 3743
    {"Vulkan 1.2 shader",
        Stage{}, "shader.vk.frag", nullptr,
        "", Format{}, "vulkan1.2", true},
    #endif
};

const struct {
    const char* name;
    ConverterFlags flags;
    Containers::Array<std::pair<Containers::StringView, Containers::StringView>> defines;
    bool valid;
    const char* message;
} ValidateFailData[] {
    {"compile warning",
        {}, {InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""}
        }}, true,
        "WARNING: 0:4: 'reserved__word' : identifiers containing consecutive underscores (\"__\") are reserved"},
    {"compile warning, Quiet",
        ConverterFlag::Quiet, {InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""}
        }}, true,
        ""},
    {"compile warning, WarningAsError",
        ConverterFlag::WarningAsError, {InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""}
        }}, false,
        /* Glslang has no concept of warnings as error so this is the same as
           the "compile warning" case except that we fail the validation */
        "WARNING: 0:4: 'reserved__word' : identifiers containing consecutive underscores (\"__\") are reserved"},
    {"link error",
        {}, {InPlaceInit, {
            {"NO_MAIN", ""}
        }}, false,
        "ERROR: Linking vertex stage: Missing entry point: Each stage requires one entry point"},
    {"compile warning + link error",
        {}, {InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""},
            {"NO_MAIN", ""}
        }}, false,
        "WARNING: 0:4: 'reserved__word' : identifiers containing consecutive underscores (\"__\") are reserved\n"
        "ERROR: Linking vertex stage: Missing entry point: Each stage requires one entry point"},
    {"compile warning + link error, Quiet",
        ConverterFlag::Quiet, {InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""},
            {"NO_MAIN", ""}
        }}, false,
        /* Same as the "link error" case */
        "ERROR: Linking vertex stage: Missing entry point: Each stage requires one entry point"},
    {"compile warning + link error, WarningAsError",
        ConverterFlag::WarningAsError, {InPlaceInit, {
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
    const char* debugInfoLevel;
} ConvertData[] {
    /* Just a subset of what's checked for validate(), to verify code paths
       specific to convert() */
    /* GCC 4.8 doesn't like using just {} for Stage or for const char* */
    {"GL shader",
        Stage{}, "shader.gl.frag", nullptr, "shader.gl.spv",
        "opengl4.5", nullptr},
    {"GL shader, explicit stage",
        Stage::Fragment, "shader.gl.frag", "shader.glsl", "shader.gl.spv",
        "opengl4.5", nullptr},
    {"Vulkan shader, default",
        Stage{}, "shader.vk.frag", nullptr, "shader.vk.spv",
        "", nullptr},
    /* Vulkan 1.0 target puts OpModuleProcessed into the shader source which
       looks strange in the disassembly, but that's all */
    {"Vulkan 1.1 shader with debug info",
        Stage{}, "shader.vk.frag", nullptr,
        /* Versions before 10 emit extra OpModuleProcessed "use-storage-buffer"
           https://github.com/KhronosGroup/glslang/issues/1829 */
        #if defined(GLSLANG_VERSION_MAJOR)
        "shader.vk.debug.spv",
        #else
        "shader.vk.debug-glslang8.spv",
        #endif
        "vulkan1.1", "1"},
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
        {}, {InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""}
        }}, true,
        "ShaderTools::GlslangConverter::convertDataToData(): compilation succeeded with the following message:\n"
        "WARNING: 0:4: 'reserved__word' : identifiers containing consecutive underscores (\"__\") are reserved\n"},
    {"compile warning, WarningAsError",
        ConverterFlag::WarningAsError, {InPlaceInit, {
            {"RESERVED_IDENTIFIER", ""}
        }}, false,
        /* Glslang has no concept of warnings as error so this is the same as
           the "compile warning" case except that we fail the validation */
        "ShaderTools::GlslangConverter::convertDataToData(): compilation failed:\n"
        "WARNING: 0:4: 'reserved__word' : identifiers containing consecutive underscores (\"__\") are reserved\n"},
    {"link error",
        {}, {InPlaceInit, {
            {"NO_MAIN", ""}
        }}, false,
        "ShaderTools::GlslangConverter::convertDataToData(): linking failed:\n"
        "ERROR: Linking vertex stage: Missing entry point: Each stage requires one entry point\n"},
    {"compile warning + link error",
        {}, {InPlaceInit, {
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

/* Yes, trailing whitespace. Fuck me. */
const char* VulkanNoExplicitBindingError =
    "ERROR: 0:28: 'binding' : sampler/texture/image requires layout(binding=X) \n"
    "ERROR: 1 compilation errors.  No code generated.";
const char* VulkanNoExplicitLocationError =
    "ERROR: 0:32: 'location' : SPIR-V requires location for user input/output \n"
    "ERROR: 1 compilation errors.  No code generated.";

const struct {
    const char* name;
    const char* define;
    Format outputFormat;
    const char* error;
} VulkanNoExplicitLayoutData[] {
    /* GCC 4.8 doesn't like using just {} for Format */
    {"no layout(binding)", "NO_EXPLICIT_BINDING", Format{},
        VulkanNoExplicitBindingError},
    {"no layout(binding), SPIR-V output", "NO_EXPLICIT_BINDING", Format::Spirv,
        VulkanNoExplicitBindingError},
    {"no layout(location)", "NO_EXPLICIT_LOCATION", Format{},
        VulkanNoExplicitLocationError},
    {"no layout(location), SPIR-V output", "NO_EXPLICIT_LOCATION", Format::Spirv,
        VulkanNoExplicitLocationError},
};

GlslangConverterTest::GlslangConverterTest() {
    addInstancedTests({&GlslangConverterTest::validate},
        Containers::arraySize(ValidateData));

    addTests({&GlslangConverterTest::validateIncludes,
              &GlslangConverterTest::validateIncludesCallback,
              &GlslangConverterTest::validateWrongInputFormat,
              &GlslangConverterTest::validateWrongInputVersion,
              &GlslangConverterTest::validateWrongOutputFormat,
              &GlslangConverterTest::validateWrongOutputVersionTarget,
              &GlslangConverterTest::validateWrongOutputVersionLanguage,
              &GlslangConverterTest::validateWrongOutputFormatForGenericOpenGL});

    addInstancedTests({&GlslangConverterTest::validateFail},
        Containers::arraySize(ValidateFailData));

    addTests({&GlslangConverterTest::validateFailWrongStage,
              &GlslangConverterTest::validateFailFileWrongStage,
              &GlslangConverterTest::validateFailOverridenInputVersion,
              &GlslangConverterTest::validateFailOverridenOutputVersion,
              &GlslangConverterTest::validateFailOverridenLimit,
              &GlslangConverterTest::validateFailIncludeNotFound});

    addInstancedTests({&GlslangConverterTest::convert},
        Containers::arraySize(ConvertData));

    addTests({&GlslangConverterTest::convertIncludes,
              &GlslangConverterTest::convertPreprocessOnlyNotImplemented,
              &GlslangConverterTest::convertWrongInputFormat,
              &GlslangConverterTest::convertWrongInputVersion,
              &GlslangConverterTest::convertWrongOutputFormat,
              &GlslangConverterTest::convertWrongOutputVersionTarget,
              &GlslangConverterTest::convertWrongOutputVersionLanguage,
              &GlslangConverterTest::convertWrongDebugInfoLevel});

    addInstancedTests({&GlslangConverterTest::convertFail},
        Containers::arraySize(ConvertFailData));

    addTests({&GlslangConverterTest::convertFailWrongStage,
              &GlslangConverterTest::convertFailFileWrongStage});

    addInstancedTests({&GlslangConverterTest::vulkanNoExplicitLayout},
        Containers::arraySize(VulkanNoExplicitLayoutData));

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef GLSLANGSHADERCONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(GLSLANGSHADERCONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void GlslangConverterTest::validate() {
    auto&& data = ValidateData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    #if !defined(GLSLANG_VERSION_MAJOR) && GLSLANG_PATCH_LEVEL < 3496
    /* GL_ARB_explicit_uniform_location is implemented only since 7.13.3496,
       https://github.com/KhronosGroup/glslang/pull/1880, earlier versions
       spit out the following error and the only way to use explicit uniform
       location is by forcing the version to 430

        ERROR: shader.gl.frag:24: '#extension' : extension not supported: GL_ARB_explicit_uniform_location
        ERROR: shader.gl.frag:27: 'location qualifier on uniform or buffer' : not supported for this version or the enabled extensions

    */
    if(data.filename == "shader.gl.frag"_s && data.spirvShouldBeValidated)
        CORRADE_SKIP("GL_ARB_explicit_uniform_location only implemented since 7.13.3496.");
    #endif

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    Containers::Array<std::pair<Containers::StringView, Containers::StringView>> defines{InPlaceInit, {
        {"A_DEFINE", ""},
        {"AN_UNDEFINE", "something awful!!"},
        {"AN_UNDEFINE", nullptr},
    }};

    if(!data.spirvShouldBeValidated)
        arrayAppend(defines, InPlaceInit, "VALIDATE_NON_SPIRV", "");

    converter->setDefinitions(defines);
    converter->setOutputFormat(data.outputFormat, data.outputVersion);

    /* Fake the file loading via a callback */
    const Containers::Array<char> file = Utility::Directory::read(Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, data.filename));
    converter->setInputFileCallback([](const std::string&, InputFileCallbackPolicy, const Containers::Array<char>& file) -> Containers::Optional<Containers::ArrayView<const char>> {
        return arrayView(file);
    }, file);

    CORRADE_COMPARE(converter->validateFile(data.stage, data.alias ? data.alias : data.filename),
        std::make_pair(true, ""));
}

void GlslangConverterTest::validateIncludes() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    CORRADE_COMPARE(converter->validateFile({}, Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, "includes.vert")),
        std::make_pair(true, ""));
}

void GlslangConverterTest::validateIncludesCallback() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    std::unordered_map<std::string, Containers::Array<char>> files;
    converter->setInputFileCallback([](const std::string& filename, InputFileCallbackPolicy policy, std::unordered_map<std::string, Containers::Array<char>>& files) -> Containers::Optional<Containers::ArrayView<const char>> {
        auto found = files.find(filename);

        /* Discard the loaded file, if not needed anymore */
        if(policy == InputFileCallbackPolicy::Close) {
            Debug{} << "Closing" << filename;

            if(found != files.end()) files.erase(found);
            return {};
        }

        Debug{} << "Loading" << filename;

        /* Extract from an archive if not there yet; fail if not extraction
           failed */
        if(found == files.end()) {
            Containers::Array<char> file = Utility::Directory::read(Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, filename));
            CORRADE_VERIFY(file);

            found = files.emplace(filename, std::move(file)).first;
        }

        return Containers::ArrayView<const char>{found->second};
    }, files);

    std::ostringstream out;
    Debug redirectOutput{&out};
    CORRADE_COMPARE(converter->validateFile({}, "includes.vert"),
        std::make_pair(true, ""));
    CORRADE_COMPARE(out.str(),
        "Loading includes.vert\n"
        "Loading sub/directory/basics.glsl\n"

        "Loading sub/directory/definitions.glsl\n"
        "Closing sub/directory/definitions.glsl\n"

        "Loading sub/directory/../relative.glsl\n"
        "Closing sub/directory/../relative.glsl\n"

        /* Here it's loading & closing basics.glsl again but because it's
           recursive while it's being in scope, it's not propagated to the
           callback */

        /* Here it's loading & closing relative.glsl again, which is propagated
           to the callback because at this point the refcount reached 0 and the
           original file got already released */
        "Loading sub/directory/../relative.glsl\n"
        "Closing sub/directory/../relative.glsl\n"

        "Closing sub/directory/basics.glsl\n"
        "Closing includes.vert\n");
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
        "ShaderTools::GlslangConverter::validateData(): output format should be Spirv or Unspecified but got ShaderTools::Format::Glsl\n");
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

void GlslangConverterTest::validateWrongOutputFormatForGenericOpenGL() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setOutputFormat(Format::Spirv, "opengl");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateData({}, {}),
        std::make_pair(false, ""));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::GlslangConverter::validateData(): generic OpenGL can't be validated with SPIR-V rules\n");
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
        {"A_DEFINE", ""}
    });
    /* We're interested in the first error only */
    converter->configuration().setValue("cascadingErrors", false);

    /* Don't specify the stage -- vertex will be assumed, which doesn't have
       gl_FragCoord */
    CORRADE_COMPARE(converter->validateData(Stage::Unspecified, Utility::Directory::read(Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, "shader.vk.frag"))),
        std::make_pair(false, /* Yes, trailing whitespace. Fuck me. */
            "ERROR: 0:35: 'gl_FragCoord' : undeclared identifier \n"
            "ERROR: 0:35: '' : compilation terminated \n"
            "ERROR: 2 compilation errors.  No code generated."));
}

void GlslangConverterTest::validateFailFileWrongStage() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    /* Same source as validate() (so it's guaranteed to be valid), just with
       wrong stage */

    converter->setDefinitions({
        {"A_DEFINE", ""}
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
            "ERROR: shader.glsl:35: 'gl_FragCoord' : undeclared identifier \n"
            "ERROR: shader.glsl:35: '' : compilation terminated \n"
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

void GlslangConverterTest::validateFailIncludeNotFound() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setDefinitions({{"MAKE_THIS_BROKEN", ""}});
    /* We're interested just in the include error. Actually, it's interesting
       that when I set this to false (which should result in *less* errors),
       there's an additional error about a missing #endif. Someone inverted the
       condition in there or what? */
    converter->configuration().setValue("cascadingErrors", true);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateFile({}, Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, "includes.vert")),
        std::make_pair(false, Utility::formatString(
            "ERROR: {0}:10: '#include' : Could not process include directive for header name: ../notfound.glsl\n"
            "ERROR: 1 compilation errors.  No code generated.", Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, "includes.vert"))));
    CORRADE_COMPARE(out.str(),
        Utility::formatString("Utility::Directory::read(): can't open {}\n", Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, "../notfound.glsl")));
}

void GlslangConverterTest::convert() {
    auto&& data = ConvertData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    #if !defined(GLSLANG_VERSION_MAJOR) && GLSLANG_PATCH_LEVEL < 3496
    /* GL_ARB_explicit_uniform_location is implemented only since 7.13.3496,
       https://github.com/KhronosGroup/glslang/pull/1880, earlier versions
       spit out the following error and the only way to use explicit uniform
       location is by forcing the version to 430

        ERROR: shader.gl.frag:24: '#extension' : extension not supported: GL_ARB_explicit_uniform_location
        ERROR: shader.gl.frag:27: 'location qualifier on uniform or buffer' : not supported for this version or the enabled extensions

    */
    if(data.filename == "shader.gl.frag"_s)
        CORRADE_SKIP("GL_ARB_explicit_uniform_location only implemented since 7.13.3496.");
    #endif

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setDefinitions({
        {"A_DEFINE", ""},
        {"AN_UNDEFINE", "something awful!!"},
        {"AN_UNDEFINE", nullptr}
    });
    converter->setOutputFormat({}, data.outputVersion);
    if(data.debugInfoLevel)
        converter->setDebugInfoLevel(data.debugInfoLevel);

    /* Fake the file loading via a callback */
    const Containers::Array<char> file = Utility::Directory::read(Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, data.filename));
    converter->setInputFileCallback([](const std::string&, InputFileCallbackPolicy, const Containers::Array<char>& file) -> Containers::Optional<Containers::ArrayView<const char>> {
        return arrayView(file);
    }, file);

    Containers::Array<char> output = converter->convertFileToData(data.stage, data.alias ? data.alias : data.filename);

    /* glslang 7.13 / 8.13 differs from 10 only in the generator version, patch
       that to have the same output */
    auto words = Containers::arrayCast<UnsignedInt>(output);
    if(words.size() >= 3 && (words[2] == 524295 || words[2] == 524296))
        words[2] = 524298;

    CORRADE_COMPARE_AS((std::string{output.begin(), output.end()}),
        Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, data.output),
        TestSuite::Compare::StringToFile);
}

void GlslangConverterTest::convertIncludes() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    /* Checking just that it passed, the rest was verified for validate()
       already */
    CORRADE_VERIFY(converter->convertFileToFile({}, Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, "includes.vert"),
        Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_OUTPUT_DIR, "includes.spv")));
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
        "ShaderTools::GlslangConverter::convertDataToData(): output format should be Spirv or Unspecified but got ShaderTools::Format::Glsl\n");
}

void GlslangConverterTest::convertWrongOutputVersionTarget() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setOutputFormat(Format::Unspecified, "opengl");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::GlslangConverter::convertDataToData(): output format version target should be opengl4.5 or vulkanX.Y but got opengl\n");
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

void GlslangConverterTest::convertWrongDebugInfoLevel() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setDebugInfoLevel("2");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::GlslangConverter::convertDataToData(): debug info level should be 0, 1 or empty but got 2\n");
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
        {"A_DEFINE", ""}
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
        "ERROR: 0:35: 'gl_FragCoord' : undeclared identifier \n"
        "ERROR: 0:35: '' : compilation terminated \n"
        "ERROR: 2 compilation errors.  No code generated.\n");
}

void GlslangConverterTest::convertFailFileWrongStage() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    /* Same source as validate() (so it's guaranteed to be valid), just with
       wrong stage */

    converter->setDefinitions({
        {"A_DEFINE", ""}
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
        "ERROR: shader.glsl:35: 'gl_FragCoord' : undeclared identifier \n"
        "ERROR: shader.glsl:35: '' : compilation terminated \n"
        "ERROR: 2 compilation errors.  No code generated.\n");
}

void GlslangConverterTest::vulkanNoExplicitLayout() {
    auto&& data = VulkanNoExplicitLayoutData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("GlslangShaderConverter");

    converter->setDefinitions({
        {"A_DEFINE", ""},
        {data.define, ""}
    });
    converter->setOutputFormat(data.outputFormat, {});
    /* We're interested in the first error only */
    converter->configuration().setValue("cascadingErrors", false);

    /* Glslang SPIR-V validation rules can be enforced via multiple different
       settings and each setting affect only a subset of these, so verify that
       we're consistent in all cases */
    std::pair<bool, Containers::String> result = converter->validateData(Stage::Fragment, Utility::Directory::read(Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, "shader.vk.frag")));
    CORRADE_COMPARE(result, std::make_pair(false, data.error));

    /* Conversion should result in exactly the same */
    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData(Stage::Fragment, Utility::Directory::read(Utility::Directory::join(GLSLANGSHADERCONVERTER_TEST_DIR, "shader.vk.frag"))));
    CORRADE_COMPARE(out.str(), Utility::formatString("ShaderTools::GlslangConverter::convertDataToData(): compilation failed:\n{}\n", data.error));
}

}}}}

CORRADE_TEST_MAIN(Magnum::ShaderTools::Test::GlslangConverterTest)
