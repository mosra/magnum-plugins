#ifndef Magnum_ShaderTools_SpirvToolsConverter_h
#define Magnum_ShaderTools_SpirvToolsConverter_h
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

/** @file
 * @brief Class @ref Magnum::ShaderTools::SpirvToolsConverter
 * @m_since_latest_{plugins}
 */

#include <Magnum/ShaderTools/AbstractConverter.h>

#include "MagnumPlugins/SpirvToolsShaderConverter/configure.h"

#ifndef DOXYGEN_GENERATING_OUTPUT
#ifndef MAGNUM_SPIRVTOOLSSHADERCONVERTER_BUILD_STATIC
    #ifdef SpirvToolsShaderConverter_EXPORTS
        #define MAGNUM_SPIRVTOOLSSHADERCONVERTER_EXPORT CORRADE_VISIBILITY_EXPORT
    #else
        #define MAGNUM_SPIRVTOOLSSHADERCONVERTER_EXPORT CORRADE_VISIBILITY_IMPORT
    #endif
#else
    #define MAGNUM_SPIRVTOOLSSHADERCONVERTER_EXPORT CORRADE_VISIBILITY_STATIC
#endif
#define MAGNUM_SPIRVTOOLSSHADERCONVERTER_LOCAL CORRADE_VISIBILITY_LOCAL
#else
#define MAGNUM_SPIRVTOOLSSHADERCONVERTER_EXPORT
#define MAGNUM_SPIRVTOOLSSHADERCONVERTER_LOCAL
#endif

