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

#include "GlslOptimizerConverter.h"

#include <cstring>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/ScopeGuard.h>
#include <Corrade/Utility/Algorithms.h>

#include "compiler/glsl/glsl_optimizer.h"

namespace Magnum { namespace ShaderTools {

struct GlslOptimizerConverter::State {
    Format inputFormat, outputFormat;
    Containers::String inputVersion, outputVersion;
};

GlslOptimizerConverter::GlslOptimizerConverter(PluginManager::AbstractManager& manager, const std::string& plugin): AbstractConverter{manager, plugin}, _state{Containers::InPlaceInit} {}

ConverterFeatures GlslOptimizerConverter::doFeatures() const {
    return ConverterFeature::ConvertData;
}

void GlslOptimizerConverter::doSetInputFormat(const Format format, const Containers::StringView version) {
    _state->inputFormat = format;
    _state->inputVersion = Containers::String::nullTerminatedGlobalView(version);
}

void GlslOptimizerConverter::doSetOutputFormat(const Format format, const Containers::StringView version) {
    _state->outputFormat = format;
    _state->outputVersion = Containers::String::nullTerminatedGlobalView(version);
}

Containers::Array<char> GlslOptimizerConverter::doConvertDataToData(const Stage stage, const Containers::ArrayView<const char> data) {
    /* Check input/output format validity */
    /** @todo allow HLSL once we implement support for it */
    if(_state->inputFormat != Format::Unspecified &&
       _state->inputFormat != Format::Glsl) {
        Error{} << "ShaderTools::GlslOptimizerConverter::convertDataToData(): input format should be Glsl or Unspecified but got" << _state->inputFormat;
        return {};
    }
    if(_state->outputFormat != Format::Unspecified &&
       _state->outputFormat != Format::Glsl) {
        Error{} << "ShaderTools::GlslOptimizerConverter::convertDataToData(): output format should be Glsl or Unspecified but got" << _state->outputFormat;
        return {};
    }
    if(!_state->inputVersion.isEmpty()) {
        Error{} << "ShaderTools::GlslOptimizerConverter::convertDataToData(): input format version should be empty but got" << _state->inputVersion;
        return {};
    }
    if(!_state->outputVersion.isEmpty()) {
        Error{} << "ShaderTools::GlslOptimizerConverter::convertDataToData(): output format version should be empty but got" << _state->outputVersion;
        return {};
    }

    glslopt_shader_type type;
    if(stage == Stage::Vertex || stage == Stage::Unspecified)
        type = kGlslOptShaderVertex;
    else if(stage == Stage::Fragment)
        type = kGlslOptShaderFragment;
    else {
        Error{} << "ShaderTools::GlslOptimizerConverter::convertDataToData(): stage can be either Vertex, Fragment or Unspecified but got" << stage;
        return {};
    }

    // TODO: target configuration
    glslopt_ctx* const ctx = glslopt_initialize(kGlslTargetOpenGL);
    Containers::ScopeGuard ctxCleanup{ctx, glslopt_cleanup};

    /* Create a (null-terminated) string out of the input data. Unfortunately
       with ArrayView we have no way to know if the input was already
       null-terminated so this is always a copy. */
    const Containers::String source = data;

    // TODO: expose glslopt_options
    glslopt_shader* const shader = glslopt_optimize(ctx, type, source.data(), 0);
    Containers::ScopeGuard{shader, glslopt_shader_delete};

    if(!glslopt_get_status(shader)) {
        Error{} << "ShaderTools::GlslOptimizerConverter::convertDataToData(): optimization failed:" << glslopt_get_log(shader);
        return {};
    }

    const char* const optimized = glslopt_get_output(shader);
    Containers::Array<char> out{Containers::NoInit, std::strlen(optimized)};
    Utility::copy(Containers::ArrayView<const char>{optimized, out.size()}, out);
    return out;
}

}}

CORRADE_PLUGIN_REGISTER(GlslOptimizerShaderConverter, Magnum::ShaderTools::GlslOptimizerConverter,
    "cz.mosra.magnum.ShaderTools.AbstractConverter/0.1")
