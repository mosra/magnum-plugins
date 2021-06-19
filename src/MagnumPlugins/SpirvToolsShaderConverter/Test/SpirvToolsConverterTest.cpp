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
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/TestSuite/Tester.h>
#include <Corrade/TestSuite/Compare/StringToFile.h>
#include <Corrade/TestSuite/Compare/File.h>
#include <Corrade/TestSuite/Compare/Container.h>
#include <Corrade/TestSuite/Compare/Numeric.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/DebugStl.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/FormatStl.h>
#include <Magnum/ShaderTools/AbstractConverter.h>

#include "configure.h"
#include "MagnumPlugins/SpirvToolsShaderConverter/configureInternal.h"

namespace Magnum { namespace ShaderTools { namespace Test { namespace {

struct SpirvToolsConverterTest: TestSuite::Tester {
    explicit SpirvToolsConverterTest();

    void validate();
    void validateFile();
    void validateWrongInputFormat();
    void validateWrongInputVersion();
    void validateWrongOutputFormat();
    void validateWrongOutputVersion();
    void validateFailWhole();
    void validateFailInstruction();
    void validateFailFileWhole();
    void validateFailFileInstruction();
    void validateFailAssemble();
    void validateFailAssembleFile();
    void validateBinarySizeNotDivisbleByFour();

    void convertNoOp();
    void convertDisassemble();
    void convertAssemble();
    void convertDisassembleFile();
    void convertAssembleFile();
    void convertWrongInputFormat();
    void convertWrongInputVersion();
    void convertWrongOutputFormat();
    void convertWrongOutputVersion();
    void convertWrongOptimizationLevel();
    void convertDisassembleExplicitFormatEmptyData();
    void convertDisassembleFail();
    void convertDisassembleFailFile();
    void convertAssembleExplicitFormatEmptyData();
    void convertAssembleFail();
    void convertAssembleFailFile();
    void convertBinarySizeNotDivisibleByFour();

