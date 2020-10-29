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

#include "GlslangConverter.h"

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StaticArray.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Utility/Algorithms.h>
#include <Corrade/Utility/ConfigurationGroup.h>
#include <Corrade/Utility/FormatStl.h>

#include <glslang/Public/ShaderLang.h> /* Haha what the fuck this name */
#include <glslang/SPIRV/GlslangToSpv.h>

namespace Magnum { namespace ShaderTools {

struct GlslangConverter::State {
    Format inputFormat, outputFormat;
    Containers::String inputVersion, outputVersion;

    Containers::String inputFilename;

    /** @todo change to a String when formatInto() supports it */
    std::string definitions;
};

void GlslangConverter::initialize() {
    /* Glslang, THANK YOU for telling me that this was meant to be called. When
       I forgot to call this, the whole thing did absolutely nothing, with no
       hint whatsoever at what went wrong. */
    glslang::InitializeProcess();
}

void GlslangConverter::finalize() {
    glslang::FinalizeProcess();
}

GlslangConverter::GlslangConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractConverter{manager, plugin}, _state{Containers::InPlaceInit} {}

ConverterFeatures GlslangConverter::doFeatures() const {
    return ConverterFeature::ConvertData|ConverterFeature::ValidateData|ConverterFeature::Preprocess|
        /* We actually don't, but without this set the doValidateFile() /
           doConvertFileTo*() intercepts don't get called when the input is
           specified through callbacks. And since we delegate to the base
           implementation, the callbacks *do* work. */
        ConverterFeature::InputFileCallback;
}

void GlslangConverter::doSetInputFormat(const Format format, const Containers::StringView version) {
    _state->inputFormat = format;
    _state->inputVersion = Containers::String::nullTerminatedGlobalView(version);
}

void GlslangConverter::doSetOutputFormat(const Format format, const Containers::StringView version) {
    _state->outputFormat = format;
    _state->outputVersion = Containers::String::nullTerminatedGlobalView(version);
}

void GlslangConverter::doSetDefinitions(const Containers::ArrayView<const std::pair<Containers::StringView, Containers::StringView>> definitions) {
    /* Concatenate (un)definitions to a preamble */
    /** @todo rework w/o std::string once we have formatInto() w/ a String */
    _state->definitions.clear();
    for(const std::pair<Containers::StringView, Containers::StringView>& definition: definitions) {
        if(!definition.second.data())
            Utility::formatInto(_state->definitions, _state->definitions.size(), "#undef {}\n", definition.first);
        else if(definition.second.isEmpty())
            Utility::formatInto(_state->definitions, _state->definitions.size(), "#define {}\n", definition.first);
        else
            Utility::formatInto(_state->definitions, _state->definitions.size(), "#define {} {}\n", definition.first, definition.second);
    }
}

namespace {

using namespace Containers::Literals;

Stage stageFromFilename(Containers::StringView filename) {
    if(filename.hasSuffix(".glsl"_s))
        return stageFromFilename(filename.stripSuffix(".glsl"_s));

    /* LCOV_EXCL_START */
    if(filename.hasSuffix(".vert"_s)) return Stage::Vertex;
    if(filename.hasSuffix(".frag"_s)) return Stage::Fragment;
    if(filename.hasSuffix(".geom"_s)) return Stage::Geometry;
    if(filename.hasSuffix(".tesc"_s)) return Stage::TessellationControl;
    if(filename.hasSuffix(".tese"_s)) return Stage::TessellationEvaluation;
    if(filename.hasSuffix(".comp"_s)) return Stage::Compute;
    if(filename.hasSuffix(".rgen"_s)) return Stage::RayGeneration;
    if(filename.hasSuffix(".rahit"_s)) return Stage::RayAnyHit;
    if(filename.hasSuffix(".rchit"_s)) return Stage::RayClosestHit;
    if(filename.hasSuffix(".rmiss"_s)) return Stage::RayMiss;
    if(filename.hasSuffix(".rint"_s)) return Stage::RayIntersection;
    if(filename.hasSuffix(".rcall"_s)) return Stage::RayCallable;
    if(filename.hasSuffix(".task"_s)) return Stage::MeshTask;
    if(filename.hasSuffix(".mesh"_s)) return Stage::Mesh;
    /* LCOV_EXCL_STOP */

   return Stage::Vertex;
}

EShLanguage translateStage(const Stage stage) {
    switch(stage) {
        /* LCOV_EXCL_START */
        case Stage::Vertex: return EShLangVertex;
        case Stage::Fragment: return EShLangFragment;
        case Stage::Geometry: return EShLangGeometry;
        case Stage::TessellationControl: return EShLangTessControl;
        case Stage::TessellationEvaluation: return EShLangTessEvaluation;
        case Stage::Compute: return EShLangCompute;
        case Stage::RayGeneration: return EShLangRayGen;
        case Stage::RayAnyHit: return EShLangAnyHit;
        case Stage::RayClosestHit: return EShLangClosestHit;
        case Stage::RayMiss: return EShLangMiss;
        case Stage::RayIntersection: return EShLangIntersect;
        case Stage::RayCallable: return EShLangCallable;
        /** @todo drop the NV suffix when mesh shaders are a KHR extension */
        case Stage::MeshTask: return EShLangTaskNV;
        case Stage::Mesh: return EShLangMeshNV;
        /* LCOV_EXCL_STOP */

        case Stage::Unspecified: return EShLangVertex;
    }

    /* Testing this would mean having a separate "graceful assert" build of the
       plugin, which is too much effort. Exclude this line from the coverage,
       then. */
    CORRADE_ASSERT_UNREACHABLE("ShaderTools::GlslangConverter: invalid stage" << stage, {}); /* LCOV_EXCL_LINE */
}

/* This tries to match the CRAZY logic in https://github.com/KhronosGroup/glslang/blob/f4f1d8a352ca1908943aea2ad8c54b39b4879080/glslang/MachineIndependent/ShaderLang.cpp#L511-L567 */
std::pair<Int, EProfile> parseInputVersion(const char* const prefix, const Containers::StringView version) {
    /* Default to desktop GL 2.1 */
    if(version.isEmpty() || version == "110"_s) return {110, ENoProfile};

    if(version == "120"_s) return {120, ENoProfile};
    if(version == "130"_s) return {130, ENoProfile};
    if(version == "140"_s) return {140, ENoProfile};
    if(version == "150"_s) return {150, ECompatibilityProfile};
    if(version == "150 core"_s) return {150, ECoreProfile};
    if(version == "330"_s) return {330, ECompatibilityProfile};
    if(version == "330 core"_s) return {330, ECoreProfile};
    if(version == "400"_s) return {400, ECompatibilityProfile};
    if(version == "400 core"_s) return {400, ECoreProfile};
    if(version == "410"_s) return {410, ECompatibilityProfile};
    if(version == "410 core"_s) return {410, ECoreProfile};
    if(version == "420"_s) return {420, ECompatibilityProfile};
    if(version == "420 core"_s) return {420, ECoreProfile};
    if(version == "430"_s) return {430, ECompatibilityProfile};
    if(version == "430 core"_s) return {430, ECoreProfile};
    if(version == "440"_s) return {440, ECompatibilityProfile};
    if(version == "440 core"_s) return {440, ECoreProfile};
    if(version == "450"_s) return {450, ECompatibilityProfile};
    if(version == "450 core"_s) return {450, ECoreProfile};
    if(version == "460"_s) return {460, ECompatibilityProfile};
    if(version == "460 core"_s) return {460, ECoreProfile};

    if(version == "100 es"_s) return {100, EEsProfile};
    if(version == "300 es"_s) return {300, EEsProfile};
    if(version == "310 es"_s) return {310, EEsProfile};
    if(version == "320 es"_s) return {320, EEsProfile};

    /** @todo it uses 500 for HLSL, wtf! */

    Error{} << prefix << "input format version should be one of supported GLSL #version strings but got" << version;
    return {};
}

std::pair<glslang::EShTargetClientVersion, glslang::EShTargetLanguageVersion> parseOutputVersion(const char* const prefix, const Containers::StringView version) {
    /* Default (if not set) is Vulkan 1.0 with SPIR-V 1.0 */
    if(version.isEmpty())
        return {glslang::EShTargetVulkan_1_0, glslang::EShTargetSpv_1_0};

    /* `<target> spv<major>.<minor>`, where the second part is optional. If
       present, split[1] will contain a space, if not, split[1] is empty. */
    Containers::Array3<Containers::StringView> split = version.partition(' ');

    glslang::EShTargetClientVersion client;
    glslang::EShTargetLanguageVersion language;
    if(split[0] == "opengl4.5"_s) {
        client = glslang::EShTargetOpenGL_450;
        language = glslang::EShTargetSpv_1_0;
    } else if(split[0] == "vulkan1.0"_s) {
        client = glslang::EShTargetVulkan_1_0;
        language = glslang::EShTargetSpv_1_0;
    } else if(split[0] == "vulkan1.1"_s) {
        client = glslang::EShTargetVulkan_1_1;
        language = glslang::EShTargetSpv_1_3;
    } else if(split[0] == "vulkan1.2"_s) {
        client = glslang::EShTargetVulkan_1_2;
        language = glslang::EShTargetSpv_1_5;
    } else {
        Error{} << prefix << "output format version target should be opengl4.5 or vulkanX.Y but got" << split[0];
        return {};
    }

    /* Override SPIR-V version, if specified as a second parameter */
    if(!split[1].isEmpty()) {
        if(split[2] == "spv1.0"_s) language = glslang::EShTargetSpv_1_0;
        else if(split[2] == "spv1.1"_s) language = glslang::EShTargetSpv_1_1;
        else if(split[2] == "spv1.2"_s) language = glslang::EShTargetSpv_1_2;
        else if(split[2] == "spv1.3"_s) language = glslang::EShTargetSpv_1_3;
        else if(split[2] == "spv1.4"_s) language = glslang::EShTargetSpv_1_4;
        else if(split[2] == "spv1.5"_s) language = glslang::EShTargetSpv_1_5;
        else {
            Error{} << prefix << "output format version language should be spvX.Y but got" << split[2];
            return {};
        }
    }

    return {client, language};
}

std::pair<bool, bool> compileAndLinkShader(glslang::TShader& shader, glslang::TProgram& program, const Utility::ConfigurationGroup& configuration, const ConverterFlags flags, const std::pair<int, EProfile> inputVersion, const std::pair<glslang::EshTargetClientVersion, glslang::EShTargetLanguageVersion> outputVersion, const bool versionExplicitlySpecified, const Containers::StringView definitions, const Containers::StringView filename, const Containers::ArrayView<const char> data) {
    /* Add preprocessor definitions */
    shader.setPreamble(definitions.data());

    /* Add the actual shader source. We're not making use of the
       multiple-source inputs here, it would only further complicate the plugin
       interface. Google's shaderc does the same, and glslangValidator (WHAT A
       NAME!!) seems to do that also, but its API is too confusing so I can't
       tell for sure. If we're validating/compiling a file, the name gets used
       in potential error messages. */
    const char* string = data.data();
    int length = data.size();
    const char* filenames = filename.data();
    shader.setStringsWithLengthsAndNames(&string, &length, filename.isEmpty() ? nullptr : &filenames, 1);

    /** @todo ability to override entrypoint name (for linking multiple same
        stages together), for some reason not working in glslang, only for
        hlsl */
    /** @todo includer */

    /* Set up builtin values and resource limits. There's no default
       constructor for that thing so we'd have to populate it either way, even
       if not exposing any of these. Sigh.

        https://github.com/KhronosGroup/glslang/blob/d1929f359a1035cb169ec54630c24ae6ce0bcc21/StandAlone/ResourceLimits.cpp

       Update when neccessary -- the last member is commented out because it's
       not in 8.13.3743 yet */
    TBuiltInResource resources;
    const Utility::ConfigurationGroup* builtins = configuration.group("builtins");
    CORRADE_INTERNAL_ASSERT(builtins);
    #define _c(name) resources.name = builtins->value<Int>(#name);
    _c(maxLights)
    _c(maxClipPlanes)
    _c(maxTextureUnits)
    _c(maxTextureCoords)
    _c(maxVertexAttribs)
    _c(maxVertexUniformComponents)
    _c(maxVaryingFloats)
    _c(maxVertexTextureImageUnits)
    _c(maxCombinedTextureImageUnits)
    _c(maxTextureImageUnits)
    _c(maxFragmentUniformComponents)
    _c(maxDrawBuffers)
    _c(maxVertexUniformVectors)
    _c(maxVaryingVectors)
    _c(maxFragmentUniformVectors)
    _c(maxVertexOutputVectors)
    _c(maxFragmentInputVectors)
    _c(minProgramTexelOffset)
    _c(maxProgramTexelOffset)
    _c(maxClipDistances)
    _c(maxComputeWorkGroupCountX)
    _c(maxComputeWorkGroupCountY)
    _c(maxComputeWorkGroupCountZ)
    _c(maxComputeWorkGroupSizeX)
    _c(maxComputeWorkGroupSizeY)
    _c(maxComputeWorkGroupSizeZ)
    _c(maxComputeUniformComponents)
    _c(maxComputeTextureImageUnits)
    _c(maxComputeImageUniforms)
    _c(maxComputeAtomicCounters)
    _c(maxComputeAtomicCounterBuffers)
    _c(maxVaryingComponents)
    _c(maxVertexOutputComponents)
    _c(maxGeometryInputComponents)
    _c(maxGeometryOutputComponents)
    _c(maxFragmentInputComponents)
    _c(maxImageUnits)
    _c(maxCombinedImageUnitsAndFragmentOutputs)
    _c(maxCombinedShaderOutputResources)
    _c(maxImageSamples)
    _c(maxVertexImageUniforms)
    _c(maxTessControlImageUniforms)
    _c(maxTessEvaluationImageUniforms)
    _c(maxGeometryImageUniforms)
    _c(maxFragmentImageUniforms)
    _c(maxCombinedImageUniforms)
    _c(maxGeometryTextureImageUnits)
    _c(maxGeometryOutputVertices)
    _c(maxGeometryTotalOutputComponents)
    _c(maxGeometryUniformComponents)
    _c(maxGeometryVaryingComponents)
    _c(maxTessControlInputComponents)
    _c(maxTessControlOutputComponents)
    _c(maxTessControlTextureImageUnits)
    _c(maxTessControlUniformComponents)
    _c(maxTessControlTotalOutputComponents)
    _c(maxTessEvaluationInputComponents)
    _c(maxTessEvaluationOutputComponents)
    _c(maxTessEvaluationTextureImageUnits)
    _c(maxTessEvaluationUniformComponents)
    _c(maxTessPatchComponents)
    _c(maxPatchVertices)
    _c(maxTessGenLevel)
    _c(maxViewports)
    _c(maxVertexAtomicCounters)
    _c(maxTessControlAtomicCounters)
    _c(maxTessEvaluationAtomicCounters)
    _c(maxGeometryAtomicCounters)
    _c(maxFragmentAtomicCounters)
    _c(maxCombinedAtomicCounters)
    _c(maxAtomicCounterBindings)
    _c(maxVertexAtomicCounterBuffers)
    _c(maxTessControlAtomicCounterBuffers)
    _c(maxTessEvaluationAtomicCounterBuffers)
    _c(maxGeometryAtomicCounterBuffers)
    _c(maxFragmentAtomicCounterBuffers)
    _c(maxCombinedAtomicCounterBuffers)
    _c(maxAtomicCounterBufferSize)
    _c(maxTransformFeedbackBuffers)
    _c(maxTransformFeedbackInterleavedComponents)
    _c(maxCullDistances)
    _c(maxCombinedClipAndCullDistances)
    _c(maxSamples)
    _c(maxMeshOutputVerticesNV)
    _c(maxMeshOutputPrimitivesNV)
    _c(maxMeshWorkGroupSizeX_NV)
    _c(maxMeshWorkGroupSizeY_NV)
    _c(maxMeshWorkGroupSizeZ_NV)
    _c(maxTaskWorkGroupSizeX_NV)
    _c(maxTaskWorkGroupSizeY_NV)
    _c(maxTaskWorkGroupSizeZ_NV)
    _c(maxMeshViewCountNV)
    //_c(maxDualSourceDrawBuffersEXT)
    #undef _c

    const Utility::ConfigurationGroup* limits = configuration.group("limits");
    CORRADE_INTERNAL_ASSERT(limits);
    #define _c(name) resources.limits.name = limits->value<bool>(#name);
    _c(nonInductiveForLoops)
    _c(whileLoops)
    _c(doWhileLoops)
    _c(generalUniformIndexing)
    _c(generalAttributeMatrixVectorIndexing)
    _c(generalVaryingIndexing)
    _c(generalSamplerIndexing)
    _c(generalVariableIndexing)
    _c(generalConstantMatrixVectorIndexing)
    #undef _c

    /* Decide on the client based on output version */
    glslang::EShClient client{};
    switch(outputVersion.first) {
        case glslang::EShTargetVulkan_1_0:
        case glslang::EShTargetVulkan_1_1:
        case glslang::EShTargetVulkan_1_2:
            client = glslang::EShClientVulkan;
            break;
        case glslang::EShTargetOpenGL_450:
            client = glslang::EShClientOpenGL;
            break;

        /* FFS */
        /* LCOV_EXCL_START */
        case glslang::EShTargetClientVersionCount:
            CORRADE_INTERNAL_ASSERT_UNREACHABLE();
        /* LCOV_EXCL_STOP */
    }
    CORRADE_INTERNAL_ASSERT(client);

    /* Configure input/output formats, targets and versions.

       For validation, according to the README, we're not supposed to set any
       client or target (and "use 0 for version", which funilly enough doesn't
       even compile), but ACTUALLY we do as it affects how the source is
       validated, what limits are checked etc. So let's just be safe and supply
       the same thing for both. */
    /** @todo support HLSL here */
    shader.setEnvInput(glslang::EShSourceGlsl,
        /* Why the heck do I specify the stage again after it was set in the
           constructor?! What's the purpose of that?! */
        shader.getStage(),
        /* The client here has to be the same as client in setEnvClient(). The
           glslangValidator (WHAT A NAME!!) executable does exactly that and I
           just can't find any use case for them to be different:
           https://github.com/KhronosGroup/glslang/blob/2de6d657dde37a421ff8afb1bd820d522df5821d/StandAlone/StandAlone.cpp#L1081-L1084  */
        client,
        /* This is the version used for #define VULKAN or #define GL_SPIRV.
           According to the ARB_gl_spirv and GL_KHR_vulkan_glsl extensions it
           should be 100, and that's what glslangValidator forces as well:
           https://github.com/KhronosGroup/glslang/blob/2de6d657dde37a421ff8afb1bd820d522df5821d/StandAlone/StandAlone.cpp#L699-L700
           If I pass 0, it'll apparently not define those, so let's do a 100 */
        100);
    shader.setEnvClient(client, outputVersion.first);
    /* setEnvTarget() needs to be set differently for validation and
       compilation because THE LIB IS SHIT, see doValidateData() and
       doConvertDataToData() for the gory details */

    /* Messages. Many of these are not exposed because of uselessness:

        -   EShMsgSpvRules / EShMsgVulkanRules is used only to override target
            SPIR-V / Vulkan version (why the fuck is it there if we have also
            an option to control the target?)
        -   EShMsgAST prints the glslang AST. Not sure what that is for except
            for debugging GLSL parser bugs. And we have SPIR-V and SPIR-V Cross
            for doing real stuff.
        -   EShMsgKeepUncalled is "for testing" which indicates that I should
            not bother
        -   EShMsgBuiltinSymbolTable spoils the output by emitting a badly
            formatted list of every known builtin. Would be useful if there was
            some better API to access that info. It's just noise when done like
            this.
    */
    Int messages = 0;
    if(configuration.value<bool>("cascadingErrors"))
        messages |= EShMsgCascadingErrors;
    if(configuration.value<bool>("permissive"))
        messages |= EShMsgRelaxedErrors;
    if(flags & ConverterFlag::Quiet)
        messages |= EShMsgSuppressWarnings;
    /** @todo Anything to enable for Verbose? getInfoDebugLog()? what's that
        useful for except for dumping some extremely internal noise? */
    /** @todo EShMsgReadHlsl if input format / extension is HLSL(?) why
        EShSourceHlsl isn't enough?, also EShMsgHlslOffsets,
        EShMsgHlslEnable16BitTypes, EShMsgHlslLegalization,
        EShMsgHlslDX9Compatible once HLSL support is in */

    /* Compile. Why the hell is it called "parse" is beyond me. Don't even
       bother going further if compilation didn't succeed. */
    const bool compilingSucceeded = shader.parse(&resources,
        inputVersion.first, inputVersion.second,
        /* Force version and profile. If the input version is specified by the
           user, assume that yes the user wants to override that. If it's not,
           use it only if the source itself doesn't have it. */
        versionExplicitlySpecified,
        /* Forward compatible. Like, I fucking don't get why these two booleans
           couldn't be specified together with the other flags in `messages`
           (fuck me, what a name). Because there's not even a modicum of usable
           docs, how the hell am I supposed to search for this variable being
           used anywhere to see what it does? */
        configuration.value<bool>("forwardCompatible"),
        /* FFS stupid thing why don't you accept int for flags?! */
        EShMessages(messages));

    /* Glslang has no way to treat warnings as errors, so instead we look at
       the info log and return failure if it's nonempty */
    if(!compilingSucceeded || ((flags & ConverterFlag::WarningAsError) && shader.getInfoLog()[0]))
        return {};

    /* Assume that the log is empty if we suppressed warnings and compilation
       succeeded. If not, we have to revisit this and add a check to convert()
       where it outputs the messages to Warning. */
    CORRADE_INTERNAL_ASSERT(!(flags & ConverterFlag::Quiet) || !shader.getInfoLog()[0]);

    /* "Link". This does not not any inter-stage linking at all, only linking
       of multiple separately compiled shaders togeter into a single stage
       (which, until now, I didn't even know was possible in GL). We don't use
       that at all, so this step only does a bunch of checks and patches after
       the single-file single-shader compilation. */
    program.addShader(&shader);
    const bool linkingSucceeded = program.link(
        /* FFS stupid thing why don't you accept int for flags?! */
        EShMessages(messages));

    /* Similarly to above, assume that the log is empty if we suppressed
       warnings and liking succeeded. If not, we have to revisit this and add a
       check to convert() where it outputs the messages to Warning. */
    CORRADE_INTERNAL_ASSERT(!linkingSucceeded || !(flags & ConverterFlag::Quiet) || !program.getInfoLog()[0]);

    /* Glslang has no way to treat warnings as errors, so instead we look at
       the info log and return failure if it's nonempty */
    if(!linkingSucceeded || ((flags & ConverterFlag::WarningAsError) && program.getInfoLog()[0]))
        return {true, false};

    return {true, true};
}

}

std::pair<bool, Containers::String> GlslangConverter::doValidateFile(const Stage stage, const Containers::StringView filename) {
    /* Save input filename for nicer error messages */
    _state->inputFilename = Containers::String::nullTerminatedGlobalView(filename);

    /* If stage is not specified, detect it from filename and then delegate
       into the default implementation */
    return AbstractConverter::doValidateFile(
        stage == Stage::Unspecified ? stageFromFilename(filename) : stage,
        filename);
}

std::pair<bool, Containers::String> GlslangConverter::doValidateData(const Stage stage, Containers::ArrayView<const char> data) {
    /* If we're validating a file, save the input filename for use in a
       potential error message. Clear it so next time plain data is validated
       the error messages aren't based on stale information. This is done as
       early as possible so the early exits don't leave it in inconsistent
       state. */
    const Containers::String inputFilename = std::move(_state->inputFilename);
    _state->inputFilename = {};

    /* Check input/output format validity */
    /** @todo allow HLSL once we implement support for it */
    if(_state->inputFormat != Format::Unspecified &&
       _state->inputFormat != Format::Glsl) {
        Error{} << "ShaderTools::GlslangConverter::validateData(): input format should be Glsl or Unspecified but got" << _state->inputFormat;
        return {};
    }
    if(_state->outputFormat != Format::Unspecified) {
        Error{} << "ShaderTools::GlslangConverter::validateData(): output format should be Unspecified but got" << _state->outputFormat;
        return {};
    }

    /* Decide on stage and input version, fail early if those don't work
       (translateStage() asserts, parseInputVersion() prints an error message
       on its own) */
    const EShLanguage translatedStage = translateStage(stage);
    const std::pair<int, EProfile> inputVersion = parseInputVersion("ShaderTools::GlslangConverter::validateData():", _state->inputVersion);
    const std::pair<glslang::EShTargetClientVersion, glslang::EShTargetLanguageVersion> outputVersion = parseOutputVersion("ShaderTools::GlslangConverter::validateData():", _state->outputVersion);
    if(!inputVersion.first || !outputVersion.first) return {};

    /* Amazing, why a part of the API has the glslang:: namespace and some
       doesn't? Why can't you just be consistent, FFS? Do you ever C++?! */
    glslang::TShader shader{translatedStage};

    /* This is stupid, but here we go -- if we set EShTargetNone for validation
       (as the README suggests), validation of Vulkan shaders will fail with

        syntax error, unexpected TEXTURE2D, expecting COMMA or SEMICOLON

       and then a thousand-line spew of all GLSL builtins ever known to man.
       Probably because texture2D becomes a reserved word in newer versions.
       HOWEVER, when we set EShTargetSpv, validation of shaders in GLSL < 140
       will fail as well, this time with

        'double' : not supported with this profile: none

       and AGAIN a thousand-line spew of all GLSL builtins ever known to man
       because the library is just stupid. It's unfixable unless I'd attempt to
       parse the source for a #version directive and decide based on that, but
       that's NOT MY JOB so I'll do a best-effort handling and use Spv when
       targeting Vulkan and None if targeting GL. I wonder on what case this
       blows up next. */
    shader.setEnvTarget(outputVersion.first == glslang::EShTargetOpenGL_450 ? glslang::EShTargetNone : glslang::EShTargetSpv, outputVersion.second);

    /* Add preprocessor definitions, input source, configure limits,
       input/output formats, targets and versions, compile and "link". This
       function is shared between doValidateData() and doConvertDataToData()
       and does the same in both. Here we use just the output log. */
    glslang::TProgram program;
    const std::pair<bool, bool> success = compileAndLinkShader(shader, program, configuration(), flags(), inputVersion, outputVersion, !_state->inputVersion.isEmpty(), _state->definitions, inputFilename, data);

    /* Trim excessive newlines and spaces from the output. What the fuck, did
       nobody ever verify what mess it spits out?! */
    /** @todo clean up once StringView::trimmed() exist */
    /** @todo clean up also trailing newlines inside, ffs */
    Containers::StringView shaderLog = shader.getInfoLog();
    while(!shaderLog.isEmpty() && (shaderLog.back() == '\n' || shaderLog.back() == ' '))
        shaderLog = shaderLog.except(1);
    if(!success.first) return {false, shaderLog};

    /* Trim excessive newlines and spaces here as well */
    /** @todo clean up once StringView::trimmed() exist */
    Containers::StringView programLog = program.getInfoLog();
    while(!programLog.isEmpty() && (programLog.back() == '\n' || programLog.back() == ' '))
        programLog = programLog.except(1);

    /** @todo use Containers::String once it can concatenate */
    std::string out = shaderLog;
    if(!shaderLog.isEmpty() && !programLog.isEmpty())
        out += '\n';
    out += programLog;
    return {success.second, out};
}

Containers::Array<char> GlslangConverter::doConvertFileToData(const Stage stage, const Containers::StringView from) {
    /* Save input filename for nicer error messages */
    _state->inputFilename = Containers::String::nullTerminatedGlobalView(from);

    /* If stage is not specified, detect it from filename and then delegate
       into the default implementation */
    return AbstractConverter::doConvertFileToData(
        stage == Stage::Unspecified ? stageFromFilename(from) : stage,
        from);
}

bool GlslangConverter::doConvertFileToFile(const Stage stage, const Containers::StringView from, const Containers::StringView to) {
    /* Save input filename for nicer error messages */
    _state->inputFilename = Containers::String::nullTerminatedGlobalView(from);

    /* If stage is not specified, detect it from filename and then delegate
       into the default implementation */
    return AbstractConverter::doConvertFileToFile(
        stage == Stage::Unspecified ? stageFromFilename(from) : stage,
        from, to);
}

Containers::Array<char> GlslangConverter::doConvertDataToData(const Stage stage, const Containers::ArrayView<const char> data) {
    /* If we're validating a file, save the input filename for use in a
       potential error message. Clear it so next time plain data is validated
       the error messages aren't based on stale information. This is done as
       early as possible so the early exits don't leave it in inconsistent
       state. */
    const Containers::String inputFilename = std::move(_state->inputFilename);
    _state->inputFilename = {};

    /** @todo implement this, should also have EShMsgOnlyPreprocessor set (or
        it's done by default?) */
    if(flags() & ConverterFlag::PreprocessOnly) {
        Error{} << "ShaderTools::GlslangConverter::convertDataToData(): PreprocessOnly is not implemented yet, sorry";
        return {};
    }

    /* Check input/output format validity */
    /** @todo allow HLSL once we implement support for it */
    if(_state->inputFormat != Format::Unspecified &&
       _state->inputFormat != Format::Glsl) {
        Error{} << "ShaderTools::GlslangConverter::convertDataToData(): input format should be Glsl or Unspecified but got" << _state->inputFormat;
        return {};
    }
    if(_state->outputFormat != Format::Unspecified &&
       _state->outputFormat != Format::Spirv) {
        Error{} << "ShaderTools::GlslangConverter::validateData(): output format should be Spirv or Unspecified but got" << _state->outputFormat;
        return {};
    }

    /* Decide on stage and input version, fail early if those don't work
       (translateStage() asserts, parseInputVersion() and parseOutputVersion()
       prints an error message on its own) */
    const EShLanguage translatedStage = translateStage(stage);
    const std::pair<int, EProfile> inputVersion = parseInputVersion("ShaderTools::GlslangConverter::convertDataToData():", _state->inputVersion);
    const std::pair<glslang::EshTargetClientVersion, glslang::EShTargetLanguageVersion> outputVersion = parseOutputVersion("ShaderTools::GlslangConverter::convertDataToData():", _state->outputVersion);
    if(!inputVersion.first || !outputVersion.first) return {};

    /* Amazing, why some enums have the glslang:: namespace and some don't /
       can't? Why can't you just be consistent, FFS? */
    glslang::TShader shader{translatedStage};

    /* This is done differently for validation and compilation, so it's not
       inside compileAndLinkShader(). Unlike in doValidateData(), here we just
       set a SPIR-V target because that's what we want. */
    shader.setEnvTarget(glslang::EShTargetSpv, outputVersion.second);

    /* Add preprocessor definitions, input source, configure limits,
       input/output formats, targets and versions, compile and "link". This
       function is shared between doValidateData() and doConvertDataToData()
       and does the same in both. */
    glslang::TProgram program;
    std::pair<bool, bool> success = compileAndLinkShader(shader, program, configuration(), flags(), inputVersion, outputVersion, !_state->inputVersion.isEmpty(), _state->definitions, inputFilename, data);

    /* Trim excessive newlines and spaces from the output. What the fuck, did
       nobody ever verify what mess it spits out?! */
    /** @todo clean up once StringView::trimmed() exist */
    /** @todo clean up also trailing newlines inside, ffs */
    Containers::StringView shaderLog = shader.getInfoLog();
    while(!shaderLog.isEmpty() && (shaderLog.back() == '\n' || shaderLog.back() == ' '))
        shaderLog = shaderLog.except(1);
    if(!success.first) {
        Error{} << "ShaderTools::GlslangConverter::convertDataToData(): compilation failed:" << Debug::newline << shaderLog;
        return {};
    }

    /* Assertions in compileAndLinkShader() should have checked that we get
       warnings only if Quiet is not enabled */
    if(!shaderLog.isEmpty())
        Warning{} << "ShaderTools::GlslangConverter::convertDataToData(): compilation succeeded with the following message:" << Debug::newline << shaderLog;

    /* Trim excessive newlines and spaces here as well */
    /** @todo clean up once StringView::trimmed() exist */
    Containers::StringView programLog = program.getInfoLog();
    while(!programLog.isEmpty() && (programLog.back() == '\n' || programLog.back() == ' '))
        programLog = programLog.except(1);

    if(!success.second) {
        Error{} << "ShaderTools::GlslangConverter::convertDataToData(): linking failed:" << Debug::newline << programLog;
        return {};
    }

    /* Assertions in compileAndLinkShader() should have checked that we get
       warnings only if Quiet is not enabled */
    if(!programLog.isEmpty())
        Warning{} << "ShaderTools::GlslangConverter::convertDataToData(): linking succeeded with the following message:" << Debug::newline << programLog;

    /* Translate the glslang IR to SPIR-V. Yes, this goes separately for each
       stage, so the actual "linking" is no linking at all (and no, it doesn't
       do any cross-stage validation or checks either, at least in the current
       version). */
    glslang::TIntermediate* ir = program.getIntermediate(translatedStage);
    CORRADE_INTERNAL_ASSERT(ir);

    glslang::SpvOptions spvOptions;
    /* We'll do these ourselves (and better) on the resulting SPIR-V instead */
    spvOptions.disableOptimizer = true;
    spvOptions.disassemble = false;
    /* We have a dedicated plugin for SPIR-V validation with far more options */
    spvOptions.validate = false;
    /** @todo expose, probably together with EShMsgDebugInfo enabled? */
    spvOptions.generateDebugInfo = false;
    /* Not in 8.13.3743 yet */
    //spvOptions.stripDebugInfo = true;

    /* WTF, a vector?! U MAD? */
    std::vector<UnsignedInt> spirv;
    spv::SpvBuildLogger logger;
    glslang::GlslangToSpv(*ir, spirv, &logger, &spvOptions);

    /* Copy the vector into something sane */
    Containers::ArrayView<const char> spirvBytes = Containers::arrayCast<const char>(Containers::arrayView(spirv));
    Containers::Array<char> out{Containers::NoInit, spirvBytes.size()};
    Utility::copy(spirvBytes, out);
    return out;
}

}}

CORRADE_PLUGIN_REGISTER(GlslangShaderConverter, Magnum::ShaderTools::GlslangConverter,
    "cz.mosra.magnum.ShaderTools.AbstractConverter/0.1")
