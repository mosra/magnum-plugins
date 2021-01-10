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

#include "spirv-tools/libspirv.h"
#include "spirv-tools/optimizer.hpp"

#ifndef CHECK_VERSION
#error CHECK_VERSION not defined
#define CHECK_VERSION 0xffffffff
#endif

int main() {
    spv_validator_options validatorOptions = spvValidatorOptionsCreate();
    spv_optimizer_options optimizerOptions = spvOptimizerOptionsCreate();
    spv_target_env env{};
    spvtools::Optimizer optimizer{env};

    #if CHECK_VERSION >= 202001
    env = SPV_ENV_VULKAN_1_2;
    #endif

    #if CHECK_VERSION >= 201905
    env = SPV_ENV_UNIVERSAL_1_5;
    #endif

    #if CHECK_VERSION >= 201904
    spvOptimizerOptionsSetPreserveBindings(optimizerOptions, true);
    spvOptimizerOptionsSetPreserveSpecConstants(optimizerOptions, true);
    #endif

    #if CHECK_VERSION >= 201903
    env = SPV_ENV_UNIVERSAL_1_4;
    env = SPV_ENV_VULKAN_1_1_SPIRV_1_4;
    spvValidatorOptionsSetUniformBufferStandardLayout(validatorOptions, false);
    spvValidatorOptionsSetBeforeHlslLegalization(validatorOptions, false);

    optimizer.RegisterVulkanToWebGPUPasses();
    optimizer.RegisterWebGPUToVulkanPasses();
    #endif

    static_cast<void>(env);
    spvValidatorOptionsDestroy(validatorOptions);
    spvOptimizerOptionsDestroy(optimizerOptions);
}