    void convertOptimize();
    void convertOptimizeFail();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractConverter> _converterManager{"nonexistent"};
};

const struct {
    const char* name;
    const char* filename;
} ValidateData[] {
    {"binary", "triangle-shaders.spv"},
    {"assembly", "triangle-shaders.spvasm"}
};

const struct {
    const char* name;
    const char* pluginNameDisassemble;
    Containers::Optional<Format> inputFormatDisassemble,
        outputFormatDisassemble;
    const char* pluginNameAssemble;
    Containers::Optional<Format> inputFormatAssemble;
    /* Not optional because we're setting the output format & version always */
    Format outputFormatAssemble;
} DisAssembleData[] {
    {"via plugin name",
        "SpirvToSpirvAssemblyShaderConverter", {}, {},
        "SpirvAssemblyToSpirvShaderConverter", {}, Format::Spirv},
    {"via plugin name + input format override",
        "SpirvAssemblyShaderConverter", Format::Spirv, {},
        "SpirvShaderConverter", Format::SpirvAssembly, Format::Spirv},
    {"via plugin name + output format override",
        "SpirvShaderConverter", {}, Format::SpirvAssembly,
        "SpirvAssemblyShaderConverter", {}, Format::Spirv},
    {"via input and output format",
        "SpirvToolsShaderConverter", Format::Spirv, Format::SpirvAssembly,
        "SpirvToolsShaderConverter", Format::SpirvAssembly, Format::Spirv}
};

const struct {
    const char* name;
    Containers::Optional<Format> outputFormatDisassemble;
    Format outputFormatAssemble;
    const char* outputFilenameDisssemble;
    const char* outputFilenameAssemble;
} DisAssembleFileData[] {
    {"via format, arbitrary filename",
        Format::SpirvAssembly, Format::Spirv,
        "shader.dat", "shader.dat"},
    {"via format, conflicting filename that gets ignored",
        Format::SpirvAssembly, Format::Spirv,
        "shader.spv", "shader.spvasm"},
    {"via filename, format unspecified",
        {}, Format::Unspecified,
        /* Defaults to SPIR-V binary, so it works even without .spv */
        "shader.spvasm", "shader.dat"}
};

const struct {
    const char* name;
    const char* level;
    const char* input;
    const char* expected;
    Format outputFormat;
} OptimizeData[] {
    /* This just tests that the input binary corresponds to the assembly,
       which is a trivially patched variant of triangle-shaders.spvasm */
    {"-O0, assembly to binary",
        "0", "triangle-shaders.noopt.spvasm",
        "triangle-shaders.noopt.spv", Format::Spirv},
    {"binary to binary",
        "1", "triangle-shaders.noopt.spv",
        "triangle-shaders.spv", Format::Spirv},
    {"binary to assembly",
        "1", "triangle-shaders.noopt.spv",
        "triangle-shaders.spvasm", Format::SpirvAssembly},
    {"assembly to binary",
        "1", "triangle-shaders.noopt.spvasm",
        "triangle-shaders.spv", Format::Spirv},
    {"assembly to assembly",
        "1", "triangle-shaders.noopt.spvasm",
        "triangle-shaders.spvasm", Format::SpirvAssembly},
    {"-Os",
        "s", "triangle-shaders.noopt.spv",
        "triangle-shaders.spv", Format::Spirv},
    {"HLSL legalization",
        "legalizeHlsl", "triangle-shaders.noopt.spv",
        "triangle-shaders.spv", Format::Spirv}
};

SpirvToolsConverterTest::SpirvToolsConverterTest() {
    addInstancedTests({&SpirvToolsConverterTest::validate,
                       &SpirvToolsConverterTest::validateFile},
        Containers::arraySize(ValidateData));

    addTests({&SpirvToolsConverterTest::validateWrongInputFormat,
              &SpirvToolsConverterTest::validateWrongInputVersion,
              &SpirvToolsConverterTest::validateWrongOutputFormat,
              &SpirvToolsConverterTest::validateWrongOutputVersion});

    addInstancedTests({&SpirvToolsConverterTest::validateFailWhole,
                       &SpirvToolsConverterTest::validateFailInstruction,
                       &SpirvToolsConverterTest::validateFailFileWhole,
                       &SpirvToolsConverterTest::validateFailFileInstruction},
        Containers::arraySize(ValidateData));

    addTests({&SpirvToolsConverterTest::validateFailAssemble,
              &SpirvToolsConverterTest::validateFailAssembleFile,
              &SpirvToolsConverterTest::validateBinarySizeNotDivisbleByFour});

    addTests({&SpirvToolsConverterTest::convertNoOp});

    addInstancedTests({&SpirvToolsConverterTest::convertDisassemble,
                       &SpirvToolsConverterTest::convertAssemble},
        Containers::arraySize(DisAssembleData));

    addInstancedTests({&SpirvToolsConverterTest::convertDisassembleFile,
                       &SpirvToolsConverterTest::convertAssembleFile},
        Containers::arraySize(DisAssembleFileData));

    addTests({&SpirvToolsConverterTest::convertWrongInputFormat,
              &SpirvToolsConverterTest::convertWrongInputVersion,
              &SpirvToolsConverterTest::convertWrongOutputFormat,
              &SpirvToolsConverterTest::convertWrongOutputVersion,
              &SpirvToolsConverterTest::convertWrongOptimizationLevel,
              &SpirvToolsConverterTest::convertDisassembleExplicitFormatEmptyData,
              &SpirvToolsConverterTest::convertDisassembleFail,
              &SpirvToolsConverterTest::convertDisassembleFailFile,
              &SpirvToolsConverterTest::convertAssembleExplicitFormatEmptyData,
              &SpirvToolsConverterTest::convertAssembleFail,
              &SpirvToolsConverterTest::convertAssembleFailFile,
              &SpirvToolsConverterTest::convertBinarySizeNotDivisibleByFour});

    addInstancedTests({&SpirvToolsConverterTest::convertOptimize},
        Containers::arraySize(OptimizeData));

    addTests({&SpirvToolsConverterTest::convertOptimizeFail});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef SPIRVTOOLSSHADERCONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(SPIRVTOOLSSHADERCONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
}

void SpirvToolsConverterTest::validate() {
    auto&& data = ValidateData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    /* The input is in SPIR-V 1.2, but isn't valid for Vulkan 1.1 because of
       OpExecutionMode OriginLowerLeft (which is used below to test failures),
       so using just general SPIR-V validation. With OpExecutionMode missing it
       would not even validate as SPIR-V. */
    converter->setOutputFormat({}, "spv1.2");

    CORRADE_COMPARE(converter->validateData({}, Utility::Directory::read(Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, data.filename))),
        std::make_pair(true, Containers::String{}));
}

void SpirvToolsConverterTest::validateFile() {
    auto&& data = ValidateData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    /* The input is in SPIR-V 1.2, but isn't valid for Vulkan 1.1 because of
       OpExecutionMode OriginLowerLeft (which is used below to test failures),
       so using just general SPIR-V validation. With OpExecutionMode missing it
       would not even validate as SPIR-V. */
    converter->setOutputFormat({}, "spv1.2");

    CORRADE_COMPARE(converter->validateFile({}, Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, data.filename)),
        std::make_pair(true, Containers::String{}));
}

void SpirvToolsConverterTest::validateWrongInputFormat() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    converter->setInputFormat(Format::Glsl);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateData({}, {}),
        std::make_pair(false, ""));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::validateData(): input format should be Spirv, SpirvAssembly or Unspecified but got ShaderTools::Format::Glsl\n");
}

void SpirvToolsConverterTest::validateWrongInputVersion() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    converter->setInputFormat(Format::Spirv, "vulkan1.1");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateData({}, {}),
        std::make_pair(false, ""));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::validateData(): input format version should be empty but got vulkan1.1\n");
}