namespace Magnum { namespace ShaderTools {

/**
@brief SPIRV-Tools shader converter plugin
@m_since_latest_{plugins}

@m_keywords{SpirvToolsShaderConverter SpirvToSpirvAssemblyShaderConverter}
@m_keywords{SpirvAssemblyToSpirvShaderConverter SpirvShaderConverter}
@m_keywords{SpirvAssemblyShaderConverter}

Uses [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools) for SPIR-V
validation, optimization and converting between SPIR-V binary and assembly text
(@ref Format::Spirv, @ref Format::SpirvAssembly).

This plugin provides the `SpirvShaderConverter`, `SpirvAssemblyShaderConverter`,
`SpirvToSpirvAssemblyShaderConverter` and `SpirvAssemblyToSpirvShaderConverter`
plugins.

@m_class{m-block m-success}

@thirdparty This plugin makes use of the
    [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools) library,
    licensed under @m_class{m-label m-success} **Apache-2.0**
    ([license text](https://github.com/KhronosGroup/SPIRV-Tools/blob/master/LICENSE),
    [choosealicense.com](https://choosealicense.com/licenses/apache-2.0/)). It
    requires attribution for public use.

@section ShaderTools-SpirvToolsConverter-usage Usage

This plugin depends on the @ref ShaderTools and [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools)
libraries and is built if `WITH_SPIRVTOOLSSHADERCONVERTER` is enabled when
building Magnum Plugins. To use as a dynamic plugin, load
@cpp "SpirvToolsShaderConverter" @ce via @ref Corrade::PluginManager::Manager.

Additionally, if you're using Magnum as a CMake subproject, bundle the
[magnum-plugins](https://github.com/mosra/magnum-plugins),
[SPIRV-Headers](https://github.com/KhronosGroup/SPIRV-Headers) and
[SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools) repositories and do
the following. If you want to use system-installed SPIRV-Tools, omit the first
part and point `CMAKE_PREFIX_PATH` to their installation dir if necessary.

@code{.cmake}
# Skip executables, tests and examples
set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
# assumes SPIRV-Headers is cloned next to SPIRV-Tools
set(SPIRV-Headers_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Headers)
add_subdirectory(SPIRV-Tools EXCLUDE_FROM_ALL)

set(WITH_SPIRVTOOLSSHADERCONVERTER ON CACHE BOOL "" FORCE)
add_subdirectory(magnum-plugins EXCLUDE_FROM_ALL)

# So the dynamically loaded plugin gets built implicitly
add_dependencies(your-app MagnumPlugins::SpirvToolsShaderConverter)
@endcode

To use as a static plugin or as a dependency of another plugin with CMake, put
[FindMagnumPlugins.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindMagnumPlugins.cmake)
and [FindSpirvTools.cmake](https://github.com/mosra/magnum-plugins/blob/master/modules/FindSpirvTools.cmake)
into your `modules/` directory, request the `SpirvToolsShaderConverter`
component of the `MagnumPlugins` package and link to the
`MagnumPlugins::SpirvToolsShaderConverter` target:

@code{.cmake}
find_package(MagnumPlugins REQUIRED SpirvToolsShaderConverter)

# ...
target_link_libraries(your-app PRIVATE MagnumPlugins::SpirvToolsShaderConverter)
@endcode

See @ref building-plugins, @ref cmake-plugins and @ref plugins for more
information.

@section ShaderTools-SpirvToolsConverter-conversion Conversion between SPIR-V binary and assembly

Use one of the @ref convertDataToData(), @ref convertDataToFile(),
@ref convertFileToData() or @ref convertFileToFile() APIs to convert between
SPIR-V binary and assembly, similarly to the
[spirv-as](https://github.com/KhronosGroup/SPIRV-Tools#assembler-tool) and
[spirv-dis](https://github.com/KhronosGroup/SPIRV-Tools#disassembler-tool)
tools. By default that's the only operation done by the plugin, additionally it
can perform optimizations as @ref ShaderTools-SpirvToolsConverter-optimization "described below".
See @ref ShaderTools-SpirvToolsConverter-format below for details on how to
switch between binary and assembly output, see @ref ShaderTools-SpirvToolsConverter-configuration
for a list of additional options related to the (dis)assembler.

The @p stage parameter of all conversion APIs is ignored, as SPIR-V has the
information embedded (and additionally a single file can contain multiple
entrypoints for multiple stages).

On error, the message contains either a line/column (when assembling) or
instruction index (when disassemling).

@section ShaderTools-SpirvToolsConverter-validation SPIR-V validation

Use @ref validateData() or @ref validateFile() to validate a SPIR-V file.
Compared to the [spirv-val](https://github.com/KhronosGroup/SPIRV-Tools#validator-tool)
tool, it accepts a SPIR-V assembly as well, converting it to a SPIR-V binary
first (equivalently to
@ref ShaderTools-SpirvToolsConverter-conversion "doing a conversion" first,
with the exact same behavior and options recognized). Note that in some cases,
such as opening an inaccessible file or an assembly error the validation
function can return @cpp {false, ""} @ce and print a message to the error
output instead.

Validation results are highly dependent on the target version set using
@ref setOutputFormat(), see @ref ShaderTools-SpirvToolsConverter-format below
for details. Additional validation options can be set through the
@ref ShaderTools-SpirvToolsConverter-configuration "plugin-specific config".

If the returned validation string contains a numeric identifier, it's always an
instruction index, even in case of a SPIR-V assembly on the input.

@section ShaderTools-SpirvToolsConverter-optimization SPIR-V optimization

Use @ref setOptimizationLevel() to set a level of optimizations performed
during @ref convertDataToData(), @ref convertDataToFile(),
@ref convertFileToData() or @ref convertFileToFile(). By default no
optimizations are done and the APIs just pass-through the data or convert
between SPIR-V binary and assembly @ref ShaderTools-SpirvToolsConverter-conversion "as described above". Valid optimization levels are:

-   `0` or the empty default performs no optimization
-   `1` optimizes for performance
-   `s` optimizes for size
-   `legalizeHlsl` turns SPIR-V originating from a HLSL source to one that can
    be accepted by Vulkan
-   `vulkanToWebGpu` turns Vulkan-compatible SPIR-V to one that can be accepted
    by WebGPU. Available since SPIRV-Tools 2019.3.
-   `webGpuToVulkan` turns WebGPU-compatible SPIR-V to one that can be accepted
    by Vulkan. Available since SPIRV-Tools 2019.3.

Compared to [spirv-opt](https://github.com/KhronosGroup/SPIRV-Tools#optimizer-tool)
it can work with assembly on both input and output as well, but there's
currently no way to directly control particular optimizer stages, only general
validation options specified through the @ref ShaderTools-SpirvToolsConverter-configuration "plugin-specific config".

@section ShaderTools-SpirvToolsConverter-format Input and output format and version

By default, the converter attempts to detect a SPIR-V binary and if that fails,
it'll assume a SPIR-V assembly, parsing it as such. The output format is
implicitly a SPIR-V binary. You can override the defaults in the following
ways:

1.  Calling @ref setInputFormat() and @ref setOutputFormat() with either
    @ref Format::Spirv or @ref Format::SpirvAssembly. @ref Format::Unspecified
    is the default behavior described above.
2.  Loading the plugin through one of the `SpirvToSpirvAssemblyShaderConverter`,
    `SpirvAssemblyToSpirvShaderConverter`, `SpirvShaderConverter` or
    `SpirvAssemblyShaderConverter`, which will set the input and output format
    accordingly, the last two setting both the input and output format to the
    same value
3.  Calling @ref convertFileToFile(), in which case the input format is
    autodetected based on file contents and the output format is a SPIR-V
    assembly instead of SPIR-V binary (the default) if the output file
    extension is `*.spvasm`.

The @p format passed to @ref setInputFormat() has to be either
@ref Format::Unspecified, @ref Format::Spirv or @ref Format::SpirvAssembly. The
@p version parameter is currently reserved for future extensions and has to be
always empty.

The @p format passed to @ref setOutputFormat() has to be either
@ref Format::Unspecified, @ref Format::Spirv or @ref Format::SpirvAssembly for
conversion and @ref Format::Unspecified for validation. The @p version
string can be one of these. Depending on the version of SPIRV-Tools the plugin
is linked against, some choices might not be available or there might be new
ones:

-   `spv1.0` for SPIR-V 1.0 with no other restrictions
-   `spv1.1` for SPIR-V 1.1 with no other restrictions
-   `spv1.2` for SPIR-V 1.2 with no other restrictions
-   `spv1.3` for SPIR-V 1.3 with no other restrictions
-   `spv1.4` for SPIR-V 1.4 with no other restrictions. Available since
    SPIRV-Tools 2019.3.
-   `spv1.5` for SPIR-V 1.5 with no other restrictions. Available since
    SPIRV-Tools 2019.5.
-   `vulkan1.0` for Vulkan 1.0 with SPIR-V 1.0
-   `vulkan1.1` for Vulkan 1.1 with SPIR-V 1.3
-   `vulkan1.1spv1.4` for Vulkan 1.1 with SPIR-V 1.4. Available since
    SPIRV-Tools 2019.3.
-   `vulkan1.2` for Vulkan 1.1 with SPIR-V 1.5. Available since SPIRV-Tools
    2020.1.
-   `opencl1.2` for OpenCL Full Profile 1.2 plus
    @m_class{m-doc-external} [cl_khr_il_program](https://www.khronos.org/registry/OpenCL/sdk/2.2/docs/man/html/cl_khr_il_program.html)
-   `opencl1.2embedded` for OpenCL Embedded Profile 1.2 plus
    @m_class{m-doc-external} [cl_khr_il_program](https://www.khronos.org/registry/OpenCL/sdk/2.2/docs/man/html/cl_khr_il_program.html)
-   `opencl2.0` for OpenCL Full Profile 2.0 plus
    @m_class{m-doc-external} [cl_khr_il_program](https://www.khronos.org/registry/OpenCL/sdk/2.2/docs/man/html/cl_khr_il_program.html)
-   `opencl2.0embedded` for OpenCL Embedded Profile 2.0 plus
    @m_class{m-doc-external} [cl_khr_il_program](https://www.khronos.org/registry/OpenCL/sdk/2.2/docs/man/html/cl_khr_il_program.html)
-   `opencl2.1` for OpenCL Full Profile 2.1
-   `opencl2.1embedded` for OpenCL Embedded Profile 2.1
-   `opencl2.2` for OpenCL Full Profile 2.2
-   `opencl2.2embedded` for OpenCL Embedded Profile 2.2
-   `opengl4.0` for OpenGL 4.0 plus @gl_extension{ARB,gl_spirv}
-   `opengl4.1` for OpenGL 4.1 plus @gl_extension{ARB,gl_spirv}
-   `opengl4.2` for OpenGL 4.2 plus @gl_extension{ARB,gl_spirv}
-   `opengl4.3` for OpenGL 4.3 plus @gl_extension{ARB,gl_spirv}
-   `opengl4.5` for OpenGL 4.5 plus @gl_extension{ARB,gl_spirv}
-   `webgpu0` for Work-In-Progress WebGPU 1.0

Default if no version string is specified is `vulkan1.0`. There's no variant
for OpenGL 4.4.

@section ShaderTools-SpirvToolsConverter-configuration Plugin-specific config

It's possible to tune various assembler, disassembler and validator options
through @ref configuration(). The assembler options are used also during
validation in case the input is a SPIR-V assembly. See below for all options
and their default values.

@snippet MagnumPlugins/SpirvToolsShaderConverter/SpirvToolsShaderConverter.conf config

*/
class MAGNUM_SPIRVTOOLSSHADERCONVERTER_EXPORT SpirvToolsConverter: public AbstractConverter {
    public:
        /** @brief Plugin manager constructor */
        explicit SpirvToolsConverter(PluginManager::AbstractManager& manager, const std::string& plugin);

