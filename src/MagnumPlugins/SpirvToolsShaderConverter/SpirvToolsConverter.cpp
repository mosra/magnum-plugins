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

#include "SpirvToolsConverter.h"

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/FormatStl.h> /** @todo remove once format() produces a String */

#include "spirv-tools/libspirv.h"
/* Unfortunately the C optimizer interface is so minimal that it's useless. No
   way to set any optimization preset, even. */
#include "spirv-tools/optimizer.hpp"
#include "MagnumPlugins/SpirvToolsShaderConverter/configureInternal.h"

namespace Magnum { namespace ShaderTools {

using namespace Containers::Literals;

struct SpirvToolsConverter::State {
    /* Initialized in the constructor */
    Format inputFormat, outputFormat;
    Containers::String inputVersion, outputVersion;

    Containers::String inputFilename, outputFilename;

    Containers::String optimizationLevel;
};

SpirvToolsConverter::SpirvToolsConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractConverter{manager, plugin}, _state{InPlaceInit} {
    /* If the plugin was loaded through some of the aliases, set implicit
       input/output formats */
    if(plugin == "SpirvAssemblyToSpirvShaderConverter") {
        _state->inputFormat = Format::SpirvAssembly;
        _state->outputFormat = Format::Spirv;
    } else if(plugin == "SpirvToSpirvAssemblyShaderConverter") {
        _state->inputFormat = Format::Spirv;
        _state->outputFormat = Format::SpirvAssembly;
    } else if(plugin == "SpirvShaderConverter") {
        _state->inputFormat = Format::Spirv;
        _state->outputFormat = Format::Spirv;
    } else if(plugin == "SpirvAssemblyShaderConverter") {
        _state->inputFormat = Format::SpirvAssembly;
        _state->outputFormat = Format::SpirvAssembly;
    } else {
        _state->inputFormat = Format::Unspecified;
        _state->outputFormat = Format::Unspecified;
    }
}

/** @todo is spvTargetEnvDescription() useful for something? in verbose output
    maybe? */

ConverterFeatures SpirvToolsConverter::doFeatures() const {
    return ConverterFeature::ValidateData|ConverterFeature::ConvertData|ConverterFeature::Optimize|
        /* We actually don't, but without this set the doValidateFile() /
           doConvertFileTo*() intercepts don't get called when the input is
           specified through callbacks. And since we delegate to the base
           implementation, the callbacks *do* work. */
        ConverterFeature::InputFileCallback;
}

void SpirvToolsConverter::doSetInputFormat(const Format format, const Containers::StringView version) {
    _state->inputFormat = format;
    _state->inputVersion = Containers::String::nullTerminatedGlobalView(version);
}

void SpirvToolsConverter::doSetOutputFormat(const Format format, const Containers::StringView version) {
    _state->outputFormat = format;
    _state->outputVersion = Containers::String::nullTerminatedGlobalView(version);
}

void SpirvToolsConverter::doSetOptimizationLevel(const Containers::StringView level) {
    _state->optimizationLevel = Containers::String::nullTerminatedGlobalView(level);
}

namespace {

/* A copy of spvDiagnosticPrint(), printing via our APIs */
void printDiagnostic(Debug& out, const Containers::StringView filename, const spv_diagnostic& diagnostic) {
    CORRADE_INTERNAL_ASSERT(diagnostic);

    out << (filename.isEmpty() ? "<data>" : filename) << Debug::nospace << ":";

    /* SPIRV-Tools count lines/columns from 0, but editors from 1 */
    if(diagnostic->isTextSource)
        out << Debug::nospace << diagnostic->position.line + 1 << Debug::nospace << ":" << Debug::nospace << diagnostic->position.column + 1 << Debug::nospace << ":";
    /* This check is in spvDiagnosticPrint() as well, I assume it's because
       some errors don't have a byte index (wrong size and such?) */
    else if(diagnostic->position.index)
        out << Debug::nospace << diagnostic->position.index << Debug::nospace << ":";

    /* Drop trailing newline, if any. Messages that print disassembled
       instructions have those. */
    out << Containers::StringView{diagnostic->error}.trimmedSuffix();
}

bool readData(const spv_context context, const Utility::ConfigurationGroup& configuration, const Format inputFormat, const Containers::StringView inputFilename, const char* const prefix, spv_binary_t& binaryStorage, spv_binary& binary, Containers::ScopeGuard& binaryDestroy, const Containers::ArrayView<const char> data, Int options) {
    /* If the format is explicitly specified as SPIR-V assembly or if it's
       unspecified and data doesn't look like a binary, parse as an assembly */
    if(inputFormat == Format::SpirvAssembly || (inputFormat == Format::Unspecified && (data.size() < 4 || *reinterpret_cast<const UnsignedInt*>(data.data()) != 0x07230203))) {
        /* There's SPV_TEXT_TO_BINARY_OPTION_NONE which has a non-zero value
           but isn't used anywhere. Looks like a brainfart, HEH. */
        if(configuration.value<bool>("preserveNumericIds"))
            options |= SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS;

        spv_diagnostic diagnostic;
        const spv_result_t error = spvTextToBinaryWithOptions(context, data, data.size(), options, &binary, &diagnostic);
        Containers::ScopeGuard diagnosticDestroy{diagnostic, spvDiagnosticDestroy};
        binaryDestroy = Containers::ScopeGuard{binary, spvBinaryDestroy};
        if(error) {
            Error e;
            e << prefix << "assembly failed:";
            printDiagnostic(e, inputFilename, diagnostic);
            return false;
        }

    /* Otherwise (explicitly specified as SPIR-V binary or unspecified and
       looking like a binary) just make a view on the data, with binaryDestroy
       unused */
    } else {
        if(data.size() % 4 != 0) {
            Error{} << "ShaderTools::SpirvToolsConverter::convertDataToData(): SPIR-V binary size not divisible by four:" << data.size() << "bytes";
            return false;
        }

        binaryStorage.code = const_cast<UnsignedInt*>(reinterpret_cast<const UnsignedInt*>(data.data()));
        binaryStorage.wordCount = data.size()/4;
        binary = &binaryStorage;
    }

    return true;
}

/* Used by doValidateData() and also the optimizer pass in
   doConvertDataToData(), as the optimizer can validate before/after */
void setValidationOptions(spv_validator_options& options, const Utility::ConfigurationGroup& configuration) {
    spvValidatorOptionsSetUniversalLimit(options,
        spv_validator_limit_max_struct_members,
        configuration.value<UnsignedInt>("maxStructMembers"));
    spvValidatorOptionsSetUniversalLimit(options,
        spv_validator_limit_max_struct_depth,
        configuration.value<UnsignedInt>("maxStructDepth"));
    spvValidatorOptionsSetUniversalLimit(options,
        spv_validator_limit_max_local_variables,
        configuration.value<UnsignedInt>("maxLocalVariables"));
    spvValidatorOptionsSetUniversalLimit(options,
        spv_validator_limit_max_global_variables,
        configuration.value<UnsignedInt>("maxGlobalVariables"));
    spvValidatorOptionsSetUniversalLimit(options,
        spv_validator_limit_max_switch_branches,
        configuration.value<UnsignedInt>("maxSwitchBranches"));;
    spvValidatorOptionsSetUniversalLimit(options,
        spv_validator_limit_max_function_args,
        configuration.value<UnsignedInt>("maxFunctionArgs"));
    spvValidatorOptionsSetUniversalLimit(options, spv_validator_limit_max_control_flow_nesting_depth,
        configuration.value<UnsignedInt>("maxControlFlowNestingDepth"));
    spvValidatorOptionsSetUniversalLimit(options, spv_validator_limit_max_access_chain_indexes,
        /* Magnum uses "indices" everywhere, so be consistent here as well */
        configuration.value<UnsignedInt>("maxAccessChainIndices"));;
    spvValidatorOptionsSetUniversalLimit(options, spv_validator_limit_max_id_bound,
        configuration.value<UnsignedInt>("maxIdBound"));
    spvValidatorOptionsSetRelaxLogicalPointer(options,
        configuration.value<bool>("relaxLogicalPointer"));
    spvValidatorOptionsSetRelaxBlockLayout(options,
        configuration.value<bool>("relaxBlockLayout"));
    #if SPIRVTOOLS_VERSION >= 201903
    spvValidatorOptionsSetUniformBufferStandardLayout(options,
        configuration.value<bool>("uniformBufferStandardLayout"));
    #endif
    spvValidatorOptionsSetScalarBlockLayout(options,
        configuration.value<bool>("scalarBlockLayout"));
    spvValidatorOptionsSetSkipBlockLayout(options,
        configuration.value<bool>("skipBlockLayout"));
    spvValidatorOptionsSetRelaxStoreStruct(options,
        /* Both the C++ API and spirv-val use "relax struct store", so use that
           instead of "relax store struct" */
        configuration.value<bool>("relaxStructStore"));
    #if SPIRVTOOLS_VERSION >= 201903
    spvValidatorOptionsSetBeforeHlslLegalization(options,
        configuration.value<bool>("beforeHlslLegalization"));
    #endif
}

}

std::pair<bool, Containers::String> SpirvToolsConverter::doValidateFile(const Stage stage, const Containers::StringView filename) {
    _state->inputFilename = Containers::String::nullTerminatedGlobalView(filename);
    return AbstractConverter::doValidateFile(stage, filename);
}

std::pair<bool, Containers::String> SpirvToolsConverter::doValidateData(Stage, const Containers::ArrayView<const char> data) {
    /* If we're validating a file, save the input filename for use in a
       potential error message. Clear it so next time plain data is validated
       the error messages aren't based on stale information. This is done as
       early as possible so the early exits don't leave it in inconsistent
       state. */
    const Containers::String inputFilename = std::move(_state->inputFilename);
    _state->inputFilename = {};
    /* If this happens, we messed up real bad (it's only set in doConvert*()) */
    CORRADE_INTERNAL_ASSERT(_state->outputFilename.isEmpty());

    if(_state->inputFormat != Format::Unspecified &&
       _state->inputFormat != Format::Spirv &&
       _state->inputFormat != Format::SpirvAssembly) {
        Error{} << "ShaderTools::SpirvToolsConverter::validateData(): input format should be Spirv, SpirvAssembly or Unspecified but got" << _state->inputFormat;
        return {};
    }
    if(!_state->inputVersion.isEmpty()) {
        Error{} << "ShaderTools::SpirvToolsConverter::validateData(): input format version should be empty but got" << _state->inputVersion;
        return {};
    }

    if(_state->outputFormat != Format::Unspecified) {
        Error{} << "ShaderTools::SpirvToolsConverter::validateData(): output format should be Unspecified but got" << _state->outputFormat;
        return {};
    }
    spv_target_env env = SPV_ENV_VULKAN_1_0;
    if(!_state->outputVersion.isEmpty() && !spvParseTargetEnv(_state->outputVersion.data(), &env)) {
        Error{} << "ShaderTools::SpirvToolsConverter::validateData(): unrecognized output format version" << _state->outputVersion;
        return {};
    }

    const spv_context context = spvContextCreate(env);
    Containers::ScopeGuard contextDestroy{context, spvContextDestroy};

    /** @todo make this work on big-endian */

    spv_binary_t binaryStorage;
    spv_binary binary{};
    Containers::ScopeGuard binaryDestroy{NoCreate};
    if(!readData(context, configuration(), _state->inputFormat, inputFilename, "ShaderTools::SpirvToolsConverter::validateData():", binaryStorage, binary, binaryDestroy, data,
        /* Implicitly preserve numeric IDs, so when we're validating a SPIR-V
           assembly, the disassembled instruction in the validation message
           matches the text input as much as possible. */
        /** @todo would be great to have an option of having a reverse mapping
            to the assembly text (lines and such) -- do that manually using
            OpName, OpSource, OpLine and such? */
        SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS
    ))
        return {};

    /* Validator options and limits */
    spv_validator_options options = spvValidatorOptionsCreate();
    Containers::ScopeGuard optionsDestroy{options, spvValidatorOptionsDestroy};
    setValidationOptions(options, configuration());

    spv_diagnostic diagnostic;
    const spv_result_t error = spvValidateWithOptions(context, options, reinterpret_cast<spv_const_binary>(binary), &diagnostic);
    Containers::ScopeGuard diagnosticDestroy{diagnostic, spvDiagnosticDestroy};
    if(error) {
        CORRADE_INTERNAL_ASSERT(diagnostic && !diagnostic->isTextSource);

        /* Drop trailing newline, if any. Messages that print disassembled
           instructions have those. */
        return {false, Utility::formatString(
            diagnostic->position.index ? "{}:{}: {}" : "{0}: {2}",
            inputFilename.isEmpty() ? "<data>" : inputFilename,
            diagnostic->position.index,
            Containers::StringView{diagnostic->error}.trimmedSuffix()
        )};
    }

    CORRADE_INTERNAL_ASSERT(!diagnostic);
    return {true, {}};
}

bool SpirvToolsConverter::doConvertFileToFile(const Stage stage, const Containers::StringView from, const Containers::StringView to) {
    _state->inputFilename = Containers::String::nullTerminatedGlobalView(from);
    _state->outputFilename = Containers::String::nullTerminatedGlobalView(to);
    return AbstractConverter::doConvertFileToFile(stage, from, to);
}

Containers::Array<char> SpirvToolsConverter::doConvertFileToData(const Stage stage, const Containers::StringView filename) {
    _state->inputFilename = Containers::String::nullTerminatedGlobalView(filename);
    return AbstractConverter::doConvertFileToData(stage, filename);
}

Containers::Array<char> SpirvToolsConverter::doConvertDataToData(Stage, const Containers::ArrayView<const char> data) {
    /* If we're converting from a file, save the input filename for use in a
       potential error message. If we're converting to a file, save the output
       filename for detecting if the output should be an assembly. Clear both
       so next time plain data is converted the error messages / output format
       aren't based on stale information. This is done as early as possible so
       the early exits don't leave it in inconsistent state. */
    const Containers::String inputFilename = std::move(_state->inputFilename);
    const Containers::String outputFilename = std::move(_state->outputFilename);
    _state->inputFilename = {};
    _state->outputFilename = {};

    if(_state->inputFormat != Format::Unspecified &&
       _state->inputFormat != Format::Spirv &&
       _state->inputFormat != Format::SpirvAssembly) {
        Error{} << "ShaderTools::SpirvToolsConverter::convertDataToData(): input format should be Spirv, SpirvAssembly or Unspecified but got" << _state->inputFormat;
        return {};
    }
    if(!_state->inputVersion.isEmpty()) {
        Error{} << "ShaderTools::SpirvToolsConverter::convertDataToData(): input format version should be empty but got" << _state->inputVersion;
        return {};
    }

    if(_state->outputFormat != Format::Unspecified &&
       _state->outputFormat != Format::Spirv &&
       _state->outputFormat != Format::SpirvAssembly) {
        Error{} << "ShaderTools::SpirvToolsConverter::convertDataToData(): output format should be Spirv, SpirvAssembly or Unspecified but got" << _state->outputFormat;
        return {};
    }

    /* Target environment. Default to Vulkan 1.0 except if optimizing for
       WebGPU, in which case default to WebGPU 0 */
    spv_target_env env = _state->optimizationLevel == "vulkanToWebGpu"_s ?
        SPV_ENV_WEBGPU_0 : SPV_ENV_VULKAN_1_0;
    if(!_state->outputVersion.isEmpty()) {
        if(!spvParseTargetEnv(_state->outputVersion.data(), &env)) {
            Error{} << "ShaderTools::SpirvToolsConverter::convertDataToData(): unrecognized output format version" << _state->outputVersion;
            return {};
        }

        /* Check if the output is legal if we optimize for Vulkan / WebGPU */
        if(_state->optimizationLevel == "vulkanToWebGpu"_s && env != SPV_ENV_WEBGPU_0) {
            Error{} << "ShaderTools::SpirvToolsConverter::convertDataToData(): can't target" << _state->outputVersion << "when optimizing for WebGPU, expected empty or webgpu0 instead";
            return {};
        }
        if(_state->optimizationLevel == "webGpuToVulkan"_s
            && env != SPV_ENV_VULKAN_1_0
            && env != SPV_ENV_VULKAN_1_1
            #if SPIRVTOOLS_VERSION >= 201903
            && env != SPV_ENV_VULKAN_1_1_SPIRV_1_4
            #endif
            #if SPIRVTOOLS_VERSION >= 202001
            && env != SPV_ENV_VULKAN_1_2
            #endif
        ) {
            Error{} << "ShaderTools::SpirvToolsConverter::convertDataToData(): can't target" << _state->outputVersion << "when optimizing for WebGPU, expected empty or vulkanX.Y instead";
            return {};
        }
    }

    const spv_context context = spvContextCreate(env);
    Containers::ScopeGuard contextDestroy{context, spvContextDestroy};

    /** @todo make this work on big-endian */

    spv_binary_t binaryStorage;
    spv_binary binary{};
    Containers::ScopeGuard binaryDestroy{NoCreate};
    if(!readData(context, configuration(), _state->inputFormat, inputFilename, "ShaderTools::SpirvToolsConverter::convertDataToData():", binaryStorage, binary, binaryDestroy, data, 0))
        return {};

    /* Run the optimizer, if desired. What the hell, is the output a vector
       again?! Is everyone mad or */
    std::vector<UnsignedInt> optimizerOutputStorage;
    if(!_state->optimizationLevel.isEmpty() && _state->optimizationLevel != "0"_s) {
        /* The env should already be correct if using vulkanToWebGpu or
           webGpuToVulkan */
        spvtools::Optimizer optimizer{env};
        if(_state->optimizationLevel == "1"_s)
            optimizer.RegisterPerformancePasses();
        else if(_state->optimizationLevel == "s"_s)
            optimizer.RegisterSizePasses();
        else if(_state->optimizationLevel == "legalizeHlsl"_s)
            optimizer.RegisterLegalizationPasses();
        #if SPIRVTOOLS_VERSION >= 201903
        else if(_state->optimizationLevel == "vulkanToWebGpu"_s)
            optimizer.RegisterVulkanToWebGPUPasses();
        else if(_state->optimizationLevel == "webGpuToVulkan"_s)
            optimizer.RegisterWebGPUToVulkanPasses();
        #endif
        else {
            Error{} << "ShaderTools::SpirvToolsConverter::convertDataToData(): optimization level should be 0, 1, s, legalizeHlsl, vulkanToWebGpu, webGpuToVulkan or empty but got" << _state->optimizationLevel;
            return {};
        }

        /* Print using our own APIs */
        optimizer.SetMessageConsumer([](spv_message_level_t level, const char* file, const spv_position_t& position, const char* message) {
            std::ostream* output{};
            const char* prefix{};
            switch(level) {
                /* LCOV_EXCL_START */
                case SPV_MSG_FATAL:
                    output = Error::output();
                    prefix = "fatal optimization error:";
                    break;
                case SPV_MSG_INTERNAL_ERROR:
                    output = Error::output();
                    prefix = "internal optimization error:";
                    break;
                case SPV_MSG_ERROR:
                    output = Error::output();
                    prefix = "optimization error:";
                    break;
                case SPV_MSG_WARNING:
                    output = Warning::output();
                    prefix = "optimization warning:";
                    break;
                case SPV_MSG_INFO:
                    output = Debug::output();
                    prefix = "optimization info";
                    break;
                case SPV_MSG_DEBUG:
                    output = Debug::output();
                    prefix = "optimization debug info";
                    break;
                /* LCOV_EXCL_STOP */
            }
            /* output can be nullptr in case Debug/Warning/Error is silenced */
            CORRADE_INTERNAL_ASSERT(prefix);

            Debug out{output};
            out << "ShaderTools::SpirvToolsConverter::convertDataToData():" << prefix << Debug::newline;
            spv_diagnostic_t diag{position, const_cast<char*>(message), false};
            printDiagnostic(out, file, &diag);
        });

        /* Validator options and limits. Same as in doValidateData(). */
        spv_validator_options validatorOptions = spvValidatorOptionsCreate();
        Containers::ScopeGuard validatorOptionsDestroy{validatorOptions, spvValidatorOptionsDestroy};
        setValidationOptions(validatorOptions, configuration());

        /* Optimizer options */
        spv_optimizer_options optimizerOptions = spvOptimizerOptionsCreate();
        Containers::ScopeGuard optimizerOptionsDestroy{optimizerOptions, spvOptimizerOptionsDestroy};
        spvOptimizerOptionsSetRunValidator(optimizerOptions,
            configuration().value<bool>("validateBeforeOptimization"));
        spvOptimizerOptionsSetValidatorOptions(optimizerOptions,
            validatorOptions);
        spvOptimizerOptionsSetMaxIdBound(optimizerOptions,
            configuration().value<UnsignedInt>("maxIdBound"));
        #if SPIRVTOOLS_VERSION >= 201904
        spvOptimizerOptionsSetPreserveBindings(optimizerOptions,
            configuration().value<UnsignedInt>("preserveBindings"));
        spvOptimizerOptionsSetPreserveSpecConstants(optimizerOptions,
            configuration().value<UnsignedInt>("preserveSpecializationConstants"));
        #endif
        #if SPIRVTOOLS_VERSION >= 201903
        optimizer.SetValidateAfterAll(configuration().value<bool>("validateAfterEachOptimization"));
        #endif
        optimizer.SetTimeReport(configuration().value<bool>("optimizerTimeReport") ? Debug::output() : nullptr);

        /* If the optimizer fails, exit. The message is printed by the message
           consumer we set above. */
        if(!optimizer.Run(binary->code, binary->wordCount, &optimizerOutputStorage, optimizerOptions))
            return {};

        /* Reference the vector guts in the binary again for the rest of the
           code. Replace the old scope guard with an empty one, which will
           also trigger the original deleter, if it was when disassembing. */
        binaryDestroy = Containers::ScopeGuard{NoCreate};
        binary = &binaryStorage;
        binary->code = optimizerOutputStorage.data();
        binary->wordCount = optimizerOutputStorage.size();
    }

    /* Disassemble, if desired, or if the output filename ends with *.spvasm */
    Containers::Array<char> out;
    if(_state->outputFormat == Format::SpirvAssembly || (_state->outputFormat == Format::Unspecified && outputFilename.hasSuffix(".spvasm"_s))) {
        /* There's SPV_BINARY_TO_TEXT_OPTION_NONE which has a non-zero value
           but isn't used anywhere. Looks like another variant of the same
           brainfart. */
        Int options = 0;
        /* SPV_BINARY_TO_TEXT_OPTION_PRINT not exposed, we always want data */
        /** @todo put Color into flags? so magnum-shaderconverter can use
            --color auto and such */
        if(configuration().value<bool>("color"))
            options |= SPV_BINARY_TO_TEXT_OPTION_COLOR;
        if(configuration().value<bool>("indent"))
            options |= SPV_BINARY_TO_TEXT_OPTION_INDENT;
        if(configuration().value<bool>("byteOffset"))
            options |= SPV_BINARY_TO_TEXT_OPTION_SHOW_BYTE_OFFSET;
        /* no-headers=false would be a hard-to-parse double negative, flip
           that (also it would mean `magnum-shaderconverter -fno-no-headers`,
           which looks extra stupid) */
        if(!configuration().value<bool>("header"))
            options |= SPV_BINARY_TO_TEXT_OPTION_NO_HEADER;
        if(configuration().value<bool>("friendlyNames"))
            options |= SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES;
        /** @todo SPV_BINARY_TO_TEXT_OPTION_COMMENT, since
            https://github.com/KhronosGroup/SPIRV-Tools/pull/3847, not in the
            2020.6 release yet -- also, expose through setDebugInfoLevel()? */

        spv_text text{};
        spv_diagnostic diagnostic;
        const spv_result_t error = spvBinaryToText(context, binary->code, binary->wordCount, options, &text, &diagnostic);
        Containers::ScopeGuard textDestroy{text, spvTextDestroy};
        Containers::ScopeGuard diagnosticDestroy{diagnostic, spvDiagnosticDestroy};
        if(error) {
            Error e;
            e << "ShaderTools::SpirvToolsConverter::convertDataToData(): disassembly failed:";
            printDiagnostic(e, inputFilename, diagnostic);
            return {};
        }

        /* Copy the text to the output. We can't take ownership of that array
           because it *might* have a different deleter (in reality it uses a
           plain delete[], but I don't want to depend on such an implementation
           detail, this is not a perf-critical code path). */
        out = Containers::Array<char>{NoInit, text->length};
        Utility::copy(Containers::arrayView(text->str, text->length), out);

    /* Otherwise simply copy the binary to the output. We can't take ownership
       of the array here either because in addition to the case above the
       binary could also point right at the input `data`. */
    } else {
        Containers::ArrayView<const char> in(reinterpret_cast<const char*>(binary->code), 4*binary->wordCount);
        out = Containers::Array<char>{NoInit, in.size()};
        Utility::copy(in, out);
    }

    return out;
}

}}

CORRADE_PLUGIN_REGISTER(SpirvToolsShaderConverter, Magnum::ShaderTools::SpirvToolsConverter,
    "cz.mosra.magnum.ShaderTools.AbstractConverter/0.1")