void SpirvToolsConverterTest::validateWrongOutputFormat() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    converter->setOutputFormat(Format::Spirv);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateData({}, {}),
        std::make_pair(false, ""));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::validateData(): output format should be Unspecified but got ShaderTools::Format::Spirv\n");
}

void SpirvToolsConverterTest::validateWrongOutputVersion() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    converter->setOutputFormat(Format::Unspecified, "vulkan2.1");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateData({}, {}),
        std::make_pair(false, ""));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::validateData(): unrecognized output format version vulkan2.1\n");
}

void SpirvToolsConverterTest::validateFailWhole() {
    auto&& data = ValidateData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    /* Set ID limit too low to make it fail */
    converter->setOutputFormat({}, "spv1.2");
    converter->configuration().setValue("maxIdBound", 15);

    CORRADE_COMPARE(converter->validateData({}, Utility::Directory::read(Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, data.filename))),
        /* Wow fuck me why the double spaces. IT'S NOT A TYPEWRITER AGE ANYMORE
           RECONSIDER YOUR LIFE CHOICES FFS */
        std::make_pair(false, "<data>: Invalid SPIR-V.  The id bound is larger than the max id bound 15."));
}

void SpirvToolsConverterTest::validateFailInstruction() {
    auto&& data = ValidateData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    /* Valid SPIR-V 1.2, but isn't valid for Vulkan 1.1 because of a lower-left
       origin */
    converter->setOutputFormat({}, "vulkan1.1");

    const char* const expected =
        #if SPIRVTOOLS_VERSION >= 202007
        "<data>:5: [VUID-StandaloneSpirv-OriginLowerLeft-04653] In the Vulkan environment, the OriginLowerLeft execution mode must not be used.\n  OpExecutionMode %2 OriginLowerLeft"
        #else
        "<data>:5: In the Vulkan environment, the OriginLowerLeft execution mode must not be used.\n  OpExecutionMode %2 OriginLowerLeft"
        #endif
        ;
    CORRADE_COMPARE(converter->validateData({}, Utility::Directory::read(Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, data.filename))),
        std::make_pair(false, expected));
}

void SpirvToolsConverterTest::validateFailFileWhole() {
    auto&& data = ValidateData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    /* Fake the file loading via a callback so we don't have a YUUGE path in
       the output */
    const Containers::Array<char> file = Utility::Directory::read(Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, data.filename));
    converter->setInputFileCallback([](const std::string&, InputFileCallbackPolicy, const Containers::Array<char>& file) -> Containers::Optional<Containers::ArrayView<const char>> {
        return arrayView(file);
    }, file);

    /* Set ID limit too low to make it fail */
    converter->setOutputFormat({}, "spv1.2");
    converter->configuration().setValue("maxIdBound", 15);

    CORRADE_COMPARE(converter->validateFile({}, data.filename),
        std::make_pair(false, Utility::formatString("{}: Invalid SPIR-V.  The id bound is larger than the max id bound 15.", data.filename)));
    /* Validating data again should not be using the stale filename */
    CORRADE_COMPARE(converter->validateData({}, file),
        std::make_pair(false, "<data>: Invalid SPIR-V.  The id bound is larger than the max id bound 15."));
}

