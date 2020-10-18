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
#include <Magnum/ShaderTools/AbstractConverter.h>

#include "configure.h"

namespace Magnum { namespace ShaderTools { namespace Test { namespace {

struct SpirvToolsConverterTest: TestSuite::Tester {
    explicit SpirvToolsConverterTest();

    void convertNoOp();
    void convertDisassemble();
    void convertAssemble();
    void convertDisassembleFile();
    void convertAssembleFile();
    void convertWrongInputFormat();
    void convertWrongInputVersion();
    void convertWrongOutputFormat();
    void convertWrongOutputVersion();
    void convertDisassembleExplicitFormatEmptyData();
    void convertDisassembleFail();
    void convertDisassembleFailFile();
    void convertAssembleExplicitFormatEmptyData();
    void convertAssembleFail();
    void convertAssembleFailFile();
    void convertBinarySizeNotDivisibleByFour();

    /* Explicitly forbid system-wide plugin dependencies */
    PluginManager::Manager<AbstractConverter> _converterManager{"nonexistent"};
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

SpirvToolsConverterTest::SpirvToolsConverterTest() {
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
              &SpirvToolsConverterTest::convertDisassembleExplicitFormatEmptyData,
              &SpirvToolsConverterTest::convertDisassembleFail,
              &SpirvToolsConverterTest::convertDisassembleFailFile,
              &SpirvToolsConverterTest::convertAssembleExplicitFormatEmptyData,
              &SpirvToolsConverterTest::convertAssembleFail,
              &SpirvToolsConverterTest::convertAssembleFailFile,
              &SpirvToolsConverterTest::convertBinarySizeNotDivisibleByFour});

    /* Load the plugin directly from the build tree. Otherwise it's static and
       already loaded. */
    #ifdef SPIRVTOOLSSHADERCONVERTER_PLUGIN_FILENAME
    CORRADE_INTERNAL_ASSERT_OUTPUT(_converterManager.load(SPIRVTOOLSSHADERCONVERTER_PLUGIN_FILENAME) & PluginManager::LoadState::Loaded);
    #endif
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

void SpirvToolsConverterTest::convertDisassembleFail() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");
    converter->setInputFormat(Format::Spirv);
    converter->setOutputFormat(Format::SpirvAssembly);

    const UnsignedInt data[] {
        0x07230203, 99, 0xbadc0de, 666, 0xfffull,
        /* length=2, OpCapability CapabilityShader */
        (2 << 16) | 17, 1,
        /* length=3, OpMemoryModel Logical GLSL450 */
        (3 << 16) | 14, 0, 1,
        0xdeadf00l /* third instruction */
    };

    std::ostringstream out;
    Error redirectError{&out};
    CORRADE_VERIFY(!converter->convertDataToData({}, data));
    CORRADE_COMPARE(out.str(),
        "ShaderTools::SpirvToolsConverter::convertDataToData(): disassembly failed: <data>:3: Invalid opcode: 57088\n");
}

void SpirvToolsConverterTest::convertDisassembleFailFile() {
    Containers::Pointer<AbstractConverter> converter = _converterManager.instantiate("SpirvToolsShaderConverter");
    converter->setInputFormat(Format::Spirv);
    converter->setOutputFormat(Format::SpirvAssembly);

    const UnsignedInt data[] {
        0x07230203, 99, 0xbadc0de, 666, 0xfffull,
        /* length=2, OpCapability CapabilityShader */
        (2 << 16) | 17, 1,
        /* length=3, OpMemoryModel Logical GLSL450 */
        (3 << 16) | 14, 0, 1,
        0xdeadf00l /* third instruction */
    };

    Containers::ArrayView<const UnsignedInt> dataView = data;

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
    CORRADE_VERIFY(!converter->convertDataToData({}, data));
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

}}}}

CORRADE_TEST_MAIN(Magnum::ShaderTools::Test::SpirvToolsConverterTest)