    private:
        MAGNUM_SPIRVTOOLSSHADERCONVERTER_LOCAL ConverterFeatures doFeatures() const override;
        MAGNUM_SPIRVTOOLSSHADERCONVERTER_LOCAL void doSetInputFormat(Format format, Containers::StringView version) override;
        MAGNUM_SPIRVTOOLSSHADERCONVERTER_LOCAL void doSetOutputFormat(Format format, Containers::StringView version) override;
        MAGNUM_SPIRVTOOLSSHADERCONVERTER_LOCAL void doSetOptimizationLevel(Containers::StringView level) override;

        MAGNUM_SPIRVTOOLSSHADERCONVERTER_LOCAL std::pair<bool, Containers::String> doValidateData(Stage stage, Containers::ArrayView<const char> data) override;
        MAGNUM_SPIRVTOOLSSHADERCONVERTER_LOCAL std::pair<bool, Containers::String> doValidateFile(Stage stage, Containers::StringView filename) override;
        MAGNUM_SPIRVTOOLSSHADERCONVERTER_LOCAL bool doConvertFileToFile(Stage stage, Containers::StringView from, Containers::StringView to) override;
        MAGNUM_SPIRVTOOLSSHADERCONVERTER_LOCAL Containers::Array<char> doConvertFileToData(Stage stage, Containers::StringView filename) override;
        MAGNUM_SPIRVTOOLSSHADERCONVERTER_LOCAL Containers::Array<char> doConvertDataToData(Magnum::ShaderTools::Stage stage, Containers::ArrayView<const char> data) override;

        struct State;
        Containers::Pointer<State> _state;
};

}}

#endif