void SpirvToolsConverterTest::validateFailFileInstruction() {
    auto&& data = ValidateData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    /* Fake the file loading via a callback so we don't have a YUUGE path in
       the output */
    const Containers::Array<char> file = Utility::Directory::read(Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, data.filename));
    converter->setInputFileCallback([](const std::string&, InputFileCallbackPolicy, const Containers::Array<char>& file) -> Containers::Optional<Containers::ArrayView<const char>> {
        return arrayView(file);
    }, file);

    /* Valid SPIR-V 1.2, but isn't valid for Vulkan 1.1 because of a lower-left
       origin */
    converter->setOutputFormat({}, "vulkan1.1");

    const char* const expected =
        #if SPIRVTOOLS_VERSION >= 202007
        "[VUID-StandaloneSpirv-OriginLowerLeft-04653] In the Vulkan environment, the OriginLowerLeft execution mode must not be used.\n  OpExecutionMode %2 OriginLowerLeft"
        #else
        "In the Vulkan environment, the OriginLowerLeft execution mode must not be used.\n  OpExecutionMode %2 OriginLowerLeft"
        #endif
        ;
    CORRADE_COMPARE(converter->validateFile({}, data.filename),
        std::make_pair(false, Utility::formatString("{}:5: {}", data.filename, expected)));
    /* Validating data again should not be using the stale filename */
    CORRADE_COMPARE(converter->validateData({}, file),
        std::make_pair(false, Utility::formatString("<data>:5: {}", expected)));
}

void SpirvToolsConverterTest::validateFailAssemble() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");
    converter->setInputFormat(Format::SpirvAssembly);

    const char data[] = R"(
        OpCapability Shader
        OpMemoryModel Logical GLSL450
        OpDeadFool
)";

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateData({}, data),
        std::make_pair(false, Containers::String{}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::validateData(): assembly failed: <data>:4:9: Invalid Opcode name 'OpDeadFool'\n");
}

void SpirvToolsConverterTest::validateFailAssembleFile() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");
    converter->setInputFormat(Format::SpirvAssembly);

    const char data[] = R"(
        OpCapability Shader
        OpMemoryModel Logical GLSL450
        OpDeadFool
)";

    Containers::ArrayView<const char> dataView = data;

    /* Fake the file loading via a callback */
    converter->setInputFileCallback([](const std::string&, InputFileCallbackPolicy, Containers::ArrayView<const char>& data) -> Containers::Optional<Containers::ArrayView<const char>> {
        return data;
    }, dataView);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateFile({}, "deadfool.spvasm"),
        std::make_pair(false, Containers::String{}));
    /* Validating data again should not be using the stale filename */
    CORRADE_COMPARE(converter->validateData({}, data),
        std::make_pair(false, Containers::String{}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::validateData(): assembly failed: deadfool.spvasm:4:9: Invalid Opcode name 'OpDeadFool'\n"
        "ShaderTools::SpirvToolsConverter::validateData(): assembly failed: <data>:4:9: Invalid Opcode name 'OpDeadFool'\n");
}

void SpirvToolsConverterTest::validateBinarySizeNotDivisbleByFour() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    /* Set the input format explicitly so we don't need to convince the
       autodetection */
    converter->setInputFormat(Format::Spirv);
    const char data[37]{};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_COMPARE(converter->validateData({}, data),
        std::make_pair(false, Containers::String{}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): SPIR-V binary size not divisible by four: 37 bytes\n");
}

void SpirvToolsConverterTest::convertNoOp() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    /* Passing anything after the binary signature should work because it just
       copies them over */
    const UnsignedInt spirvData[]{
        0x07230203, 99, 0xbadc0de, 666, 0xfffull, 0xdead
    };

    /* Invalid stages are also fine, should output exactly the same thing */
    CORRADE_COMPARE_AS(converter->convertDataToData(Stage(0xc0fffe), spirvData),
        Containers::arrayCast<const char>(Containers::arrayView(spirvData)),
        TestSuite::Compare::Container);
}

void SpirvToolsConverterTest::convertDisassemble() {
    auto&& data = DisAssembleData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate(data.pluginNameDisassemble);
    if(data.inputFormatDisassemble)
        converter->setInputFormat(*data.inputFormatDisassemble);
    if(data.outputFormatDisassemble)
        converter->setOutputFormat(*data.outputFormatDisassemble);

    /* Disable features that make the output nicer to read but not
       roundtrippable */
    converter->configuration().setValue("friendlyNames", false);
    converter->configuration().setValue("header", false);

    /** @todo ugh the casts are AWFUL, FIX FFS */
    CORRADE_COMPARE_AS(Containers::StringView{Containers::ArrayView<const char>{converter->convertFileToData({}, Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, "triangle-shaders.spv"))}},
        Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, "triangle-shaders.spvasm"),
        TestSuite::Compare::StringToFile);
}

void SpirvToolsConverterTest::convertAssemble() {
    auto&& data = DisAssembleData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate(data.pluginNameAssemble);

    if(data.inputFormatAssemble)
        converter->setInputFormat(*data.inputFormatAssemble);
    /* Testing with a non-null-terminated version string -- it should be copied
       to a new null-terminated string internally as spvParseTargetEnv() works
       only with null-terminated strings */
    {
        using namespace Containers::Literals;
        converter->setOutputFormat(data.outputFormatAssemble, "spv1.23"_s.except(1));
    }

    /* Otherwise the output will not be roundtrippable */
    converter->configuration().setValue("preserveNumericIds", true);

    Containers::Array<char> out = converter->convertFileToData({}, Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, "triangle-shaders.spvasm"));
    CORRADE_COMPARE_AS(out.size(), 5*4, TestSuite::Compare::Greater);

    /* The output generator ID is something from Khronos, patch it back to ours
       so the file compares equal. */
    CORRADE_COMPARE(Containers::arrayCast<UnsignedInt>(out.prefix(5*4))[2],
        0x70000);
    Containers::arrayCast<UnsignedInt>(out.prefix(5*4))[2] = 0xdeadc0de;

    /** @todo ugh the casts are AWFUL, FIX FFS */
    CORRADE_COMPARE_AS(Containers::StringView{Containers::ArrayView<const char>{out}},
        Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, "triangle-shaders.spv"),
        TestSuite::Compare::StringToFile);
}

void SpirvToolsConverterTest::convertDisassembleFile() {
    auto&& data = DisAssembleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");
    if(data.outputFormatDisassemble)
        converter->setOutputFormat(*data.outputFormatDisassemble);

    /* Disable features that make the output nicer to read but not
       roundtrippable */
    converter->configuration().setValue("friendlyNames", false);
    converter->configuration().setValue("header", false);

    const std::string filename = Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_OUTPUT_DIR, data.outputFilenameDisssemble);
    CORRADE_VERIFY(converter->convertFileToFile({},
        Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, "triangle-shaders.spv"),
        filename));

    CORRADE_COMPARE_AS(filename,
        Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, "triangle-shaders.spvasm"),
        TestSuite::Compare::File);
}

void SpirvToolsConverterTest::convertAssembleFile() {
    auto&& data = DisAssembleFileData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    /* Testing with a non-null-terminated version string -- it should be copied
       to a new null-terminated string internally as spvParseTargetEnv() works
       only with null-terminated strings */
    {
        using namespace Containers::Literals;
        converter->setOutputFormat(data.outputFormatAssemble, "spv1.23"_s.except(1));
    }

    /* Otherwise the output will not be roundtrippable */
    converter->configuration().setValue("preserveNumericIds", true);

    const std::string filename = Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_OUTPUT_DIR, data.outputFilenameAssemble);
    CORRADE_VERIFY(converter->convertFileToFile({},
        Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, "triangle-shaders.spvasm"),
        filename));

    Containers::Array<char> out = Utility::Directory::read(filename);
    CORRADE_COMPARE_AS(out.size(), 5*4, TestSuite::Compare::Greater);

    /* The output generator ID is something from Khronos, patch it back to ours
       so the file compares equal. */
    CORRADE_COMPARE(Containers::arrayCast<UnsignedInt>(out.prefix(5*4))[2],
        0x70000);
    Containers::arrayCast<UnsignedInt>(out.prefix(5*4))[2] = 0xdeadc0de;

    /** @todo ugh the casts are AWFUL, FIX FFS */
    CORRADE_COMPARE_AS(Containers::StringView{Containers::ArrayView<const char>{out}},
        Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, "triangle-shaders.spv"),
        TestSuite::Compare::StringToFile);
}

void SpirvToolsConverterTest::convertWrongInputFormat() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    converter->setInputFormat(Format::Glsl);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): input format should be Spirv, SpirvAssembly or Unspecified but got ShaderTools::Format::Glsl\n");
}

void SpirvToolsConverterTest::convertWrongInputVersion() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    converter->setInputFormat(Format::Spirv, "vulkan1.1");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): input format version should be empty but got vulkan1.1\n");
}

void SpirvToolsConverterTest::convertWrongOutputFormat() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    converter->setOutputFormat(Format::Glsl);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): output format should be Spirv, SpirvAssembly or Unspecified but got ShaderTools::Format::Glsl\n");
}

void SpirvToolsConverterTest::convertWrongOutputVersion() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    converter->setOutputFormat(Format::Spirv, "vulkan2.1");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): unrecognized output format version vulkan2.1\n");
}

void SpirvToolsConverterTest::convertWrongOptimizationLevel() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    converter->setOptimizationLevel("2");
    /* Force input format to binary so it doesn't go through disassembly (and
       fail on that) */
    converter->setInputFormat(Format::Spirv);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): optimization level should be 0, 1, s, legalizeHlsl or empty but got 2\n");
}

void SpirvToolsConverterTest::convertDisassembleExplicitFormatEmptyData() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");
    converter->setInputFormat(Format::Spirv);
    converter->setOutputFormat(Format::SpirvAssembly);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    /* No instruction index printed here */
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): disassembly failed: <data>: Missing module.\n");
}

constexpr UnsignedInt InvalidInstructionData[] {
    /* Magic, version, generator magic, bound, reserved. Version low and
       high byte has to be zero, otherwise 2020.5 and newer fails with
       cryptic "Internal error: unhandled header parse failure". */
    0x07230203, 0x00010000, 0xbadc0de, 666, 0xfffull,
    /* length=2, OpCapability CapabilityShader */
    (2 << 16) | 17, 1,
    /* length=3, OpMemoryModel Logical GLSL450 */
    (3 << 16) | 14, 0, 1,
    0xdeadf00l /* third instruction */
};

void SpirvToolsConverterTest::convertDisassembleFail() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");
    converter->setInputFormat(Format::Spirv);
    converter->setOutputFormat(Format::SpirvAssembly);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, InvalidInstructionData));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): disassembly failed: <data>:3: Invalid opcode: 57088\n");
}

void SpirvToolsConverterTest::convertDisassembleFailFile() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");
    converter->setInputFormat(Format::Spirv);
    converter->setOutputFormat(Format::SpirvAssembly);

    Containers::ArrayView<const UnsignedInt> dataView = InvalidInstructionData;

    /* Fake the file loading via a callback */
    converter->setInputFileCallback([](const std::string&, InputFileCallbackPolicy, Containers::ArrayView<const UnsignedInt>& data) -> Containers::Optional<Containers::ArrayView<const char>> {
        /** @todo drop the cast once callbacks return void views */
        return Containers::arrayCast<const char>(data);
    }, dataView);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertFileToData({}, "deadfool.spv"));
    /* Test the doConvertFileToFile() intercept too */
    CORRADE_VERIFY(!converter->convertFileToFile({}, "another.spv", ""));
    /* Converting data again should not be using the stale filename */
    CORRADE_VERIFY(!converter->convertDataToData({}, InvalidInstructionData));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): disassembly failed: deadfool.spv:3: Invalid opcode: 57088\n"
        "ShaderTools::SpirvToolsConverter::convertDataToData(): disassembly failed: another.spv:3: Invalid opcode: 57088\n"
        "ShaderTools::SpirvToolsConverter::convertDataToData(): disassembly failed: <data>:3: Invalid opcode: 57088\n");
}

void SpirvToolsConverterTest::convertAssembleExplicitFormatEmptyData() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");
    converter->setInputFormat(Format::SpirvAssembly);
    converter->setOutputFormat(Format::Spirv);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, {}));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): assembly failed: <data>:1:1: Missing assembly text.\n");
}

void SpirvToolsConverterTest::convertAssembleFail() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");
    converter->setInputFormat(Format::SpirvAssembly);
    converter->setOutputFormat(Format::Spirv);

    const char data[] = R"(
        OpCapability Shader
        OpMemoryModel Logical GLSL450
        OpDeadFool
)";

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, data));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): assembly failed: <data>:4:9: Invalid Opcode name 'OpDeadFool'\n");
}

void SpirvToolsConverterTest::convertAssembleFailFile() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");
    converter->setInputFormat(Format::SpirvAssembly);
    converter->setOutputFormat(Format::Spirv);

    const char data[] = R"(
        OpCapability Shader
        OpMemoryModel Logical GLSL450
        OpDeadFool
)";

    Containers::ArrayView<const char> dataView = data;

    /* Fake the file loading via a callback */
    converter->setInputFileCallback([](const std::string&, InputFileCallbackPolicy, Containers::ArrayView<const char>& data) -> Containers::Optional<Containers::ArrayView<const char>> {
        return data;
    }, dataView);

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertFileToData({}, "deadfool.spvasm"));
    /* Test the doConvertFileToFile() intercept too */
    CORRADE_VERIFY(!converter->convertFileToFile({}, "another.spvasm", ""));
    /* Converting data again should not be using the stale filename */
    CORRADE_VERIFY(!converter->convertDataToData({}, data));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): assembly failed: deadfool.spvasm:4:9: Invalid Opcode name 'OpDeadFool'\n"
        "ShaderTools::SpirvToolsConverter::convertDataToData(): assembly failed: another.spvasm:4:9: Invalid Opcode name 'OpDeadFool'\n"
        "ShaderTools::SpirvToolsConverter::convertDataToData(): assembly failed: <data>:4:9: Invalid Opcode name 'OpDeadFool'\n");
}

void SpirvToolsConverterTest::convertBinarySizeNotDivisibleByFour() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    /* Set the input format explicitly so we don't need to convince the
       autodetection */
    converter->setInputFormat(Format::Spirv);
    const char data[37]{};

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, data));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): SPIR-V binary size not divisible by four: 37 bytes\n");
}

void SpirvToolsConverterTest::convertOptimize() {
    auto&& data = OptimizeData[testCaseInstanceId()];
    setTestCaseDescription(data.name);

    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    converter->setOptimizationLevel(data.level);

    /* Disable features that make the binary more compact and assembly nicer to
       read but not roundtrippable */
    converter->configuration().setValue("preserveNumericIds", true);
    converter->configuration().setValue("friendlyNames", false);
    converter->configuration().setValue("header", false);

    /* The input is in SPIR-V 1.2, but isn't valid for Vulkan 1.1 because of
       OpExecutionMode OriginLowerLeft (which is used above to test failures),
       so using just general SPIR-V validation. With OpExecutionMode missing it
       would not even validate as SPIR-V.

       This is here in order to match the original triangle-shaders.spv, which
       use the same target version. */
    converter->setOutputFormat(data.outputFormat, "spv1.2");

    Containers::Array<char> out = converter->convertFileToData({},
        Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, data.input));
    CORRADE_COMPARE_AS(out.size(), 5*4, TestSuite::Compare::Greater);

    /* If we end up with a binary and the input was an assembly, the output
       generator ID is something from Khronos, patch it back to ours so the
       files compare equal. */
    if(data.outputFormat == Format::Spirv && Containers::arrayCast<UnsignedInt>(out.prefix(5*4))[2] == 0x70000)
        Containers::arrayCast<UnsignedInt>(out.prefix(5*4))[2] = 0xdeadc0de;

    /** @todo ugh the casts are AWFUL, FIX FFS */
    CORRADE_COMPARE_AS(Containers::StringView{Containers::ArrayView<const char>{out}},
        Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, data.expected),
        TestSuite::Compare::StringToFile);
}

void SpirvToolsConverterTest::convertOptimizeFail() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");

    converter->setOptimizationLevel("1");

    /* This makes the validation fail because of a lower-left origin (same as
       in validateFailInstruction()) */
    converter->setOutputFormat({}, "vulkan1.1");

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertFileToData({},
        Utility::Directory::join(SPIRVTOOLSSHADERCONVERTER_TEST_DIR, "triangle-shaders.noopt.spv")));

    const char* const expected =
        #if SPIRVTOOLS_VERSION >= 202007
        "[VUID-StandaloneSpirv-OriginLowerLeft-04653] In the Vulkan environment, the OriginLowerLeft execution mode must not be used.\n  OpExecutionMode %2 OriginLowerLeft"
        #else
        "In the Vulkan environment, the OriginLowerLeft execution mode must not be used.\n  OpExecutionMode %2 OriginLowerLeft"
        #endif
        ;
    CORRADE_COMPARE(out.str(), Utility::formatString(
        "ShaderTools::SpirvToolsConverter::convertDataToData(): optimization error:\n"
        "<data>:5: {}\n", expected));
}

}}}}

CORRADE_TEST_MAIN(Magnum::ShaderTools::Test::SpirvToolsConverterTest)
